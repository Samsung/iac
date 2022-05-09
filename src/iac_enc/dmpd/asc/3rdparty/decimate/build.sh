g++ flitflit.cpp -g -I/usr/include/eigen3 -std=c++11 -fPIC -shared -olibdecimate.so
gcc test.c -g -o test -L./ -ldecimate
cp libdecimate.so ../lib

