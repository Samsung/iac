#!/bin/bash

BUILD_LIBS=$PWD/build_libs
#1, build libiac
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make
make install

#2, build test/tools/encode2mp4

cd test/tools/encode2mp4
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make 

#3, build test/tools/encode2mp4

cd ../mp4opusplay
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  .
make 