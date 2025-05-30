/*
 * Copyright(C) 1999-2021 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 *
 * See packages/seacas/LICENSE for details
 */
/*****************************************************************************
 *
 * testrd - read exodus file test.exo created by testwt
 *
 *****************************************************************************/

#include "exodusII.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include "drmd.h" */

int main(int argc, char **argv)
{
  ex_opts(EX_VERBOSE | EX_ABORT);

  /* open EXODUS II files */
  float version;
  int   CPU_word_size = 0; /* sizeof(float) */
  int   IO_word_size  = 0; /* use what is stored in file */

  int exoid = ex_open("test.exo",     /* filename path */
                      EX_READ,        /* access mode = READ */
                      &CPU_word_size, /* CPU word size */
                      &IO_word_size,  /* IO word size */
                      &version);      /* ExodusII library version */

  printf("\nafter ex_open\n");
  if (exoid < 0) {
    exit(1);
  }

  printf("test.exo is an EXODUSII file; version %4.2f\n", version);
  /*   printf ("         CPU word size %1d\n",CPU_word_size);  */
  printf("         I/O word size %1d\n", IO_word_size);
  int   idum;
  char *cdum = NULL;
  ex_inquire(exoid, EX_INQ_API_VERS, &idum, &version, cdum);
  printf("EXODUSII API; version %4.2f\n", version);

  ex_inquire(exoid, EX_INQ_LIB_VERS, &idum, &version, cdum);
  printf("EXODUSII Library API; version %4.2f (%d)\n", version, idum);

  /* read database parameters */

  char title[MAX_LINE_LENGTH + 1];
  int  num_dim, num_nodes, num_elem, num_elem_blk, num_node_sets;
  int  num_side_sets;
  int  error = ex_get_init(exoid, title, &num_dim, &num_nodes, &num_elem, &num_elem_blk,
                           &num_node_sets, &num_side_sets);

  printf("after ex_get_init, error = %3d\n", error);

  printf("database parameters:\n");
  printf("title =  '%s'\n", title);
  printf("num_dim = %3d\n", num_dim);
  printf("num_nodes = %3d\n", num_nodes);
  printf("num_elem = %3d\n", num_elem);
  printf("num_elem_blk = %3d\n", num_elem_blk);
  printf("num_node_sets = %3d\n", num_node_sets);
  printf("num_side_sets = %3d\n", num_side_sets);

  /* Check that ex_inquire gives same title */
  float fdum;
  char  title_chk[MAX_LINE_LENGTH + 1];
  error = ex_inquire(exoid, EX_INQ_TITLE, &idum, &fdum, title_chk);
  printf(" after ex_inquire, error = %d\n", error);
  if (strcmp(title, title_chk) != 0) {
    printf("error in ex_inquire for EX_INQ_TITLE\n");
  }

  /* Verify that `ex_get_init_global` does not crash on a serial file which does not have the
     dimensions being queried.
  */
  {
    int nng, neg, nebg, nnsg, nssg;
    error = ex_get_init_global(exoid, &nng, &neg, &nebg, &nnsg, &nssg);
    if (error != EX_NOERR) {
      printf("after ex_get_init_global, error = %3d\n", error);
    }
  }

  /* read nodal coordinates values and names from database */

  float *x = (float *)calloc(num_nodes, sizeof(float));
  float *y = (num_dim >= 2) ? (float *)calloc(num_nodes, sizeof(float)) : NULL;
  float *z = (num_dim >= 3) ? (float *)calloc(num_nodes, sizeof(float)) : NULL;

  error = ex_get_coord(exoid, x, y, z);
  printf("\nafter ex_get_coord, error = %3d\n", error);

  printf("x coords = \n");
  for (int i = 0; i < num_nodes; i++) {
    printf("%5.1f\n", x[i]);
  }

  if (num_dim >= 2) {
    printf("y coords = \n");
    for (int i = 0; i < num_nodes; i++) {
      printf("%5.1f\n", y[i]);
    }
  }
  if (num_dim >= 3) {
    printf("z coords = \n");
    for (int i = 0; i < num_nodes; i++) {
      printf("%5.1f\n", z[i]);
    }
  }

  /*
    error = ex_get_1_coord (exoid, 2, x, y, z);
    printf ("\nafter ex_get_1_coord, error = %3d\n", error);

    printf ("x coord of node 2 = \n");
    printf ("%f \n", x[0]);

    printf ("y coord of node 2 = \n");
    printf ("%f \n", y[0]);
  */
  free(x);
  if (num_dim >= 2) {
    free(y);
  }
  if (num_dim >= 3) {
    free(z);
  }

  char *coord_names[3];
  for (int i = 0; i < num_dim; i++) {
    coord_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
  }

  error = ex_get_coord_names(exoid, coord_names);
  printf("\nafter ex_get_coord_names, error = %3d\n", error);
  printf("x coord name = '%s'\n", coord_names[0]);
  if (num_dim > 1) {
    printf("y coord name = '%s'\n", coord_names[1]);
  }
  if (num_dim > 2) {
    printf("z coord name = '%s'\n", coord_names[2]);
  }

  for (int i = 0; i < num_dim; i++) {
    free(coord_names[i]);
  }

  {
    int num_attrs = 0;
    error         = ex_get_attr_param(exoid, EX_NODAL, 0, &num_attrs);
    printf(" after ex_get_attr_param, error = %d\n", error);
    printf("num nodal attributes = %d\n", num_attrs);
    if (num_attrs > 0) {
      char *attrib_names[10];
      for (int j = 0; j < num_attrs; j++) {
        attrib_names[j] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
      }
      error = ex_get_attr_names(exoid, EX_NODAL, 0, attrib_names);
      printf(" after ex_get_attr_names, error = %d\n", error);

      if (error == 0) {
        float *attrib = (float *)calloc(num_nodes, sizeof(float));
        for (int j = 0; j < num_attrs; j++) {
          printf("nodal attribute %d = '%s'\n", j, attrib_names[j]);
          error = ex_get_one_attr(exoid, EX_NODAL, 0, j + 1, attrib);
          printf(" after ex_get_one_attr, error = %d\n", error);
          for (int i = 0; i < num_nodes; i++) {
            printf("%5.1f\n", attrib[i]);
          }
          free(attrib_names[j]);
        }
        free(attrib);
      }
    }
  }

  /* read element order map */

  int *elem_map = (int *)calloc(num_elem, sizeof(int));

  error = ex_get_id_map(exoid, EX_ELEM_MAP, elem_map);
  printf("\nafter ex_get_id_map, error = %3d\n", error);

  for (int i = 0; i < num_elem; i++) {
    printf("elem_id_map(%d) = %d \n", i, elem_map[i]);
  }
  /* NOTE: elem_map used below */

  /* read element block parameters */

  int *num_elem_in_block = NULL;
  if (num_elem_blk > 0) {
    int *ids                = (int *)calloc(num_elem_blk, sizeof(int));
    num_elem_in_block       = (int *)calloc(num_elem_blk, sizeof(int));
    int *num_nodes_per_elem = (int *)calloc(num_elem_blk, sizeof(int));
    int *num_attr           = (int *)calloc(num_elem_blk, sizeof(int));

    error = ex_get_ids(exoid, EX_ELEM_BLOCK, ids);
    printf("\nafter ex_get_elem_blk_ids, error = %3d\n", error);

    char *block_names[10];
    for (int i = 0; i < num_elem_blk; i++) {
      block_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_names(exoid, EX_ELEM_BLOCK, block_names);
    printf("\nafter ex_get_names, error = %3d\n", error);

    for (int i = 0; i < num_elem_blk; i++) {
      char name[MAX_STR_LENGTH + 1];
      ex_get_name(exoid, EX_ELEM_BLOCK, ids[i], name);
      if (strcmp(name, block_names[i]) != 0) {
        printf("error in ex_get_name for block id %d\n", ids[i]);
      }
      char elem_type[MAX_STR_LENGTH + 1];
      error = ex_get_block(exoid, EX_ELEM_BLOCK, ids[i], elem_type, &(num_elem_in_block[i]),
                           &(num_nodes_per_elem[i]), 0, 0, &(num_attr[i]));
      printf("\nafter ex_get_elem_block, error = %d\n", error);

      printf("element block id = %2d\n", ids[i]);
      printf("element type = '%s'\n", elem_type);
      printf("num_elem_in_block = %2d\n", num_elem_in_block[i]);
      printf("num_nodes_per_elem = %2d\n", num_nodes_per_elem[i]);
      printf("num_attr = %2d\n", num_attr[i]);
      printf("name = '%s'\n", block_names[i]);
      free(block_names[i]);
    }

    /* Read per-block id map and compare to overall id map... */
#if 0
    int offset = 0;
#endif
    for (int i = 0; i < num_elem_blk; i++) {
      int *block_map = (int *)calloc(num_elem_in_block[i], sizeof(int));
      error          = ex_get_block_id_map(exoid, EX_ELEM_BLOCK, ids[i], block_map);

      /* Compare values with overall id map */
#if 0
      for (int j = 0; j < num_elem_in_block[i]; j++) {
	assert(block_map[j] == elem_map[offset + j]);
      }
      offset += num_elem_in_block[i];
#endif
      free(block_map);
    }

    /* read element block properties */
    int num_props = ex_inquire_int(exoid, EX_INQ_EB_PROP);
    printf("\nafter ex_inquire, error = %d\n", error);
    printf("\nThere are %2d properties for each element block\n", num_props);

    char *prop_names[3];
    for (int i = 0; i < num_props; i++) {
      prop_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_prop_names(exoid, EX_ELEM_BLOCK, prop_names);
    printf("after ex_get_prop_names, error = %d\n", error);

    for (int i = 1; i < num_props; i++) /* Prop 1 is id; skip that here */
    {
      for (int j = 0; j < num_elem_blk; j++) {
        int prop_value;
        error = ex_get_prop(exoid, EX_ELEM_BLOCK, ids[j], prop_names[i], &prop_value);
        if (error == 0) {
          printf("element block %2d, property(%2d): '%s'= %5d\n", j + 1, i + 1, prop_names[i],
                 prop_value);
        }
        else {
          printf("after ex_get_prop, error = %d\n", error);
        }
      }
    }

    for (int i = 0; i < num_props; i++) {
      free(prop_names[i]);
    }

    /* read element connectivity */

    for (int i = 0; i < num_elem_blk; i++) {
      if (num_elem_in_block[i] > 0) {
        int *connect = (int *)calloc((num_nodes_per_elem[i] * num_elem_in_block[i]), sizeof(int));

        error = ex_get_conn(exoid, EX_ELEM_BLOCK, ids[i], connect, NULL, NULL);
        printf("\nafter ex_get_elem_conn, error = %d\n", error);

        printf("connect array for elem block %2d\n", ids[i]);

        for (int j = 0; j < num_nodes_per_elem[i]; j++) {
          printf("%3d\n", connect[j]);
        }
        free(connect);
      }
    }

    /* read element block attributes */
    for (int i = 0; i < num_elem_blk; i++) {
      if (num_elem_in_block[i] > 0 && num_attr[i] > 0) {
        char *attrib_names[10];
        for (int j = 0; j < num_attr[i]; j++) {
          attrib_names[j] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
        }

        float *attrib = (float *)calloc(num_attr[i] * num_elem_in_block[i], sizeof(float));
        error         = ex_get_attr(exoid, EX_ELEM_BLOCK, ids[i], attrib);
        printf("\n after ex_get_elem_attr, error = %d\n", error);

        if (error == 0) {
          error = ex_get_attr_names(exoid, EX_ELEM_BLOCK, ids[i], attrib_names);
          printf(" after ex_get_elem_attr_names, error = %d\n", error);

          if (error == 0) {
            printf("element block %d attribute '%s' = %6.4f\n", ids[i], attrib_names[0], *attrib);
          }
        }
        free(attrib);
        for (int j = 0; j < num_attr[i]; j++) {
          free(attrib_names[j]);
        }
      }
    }

    free(ids);
    free(num_nodes_per_elem);
    free(num_attr);
  }
  free(elem_map);

  /* read individual node sets */
  if (num_node_sets > 0) {
    int *ids = (int *)calloc(num_node_sets, sizeof(int));

    error = ex_get_ids(exoid, EX_NODE_SET, ids);
    printf("\nafter ex_get_node_set_ids, error = %3d\n", error);

    char *nset_names[10];
    for (int i = 0; i < num_node_sets; i++) {
      nset_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_names(exoid, EX_NODE_SET, nset_names);
    printf("\nafter ex_get_names, error = %3d\n", error);

    for (int i = 0; i < num_node_sets; i++) {
      char name[MAX_STR_LENGTH + 1];
      ex_get_name(exoid, EX_NODE_SET, ids[i], name);
      if (strcmp(name, nset_names[i]) != 0) {
        printf("error in ex_get_name for nodeset id %d\n", ids[i]);
      }

      int num_nodes_in_set;
      int num_df_in_set;
      error = ex_get_set_param(exoid, EX_NODE_SET, ids[i], &num_nodes_in_set, &num_df_in_set);
      printf("\nafter ex_get_node_set_param, error = %3d\n", error);

      printf("\nnode set %2d parameters: \n", ids[i]);
      printf("num_nodes = %2d\n", num_nodes_in_set);
      printf("name = '%s'\n", nset_names[i]);
      free(nset_names[i]);
      int   *node_list = (int *)calloc(num_nodes_in_set, sizeof(int));
      float *dist_fact = (float *)calloc(num_nodes_in_set, sizeof(float));

      error = ex_get_set(exoid, EX_NODE_SET, ids[i], node_list, NULL);
      printf("\nafter ex_get_node_set, error = %3d\n", error);

      if (num_df_in_set > 0) {
        error = ex_get_set_dist_fact(exoid, EX_NODE_SET, ids[i], dist_fact);
        printf("\nafter ex_get_node_set_dist_fact, error = %3d\n", error);
      }

      printf("\nnode list for node set %2d\n", ids[i]);

      for (int j = 0; j < num_nodes_in_set; j++) {
        printf("%3d\n", node_list[j]);
      }

      if (num_df_in_set > 0) {
        printf("dist factors for node set %2d\n", ids[i]);

        for (int j = 0; j < num_df_in_set; j++) {
          printf("%5.2f\n", dist_fact[j]);
        }
      }
      else {
        printf("no dist factors for node set %2d\n", ids[i]);
      }

      free(node_list);
      free(dist_fact);

      {
        int num_attrs = 0;
        error         = ex_get_attr_param(exoid, EX_NODE_SET, ids[i], &num_attrs);
        printf(" after ex_get_attr_param, error = %d\n", error);
        printf("num nodeset attributes for nodeset %d = %d\n", ids[i], num_attrs);
        if (num_attrs > 0) {
          char *attrib_names[10];
          for (int j = 0; j < num_attrs; j++) {
            attrib_names[j] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
          }
          error = ex_get_attr_names(exoid, EX_NODE_SET, ids[i], attrib_names);
          printf(" after ex_get_attr_names, error = %d\n", error);

          if (error == 0) {
            float *attrib = (float *)calloc(num_nodes_in_set, sizeof(float));
            for (int j = 0; j < num_attrs; j++) {
              printf("nodeset attribute %d = '%s'\n", j, attrib_names[j]);
              error = ex_get_one_attr(exoid, EX_NODE_SET, ids[i], j + 1, attrib);
              printf(" after ex_get_one_attr, error = %d\n", error);
              for (int k = 0; k < num_nodes_in_set; k++) {
                printf("%5.1f\n", attrib[k]);
              }
              free(attrib_names[j]);
            }
            free(attrib);
          }
        }
      }
    }
    free(ids);

    /* read node set properties */
    int num_props = ex_inquire_int(exoid, EX_INQ_NS_PROP);
    printf("\nafter ex_inquire, error = %d\n", error);
    printf("\nThere are %2d properties for each node set\n", num_props);

    char *prop_names[10];
    for (int i = 0; i < num_props; i++) {
      prop_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }
    int *prop_values = (int *)calloc(num_node_sets, sizeof(int));

    error = ex_get_prop_names(exoid, EX_NODE_SET, prop_names);
    printf("after ex_get_prop_names, error = %d\n", error);

    for (int i = 0; i < num_props; i++) {
      error = ex_get_prop_array(exoid, EX_NODE_SET, prop_names[i], prop_values);
      if (error == 0) {
        for (int j = 0; j < num_node_sets; j++) {
          printf("node set %2d, property(%2d): '%s'= %5d\n", j + 1, i + 1, prop_names[i],
                 prop_values[j]);
        }
      }
      else {
        printf("after ex_get_prop_array, error = %d\n", error);
      }
    }
    for (int i = 0; i < num_props; i++) {
      free(prop_names[i]);
    }
    free(prop_values);
  }
  /* read concatenated node sets; this produces the same information as
   * the above code which reads individual node sets
   */

  int *num_nodes_per_set = NULL;
  if (num_node_sets > 0) {
    int *ids            = (int *)calloc(num_node_sets, sizeof(int));
    num_nodes_per_set   = (int *)calloc(num_node_sets, sizeof(int));
    int *num_df_per_set = (int *)calloc(num_node_sets, sizeof(int));
    int *node_ind       = (int *)calloc(num_node_sets, sizeof(int));
    int *df_ind         = (int *)calloc(num_node_sets, sizeof(int));

    int list_len = ex_inquire_int(exoid, EX_INQ_NS_NODE_LEN);
    printf("\nafter ex_inquire: EX_INQ_NS_NODE_LEN = %d, error = %3d\n", list_len, error);
    int *node_list = (int *)calloc(list_len, sizeof(int));

    list_len = ex_inquire_int(exoid, EX_INQ_NS_DF_LEN);
    printf("\nafter ex_inquire: EX_INQ_NS_DF_LEN = %d, error = %3d\n", list_len, error);
    float *dist_fact = (float *)calloc(list_len, sizeof(float));

    {
      struct ex_set_specs set_specs;

      set_specs.sets_ids            = ids;
      set_specs.num_entries_per_set = num_nodes_per_set;
      set_specs.num_dist_per_set    = num_df_per_set;
      set_specs.sets_entry_index    = node_ind;
      set_specs.sets_dist_index     = df_ind;
      set_specs.sets_entry_list     = node_list;
      set_specs.sets_extra_list     = NULL;
      set_specs.sets_dist_fact      = dist_fact;

      error = ex_get_concat_sets(exoid, EX_NODE_SET, &set_specs);
    }
    printf("\nafter ex_get_concat_node_sets, error = %3d\n", error);

    printf("\nconcatenated node set info\n");

    printf("ids = \n");
    for (int i = 0; i < num_node_sets; i++) {
      printf("%3d\n", ids[i]);
    }

    printf("num_nodes_per_set = \n");
    for (int i = 0; i < num_node_sets; i++) {
      printf("%3d\n", num_nodes_per_set[i]);
    }

    printf("node_ind = \n");
    for (int i = 0; i < num_node_sets; i++) {
      printf("%3d\n", node_ind[i]);
    }

    printf("node_list = \n");
    for (int i = 0; i < list_len; i++) {
      printf("%3d\n", node_list[i]);
    }

    printf("dist_fact = \n");
    for (int i = 0; i < list_len; i++) {
      printf("%5.3f\n", dist_fact[i]);
    }

    free(ids);
    free(df_ind);
    free(node_ind);
    free(num_df_per_set);
    free(node_list);
    free(dist_fact);
  }

  /* read individual side sets */
  if (num_side_sets > 0) {
    int *ids = (int *)calloc(num_side_sets, sizeof(int));

    error = ex_get_ids(exoid, EX_SIDE_SET, ids);
    printf("\nafter ex_get_side_set_ids, error = %3d\n", error);

    char *sset_names[10];
    for (int i = 0; i < num_side_sets; i++) {
      sset_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_names(exoid, EX_SIDE_SET, sset_names);
    printf("\nafter ex_get_names, error = %3d\n", error);

    for (int i = 0; i < num_side_sets; i++) {
      char name[MAX_STR_LENGTH + 1];
      ex_get_name(exoid, EX_SIDE_SET, ids[i], name);
      if (strcmp(name, sset_names[i]) != 0) {
        printf("error in ex_get_name for sideset id %d\n", ids[i]);
      }

      int num_sides_in_set;
      int num_df_in_set;
      error = ex_get_set_param(exoid, EX_SIDE_SET, ids[i], &num_sides_in_set, &num_df_in_set);
      printf("\nafter ex_get_side_set_param, error = %3d\n", error);

      printf("side set %2d parameters:\n", ids[i]);
      printf("name = '%s'\n", sset_names[i]);
      printf("num_sides = %3d\n", num_sides_in_set);
      printf("num_dist_factors = %3d\n", num_df_in_set);
      free(sset_names[i]);

      /* Note: The # of elements is same as # of sides!  */
      int    num_elem_in_set = num_sides_in_set;
      int   *elem_list       = (int *)calloc(num_elem_in_set, sizeof(int));
      int   *side_list       = (int *)calloc(num_sides_in_set, sizeof(int));
      int   *node_ctr_list   = (int *)calloc(num_elem_in_set, sizeof(int));
      int   *node_list       = (int *)calloc(num_elem_in_set * 21, sizeof(int));
      float *dist_fact       = (float *)calloc(num_df_in_set, sizeof(float));

      error = ex_get_set(exoid, EX_SIDE_SET, ids[i], elem_list, side_list);
      printf("\nafter ex_get_side_set, error = %3d\n", error);

      error = ex_get_side_set_node_list(exoid, ids[i], node_ctr_list, node_list);
      printf("\nafter ex_get_side_set_node_list, error = %3d\n", error);

      if (num_df_in_set > 0) {
        error = ex_get_set_dist_fact(exoid, EX_SIDE_SET, ids[i], dist_fact);
        printf("\nafter ex_get_side_set_dist_fact, error = %3d\n", error);
      }

      printf("element list for side set %2d\n", ids[i]);
      for (int j = 0; j < num_elem_in_set; j++) {
        printf("%3d\n", elem_list[j]);
      }

      printf("side list for side set %2d\n", ids[i]);
      for (int j = 0; j < num_sides_in_set; j++) {
        printf("%3d\n", side_list[j]);
      }

      int node_ctr = 0;
      printf("node list for side set %2d\n", ids[i]);
      for (int k = 0; k < num_elem_in_set; k++) {
        for (int j = 0; j < node_ctr_list[k]; j++) {
          printf("%3d\n", node_list[node_ctr + j]);
        }
        node_ctr += node_ctr_list[k];
      }

      if (num_df_in_set > 0) {
        printf("dist factors for side set %2d\n", ids[i]);

        for (int j = 0; j < num_df_in_set; j++) {
          printf("%5.3f\n", dist_fact[j]);
        }
      }
      else {
        printf("no dist factors for side set %2d\n", ids[i]);
      }

      free(elem_list);
      free(side_list);
      free(node_ctr_list);
      free(node_list);
      free(dist_fact);
    }

    /* read side set properties */
    int num_props = ex_inquire_int(exoid, EX_INQ_SS_PROP);
    printf("\nafter ex_inquire, error = %d\n", error);
    printf("\nThere are %2d properties for each side set\n", num_props);

    char *prop_names[10];
    for (int i = 0; i < num_props; i++) {
      prop_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_prop_names(exoid, EX_SIDE_SET, prop_names);
    printf("after ex_get_prop_names, error = %d\n", error);

    for (int i = 0; i < num_props; i++) {
      for (int j = 0; j < num_side_sets; j++) {
        int prop_value;
        error = ex_get_prop(exoid, EX_SIDE_SET, ids[j], prop_names[i], &prop_value);
        if (error == 0) {
          printf("side set %2d, property(%2d): '%s'= %5d\n", j + 1, i + 1, prop_names[i],
                 prop_value);
        }
        else {
          printf("after ex_get_prop, error = %d\n", error);
        }
      }
    }
    for (int i = 0; i < num_props; i++) {
      free(prop_names[i]);
    }
    free(ids);
  }

  int *num_elem_per_set = NULL;
  if (num_side_sets > 0) {
    int elem_list_len = ex_inquire_int(exoid, EX_INQ_SS_ELEM_LEN);
    printf("\nafter ex_inquire: EX_INQ_SS_ELEM_LEN = %d,  error = %d\n", elem_list_len, error);

    int node_list_len = ex_inquire_int(exoid, EX_INQ_SS_NODE_LEN);
    printf("\nafter ex_inquire: EX_INQ_SS_NODE_LEN = %d,  error = %d\n", node_list_len, error);

    int df_list_len = ex_inquire_int(exoid, EX_INQ_SS_DF_LEN);
    printf("\nafter ex_inquire: EX_INQ_SS_DF_LEN = %d,  error = %d\n", df_list_len, error);

    /* read concatenated side sets; this produces the same information as
     * the above code which reads individual side sets
     */

    /* concatenated side set read */

    struct ex_set_specs set_specs;

    int *ids              = (int *)calloc(num_side_sets, sizeof(int));
    num_elem_per_set      = (int *)calloc(num_side_sets, sizeof(int));
    int   *num_df_per_set = (int *)calloc(num_side_sets, sizeof(int));
    int   *elem_ind       = (int *)calloc(num_side_sets, sizeof(int));
    int   *df_ind         = (int *)calloc(num_side_sets, sizeof(int));
    int   *elem_list      = (int *)calloc(elem_list_len, sizeof(int));
    int   *side_list      = (int *)calloc(elem_list_len, sizeof(int));
    float *dist_fact      = (float *)calloc(df_list_len, sizeof(float));

    set_specs.sets_ids            = ids;
    set_specs.num_entries_per_set = num_elem_per_set;
    set_specs.num_dist_per_set    = num_df_per_set;
    set_specs.sets_entry_index    = elem_ind;
    set_specs.sets_dist_index     = df_ind;
    set_specs.sets_entry_list     = elem_list;
    set_specs.sets_extra_list     = side_list;
    set_specs.sets_dist_fact      = dist_fact;

    error = ex_get_concat_sets(exoid, EX_SIDE_SET, &set_specs);
    printf("\nafter ex_get_concat_side_sets, error = %3d\n", error);

    printf("concatenated side set info\n");

    printf("ids = \n");
    for (int i = 0; i < num_side_sets; i++) {
      printf("%3d\n", ids[i]);
    }

    printf("num_elem_per_set = \n");
    for (int i = 0; i < num_side_sets; i++) {
      printf("%3d\n", num_elem_per_set[i]);
    }

    printf("num_dist_per_set = \n");
    for (int i = 0; i < num_side_sets; i++) {
      printf("%3d\n", num_df_per_set[i]);
    }

    printf("elem_ind = \n");
    for (int i = 0; i < num_side_sets; i++) {
      printf("%3d\n", elem_ind[i]);
    }

    printf("dist_ind = \n");
    for (int i = 0; i < num_side_sets; i++) {
      printf("%3d\n", df_ind[i]);
    }

    printf("elem_list = \n");
    for (int i = 0; i < elem_list_len; i++) {
      printf("%3d\n", elem_list[i]);
    }

    printf("side_list = \n");
    for (int i = 0; i < elem_list_len; i++) {
      printf("%3d\n", side_list[i]);
    }

    printf("dist_fact = \n");
    for (int i = 0; i < df_list_len; i++) {
      printf("%5.3f\n", dist_fact[i]);
    }

    free(ids);
    free(num_df_per_set);
    free(df_ind);
    free(elem_ind);
    free(elem_list);
    free(side_list);
    free(dist_fact);
  }
  /* end of concatenated side set read */

  /* read QA records */

  int num_qa_rec = ex_inquire_int(exoid, EX_INQ_QA);

  char *qa_record[2][4];
  for (int i = 0; i < num_qa_rec; i++) {
    for (int j = 0; j < 4; j++) {
      qa_record[i][j] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }
  }

  error = ex_get_qa(exoid, qa_record);
  printf("\nafter ex_get_qa, error = %3d\n", error);

  printf("QA records = \n");
  for (int i = 0; i < num_qa_rec; i++) {
    for (int j = 0; j < 4; j++) {
      printf(" '%s'\n", qa_record[i][j]);
      free(qa_record[i][j]);
    }
  }

  /* read information records */

  int num_info = ex_inquire_int(exoid, EX_INQ_INFO);
  printf("\nafter ex_inquire, error = %3d\n", error);

  char *info[3];
  for (int i = 0; i < num_info; i++) {
    info[i] = (char *)calloc((MAX_LINE_LENGTH + 1), sizeof(char));
  }

  error = ex_get_info(exoid, info);
  printf("\nafter ex_get_info, error = %3d\n", error);

  printf("info records = \n");
  for (int i = 0; i < num_info; i++) {
    printf(" '%s'\n", info[i]);
    free(info[i]);
  }

  /* read global variables parameters and names */

  int num_glo_vars;
  error = ex_get_variable_param(exoid, EX_GLOBAL, &num_glo_vars);
  printf("\nafter ex_get_variable_param, error = %3d\n", error);

  if (num_glo_vars > 0) {
    char *var_names[3];
    for (int i = 0; i < num_glo_vars; i++) {
      var_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_variable_names(exoid, EX_GLOBAL, num_glo_vars, var_names);
    printf("\nafter ex_get_variable_names, error = %3d\n", error);

    printf("There are %2d global variables; their names are :\n", num_glo_vars);
    for (int i = 0; i < num_glo_vars; i++) {
      printf(" '%s'\n", var_names[i]);
      free(var_names[i]);
    }
  }

  /* read nodal variables parameters and names */
  int num_nod_vars = 0;
  if (num_nodes > 0) {
    error = ex_get_variable_param(exoid, EX_NODAL, &num_nod_vars);
    printf("\nafter ex_get_variable_param, error = %3d\n", error);

    char *var_names[3];
    for (int i = 0; i < num_nod_vars; i++) {
      var_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_variable_names(exoid, EX_NODAL, num_nod_vars, var_names);
    printf("\nafter ex_get_variable_names, error = %3d\n", error);

    printf("There are %2d nodal variables; their names are :\n", num_nod_vars);
    for (int i = 0; i < num_nod_vars; i++) {
      printf(" '%s'\n", var_names[i]);
      free(var_names[i]);
    }
  }

  /* read element variables parameters and names */

  int num_ele_vars = 0;
  if (num_elem > 0) {
    error = ex_get_variable_param(exoid, EX_ELEM_BLOCK, &num_ele_vars);
    printf("\nafter ex_get_variable_param, error = %3d\n", error);

    char *var_names[3];
    for (int i = 0; i < num_ele_vars; i++) {
      var_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
    }

    error = ex_get_variable_names(exoid, EX_ELEM_BLOCK, num_ele_vars, var_names);
    printf("\nafter ex_get_variable_names, error = %3d\n", error);

    printf("There are %2d element variables; their names are :\n", num_ele_vars);
    for (int i = 0; i < num_ele_vars; i++) {
      printf(" '%s'\n", var_names[i]);
      free(var_names[i]);
    }

    /* read element variable truth table */

    if (num_ele_vars > 0) {
      int *truth_tab = (int *)calloc((num_elem_blk * num_ele_vars), sizeof(int));

      error = ex_get_truth_table(exoid, EX_ELEM_BLOCK, num_elem_blk, num_ele_vars, truth_tab);
      printf("\nafter ex_get_elem_var_tab, error = %3d\n", error);

      printf("This is the element variable truth table:\n");

      for (int i = 0; i < num_elem_blk * num_ele_vars; i++) {
        printf("%2d\n", truth_tab[i]);
      }
      free(truth_tab);
    }
  }

  /* read nodeset variables parameters and names */

  int num_nset_vars = 0;
  if (num_node_sets > 0) {
    error = ex_get_variable_param(exoid, EX_NODE_SET, &num_nset_vars);
    printf("\nafter ex_get_variable_param, error = %3d\n", error);

    if (num_nset_vars > 0) {
      char *var_names[3];
      for (int i = 0; i < num_nset_vars; i++) {
        var_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
      }

      error = ex_get_variable_names(exoid, EX_NODE_SET, num_nset_vars, var_names);
      printf("\nafter ex_get_variable_names, error = %3d\n", error);

      printf("There are %2d nodeset variables; their names are :\n", num_nset_vars);
      for (int i = 0; i < num_nset_vars; i++) {
        printf(" '%s'\n", var_names[i]);
        free(var_names[i]);
      }

      /* read nodeset variable truth table */

      if (num_nset_vars > 0) {
        int *truth_tab = (int *)calloc((num_node_sets * num_nset_vars), sizeof(int));

        error = ex_get_truth_table(exoid, EX_NODE_SET, num_node_sets, num_nset_vars, truth_tab);
        printf("\nafter ex_get_nset_var_tab, error = %3d\n", error);

        printf("This is the nodeset variable truth table:\n");

        for (int i = 0; i < num_node_sets * num_nset_vars; i++) {
          printf("%2d\n", truth_tab[i]);
        }
        free(truth_tab);
      }
    }
  }

  /* read sideset variables parameters and names */
  int num_sset_vars = 0;
  if (num_side_sets > 0) {
    error = ex_get_variable_param(exoid, EX_SIDE_SET, &num_sset_vars);
    printf("\nafter ex_get_variable_param, error = %3d\n", error);

    if (num_sset_vars > 0) {
      char *var_names[3];
      for (int i = 0; i < num_sset_vars; i++) {
        var_names[i] = (char *)calloc((MAX_STR_LENGTH + 1), sizeof(char));
      }

      error = ex_get_variable_names(exoid, EX_SIDE_SET, num_sset_vars, var_names);
      printf("\nafter ex_get_variable_names, error = %3d\n", error);

      printf("There are %2d sideset variables; their names are :\n", num_sset_vars);
      for (int i = 0; i < num_sset_vars; i++) {
        printf(" '%s'\n", var_names[i]);
        free(var_names[i]);
      }

      /* read sideset variable truth table */

      if (num_sset_vars > 0) {
        int *truth_tab = (int *)calloc((num_side_sets * num_sset_vars), sizeof(int));

        error = ex_get_truth_table(exoid, EX_SIDE_SET, num_side_sets, num_sset_vars, truth_tab);
        printf("\nafter ex_get_sset_var_tab, error = %3d\n", error);

        printf("This is the sideset variable truth table:\n");

        for (int i = 0; i < num_side_sets * num_sset_vars; i++) {
          printf("%2d\n", truth_tab[i]);
        }
        free(truth_tab);
      }
    }
  }

  /* determine how many time steps are stored */
  int num_time_steps = ex_inquire_int(exoid, EX_INQ_TIME);
  printf("\nafter ex_inquire, error = %3d\n", error);
  printf("There are %2d time steps in the database.\n", num_time_steps);

  /* read time value at one time step */

  int   time_step = 3;
  float time_value;
  error = ex_get_time(exoid, time_step, &time_value);
  printf("\nafter ex_get_time, error = %3d\n", error);

  printf("time value at time step %2d = %5.3f\n", time_step, time_value);

  /* read time values at all time steps */

  float *time_values = (float *)calloc(num_time_steps, sizeof(float));

  error = ex_get_all_times(exoid, time_values);
  printf("\nafter ex_get_all_times, error = %3d\n", error);

  printf("time values at all time steps are:\n");
  for (int i = 0; i < num_time_steps; i++) {
    printf("%5.3f\n", time_values[i]);
  }

  free(time_values);

  /* read all global variables at one time step */
  int var_index = 1;
  int beg_time  = 1;
  int end_time  = -1;

  if (num_glo_vars > 0) {
    float *var_values = (float *)calloc(num_glo_vars, sizeof(float));

    error = ex_get_var(exoid, time_step, EX_GLOBAL, 1, 1, num_glo_vars, var_values);
    printf("\nafter ex_get_glob_vars, error = %3d\n", error);

    printf("global variable values at time step %2d\n", time_step);
    for (int i = 0; i < num_glo_vars; i++) {
      printf("%5.3f\n", var_values[i]);
    }

    free(var_values);

    /* read a single global variable through time */

    var_values = (float *)calloc(num_time_steps, sizeof(float));

    error = ex_get_var_time(exoid, EX_GLOBAL, var_index, 1, beg_time, end_time, var_values);
    printf("\nafter ex_get_glob_var_time, error = %3d\n", error);

    printf("global variable %2d values through time:\n", var_index);
    for (int i = 0; i < num_time_steps; i++) {
      printf("%5.3f\n", var_values[i]);
    }

    free(var_values);
  }

  /* read a nodal variable at one time step */
  if (num_nodes > 0) {
    float *var_values = (float *)calloc(num_nodes, sizeof(float));

    error = ex_get_var(exoid, time_step, EX_NODAL, var_index, 1, num_nodes, var_values);
    printf("\nafter ex_get_nodal_var, error = %3d\n", error);

    printf("nodal variable %2d values at time step %2d\n", var_index, time_step);
    for (int i = 0; i < num_nodes; i++) {
      printf("%5.3f\n", var_values[i]);
    }

    free(var_values);

    /* read a nodal variable through time */

    var_values = (float *)calloc(num_time_steps, sizeof(float));

    int node_num = 1;
    error = ex_get_var_time(exoid, EX_NODAL, var_index, node_num, beg_time, end_time, var_values);
    printf("\nafter ex_get_nodal_var_time, error = %3d\n", error);

    printf("nodal variable %2d values for node %2d through time:\n", var_index, node_num);
    for (int i = 0; i < num_time_steps; i++) {
      printf("%5.3f\n", var_values[i]);
    }

    free(var_values);
  }
  /* read an element variable at one time step */

  if (num_elem_blk > 0) {
    int *ids = (int *)calloc(num_elem_blk, sizeof(int));

    error = ex_get_ids(exoid, EX_ELEM_BLOCK, ids);
    printf("\n after ex_get_elem_blk_ids, error = %3d\n", error);

    for (int i = 0; i < num_elem_blk; i++) {
      if (num_elem_in_block[i] > 0) {
        float *var_values = (float *)calloc(num_elem_in_block[i], sizeof(float));

        error = ex_get_var(exoid, time_step, EX_ELEM_BLOCK, var_index, ids[i], num_elem_in_block[i],
                           var_values);
        printf("\nafter ex_get_elem_var, error = %3d\n", error);

        if (!error) {
          printf("element variable %2d values of element block %2d at time step %2d\n", var_index,
                 ids[i], time_step);
          for (int j = 0; j < num_elem_in_block[i]; j++) {
            printf("%5.3f\n", var_values[j]);
          }
        }

        free(var_values);
      }
    }
    free(num_elem_in_block);
    free(ids);
  }
  /* read an element variable through time */

  if (num_ele_vars > 0) {
    float *var_values = (float *)calloc(num_time_steps, sizeof(float));

    var_index    = 2;
    int elem_num = 2;
    error =
        ex_get_var_time(exoid, EX_ELEM_BLOCK, var_index, elem_num, beg_time, end_time, var_values);
    printf("\nafter ex_get_elem_var_time, error = %3d\n", error);

    printf("element variable %2d values for element %2d through time:\n", var_index, elem_num);
    for (int i = 0; i < num_time_steps; i++) {
      printf("%5.3f\n", var_values[i]);
    }

    free(var_values);
  }

  /* read a sideset variable at one time step */

  if (num_sset_vars > 0) {
    int *ids = (int *)calloc(num_side_sets, sizeof(int));

    error = ex_get_ids(exoid, EX_SIDE_SET, ids);
    printf("\n after ex_get_side_set_ids, error = %3d\n", error);

    for (int i = 0; i < num_side_sets; i++) {
      float *var_values = (float *)calloc(num_elem_per_set[i], sizeof(float));

      error = ex_get_var(exoid, time_step, EX_SIDE_SET, var_index, ids[i], num_elem_per_set[i],
                         var_values);
      printf("\nafter ex_get_sset_var, error = %3d\n", error);

      if (!error) {
        printf("sideset variable %2d values of sideset %2d at time step %2d\n", var_index, ids[i],
               time_step);
        for (int j = 0; j < num_elem_per_set[i]; j++) {
          printf("%5.3f\n", var_values[j]);
        }
      }

      free(var_values);
    }
    free(ids);
  }
  free(num_elem_per_set);

  /* read a nodeset variable at one time step */

  if (num_nset_vars > 0) {
    int *ids = (int *)calloc(num_node_sets, sizeof(int));

    error = ex_get_ids(exoid, EX_NODE_SET, ids);
    printf("\n after ex_get_node_set_ids, error = %3d\n", error);

    for (int i = 0; i < num_node_sets; i++) {
      float *var_values = (float *)calloc(num_nodes_per_set[i], sizeof(float));

      error = ex_get_var(exoid, time_step, EX_NODE_SET, var_index, ids[i], num_nodes_per_set[i],
                         var_values);
      printf("\nafter ex_get_nset_var, error = %3d\n", error);

      if (!error) {
        printf("nodeset variable %2d values of nodeset %2d at time step %2d\n", var_index, ids[i],
               time_step);
        for (int j = 0; j < num_nodes_per_set[i]; j++) {
          printf("%5.3f\n", var_values[j]);
        }
      }

      free(var_values);
    }
    free(ids);
  }
  if (num_node_sets > 0) {
    free(num_nodes_per_set);
  }

  error = ex_close(exoid);
  printf("\nafter ex_close, error = %3d\n", error);
  return 0;
}
