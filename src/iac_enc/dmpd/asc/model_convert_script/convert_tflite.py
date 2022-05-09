import tensorflow as tf

feature_in_path = './pb_model/feature_frozen_model.pb'
estimator_in_path= './pb_model/estimator_frozen_model.pb'

# !!!!!!
# change the output name
feature_out_path    = './tflite_model/feature_model.tflite'
estimator_out_path  = './tflite_model/estimator_model.tflite'

feature_input_arrays = ["input"]
estimator_input_arrays = ["inp_features"]
# !!!!!!
# Change the channel shape
#input_shapes = {"input": [1, 68,68, 1]}
feature_input_shapes = {"input": [1, 68,68, 5]}
estimator_input_shapes = {"inp_features": [1, 3072]}
# input_shapes = {"input": [1, 68,68, 4]}

feature_output_arrays = ["model_asc_direct/add_3"]
estimator_output_arrays = ["Softmax","ArgMax"]

estimator_converter = tf.lite.TFLiteConverter.from_frozen_graph(estimator_in_path, estimator_input_arrays, estimator_output_arrays, estimator_input_shapes)
estimator_tflite_model = estimator_converter.convert()
open(estimator_out_path, "wb").write(estimator_tflite_model)


feature_converter = tf.lite.TFLiteConverter.from_frozen_graph(feature_in_path, feature_input_arrays, feature_output_arrays, feature_input_shapes)
feature_tflite_model = feature_converter.convert()
open(feature_out_path, "wb").write(feature_tflite_model)

