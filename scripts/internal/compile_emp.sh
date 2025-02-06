#!/bin/bash
. ./scripts/_config.sh cpp
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
  repo_dir=${thirdparty_dir}/pado-emp/${repo}
  cd ${curdir}
  if [ -d ${repo_dir} ]; then
    mkdir -p ${builddir}/${repo}
    cd ${builddir}/${repo}

    cmake ${repo_dir} \
      -DTHREADING=${enable_threading} \
      -DENABLE_EMP_TOOL_TEST=${enable_test} \
      -DENABLE_EMP_OT_TEST=${enable_test} \
      -DENABLE_EMP_ZK_TEST=${enable_test} \
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
done

cd ${curdir}
exit 0
