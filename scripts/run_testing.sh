#!/bin/bash

#SBATCH -J mystery
#SBATCH -p cnall
#SBATCH -N 1
#SBATCH -o logs/batch_testing/stdout.%j
#SBATCH -e logs/batch_testing/stderr.%j
#SBATCH --ntasks-per-node=56

source ~/.bashrc
module load compilers/gcc/v12.2.0
conda activate mystery

if [[ -z "$SLURM_SUBMIT_DIR" ]]; then
  SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
  export SLURM_SUBMIT_DIR="$(dirname "$SCRIPT_DIR")"
fi

CFITSIO_PATH=$SLURM_SUBMIT_DIR/cfitsio

g++ -std=c++20 -O3 -march=native $SLURM_SUBMIT_DIR/src/ratio_calculate.cpp \
  -I $CFITSIO_PATH/include \
  -L $CFITSIO_PATH/lib \
  -Wl,-rpath,"$CFITSIO_PATH/lib" \
  -lcfitsio \
  -lpthread \
  -ffast-math -funroll-loops \
  -DNUM_THREADS=${NUM_THREADS:-1} \
  -o $SLURM_SUBMIT_DIR/bin/ratio_calculate

# ---- timing helper (prints only to stderr) ----
measure_step() {
  local label="$1"; shift
  local start_ns end_ns dur_ms

  start_ns=$(date +%s%N)
  "$@"
  end_ns=$(date +%s%N)

  dur_ms=$(( (end_ns - start_ns) / 1000000 ))
  echo "[TIME] ${label}: ${dur_ms} ms" >&2
}
# -----------------------------------------------

echo "X Ray Image task:"

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Running..."
echo "================================================================"

$SLURM_SUBMIT_DIR/bin/ratio_calculate

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Checking..."
echo "================================================================"

python $SLURM_SUBMIT_DIR/src/score.py

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Done."
echo "================================================================"
