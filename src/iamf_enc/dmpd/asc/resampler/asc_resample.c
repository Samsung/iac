#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include "asc_resampler.h"

static void *asc_resample_alloc(int size) { return calloc(size, 1); }
static void *asc_resample_realloc(void *ptr, int size) { return realloc(ptr, size); }
static void asc_resample_free(void *ptr) { free(ptr); }

#define RS_MULT16_16(a,b)     ((float)(a)*(float)(b))
#define RS_PSHR32(a,shift) (a)
#define RS_MULT16_32_Q15(a,b)     ((a)*(b))
#define RS_SATURATE32PSHR(x,shift,a) (x)
#define RS_FLT2INT16(x) ((x) < -32767 ? -32768 : ((x) > 32766 ? 32767 : (x)))
#define RS_FLTADJUST(x) ((x) < -1.0 ? -1.0 : ((x) > 1.0 ? 1.0 : (x)))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RS_IMAX(a,b) ((a) > (b) ? (a) : (b))
#define RS_IMIN(a,b) ((a) < (b) ? (a) : (b))

#ifndef NULL
#define NULL 0
#endif

#ifndef UINT32_MAX
#define UINT32_MAX 4294967295U
#endif

/* Numer of elements to allocate on the stack */
#define RS_FIXED_STACK_ALLOC 1024

//static const float kaiser12_table[68] = {
static const double kaiser12_table[68] = {
	0.99859849, 1.00000000, 0.99859849, 0.99440475, 0.98745105, 0.97779076,
	0.96549770, 0.95066529, 0.93340547, 0.91384741, 0.89213598, 0.86843014,
	0.84290116, 0.81573067, 0.78710866, 0.75723148, 0.72629970, 0.69451601,
	0.66208321, 0.62920216, 0.59606986, 0.56287762, 0.52980938, 0.49704014,
	0.46473455, 0.43304576, 0.40211431, 0.37206735, 0.34301800, 0.31506490,
	0.28829195, 0.26276832, 0.23854851, 0.21567274, 0.19416736, 0.17404546,
	0.15530766, 0.13794294, 0.12192957, 0.10723616, 0.09382272, 0.08164178,
	0.07063950, 0.06075685, 0.05193064, 0.04409466, 0.03718069, 0.03111947,
	0.02584161, 0.02127838, 0.01736250, 0.01402878, 0.01121463, 0.00886058,
	0.00691064, 0.00531256, 0.00401805, 0.00298291, 0.00216702, 0.00153438,
	0.00105297, 0.00069463, 0.00043489, 0.00025272, 0.00013031, 0.0000527734,
	0.00001000, 0.00000000 };
/*
static const float kaiser12_table[36] = {
static const double kaiser12_table[36] = {
0.99440475, 1.00000000, 0.99440475, 0.97779076, 0.95066529, 0.91384741,
0.86843014, 0.81573067, 0.75723148, 0.69451601, 0.62920216, 0.56287762,
0.49704014, 0.43304576, 0.37206735, 0.31506490, 0.26276832, 0.21567274,
0.17404546, 0.13794294, 0.10723616, 0.08164178, 0.06075685, 0.04409466,
0.03111947, 0.02127838, 0.01402878, 0.00886058, 0.00531256, 0.00298291,
0.00153438, 0.00069463, 0.00025272, 0.0000527734, 0.00000500, 0.00000000};
*/
//static const float kaiser10_table[36] = {
static const double kaiser10_table[36] = {
	0.99537781, 1.00000000, 0.99537781, 0.98162644, 0.95908712, 0.92831446,
	0.89005583, 0.84522401, 0.79486424, 0.74011713, 0.68217934, 0.62226347,
	0.56155915, 0.50119680, 0.44221549, 0.38553619, 0.33194107, 0.28205962,
	0.23636152, 0.19515633, 0.15859932, 0.12670280, 0.09935205, 0.07632451,
	0.05731132, 0.04193980, 0.02979584, 0.02044510, 0.01345224, 0.00839739,
	0.00488951, 0.00257636, 0.00115101, 0.00035515, 0.00000000, 0.00000000 };

//static const float kaiser8_table[36] = {
static const double kaiser8_table[36] = {
	0.99635258, 1.00000000, 0.99635258, 0.98548012, 0.96759014, 0.94302200,
	0.91223751, 0.87580811, 0.83439927, 0.78875245, 0.73966538, 0.68797126,
	0.63451750, 0.58014482, 0.52566725, 0.47185369, 0.41941150, 0.36897272,
	0.32108304, 0.27619388, 0.23465776, 0.19672670, 0.16255380, 0.13219758,
	0.10562887, 0.08273982, 0.06335451, 0.04724088, 0.03412321, 0.02369490,
	0.01563093, 0.00959968, 0.00527363, 0.00233883, 0.00050000, 0.00000000 };

//static const float kaiser6_table[36] = {
static const double kaiser6_table[36] = {
	0.99733006, 1.00000000, 0.99733006, 0.98935595, 0.97618418, 0.95799003,
	0.93501423, 0.90755855, 0.87598009, 0.84068475, 0.80211977, 0.76076565,
	0.71712752, 0.67172623, 0.62508937, 0.57774224, 0.53019925, 0.48295561,
	0.43647969, 0.39120616, 0.34752997, 0.30580127, 0.26632152, 0.22934058,
	0.19505503, 0.16360756, 0.13508755, 0.10953262, 0.08693120, 0.06722600,
	0.05031820, 0.03607231, 0.02432151, 0.01487334, 0.00752000, 0.00000000 };

struct ResampleFuncDef {
	//const float *table;
	const double *table;
	int oversample;
};

static const struct ResampleFuncDef kaiser12_funcdef = { kaiser12_table, 64 };
#define RS_KAISER12 (&kaiser12_funcdef)
static const struct ResampleFuncDef kaiser10_funcdef = { kaiser10_table, 32 };
#define RS_KAISER10 (&kaiser10_funcdef)
static const struct ResampleFuncDef kaiser8_funcdef = { kaiser8_table, 32 };
#define RS_KAISER8 (&kaiser8_funcdef)
static const struct ResampleFuncDef kaiser6_funcdef = { kaiser6_table, 32 };
#define RS_KAISER6 (&kaiser6_funcdef)

struct ResampleFQualityMapping {
	int base_length;
	int oversample;
	float downsample_bandwidth;
	float upsample_bandwidth;
	const struct ResampleFuncDef *window_func;
};


/* This table maps conversion quality to internal parameters. There are two
reasons that explain why the up-sampling bandwidth is larger than the
down-sampling bandwidth:
1) When up-sampling, we can assume that the spectrum is already attenuated
close to the Nyquist rate (from an A/D or a previous resampling filter)
2) Any aliasing that occurs very close to the Nyquist rate will be masked
by the sinusoids/noise just below the Nyquist rate (guaranteed only for
up-sampling).
*/
static const struct ResampleFQualityMapping quality_map[11] = {
		{ 8, 4, 0.830f, 0.860f, RS_KAISER6 }, /* Q0 */
		{ 16, 4, 0.850f, 0.880f, RS_KAISER6 }, /* Q1 */
		{ 32, 4, 0.882f, 0.910f, RS_KAISER6 }, /* Q2 */  /* 82.3% cutoff ( ~60 dB stop) 6  */
		{ 48, 8, 0.895f, 0.917f, RS_KAISER8 }, /* Q3 */  /* 84.9% cutoff ( ~80 dB stop) 8  */
		{ 64, 8, 0.921f, 0.940f, RS_KAISER8 }, /* Q4 */  /* 88.7% cutoff ( ~80 dB stop) 8  */
		{ 80, 16, 0.922f, 0.940f, RS_KAISER10 }, /* Q5 */  /* 89.1% cutoff (~100 dB stop) 10 */
		{ 96, 16, 0.940f, 0.945f, RS_KAISER10 }, /* Q6 */  /* 91.5% cutoff (~100 dB stop) 10 */
		{ 128, 16, 0.950f, 0.950f, RS_KAISER10 }, /* Q7 */  /* 93.1% cutoff (~100 dB stop) 10 */
		{ 160, 16, 0.960f, 0.960f, RS_KAISER10 }, /* Q8 */  /* 94.5% cutoff (~100 dB stop) 10 */
		{ 192, 32, 0.968f, 0.968f, RS_KAISER12 }, /* Q9 */  /* 95.5% cutoff (~100 dB stop) 10 */
		{ 256, 32, 0.975f, 0.975f, RS_KAISER12 }, /* Q10 */ /* 96.6% cutoff (~100 dB stop) 10 */
};
/*8,24,40,56,80,104,128,160,200,256,320*/
static double compute_func(float x, const struct ResampleFuncDef *func)
{ ////
	float y, frac;
	double interp[4];
	int ind;
	y = x*func->oversample;
	ind = (int)floor(y);
	frac = (y - ind);
	/* CSE with handle the repeated powers */
	interp[3] = -0.1666666667*frac + 0.1666666667*(frac*frac*frac);
	interp[2] = frac + 0.5*(frac*frac) - 0.5*(frac*frac*frac);
	/*interp[2] = 1.f - 0.5f*frac - frac*frac + 0.5f*frac*frac*frac;*/
	interp[0] = -0.3333333333*frac + 0.5*(frac*frac) - 0.1666666667*(frac*frac*frac);
	/* Just to make sure we don't have rounding problems */
	interp[1] = 1.f - interp[3] - interp[2] - interp[0];

	/*sum = frac*accum[1] + (1-frac)*accum[2];*/
	return interp[0] * func->table[ind] + interp[1] * func->table[ind + 1] + interp[2] * func->table[ind + 2] + interp[3] * func->table[ind + 3];
}

/* The slow way of computing a sinc for the table. Should improve that some day */
static float sinc(float cutoff, float x, int N, const struct ResampleFuncDef *window_func)
{ /////
	/*fprintf (stderr, "%f ", x);*/
	float xx = x * cutoff;
	if (fabs(x)<1e-6)
		return cutoff;
	else if (fabs(x) > .5*N)
		return 0;
	/*FIXME: Can it really be any slower than this? */
	return cutoff*sin(M_PI*xx) / (M_PI*xx) * compute_func(fabs(2.*x / N), window_func);
}

static void cubic_coef(float frac, float interp[4])
{ ////
	/* Compute interpolation coefficients. I'm not sure whether this corresponds to cubic interpolation
	but I know it's MMSE-optimal on a sinc */
	interp[0] = -0.16667f*frac + 0.16667f*frac*frac*frac;
	interp[1] = frac + 0.5f*frac*frac - 0.5f*frac*frac*frac;
	/*interp[2] = 1.f - 0.5f*frac - frac*frac + 0.5f*frac*frac*frac;*/
	interp[3] = -0.33333f*frac + 0.5f*frac*frac - 0.16667f*frac*frac*frac;
	/* Just to make sure we don't have rounding problems */
	interp[2] = 1. - interp[0] - interp[1] - interp[3];
}

static int resampler_basic_direct_single(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ /////
	const int N = st->filt_len;
	int out_sample = 0;
	int last_sample = st->last_sample[channel_index];
	uint32_t samp_frac_num = st->samp_frac_num[channel_index];
	const float *sinc_table = st->sinc_table;
	const int out_stride = st->out_stride;
	const int int_advance = st->int_advance;
	const int frac_advance = st->frac_advance;
	const uint32_t den_rate = st->den_rate;
	float sum;

	while (!(last_sample >= (int32_t)*in_len || out_sample >= (int32_t)*out_len))
	{
		const float *sinct = &sinc_table[samp_frac_num*N];
		const float *iptr = &in[last_sample];

		int j;
		sum = 0;
		for (j = 0; j<N; j++) sum += RS_MULT16_16(sinct[j], iptr[j]);

		/*    This code is slower on most DSPs which have only 2 accumulators.
		Plus this this forces truncation to 32 bits and you lose the HW guard bits.
		I think we can trust the compiler and let it vectorize and/or unroll itself.
		float accum[4] = {0,0,0,0};
		for(j=0;j<N;j+=4) {
		accum[0] += RS_MULT16_16(sinct[j], iptr[j]);
		accum[1] += RS_MULT16_16(sinct[j+1], iptr[j+1]);
		accum[2] += RS_MULT16_16(sinct[j+2], iptr[j+2]);
		accum[3] += RS_MULT16_16(sinct[j+3], iptr[j+3]);
		}
		sum = accum[0] + accum[1] + accum[2] + accum[3];
		*/
		sum = RS_SATURATE32PSHR(sum, 15, 32767);

		out[out_stride * out_sample++] = sum;
		last_sample += int_advance;
		samp_frac_num += frac_advance;
		if (samp_frac_num >= den_rate)
		{
			samp_frac_num -= den_rate;
			last_sample++;
		}
	}

	st->last_sample[channel_index] = last_sample;
	st->samp_frac_num[channel_index] = samp_frac_num;
	return out_sample;
}

/* This is the same as the previous function, except with a double-precision accumulator */
static int resampler_basic_direct_double(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	const int N = st->filt_len;
	int out_sample = 0;
	int last_sample = st->last_sample[channel_index];
	uint32_t samp_frac_num = st->samp_frac_num[channel_index];
	const float *sinc_table = st->sinc_table;
	const int out_stride = st->out_stride;
	const int int_advance = st->int_advance;
	const int frac_advance = st->frac_advance;
	const uint32_t den_rate = st->den_rate;
	double sum;

	while (!(last_sample >= (int32_t)*in_len || out_sample >= (int32_t)*out_len))
	{
		const float *sinct = &sinc_table[samp_frac_num*N];
		const float *iptr = &in[last_sample];

		int j;
		double accum[4] = { 0, 0, 0, 0 };

		for (j = 0; j<N; j += 4) {
			accum[0] += sinct[j] * iptr[j];
			accum[1] += sinct[j + 1] * iptr[j + 1];
			accum[2] += sinct[j + 2] * iptr[j + 2];
			accum[3] += sinct[j + 3] * iptr[j + 3];
		}
		sum = accum[0] + accum[1] + accum[2] + accum[3];

		out[out_stride * out_sample++] = RS_PSHR32(sum, 15);
		last_sample += int_advance;
		samp_frac_num += frac_advance;
		if (samp_frac_num >= den_rate)
		{
			samp_frac_num -= den_rate;
			last_sample++;
		}
	}

	st->last_sample[channel_index] = last_sample;
	st->samp_frac_num[channel_index] = samp_frac_num;
	return out_sample;
}

static int resampler_basic_interpolate_single(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	const int N = st->filt_len;
	int out_sample = 0;
	int last_sample = st->last_sample[channel_index];
	uint32_t samp_frac_num = st->samp_frac_num[channel_index];
	const int out_stride = st->out_stride;
	const int int_advance = st->int_advance;
	const int frac_advance = st->frac_advance;
	const uint32_t den_rate = st->den_rate;
	float sum;

	while (!(last_sample >= (int32_t)*in_len || out_sample >= (int32_t)*out_len))
	{
		const float *iptr = &in[last_sample];

		const int offset = samp_frac_num*st->oversample / st->den_rate;
		const float frac = ((float)((samp_frac_num*st->oversample) % st->den_rate)) / st->den_rate;

		float interp[4];

		int j;
		float accum[4] = { 0, 0, 0, 0 };

		for (j = 0; j<N; j++) {
			const float curr_in = iptr[j];
			accum[0] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset - 2]);
			accum[1] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset - 1]);
			accum[2] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset]);
			accum[3] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset + 1]);
		}

		cubic_coef(frac, interp);
		sum = RS_MULT16_32_Q15(interp[0], accum[0]) + RS_MULT16_32_Q15(interp[1], accum[1]) + RS_MULT16_32_Q15(interp[2], accum[2]) + RS_MULT16_32_Q15(interp[3], accum[3]);
		sum = RS_SATURATE32PSHR(sum, 15, 32767);

		out[out_stride * out_sample++] = sum;
		last_sample += int_advance;
		samp_frac_num += frac_advance;
		if (samp_frac_num >= den_rate)
		{
			samp_frac_num -= den_rate;
			last_sample++;
		}
	}

	st->last_sample[channel_index] = last_sample;
	st->samp_frac_num[channel_index] = samp_frac_num;

	return out_sample;
}

/* This is the same as the previous function, except with a double-precision accumulator */
static int resampler_basic_interpolate_double(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ /////
	const int N = st->filt_len;
	int out_sample = 0;
	int last_sample = st->last_sample[channel_index];
	uint32_t samp_frac_num = st->samp_frac_num[channel_index];
	const int out_stride = st->out_stride;
	const int int_advance = st->int_advance;
	const int frac_advance = st->frac_advance;
	const uint32_t den_rate = st->den_rate;
	float sum;

	while (!(last_sample >= (int32_t)*in_len || out_sample >= (int32_t)*out_len))
	{
		const float *iptr = &in[last_sample];

		const int offset = samp_frac_num*st->oversample / st->den_rate;
		const float frac = ((float)((samp_frac_num*st->oversample) % st->den_rate)) / st->den_rate;
		float interp[4];

		int j;
		double accum[4] = { 0, 0, 0, 0 };

		for (j = 0; j<N; j++) {
			const double curr_in = iptr[j];
			accum[0] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset - 2]);
			accum[1] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset - 1]);
			accum[2] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset]);
			accum[3] += RS_MULT16_16(curr_in, st->sinc_table[4 + (j + 1)*st->oversample - offset + 1]);
		}

		cubic_coef(frac, interp);
		sum = RS_MULT16_32_Q15(interp[0], accum[0]) + RS_MULT16_32_Q15(interp[1], accum[1]) + RS_MULT16_32_Q15(interp[2], accum[2]) + RS_MULT16_32_Q15(interp[3], accum[3]);

		out[out_stride * out_sample++] = RS_PSHR32(sum, 15);
		last_sample += int_advance;
		samp_frac_num += frac_advance;
		if (samp_frac_num >= den_rate)
		{
			samp_frac_num -= den_rate;
			last_sample++;
		}
	}

	st->last_sample[channel_index] = last_sample;
	st->samp_frac_num[channel_index] = samp_frac_num;
	return out_sample;
}

/* This resampler is used to produce zero output in situations where memory
for the filter could not be allocated.  The expected numbers of input and
output samples are still processed so that callers failing to check error
codes are not surprised, possibly getting into infinite loops. */
static int resampler_basic_zero(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	int out_sample = 0;
	int last_sample = st->last_sample[channel_index];
	uint32_t samp_frac_num = st->samp_frac_num[channel_index];
	const int out_stride = st->out_stride;
	const int int_advance = st->int_advance;
	const int frac_advance = st->frac_advance;
	const uint32_t den_rate = st->den_rate;

	(void)in;
	while (!(last_sample >= (int32_t)*in_len || out_sample >= (int32_t)*out_len))
	{
		out[out_stride * out_sample++] = 0;
		last_sample += int_advance;
		samp_frac_num += frac_advance;
		if (samp_frac_num >= den_rate)
		{
			samp_frac_num -= den_rate;
			last_sample++;
		}
	}

	st->last_sample[channel_index] = last_sample;
	st->samp_frac_num[channel_index] = samp_frac_num;
	return out_sample;
}

static int multiply_frac(uint32_t *result, uint32_t value, uint32_t num, uint32_t den)
{ ///
	uint32_t major = value / den;
	uint32_t remain = value % den;
	/* TODO: Could use 64 bits operation to check for overflow. But only guaranteed in C99+ */
	if (remain > UINT32_MAX / num || major > UINT32_MAX / num
		|| major * num > UINT32_MAX - remain * num / den)
		return ASC_RESAMPLER_ERR_OVERFLOW;
	*result = remain * num / den + major * num;
	return ASC_RESAMPLER_ERR_SUCCESS;
}

static int update_filter(AscResamplerState *st)
{ ////
	uint32_t old_length = st->filt_len;
	uint32_t old_alloc_size = st->mem_alloc_size;
	int use_direct;
	uint32_t min_sinc_table_length;
	uint32_t min_alloc_size;

	st->int_advance = st->num_rate / st->den_rate;
	st->frac_advance = st->num_rate%st->den_rate;
	st->oversample = quality_map[st->quality].oversample;
	st->filt_len = quality_map[st->quality].base_length;

	if (st->num_rate > st->den_rate)
	{
		/* down-sampling */
		st->cutoff = quality_map[st->quality].downsample_bandwidth * st->den_rate / st->num_rate;
		if (multiply_frac(&st->filt_len, st->filt_len, st->num_rate, st->den_rate) != ASC_RESAMPLER_ERR_SUCCESS)
			goto fail;
		/* Round up to make sure we have a multiple of 8 for SSE */
		st->filt_len = ((st->filt_len - 1)&(~0x7)) + 8;
		if (2 * st->den_rate < st->num_rate)
			st->oversample >>= 1;
		if (4 * st->den_rate < st->num_rate)
			st->oversample >>= 1;
		if (8 * st->den_rate < st->num_rate)
			st->oversample >>= 1;
		if (16 * st->den_rate < st->num_rate)
			st->oversample >>= 1;
		if (st->oversample < 1)
			st->oversample = 1;
	}
	else {
		/* up-sampling */
		st->cutoff = quality_map[st->quality].upsample_bandwidth;
	}

	/* Choose the resampling type that requires the least amount of memory */
	use_direct = st->filt_len*st->den_rate <= st->filt_len*st->oversample + 8
		&& INT_MAX / sizeof(float) / st->den_rate >= st->filt_len;

	if (use_direct)
	{
		min_sinc_table_length = st->filt_len*st->den_rate;
	}
	else {
		if ((INT_MAX / sizeof(float) - 8) / st->oversample < st->filt_len)
			goto fail;

		min_sinc_table_length = st->filt_len*st->oversample + 8;
	}
	if (st->sinc_table_length < min_sinc_table_length)
	{
		float *sinc_table = (float *)asc_resample_realloc(st->sinc_table, min_sinc_table_length*sizeof(float));
		if (!sinc_table)
			goto fail;

		st->sinc_table = sinc_table;
		st->sinc_table_length = min_sinc_table_length;
	}
	if (use_direct)
	{
		uint32_t i;
		for (i = 0; i<st->den_rate; i++)
		{
			int32_t j;
			for (j = 0; j<st->filt_len; j++)
			{
				st->sinc_table[i*st->filt_len + j] = sinc(st->cutoff, ((j - (int32_t)st->filt_len / 2 + 1) - ((float)i) / st->den_rate), st->filt_len, quality_map[st->quality].window_func);
			}
		}
		if (st->quality>8)
			st->resampler_ptr = resampler_basic_direct_double;
		else
			st->resampler_ptr = resampler_basic_direct_single;

		/*fprintf (stderr, "resampler uses direct sinc table and normalised cutoff %f\n", cutoff);*/
	}
	else {
		int32_t i;
		for (i = -4; i<(int32_t)(st->oversample*st->filt_len + 4); i++)
			st->sinc_table[i + 4] = sinc(st->cutoff, (i / (float)st->oversample - st->filt_len / 2), st->filt_len, quality_map[st->quality].window_func);
		if (st->quality>8)
			st->resampler_ptr = resampler_basic_interpolate_double;
		else
			st->resampler_ptr = resampler_basic_interpolate_single;

		/*fprintf (stderr, "resampler uses interpolated sinc table and normalised cutoff %f\n", cutoff);*/
	}

	/* Here's the place where we update the filter memory to take into account
	the change in filter length. It's probably the messiest part of the code
	due to handling of lots of corner cases. */

	/* Adding buffer_size to filt_len won't overflow here because filt_len
	could be multiplied by sizeof(float) above. */
	min_alloc_size = st->filt_len - 1 + st->buffer_size;
	if (min_alloc_size > st->mem_alloc_size)
	{
		float *mem;
		if (INT_MAX / sizeof(float) / st->nb_channels < min_alloc_size)
			goto fail;
		else if (!(mem = (float*)asc_resample_realloc(st->mem, st->nb_channels*min_alloc_size * sizeof(*mem))))
			goto fail;

		st->mem = mem;
		st->mem_alloc_size = min_alloc_size;
	}
	if (!st->started)
	{
		uint32_t i;
		for (i = 0; i<st->nb_channels*st->mem_alloc_size; i++)
			st->mem[i] = 0;
		/*asc_warning("reinit filter");*/
	}
	else if (st->filt_len > old_length)
	{
		uint32_t i;
		/* Increase the filter length */
		/*asc_warning("increase filter size");*/
		for (i = st->nb_channels; i--;)
		{
			uint32_t j;
			uint32_t olen = old_length;
			/*if (st->magic_samples[i])*/
			{
				/* Try and remove the magic samples as if nothing had happened */

				/* FIXME: This is wrong but for now we need it to avoid going over the array bounds */
				olen = old_length + 2 * st->magic_samples[i];
				for (j = old_length - 1 + st->magic_samples[i]; j--;)
					st->mem[i*st->mem_alloc_size + j + st->magic_samples[i]] = st->mem[i*old_alloc_size + j];
				for (j = 0; j<st->magic_samples[i]; j++)
					st->mem[i*st->mem_alloc_size + j] = 0;
				st->magic_samples[i] = 0;
			}
			if (st->filt_len > olen)
			{
				/* If the new filter length is still bigger than the "augmented" length */
				/* Copy data going backward */
				for (j = 0; j<olen - 1; j++)
					st->mem[i*st->mem_alloc_size + (st->filt_len - 2 - j)] = st->mem[i*st->mem_alloc_size + (olen - 2 - j)];
				/* Then put zeros for lack of anything better */
				for (; j<st->filt_len - 1; j++)
					st->mem[i*st->mem_alloc_size + (st->filt_len - 2 - j)] = 0;
				/* Adjust last_sample */
				st->last_sample[i] += (st->filt_len - olen) / 2;
			}
			else {
				/* Put back some of the magic! */
				st->magic_samples[i] = (olen - st->filt_len) / 2;
				for (j = 0; j<st->filt_len - 1 + st->magic_samples[i]; j++)
					st->mem[i*st->mem_alloc_size + j] = st->mem[i*st->mem_alloc_size + j + st->magic_samples[i]];
			}
		}
	}
	else if (st->filt_len < old_length)
	{
		uint32_t i;
		/* Reduce filter length, this a bit tricky. We need to store some of the memory as "magic"
		samples so they can be used directly as input the next time(s) */
		for (i = 0; i<st->nb_channels; i++)
		{
			uint32_t j;
			uint32_t old_magic = st->magic_samples[i];
			st->magic_samples[i] = (old_length - st->filt_len) / 2;
			/* We must copy some of the memory that's no longer used */
			/* Copy data going backward */
			for (j = 0; j<st->filt_len - 1 + st->magic_samples[i] + old_magic; j++)
				st->mem[i*st->mem_alloc_size + j] = st->mem[i*st->mem_alloc_size + j + st->magic_samples[i]];
			st->magic_samples[i] += old_magic;
		}
	}
	return ASC_RESAMPLER_ERR_SUCCESS;

fail:
	st->resampler_ptr = resampler_basic_zero;
	/* st->mem may still contain consumed input samples for the filter.
	Restore filt_len so that filt_len - 1 still points to the position after
	the last of these samples. */
	st->filt_len = old_length;
	return ASC_RESAMPLER_ERR_ALLOC_FAILED;
}

AscResamplerState *asc_resampler_init(uint32_t nb_channels, uint32_t in_rate, uint32_t out_rate, int quality, int *err)
{ /////
	return asc_resampler_init_frac(nb_channels, in_rate, out_rate, in_rate, out_rate, quality, err);
}

AscResamplerState *asc_resampler_init_frac(uint32_t nb_channels, uint32_t ratio_num, uint32_t ratio_den, uint32_t in_rate, uint32_t out_rate, int quality, int *err)
{ ///
	AscResamplerState *st;
	int filter_err;

	if (nb_channels == 0 || ratio_num == 0 || ratio_den == 0 || quality > 10 || quality < 0)
	{
		if (err)
			*err = ASC_RESAMPLER_ERR_INVALID_ARG;
		return NULL;
	}
	st = (AscResamplerState *)asc_resample_alloc(sizeof(AscResamplerState));
	if (!st)
	{
		if (err)
			*err = ASC_RESAMPLER_ERR_ALLOC_FAILED;
		return NULL;
	}
	st->initialised = 0;
	st->started = 0;
	st->in_rate = 0;
	st->out_rate = 0;
	st->num_rate = 0;
	st->den_rate = 0;
	st->quality = -1;
	st->sinc_table_length = 0;
	st->mem_alloc_size = 0;
	st->filt_len = 0;
	st->mem = 0;
	st->resampler_ptr = 0;

	st->cutoff = 1.f;
	st->nb_channels = nb_channels;
	st->in_stride = 1;
	st->out_stride = 1;

	st->buffer_size = 160;

	/* Per channel data */
	if (!(st->last_sample = (int32_t*)asc_resample_alloc(nb_channels*sizeof(int32_t))))
		goto fail;
	if (!(st->magic_samples = (uint32_t*)asc_resample_alloc(nb_channels*sizeof(uint32_t))))
		goto fail;
	if (!(st->samp_frac_num = (uint32_t*)asc_resample_alloc(nb_channels*sizeof(uint32_t))))
		goto fail;

	asc_resampler_set_quality(st, quality);
	asc_resampler_set_rate_frac(st, ratio_num, ratio_den, in_rate, out_rate);

	filter_err = update_filter(st);
	if (filter_err == ASC_RESAMPLER_ERR_SUCCESS)
	{
		st->initialised = 1;
	}
	else {
		asc_resampler_destroy(st);
		st = NULL;
	}
	if (err)
		*err = filter_err;

	return st;

fail:
	if (err)
		*err = ASC_RESAMPLER_ERR_ALLOC_FAILED;
	asc_resampler_destroy(st);
	return NULL;
}

void asc_resampler_destroy(AscResamplerState *st)
{ ////
	asc_resample_free(st->mem);
	asc_resample_free(st->sinc_table);
	asc_resample_free(st->last_sample);
	asc_resample_free(st->magic_samples);
	asc_resample_free(st->samp_frac_num);
	asc_resample_free(st);
}

static int asc_resampler_process_native(AscResamplerState *st, uint32_t channel_index, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	int j = 0;
	const int N = st->filt_len;
	int out_sample = 0;
	float *mem = st->mem + channel_index * st->mem_alloc_size;
	uint32_t ilen;

	st->started = 1;

	/* Call the right resampler through the function ptr */
	out_sample = st->resampler_ptr(st, channel_index, mem, in_len, out, out_len);

	if (st->last_sample[channel_index] < (int32_t)*in_len)
		*in_len = st->last_sample[channel_index];
	*out_len = out_sample;
	st->last_sample[channel_index] -= *in_len;

	ilen = *in_len;

	for (j = 0; j<N - 1; ++j)
		mem[j] = mem[j + ilen];

	return ASC_RESAMPLER_ERR_SUCCESS;
}

static int asc_resampler_magic(AscResamplerState *st, uint32_t channel_index, float **out, uint32_t out_len)
{ ////
	uint32_t tmp_in_len = st->magic_samples[channel_index];
	float *mem = st->mem + channel_index * st->mem_alloc_size;
	const int N = st->filt_len;

	asc_resampler_process_native(st, channel_index, &tmp_in_len, *out, &out_len);

	st->magic_samples[channel_index] -= tmp_in_len;

	/* If we couldn't process all "magic" input samples, save the rest for next time */
	if (st->magic_samples[channel_index])
	{
		uint32_t i;
		for (i = 0; i<st->magic_samples[channel_index]; i++)
			mem[N - 1 + i] = mem[N - 1 + i + tmp_in_len];
	}
	*out += out_len*st->out_stride;
	return out_len;
}

int asc_resampler_process_int(AscResamplerState *st, uint32_t channel_index, const int16_t *in, uint32_t *in_len, int16_t *out, uint32_t *out_len)
{ ////
	int j;
	const int istride_save = st->in_stride;
	const int ostride_save = st->out_stride;
	uint32_t ilen = *in_len;
	uint32_t olen = *out_len;
	float *x = st->mem + channel_index * st->mem_alloc_size;
	const uint32_t xlen = st->mem_alloc_size - (st->filt_len - 1);
	const unsigned int ylen = RS_FIXED_STACK_ALLOC;
	float ystack[RS_FIXED_STACK_ALLOC];

	st->out_stride = 1;

	while (ilen && olen) {
		float *y = ystack;
		uint32_t ichunk = (ilen > xlen) ? xlen : ilen;
		uint32_t ochunk = (olen > ylen) ? ylen : olen;
		uint32_t omagic = 0;

		if (st->magic_samples[channel_index]) {
			omagic = asc_resampler_magic(st, channel_index, &y, ochunk);
			ochunk -= omagic;
			olen -= omagic;
		}
		if (!st->magic_samples[channel_index]) {
			if (in) {
				for (j = 0; j<ichunk; ++j)
					x[j + st->filt_len - 1] = in[j*istride_save];
			}
			else {
				for (j = 0; j<ichunk; ++j)
					x[j + st->filt_len - 1] = 0;
			}

			asc_resampler_process_native(st, channel_index, &ichunk, y, &ochunk);
		}
		else {
			ichunk = 0;
			ochunk = 0;
		}

		for (j = 0; j<ochunk + omagic; ++j)
			out[j*ostride_save] = RS_FLT2INT16(ystack[j]);

		ilen -= ichunk;
		olen -= ochunk;
		out += (ochunk + omagic) * ostride_save;
		if (in)
			in += ichunk * istride_save;
	}
	st->out_stride = ostride_save;
	*in_len -= ilen;
	*out_len -= olen;

	return st->resampler_ptr == resampler_basic_zero ? ASC_RESAMPLER_ERR_ALLOC_FAILED : ASC_RESAMPLER_ERR_SUCCESS;
}

int asc_resampler_process_interleaved_int(AscResamplerState *st, const int16_t *in, uint32_t *in_len, int16_t *out, uint32_t *out_len)
{ ////
	uint32_t i;
	int istride_save, ostride_save;
	uint32_t bak_out_len = *out_len;
	uint32_t bak_in_len = *in_len;
	istride_save = st->in_stride;
	ostride_save = st->out_stride;
	st->in_stride = st->out_stride = st->nb_channels;
	for (i = 0; i<st->nb_channels; i++)
	{
		*out_len = bak_out_len;
		*in_len = bak_in_len;
		if (in != NULL)
			asc_resampler_process_int(st, i, in + i, in_len, out + i, out_len);
		else
			asc_resampler_process_int(st, i, NULL, in_len, out + i, out_len);
	}
	st->in_stride = istride_save;
	st->out_stride = ostride_save;
	return st->resampler_ptr == resampler_basic_zero ? ASC_RESAMPLER_ERR_ALLOC_FAILED : ASC_RESAMPLER_ERR_SUCCESS;
}

int asc_resampler_process_float(AscResamplerState *st, uint32_t channel_index, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	int j;
	const int istride_save = st->in_stride;
	const int ostride_save = st->out_stride;
	uint32_t ilen = *in_len;
	uint32_t olen = *out_len;
	float *x = st->mem + channel_index * st->mem_alloc_size;
	const uint32_t xlen = st->mem_alloc_size - (st->filt_len - 1);
	const unsigned int ylen = RS_FIXED_STACK_ALLOC;
	float ystack[RS_FIXED_STACK_ALLOC];

	st->out_stride = 1;

	while (ilen && olen) {
		float *y = ystack;
		uint32_t ichunk = (ilen > xlen) ? xlen : ilen;
		uint32_t ochunk = (olen > ylen) ? ylen : olen;
		uint32_t omagic = 0;

		if (st->magic_samples[channel_index]) {
			omagic = asc_resampler_magic(st, channel_index, &y, ochunk);
			ochunk -= omagic;
			olen -= omagic;
		}
		if (!st->magic_samples[channel_index]) {
			if (in) {
				for (j = 0; j<ichunk; ++j)
					x[j + st->filt_len - 1] = in[j*istride_save];
			}
			else {
				for (j = 0; j<ichunk; ++j)
					x[j + st->filt_len - 1] = 0;
			}

			asc_resampler_process_native(st, channel_index, &ichunk, y, &ochunk);
		}
		else {
			ichunk = 0;
			ochunk = 0;
		}

		for (j = 0; j<ochunk + omagic; ++j)
			out[j*ostride_save] = RS_FLTADJUST(ystack[j]);

		ilen -= ichunk;
		olen -= ochunk;
		out += (ochunk + omagic) * ostride_save;
		if (in)
			in += ichunk * istride_save;
	}
	st->out_stride = ostride_save;
	*in_len -= ilen;
	*out_len -= olen;

	return st->resampler_ptr == resampler_basic_zero ? ASC_RESAMPLER_ERR_ALLOC_FAILED : ASC_RESAMPLER_ERR_SUCCESS;
}

int asc_resampler_process_interleaved_float(AscResamplerState *st, const float *in, uint32_t *in_len, float *out, uint32_t *out_len)
{ ////
	uint32_t i;
	int istride_save, ostride_save;
	uint32_t bak_out_len = *out_len;
	uint32_t bak_in_len = *in_len;
	istride_save = st->in_stride;
	ostride_save = st->out_stride;
	st->in_stride = st->out_stride = st->nb_channels;
	for (i = 0; i<st->nb_channels; i++)
	{
		*out_len = bak_out_len;
		*in_len = bak_in_len;
		if (in != NULL)
			asc_resampler_process_float(st, i, in + i, in_len, out + i, out_len);
		else
			asc_resampler_process_float(st, i, NULL, in_len, out + i, out_len);
	}
	st->in_stride = istride_save;
	st->out_stride = ostride_save;
	return st->resampler_ptr == resampler_basic_zero ? ASC_RESAMPLER_ERR_ALLOC_FAILED : ASC_RESAMPLER_ERR_SUCCESS;
}

int asc_resampler_set_rate(AscResamplerState *st, uint32_t in_rate, uint32_t out_rate)
{
	return asc_resampler_set_rate_frac(st, in_rate, out_rate, in_rate, out_rate);
}

void asc_resampler_get_rate(AscResamplerState *st, uint32_t *in_rate, uint32_t *out_rate)
{
	*in_rate = st->in_rate;
	*out_rate = st->out_rate;
}

static uint32_t compute_gcd(uint32_t a, uint32_t b)
{ ////
	while (b != 0)
	{
		uint32_t temp = a;

		a = b;
		b = temp % b;
	}
	return a;
}

int asc_resampler_set_rate_frac(AscResamplerState *st, uint32_t ratio_num, uint32_t ratio_den, uint32_t in_rate, uint32_t out_rate)
{ ////
	uint32_t fact;
	uint32_t old_den;
	uint32_t i;

	if (ratio_num == 0 || ratio_den == 0)
		return ASC_RESAMPLER_ERR_INVALID_ARG;

	if (st->in_rate == in_rate && st->out_rate == out_rate && st->num_rate == ratio_num && st->den_rate == ratio_den)
		return ASC_RESAMPLER_ERR_SUCCESS;

	old_den = st->den_rate;
	st->in_rate = in_rate;
	st->out_rate = out_rate;
	st->num_rate = ratio_num;
	st->den_rate = ratio_den;

	fact = compute_gcd(st->num_rate, st->den_rate);

	st->num_rate /= fact;
	st->den_rate /= fact;

	if (old_den > 0)
	{
		for (i = 0; i<st->nb_channels; i++)
		{
			if (multiply_frac(&st->samp_frac_num[i], st->samp_frac_num[i], st->den_rate, old_den) != ASC_RESAMPLER_ERR_SUCCESS)
				return ASC_RESAMPLER_ERR_OVERFLOW;
			/* Safety net */
			if (st->samp_frac_num[i] >= st->den_rate)
				st->samp_frac_num[i] = st->den_rate - 1;
		}
	}

	if (st->initialised)
		return update_filter(st);
	return ASC_RESAMPLER_ERR_SUCCESS;
}

void asc_resampler_get_ratio(AscResamplerState *st, uint32_t *ratio_num, uint32_t *ratio_den)
{
	*ratio_num = st->num_rate;
	*ratio_den = st->den_rate;
}

int asc_resampler_set_quality(AscResamplerState *st, int quality)
{ ////
	if (quality > 10 || quality < 0)
		return ASC_RESAMPLER_ERR_INVALID_ARG;
	if (st->quality == quality)
		return ASC_RESAMPLER_ERR_SUCCESS;
	st->quality = quality;
	if (st->initialised)
		return update_filter(st);
	return ASC_RESAMPLER_ERR_SUCCESS;
}

void asc_resampler_get_quality(AscResamplerState *st, int *quality)
{
	*quality = st->quality;
}

void asc_resampler_set_input_stride(AscResamplerState *st, uint32_t stride)
{
	st->in_stride = stride;
}

void asc_resampler_get_input_stride(AscResamplerState *st, uint32_t *stride)
{
	*stride = st->in_stride;
}

void asc_resampler_set_output_stride(AscResamplerState *st, uint32_t stride)
{
	st->out_stride = stride;
}

void asc_resampler_get_output_stride(AscResamplerState *st, uint32_t *stride)
{
	*stride = st->out_stride;
}

int asc_resampler_get_input_latency(AscResamplerState *st)
{
	return st->filt_len / 2;
}

int asc_resampler_get_output_latency(AscResamplerState *st)
{ ///
	return ((st->filt_len / 2) * st->den_rate + (st->num_rate >> 1)) / st->num_rate;
}


/** Make sure that the first samples to go out of the resamplers don't have
* leading zeros. This is only useful before starting to use a newly created
* resampler. It is recommended to use that when resampling an audio file, as
* it will generate a file with the same length. For real-time processing,
* it is probably easier not to use this call (so that the output duration
* is the same for the first frame).
* @param st Resampler state
*/
int asc_resampler_skip_zeros(AscResamplerState *st)
{
	uint32_t i;
	for (i = 0; i<st->nb_channels; i++)
		st->last_sample[i] = st->filt_len / 2;
	return ASC_RESAMPLER_ERR_SUCCESS;
}

int asc_resampler_reset_mem(AscResamplerState *st)
{
	uint32_t i;
	for (i = 0; i<st->nb_channels; i++)
	{
		st->last_sample[i] = 0;
		st->magic_samples[i] = 0;
		st->samp_frac_num[i] = 0;
	}
	for (i = 0; i<st->nb_channels*(st->filt_len - 1); i++)
		st->mem[i] = 0;
	return ASC_RESAMPLER_ERR_SUCCESS;
}

const char *asc_resampler_strerror(int err)
{ ////
	switch (err)
	{
	case ASC_RESAMPLER_ERR_SUCCESS:
		return "Success.";
	case ASC_RESAMPLER_ERR_ALLOC_FAILED:
		return "Memory allocation failed.";
	case ASC_RESAMPLER_ERR_BAD_STATE:
		return "Bad resampler state.";
	case ASC_RESAMPLER_ERR_INVALID_ARG:
		return "Invalid argument.";
	case ASC_RESAMPLER_ERR_PTR_OVERLAP:
		return "Input and output buffers overlap.";
	default:
		return "Unknown error. Bad error code or strange version mismatch.";
	}
}