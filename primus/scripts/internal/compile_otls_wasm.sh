#!/bin/bash
. ./scripts/_config.sh wasm
#######################################################################
#######################################################################

# ######################
ossl_root=${installdir}
# ######################

#
#
# ######################
repo=otls
repo_dir=${thirdparty_dir}/${repo}
cd ${curdir}
if [ -d ${repo_dir} ]; then
  mkdir -p ${builddir}/${repo}
  cd ${builddir}/${repo}

  # multi-thread (do not use `ALLOW_MEMORY_GROWTH` with pthread. slow)
  PARALLEL_CFLAGS=" -pthread -sPTHREAD_POOL_SIZE=4 -sPROXY_TO_PTHREAD"
  EMCC_CFLAGS+=${PARALLEL_CFLAGS}

  if [ "${build_type}" = "Debug" ]; then
    EMCC_CFLAGS+=" -O0 --debug -ggdb3"
  else
    EMCC_CFLAGS+=" -O3"
  fi
  EMCC_CFLAGS+=" -fwasm-exceptions"

  export EMCC_CFLAGS=${EMCC_CFLAGS}
  echo "EMCC_CFLAGS: ${EMCC_CFLAGS}"

  # -DCMAKE_CROSSCOMPILING_EMULATOR=$NODE_HOME/bin/node
  emcmake cmake ${repo_dir} \
    -DLOAD_CIRCUIT_FROM_MEM=${load_circuits_from_memory} \
    -DTHREADING=${enable_threading} \
    -DUSE_PRIMUS_EMP=ON \
    -DENABLE_WASM=ON \
    -DENABLE_OTLS_TEST=${enable_test} \
    -DCMAKE_INSTALL_PREFIX=${installdir} \
    -DCMAKE_BUILD_TYPE=${build_type} \
    -DOPENSSL_INCLUDE_DIR=${ossl_root}/include \
    -DOPENSSL_SSL_LIBRARY=${ossl_root}/lib/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=${ossl_root}/lib/libcrypto.a
  emmake make -j${nproc} ${make_verbose}
  make install
else
  echo "${repo} not exist!"
fi

cd ${curdir}
exit 0
