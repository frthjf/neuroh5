'''
This script reads a text file with Layer-Section-Node connectivity
and creates a CCS or CRS representation of in an HDF5 file. We assume that
the entries are sorted first by column index and then by row index The
datasets are extensible and GZIP compression was applied.

Layer-Section-Node connectivity has the following format:

src dest weight layer section node

'''

import h5py
import numpy as np
import sys, os
import click

def h5_get_group (h, groupname):
    if groupname in h.keys():
        g = h[groupname]
    else:
        g = h.create_group(groupname)
    return g

def h5_get_dataset (g, dsetname, **kwargs):
    if dsetname in g.keys():
        dset = g[dsetname]
    else:
        dset = g.create_dataset(dsetname, (0,), **kwargs)
    return dset

def h5_concat_dataset(dset, data):
    dsize = dset.shape[0]
    print "dsize = ", dsize
    newshape = (dsize+len(data),)
    print "newshape = ", newshape
    dset.resize(newshape)
    dset[dsize:] = data
    return dset

@click.group()
def cli():
    return

@cli.command(name="import-lsn")
@click.argument("groupname", type=str, default="lsn")
@click.argument("inputfiles", type=click.Path(exists=True), nargs=-1)
@click.argument("outputfile", type=click.Path())
@click.option('--ccs', 'order', flag_value='ccs', default=True)
@click.option('--crs', 'order', flag_value='crs')
@click.option("--colsep", type=str, default=' ')
def import_lsn(inputfiles, outputfile, groupname, order, colsep):

    for inputfile in inputfiles:

        click.echo("Importing file: %s\n" % inputfile) 
        f = open(inputfile)
        lines = f.readlines()
        f.close()

        if order=='ccs':

            col_ptr = [0]
            row_idx = []
            syn_weight = []
            layer = []
            seg_idx = []
            node_idx = []
        
            # read and and parse line-by-line
        
            col_old = 0
        
            for l in lines:
                a = l.split(colsep)
                col = int(a[1])-1
                if col > col_old:
                    col_ptr.append(len(syn_weight))
                    col_old = col
                row_idx.append(int(a[0])-1)
                syn_weight.append(float(a[2]))
                layer.append(int(a[3]))
                seg_idx.append(int(a[4])-1)
                node_idx.append(int(a[5])-1)
                
            col_ptr.append(len(syn_weight))

            with h5py.File(outputfile, "a", libver="latest") as h5:

                g1 = h5_get_group (h5, groupname)

                # maxshape=(None,) makes a dataset dimension unlimited.
                # compression=6 applies GZIP compression level 6
                # h5py picks a chunk size for us, but we could also set
                # that manually
                dset = h5_get_dataset(g1, "col_ptr", maxshape=(None,),
                                      dtype=np.uint32, compression=6)
                dset = h5_concat_dataset(dset, np.asarray(col_ptr))
                
                dset = h5_get_dataset(g1, "row_idx", maxshape=(None,),
                                      dtype=np.uint32, compression=6)
                dset = h5_concat_dataset(dset, np.asarray(row_idx))
                
                # for floating point numbers, it's usally beneficial to apply the
                # bit-shuffle filter before compressing with GZIP
                dset = h5_get_dataset(g1, "Synaptic weight", 
                                      maxshape=(None,), dtype=np.float32,
                                      compression=6, shuffle=True)
                dset = h5_concat_dataset(dset, np.asarray(syn_weight))
                
                # create an HDF5 enumerated type for the layer information
                mapping = {"GRANULE_LAYER": 1, "INNER_MOLECULAR_LAYER": 2,
                           "MIDDLE_MOLECULAR_LAYER": 3, "OUTER_MOLECULAR_LAYER": 4}
                dt = h5py.special_dtype(enum=(np.uint8, mapping))
                dset = h5_get_dataset(g1, "Layer", maxshape=(None,),
                                         dtype=dt, compression=6)
                dset = h5_concat_dataset(dset, np.asarray(layer))
                
                dset = h5_get_dataset(g1, "seg_idx", maxshape=(None,),
                                      dtype=np.uint16, compression=6)
                dset = h5_concat_dataset(dset, np.asarray(seg_idx))
                
                dset = h5_get_dataset(g1, "node_idx", maxshape=(None,),
                                      dtype=np.uint16, compression=6)
                dset = h5_concat_dataset(dset, np.asarray(node_idx))

        elif order=='crs':

            for inputfile in inputfiles:

                row_ptr = [0]
                col_idx = []
                syn_weight = []
                layer = []
                seg_idx = []
                node_idx = []
                
                row_old = 0
                
            for l in lines:
                a = l.split(colsep)
                row = int(a[0])-1
                if row > row_old:
                    row_ptr.append(len(syn_weight))
                    row_old = row
                col_idx.append(int(a[1])-1)
                syn_weight.append(float(a[2]))
                layer.append(int(a[3]))
                seg_idx.append(int(a[4])-1)
                node_idx.append(int(a[5])-1)
                
            row_ptr.append(len(syn_weight))

            with h5py.File(outputfile, "a", libver="latest") as h5:

                g1 = h5_get_group (h5, groupname)
                
                dset = h5_get_dataset(g1, "row_ptr", dtype=np.uint32)
                dset = h5_concat_dataset(dset, np.asarray(row_ptr))
                
                dset = h5_get_dataset(g1, "col_idx", dtype=np.uint32)
                dset = h5_concat_dataset(dset, np.asarray(col_idx))
                
                dset = h5_get_dataset(g1, "Synaptic weight", 
                                         dtype=np.float32)
                dset = h5_concat_dataset(dset, np.asarray(syn_weight))
                
                # create an HDF5 enumerated type for the layer information
                mapping = {"GRANULE_LAYER": 1, "INNER_MOLECULAR_LAYER": 2,
                        "MIDDLE_MOLECULAR_LAYER": 3, "OUTER_MOLECULAR_LAYER": 4}
                dt = h5py.special_dtype(enum=(np.uint8, mapping))
                dset = h5_get_dataset(g1, "Layer", dtype=dt)
                dset = h5_concat_dataset(dset, np.asarray(layer))
                
                dset = h5_get_dataset(g1, "seg_idx", dtype=np.uint16)
                dset = h5_concat_dataset(dset, np.asarray(seg_idx))
                
                dset = h5_get_dataset(g1, "node_idx", dtype=np.uint16)
                dset = h5_concat_dataset(dset, np.asarray(node_idx))

            
