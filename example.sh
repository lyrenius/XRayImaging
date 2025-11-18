#!/bin/bash

#SBATCH -J mystery
#SBATCH -p cnmix
#SBATCH -N 1
#SBATCH -o logs/stdout.%j
#SBATCH -e logs/stderr.%j
#SBATCH --ntasks-per-node=56

source ~/.bashrc
conda activate mystery

bash run.sh
