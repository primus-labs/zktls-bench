#!/bin/bash
target=${1:-"cpp"} # cpp, wasm

# The current directory is this repo's ROOT directory
curdir=$(pwd)
uname_s=$(uname -s)
arch_n=$(arch)
nproc=8
if [[ "$OSTYPE" =~ ^linux ]]; then
  nproc=$(nproc)
fi
echo "===== compile target: ${target}, on ${uname_s} ${arch_n} ====="
#######################################################################
#######################################################################
build_type=Debug
build_type=Release

make_verbose="VERBOSE=1"
make_verbose=

# switch ON/OFF to enable/disable
enable_test=ON
enable_test=OFF
enable_threading=OFF
enable_threading=ON

# `ON`: do not print any log
cpp_no_print=OFF
wasm_no_print=OFF

# ON: use websocket io
# OFF: use net io
use_websocket_io=ON

load_certs_from_memory=ON
load_circuits_from_memory=ON
#######################################################################
#######################################################################

if [ "${target}" = "" ]; then
  echo "target is empty"
  exit 0
fi
builddir=${curdir}/build/${target}
installdir=${curdir}/install/${target}
scripts_dir=${curdir}/scripts
logs_dir=${builddir}/logs
mkdir -p ${builddir} ${installdir} ${logs_dir}
#######################################################################
#######################################################################
thirdparty_dir=${curdir}/3rd/
#######################################################################
#######################################################################
