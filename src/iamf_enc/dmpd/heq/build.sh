if [ -f "HeightMixingParameter" ] ;then
    rm HeightMixingParameter
fi

if [ -f "c_dspInBuf.txt" ] ; then
    rm c_dspInBuf.txt
fi 
gcc wave.c utils.c DHE.c -g -Wall -lm -o HeightMixingParameter
