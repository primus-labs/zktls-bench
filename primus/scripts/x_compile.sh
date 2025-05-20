#!/bin/bash
curdir=$(pwd)
target=${1:-"all"} # cpp, wasm, all(cpp+wasm)

cd ${curdir}

# checks
./scripts/check.sh
if [ $? -ne 0 ]; then
  exit 1
fi

bash ./scripts/internal/apply_patch.sh

if [ "${target}" = "cpp" ] || [ "${target}" = "all" ]; then
  bash ./scripts/internal/compile_openssl.sh
  bash ./scripts/internal/compile_emp.sh
  bash ./scripts/internal/compile_otls.sh
  bash ./scripts/internal/compile_bench.sh
fi

if [ "${target}" = "wasm" ] || [ "${target}" = "all" ]; then
  bash ./scripts/internal/compile_openssl_wasm.sh
  bash ./scripts/internal/compile_emp_wasm.sh
  bash ./scripts/internal/compile_otls_wasm.sh
  bash ./scripts/internal/compile_bench_wasm.sh
fi

cd ${curdir}

bash ./scripts/internal/revert_patch.sh
echo "compile done"
