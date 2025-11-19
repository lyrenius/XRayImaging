#!/bin/bash

wget https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio_latest.tar.gz
tar xf cfitsio_latest.tar.gz
CFITSIO_PATH=$PWD/cfitsio
cd cfitsio-*
./configure --prefix=$PWD/cfitsio
make -j