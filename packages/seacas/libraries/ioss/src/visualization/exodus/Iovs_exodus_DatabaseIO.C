// Copyright(C) 1999-2022 National Technology & Engineering Solutions
// of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
// NTESS, the U.S. Government retains certain rights in this software.
//
// See packages/seacas/LICENSE for details

#include <Ioss_CodeTypes.h>
#include <tokenize.h>
#include <visualization/exodus/Iovs_exodus_DatabaseIO.h>
#include <visualization/utils/Iovs_Utils.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include <Ioss_ElementTopology.h>
#include <Ioss_FileInfo.h>
#include <Ioss_ParallelUtils.h>
#include <Ioss_SerializeIO.h>
#include <Ioss_SubSystem.h>
#include <Ioss_SurfaceSplit.h>
#include <Ioss_Utils.h>

namespace { // Internal helper functions
  enum class entity_type { NODAL, ELEM_BLOCK, NODE_SET, SIDE_SET };
  int64_t get_id(const Ioss::GroupingEntity *entity, Iovs_exodus::EntityIdSet *idset);
  bool    set_id(const Ioss::GroupingEntity *entity, Iovs_exodus::EntityIdSet *idset);
  int64_t extract_id(const std::string &name_id);
} // End anonymous namespace

namespace Iovs_exodus {
  int field_warning(const Ioss::GroupingEntity *ge, const Ioss::Field &field,
                    const std::string &inout);

  DatabaseIO::DatabaseIO(Ioss::Region *region, const std::string &filename,
                         Ioss::DatabaseUsage db_usage, Ioss_MPI_Comm communicator,
                         const Ioss::PropertyManager &props)
      : Ioss::DatabaseIO(region,
                         Iovs::Utils::getInstance().getDatabaseOutputFilePath(filename, props),
                         db_usage, communicator, props),
        isInput(false), singleProcOnly(false), doLogging(false), createSideSets(false),
        createNodeSets(false), nodeBlockCount(0), elementBlockCount(0)
  {

    Iovs::Utils::DatabaseInfo dbinfo;
    dbinfo.databaseFilename   = this->DBFilename;
    dbinfo.separatorCharacter = std::string(1, this->get_field_separator());
    dbinfo.parallelUtils      = &this->util();

    Iovs::Utils::getInstance().checkDbUsage(db_usage);
    Iovs::Utils::getInstance().createDatabaseOutputFile(dbinfo);
    dbState                              = Ioss::STATE_UNKNOWN;
    this->globalNodeAndElementIDsCreated = false;

    Iovs::Utils::getInstance().writeToCatalystLogFile(dbinfo, props);
    this->catExoMesh = Iovs::Utils::getInstance().createCatalystExodusMesh(dbinfo, props);

    if (props.exists("CATALYST_CREATE_NODE_SETS")) {
      this->createNodeSets = props.get("CATALYST_CREATE_NODE_SETS").get_int();
    }

    if (props.exists("CATALYST_CREATE_SIDE_SETS")) {
      this->createSideSets = props.get("CATALYST_CREATE_SIDE_SETS").get_int();
    }
  }

  DatabaseIO::~DatabaseIO() { this->catExoMesh->Delete(); }

  bool DatabaseIO::begin__(Ioss::State state)
  {
    dbState              = state;
    Ioss::Region *region = this->get_region();

    if (state == Ioss::STATE_MODEL) {
      std::vector<int>                   element_block_id_list;
      Ioss::ElementBlockContainer const &ebc = region->get_element_blocks();
      for (auto i : ebc) {
        element_block_id_list.push_back(get_id(i, &ids_));
      }
      this->catExoMesh->InitializeElementBlocks(element_block_id_list);
    }
    return true;
  }

  bool DatabaseIO::end__(Ioss::State state)
  {
    // Transitioning out of state 'state'
    assert(state == dbState);
    switch (state) {
    case Ioss::STATE_DEFINE_MODEL: write_meta_data(); break;
    case Ioss::STATE_DEFINE_TRANSIENT: break;
    default: // ignore everything else...
      break;
    }

    {
      dbState = Ioss::STATE_UNKNOWN;
    }

    return true;
  }

  // Default versions do nothing at this time...
  // Will be used for global variables...
  bool DatabaseIO::begin_state__(int state, double time)
  {
    Ioss::SerializeIO serializeIO__(this);
    this->catExoMesh->ReleaseMemory();

    if (!this->globalNodeAndElementIDsCreated) {
      this->create_global_node_and_element_ids();
    }

    this->catExoMesh->SetTimeData(time, state - 1);

    return true;
  }

  bool DatabaseIO::end_state__(int /*state*/, double /*time*/)
  {
    Ioss::SerializeIO        serializeIO__(this);
    std::vector<int>         error_codes;
    std::vector<std::string> error_messages;
    this->catExoMesh->logMemoryUsageAndTakeTimerReading();
    this->catExoMesh->PerformCoProcessing(error_codes, error_messages);
    this->catExoMesh->logMemoryUsageAndTakeTimerReading();
    Iovs::Utils::getInstance().reportCatalystErrorMessages(error_codes, error_messages,
                                                           this->parallel_rank());

    return true;
  }

  void DatabaseIO::read_meta_data__() {}

  void DatabaseIO::create_global_node_and_element_ids() const
  {
    const Ioss::ElementBlockContainer &element_blocks = this->get_region()->get_element_blocks();
    Ioss::ElementBlockContainer::const_iterator I;
    std::vector<std::string>                    component_names;
    component_names.emplace_back("GlobalElementId");
    for (I = element_blocks.begin(); I != element_blocks.end(); ++I) {
      int     bid       = get_id((*I), &ids_);
      int64_t eb_offset = (*I)->get_offset();
      this->catExoMesh->CreateElementVariable(component_names, bid,
                                              &this->elemMap.map()[eb_offset + 1]);
    }

    component_names.clear();
    component_names.emplace_back("GlobalNodeId");
    this->catExoMesh->CreateNodalVariable(component_names, &this->nodeMap.map()[1]);

    this->globalNodeAndElementIDsCreated = true;
  }

  //------------------------------------------------------------------------
  int64_t DatabaseIO::put_field_internal(const Ioss::Region * /* region */,
                                         const Ioss::Field &field, void *data,
                                         size_t data_size) const
  {
    // For now, assume that all TRANSIENT fields on a region
    // are REDUCTION fields (1 value).  We need to gather these
    // and output them all at one time.  The storage location is a
    // 'globalVariables' array
    Ioss::SerializeIO serializeIO__(this);

    size_t                num_to_get = field.verify(data_size);
    Ioss::Field::RoleType role       = field.get_role();
    if ((role == Ioss::Field::TRANSIENT || role == Ioss::Field::REDUCTION) && num_to_get == 1) {
      const char            *complex_suffix[] = {".re", ".im"};
      Ioss::Field::BasicType ioss_type        = field.get_type();
      auto                  *rvar             = static_cast<double *>(data);
      int                   *ivar             = static_cast<int *>(data);
      auto                  *ivar64           = static_cast<int64_t *>(data);

      int comp_count = field.get_component_count(Ioss::Field::InOut::OUTPUT);

      int re_im = 1;
      if (field.get_type() == Ioss::Field::COMPLEX) {
        re_im = 2;
      }
      for (int complex_comp = 0; complex_comp < re_im; complex_comp++) {
        std::string field_name = field.get_name();
        if (re_im == 2) {
          field_name += complex_suffix[complex_comp];
        }

        std::vector<std::string> component_names;
        std::vector<double>      globalValues;
        for (int i = 0; i < comp_count; i++) {
          std::string var_name = get_component_name(field, Ioss::Field::InOut::OUTPUT, i + 1);
          component_names.push_back(var_name);

          // Transfer from 'variables' array.
          if (ioss_type == Ioss::Field::REAL || ioss_type == Ioss::Field::COMPLEX) {
            globalValues.push_back(rvar[i]);
          }
          else if (ioss_type == Ioss::Field::INTEGER) {
            globalValues.push_back(ivar[i]);
          }
          else if (ioss_type == Ioss::Field::INT64) {
            globalValues.push_back(ivar64[i]);
          }
        }
        this->catExoMesh->CreateGlobalVariable(component_names, globalValues.data());
      }
    }
    else if (num_to_get != 1) {
      // There should have been a warning/error message printed to the
      // log file earlier for this, so we won't print anything else
      // here since it would be printed for each and every timestep....
      ;
    }
    else {
      std::ostringstream errmsg;
      errmsg << "The variable named '" << field.get_name()
             << "' is of the wrong type. A region variable must be of type"
             << " TRANSIENT or REDUCTION.\n"
             << "This is probably an internal error; please notify gdsjaar@sandia.gov";
      IOSS_ERROR(errmsg);
    }
    return num_to_get;
  }

  int64_t DatabaseIO::put_field_internal(const Ioss::NodeBlock *nb, const Ioss::Field &field,
                                         void *data, size_t data_size) const
  {
    Ioss::SerializeIO serializeIO__(this);
    size_t            num_to_get = field.verify(data_size);

    if (num_to_get > 0) {
      Ioss::Field::RoleType role = field.get_role();

      if (role == Ioss::Field::MESH) {
        if (field.get_name() == "mesh_model_coordinates") {
          this->catExoMesh->InitializeGlobalPoints(num_to_get,
                                                   nb->get_property("component_degree").get_int(),
                                                   static_cast<double *>(data));
        }
        else if (field.get_name() == "ids") {
          // The ids coming in are the global ids; their position is the
          // local id -1 (That is, data[0] contains the global id of local
          // node 1)

          // Another 'const-cast' since we are modifying the database just
          // for efficiency; which the client does not see...
          auto *new_this = const_cast<DatabaseIO *>(this);
          /*64 bit should be okay*/
          new_this->handle_node_ids(data, num_to_get);
        }
        else if (field.get_name() == "connectivity") {
          // Do nothing, just handles an idiosyncrasy of the GroupingEntity
        }
        else {
          return field_warning(nb, field, "mesh output");
        }
      }
      else if (role == Ioss::Field::TRANSIENT) {
        const char            *complex_suffix[] = {".re", ".im"};
        Ioss::Field::BasicType ioss_type        = field.get_type();
        std::vector<double>    temp(num_to_get);
        int                    comp_count = field.get_component_count(Ioss::Field::InOut::OUTPUT);
        int                    re_im      = 1;
        if (ioss_type == Ioss::Field::COMPLEX) {
          re_im = 2;
        }
        for (int complex_comp = 0; complex_comp < re_im; complex_comp++) {
          std::string field_name = field.get_name();
          if (re_im == 2) {
            field_name += complex_suffix[complex_comp];
          }

          std::vector<double>      interleaved_data(num_to_get * comp_count);
          std::vector<std::string> component_names;
          for (int i = 0; i < comp_count; i++) {
            std::string var_name = get_component_name(field, Ioss::Field::InOut::OUTPUT, i + 1);
            component_names.push_back(var_name);

            size_t begin_offset = (re_im * i) + complex_comp;
            size_t stride       = re_im * comp_count;

            if (ioss_type == Ioss::Field::REAL || ioss_type == Ioss::Field::COMPLEX) {
              this->nodeMap.map_field_to_db_scalar_order(static_cast<double *>(data), temp,
                                                         begin_offset, num_to_get, stride, 0);
            }
            else if (ioss_type == Ioss::Field::INTEGER) {
              this->nodeMap.map_field_to_db_scalar_order(static_cast<int *>(data), temp,
                                                         begin_offset, num_to_get, stride, 0);
            }
            else if (ioss_type == Ioss::Field::INT64) {
              this->nodeMap.map_field_to_db_scalar_order(static_cast<int64_t *>(data), temp,
                                                         begin_offset, num_to_get, stride, 0);
            }

            for (unsigned int j = 0; j < num_to_get; j++) {
              interleaved_data[j * comp_count + i] = temp[j];
            }
          }
          this->catExoMesh->CreateNodalVariable(component_names, interleaved_data.data());
        }
      }
      else if (role == Ioss::Field::REDUCTION) {
      }
    }
    return num_to_get;
  }

  int64_t DatabaseIO::put_field_internal(const Ioss::ElementBlock *eb, const Ioss::Field &field,
                                         void *data, size_t data_size) const
  {
    Ioss::SerializeIO serializeIO__(this);

    size_t num_to_get = field.verify(data_size);
    if (num_to_get > 0) {

      // Get the element block id and element count

      int64_t               element_count = eb->entity_count();
      Ioss::Field::RoleType role          = field.get_role();

      if (role == Ioss::Field::MESH) {
        // Handle the MESH fields required for an ExodusII file model.
        // (The 'genesis' portion)
        if (field.get_name() == "connectivity") {
          if (element_count > 0) {
            // Map element connectivity from global node id to local node id.
            // Do it in 'data' ...
            int element_nodes = eb->topology()->number_nodes();
            assert(field.transformed_storage()->component_count() == element_nodes);
            nodeMap.reverse_map_data(data, field, num_to_get * element_nodes);
            Ioss::Field::BasicType ioss_type = field.get_type();
            int64_t                eb_offset = eb->get_offset();
            int                    id        = get_id(eb, &ids_);

            if (ioss_type == Ioss::Field::INTEGER) {
              this->catExoMesh->CreateElementBlock(
                  eb->name().c_str(), id, eb->get_property("topology_type").get_string(),
                  element_nodes, num_to_get, &this->elemMap.map()[eb_offset + 1],
                  static_cast<int *>(data));
            }
            else if (ioss_type == Ioss::Field::INT64) {
              this->catExoMesh->CreateElementBlock(
                  eb->name().c_str(), id, eb->get_property("topology_type").get_string(),
                  element_nodes, num_to_get, &this->elemMap.map()[eb_offset + 1],
                  static_cast<int64_t *>(data));
            }
          }
        }
        else if (field.get_name() == "ids") {
          // Another 'const-cast' since we are modifying the database just
          // for efficiency; which the client does not see...
          auto *new_this = const_cast<DatabaseIO *>(this);
          new_this->handle_element_ids(eb, data, num_to_get);
        }
        else if (field.get_name() == "skin") {
          // Not applicable to viz output.
        }
        else {
          Ioss::WARNING() << " ElementBlock " << eb->name() << ". Unknown field "
                          << field.get_name();
          num_to_get = 0;
        }
      }
      else if (role == Ioss::Field::ATTRIBUTE) {
        /* TODO */
      }
      else if (role == Ioss::Field::TRANSIENT) {
        const char            *complex_suffix[] = {".re", ".im"};
        Ioss::Field::BasicType ioss_type        = field.get_type();
        std::vector<double>    temp(num_to_get);
        int64_t                eb_offset  = eb->get_offset();
        int                    comp_count = field.get_component_count(Ioss::Field::InOut::OUTPUT);
        int                    bid        = get_id(eb, &ids_);

        int re_im = 1;
        if (ioss_type == Ioss::Field::COMPLEX) {
          re_im = 2;
        }
        for (int complex_comp = 0; complex_comp < re_im; complex_comp++) {
          std::string field_name = field.get_name();
          if (re_im == 2) {
            field_name += complex_suffix[complex_comp];
          }

          std::vector<double>      interleaved_data(num_to_get * comp_count);
          std::vector<std::string> component_names;
          for (int i = 0; i < comp_count; i++) {
            std::string var_name = get_component_name(field, Ioss::Field::InOut::OUTPUT, i + 1);
            component_names.push_back(var_name);

            int64_t begin_offset = (re_im * i) + complex_comp;
            int64_t stride       = re_im * comp_count;

            if (ioss_type == Ioss::Field::REAL || ioss_type == Ioss::Field::COMPLEX) {
              this->elemMap.map_field_to_db_scalar_order(
                  static_cast<double *>(data), temp, begin_offset, num_to_get, stride, eb_offset);
            }
            else if (ioss_type == Ioss::Field::INTEGER) {
              this->elemMap.map_field_to_db_scalar_order(
                  static_cast<int *>(data), temp, begin_offset, num_to_get, stride, eb_offset);
            }
            else if (ioss_type == Ioss::Field::INT64) {
              this->elemMap.map_field_to_db_scalar_order(
                  static_cast<int64_t *>(data), temp, begin_offset, num_to_get, stride, eb_offset);
            }
            for (unsigned int j = 0; j < num_to_get; j++) {
              interleaved_data[j * comp_count + i] = temp[j];
            }
          }
          this->catExoMesh->CreateElementVariable(component_names, bid, interleaved_data.data());
        }
      }
      else if (role == Ioss::Field::REDUCTION) {
      }
    }
    return num_to_get;
  }

  void DatabaseIO::write_meta_data()
  {
    Ioss::Region *region = get_region();

    // Node Blocks --
    {
      const Ioss::NodeBlockContainer &node_blocks = region->get_node_blocks();
      assert(node_blocks.size() == 1);
      nodeCount = node_blocks[0]->entity_count();
    }

    // NodeSets ...
    {
      const Ioss::NodeSetContainer          &nodesets = region->get_nodesets();
      Ioss::NodeSetContainer::const_iterator I;
      for (I = nodesets.begin(); I != nodesets.end(); ++I) {
        set_id(*I, &ids_);
      }
    }

    // SideSets ...
    {
      const Ioss::SideSetContainer          &ssets = region->get_sidesets();
      Ioss::SideSetContainer::const_iterator I;

      for (I = ssets.begin(); I != ssets.end(); ++I) {
        set_id(*I, &ids_);
      }
    }

    // Element Blocks --
    {
      const Ioss::ElementBlockContainer          &element_blocks = region->get_element_blocks();
      Ioss::ElementBlockContainer::const_iterator I;
      // Set ids of all entities that have "id" property...
      for (I = element_blocks.begin(); I != element_blocks.end(); ++I) {
        set_id(*I, &ids_);
      }

      elementBlockCount = 0;
      elementCount      = 0;
      for (I = element_blocks.begin(); I != element_blocks.end(); ++I) {
        elementBlockCount++;
        elementCount += (*I)->entity_count();
      }
    }
  }

  int64_t DatabaseIO::handle_node_ids(void *ids, int64_t num_to_get)
  {
    // std::cerr << "DatabaseIO::handle_node_ids executing\n";
    /*!
     * There are two modes we need to support in this routine:
     * 1. Initial definition of node map (local->global) and
     * reverseNodeMap (global->local).
     * 2. Redefinition of node map via 'reordering' of the original
     * map when the nodes on this processor are the same, but their
     * order is changed (or count because of ghosting)
     *
     * So, there will be two maps the 'nodeMap' map is a 'direct lookup'
     * map which maps current local position to global id and the
     * 'reverseNodeMap' is an associative lookup which maps the
     * global id to 'original local'.  There is also a
     * 'reorderNodeMap' which is direct lookup and maps current local
     * position to original local.

     * The ids coming in are the global ids; their position is the
     * "local id-1" (That is, data[0] contains the global id of local
     * node 1 in this node block).
     *
     * int local_position = reverseNodeMap[NodeMap[i+1]]
     * (the nodeMap and reverseNodeMap are 1-based)
     *
     * To determine which map to update on a call to this function, we
     * use the following hueristics:
     * -- If the database state is 'STATE_MODEL:', then update the
     *    'reverseNodeMap' and 'nodeMap'
     *
     * -- If the database state is not STATE_MODEL, then leave the
     *    'reverseNodeMap' and 'nodeMap' alone since they correspond to the
     *    information already written to the database. [May want to add a
     *    STATE_REDEFINE_MODEL]
     *
     * -- In both cases, update the reorderNodeMap
     *
     * NOTE: The mapping is done on TRANSIENT fields only; MODEL fields
     *       should be in the original order...
     */
    assert(num_to_get == nodeCount);

    nodeMap.set_size(nodeCount);

    bool in_define = (dbState == Ioss::STATE_MODEL) || (dbState == Ioss::STATE_DEFINE_MODEL);
    if (int_byte_size_api() == 4) {
      nodeMap.set_map(static_cast<int *>(ids), num_to_get, 0, in_define);
    }
    else {
      nodeMap.set_map(static_cast<int64_t *>(ids), num_to_get, 0, in_define);
    }

    if (in_define) {
      // Only a single nodeblock and all set
      assert(get_region()->get_property("node_block_count").get_int() == 1);
    }
    return num_to_get;
  }

  size_t handle_block_ids(const Ioss::EntityBlock *eb, Ioss::State db_state, Ioss::Map &entity_map,
                          void *ids, size_t int_byte_size, size_t num_to_get,
                          /*int file_pointer,*/ int /*my_processor*/)
  {
    /*!
     * NOTE: "element" is generic for "element", "face", or "edge"
     *
     * There are two modes we need to support in this routine:
     * 1. Initial definition of element map (local->global) and
     * reverseElementMap (global->local).
     * 2. Redefinition of element map via 'reordering' of the original
     * map when the elements on this processor are the same, but their
     * order is changed.
     *
     * So, there will be two maps the 'elementMap' map is a 'direct lookup'
     * map which maps current local position to global id and the
     * 'reverseElementMap' is an associative lookup which maps the
     * global id to 'original local'.  There is also a
     * 'reorderElementMap' which is direct lookup and maps current local
     * position to original local.

     * The ids coming in are the global ids; their position is the
     * local id -1 (That is, data[0] contains the global id of local
     * element 1 in this element block).  The 'model-local' id is
     * given by eb_offset + 1 + position:
     *
     * int local_position = reverseElementMap[ElementMap[i+1]]
     * (the elementMap and reverseElementMap are 1-based)
     *
     * But, this assumes 1..numel elements are being output at the same
     * time; we are actually outputting a blocks worth of elements at a
     * time, so we need to consider the block offsets.
     * So... local-in-block position 'i' is index 'eb_offset+i' in
     * 'elementMap' and the 'local_position' within the element
     * blocks data arrays is 'local_position-eb_offset'.  With this, the
     * position within the data array of this element block is:
     *
     * int eb_position =
     * reverseElementMap[elementMap[eb_offset+i+1]]-eb_offset-1
     *
     * To determine which map to update on a call to this function, we
     * use the following hueristics:
     * -- If the database state is 'Ioss::STATE_MODEL:', then update the
     *    'reverseElementMap'.
     * -- If the database state is not Ioss::STATE_MODEL, then leave
     *    the 'reverseElementMap' alone since it corresponds to the
     *    information already written to the database. [May want to add
     *    a Ioss::STATE_REDEFINE_MODEL]
     * -- Always update elementMap to match the passed in 'ids'
     *    array.
     *
     * NOTE: the maps are built an element block at a time...
     * NOTE: The mapping is done on TRANSIENT fields only; MODEL fields
     *       should be in the original order...
     */

    // Overwrite this portion of the 'elementMap', but keep other
    // parts as they were.  We are adding elements starting at position
    // 'eb_offset+offset' and ending at
    // 'eb_offset+offset+num_to_get'. If the entire block is being
    // processed, this reduces to the range 'eb_offset..eb_offset+my_element_count'

    int64_t eb_offset = eb->get_offset();

    bool in_define = (db_state == Ioss::STATE_MODEL) || (db_state == Ioss::STATE_DEFINE_MODEL);
    if (int_byte_size == 4) {
      entity_map.set_map(static_cast<int *>(ids), num_to_get, eb_offset, in_define);
    }
    else {
      entity_map.set_map(static_cast<int64_t *>(ids), num_to_get, eb_offset, in_define);
    }
    return num_to_get;
  }

  int64_t DatabaseIO::handle_element_ids(const Ioss::ElementBlock *eb, void *ids, size_t num_to_get)
  {
    elemMap.set_size(elementCount);
    return handle_block_ids(eb, dbState, elemMap, ids, int_byte_size_api(), num_to_get,
                            /*get_file_pointer(),*/ myProcessor);
  }

  const Ioss::Map &DatabaseIO::get_node_map() const
  {
    // Allocate space for node number map and read it in...
    // Can be called multiple times, allocate 1 time only
    if (nodeMap.map().empty()) {
      nodeMap.set_size(nodeCount);

      // Output database; nodeMap not set yet... Build a default map.
      for (int64_t i = 1; i < nodeCount + 1; i++) {
        nodeMap.map()[i] = i;
      }
    }
    return nodeMap;
  }

  // Not used...
  const Ioss::Map &DatabaseIO::get_element_map() const
  {
    // std::cercercerrn new nathan Iovs DatabaseIO::get_element_map\n";
    // Allocate space for element number map and read it in...
    // Can be called multiple times, allocate 1 time only
    if (elemMap.map().empty()) {
      elemMap.set_size(elementCount);

      // Output database; elementMap not set yet... Build a default map.
      for (int64_t i = 1; i < elementCount + 1; i++) {
        elemMap.map()[i] = i;
      }
    }
    return elemMap;
  }

  int field_warning(const Ioss::GroupingEntity *ge, const Ioss::Field &field,
                    const std::string &inout)
  {
    Ioss::WARNING() << ge->type() << " '" << ge->name() << "'. Unknown " << inout << " field '"
                    << field.get_name() << "'";
    return -4;
  }

  int64_t DatabaseIO::put_field_internal(const Ioss::NodeSet *ns, const Ioss::Field &field,
                                         void *data, size_t data_size) const
  {
    int64_t num_to_get          = field.verify(data_size);
    int64_t cns_save_num_to_get = 0;

    if (num_to_get > 0 && (field.get_name() == "ids" || field.get_name() == "ids_raw")) {

      // int id = get_id(ns, &this->ids_);

      if (!this->createNodeSets) {
        cns_save_num_to_get = num_to_get;
        num_to_get          = 0;
      }

      if (field.get_type() == Ioss::Field::INTEGER) {
        this->nodeMap.reverse_map_data(data, field, num_to_get);
        // this->catExoMesh->CreateNodeSet(ns->name().c_str(), id, num_to_get,
        //    static_cast<int *>(data));
      }
      else if (field.get_type() == Ioss::Field::INT64) {
        this->nodeMap.reverse_map_data(data, field, num_to_get);
        // this->catExoMesh->CreateNodeSet(ns->name().c_str(), id, num_to_get,
        //    static_cast<int64_t *>(data));
      }

      if (!this->createNodeSets) {
        num_to_get = cns_save_num_to_get;
      }
    }
    return num_to_get;
  }

  int64_t DatabaseIO::put_field_internal(const Ioss::SideSet *fs, const Ioss::Field &field,
                                         void * /*data*/, size_t data_size) const
  {
    size_t num_to_get = field.verify(data_size);
    if (field.get_name() == "ids") {
      // Do nothing, just handles an idiosyncrasy of the GroupingEntity
    }
    else {
      num_to_get = Ioss::Utils::field_warning(fs, field, "output");
    }
    return num_to_get;
  }

  int64_t DatabaseIO::put_field_internal(const Ioss::SideBlock *eb, const Ioss::Field &field,
                                         void *data, size_t data_size) const
  {
    int64_t num_to_get          = field.verify(data_size);
    int64_t css_save_num_to_get = 0;

    if ((field.get_name() == "element_side") || (field.get_name() == "element_side_raw")) {
      size_t side_offset = Ioss::Utils::get_side_offset(eb);

      // int id = get_id(eb, &this->ids_);

      size_t index = 0;

      if (field.get_type() == Ioss::Field::INTEGER) {
        Ioss::IntVector element(num_to_get);
        Ioss::IntVector side(num_to_get);
        int            *el_side = static_cast<int *>(data);

        for (unsigned int i = 0; i < num_to_get; i++) {
          element[i] = el_side[index++];
          side[i]    = el_side[index++] + side_offset;
        }

        if (!this->createSideSets) {
          css_save_num_to_get = num_to_get;
          num_to_get          = 0;
        }

        // const Ioss::SideSet *ebowner = eb->owner();
        /*NOTE: Jeff Mauldin JAM 2015Oct8
         CreateSideSet is called once for each block which the sideset
         spans, and the eb->name() for the side set is the ebowner->name()
         with additional characters to indicate which block we are doing.
         The current implementation of the sierra/catalyst sideset
         construction creates a single independent sideset and collects all
         the nodes and elements from the side set from each block spanned
         by the sideset into that single sideset.  It needs to have the
         ebowner->name(), not the eb->name(), because that is the name
         in the input deck for the sideset for reference for things like
         extractblock.  It may become necessary at a later date to
         pass in both ebowner->name() AND eb->name(), but for now we
         are just passing in ebowner->name() to give us correct
         functionality while not changing the function interface*/
        // this->catExoMesh->CreateSideSet(ebowner->name().c_str(), id, num_to_get,
        //    &element[0], &side[0]);

        if (!this->createSideSets) {
          num_to_get = css_save_num_to_get;
        }
      }
      else {
        Ioss::Int64Vector element(num_to_get);
        Ioss::Int64Vector side(num_to_get);
        auto             *el_side = static_cast<int64_t *>(data);

        for (unsigned int i = 0; i < num_to_get; i++) {
          element[i] = el_side[index++];
          side[i]    = el_side[index++] + side_offset;
        }

        if (!this->createSideSets) {
          css_save_num_to_get = num_to_get;
          num_to_get          = 0;
        }

        // const Ioss::SideSet *ebowner = eb->owner();
        /*NOTE: Jeff Mauldin JAM 2015Oct8
         CreateSideSet is called once for each block which the sideset
         spans, and the eb->name() for the side set is the ebowner->name()
         with additional characters to indicate which block we are doing.
         The current implementation of the sierra/catalyst sideset
         construction creates a single independent sideset and collects all
         the nodes and elements from the side set from each block spanned
         by the sideset into that single sideset.  It needs to have the
         ebowner->name(), not the eb->name(), because that is the name
         in the input deck for the sideset for reference for things like
         extractblock.  It may become necessary at a later date to
         pass in both ebowner->name() AND eb->name(), but for now we
         are just passing in ebowner->name() to give us correct
         functionality while not changing the function interface*/
        // this->catExoMesh->CreateSideSet(ebowner->name().c_str(), id,
        //    num_to_get, &element[0], &side[0]);

        if (!this->createSideSets) {
          num_to_get = css_save_num_to_get;
        }
      }
    }
    return num_to_get;
  }
} // namespace Iovs_exodus

namespace {

  int64_t get_id(const Ioss::GroupingEntity *entity, Iovs_exodus::EntityIdSet *idset)
  {
    // Sierra uses names to refer to grouping entities; however,
    // exodusII requires integer ids.  When reading an exodusII file,
    // the DatabaseIO creates a name by concatenating the entity
    // type (e.g., 'block') and the id separated by an underscore.  For
    // example, an exodusII element block with an id of 100 would be
    // encoded into "block_100"

    // This routine tries to determine the id of the entity using 3
    // approaches:
    //
    // 1. If the entity contains a property named 'id', this is used.
    // The DatabaseIO actually stores the id in the "id" property;
    // however, other grouping entity creators are not required to do
    // this so the property is not guaranteed to exist.
    //
    // 2.If property does not exist, it tries to decode the entity name
    // based on the above encoding.  Again, it is not required that the
    // name follow this convention so success is not guaranteed.
    //
    // 3. If all other schemes fail, the routine picks an id for the entity
    // and returns it.  It also stores this id in the "id" property so an
    // entity will always return the same id for multiple calls.
    // Note that this violates the 'const'ness of the entity so we use
    // a const-cast.

    // Avoid a few string constructors/destructors
    static std::string prop_name("name");
    static std::string id_prop("id");

    int64_t id = 1;

    if (entity->property_exists(id_prop)) {
      id = entity->get_property(id_prop).get_int();
      return id;
    }

    // Try to decode an id from the name.
    std::string name_string = entity->get_property(prop_name).get_string();
    id                      = extract_id(name_string);
    if (id <= 0) {
      id = 1;
    }

    // At this point, we either have an id equal to '1' or we have an id
    // extracted from the entities name. Increment it until it is
    // unique...
    int type = static_cast<int>(entity->type());
    while (idset->find(std::make_pair(type, id)) != idset->end()) {
      ++id;
    }

    // 'id' is a unique id for this entity type...
    idset->insert(std::make_pair(type, id));
    auto *new_entity = const_cast<Ioss::GroupingEntity *>(entity);
    new_entity->property_add(Ioss::Property(id_prop, id));
    return id;
  }

  bool set_id(const Ioss::GroupingEntity *entity, Iovs_exodus::EntityIdSet *idset)
  {
    // See description of 'get_id' function.  This function just primes
    // the idset with existing ids so that when we start generating ids,
    // we don't overwrite an existing one.

    // Avoid a few string constructors/destructors
    static std::string prop_name("name");
    static std::string id_prop("id");

    bool succeed = false;
    if (entity->property_exists(id_prop)) {
      int64_t id = entity->get_property(id_prop).get_int();

      // See whether it already exists...
      int type = static_cast<int>(entity->type());
      succeed  = idset->insert(std::make_pair(type, id)).second;
      if (!succeed) {
        // Need to remove the property so it doesn't cause problems
        // later...
        auto *new_entity = const_cast<Ioss::GroupingEntity *>(entity);
        new_entity->property_erase(id_prop);
        assert(!entity->property_exists(id_prop));
      }
    }
    return succeed;
  }

  int64_t extract_id(const std::string &name_id)
  {
    std::vector<std::string> tokens = Ioss::tokenize(name_id, "_");

    if (tokens.size() == 1) {
      return 0;
    }

    // Check whether last token is an integer...
    std::string str_id = tokens[tokens.size() - 1];
    size_t      len    = str_id.length();
    bool        is_int = true;
    for (size_t i = 0; i < len; i++) {
      if (str_id[i] < '0' || str_id[i] > '9') {
        is_int = false;
        break;
      }
    }
    if (is_int) {
      return std::atoi(str_id.c_str());
    }

    return 0;
  }

} // end anonymous namespace
