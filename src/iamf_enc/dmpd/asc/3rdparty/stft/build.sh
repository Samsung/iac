gcc stft.c hann_matrix.c window.c -g -I./ -L./ -lfftw3f -fPIC -shared -olibstft.so
gcc stft_test.c transpose.c -g -o test -L./ -lstft

