#!/bin/bash

BUILD_LIBS=$PWD/build_libs

#1, build libiamf
cmake -B build -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
cmake --build build --clean-first
cmake --install build

#2, build test/tools/iamfplayer

cd test/tools/iamfplayer
cmake -B build -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
cmake --build build --clean-first
cd -
