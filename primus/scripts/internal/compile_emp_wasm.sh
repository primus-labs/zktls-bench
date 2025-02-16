#!/bin/bash
. ./scripts/_config.sh wasm
#######################################################################
#######################################################################

# ######################
emp_repos=(
  emp-tool
  emp-ot
  emp-zk
)
ossl_root=${installdir}
# ######################

#
#
# ######################
for repo in ${emp_repos[@]}; do
  echo "compile ${repo}"
  repo_dir=${thirdparty_dir}/primus-emp/${repo}
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

    emcmake cmake ${repo_dir} \
      -DUSE_WASM_EXCEPTION=ON \
      -DTHREADING=${enable_threading} \
      -DENABLE_WASM=ON \
      -DENABLE_EMP_TOOL_TEST=${enable_test} \
      -DENABLE_EMP_OT_TEST=${enable_test} \
      -DENABLE_EMP_ZK_TEST=${enable_test} \
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
done

cd ${curdir}
exit 0
