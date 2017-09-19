// -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//==============================================================================
///  @file serialize_cell_attributes.hh
///
///  Functions for serializing cell attributes.
///
///  Copyright (C) 2017 Project Neuroh5.
//==============================================================================

#include <mpi.h>
#include <cstring>
#include <vector>
#include <map>
#include <string>

#include "neuroh5_types.hh"
#include "attr_map.hh"

namespace neuroh5
{
  namespace data
  {

    void serialize_rank_attr_map (const size_t num_ranks,
                                  const size_t start_rank,
                                  const map <rank_t, AttrMap>& rank_attr_map,
                                  std::vector<int>& sendcounts,
                                  std::vector<char> &sendbuf,
                                  std::vector<int> &sdispls);

    void deserialize_rank_attr_map (const size_t num_ranks,
                                    const std::vector<char> &recvbuf,
                                    const std::vector<int>& recvcounts,
                                    const std::vector<int>& rdispls,
                                    AttrMap& all_attr_map);

    
  }
}
