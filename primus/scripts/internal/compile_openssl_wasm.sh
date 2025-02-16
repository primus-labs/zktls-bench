#!/bin/bash
. ./scripts/_config.sh wasm
#######################################################################
#######################################################################

# openssl version
ov=3.0.8  # use todo(https://www.openssl.org/source/openssl-3.0.8.tar.gz)
ov=1.1.1t # use ./openssl-1.1.1t.tar.gz(https://www.openssl.org/source/openssl-1.1.1t.tar.gz)
ov=1.1.1  # use git@github.com:pado-labs/openssl.git -b ossl
echo "use openssl: ${ov}"

# uncompress opnessl
ossldir=${thirdparty_dir}/openssl-${ov}

# compile openssl
if [ ! -f "${installdir}/lib/libssl.a" ]; then
  cd ${ossldir}
  make clean
  if [ "${build_type}" = "Debug" ]; then
    CFLAGS=" -O0 --debug -ggdb3"
  else
    CFLAGS=" -O1"
  fi
  export CFLAGS=${CFLAGS}
  emconfigure ./config -no-asm -no-shared -no-async -no-dso --prefix=${installdir}
  if [ "${uname_s}" = "Darwin" ]; then
    # set CNF_CFLAGS=-pthread
    sed -i "" 's/CNF_CFLAGS=.*/CNF_CFLAGS=-pthread/' Makefile
    # set CROSS_COMPILE=
    sed -i "" 's/CROSS_COMPILE=.*/CROSS_COMPILE=/' Makefile
  else
    # set CROSS_COMPILE=
    sed -i 's/CROSS_COMPILE=.*/CROSS_COMPILE=/' Makefile
  fi
  emmake make -j${nproc} build_libs ${make_verbose}
  make install_dev
  make clean
  cd ${curdir}
fi
