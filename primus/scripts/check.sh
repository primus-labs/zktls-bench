#!/bin/bash
curdir=$(pwd)

if [ ! -f "${curdir}/3rd/openssl-1.1.1/config" ]; then
  echo "please init/update openssl (submodule)"
  exit 1
fi
if [ ! -f "${curdir}/3rd/primus-emp/emp-tool/CMakeLists.txt" ]; then
  echo "please init/update primus-emp (submodule)"
  exit 1
fi
if [ ! -f "${curdir}/3rd/otls/CMakeLists.txt" ]; then
  echo "please init/update otls (submodule)"
  exit 1
fi

exit 0
