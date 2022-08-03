/**
 * @file immersive_audio_decoder.h
 * @brief Immersive audio decoder reference API
 */

#ifndef _IMMERSIVE_AUDIO_DECODER_H_
#define _IMMERSIVE_AUDIO_DECODER_H_

#include <stdint.h>
#include "immersive_audio_defines.h"


#ifdef __cplusplus
extern "C" {
#endif

/**\name Immersive audio parameter */
/**@{*/

typedef enum {
    IA_PARAM_DEMIXING_INFO,
} IAParamID;

typedef struct IAParam IAParam;

/**
 * @brief   Create an immersive audio parameter handler with raw data.
 * @param   [in] id : see @ref IAParamID.
 * @param   [in] data : raw data.
 * @param   [in] size : the length of raw data.
 * @return  an immersive audio parameter handler.
 * */
IAParam*    immersive_audio_param_raw_data_new (IAParamID id, uint8_t *data, uint32_t size);

/**
 * @brief   Free an immersive audio parameter handler.
 * @param   [in] param : an immersive audio parameter handler.
 * @return  void
 * */
void    immersive_audio_param_free(IAParam *param);

/**@}*/
/**\name Immersive audio decoder functions */
/**@{*/

typedef struct IADecoder IADecoder;

/**
 *  For AV application, it only applies Limiter at -1dBTP.
 *
 *  For TV application, it only applies Loudness normalization at -24LKFS and
 *  Limiter at -1dBTP.
 *
 *  For Mobile application, it applies Loudness normalization at -24LKFS,
 *  the pre-defined DRC control and adjusting of target loudness at -16 LKFS,
 *  and Limiter at -1dBTP.
 * */
enum {
    IA_DRC_AV_MODE,
    IA_DRC_TV_MODE,
    IA_DRC_MODILE_MODE
};

/**
 * @brief   Get the number of channels of the channel layout.
 * @param   [in] type : see @ref IAChannelLayoutType.
 * @return  the number of channels.
 * */
int     immersive_audio_channel_layout_get_channels_count (IAChannelLayoutType type);

/**
 * @brief   Create an immersive audio decoder handler.
 * @param   [in] codec : see @ref IACodecID, desired decoder type.
 * @param   [in] codec_spec : contains decoder configuration information.
 *                            codec specific info obu as default format.
 *                            the flags indicates other formats of the codec
 *                            specific information (@ref IA_FLAG_CODEC_CONFIG_RAW,
 *                            @ref IA_FLAG_CODEC_CONFIG_ISOBMFF).
 * @param   [in] clen : the length of codec specific information.
 * @param   [in] meta : the immersive audio static meta obu data.
 * @param   [in] mlen : the length of static meta data.
 * @param   [in] flags : bit field with flags for the decoder.
 * @return  an immersive audio decoder handler.
 * */
IADecoder*  immersive_audio_decoder_create (IACodecID codec,
                                          uint8_t* codec_spec, uint32_t clen,
                                          uint8_t *meta, uint32_t mlen,
                                          uint32_t flags);

/**
 * @brief   De-allocate all resources of an immersive audio decoder handler.
 * @param   [in] ths : an immersive audio decoder handler.
 * @return  void.
 * */
void    immersive_audio_decoder_destory (IADecoder* ths);

/**
 * @brief   Decode an immersive audio packet.
 * @param   [in] ths : an immersive audio decoder handler.
 * @param   [in] data : input payload.
 * @param   [in] len : the lengh of payload.
 * @param   [out] pcm : output signal, with interleaved samples.
 * @param   [in] size : The number of samples per channel of available space in \a pcm.
 * @param   [in] param : immersive audio parameters. see @ref IAParamID.
 * @param   [in] count : the number of immersive audio parameters.
 * */
int     immersive_audio_decoder_decode (IADecoder* ths,
                                        uint8_t* data, uint32_t len,
                                        uint16_t* pcm, uint32_t size,
                                        IAParam* param[], uint32_t count);

/**
 * @brief   Set channel layout to immersive audio handler.
 * @param   [in] ths : an immersive audio decoder handler.
 * @param   [in] type : see @ref IAChannelLayoutType.
 * */
IAErrCode   immersive_audio_decoder_set_channel_layout (IADecoder* ths,
                                                        IAChannelLayoutType type);

/**
 * @brief   Set drc mode to immersive audio handler.
 * @param   [in] ths : an immersive audio decoder handler.
 * @param   [in] mode : see @ref IA_DRC_AV_MODE, @ref IA_DRC_TV_MODE, @ref IA_DRC_MODILE_MODE.
 * */
IAErrCode   immersive_audio_decoder_set_drc_mode (IADecoder* ths, int mode);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* _IMMERSIVE_AUDIO_DECODER_H_ */
