#!/bin/bash
set -e

curdir=$(pwd)
if [ ! -d "./3rd/web-prover/proofs" ]; then
  echo "please run 'git submodule update --init' to fetch web-prover"
  exit 1
fi

cd 3rd/web-prover/proofs
make web-prover-circuits
