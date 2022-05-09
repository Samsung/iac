import argparse
import math
import os
import sys
import time

import numpy as np
import scipy.io.wavfile
import tensorflow as tf
from scipy.signal import hilbert
from tensorflow.python.tools import freeze_graph

import asc_dn_bs as asc_dn
from asc_common_bs import *
from asc_util_bs import *

def def_args():
    parser = argparse.ArgumentParser(description='Audio_Scene_Classification_Mch: Direct Test', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--cpu', action='store_true', help='Run on CPU')
    parser.add_argument('-g', '--gpu', type=int, help='Specify GPU number to use')
    parser.add_argument('-m', '--model', default='./model', type=str, help='Directory where models will be stored')
    #parser.add_argument('-i', '--input', default='./input',type=str, help='Input file or directory name')
    parser.add_argument('-i', '--input', default='./input',type=str, help='Input file or directory name')
    parser.add_argument('-o', '--output', default='./output', type=str, help='Output file or directory name')
    parser.add_argument('-e', '--ext', default='wav', type=str, help='File extension name of input')
    parser.add_argument('-v', '--verbose', default=1, choices=[0, 1, 2], type=int, help='Verbose')
    parser.add_argument('--srate', default=48000, type=int, help='sampling rate ')
    parser.add_argument('--ds_factor', default=4, type=int, help='down-sampling factor ')
    # settings for decoding mode
    parser.add_argument('--ori_fsize', default=960, type=int, help='original frame size')
    parser.add_argument('--fsize', default=8640, type=int, help='frame size for frame decoding')
    parser.add_argument('--dsize', default=8640, type=int, help='data unit size to be output per frame')
    # asc related parameters
    parser.add_argument('--nclass_asc', default=3, type=int, help='the number of audio scenes')
    parser.add_argument('--nlayer_asc', nargs=5, default=[1, 1, 1, 0, 0], type=int, help='the number of network layer for ASC to be applied: [for )]')
    parser.add_argument('--filter-para_asc', nargs=4, default=[3, 2, 32, 512], type=int, help='[kernel size, pool rate, # of out features, # of output ch for ASC')
    parser.add_argument('--feature_size', default=512, type=int, help='the size of the feature for frame')
    parser.add_argument('--keep_conv', default=1.0, type=float, help='the keep rate for drop out in conv layer for ASC')
    parser.add_argument('--keep_hidden', default=1.0, type=float, help='the keep rate for drop out in hidden layer for ASC')
    parser.add_argument('--fft-size_asc', default=256, type=int, help='fft size for stft transformation')
    parser.add_argument('--hop-size_asc', default=128, type=int, help='hop size for stft transformation')
    parser.add_argument('--mel-bins_asc', default=68, type=int, help='freq. bins for mel-scaling')
    parser.add_argument('--wlabel', default=0, choices=[0, 1], type=int, help='0: test wo lable, 1: with label')
    parser.add_argument('--nor_rms', default=0.05, type=float, help='normalized target rms value')
    parser.add_argument('--kernel_similarity', default=5, type=int, help='the number of previous frames for calculating simialrities with the target frame')
    parser.add_argument('--kernel_features', default=3, type=int, help='the number of previous frames for the estimator network')
    args = parser.parse_args()
    return args



# !!!!!!!!
# model checkpoint path
# change this when convert dialog_classification or effect_classification model

ckpt_path = "./model/asc-2070"
# ckpt_path = "./model2/asc-56745"

def main(): 
    args = def_args()
    with tf.device("/cpu:0"):
        tf.reset_default_graph() 

        with tf.Session(graph = tf.get_default_graph()) as sess:

            # Input tensor placeholder, in original graph, this input should be processed beforehand by asc_dn.asc_preprocess and asc_dn.asc_log_mstft_transform.
            # Look at original graph in line 209-225 in file asc_test.py for detailed preprocessing.
            # Model effect_classification require 4 channel input
            # !!!!!!
            #inp_asc = tf.placeholder(tf.float32, shape=(1, args.mel_bins_asc, args.mel_bins_asc, 1), name="input")
            
            
            # !!!!!!
            # Have to command out dropout layers in the model, for inference dropout is not used.
            # Specifically, for dialog_classification model, command out line 227,234,243,249,255, in function ASCblksimple_1ch in asc_dn_bs.py
            # For effect_classification model, command out line 434,441,450,456,462 in function ASCblksimple_Nch in asc_dn_bs.py
            # !!!!!!            
            feature_extractor_graph(args)

            global_step = tf.train.get_global_step()
            sess.run(tf.global_variables_initializer())

            saver = tf.train.Saver()
            tf.train.write_graph(sess.graph_def, './pb_model', 'feature_model.pb') 
            freeze_graph.freeze_graph(
                input_graph='./pb_model/feature_model.pb',
                input_saver='',
                input_binary=False, 
                input_checkpoint=ckpt_path,
                # !!!!!!
                # Also change here the node name
                #output_node_names='model_asc_direct/FC3/BiasAdd,Softmax,ArgMax',
                output_node_names='model_asc_direct/add_3',
                restore_op_name='save/restore_all',
                filename_tensor_name='save/Const:0',
                output_graph='./pb_model/feature_frozen_model.pb',
                clear_devices=False,
                initializer_nodes=''
                )
        print("done") 

        tf.reset_default_graph() 
        with tf.Session(graph = tf.get_default_graph()) as sess:

            # Input tensor placeholder, in original graph, this input should be processed beforehand by asc_dn.asc_preprocess and asc_dn.asc_log_mstft_transform.
            # Look at original graph in line 209-225 in file asc_test.py for detailed preprocessing.
            # Model effect_classification require 4 channel input
            # !!!!!!
            #inp_asc = tf.placeholder(tf.float32, shape=(1, args.mel_bins_asc, args.mel_bins_asc, 1), name="input")


            # !!!!!!
            # Have to command out dropout layers in the model, for inference dropout is not used.
            # Specifically, for dialog_classification model, command out line 227,234,243,249,255, in function ASCblksimple_1ch in asc_dn_bs.py
            # For effect_classification model, command out line 434,441,450,456,462 in function ASCblksimple_Nch in asc_dn_bs.py
            # !!!!!!            
            estimator_graph(args)

            global_step = tf.train.get_global_step()
            sess.run(tf.global_variables_initializer())

            saver = tf.train.Saver()
            tf.train.write_graph(sess.graph_def, './pb_model', 'estimator_model.pb') 
            freeze_graph.freeze_graph(
                input_graph='./pb_model/estimator_model.pb',
                input_saver='',
                input_binary=False, 
                input_checkpoint=ckpt_path,
                # !!!!!!
                # Also change here the node name
                #output_node_names='model_asc_direct/FC3/BiasAdd,Softmax,ArgMax',
                output_node_names='model_asc_direct/FC3/BiasAdd,Softmax,ArgMax',
                restore_op_name='save/restore_all',
                filename_tensor_name='save/Const:0',
                output_graph='./pb_model/estimator_frozen_model.pb',
                clear_devices=False,
                initializer_nodes=''
                )
        print("done") 


# def define_graph(args, inp_asc,inp_features):
#     filter_para_asc = args.filter_para_asc
#     nclass_asc = args.nclass_asc
#     f_size = args.feature_size
#     kernel_s = args.kernel_similarity
#     kernel_f = args.kernel_features
    
    
#     out_feature = asc_dn.ASC_feature_extractor(inp_asc, filter_para_asc, nclass_asc, p_keep=[1, 1], padding="VALID", bias="True", stride=1, name='model_asc_direct',reuse=False)
    
#     featuresWsiml = asc_dn.ASC_calc_similarities_and_concat_features(inp_features, f_size, kernel_s, kernel_f)
#     out_asc = asc_dn.ASC_estimator(featuresWsiml, filter_para_asc, nclass_asc, p_keep=[1, 1], padding="VALID", bias="True", stride=1, name='model_asc_direct', reuse=False)
#     out_asc_softmax = tf.nn.softmax(out_asc, axis=-1)
#     out_asc_result = asc_dn.get_test_result_asc(out_asc)
    
#     return out_asc,out_asc_softmax,out_asc_result

#     # !!!!!!
#     # Here we can change the graph for two models
#     # return dialog_classification(inp_asc, filter_para_asc, nclass_asc)
#     #return effect_classification(inp_asc, filter_para_asc, nclass_asc)
def feature_extractor_graph(args):
    filter_para_asc = args.filter_para_asc
    nclass_asc = args.nclass_asc

    inp_asc = tf.placeholder(tf.float32, shape=(1, args.mel_bins_asc, args.mel_bins_asc, 5), name="input")
    out_feature = asc_dn.ASC_feature_extractor(inp_asc, filter_para_asc, nclass_asc, p_keep=[1, 1], padding="VALID", bias="True", stride=1, name='model_asc_direct',reuse=False)
    
    return out_feature

def estimator_graph(args):
    f_size = args.feature_size
    kernel_s = args.kernel_similarity 
    filter_para_asc = args.filter_para_asc
    nclass_asc = args.nclass_asc
    kernel_f = args.kernel_features
    
    inp_features = tf.placeholder(tf.float32, shape=(1, f_size*(kernel_s+1)),name="inp_features")

    featuresWsiml = asc_dn.ASC_calc_similarities_and_concat_features(inp_features, f_size, kernel_s, kernel_f)
    out_asc = asc_dn.ASC_estimator(featuresWsiml, filter_para_asc, nclass_asc, p_keep=[1, 1], padding="VALID", bias="True", stride=1, name='model_asc_direct', reuse=False)
    out_asc_softmax = tf.nn.softmax(out_asc, axis=-1)
    out_asc_result = asc_dn.get_test_result_asc(out_asc)

    return out_asc,out_asc_softmax,out_asc_result



if __name__ == '__main__': 
    main()



