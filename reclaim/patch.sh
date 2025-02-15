#!/bin/bash
# format:
#  head    1 line
#  native 72 lines
#  wasm   72 lines
# Usage:
#   bash patch.sh ../bench_data/reclaim-x86-3_1Ghz-tls12.csv native-tls1.2
if [ $# -lt 1 ]; then
  echo "Usage: $0 \${origfile} \${[kind]}"
  exit 1
fi
origfile=$1
kind=native
if [ $# -ge 2 ]; then
  kind=$2
fi

if [ ! -f "$origfile" ]; then
  echo "file $origfile not exist!"
  exit 1
fi
if [ ! -f "patched/$kind.csv" ]; then
  echo "file patched/$kind.csv not exist!"
  exit 1
fi

mkdir -p logs
cut -d, -f1-6 $origfile >logs/r1.csv
cut -d, -f9-10 $origfile >logs/r3.csv
paste -d, logs/r1.csv patched/$kind.csv logs/r3.csv >patched-$kind.csv
echo "Output ./patched-$kind.csv"
