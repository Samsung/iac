#Codec source update script
#OPUS
BUILD_LIBS=$PWD
cd opus/opus-1.3.1/

make clean
./configure --prefix=${BUILD_LIBS} --with-pic --enable-float-approx --disable-shared
 
make
make install

#FDK-AAC
cd ../../aac/fdk-aac-2.0.2
make clean
./configure --prefix=${BUILD_LIBS} --with-pic  --disable-shared
make
make install

#FLAC
cd ../../flac/flac-1.4.2/
make clean
cmake . -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS} -DWITH_OGG=OFF
make
sudo make install

