This library aims to be a friendly, portable C implementation of the immersive audio (IAC) 
storage format, as described here:

<https://aomediacodec.github.io/iac/>



## Usage

Please see the examples in the "test/tools" directory. If you're already building this project.

### Compiling
There are 2 parts to build: iac(iac_dec&iac_enc&dmpd) tools(encode2mp4&mp4iacplay).

"build_x86.sh" is an example to build, you can run it directly at your side.

1. build iac in "src" directory.
```sh
% BUILD_LIBS=$PWD/build_libs
% cmake ./ -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}
% make 
% make install
```

2. build tools in "test/tools/encode2mp4" and "test/tools/mp4iacplay" directory separately
```sh
% cmake ./-DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}
% make 
```

Remark: please ensure that they have same CMAKE_INSTALL_PREFIX.


### Tools(encode2mp4)
This tool aims to encode PCM data to IA bitstream and encapsulate to Mp4/Fmp4

1. encode scalable channel layout input format.
```sh
-codec   : <opus/aac>
-mode    : <input channel layout/channel layout combinations>
<input wav file>
<output mp4 file>

Example:  ./encode2mp4 -codec opus -mode 7.1.4/2.0.0+3.1.2+5.1.2 input.wav outout.mp4
```
Remark: "estimator_model.tflite" and "feature_model.tflite" are required in exacuting directory.

2. encode non-scalable channel layout input format.
```sh
Example:  ./encode2mp4 -codec opus -mode 7.1.4/0.0.0 input.wav outout.mp4
```

### Tools(mp4iacplay)
This tool aims to parse Mp4/Fmp4, decode IA bitstream and dump to wav file.
```sh
./mp4iacplayer <options> <input/output file>
options:
-o1          : -o1(mp4 dump output)
-o2          : -o2(decode CMF4 opus bitstream, audio processing and output wave file).
-d[0-2]      : DRC mode (0: av mode, 1:tv mode, 2:mobile mode).
-l[1-8]      : layout(1:2.0, 2:5.1, 3:5.1.2, 4:5.1.4, 5:7.1, 6:7.1.2, 7:7.1.4, 8:3.1.2.

Example:  ./mp4iacplayer -o2 -l1 input.mp4
          ./mp4iacplayer -d2 -o2 -l1 input.mp4
```


## Build Notes

1) Building this project requires [CMake](https://cmake.org/).

2) Building this project requires opus or aac library, please ensure that there are library in "dep_codecs/lib",
and there are headers in "dep_codecs/include" already. If not, please build(patch_script.sh) and install in advance.

3) "src/dmpd" part building relys on 3rd part libs("dep_external/lib": libfftw3f,libflatccrt)
Thay have been provided already in "dep_external/lib", if meet target link issue, please download the opensource code,
and build at your side. After building, please replace them.
[fftw](http://www.fftw.org/).
[flatcc](https://github.com/dvidelabs/flatcc).

Remark: please add compile options:-fPIC  



## License

Released under the BSD License.

```markdown

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
