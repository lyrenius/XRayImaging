#!/bin/bash

#SBATCH -J mystery
#SBATCH -p cnmix
#SBATCH -N 1
#SBATCH -o logs/stdout.%j
#SBATCH -e logs/stderr.%j
#SBATCH --ntasks-per-node=56

source ~/.bashrc
module load compilers/gcc/v12.2.0
conda activate mystery

CFITSIO_PATH=$PWD/cfitsio

g++ -std=c++20 -O3 -march=native trans.cpp \
  -I$CFITSIO_PATH/include \
  -L$CFITSIO_PATH/lib \
  -lcfitsio -lm \
  -o trans

g++ -O3 -o ratio_calculate ratio_calculate.cpp

bash run.sh
