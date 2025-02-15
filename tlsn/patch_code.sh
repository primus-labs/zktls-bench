#!/bin/bash
set -e

curdir=$(pwd)
if [ ! -d "../3rd/tlsn/crates/benches" ]; then
  echo "please run 'git submodule update --init' to fetch tlsn"
  exit 1
fi

cd ../3rd/tlsn
git apply --check ${curdir}/tlsn.patch
git apply ${curdir}/tlsn.patch
