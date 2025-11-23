#!/bin/bash

for rep in $(seq 1 20); do
  echo "Submitting run: rep=${rep}"
  sbatch --export=ALL,NUM_THREADS=16 scripts/run_testing.sh
  sleep 2
done
