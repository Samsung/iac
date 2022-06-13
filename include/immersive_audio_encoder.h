#ifndef IMMERSIVE_AUDIO_ENCODER_H
#define IMMERSIVE_AUDIO_ENCODER_H

#include "immersive_audio_encoder_defines.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct IAEncoder IAEncoder;

typedef enum {
  CHANNEL_LAYOUT_100, //1.0.0
  CHANNEL_LAYOUT_200, //2.0.0 
  CHANNEL_LAYOUT_510, //5.1.0
  CHANNEL_LAYOUT_512, //5.1.2
  CHANNEL_LAYOUT_514, //5.1.4
  CHANNEL_LAYOUT_710, //7.1.0
  CHANNEL_LAYOUT_712, //7.1.2
  CHANNEL_LAYOUT_714, //7.1.4
  CHANNEL_LAYOUT_312, //3.1.2
  CHANNEL_LAYOUT_MAX
}CHANNEL_LAYOUT;

typedef enum {
  IA_DEP_CODEC_OPUS,
  IA_DEP_CODEC_AAC,
  IA_DEP_CODEC_MAX
} IA_DEP_CODEC_TYPE;

#ifndef DEMIXING_MATRIX_SIZE_MAX
#define DEMIXING_MATRIX_SIZE_MAX (18 * 18 * 2)
#endif
typedef struct {
  int output_channel_count;
  int substream_count;
  int coupled_substream_count;
  int channel_mapping[12]; // todo 12?
  int demixing_matrix[DEMIXING_MATRIX_SIZE_MAX]; // todo DEMIXING_MATRIX_SIZE_MAX?
}AMBISONICS_LAYER_CONFIG;

typedef struct {
  int loudspeaker_layout;
  int output_gain_is_present_flag;
  int recon_gain_is_present_flag;
  int substream_count;
  int coupled_substream_count;
  int loudness;
  int output_gain_flags;
  int output_gain;
}CHANNEL_AUDIO_LAYER_CONFIG;

typedef struct {
  int version;
  int ambisonics_mode;
  int channel_audio_layer;

  AMBISONICS_LAYER_CONFIG ambisonics_layer_config;
  CHANNEL_AUDIO_LAYER_CONFIG channel_audio_layer_config[CHANNEL_LAYOUT_MAX];
}IA_STATIC_METADATA;




/**
* @brief     Create immersive audio encoder handler
* @param     [in] Fs : sample rate.
* @param     [in]channel_layout_in : the input of channel layout, ex: 7.1.4
* @param     [in]channel_layout_cb : the scalable channel layout combinations, ex: 2ch / 3.1.2ch / 5.1.2ch
* @param     [in]codec_id : the codec id. 0:opus, 1:aac
* @param     [in]error : the return error when create ia handle
* @return    return immersive audio encoder handler
* @remark    Adjacent channel layouts of a scalable format (where CLn-1 is the precedent channel layout and CLn is the next one) are
*            only allowed as below, where CLn = S(n).W(n).H(n)
*            S(n-1) ? S(n) and W(n 1) ? W(n) and H(n 1) ? H(n) except "S(n 1) = S(n) and W(n 1) = W(n) and H(n 1) = H(n)"
*            NOTE: S(Surround Channel), W(Subwoofer Channel), H(Height Channel)
*            if [in]channel_layout_cb is set with CHANNEL_LAYOUT_MAX, then it is non-scalable encoding.
*/
IAEncoder *immersive_audio_encoder_create(int32_t Fs,
  int channel_layout_in,
  const unsigned char *channel_layout_cb,//
  int codec_id, //0:opus, 1:aac
  int *error);

/*
Following 3 apis are used to implement Down-Mix Parameter Determination.
*/
/**
* @brief     Prepare the starting of own-Mix Parameter Determination
* @param     [in] st : channel group encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_dmpd_start(IAEncoder *st);

/**
* @brief     Down-Mix Parameter Determination pre-process.
* @param     [in] st : channel group encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_dmpd_process(IAEncoder *st, const int16_t *pcm, int frame_size);

/**
* @brief     Stop the Down-Mix Parameter Determination pre-process.
* @param     [in] st : channel group encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_dmpd_stop(IAEncoder *st);

/*
Following 3 apis are used to calculate loundness and output_gain.
*/
/**
* @brief     Prepare the starting of loudness &gain process
* @param     [in] st : immersive audio encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_loudness_gain_start(IAEncoder *st);

/**
* @brief     loudness &gain process
* @param     [in] st : immersive audio encoder handle.
* @param     [in] pcm : input pcm sample data.
* @param     [in] frame_size : frame size of input pcm.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_loudness_gain(IAEncoder *st, const int16_t *pcm, int frame_size);

/**
* @brief     Stop the loudness &gain process
* @param     [in] st : immersive audio encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_loudness_gain_stop(IAEncoder *st);

/**
* @brief     Return IA static metadata which is used in mp4 muxer
* @param     [in] st : immersive audio encoder handle.
* @return    return the ia_static_metada sturcture
*/
IA_STATIC_METADATA get_immersive_audio_encoder_ia_static_metadata(IAEncoder *st);

/**
* @brief     Gain down to downmixed pcm data 
* @param     [in] st : immersive audio encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_gaindown(IAEncoder *st);

/**
* @brief     Calculate the scalable factor.
* @param     [in] st : immersive audio encoder handle.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_scalefactor(IAEncoder *st);

/**
* @brief     Encode a ia ia frame.
* @param     [in] st : immersive audio encoder handle.
* @param     [in] pcm : input pcm sample data.
* @param     [in] frame_size : Number of samples per channel in the input signal.
* @param     [in] data : Output payload.
* @param     [in] max_data_bytes : Size of the allocated memory for the output payload
* @return    @0: success,@others: fail
*/
int immersive_audio_encode(IAEncoder *st, const int16_t *pcm, int frame_size, unsigned char* data, int *demix_mode, int32_t max_data_bytes);

/**
* @brief     Perform a CTL function on a  immersive audio encoder.
* @param     [in] st : immersive audio encoder handle.
* @param     [in] request : control request.
* @return    @0: success,@others: fail
*/
int immersive_audio_encoder_ctl(IAEncoder *st, int request, ...);

/**
* @brief     Free the immersive audio encoder.
* @param     [in] st : immersive audio encoder handle.
*/
void immersive_audio_encoder_destroy(IAEncoder *st);

#ifdef __cplusplus
}
#endif

#endif /* IMMERSIVE_AUDIO_ENCODER_H */

