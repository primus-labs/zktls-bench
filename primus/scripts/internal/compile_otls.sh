#!/bin/bash
. ./scripts/_config.sh cpp
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

  cmake ${repo_dir} \
    -DLOAD_CIRCUIT_FROM_MEM=${load_circuits_from_memory} \
    -DTHREADING=${enable_threading} \
    -DUSE_PRIMUS_EMP=ON \
    -DENABLE_OTLS_TEST=${enable_test} \
    -DCMAKE_INSTALL_PREFIX=${installdir} \
    -DCMAKE_BUILD_TYPE=${build_type} \
    -DOPENSSL_INCLUDE_DIR=${ossl_root}/include \
    -DOPENSSL_SSL_LIBRARY=${ossl_root}/lib/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=${ossl_root}/lib/libcrypto.a
  make -j${nproc} ${make_verbose}
  make install
else
  echo "${repo} not exist!"
fi

cd ${curdir}
exit 0
