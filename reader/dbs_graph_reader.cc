#include "dbs_graph_reader.hh"

#include "ngh5paths.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <cstring>

#define MAX_PRJ_NAME 1024

using namespace std;

bool debug_enabled = true;

void DEBUG(){}

template<typename First, typename ...Rest>
void DEBUG(First && first, Rest && ...rest)
{
  if (debug_enabled)
    {
      std::cerr << std::forward<First>(first);
      DEBUG(std::forward<Rest>(rest)...);
    }
}

std::string ngh5_prj_path (const char *dsetname, const char *name) 
{
  std::string result;
  result = std::string("/Projections/") + dsetname + name;
  return result;
}


/*****************************************************************************
 * Read projection names
 *****************************************************************************/

herr_t read_projection_names
(
 MPI_Comm                                comm,
 const char*                             fname, 
 vector<string> &prj_vector
 )
{
  herr_t ierr = 0;

  int rank, size;
  assert(MPI_Comm_size(comm, &size) >= 0);
  assert(MPI_Comm_rank(comm, &rank) >= 0);

  // MPI rank 0 reads and broadcasts the number of ranges 
  hsize_t num_projections, i;
  hid_t file = -1, grp = -1;
  char prj_name[MAX_PRJ_NAME]; char *prj_names_buf;
  size_t prj_names_total_length = 0;
  vector<char> prj_names;
  vector<uint64_t> prj_name_lengths;

  // process 0 reads the names of projections and broadcasts
  if (rank == 0)
    {
      file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
      assert(file >= 0);

      grp = H5Gopen(file, string("/Projections").c_str(), H5P_DEFAULT);
      
      assert(H5Gget_num_objs(grp, &num_projections)>=0);
    }

  assert(MPI_Bcast(&num_projections, 1, MPI_UINT64_T, 0, comm) >= 0);

  // allocate buffer
  prj_name_lengths.resize(num_projections);
  prj_vector.resize(num_projections);

  // MPI rank 0 reads and broadcasts the projection names
  if (rank == 0)
    {
      for (i = 0; i < num_projections; i++) 
        {
          size_t len;
          /* For each object in the group, get the name */
          assert(H5Gget_objname_by_idx(grp, i, prj_name, (size_t)MAX_PRJ_NAME )>0);
          DEBUG("Projection ",i," is named ",prj_name,"\n"); 
          len = strlen(prj_name);
          prj_vector[i] = string(prj_name);
          prj_name_lengths[i] = len;
          prj_names_total_length =  prj_names_total_length + len;
        }

      assert(H5Gclose(grp) >= 0);
      assert(H5Fclose(file) >= 0);
    }

  // Broadcast projection name lengths
  assert(MPI_Bcast(&prj_names_total_length, 1, MPI_UINT64_T, 0, comm) >= 0);
  assert(MPI_Bcast(&prj_name_lengths[0], num_projections, MPI_UINT64_T, 0, comm) >= 0);
  
  // Broadcast projection names
  assert((prj_names_buf = (char *)malloc(prj_names_total_length)) != NULL);
  assert(MPI_Bcast(prj_names_buf, prj_names_total_length, MPI_BYTE, 0, comm) >= 0);

  // Copy projection names into prj_vector
  size_t offset = 0;
  for (i = 0; i < num_projections; i++) 
    {
      size_t len = prj_name_lengths[i];
      memcpy(prj_name, prj_names_buf+offset, len);
      prj_name[len] = '\0';
      prj_vector[i] = string(prj_name);
      offset = offset + len;
    }

  free(prj_names_buf);
  
  return ierr;
}

/*****************************************************************************
 * Read the basic DBS graph structure
 *****************************************************************************/

herr_t read_dbs_projection
(
 MPI_Comm            comm,
 const char*         fname, 
 const char*         dsetname, 
 const vector<pop_range_t> &pop_vector,
 NODE_IDX_T&         base,
 NODE_IDX_T&         dst_start,
 NODE_IDX_T&         src_start,
 vector<DST_BLK_PTR_T>&  dst_blk_ptr,
 vector<NODE_IDX_T>& dst_idx,
 vector<DST_PTR_T>&  dst_ptr,
 vector<NODE_IDX_T>& src_idx
 )
{
  herr_t ierr = 0;
  unsigned int rank, size;
  assert(MPI_Comm_size(comm, (int*)&size) >= 0);
  assert(MPI_Comm_rank(comm, (int*)&rank) >= 0);

  /***************************************************************************
   * MPI rank 0 reads and broadcasts the number of nodes
   ***************************************************************************/

  uint64_t num_blocks; 


  // process 0 reads the size of dst_blk_ptr and the source and target populations
  if (rank == 0)
    {
      uint16_t dst_pop, src_pop;
      hid_t file, fspace, mspace, dset;
      hsize_t one = 1;

      file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
      assert(file >= 0);
      dset = H5Dopen2(file, ngh5_prj_path(dsetname, DST_BLK_PTR_H5_PATH).c_str(), H5P_DEFAULT);
      assert(dset >= 0);
      fspace = H5Dget_space(dset);
      assert(fspace >= 0);
      num_blocks = (uint64_t) H5Sget_simple_extent_npoints(fspace) - 1;
      assert(num_blocks > 0);

      assert(H5Sclose(fspace) >= 0);
      assert(H5Dclose(dset) >= 0);

      mspace = H5Screate_simple(1, &one, NULL);
      assert(mspace >= 0);
      ierr = H5Sselect_all(mspace);
      assert(ierr >= 0);

      dset = H5Dopen2(file, ngh5_prj_path(dsetname, DST_POP_H5_PATH).c_str(), H5P_DEFAULT);
      assert(dset >= 0);
      fspace = H5Dget_space(dset);
      assert(fspace >= 0);
  
      //ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, NULL, NULL, &one, &one);
      //assert(ierr >= 0);

      ierr = H5Dread(dset, NODE_IDX_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &dst_pop);
      assert(ierr >= 0);

      assert(H5Sclose(fspace) >= 0);
      assert(H5Dclose(dset) >= 0);
      assert(H5Sclose(mspace) >= 0);

      mspace = H5Screate_simple(1, &one, NULL);
      assert(mspace >= 0);
      ierr = H5Sselect_all(mspace);
      assert(ierr >= 0);

      dset = H5Dopen2(file, ngh5_prj_path(dsetname, SRC_POP_H5_PATH).c_str(), H5P_DEFAULT);
      assert(dset >= 0);
      fspace = H5Dget_space(dset);
      assert(fspace >= 0);
  
      //ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, NULL, NULL, &one, &one);
      //assert(ierr >= 0);

      ierr = H5Dread(dset, NODE_IDX_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &src_pop);
      assert(ierr >= 0);

      assert(H5Sclose(fspace) >= 0);
      assert(H5Dclose(dset) >= 0);
      assert(H5Sclose(mspace) >= 0);
      assert(H5Fclose(file) >= 0);

      dst_start = pop_vector[dst_pop].start;
      src_start = pop_vector[src_pop].start;

      DEBUG("num_blocks = ", num_blocks,
            " dst_start = ", dst_start,
            " src_start = ", src_start,
            "\n");
    }

  assert(MPI_Bcast(&num_blocks, 1, MPI_UINT64_T, 0, comm) >= 0);
  assert(MPI_Bcast(&dst_start, 1, MPI_UINT32_T, 0, comm) >= 0);
  assert(MPI_Bcast(&src_start, 1, MPI_UINT32_T, 0, comm) >= 0);

  /***************************************************************************
   * read BLOCK_PTR
   ***************************************************************************/

  // determine my block of block_ptr
  hsize_t start, stop, ppcount, block;

  if (num_blocks < size)
    { ppcount = 1; }
  else
    { ppcount = (hsize_t) num_blocks/size; }
    
  start = (hsize_t) rank*ppcount;
  stop = (hsize_t) (rank+1)*ppcount + 1;
  base = (NODE_IDX_T) start;

  // patch the last rank
  if ((num_blocks < size) && (rank == size-1))
    { stop = (hsize_t) num_blocks+1; }
  else if ((num_blocks >= size) && (rank == num_blocks-1))
    { stop = (hsize_t) num_blocks+1; };

  DEBUG("rank ", rank, ": ", "ppcount = ", ppcount, " start = ", start, " stop = ", stop, "\n");

  if ((num_blocks - 1) < rank)
    { block = 0; }
  else 
    { block = stop - start; }

  DEBUG("rank ", rank, ": ", "block = ", block, "\n");
  std::cerr << std::flush;

  // allocate buffer and memory dataspace
  dst_blk_ptr.resize(block);
  
  if (rank <= (num_blocks - 1))
    { assert(dst_blk_ptr.size() > 0); }

  DEBUG("rank ", rank, ": ", "dst_blk_ptr.size = ", dst_blk_ptr.size(), "\n");

  hid_t mspace = H5Screate_simple(1, &block, NULL);
  assert(mspace >= 0);
  ierr = H5Sselect_all(mspace);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "after create_simple\n");
  std::cerr << std::flush;
  // open the file (independent I/O)
  DEBUG("rank ", rank, ": ", "before open\n");
  std::cerr << std::flush;

  hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
  assert(fapl >= 0);
  assert(H5Pset_fapl_mpio(fapl, comm, MPI_INFO_NULL) >= 0);

  hid_t file = H5Fopen(fname, H5F_ACC_RDONLY, fapl);
  assert(file >= 0);
  DEBUG("rank ", rank, ": ", "after open\n");
  std::cerr << std::flush;
  hid_t dset = H5Dopen2(file, ngh5_prj_path(dsetname, DST_BLK_PTR_H5_PATH).c_str(), H5P_DEFAULT);
  assert(dset >= 0);
  DEBUG("rank ", rank, ": ", "after open2\n");
  std::cerr << std::flush;

  // make hyperslab selection
  hid_t fspace = H5Dget_space(dset);
  assert(fspace >= 0);
  hsize_t one = 1;
  
  DEBUG("rank ", rank, ": ", "before hyperslab\n");
  std::cerr << std::flush;

  ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL, &one, &block);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "before read\n");

  ierr = H5Dread(dset, DST_BLK_PTR_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &dst_blk_ptr[0]);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "after read\n");

  assert(H5Sclose(fspace) >= 0);
  assert(H5Dclose(dset) >= 0);
  assert(H5Sclose(mspace) >= 0);

  // rebase the block_ptr array to local offsets
  // REBASE is going to be the start offset for the hyperslab
  DST_BLK_PTR_T block_rebase = 0;
  if (rank <= (num_blocks - 1))
    {
      block_rebase = dst_blk_ptr[0];
      DEBUG("rank ", rank, ": ", "block_rebase = ", block_rebase, "\n");
      
      for (size_t i = 0; i < dst_blk_ptr.size(); ++i)
        {
          dst_blk_ptr[i] -= block_rebase;
        }
    }

  /***************************************************************************
   * read DST_IDX
   ***************************************************************************/

  // determine my read block of dst_idx

  if (rank <= (num_blocks - 1))
    {
      block = block - 1;
      dst_idx.resize(block);
      assert(dst_idx.size() > 0);
    }
  
  DEBUG("rank ", rank, ": ", "dst_idx: block = ", block, "\n");
  DEBUG("rank ", rank, ": ", "dst_idx: start = ", start, "\n");

  mspace = H5Screate_simple(1, &block, NULL);
  assert(mspace >= 0);
  ierr = H5Sselect_all(mspace);
  assert(ierr >= 0);

  dset = H5Dopen2(file, ngh5_prj_path(dsetname, DST_IDX_H5_PATH).c_str(), H5P_DEFAULT);
  assert(dset >= 0);

  // make hyperslab selection
  fspace = H5Dget_space(dset);
  assert(fspace >= 0);
  ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL, &one, &block);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "dst_idx: before read\n");
  ierr = H5Dread(dset, NODE_IDX_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &dst_idx[0]);
  assert(ierr >= 0);
  DEBUG("rank ", rank, ": ", "dst_idx: after read\n");

  assert(H5Sclose(fspace) >= 0);
  assert(H5Dclose(dset) >= 0);
  assert(H5Sclose(mspace) >= 0);


  /***************************************************************************
   * read DST_PTR
   ***************************************************************************/

  // determine my read block of dst_ptr
  if (rank <= (num_blocks - 1))
    {
      block = (hsize_t)(dst_blk_ptr.back() - dst_blk_ptr.front());
      start = (hsize_t)block_rebase;
      dst_ptr.resize(block);
      assert(dst_ptr.size() > 0);
    }

  DEBUG("rank ", rank, ": ", "dst_ptr: block = ", block, "\n");
  DEBUG("rank ", rank, ": ", "dst_ptr: start = ", start, "\n");

  // allocate buffer and memory dataspace

  mspace = H5Screate_simple(1, &block, NULL);
  assert(mspace >= 0);
  ierr = H5Sselect_all(mspace);
  assert(ierr >= 0);

  dset = H5Dopen2(file, ngh5_prj_path(dsetname, DST_PTR_H5_PATH).c_str(), H5P_DEFAULT);
  assert(dset >= 0);

  // make hyperslab selection
  fspace = H5Dget_space(dset);
  assert(fspace >= 0);
  ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL, &one, &block);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "dst_ptr: before read\n");
  ierr = H5Dread(dset, DST_PTR_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &dst_ptr[0]);
  assert(ierr >= 0);
  DEBUG("rank ", rank, ": ", "dst_ptr: after read\n");

  assert(H5Sclose(fspace) >= 0);
  assert(H5Dclose(dset) >= 0);
  assert(H5Sclose(mspace) >= 0);

  DST_PTR_T dst_rebase;
  if (rank <= (num_blocks - 1))
    {
      dst_rebase = dst_ptr[0];
      for (size_t i = 0; i < dst_ptr.size(); ++i)
        {
          dst_ptr[i] -= dst_rebase;
        }
    }

  /***************************************************************************
   * read SRC_IDX
   ***************************************************************************/

  // determine my read block of dst_idx
  if (rank <= (num_blocks - 1))
    {
      block = (hsize_t)(dst_ptr.back() - dst_ptr.front());
      start = (hsize_t)dst_rebase;
      // allocate buffer and memory dataspace
      src_idx.resize(block);
      assert(src_idx.size() > 0);
    }

  mspace = H5Screate_simple(1, &block, NULL);
  assert(mspace >= 0);
  ierr = H5Sselect_all(mspace);
  assert(ierr >= 0);

  dset = H5Dopen2(file, ngh5_prj_path(dsetname, SRC_IDX_H5_PATH).c_str(), H5P_DEFAULT);
  assert(dset >= 0);

  // make hyperslab selection
  fspace = H5Dget_space(dset);
  assert(fspace >= 0);
  ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL, &one, &block);
  assert(ierr >= 0);

  DEBUG("rank ", rank, ": ", "src_idx: before read\n");
  ierr = H5Dread(dset, NODE_IDX_H5_NATIVE_T, mspace, fspace, H5P_DEFAULT, &src_idx[0]);
  assert(ierr >= 0);
  DEBUG("rank ", rank, ": ", "src_idx: after read\n");

  assert(H5Sclose(fspace) >= 0);
  assert(H5Dclose(dset) >= 0);
  assert(H5Sclose(mspace) >= 0);

  assert(H5Fclose(file) >= 0);

  return ierr;
}
