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

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PARENT_DIR="$(dirname "$SCRIPT_DIR")"
CFITSIO_PATH=$PARENT_DIR/cfitsio

 g++ -std=c++20 -O3 -march=native $PARENT_DIR/src/trans.cpp \
   -I $CFITSIO_PATH/include \
   -L $CFITSIO_PATH/lib \
   -Wl,-rpath,"$CFITSIO_PATH/lib" \
   -lcfitsio -lm \
   -o $PARENT_DIR/bin/trans

g++ -std=c++20 -O3 -march=native -o $PARENT_DIR/bin/ratio_calculate $PARENT_DIR/src/ratio_calculate.cpp

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

measure_step "trans read" $PARENT_DIR/bin/trans read
measure_step "ratio_calculate" $PARENT_DIR/bin/ratio_calculate
measure_step "trans write" $PARENT_DIR/bin/trans write

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Checking..."
echo "================================================================"

python $PARENT_DIR/src/score.py

echo "================================================================"
echo "[$(date +"%Y-%m-%d %H:%M:%S.%3N")] Done."
echo "================================================================"
