#!/bin/bash
set -e

cd ./3rd/web-prover

make wasm
make wasm-demo
