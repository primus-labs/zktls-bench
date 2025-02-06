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
repo=pado
repo_dir=${curdir}/${repo}
cd ${curdir}
if [ -d ${repo_dir} ]; then
  mkdir -p ${builddir}/${repo}
  cd ${builddir}/${repo}

  cmake ${repo_dir} \
    -DLOAD_CIRCUIT_FROM_MEM=${load_circuits_from_memory} \
    -DLOAD_CERT_FROM_MEM=${load_certs_from_memory} \
    -DOPT_NO_PRINT=${cpp_no_print} \
    -DTHREADING=${enable_threading} \
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
