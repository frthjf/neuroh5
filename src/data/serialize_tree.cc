// -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//==============================================================================
///  @file serialize_tree.cc
///
///  Top-level functions for serializing/deserializing tree structures.
///
///  Copyright (C) 2016-2021 Project NeuroH5.
//==============================================================================

#include "debug.hh"

#include "serialize_tree.hh"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <set>
#include <bitset>
#include <climits>
#include <map>
#include <vector>
#include <forward_list>

#include "cereal/archives/binary.hpp"

#include "throw_assert.hh"

using namespace std;
using namespace neuroh5;

namespace neuroh5
{
  namespace data
  {
      
    void serialize_rank_tree_map (const size_t num_ranks,
                                  const size_t start_rank,
                                  const map <rank_t, map<CELL_IDX_T, neurotree_t> >& rank_tree_map,
                                  vector<int>& sendcounts,
                                  vector<char> &sendbuf,
                                  vector<int> &sdispls)
    {
      vector<int> rank_sequence;

      sdispls.resize(num_ranks);
      sendcounts.resize(num_ranks);

      int end_rank = num_ranks;
      throw_assert(start_rank < end_rank, "serialize_rank_tree_map: invalid start rank");
      
      // Recommended all-to-all communication pattern: start at the current rank, then wrap around;
      // (as opposed to starting at rank 0)
      for (int key_rank = start_rank; key_rank < end_rank; key_rank++)
        {
          rank_sequence.push_back(key_rank);
        }
      for (int key_rank = 0; key_rank < (int)start_rank; key_rank++)
        {
          rank_sequence.push_back(key_rank);
        }

      size_t sendpos = 0;
      for (const int& key_rank : rank_sequence)
        {
          std::stringstream ss(ios::in | ios::out | ios::binary); 
          sdispls[key_rank] = sendpos;
          
          auto it1 = rank_tree_map.find(key_rank);
          if (it1 != rank_tree_map.end())
            {
              const map<CELL_IDX_T, neurotree_t>& tree_map = it1->second;
              {
                
                cereal::BinaryOutputArchive oarchive(ss); // Create an output archive
                oarchive(tree_map); // Write the data to the archive
                
              } // archive goes out of scope, ensuring all contents are flushed
              const string& sstr = ss.str();
              sendbuf.reserve(sendbuf.size() + sstr.size());
              copy(sstr.begin(), sstr.end(), back_inserter(sendbuf));
              
              sendpos = sendbuf.size();
            }
          sendcounts[key_rank] = sendpos - sdispls[key_rank];

        }
      
    }
    


    void deserialize_rank_tree_map (const size_t num_ranks,
                                    const vector<char> &recvbuf,
                                    const vector<int>& recvcounts,
                                    const vector<int>& rdispls,
                                    map<CELL_IDX_T, neurotree_t> &all_tree_map
                                    )
    {
      const int recvbuf_size = recvbuf.size();

      for (size_t ridx = 0; ridx < num_ranks; ridx++)
        {
          if (recvcounts[ridx] > 0)
            {
              int recvsize  = recvcounts[ridx];
              int recvpos   = rdispls[ridx];
              int startpos  = recvpos;
              map<CELL_IDX_T, neurotree_t> tree_map;

              throw_assert(recvpos < recvbuf_size,
                           "deserialize_rank_tree_map: invalid buffer displacement");

              {
                const string& s = string(recvbuf.begin()+startpos, recvbuf.begin()+startpos+recvsize);
                stringstream ss(s, ios::in | ios::out | ios::binary);

                cereal::BinaryInputArchive iarchive(ss); // Create an input archive
                
                iarchive(tree_map); // Read the data from the archive
              }

              all_tree_map.insert(tree_map.cbegin(), tree_map.cend());

            }
        }
    }

    void deserialize_rank_tree_list (const size_t num_ranks,
                                     const vector<char> &recvbuf,
                                     const vector<int>& recvcounts,
                                     const vector<int>& rdispls,
                                     forward_list<neurotree_t> &all_tree_list
                                     )
    {
      const int recvbuf_size = recvbuf.size();

      for (size_t ridx = 0; ridx < num_ranks; ridx++)
        {
          if (recvcounts[ridx] > 0)
            {
              int recvsize  = recvcounts[ridx];
              int recvpos   = rdispls[ridx];
              int startpos  = recvpos;
              map<CELL_IDX_T, neurotree_t> tree_map;
              throw_assert(recvpos < recvbuf_size,
                           "deserialize_rank_tree_vector: invalid buffer displacement");
              
              {
                const string& s = string(recvbuf.begin()+startpos, recvbuf.begin()+startpos+recvsize);
                stringstream ss(s, ios::in | ios::out | ios::binary);

                cereal::BinaryInputArchive iarchive(ss); // Create an input archive
                
                iarchive(tree_map); // Read the data from the archive
              }

              for (auto const& iter : tree_map)
                {
                  all_tree_list.push_front(iter.second);
                }
            }
        }
    }

  }
}
