#!/bin/bash

if [ ! -d build/ ]; then
  mkdir build
fi

cd build
if [ ! -f toolchain.cmake ]; then
  cp ../contrib/toolchains/toolchain-linux-default-release.cmake toolchain.cmake
fi

cmake -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -G Ninja .. || exit 4
cmake --build .
