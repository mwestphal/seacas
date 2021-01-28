// Copyright(C) 2021 National Technology & Engineering Solutions
// of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
// NTESS, the U.S. Government retains certain rights in this software.
//
// See packages/seacas/LICENSE for details
#ifndef ZE_Cell_H
#define ZE_Cell_H
//
// `Cell`:
//  -- for each location:
//     -- i,j location (could be calculated, but easier if entry knows it...)
//     -- reference to unit_cell region
//     -- global node offset for node ids (over all ranks if parallel)
//     -- local node offset for node ids (for this rank only if parallel)
//     -- global element block offsets for element ids for each element block (over all ranks if
//     parallel)
//     -- local element block offsets for element ids for each element block (for this rank only
//     if parallel)
//     -- offX, offY -- coordinate offsets for nodes (can be calculated?)
//     -- rank (for parallel -- which rank does this entry exist on)
//

#include <memory>
#include <utility>

#include "UnitCell.h"

// Each entry in grid will have the following information:
enum class Axis { X, Y, Z };
enum class Mode { GLOBAL, PROCESSOR };
enum class Loc { C = 0, BL, B, BR, L, R, TL, T, TR };
class Cell
{
public:
  std::pair<double, double> get_coordinate_range(enum Axis) const;
  void                      initialize(size_t i, size_t j, std::shared_ptr<UnitCell> region);

  //! True if this cell has a neighbor to its "left" (lower i)
  bool has_neighbor_i() const { return m_i > 0; }

  //! True if this cell has a neighbor "below it" (lower j)
  bool has_neighbor_j() const { return m_j > 0; }

  //! True if this cell has a processor boundary to the specified direction
  //! Note that cell cannot compute this, but is "told" this during decomposition
  //! of the owning `Grid`
  //! There is a processor boundary if the rank of the cell at that location is
  //! different than the rank of this cell...
  bool processor_boundary(enum Loc loc) const
  {
    return m_ranks[(int)loc] >= 0 && (m_ranks[0] != m_ranks[(int)loc]);
  }

  //! Number of nodes that will be added to global node count when this cell is added to
  //! grid -- accounts for coincident nodes if this cell has neighbor(s)
  size_t added_node_count(enum Mode mode) const;

  //! The mpi rank that this cell, or the neighboring cells, will be on in a parallel run.
  int rank(enum Loc loc) const { return m_ranks[(int)loc]; }

  //! The mpi rank that this cell will be on in a parallel run.
  void set_rank(enum Loc loc, int my_rank) { m_ranks[(int)loc] = my_rank; }

  //! Create a vector of `node_count` length which has the following values:
  //! * 0: Node that is not shared with any "lower" neighbors.
  //! * 1: Node on `min_I` face
  //! * 2: Node on `min_J` face
  //! * 3: Node on `min_I-min_J` line
  //! If `mode == PROCESSOR`, then modify due to processor boundaries...
  std::vector<int> categorize_nodes(enum Mode mode) const;

  //! A vector containing the global node ids of the nodes on the `max_I` face of this
  //! unit cell. These nodes will be shared with the "right" (higher i) neighbor.
  //! Once that cell uses this information, it will clear out the vector.
  mutable std::vector<int64_t> max_I_nodes;

  //! A vector containing the global node ids of the nodes on the `max_J` face of this
  //! unit cell. These nodes will be shared with the "above" (higher j) neighbor.
  //! Once that cell uses this information, it will clear out the vector.
  mutable std::vector<int64_t> max_J_nodes;

  //! The UnitCell that occupies this location in the grid / latice
  std::shared_ptr<UnitCell> m_unitCell;

  //! The `i` location of this entry in the grid
  size_t m_i{0};
  //! The `j` location of this entry in the grid
  size_t m_j{0};

  int64_t m_globalNodeIdOffset{0};
  int64_t m_localNodeIdOffset{0};

  std::map<std::string, size_t> m_globalElementIdOffset;
  std::map<std::string, size_t> m_localElementIdOffset;

  //! The offset that must be added to the `x` coordinates of the
  //! UnitCell to place it in the correct global location of the
  //! output mesh
  double m_offX{0.0};
  //! The offset that must be added to the `y` coordinates of the
  //! UnitCell to place it in the correct global location of the
  //! output mesh
  double m_offY{0.0};

private:
  //! The MPI ranks of all surrounding cells in order:
  //!  6 7 8    TL T TR
  //!  4 0 5     L C R
  //!  1 2 3    BL B BR
  std::array<int, 9> m_ranks{0, -1, -1, -1, -1, -1, -1, -1, -1};

  //! True if `Grid.finalize()` has been run on this cell / entry.
public:
  bool m_consistent{false};
};

#endif
