// -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//==============================================================================
///  @file append_graph.hh
///
///  Top-level functions for writing graphs in DBS (Destination Block Sparse)
///  format.
///
///  Copyright (C) 2016-2017 Project NeuroH5.
//==============================================================================

#ifndef WRITE_GRAPH_HH
#define WRITE_GRAPH_HH

#include "neuroh5_types.hh"
#include "attr_map.hh"

#include <mpi.h>
#include <vector>

namespace neuroh5
{
  namespace graph
  {
    int append_graph
    (
     MPI_Comm         all_comm,
     const int        io_size_arg,
     const string&    file_name,
     const string&    src_pop_name,
     const string&    dst_pop_name,
     const vector<vector<string>>& edge_attr_names,
     const edge_map_t&  input_edge_map
     );

  }
}

#endif
