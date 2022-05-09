/**
 * @file immersive_audio_decoder.h
 * @brief Immersive audio decoder reference API
 */

#ifndef _IMMERSIVE_AUDIO_DECODER_H_
#define _IMMERSIVE_AUDIO_DECODER_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond IMMERSIVE_AUDIO_INTERNAL_DOC */

/** These are the actual decoder CTL ID numbers.
  * They should not be used directly by applications.c
  * In general, SETs should be even and GETs should be odd.
  * */
/**@{*/
/**@}*/

/** @endcond */

enum ChannelLayoutType {
    channel_layout_type_invalid = -1,
    channel_layout_type_1_0_0 = 0,
    channel_layout_type_2_0_0,
    channel_layout_type_5_1_0,
    channel_layout_type_5_1_2,
    channel_layout_type_5_1_4,
    channel_layout_type_7_1_0,
    channel_layout_type_7_1_2,
    channel_layout_type_7_1_4,
    channel_layout_type_3_1_2,
    channel_layout_type_max = channel_layout_type_3_1_2,
    channel_layout_type_count
};

typedef struct IADecoder IADecoder;

/**\name Immersive audio decoder functions */
/**@{*/

/** Gets the total number of channels for the channel layout
  * (see @ref ChannelLayoutType).
  * @param type <tt>uint32_t</tt>: Channel layout type.
  * @returns The positive number for valid channle layout type,
  *          or a negative error code.
  */

int32_t channel_layout_get_channel_count (int32_t type);


/** Gets the size of an <code>IADecoder</code> structure.
  * @returns The size in bytes on success, or a negative error code
  *          on error.
  */

int32_t immersive_audio_decoder_get_size (void);


/** Allocates and initializes a immersive audio decoder state.
  * Call immersive_audio_decoder_destroy() to release
  * this object when finished.
  * @param Fs <tt>int32_t</tt>: Sampling rate to decode at (in Hz).
  *                                This must be one of 8000, 12000, 16000,
  *                                24000, or 48000.
  * @param channels <tt>int</tt>: Number of channels to output.
  *                               This must be at most 255.
  * @param meta <tt>const uint8_t *</tt>: meta data.
  * @param len <tt>uint32_t</tt>: meta data length.
  * @param codec_info <tt>int32_t</tt>: Whether the metadata contains codec
  *                                      specification information.
  * @param[out] error <tt>int *</tt>: Returns 0 on success, or a negative
  *                                   error code on failure.
  */

IADecoder *immersive_audio_decoder_create (
    int32_t Fs,
    int channels,
    const unsigned char *meta,
    int32_t len,
    int32_t codec_info,
    int *error
);


/** Intialize a previously allocated immersive audio decoder state object.
  * This is intended for applications which use their own allocator instead of
  * malloc.
  * To reset a previously initialized state, use
  * immersive_audio_decoder_uninit().
  * @see immersive_audio_decoder_create
  * @see immersive_audio_deocder_get_size
  * @param st <tt>IADecoder*</tt>: Immersive audio decoder state to
  *                                              initialize.
  * @param Fs <tt>int32_t</tt>: Sampling rate to decode at (in Hz).
  *                                This must be one of 8000, 12000, 16000,
  *                                24000, or 48000.
  * @param channels <tt>int</tt>: Number of channels to output.
  *                               This must be at most 255.
  * @param meta <tt>uint8_t *</tt>: Meta data.
  * @param len <tt>uint32_t</tt>: Meta data length.
  * @param codec_info <tt>int32_t</tt>: Whether the metadata contains codec
  *                                      specification information.
  * @returns 0 on success, or a negative error code on failure.
  */

int immersive_audio_decoder_init (
    IADecoder *st,
    int32_t Fs,
    int channels,
    const unsigned char *meta,
    int32_t len,
    int32_t codec_info
);


/** Gets the all channel layout types supported by the immersive audio decoder.
  * @param st <tt>IADecoder*</tt>: Immersive audio decoder state.
  * @param[out] types <tt>uint32_t</tt>: Channel layout types.
  * @returns The positive number for valid channle layout type,
  *          or a negative error code.
  */

int immersive_audio_get_valid_layouts (
    IADecoder *st,
    int32_t types[channel_layout_type_count]
);


/** Set output layout to immersive audio decoder.
  * @param st <tt>IADecoder*</tt>: Immersive audio decoder state.
  * @param type <tt>uint32_t</tt>: Channel layout type.
  * @returns #OPUS_OK on success, or an error code (see @ref opus_errorcodes)
  *          on failure.
  */
int immersive_audio_set_layout (
    IADecoder *st,
    int32_t type
);


/** Decode a immersive audio packet.
  * @param st <tt>IADecoder*</tt>: Immersive audio decoder state.
  * @param[in] data <tt>const unsigned char*</tt>: Input payload.
  *                                                Use a <code>NULL</code>
  *                                                pointer to indicate packet
  *                                                loss.
  * @param len <tt>int32_t</tt>: Number of bytes in payload.
  * @param[out] pcm <tt>int16_t*</tt>: Output signal, with interleaved
  *                                       samples.
  *                                       This must contain room for
  *                                       <code>frame_size*channels</code>
  *                                       samples.
  * @param frame_size <tt>int</tt>: The number of samples per channel of
  *                                 available space in \a pcm.
  *                                 If this is less than the maximum packet duration
  *                                 (120 ms; 5760 for 48kHz), this function will not be capable
  *                                 of decoding some packets. In the case of PLC (data==NULL)
  *                                 or FEC (decode_fec=1), then frame_size needs to be exactly
  *                                 the duration of audio that is missing, otherwise the
  *                                 decoder will not be in the optimal state to decode the
  *                                 next incoming packet. For the PLC and FEC cases, frame_size
  *                                 <b>must</b> be a multiple of 2.5 ms.
  * @param decode_fec <tt>int</tt>: Flag (0 or 1) to request that any in-band
  *                                 forward error correction data be decoded.
  *                                 If no such data is available, the frame is
  *                                 decoded as if it were lost.
  * @param demixing_mode <tt>int</tt>: demixing mode.
  * @returns Number of samples decoded on success or a negative error code
  *          on failure.
  */
int immersive_audio_decode(
    IADecoder *st,
    const unsigned char *data,
    int32_t len,
    int16_t *pcm,
    int frame_size,
    int decode_fec,
    int demixing_mode
);


/** Resets an <code>IADecoder</code> allocated by
  * immersive_audio_decoder_create().
  * @param st <tt>IADecoder</tt>: Immersive audio decoder state to be
  *                                   uninitialized.
  */
void immersive_audio_decoder_uninit (IADecoder *st);


/** Frees an <code>IADecoder</code> allocated by
  * immersive_audio_decoder_create().
  * @param st <tt>IADecoder</tt>: Immersive audio decoder state to be freed.
  */
void immersive_audio_decoder_destroy(IADecoder *st);


/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* _IMMERSIVE_AUDIO_DECODER_H_ */
