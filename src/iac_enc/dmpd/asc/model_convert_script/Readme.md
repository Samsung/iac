## convert ckpt to tflite model guide

### add two python files under the same directory as asc_test.py
+ `convert_pb.py`  
+ `convert_tflite.py`

```shell
.
├── asc_common_bs.py
├── asc_dn_bs.py
├── asc_test.py
├── asc_util_bs.py
├── convert_pb.py
├── convert_tflite.py
├── cSrc
├── estimator
├── feature
├── input
├── model
├── output
├── pb_model
├── Readme.md
├── Readme.txt
└── tflite_model
```

### make two folders under the same directory as asc_test.py
+ `mkdir pb_model`
+ `mkdir tflite_model`


### modify `convert_pb.py` with the real ckpt model name

```python
ckpt_path = "./model/asc-2070" # change to real ckpt model name
```

### run `convert_pb.py` to convert `ckpt model` to `pb model`

```shell
pb_model/
├── estimator_frozen_model.pb
├── estimator_model.pb
├── feature_frozen_model.pb
└── feature_model.pb
```

### run `convert_tflite.py` to convert `pb model` to `tflite model`
```shell
tflite_model/
├── estimator_model.tflite
└── feature_model.tflite
```