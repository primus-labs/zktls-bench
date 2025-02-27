#!/bin/bash
set -e

curdir=$(pwd)
if [ ! -d "./3rd/web-prover" ]; then
  echo "please run 'git submodule update --init' to fetch web-prover"
  exit 1
fi

cd ./3rd/web-prover
git apply --check ${curdir}/origo.patch
git apply ${curdir}/origo.patch

cp ${curdir}/fixture/* fixture/
