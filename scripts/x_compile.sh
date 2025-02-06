#!/bin/bash
curdir=$(pwd)
target=${1:-"all"} # cpp, wasm, all(cpp+wasm)

cd ${curdir}

# checks
./scripts/check.sh
if [ $? -ne 0 ]; then
  exit 1
fi

if [ "${target}" = "cpp" ] || [ "${target}" = "all" ]; then
  bash ./scripts/internal/compile_openssl.sh
  bash ./scripts/internal/compile_emp.sh
  bash ./scripts/internal/compile_otls.sh
  bash ./scripts/internal/compile_bench.sh
fi

cd ${curdir}

echo "compile done"
