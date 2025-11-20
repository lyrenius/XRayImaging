#!/bin/bash

#SBATCH -J mystery
#SBATCH -p cnall
#SBATCH -N 1
#SBATCH -o logs/stdout.%j
#SBATCH -e logs/stderr.%j
#SBATCH --ntasks-per-node=56

source ~/.bashrc
module load compilers/gcc/v12.2.0
conda activate mystery

if [[ -z "$SLURM_SUBMIT_DIR" ]]; then
  SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
  SLURM_SUBMIT_DIR="$(dirname "$SCRIPT_DIR")"
fi

CFITSIO_PATH=$SLURM_SUBMIT_DIR/cfitsio

g++ -std=c++20 -O3 -march=native $SLURM_SUBMIT_DIR/src/trans.cpp \
  -I $CFITSIO_PATH/include \
  -L $CFITSIO_PATH/lib \
  -Wl,-rpath,"$CFITSIO_PATH/lib" \
  -lcfitsio -lm \
  -lpthread \
  -o $SLURM_SUBMIT_DIR/bin/trans

g++ -std=c++20 -O3 -march=native -o $SLURM_SUBMIT_DIR/bin/ratio_calculate $SLURM_SUBMIT_DIR/src/ratio_calculate.cpp

echo "X Ray Image task:"

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Running..."
echo "================================================================"

$SLURM_SUBMIT_DIR/bin/trans read
$SLURM_SUBMIT_DIR/bin/ratio_calculate
$SLURM_SUBMIT_DIR/bin/trans write

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Checking..."
echo "================================================================"

python $SLURM_SUBMIT_DIR/src/score.py

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Done."
echo "================================================================"
