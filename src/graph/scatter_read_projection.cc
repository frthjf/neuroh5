// -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//==============================================================================
///  @file scatter_read_projection.cc
///
///  Top-level functions for reading edges in DBS (Destination Block Sparse)
///  format.
///
///  Copyright (C) 2016-2017 Project Neurograph.
//==============================================================================

#include "debug.hh"

#include "neuroh5_types.hh"
#include "read_projection_datasets.hh"
#include "edge_attributes.hh"
#include "cell_populations.hh"
#include "validate_edge_list.hh"
#include "scatter_read_projection.hh"
#include "bcast_template.hh"
#include "alltoallv_template.hh"
#include "serialize_edge.hh"
#include "serialize_data.hh"
#include "append_rank_edge_map.hh"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <set>
#include <map>
#include <vector>

#undef NDEBUG
#include <cassert>

using namespace std;

namespace neuroh5
{
  namespace graph
  {
    
    /*****************************************************************************
     * Load and scatter edge data structures 
     *****************************************************************************/

    int scatter_read_projection (MPI_Comm all_comm, const int io_size, EdgeMapType edge_map_type, 
                            const string& file_name, const string& src_pop_name, const string& dst_pop_name, 
                            const vector<string> &attr_namespaces,
                            const map<NODE_IDX_T, rank_t>&  node_rank_map,
                            const vector<pop_range_t>& pop_vector,
                            const map<NODE_IDX_T,pair<uint32_t,pop_t> >& pop_ranges,
                            const vector< pair<pop_t, string> >& pop_labels,
                            const set< pair<pop_t, pop_t> >& pop_pairs,
                            vector < edge_map_t >& prj_vector,
                            vector < map <string, vector < vector<string> > > > & edge_attr_names_vector,
                            size_t &local_num_nodes, size_t &local_num_edges, size_t &total_num_edges,
                            size_t offset, size_t numitems)
    {
      // MPI Communicator for I/O ranks
      MPI_Comm io_comm;
      // MPI group color value used for I/O ranks
      int io_color = 1;

      int rank, size;
      assert(MPI_Comm_size(all_comm, &size) >= 0);
      assert(MPI_Comm_rank(all_comm, &rank) >= 0);

      // Am I an I/O rank?
      if (rank < io_size)
        {
          MPI_Comm_split(all_comm,io_color,rank,&io_comm);
          MPI_Comm_set_errhandler(io_comm, MPI_ERRORS_RETURN);
        }
      else
        {
          MPI_Comm_split(all_comm,0,rank,&io_comm);
        }
      MPI_Barrier(all_comm);

      vector<NODE_IDX_T> send_edges, recv_edges, total_recv_edges;
      rank_edge_map_t prj_rank_edge_map;
      edge_map_t prj_edge_map;
      size_t num_edges = 0;
      map<string, vector< vector<string> > > edge_attr_names;
      
      local_num_nodes=0; local_num_edges=0;
      
      {
        vector<char> recvbuf;
        vector<int> recvcounts, rdispls;

        {
          vector<char> sendbuf; 
          vector<int> sendcounts(size,0), sdispls(size,0);

          DEBUG("projection ", src_pop_name, " -> ", dst_pop_name, "\n");

          if (rank < (int)io_size)
            {
              DST_BLK_PTR_T block_base;
              DST_PTR_T edge_base, edge_count;
              NODE_IDX_T dst_start, src_start;
              vector<DST_BLK_PTR_T> dst_blk_ptr;
              vector<NODE_IDX_T> dst_idx;
              vector<DST_PTR_T> dst_ptr;
              vector<NODE_IDX_T> src_idx;
              map<string, data::NamedAttrVal> edge_attr_map;
            
              uint32_t dst_pop_idx=0, src_pop_idx=0;
              bool src_pop_set = false, dst_pop_set = false;
            
              for (size_t i=0; i< pop_labels.size(); i++)
                {
                  if (src_pop_name == get<1>(pop_labels[i]))
                    {
                      src_pop_idx = get<0>(pop_labels[i]);
                      src_pop_set = true;
                    }
                  if (dst_pop_name == get<1>(pop_labels[i]))
                    {
                      dst_pop_idx = get<0>(pop_labels[i]);
                      dst_pop_set = true;
                    }
                }
              assert(dst_pop_set && src_pop_set);

              dst_start = pop_vector[dst_pop_idx].start;
              src_start = pop_vector[src_pop_idx].start;

              DEBUG("Task ",rank," scatter: reading projection ", src_pop_name, " -> ", dst_pop_name);
              assert(hdf5::read_projection_datasets(io_comm, file_name, src_pop_name, dst_pop_name,
                                                    dst_start, src_start, block_base, edge_base,
                                                    dst_blk_ptr, dst_idx, dst_ptr, src_idx,
                                                    total_num_edges, offset, numitems) >= 0);
          
              DEBUG("Task ",rank," scatter: validating projection ", src_pop_name, " -> ", dst_pop_name);
              // validate the edges
              assert(validate_edge_list(dst_start, src_start, dst_blk_ptr, dst_idx, dst_ptr, src_idx,
                                        pop_ranges, pop_pairs) == true);
          
          
              edge_count = src_idx.size();
              DEBUG("Task ",rank," scatter: reading attributes for ", src_pop_name, " -> ", dst_pop_name);
              for (string attr_namespace : attr_namespaces) 
                {
                  vector< pair<string,hid_t> > edge_attr_info;
                  assert(graph::get_edge_attributes(file_name, src_pop_name, dst_pop_name,
                                                    attr_namespace, edge_attr_info) >= 0);
                  assert(graph::read_all_edge_attributes(io_comm, file_name,
                                                         src_pop_name, dst_pop_name, attr_namespace,
                                                         edge_base, edge_count, edge_attr_info,
                                                         edge_attr_map[attr_namespace]) >= 0);
                  
                  edge_attr_map[attr_namespace].attr_names(edge_attr_names[attr_namespace]);
                }

              DEBUG("Task ",rank," scatter: creating rank edge map for ", src_pop_name, " -> ", dst_pop_name);

              // append to the edge map
              assert(data::append_rank_edge_map(dst_start, src_start, dst_blk_ptr, dst_idx, dst_ptr, src_idx,
                                                edge_attr_map, node_rank_map, num_edges, prj_rank_edge_map,
                                                edge_map_type) >= 0);
              
              DEBUG("scatter: read ", num_edges, " edges from projection ", src_pop_name, " -> ", dst_pop_name);
          
              // ensure that all edges in the projection have been read and appended to edge_list
              assert(num_edges == src_idx.size());
          
              size_t num_packed_edges = 0;
              DEBUG("scatter: packing edge data from projection ", src_pop_name, " -> ", dst_pop_name);
          
              data::serialize_rank_edge_map (size, rank, prj_rank_edge_map, 
                                             num_packed_edges, sendcounts, sendbuf, sdispls);

              // ensure the correct number of edges is being packed
              assert(num_packed_edges == num_edges);
              DEBUG("scatter: packed ", num_packed_edges, " edges from projection ", src_pop_name, " -> ", dst_pop_name);

            } // rank < io_size

          MPI_Comm_free(&io_comm);

          assert(mpi::alltoallv_vector<char>(all_comm, MPI_CHAR, sendcounts, sdispls, sendbuf,
                                             recvcounts, rdispls, recvbuf) >= 0);
        }

        if (recvbuf.size() > 0)
          {
            data::deserialize_rank_edge_map (size, recvbuf, recvcounts, rdispls, 
                                             prj_edge_map, local_num_nodes, local_num_edges);
          }
      }
      
      DEBUG("scatter: rank ", rank, " unpacked ", local_num_edges, " edges for projection ", src_pop_name, " -> ", dst_pop_name);
      
      prj_vector.push_back(prj_edge_map);

      if (!attr_namespaces.empty())
        {
          vector<char> sendbuf; uint32_t sendbuf_size=0;
          if (rank == 0)
            {
              data::serialize_data(edge_attr_names, sendbuf);
              sendbuf_size = sendbuf.size();
            }
          
          assert(MPI_Bcast(&sendbuf_size, 1, MPI_UINT32_T, 0, all_comm) >= 0);
          sendbuf.resize(sendbuf_size);
          assert(MPI_Bcast(&sendbuf[0], sendbuf_size, MPI_CHAR, 0, all_comm) >= 0);
          
          if (rank != 0)
            {
              data::deserialize_data(sendbuf, edge_attr_names);
            }
          edge_attr_names_vector.push_back(edge_attr_names);
          
          DEBUG("scatter: finished broadcasting attribute names for projection ", src_pop_name, " -> ", dst_pop_name);
        }
      

      return 0;
    }


  }
  
}
