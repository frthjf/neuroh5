#include "debug.hh"
#include "ngh5paths.h"
#include "ngh5types.hh"

#include "dbs_graph_reader.hh"
#include "population_reader.hh"

#include "hdf5.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <vector>

#include <mpi.h>

using namespace std;

/*****************************************************************************
 * Append src/dst node pairs to a list of edges
 *****************************************************************************/

int append_edge_list
(
 const NODE_IDX_T&         base,
 const NODE_IDX_T&         dst_start,
 const NODE_IDX_T&         src_start,
 const vector<DST_BLK_PTR_T>&  dst_blk_ptr,
 const vector<NODE_IDX_T>& dst_idx,
 const vector<DST_PTR_T>&  dst_ptr,
 const vector<NODE_IDX_T>& src_idx,
 vector<NODE_IDX_T>&       edge_list
 )
{
  int ierr = 0; size_t dst_ptr_size;
  
  if (dst_blk_ptr.size() > 0) 
    {
      dst_ptr_size = dst_ptr.size();
      for (size_t b = 0; b < dst_blk_ptr.size()-1; ++b)
        {
          size_t low_dst_ptr = dst_blk_ptr[b], high_dst_ptr = dst_blk_ptr[b+1];
          NODE_IDX_T dst_base = dst_idx[b];
          for (size_t i = low_dst_ptr, ii = 0; i < high_dst_ptr; ++i, ++ii)
            {
              if (i < dst_ptr_size-1) 
                {
                  NODE_IDX_T dst = dst_base + ii + dst_start;
                  size_t low = dst_ptr[i], high = dst_ptr[i+1];
                  for (size_t j = low; j < high; ++j)
                    {
                      NODE_IDX_T src = src_idx[j] + src_start;
                      edge_list.push_back(src);
                      edge_list.push_back(dst);
                    }
                }
            }
        }
    }

  return ierr;
}


/*****************************************************************************
 * Main driver
 *****************************************************************************/

int main(int argc, char** argv)
{
  char *input_file_name;
  
  assert(MPI_Init(&argc, &argv) >= 0);

  int rank, size;
  assert(MPI_Comm_size(MPI_COMM_WORLD, &size) >= 0);
  assert(MPI_Comm_rank(MPI_COMM_WORLD, &rank) >= 0);

  // parse arguments

  if (argc < 2) 
    {
      std::cout << "Usage: reader <FILE> <DATASET> ..." << std::endl;
      exit(1);
    }

  input_file_name = argv[1];
 
  // read the population info
  set< pair<pop_t, pop_t> > pop_pairs;
  assert(read_population_combos(MPI_COMM_WORLD, input_file_name, pop_pairs) >= 0);

  vector<pop_range_t> pop_vector;
  map<NODE_IDX_T,pair<uint32_t,pop_t> > pop_ranges;
  assert(read_population_ranges(MPI_COMM_WORLD, input_file_name, pop_ranges, pop_vector) >= 0);

  vector<string> prj_names;
  assert(read_projection_names(MPI_COMM_WORLD, input_file_name, prj_names) >= 0);
      
  vector<NODE_IDX_T> edge_list;

  // read the edges
  for (size_t i = 0; i < prj_names.size(); i++)
    {
      NODE_IDX_T base, dst_start, src_start;
      vector<DST_BLK_PTR_T> dst_blk_ptr;
      vector<NODE_IDX_T> dst_idx;
      vector<DST_PTR_T> dst_ptr;
      vector<NODE_IDX_T> src_idx;

      printf("Reading projection %lu (%s)\n", i, prj_names[i].c_str());

      assert(read_dbs_projection(MPI_COMM_WORLD, input_file_name, prj_names[i].c_str(), 
                                 pop_vector, base, dst_start, src_start, dst_blk_ptr, dst_idx, dst_ptr, src_idx) >= 0);
      
      // validate the edges
      assert(validate_edge_list(base, dst_start, src_start, dst_blk_ptr, dst_idx, dst_ptr, src_idx, pop_ranges, pop_pairs) == true);
      
      // append to the partitioner input list
      assert(append_edge_list(base, dst_start, src_start, dst_blk_ptr, dst_idx, dst_ptr, src_idx, edge_list) >= 0);

      printf("Task %d projection %lu has %lu edges\n", rank, i, src_idx.size());

    }
  
  MPI_Barrier(MPI_COMM_WORLD);

  printf("Task %d has %lu edges\n", rank,  edge_list.size());
  
  if (edge_list.size() > 0) 
    {
      ofstream outfile;
      stringstream outfilename;
      assert(edge_list.size()%2 == 0);

      outfilename << string(input_file_name) << "." << rank << ".edges";
      outfile.open(outfilename.str());

      for (size_t i = 0, k = 0; i < edge_list.size()-1; i+=2, k++)
        {
          outfile << k << " " << edge_list[i] << " " << edge_list[i+1] << std::endl;
        }
      outfile.close();

    }

  MPI_Finalize();
  return 0;
}
