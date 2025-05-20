#! /bin/bash

file_dir=`cd $(dirname $0) && pwd`
openssl_dir=${file_dir}/../../3rd/openssl-1.1.1

cd ${openssl_dir}

git checkout -- .
