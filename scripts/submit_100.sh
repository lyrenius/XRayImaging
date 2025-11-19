#!/bin/bash

for i in {1..100}; do
  echo "Submitting job $i"
  sbatch run.sh
done