#!/bin/bash
set -e

sudo apt-get install libssl-dev
sudo apt-get install pkg-config
sudo apt-get install clang
sudo apt-get install protobuf-compiler

curdir=$(pwd)
if [ ! -d "./3rd/web-prover/proofs" ]; then
  echo "please run 'git submodule update --init' to fetch web-prover"
  exit 1
fi

cd 3rd/web-prover
make artifacts 
