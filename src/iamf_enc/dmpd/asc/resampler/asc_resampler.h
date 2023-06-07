#ifndef ASC_RESAMPLER_H
#define ASC_RESAMPLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASC_RESAMPLER_QUALITY_MAX 10
#define ASC_RESAMPLER_QUALITY_MIN 0
#define ASC_RESAMPLER_QUALITY_DEFAULT 4
#define ASC_RESAMPLER_QUALITY_VOIP 3
#define ASC_RESAMPLER_QUALITY_DESKTOP 5

	enum {
		ASC_RESAMPLER_ERR_SUCCESS = 0,
		ASC_RESAMPLER_ERR_ALLOC_FAILED = 1,
		ASC_RESAMPLER_ERR_BAD_STATE = 2,
		ASC_RESAMPLER_ERR_INVALID_ARG = 3,
		ASC_RESAMPLER_ERR_PTR_OVERLAP = 4,
		ASC_RESAMPLER_ERR_OVERFLOW = 5,

		ASC_RESAMPLER_ERR_MAX_ERROR
	};

	struct AscResamplerState_;
	typedef struct AscResamplerState_ AscResamplerState;
  typedef int(*asc_resampler_basic_func)(AscResamplerState *, uint32_t, const float *, uint32_t *, float *, uint32_t *);

  struct AscResamplerState_ {
    int16_t *buffer;

    uint32_t in_rate;
    uint32_t out_rate;
    uint32_t num_rate;
    uint32_t den_rate;

    int    quality;
    uint32_t nb_channels;
    uint32_t filt_len;
    uint32_t mem_alloc_size;
    uint32_t buffer_size;
    int          int_advance;
    int          frac_advance;
    float  cutoff;
    uint32_t oversample;
    int          initialised;
    int          started;

    /* These are per-channel */
    int32_t  *last_sample;
    uint32_t *samp_frac_num;
    uint32_t *magic_samples;

    float *mem;
    float *sinc_table;
    uint32_t sinc_table_length;
    asc_resampler_basic_func resampler_ptr;

    int    in_stride;
    int    out_stride;
  };
	/** Create a new resampler with integer input and output rates.
	* @param nb_channels Number of channels to be processed
	* @param in_rate Input sampling rate (integer number of Hz).
	* @param out_rate Output sampling rate (integer number of Hz).
	* @param quality Resampling quality between 0 and 10, where 0 has poor quality
	* and 10 has very high quality.
	* @return Newly created resampler state
	* @retval NULL Error: not enough memory
	*/
	AscResamplerState *asc_resampler_init(uint32_t nb_channels,
		uint32_t in_rate,
		uint32_t out_rate,
		int quality,
		int *err);

	/** Create a new resampler with fractional input/output rates. The sampling
	* rate ratio is an arbitrary rational number with both the numerator and
	* denominator being 32-bit integers.
	* @param nb_channels Number of channels to be processed
	* @param ratio_num Numerator of the sampling rate ratio
	* @param ratio_den Denominator of the sampling rate ratio
	* @param in_rate Input sampling rate rounded to the nearest integer (in Hz).
	* @param out_rate Output sampling rate rounded to the nearest integer (in Hz).
	* @param quality Resampling quality between 0 and 10, where 0 has poor quality
	* and 10 has very high quality.
	* @return Newly created resampler state
	* @retval NULL Error: not enough memory
	*/
	AscResamplerState *asc_resampler_init_frac(uint32_t nb_channels,
		uint32_t ratio_num,
		uint32_t ratio_den,
		uint32_t in_rate,
		uint32_t out_rate,
		int quality,
		int *err);

	/** Destroy a resampler state.
	* @param st Resampler state
	*/
	void asc_resampler_destroy(AscResamplerState *st);

	/** Resample an int array. The input and output buffers must *not* overlap.
	* @param st Resampler state
	* @param channel_index Index of the channel to process for the multi-channel
	* base (0 otherwise)
	* @param in Input buffer
	* @param in_len Number of input samples in the input buffer. Returns the number
	* of samples processed
	* @param out Output buffer
	* @param out_len Size of the output buffer. Returns the number of samples written
	*/
	int asc_resampler_process_int(AscResamplerState *st,
		uint32_t channel_index,
		const int16_t *in,
		uint32_t *in_len,
		int16_t *out,
		uint32_t *out_len);

	/** Resample an interleaved int array. The input and output buffers must *not* overlap.
	* @param st Resampler state
	* @param in Input buffer
	* @param in_len Number of input samples in the input buffer. Returns the number
	* of samples processed. This is all per-channel.
	* @param out Output buffer
	* @param out_len Size of the output buffer. Returns the number of samples written.
	* This is all per-channel.
	*/
	int asc_resampler_process_interleaved_int(AscResamplerState *st,
		const int16_t *in,
		uint32_t *in_len,
		int16_t *out,
		uint32_t *out_len);

	/** Set (change) the input/output sampling rates (integer value).
	* @param st Resampler state
	* @param in_rate Input sampling rate (integer number of Hz).
	* @param out_rate Output sampling rate (integer number of Hz).
	*/
	int asc_resampler_set_rate(AscResamplerState *st,
		uint32_t in_rate,
		uint32_t out_rate);

	/** Get the current input/output sampling rates (integer value).
	* @param st Resampler state
	* @param in_rate Input sampling rate (integer number of Hz) copied.
	* @param out_rate Output sampling rate (integer number of Hz) copied.
	*/
	void asc_resampler_get_rate(AscResamplerState *st,
		uint32_t *in_rate,
		uint32_t *out_rate);

	/** Set (change) the input/output sampling rates and resampling ratio
	* (fractional values in Hz supported).
	* @param st Resampler state
	* @param ratio_num Numerator of the sampling rate ratio
	* @param ratio_den Denominator of the sampling rate ratio
	* @param in_rate Input sampling rate rounded to the nearest integer (in Hz).
	* @param out_rate Output sampling rate rounded to the nearest integer (in Hz).
	*/
	int asc_resampler_set_rate_frac(AscResamplerState *st,
		uint32_t ratio_num,
		uint32_t ratio_den,
		uint32_t in_rate,
		uint32_t out_rate);

	/** Get the current resampling ratio. This will be reduced to the least
	* common denominator.
	* @param st Resampler state
	* @param ratio_num Numerator of the sampling rate ratio copied
	* @param ratio_den Denominator of the sampling rate ratio copied
	*/
	void asc_resampler_get_ratio(AscResamplerState *st,
		uint32_t *ratio_num,
		uint32_t *ratio_den);

	/** Set (change) the conversion quality.
	* @param st Resampler state
	* @param quality Resampling quality between 0 and 10, where 0 has poor
	* quality and 10 has very high quality.
	*/
	int asc_resampler_set_quality(AscResamplerState *st,
		int quality);

	/** Get the conversion quality.
	* @param st Resampler state
	* @param quality Resampling quality between 0 and 10, where 0 has poor
	* quality and 10 has very high quality.
	*/
	void asc_resampler_get_quality(AscResamplerState *st,
		int *quality);

	/** Set (change) the input stride.
	* @param st Resampler state
	* @param stride Input stride
	*/
	void asc_resampler_set_input_stride(AscResamplerState *st,
		uint32_t stride);

	/** Get the input stride.
	* @param st Resampler state
	* @param stride Input stride copied
	*/
	void asc_resampler_get_input_stride(AscResamplerState *st,
		uint32_t *stride);

	/** Set (change) the output stride.
	* @param st Resampler state
	* @param stride Output stride
	*/
	void asc_resampler_set_output_stride(AscResamplerState *st,
		uint32_t stride);

	/** Get the output stride.
	* @param st Resampler state copied
	* @param stride Output stride
	*/
	void asc_resampler_get_output_stride(AscResamplerState *st,
		uint32_t *stride);

	/** Get the latency introduced by the resampler measured in input samples.
	* @param st Resampler state
	*/
	int asc_resampler_get_input_latency(AscResamplerState *st);

	/** Get the latency introduced by the resampler measured in output samples.
	* @param st Resampler state
	*/
	int asc_resampler_get_output_latency(AscResamplerState *st);

	/** Make sure that the first samples to go out of the resamplers don't have
	* leading zeros. This is only useful before starting to use a newly created
	* resampler. It is recommended to use that when resampling an audio file, as
	* it will generate a file with the same length. For real-time processing,
	* it is probably easier not to use this call (so that the output duration
	* is the same for the first frame).
	* @param st Resampler state
	*/
	int asc_resampler_skip_zeros(AscResamplerState *st);

	/** Reset a resampler so a new (unrelated) stream can be processed.
	* @param st Resampler state
	*/
	int asc_resampler_reset_mem(AscResamplerState *st);

	/** Returns the English meaning for an error code
	* @param err Error code
	* @return English string
	*/
	const char *asc_resampler_strerror(int err);

	int asc_resampler_process_float(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len);
	int asc_resampler_process_interleaved_float(AscResamplerState *st, const float *in, uint32_t *in_len, float *out, uint32_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
