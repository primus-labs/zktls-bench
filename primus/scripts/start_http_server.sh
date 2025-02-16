#!/bin/bash

if [ ! -f "./build/wasm/bench/bin/zktls_bench.wasm" ]; then
  echo "Please compile wasm first"
  exit 1
fi
cp -f ./build/wasm/bench/bin/zktls_bench.* ./browser/html/

cd ./browser/html
python -m http.server
