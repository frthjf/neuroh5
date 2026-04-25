#ifndef APPEND_TEMPLATE
#define APPEND_TEMPLATE

#include <hdf5.h>

#include <cstdio>
#include <string>
#include <vector>

#include "throw_assert.hh"

namespace neuroh5
{
  namespace hdf5
  {
    template<class T>
    herr_t append
    (
     hid_t&             file,
     const std::string& name,
     const hsize_t&     newsize,
     const hsize_t&     start,
     const hsize_t&     len,
     const hid_t&       ntype,
     const std::vector<T>&    v,
     const hid_t& wapl
     )
    {

      herr_t ierr = 0;

      hid_t dset = H5Dopen2(file, name.c_str(), H5P_DEFAULT);
      throw_assert(dset >= 0, "error in H5Dopen2");

      if (newsize > 0)
        {
          ierr = H5Dset_extent (dset, &newsize);
          throw_assert(ierr >= 0, "error in H5Dset_extent");
        }

      // make hyperslab selection
      hid_t fspace = H5Dget_space(dset);
      throw_assert(fspace >= 0, "error in H5Dget_space");
      hsize_t one = 1;
      hid_t mspace = H5Screate_simple(1, &len, NULL);
      throw_assert(mspace >= 0, "error in H5Screate_simple");

      if (len > 0)
        {
          ierr = H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL,
                                     &one, &len);
          throw_assert(ierr >= 0, "error in H5Sselect_hyperslab");
          ierr = H5Sselect_all(mspace);
          throw_assert(ierr >= 0, "error in H5Sselect_all");
        }
      else
        {
          ierr = H5Sselect_none(fspace);
          throw_assert(ierr >= 0, "error in H5Sselect_none");
          ierr = H5Sselect_none(mspace);
          throw_assert(ierr >= 0, "error in H5Sselection_none");
        }


      // On ranks with len == 0, H5Sselect_none has been used above so HDF5
      // will not read from the buffer; still pass a valid pointer to avoid
      // dereferencing an empty std::vector (undefined behavior).
      T sentinel;
      const void* buf = (len > 0) ? static_cast<const void*>(&v[0])
                                  : static_cast<const void*>(&sentinel);
      ierr = H5Dwrite(dset, ntype, mspace, fspace, wapl, buf);
      throw_assert(ierr >= 0, "error in H5Dwrite");

      ierr = H5Sclose(mspace);
      throw_assert(ierr >= 0, "error in H5Sclose");
      ierr = H5Sclose(fspace)
      throw_assert(ierr >= 0, "error in H5Sclose");
      ierr = H5Dclose(dset);
      throw_assert(ierr >= 0, "error in H5Dclose");

      return ierr;
    }
  }
}

#endif
