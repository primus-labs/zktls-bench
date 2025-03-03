#!/bin/bash
set -e
if [ $# -ne 1 ]; then
	echo "Usage: $0 \${patch_file}"
	exit 1
fi
patchFile=$1

curdir=$(pwd)
if [ ! -d "./3rd/web-prover" ]; then
  echo "please run 'git submodule update --init' to fetch web-prover"
  exit 1
fi

cd ./3rd/web-prover
git checkout -- .
git apply --check ${curdir}/$patchFile
git apply ${curdir}/$patchFile

cp ${curdir}/fixture/* fixture/
