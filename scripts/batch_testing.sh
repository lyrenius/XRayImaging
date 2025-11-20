#!/bin/bash

# Sweep NUM_THREADS = 1..28, each 10 times
for nt in $(seq 1 28); do
  for rep in $(seq 1 10); do
    echo "Submitting run: NUM_THREADS=${nt}, rep=${rep}"
    sbatch --export=ALL,NUM_THREADS=${nt} scripts/run_testing.sh
    sleep 1
  done
done
