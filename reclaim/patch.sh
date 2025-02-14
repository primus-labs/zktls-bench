#!/bin/bash
# format:
#  head    1 line
#  native 72 lines
#  wasm   72 lines
if [ $# -lt 1 ]; then
  echo "Usage: $0 \${origfile}"
  exit 1
fi
origfile=$1

mkdir -p logs
cut -d, -f1-6 $origfile >logs/r1.csv
cut -d, -f9-10 $origfile >logs/r3.csv
paste -d, logs/r1.csv patch.csv logs/r3.csv >patched.csv
