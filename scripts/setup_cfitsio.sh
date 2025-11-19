#!/bin/bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PARENT_DIR="$(dirname "$SCRIPT_DIR")"
cd $PARENT_DIR

wget https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio_latest.tar.gz
tar xf cfitsio_latest.tar.gz

CFITSIO_PATH=cfitsio
cd cfitsio-*
./configure --prefix=$PWD/../cfitsio
make -j