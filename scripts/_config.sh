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
#build_type=Release

make_verbose="VERBOSE=1"
#make_verbose=

# switch ON/OFF to enable/disable
enable_test=ON
enable_test=OFF
enable_threading=OFF
enable_threading=ON

# `ON`: do not print any log
cpp_no_print=OFF
wasm_no_print=OFF

load_certs_from_memory=ON
load_circuits_from_memory=ON
#######################################################################
# android
android_ndk_21=
android_ndk_26=
android_ndk_21=/root/x/android/sdk/ndk/21.4.7075529
android_ndk_26=/root/x/android/sdk/ndk/26.1.10909125
android_cmake=cmake
if [ "${target}" = "android" ]; then
  if [ ! -d $android_ndk_21 ]; then
    echo "please install/set andorid ndk 21.4.7075529. (only for openssl)"
    exit 1
  fi
  if [ ! -d $android_ndk_26 ]; then
    echo "please install/set andorid ndk 26.1.10909125"
    exit 1
  fi
fi

# ios
if [ "${target}" = "ios" ]; then
  if [ "${uname_s}" != "Darwin" ]; then
    echo "unsupported platform[${uname_s}] for ios"
    exit 1
  fi
fi
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
