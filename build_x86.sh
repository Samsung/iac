#!/bin/bash

BUILD_LIBS=$PWD/build_libs
#1, build libiamf
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make
make install

#2, build test/tools/iamfpackager

cd test/tools/iamfpackager
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make 
cd -

#3, build test/tools/iamfplayer

cd test/tools/iamfplayer
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make 
cd -
