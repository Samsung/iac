build steps(linux):

1,cd build
2,cmake ../
3,make
4,./AudioSceneClassificaiton replace_audio.wav




build steps(windows):

1, make visual studio project by CMake app(windows)
   I selected Visual Studio 14 2015 generator. 
   CMake setting:
   Where is the source code: /AI-3D-Audio/refer-code/code/audio_scene_classificaiton/
   Where to build the binaries: /AI-3D-Audio/refer-code/code/audio_scene_classificaiton/build_win32   (this diretory is new created)
2, open AudioSceneClassificaiton.sln in /build_win32, build AudioSceneClassificaiton
   copy model files(build/estimator_model.tflite  and  build/feature_model.tflite) to exacuting diretory