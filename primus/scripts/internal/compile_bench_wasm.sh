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
repo=bench
repo_dir=${curdir}/${repo}
cd ${curdir}
if [ -d ${repo_dir} ]; then
  mkdir -p ${builddir}/${repo}
  cd ${builddir}/${repo}

  EMCC_CFLAGS="-sWASM=1"

  # sse* simd
  SIMD_CFLAGS=" -msimd128 -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx"
  EMCC_CFLAGS+=${SIMD_CFLAGS}

  # WebSocket/POSIX Socket
  SOCKET_FLAGS=" -lwebsocket.js -sPROXY_POSIX_SOCKETS -sUSE_PTHREADS -sPROXY_TO_PTHREAD"
  EMCC_CFLAGS+=${SOCKET_FLAGS}

  # multi-thread (do not use `ALLOW_MEMORY_GROWTH` with pthread. slow)
  PARALLEL_CFLAGS=" -pthread -sPTHREAD_POOL_SIZE=4 -sPROXY_TO_PTHREAD"
  EMCC_CFLAGS+=${PARALLEL_CFLAGS}

  # memory
  MEM_CFLAGS=" -sSTACK_SIZE=1MB -sINITIAL_MEMORY=512MB -sMAXIMUM_MEMORY=3GB"
  EMCC_CFLAGS+=${MEM_CFLAGS}

  # runtime exports
  EMCC_CFLAGS+=" -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,UTF8ArrayToString,stringToNewUTF8"

  # others
  EMCC_CFLAGS+=" -sMAIN_MODULE=2 -sERROR_ON_UNDEFINED_SYMBOLS=0"
  EMCC_CFLAGS+=" -sALLOW_BLOCKING_ON_MAIN_THREAD -sUSE_SDL=0"
  if [ "${build_type}" = "Debug" ]; then
    EMCC_CFLAGS+=" -O0 --debug -ggdb3"
  else
    EMCC_CFLAGS+=" -O3"
  fi
  EMCC_CFLAGS+=" -fwasm-exceptions"
  EMCC_CFLAGS+=" -sENVIRONMENT=web,worker,node" # 'web,webview,worker,node'

  export EMCC_CFLAGS=${EMCC_CFLAGS}
  echo "EMCC_CFLAGS: ${EMCC_CFLAGS}"

  # -DCMAKE_CROSSCOMPILING_EMULATOR=$NODE_HOME/bin/node
  emcmake cmake ${repo_dir} \
    -DLOAD_CIRCUIT_FROM_MEM=${load_circuits_from_memory} \
    -DLOAD_CERT_FROM_MEM=${load_certs_from_memory} \
    -DOPT_NO_PRINT=${wasm_no_print} \
    -DTHREADING=${enable_threading} \
    -DENABLE_WASM=ON \
    -DCMAKE_INSTALL_PREFIX=${installdir} \
    -DCMAKE_BUILD_TYPE=${build_type} \
    -DOPENSSL_INCLUDE_DIR=${ossl_root}/include \
    -DOPENSSL_SSL_LIBRARY=${ossl_root}/lib/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=${ossl_root}/lib/libcrypto.a
  emmake make -j${nproc} ${make_verbose}
else
  echo "${repo} not exist!"
fi

cd ${curdir}
exit 0
