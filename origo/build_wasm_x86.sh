#!/bin/bash
set -e
curdir=$(pwd)

cd ./3rd/web-prover
make wasm

cd $curdir
cd client_wasm
npm install
