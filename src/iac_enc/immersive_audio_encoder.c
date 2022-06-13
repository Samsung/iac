#include "immersive_audio_encoder.h"
#include "immersive_audio_encoder_private.h"
#include "math.h"
#include "bitstreamrw.h"
#include "fixedp11_5.h"
#include "obuwrite.h"

static int default_dmix_index = 1;
static int default_w_index = 0;

union trans2char
{
  float f;
  unsigned char c[4];
};


static void get_dec_map(unsigned channels, int metamode, int *stream_count, int *coupled_stream_count, unsigned char *stream_map)
{
  int done = 0;
  switch (channels)
  {
  case 1:
    stream_map[0] = 0;
    done = 1;
    break;
  case 2:
    *stream_count = 1;
    *coupled_stream_count = 1;
    stream_map[0] = 0;
    stream_map[1] = 1;
    done = 1;
    break;
  case 3:
    *stream_count = 2;
    *coupled_stream_count = 1;
    stream_map[0] = 0; // L
    stream_map[1] = 2; // C
    stream_map[2] = 1; // R
    done = 1;
    break;
  case 4:
    switch (metamode)
    {
    case 0: // fL fR bL bR
    case 1: // sL sR bL bR
      *stream_count = 2;
      *coupled_stream_count = 2;
      stream_map[0] = 0;
      stream_map[1] = 1;
      stream_map[2] = 2;
      stream_map[3] = 3;
      done = 1;
      break;
    case 2: // fC LFE sL sR
      *stream_count = 3;
      *coupled_stream_count = 1;
      stream_map[0] = 2;
      stream_map[1] = 3;
      stream_map[2] = 0;
      stream_map[3] = 1;
      done = 1;
      break;
    }
    break;
  case 5:
    switch (metamode)
    {
    case 0: // fL fC fR bL bR
      *stream_count = 3;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 4; // fC
      stream_map[2] = 1; // fR
      stream_map[3] = 2; // bL
      stream_map[4] = 3; // bR
      done = 1;
      break;
    case 1: case 2:
      break;
    }
    break;
  case 6:
    switch (metamode)
    {
    case 0: // fL fC fR bL bR LFE
      *stream_count = 4;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 4; // fC
      stream_map[2] = 1; // fR
      stream_map[3] = 2; // bL
      stream_map[4] = 3; // bR
      stream_map[5] = 5; // LFE
      done = 1;
      break;
    case 1: // fC LFE sL sR bL bR
      *stream_count = 4;
      *coupled_stream_count = 2;
      stream_map[0] = 4; // fC
      stream_map[1] = 0; // sL
      stream_map[2] = 1; // sR
      stream_map[3] = 2; // bL
      stream_map[4] = 3; // bR
      stream_map[5] = 5; // LFE
      done = 1;
      break;
    case 2: // fL fR sL sR bL bR
      *stream_count = 3;
      *coupled_stream_count = 3;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 2; // sL
      stream_map[3] = 3; // sR
      stream_map[4] = 4; // bL
      stream_map[5] = 5; // bR
      done = 1;
      break;
    }
    break;
  case 7:
    switch (metamode)
    {
    case 0: // fL fR fC LFE bC sL sR
      *stream_count = 5;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 4; // fC
      stream_map[2] = 1; // fR
      stream_map[3] = 2; // sL
      stream_map[4] = 3; // sR
      stream_map[5] = 5; // bC
      stream_map[6] = 6; // LFE
      done = 1;
      break;
    case 1: case 2:
      break;
    }
    break;
  case 8:
    switch (metamode)
    {
    case 0: // fL fR fC LFE sL sR bL bR
      *stream_count = 5;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 6; // fC
      stream_map[2] = 1; // fR
      stream_map[3] = 2; // sL
      stream_map[4] = 3; // sR
      stream_map[5] = 4; // bL
      stream_map[6] = 5; // bR
      stream_map[7] = 7; // LFE
      break;
    case 1: case 2:
      break;
    }
    break;
  }
  if (done == 0)
  {
    *stream_count = channels;
    *coupled_stream_count = 0;
    for (int i = 0; i < channels; i++)
      stream_map[i] = 255;
  }
}

static void get_enc_map(unsigned channels, int metamode, int *stream_count, int *coupled_stream_count, unsigned char *stream_map)
{
  int done = 0;
  switch (channels)
  {
  case 1:
    stream_map[0] = 0;
    done = 1;
    break;
  case 2:
    *stream_count = 1;
    *coupled_stream_count = 1;
    stream_map[0] = 0;
    stream_map[1] = 1;
    done = 1;
    break;
  case 3:
    *stream_count = 2;
    *coupled_stream_count = 1;
    stream_map[0] = 0; // L
    stream_map[1] = 1; // R
    stream_map[2] = 2; // C
    done = 1;
    break;
  case 4:
    switch (metamode)
    {
    case 0: // fL fR bL bR
    case 1: // sL sR bL bR
      *stream_count = 2;
      *coupled_stream_count = 2;
      stream_map[0] = 0;
      stream_map[1] = 1;
      stream_map[2] = 2;
      stream_map[3] = 3;
      done = 1;
      break;
    case 2: // fC LFE sL sR
      *stream_count = 3;
      *coupled_stream_count = 1;
      stream_map[0] = 2;
      stream_map[1] = 3;
      stream_map[2] = 0;
      stream_map[3] = 1;
      done = 1;
      break;
    }
    break;
  case 5:
    switch (metamode)
    {
    case 0: // fL fR fC bL bR
      *stream_count = 3;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 3; // bL
      stream_map[3] = 4; // bR
      stream_map[4] = 2; // fC
      done = 1;
      break;
    case 1: case 2:
      break;
    }
    break;
  case 6:
    switch (metamode)
    {
    case 0: // fL fR fC LFE bL bR
      *stream_count = 4;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 4; // bL
      stream_map[3] = 5; // bR
      stream_map[4] = 2; // fC
      stream_map[5] = 3; // LFE
      done = 1;
      break;
    case 1: // fC LFE sL sR bL bR
      *stream_count = 4;
      *coupled_stream_count = 2;
      stream_map[0] = 2; // sL
      stream_map[1] = 3; // sR
      stream_map[2] = 4; // bL
      stream_map[3] = 5; // bR
      stream_map[4] = 0; // fC
      stream_map[5] = 1; // LFE
      done = 1;
      break;
    case 2: // fL fR sL sR bL bR
      *stream_count = 3;
      *coupled_stream_count = 3;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 2; // sL
      stream_map[3] = 3; // sR
      stream_map[4] = 4; // bL
      stream_map[5] = 5; // bR
      done = 1;
      break;
    }
    break;
  case 7:
    switch (metamode)
    {
    case 0: // fL fR fC LFE bC bL bR
      *stream_count = 5;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 5; // bL
      stream_map[3] = 6; // bR
      stream_map[4] = 2; // fC
      stream_map[5] = 4; // bC
      stream_map[6] = 3; // LFE
      done = 1;
      break;
    case 1: case 2:
      break;
    }
    break;
  case 8:
    switch (metamode)
    {
    case 0: // fL fR fC LFE sL sR bL bR
      *stream_count = 5;
      *coupled_stream_count = 2;
      stream_map[0] = 0; // fL
      stream_map[1] = 1; // fR
      stream_map[2] = 4; // sL
      stream_map[3] = 5; // sR
      stream_map[4] = 6; // bL
      stream_map[5] = 7; // bR
      stream_map[6] = 2; // fC
      stream_map[7] = 3; // LFE
      done = 1;
      break;
    case 1: case 2:
      break;
    }
    break;
  }

  if (done == 0)
  {
    *stream_count = channels;
    *coupled_stream_count = 0;
    for (int i = 0; i < channels; i++)
      stream_map[i] = i;
  }
}


static void reorder_channels(uint8_t *channel_map, int channels, unsigned pcm_frames, int16_t *samples)
{
  int16_t t[MAX_CHANNELS];
  int map_ch;

  for (int i = 0; i < pcm_frames; i++)
  {
    for (int ch = 0; ch < channels; ch++)
    {
      map_ch = channel_map[ch];
      t[ch] = samples[i*channels + map_ch];
    }
    for (int ch = 0; ch < channels; ch++)
    {
      samples[i*channels + ch] = t[ch];
    }
  }
}

LoudGainMeasure * immersive_audio_encoder_loudgain_create(const unsigned char *channel_layout_map, int sample_rate, int frame_size)
{
  LoudGainMeasure *lm = (LoudGainMeasure *)malloc(sizeof(LoudGainMeasure));
  if(!lm)return NULL;
  memset(lm, 0x00, sizeof(LoudGainMeasure));
  memcpy(lm->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_DMAX);
  lm->frame_size = frame_size;
  int channel_loudness[CHANNEL_LAYOUT_MAX] = { 1, 2, 6, 8, 10, 8, 10, 12, 6 };
  channelLayout channellayout[CHANNEL_LAYOUT_MAX] = { CHANNELMONO, CHANNELSTEREO ,CHANNEL51 ,CHANNEL512 ,CHANNELUNKNOWN, CHANNEL71, CHANNELUNKNOWN, CHANNEL714 , CHANNEL312 ,};///////TODO change if channels are changed.
  //int channel_loudness[MAX_CHANNELS] = { 2,6,8,12, }; ///////TODO change if channels are changed.
  //channelLayout channellayout[MAX_CHANNELS] = { CHANNELSTEREO ,CHANNEL312 ,CHANNEL512 ,CHANNEL714 , };///////TODO change if channels are changed.
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = lm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    AudioLoudMeterInit(&(lm->loudmeter[layout]));
    lm->loudmeter[layout].initParams(&(lm->loudmeter[layout]), 0.4f, 0.75f, 3.0f);
    lm->loudmeter[layout].prepare(&(lm->loudmeter[layout]), sample_rate, channel_loudness[layout], channellayout[layout]);
    lm->loudmeter[layout].startIntegrated(&(lm->loudmeter[layout]));

    lm->entire_peaksqr_gain[layout] = 0.0;
    lm->entire_truepeaksqr_gain[layout] = 0.0;
  }
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    audio_true_peak_meter_init(&(lm->peakmeter[i]));
    audio_true_peak_meter_reset_states(&(lm->peakmeter[i]));
  }
  lm->msize25pct = sample_rate / 10 / frame_size;

  lm->measure_end = 0;
  return lm;
}

int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout)
{
  int ret = 0;
  resultData_s result;
  result = lm->loudmeter[channel_layout].processFrameLoudness(&(lm->loudmeter[channel_layout]), inbuffer, lm->msize25pct, lm->frame_size);
  return 0;
}

int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int begin_ch, int nch)
{
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < lm->frame_size; fr++)
  {
    for (int ch = begin_ch; ch < begin_ch + nch; ch++)
    {
      dsig = inbuffer[ch*lm->frame_size + fr];
      if (lm->entire_peaksqr_gain[channel_layout] < dsig*dsig) ///////TODO
      {
        lm->entire_peaksqr_gain[channel_layout] = dsig*dsig;
      }

      tsig = audio_true_peak_meter_next_true_peak(&(lm->peakmeter[ch]), dsig);
      if (lm->entire_truepeaksqr_gain[channel_layout] < tsig*tsig)
      {
        lm->entire_truepeaksqr_gain[channel_layout] = tsig*tsig;
      }
    }
  }
  lm->gaindown_flag[channel_layout] = 1;
  return 0;
}

int immersive_audio_encoder_gain_measure2(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int ch, int index)
{
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < lm->frame_size; fr++)
  {
    dsig = inbuffer[ch*lm->frame_size + fr];
    if (lm->entire_peaksqr_gain[channel_layout] < dsig*dsig) ///////TODO
    {
      lm->entire_peaksqr_gain[channel_layout] = dsig*dsig;
    }

    tsig = audio_true_peak_meter_next_true_peak(&(lm->peakmeter[index]), dsig);
    if (lm->entire_truepeaksqr_gain[channel_layout] < tsig*tsig)
    {
      lm->entire_truepeaksqr_gain[channel_layout] = tsig*tsig;
    }
  }
  lm->gaindown_flag[channel_layout] = 1;
  return 0;
}

int immersive_audio_encoder_loudgain_destory(LoudGainMeasure *lm)
{
  if (!lm)
    return -1;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = lm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    AudioLoudMeterDeinit(&(lm->loudmeter[layout]));
  }
  for (int i = 0; i < MAX_CHANNELS; i++)
  {
    audio_true_peak_meter_deinit(&(lm->peakmeter[i]));
  }

  free(lm);
  return 0;
}


void conv_writtenpcm(float *pcmbuf, void *wavbuf, int nch, int frame_size)
{
  int16_t *wbuf = (int16_t *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < frame_size; j++)
    {
      wbuf[i + j*nch] = (int16_t)(pcmbuf[i * frame_size + j] * 32767.0);
    }
  }
}

void conv_writtenpcm1(float *pcmbuf, void *wavbuf, int nch, int frame_size)
{
  int16_t *wbuf = (int16_t *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < frame_size; j++)
    {
      wbuf[i + j*nch] = (int16_t)(pcmbuf[i + j*nch] * 32767.0);
    }
  }
}
void conv_writtenfloat(float *pcmbuf, void *wavbuf, int nch, int frame_size)
{
  unsigned char *wbuf = (unsigned char *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < frame_size; j++)
    {
      union trans2char trans;
      trans.f = pcmbuf[i*frame_size + j];
      wbuf[(i + j*nch) * 4] = trans.c[0];
      wbuf[(i + j*nch) * 4 + 1] = trans.c[1];
      wbuf[(i + j*nch) * 4 + 2] = trans.c[2];
      wbuf[(i + j*nch) * 4 + 3] = trans.c[3];
    }
  }
}

static uint16_t bswapu16(const uint16_t u16)
{
  return (u16 << 8) | (u16 >> 8);
}

int write_recon_gain(IAEncoder *st, unsigned char* buffer, int type) //0 common, 1 base, 2 advance, ret write size
{
#undef MHDR_LEN
#define MHDR_LEN 255
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char bitstr[MHDR_LEN] = { 0, };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  unsigned char coded_data_leb[10];
  int coded_size = 0;
 
  int layout = st->channel_layout_map[type];
  if (st->recon_gain_flag)
  {
    uint16_t recon_gain_flag = 0;
    int max_recon_gain_fields = 12;
#if 0
    int max_recon_gain_fields_msb = 14;
    for (int i = 0; i < max_recon_gain_fields_msb; i++)
    {
      // scalable_map is based on wav channel order.
      // get_recon_gain_flags_map convert wav channel order to vorbis channel order.
      /*
      b1(LSB) b2      b3      b4      b5      b6      b7      b8      b9      b10      b11      b12(MSB)
      L	      C       R	      Ls(Lss) Rs(Rss)	Ltf	    Rtf     Lb(Lrs)	Rb(Rrs)	Ltb(Ltr) Rtb(Rtr) LFE
      */
      int channel = get_recon_gain_flags_map_msb[layout][i];
      if (channel >= 0 && st->upmixer->scalable_map[layout][channel] == 1)
      {
        recon_gain_flag = recon_gain_flag | (0x01 << i);
      }
    }
#else
    for (int i = 0; i < max_recon_gain_fields; i++)
    {
      // scalable_map is based on wav channel order.
      // get_recon_gain_flags_map convert wav channel order to vorbis channel order.
      /*
      b1(LSB) b2      b3      b4      b5      b6      b7      b8      b9      b10      b11      b12(MSB)
      L	      C       R	      Ls(Lss) Rs(Rss)	Ltf	    Rtf     Lb(Lrs)	Rb(Rrs)	Ltb(Ltr) Rtb(Rtr) LFE
      */
      int channel = get_recon_gain_flags_map[layout][i];
      if (channel >= 0 && st->upmixer->scalable_map[layout][channel] == 1)
      {
        recon_gain_flag = recon_gain_flag | (0x01 << i);
      }
    }
#endif
    if (uleb_encode(recon_gain_flag, sizeof(recon_gain_flag), coded_data_leb,
      &coded_size) != 0) { // Channel_Group_Size (leb128())
      return 0;
    }
    for (int i = 0; i < coded_size; i++)
    {
      bs_setbits(&bs, coded_data_leb[i], 8);
    }
    //printf(" scalablefactor: \n");
    for (int i = 0; i < max_recon_gain_fields; i++)
    {
      // channel range is 0 ~ nch for st->mdhr.scalablefactor[layout],
      int channel_index = get_recon_gain_value_map[layout][i];
      int channel = get_recon_gain_flags_map[layout][i];

      if (channel_index >= 0 && channel > 0 && st->upmixer->scalable_map[layout][channel] == 1)
      {
        bs_setbits(&bs, st->mdhr.scalablefactor[layout][channel_index], 8);
        //printf("[%d]:%d ", channel_index,st->mdhr.scalablefactor[layout][channel_index]);
      }
    }
    //printf("\n");
  }
  memcpy(buffer, bitstr, bs.m_posBase);
  return bs.m_posBase;
}

int immersive_audio_encoder_ctl_va_list(IAEncoder *et, int request,
  va_list ap)
{
  int ret = IA_OK;
  switch (request)
  {
  case IA_SET_RECON_GAIN_FLAG_REQUEST:
  {
    uint32_t recon_gain_flag;
    recon_gain_flag = va_arg(ap, uint32_t);
    printf("\nrecon_gain_flag: %d\n", recon_gain_flag);
    if (recon_gain_flag < 0)
    {
      goto bad_arg;
    }
    et->recon_gain_flag = recon_gain_flag;
    et->upmixer->recon_gain_flag = recon_gain_flag;
    et->decode_init(et);
  }
  break;
  case IA_SET_SCALE_FACTOR_MODE_REQUEST:
  {
    uint32_t scalefactor_mode;
    scalefactor_mode = va_arg(ap, uint32_t);
    if (scalefactor_mode < 0)
    {
      goto bad_arg;
    }
    et->scalefactor_mode = scalefactor_mode;
    printf("scalefactor_mode: %d\n", et->scalefactor_mode);
  }
  break;
  case IA_SET_OUTPUT_GAIN_FLAG_REQUEST:
  {
    uint32_t output_gain_flag;
    output_gain_flag = va_arg(ap, uint32_t);
    if (output_gain_flag < 0)
    {
      goto bad_arg;
    }
    et->output_gain_flag = output_gain_flag;
    printf("output_gain_flag: %d\n", et->output_gain_flag);
  }
  break;
  case IA_SET_TEMP_DOWNMIX_FILE:
  {
    char * temp_f;
    temp_f = va_arg(ap, char*);
    printf("temp file name: %s\n", temp_f);
    sprintf(et->dmix_fn, "%s_dmix.txt", temp_f);
    sprintf(et->weight_fn, "%s_w.txt", temp_f);
  }
  break;
  case IA_SET_SUBSTREAM_SIZE_FLAG_REQUEST:
  {
    uint32_t substream_size_flag;
    substream_size_flag = va_arg(ap, uint32_t);
    printf("substream_size_flag: %d\n", substream_size_flag);
    if (substream_size_flag < 0)
    {
      goto bad_arg;
    }
    et->substream_size_flag = substream_size_flag;
  }
  break;
  case IA_SET_BITRATE_REQUEST:
  {
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    int32_t value = va_arg(ap, int32_t);
    int pre_ch = 0;
#if 0
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[i].dep_encoder;
      int bitrate = value * (channel_map714[layout] - pre_ch);
      opus_multistream_encoder_ctl(dep_encoder, IA_SET_BITRATE(bitrate));
      pre_ch = channel_map714[layout];
    }
#endif
  }
  break;
  case IA_SET_BANDWIDTH_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
#if 0
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[i].dep_encoder;
      opus_multistream_encoder_ctl(dep_encoder, IA_SET_BANDWIDTH(value));
    }
#endif
  }
  break;
  case IA_SET_VBR_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
#if 0
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[i].dep_encoder;
      opus_multistream_encoder_ctl(dep_encoder, IA_SET_VBR(value));
    }
#endif
  }
  break;
  case IA_SET_COMPLEXITY_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
#if 0
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[i].dep_encoder;
      opus_multistream_encoder_ctl(dep_encoder, IA_SET_COMPLEXITY(value));
    }
#endif
  }
  break;
  case IA_GET_LOOKAHEAD_REQUEST:
  {
    int32_t *value = va_arg(ap, int32_t*);
#if 0
    OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[0].dep_encoder;
    opus_multistream_encoder_ctl(dep_encoder, IA_GET_LOOKAHEAD(value));
#endif
  }
  break;
  case IA_SET_FORCE_MODE_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
#if 0
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *dep_encoder = et->ia_encoder_dcg[i].dep_encoder;
      opus_multistream_encoder_ctl(dep_encoder, IA_SET_FORCE_MODE(IA_MODE_CELT_ONLY));
    }
#endif
  }
  break;
  default:
    ret = IA_UNIMPLEMENTED;
    break;
  }
  return ret;
bad_arg:
  return IA_BAD_ARG;
}

int immersive_audio_encoder_ctl(IAEncoder *et, int request, ...)
{
  int ret;
  va_list ap;
  va_start(ap, request);
  ret = immersive_audio_encoder_ctl_va_list(et, request, ap);
  va_end(ap);
  return ret;
}

static int get_scalable_format(IAEncoder *st, int channel_layout_in, const unsigned char *channel_layout_cb)
{
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char channel_layout_map[CHANNEL_LAYOUT_MAX] = { CHANNEL_LAYOUT_MAX , };
  int channel_groups = 0;
  for (channel_groups = 0; channel_groups < CHANNEL_LAYOUT_MAX; channel_groups++)
  {
    channel_layout_map[channel_groups] = channel_layout_cb[channel_groups];
    if (channel_layout_cb[channel_groups] == CHANNEL_LAYOUT_MAX)
      break;
  }
  channel_layout_map[channel_groups] = channel_layout_in;
  channel_groups++;
  channel_layout_map[channel_groups] = CHANNEL_LAYOUT_MAX;

  int last_s_channels = 0, last_h_channels = 0;

  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    if (get_surround_channels(layout) < last_s_channels || get_height_channels(layout) < last_h_channels
      || (get_surround_channels(layout) == last_s_channels && get_height_channels(layout) == last_h_channels))
    {
      printf("The combination is illegal!!!, please confirm the rules:\n");
      printf("Adjacent channel layouts of a scalable format(where CLn1 is the precedent channel layout and CLn is the next one)\n");
      printf("are only allowed as below, where CLn = S(n).W(n).H(n)\n");
      printf(">>>> S(n-1) <= S(n) and W(n 1) <= W(n) and H(n 1) <= H(n) except: S(n-1) = S(n) and W(n-1) = W(n) and H(n-1) = H(n) \n");
      printf("         NOTE: S(Surround Channel), W(Subwoofer Channel), H(Height Channel)\n");
      return 0;
    }
    last_s_channels = get_surround_channels(layout);
    last_h_channels = get_height_channels(layout);
  }

#if 1
  int idx = 0, ret = 0;;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];
  printf("\nTransmission Channels Order: \n");
  printf("---\n");
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    ret = enc_get_new_channels2(last_cl_layout, layout, new_channels);
    int channel_c = enc_has_c_channel(ret, new_channels);
////print new channels
    if (ret > 0) {
      for (int ch = 0; ch<ret; ++ch) {
        printf("%s\n", enc_get_channel_name(new_channels[ch]));
      }
    }
/////////////////////
    for (int j = 0; j < ret; j++)
    {
      st->ia_encoder_dcg[i].enc_stream_map[j] = j;
      st->ia_decoder_dcg[i].dec_stream_map[j] = j;
    }
    if (channel_c >= 0)
    {
      if (last_cl_layout == CHANNEL_LAYOUT_D100)
      {
        st->ia_encoder_dcg[i].stream_count = (ret - 2) / 2 + 2 + 1;
      }
      else
        st->ia_encoder_dcg[i].stream_count = (ret -2) / 2 + 2;
      st->ia_encoder_dcg[i].coupled_stream_count = (ret - 2) / 2;
      st->ia_encoder_dcg[i].channel = ret;

      st->ia_decoder_dcg[i].stream_count = st->ia_encoder_dcg[i].stream_count;
      st->ia_decoder_dcg[i].coupled_stream_count = st->ia_encoder_dcg[i].coupled_stream_count;
      st->ia_decoder_dcg[i].channel = st->ia_encoder_dcg[i].channel;
    }
    else
    {
      if (ret == 1)
      {
        st->ia_encoder_dcg[i].stream_count = 1;
        st->ia_encoder_dcg[i].coupled_stream_count = 0;
        st->ia_encoder_dcg[i].channel = 1;
      }
      else
      {
        st->ia_encoder_dcg[i].stream_count = ret / 2;
        st->ia_encoder_dcg[i].coupled_stream_count = ret / 2;
        st->ia_encoder_dcg[i].channel = ret;
      }

      st->ia_decoder_dcg[i].stream_count = st->ia_encoder_dcg[i].stream_count;
      st->ia_decoder_dcg[i].coupled_stream_count = st->ia_encoder_dcg[i].coupled_stream_count;
      st->ia_decoder_dcg[i].channel = st->ia_encoder_dcg[i].channel;
    }
    printf("---\n");
    idx += ret;

    last_cl_layout = layout;
  }
#endif

  memcpy(st->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_MAX);
  return channel_groups;
}

//
int immersive_audio_encoder_dmpd_start(IAEncoder *st)
{
  printf("\nDownMix Parameter Determination start...\n");
  int channel_layout_in = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    channel_layout_in = lay_out;
  }
  st->asc = ia_asc_start(channel_layout_in);
  st->heq = ia_heq_start(channel_layout_in, st->input_sample_rate);
  return 0;
}

int immersive_audio_encoder_dmpd_process(IAEncoder *st, const int16_t *pcm, int frame_size)
{
  ia_asc_process(st->asc, pcm, frame_size);
  ia_heq_process(st->heq, pcm, frame_size);
  return 0;
}

int immersive_audio_encoder_dmpd_stop(IAEncoder *st)
{
  printf("DownMix Parameter Determination stop!!!\n\n");
  ia_asc_stop(st->asc);
  ia_heq_stop(st->heq);
  st->asc = NULL;
  st->heq = NULL;
  return 0;
}
//

extern encode_creator_t dep_encoders[];
extern decode_creator_t dep_decoders[];

static const char* dep_codec_name[] = {
  "opus", "aac"};

IAEncoder *immersive_audio_encoder_create(int32_t Fs,
  int channel_layout_in,
  const unsigned char *channel_layout_cb,
  int codec_id,  //0:opus, 1:aac
  int *error)
{
  IAEncoder *st = (IAEncoder*)malloc(sizeof(IAEncoder));
  if(!st)return NULL;
  memset(st, 0x00, sizeof(IAEncoder));

  st->input_sample_rate = Fs;
  int channel_groups = 1;
  channel_groups = get_scalable_format(st, channel_layout_in, channel_layout_cb);
  st->channel_groups = channel_groups;
  if(channel_groups == 0)
    exit(-1);

//////////////////////////////////////////////////////////////
  //dep codec select.
  for (int i = 0;; i++)
  {
    if (dep_encoders[i].opcode == codec_id || dep_encoders[i].opcode == -1)
    {
      st->encode_init = dep_encoders[i].init;
      st->encode_frame = dep_encoders[i].encode;
      st->encode_close = dep_encoders[i].close;
      break;
    }
  }

  for (int i = 0;; i++)
  {
    if (dep_decoders[i].opcode == codec_id || dep_decoders[i].opcode == -1)
    {
      st->decode_init = dep_decoders[i].init;
      st->decode_frame = dep_decoders[i].decode;
      st->decode_close = dep_decoders[i].close;
      break;
    }
  }

  if (st->encode_init == NULL || st->decode_init == NULL)
  {
    printf("Codec:%d is not supported\n", codec_id);
    free(st);
    return NULL;
  }
//////////////////////////////////////////////////////////////


  st->encode_init(st);
  printf("\nDep Codec: %s\n", dep_codec_name[codec_id]);

  st->recon_gain_flag = 0;
  st->scalefactor_mode = 2;

  int frame_size = 960, preskip_size = 312;//opus
  if (codec_id == IA_DEP_CODEC_AAC)
  {
    frame_size = 1024;
    preskip_size = 720;
  }
  st->codec_id = codec_id;
  //int frame_size = 1024 //aac
  st->frame_size = frame_size;
  st->preskip_size = preskip_size;
  st->downmixer = downmix_create(st->channel_layout_map, frame_size);
  st->loudgain = immersive_audio_encoder_loudgain_create(st->channel_layout_map, Fs, frame_size);
  st->mdhr.dialog_onoff = 1;
  st->mdhr.dialog_level = 0;
  st->mdhr.drc_profile = 0;
  st->mdhr.len_of_4chauxstrm = 0;
  st->mdhr.lfe_onoff = 1;
  st->mdhr.lfe_gain = 0;
  st->mdhr.len_of_6chauxstrm = 0;
  st->mdhr.dmix_matrix_type = 1;
  st->mdhr.weight_type = 1;
  st->mdhr.major_version = 1;
  st->mdhr.minor_version = 1;
  st->mdhr.coding_type = 0;
  st->mdhr.nsamples_of_frame = 0;

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    st->mdhr.LKFSch[i] = 1;
    st->mdhr.dmixgain[i] = 256;
    st->mdhr.chsilence[i] = 0xFFFFFFFF;
    for (int j = 0; j < 12;j++)
      st->mdhr.scalablefactor[i][j] = 0xFF;
  }

  st->upmixer = upmix_create(0, st->channel_layout_map, frame_size, preskip_size);
  st->upmixer->mdhr_l = st->mdhr;
  st->upmixer->mdhr_c = st->mdhr;

  scalablefactor_init();
  st->sf = scalablefactor_create(st->channel_layout_map, frame_size);

  st->fp_dmix = NULL;
  st->fp_weight = NULL;
  sprintf(st->dmix_fn, "%s_dmix.txt", "audio");
  sprintf(st->weight_fn, "%s_w.txt", "audio");
  memset(&(st->fc), 0x00, sizeof(st->fc));

  if (channel_groups == 1)
  {
    int layout = st->channel_layout_map[0];
    uint8_t *tchs = NULL;
    int nch = enc_get_layout_channel_count(layout);
    tchs = enc_get_layout_channels(layout);
    for (int i = 0; i < nch; i++)
    {
      for (int j = 0; j < nch; j++)
      {
        if (st->downmixer->channel_order[i] == tchs[j])
        {
          st->ia_encoder_dcg[0].enc_stream_map[i] = j;
          break;
        }
      }
    }
  }
  return st;
}


int immersive_audio_encoder_loudness_gain_start(IAEncoder *st)
{
  st->fp_dmix = fopen(st->dmix_fn, "r");
  st->fp_weight = fopen(st->weight_fn, "r");

  if (st->fp_dmix == NULL)
  {
    printf("no *_dmix.txt, use default value:1\n");
  }
  if (st->fp_weight == NULL)
  {
    printf("no *_w.txt, use default value:0\n");
  }

  ia_intermediate_file_writeopen(st, FILE_DOWNMIX_M, "ALL");
  ia_intermediate_file_writeopen(st, FILE_DOWNMIX_S, "ALL");
  return 0;
}

int immersive_audio_encoder_loudness_gain(IAEncoder *st, const int16_t *pcm, int frame_size)
{
  /////////////////////////////////////USED FOR DEBUG
  //downmix parameter getting, which will be removed in future 
  int dmix_index = default_dmix_index, w_index = default_w_index;
  if (st->fp_dmix)
    fscanf(st->fp_dmix, "%d", &dmix_index);
  if (st->fp_weight)
    fscanf(st->fp_weight, "%d", &w_index);
  /////////////////////////////////////////////////
  //printf("dmix_index %d , w_index %d \n", dmix_index, w_index);

  int16_t temp[IA_FRAME_MAXSIZE * 12 * 2];
  downmix2(st->downmixer, pcm, dmix_index, w_index);

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    immersive_audio_encoder_loudness_measure(st->loudgain, st->downmixer->downmix_m[lay_out], lay_out);
    conv_writtenfloat(st->downmixer->downmix_m[lay_out], temp, channel_map714[lay_out], st->frame_size);
    ia_intermediate_file_write(st, FILE_DOWNMIX_M, downmix_m_wav[lay_out], temp, st->frame_size);

    conv_writtenfloat(st->downmixer->downmix_s[lay_out], temp, channel_map714[lay_out] - pre_ch, st->frame_size);
    ia_intermediate_file_write(st, FILE_DOWNMIX_S, downmix_s_wav[lay_out], temp, st->frame_size);
    pre_ch = channel_map714[lay_out];
  }

  pre_ch = 0;
  int cl_index = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    for (int j = 0; j < channel_map714[lay_out] - pre_ch; j++)
    {
      int cl = st->downmixer->channel_order[cl_index];
      if (st->downmixer->gaindown_map[lay_out][cl] ||
        (lay_out != CHANNEL_LAYOUT_200 && cl == enc_channel_l2)) // Mono cases
      {
        st->gaindown_map[cl_index] = 1;
        immersive_audio_encoder_gain_measure2(st->loudgain, st->downmixer->downmix_s[lay_out], lay_out, j, cl_index);
      }
      cl_index++;
    }
    pre_ch = channel_map714[lay_out];
  }


  return 0;
}

int immersive_audio_encoder_loudness_gain_stop(IAEncoder *st)
{
  int ret = 0;
  LoudGainMeasure *lm = st->loudgain;
  if (lm->measure_end)
    return ret;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = lm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    lm->loudmeter[layout].stopIntegrated(&(lm->loudmeter[layout]));
    lm->loudmeter[layout].processMomentaryLoudness(&(lm->loudmeter[layout]), lm->msize25pct);

    lm->entire_loudness[layout] = lm->loudmeter[layout].getIntegratedLoudness(&(lm->loudmeter[layout]));
    lm->entire_peaksqr[layout] = lm->loudmeter[layout].getEntirePeakSquare(&(lm->loudmeter[layout]));
    lm->entire_truepeaksqr[layout] = lm->loudmeter[layout].getEntireTruePeakSquare(&(lm->loudmeter[layout]));
  }
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = lm->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    lm->dmixgain_lin[layout] = db2lin(-1.0) / sqrt(lm->entire_truepeaksqr_gain[layout]);
    if (lm->dmixgain_lin[layout] > 1) lm->dmixgain_lin[layout] = 1;
    lm->dmixgain_lin[layout] = lin2db(lm->dmixgain_lin[layout]);
  }
  lm->measure_end = 1;

  const char* channel_layout_names[] = {
    "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4", "3.1.2"  };
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int lay_out = lm->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_DMAX)
      break;
    printf("[%s]entireLoudness: %f LKFS\n", channel_layout_names[lay_out], st->loudgain->entire_loudness[lay_out]);
    st->mdhr.LKFSch[lay_out] = float_to_q(st->loudgain->entire_loudness[lay_out], 8);
  }

  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int lay_out = lm->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_DMAX)
      break;
    if (lm->gaindown_flag[lay_out] == 0)
      continue;
    printf("[%s]dmixgain: %f dB\n", channel_layout_names[lay_out], st->loudgain->dmixgain_lin[lay_out]);
    st->mdhr.dmixgain[lay_out] = float_to_q2(db2lin(st->loudgain->dmixgain_lin[lay_out]), 8);
  }

  ia_intermediate_file_writeclose(st, FILE_DOWNMIX_M, "ALL");
  ia_intermediate_file_writeclose(st, FILE_DOWNMIX_S, "ALL");
  if (st->fp_dmix)
  {
    fclose(st->fp_dmix);
    st->fp_dmix = NULL;
  }
  if (st->fp_weight)
  {
    fclose(st->fp_weight);
    st->fp_weight = NULL;
  }
  return ret; 
}
IA_STATIC_METADATA get_immersive_audio_encoder_ia_static_metadata(IAEncoder *st)
{
  IA_STATIC_METADATA ret;
  memset(&ret, 0x00, sizeof(ret));
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  ret.ambisonics_mode = 0;
  int max_output_gain_flags_fields = 6;
  int pre_ch = 0;
  int cl_index = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    ret.channel_audio_layer++;
    ret.channel_audio_layer_config[i].loudspeaker_layout = layout;
    ret.channel_audio_layer_config[i].output_gain_is_present_flag = st->output_gain_flag;
    ret.channel_audio_layer_config[i].recon_gain_is_present_flag = st->recon_gain_flag;
    ret.channel_audio_layer_config[i].substream_count = st->ia_encoder_dcg[i].stream_count;
    ret.channel_audio_layer_config[i].coupled_substream_count = st->ia_encoder_dcg[i].coupled_stream_count;
    ret.channel_audio_layer_config[i].loudness = st->mdhr.LKFSch[layout];

/*
Bit position : Channel Name
b5(MSB)  : Left channel (L1, L2, L3)
b4     : Right channel (R2, R3)
b3     : Left Surround channel (Ls5)
b2     : Right Surround channel (Rs5)
b1     : Left Top Front channel (Ltf)
b0     : Rigth Top Front channel (Rtf)
*/
    uint16_t output_gain_flags = 0;
    for (int j = 0; j < channel_map714[layout] - pre_ch; j++)
    {
      int cl = st->downmixer->channel_order[cl_index];
      if (st->gaindown_map[cl_index] == 1) 
      {
        int shift = get_output_gain_flags_map[cl];
        if (shift >= 0)
        {
          output_gain_flags = output_gain_flags | (0x01 << shift);
        }
      }
      cl_index++;
    }
    pre_ch = channel_map714[layout];

    ret.channel_audio_layer_config[i].output_gain_flags = output_gain_flags;
    ret.channel_audio_layer_config[i].output_gain = st->mdhr.dmixgain[layout];
  }
  return ret;
}


int immersive_audio_encoder_gaindown(IAEncoder *st)//
{
  if (st->recon_gain_flag == 0)
  {
    /////////////USED FOR DEBUG
    st->fp_dmix = fopen(st->dmix_fn, "r");
    st->fp_weight = fopen(st->weight_fn, "r");
    if (st->fp_dmix == NULL)
    {
      printf("no *_dmix.txt, use default value:1\n");
    }
    if (st->fp_weight == NULL)
    {
      printf("no *_w.txt, use default value:0\n");
    }
    downmix_clear(st->downmixer);

    return 0;
  }
  int ret = 0;
  ia_intermediate_file_readopen(st, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeopen(st, FILE_GAIN_DOWN, "ALL");

  float *downmix_s[CHANNEL_LAYOUT_MAX]; //common method, don't rely on encode/decode,just gain down
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    downmix_s[layout] = (float *)malloc(st->frame_size * MAX_CHANNELS * sizeof(float));
    if(!downmix_s[layout])
    {
      ret = -1;
      goto FAILED;
    }
    memset(downmix_s[layout], 0x00, st->frame_size * MAX_CHANNELS * sizeof(float));
  }

  int pcm_data_s;
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;

  int16_t gain_down_out[IA_FRAME_MAXSIZE * MAX_CHANNELS];

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;

    pcm_data_s = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], downmix_s[layout], st->frame_size);
  }


  while (pcm_data_s)
  {
    gaindown2(downmix_s, st->channel_layout_map, st->gaindown_map, st->mdhr.dmixgain, st->frame_size);

    unsigned char pre_ch = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      conv_writtenpcm1(downmix_s[layout], gain_down_out, channel_map714[layout] - pre_ch, st->frame_size);
      ia_intermediate_file_write(st, FILE_GAIN_DOWN, gaindown_wav[layout], gain_down_out, st->frame_size);
      pre_ch = channel_map714[layout];
    }


    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;

      pcm_data_s = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], downmix_s[layout], st->frame_size);
    }
  }

  ia_intermediate_file_readclose(st, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeclose(st, FILE_GAIN_DOWN, "ALL");

FAILED:
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    if (downmix_s[layout])
      free(downmix_s[layout]);
  }

  return ret;

}

static int extract_pcm_from_group(int16_t *input, int16_t * out, int nch, int ith, int single, int frame_size)
{
  int channle_index = 0;
  if (single)
  {
    for (int i = 0; i < frame_size; i++)
    {
      out[i] = input[i*nch + ith];
    }
    channle_index = ith + 1;
  }
  else //couple
  {
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < frame_size; j++)
      {
        out[j * 2 + i] = input[j*nch + ith + i];
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;

}

static int insert_pcm_to_group(int16_t *input, int16_t * out, int nch, int ith, int single, int frame_size)
{
  int channle_index = 0;
  if (single)
  {
    for (int i = 0; i < frame_size; i++)
    {
      out[i*nch + ith] = input[i];
    }
    channle_index = ith + 1;
  }
  else
  {
    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < frame_size; j++)
      {
        out[j*nch + ith + i] = input[j * 2 + i];
      }
    }
    channle_index = ith + 2;
  }
  return channle_index;
}

void extension_encode_priv(IAEncoder *st)
{

  ia_intermediate_file_readopen(st, FILE_GAIN_DOWN, "ALL");
  ia_intermediate_file_writeopen(st, FILE_DECODED, "ALL");
  ia_intermediate_file_writeopen(st, FILE_ENCODED, "ALL");

  int16_t gain_down_in[IA_FRAME_MAXSIZE * MAX_CHANNELS];
  int pcm_data;
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char encoded_frame[MAX_PACKET_SIZE] = { 0, };
  int16_t decoded_frame[MAX_PACKET_SIZE];
  int presize[CHANNEL_LAYOUT_MAX];
  
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    presize[i] = st->preskip_size;
  }
  int16_t extract_pcm[IA_FRAME_MAXSIZE * 2];
  int16_t extract_pcm_dec[IA_FRAME_MAXSIZE * 2];
#if 1
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, st->frame_size);
    reorder_channels(st->ia_encoder_dcg[i].enc_stream_map, st->ia_encoder_dcg[i].channel, st->frame_size, gain_down_in);

    for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
    {
      if (j < st->ia_encoder_dcg[i].coupled_stream_count)
      {
        extract_pcm_from_group(gain_down_in, extract_pcm, st->ia_encoder_dcg[i].channel, j * 2, 0, st->frame_size);
        int32_t encoded_size = st->encode_frame(st, i, j, 2, extract_pcm, encoded_frame);
        ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
        int ret = st->decode_frame(st, i, j, 2, encoded_frame, encoded_size, extract_pcm_dec);
        insert_pcm_to_group(extract_pcm_dec, decoded_frame, st->ia_encoder_dcg[i].channel, j * 2, 0, st->frame_size);
      }
      else
      {
        extract_pcm_from_group(gain_down_in, extract_pcm, st->ia_encoder_dcg[i].channel, st->ia_encoder_dcg[i].coupled_stream_count + j, 1 , st->frame_size);
        int32_t encoded_size = st->encode_frame(st, i, j, 1, extract_pcm, encoded_frame);
        ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
        int ret = st->decode_frame(st, i, j, 1, encoded_frame, encoded_size, extract_pcm_dec);
        insert_pcm_to_group(extract_pcm_dec, decoded_frame, st->ia_encoder_dcg[i].channel, st->ia_encoder_dcg[i].coupled_stream_count + j, 1, st->frame_size);
      }
    }

    reorder_channels(st->ia_decoder_dcg[i].dec_stream_map, st->ia_decoder_dcg[i].channel, st->frame_size, (int16_t*)decoded_frame);

    if (presize[lay_out] > 0)
    {
      memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
      presize[lay_out] = 0;
    }
    ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, st->frame_size);
  }
#else
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, st->frame_size);
    reorder_channels(st->ia_encoder_dcg[i].enc_stream_map, st->ia_encoder_dcg[i].channel, FRAME_SIZE, gain_down_in);
    int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[i].dep_encoder,
      gain_down_in,
      FRAME_SIZE,
      encoded_frame,
      MAX_PACKET_SIZE);

    ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
    int pcm_size = sizeof(int16_t) * st->fc.f_gaindown_wav[lay_out].info.channels * FRAME_SIZE;
    int ret = opus_multistream_decode(st->ia_decoder_dcg[i].dep_decoder,
      encoded_frame, encoded_size, (int16_t*)decoded_frame, pcm_size, 0);

    reorder_channels(st->ia_decoder_dcg[i].dec_stream_map, st->ia_decoder_dcg[i].channel, ret, (int16_t*)decoded_frame);

    if (presize[lay_out] > 0)
    {
      memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
      presize[lay_out] = 0;
    }
    ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, FRAME_SIZE);
  }
#endif


  int count_ = 0;
  while (pcm_data)
  {
#if 1
    count_++;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int lay_out = st->channel_layout_map[i];
      if (lay_out == CHANNEL_LAYOUT_MAX)
        break;
      pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, st->frame_size);
      reorder_channels(st->ia_encoder_dcg[i].enc_stream_map, st->ia_encoder_dcg[i].channel, st->frame_size, gain_down_in);
      for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
      {
        if (j < st->ia_encoder_dcg[i].coupled_stream_count)
        {
          extract_pcm_from_group(gain_down_in, extract_pcm, st->ia_encoder_dcg[i].channel, j * 2, 0, st->frame_size);
          int32_t encoded_size = st->encode_frame(st, i, j, 2, extract_pcm, encoded_frame);
          ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
          int ret = st->decode_frame(st, i, j, 2, encoded_frame, encoded_size, extract_pcm_dec);
          insert_pcm_to_group(extract_pcm_dec, decoded_frame, st->ia_encoder_dcg[i].channel, j * 2, 0, st->frame_size);
        }
        else
        {
          extract_pcm_from_group(gain_down_in, extract_pcm, st->ia_encoder_dcg[i].channel, st->ia_encoder_dcg[i].coupled_stream_count + j, 1, st->frame_size);
          int32_t encoded_size = st->encode_frame(st, i, j, 1, extract_pcm, encoded_frame);
          ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
          int ret = st->decode_frame(st, i, j, 1, encoded_frame, encoded_size, extract_pcm_dec);
          insert_pcm_to_group(extract_pcm_dec, decoded_frame, st->ia_encoder_dcg[i].channel, st->ia_encoder_dcg[i].coupled_stream_count + j, 1, st->frame_size);
        }
      }

      reorder_channels(st->ia_decoder_dcg[i].dec_stream_map, st->ia_decoder_dcg[i].channel, st->frame_size, (int16_t*)decoded_frame);

      if (presize[lay_out] > 0)
      {
        memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
        presize[lay_out] = 0;
      }

      ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, st->frame_size);
    }
#else
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int lay_out = st->channel_layout_map[i];
      if (lay_out == CHANNEL_LAYOUT_MAX)
        break;
      pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, st->frame_size);
      reorder_channels(st->ia_encoder_dcg[i].enc_stream_map, st->ia_encoder_dcg[i].channel, st->frame_size, gain_down_in);
      int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[i].dep_encoder,
        gain_down_in,
        st->frame_size,
        encoded_frame,
        MAX_PACKET_SIZE);

      ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
      int pcm_size = sizeof(int16_t) * st->fc.f_gaindown_wav[lay_out].info.channels * st->frame_size;
      int ret = opus_multistream_decode(st->ia_decoder_dcg[i].dep_decoder,
        encoded_frame, encoded_size, (int16_t*)decoded_frame, pcm_size, 0);

      reorder_channels(st->ia_decoder_dcg[i].dec_stream_map, st->ia_decoder_dcg[i].channel, ret, (int16_t*)decoded_frame);

      if (presize[lay_out] > 0)
      {
        memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
        presize[lay_out] = 0;
      }

      ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, st->frame_size);
    }
#endif
  }

  ia_intermediate_file_readclose(st, FILE_GAIN_DOWN, "ALL");
  ia_intermediate_file_writeclose(st, FILE_DECODED, "ALL");
  ia_intermediate_file_writeclose(st, FILE_ENCODED, "ALL");
}

int immersive_audio_encoder_scalefactor(IAEncoder *st)//
{
  if (st->recon_gain_flag == 0)
    return 0;
  /////////////USED FOR DEBUG
  st->fp_dmix = fopen(st->dmix_fn, "r");
  st->fp_weight = fopen(st->weight_fn, "r");
  if (st->fp_dmix == NULL)
  {
    printf("no *_dmix.txt, use default value:1\n");
  }
  if (st->fp_weight == NULL)
  {
    printf("no *_w.txt, use default value:0\n");
  }
  ///////////////////////////

  extension_encode_priv(st);

  int16_t temp[IA_FRAME_MAXSIZE * 12 * 2];
  //calculate scalefactor
  if (st->recon_gain_flag)
  {
    ia_intermediate_file_readopen(st, FILE_DECODED, "ALL");
    ia_intermediate_file_writeopen(st, FILE_UPMIX, "ALL");

    int16_t *up_input[CHANNEL_LAYOUT_MAX];
    int pcm_frames = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      up_input[layout] = (int16_t *)malloc(st->frame_size * MAX_CHANNELS * sizeof(int16_t));
	  if(!up_input[layout])goto FAILED;
      memset(up_input[layout], 0x00, st->frame_size * MAX_CHANNELS * sizeof(int16_t));
      st->upmixer->up_input[layout] = up_input[layout];
      if (st->codec_id == IA_DEP_CODEC_OPUS)
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);
      else if (st->codec_id == IA_DEP_CODEC_AAC)
      {
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);
      }
    }


    while (pcm_frames)
    {
      int dmix_index = default_dmix_index, w_index = default_w_index;
      if (st->fp_dmix)
        fscanf(st->fp_dmix, "%d", &dmix_index);
      if (st->fp_weight)
        fscanf(st->fp_weight, "%d", &w_index);
      st->mdhr.dmix_matrix_type = dmix_index;
      st->mdhr.weight_type = w_index;

      st->upmixer->mdhr_c = st->mdhr;

      upmix3(st->upmixer, st->gaindown_map);
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        //pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);

        conv_writtenfloat(st->upmixer->upmix[layout], temp, st->fc.f_upmix_wav[layout].info.channels, st->frame_size);
        ia_intermediate_file_write(st, FILE_UPMIX, upmix_wav[layout], temp, st->frame_size);
      }


      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], st->frame_size);
      }
    }

    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      if (up_input[layout])
        free(up_input[layout]);
    }

    ia_intermediate_file_readclose(st, FILE_DECODED, "ALL");
    ia_intermediate_file_writeclose(st, FILE_UPMIX, "ALL");

    if (st->fp_dmix)
    {
      fclose(st->fp_dmix);
      st->fp_dmix = NULL;
    }
    if (st->fp_weight)
    {
      fclose(st->fp_weight);
      st->fp_weight = NULL;
    }

    float *m_input = NULL, *r_input = NULL, *s_input = NULL;
    m_input = (float*)malloc(st->frame_size * MAX_CHANNELS*sizeof(float));
    r_input = (float*)malloc(st->frame_size * MAX_CHANNELS*sizeof(float));
    s_input = (float*)malloc(st->frame_size * MAX_CHANNELS*sizeof(float));
    if(!m_input||!r_input||!s_input)goto FAILED;

    float  tmp_s[IA_FRAME_MAXSIZE * MAX_CHANNELS];
    ia_intermediate_file_writeopen(st, FILE_SCALEFACTOR, "ALL");
    ia_intermediate_file_readopen(st, FILE_DOWNMIX_M, "ALL");
    //ia_intermediate_file_readopen(st, FILE_DOWNMIX_S, "ALL");
    ia_intermediate_file_readopen(st, FILE_UPMIX, "ALL");
    
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
#if 0
    int read_size = 0;
    int s_channel = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      pcm_frames = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], tmp_s, st->frame_size);
      int channel = st->fc.f_downmix_s_wav[layout].info.channels;
      for (int j = 0; j < channel; j++)
      {
        for (int k = 0; k < st->frame_size; k++)
        {
          s_input[(j + s_channel)*st->frame_size + k] = tmp_s[j + k*channel];
        }
      }
      read_size = st->frame_size * st->fc.f_downmix_s_wav[layout].info.channels;
      s_channel += channel;
    }
#endif
    printf("\nCalculate Scalable Factor start...\n");
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      //ia_intermediate_file_read(st, FILE_DOWNMIX_M, downmix_m_wav[layout], m_input, st->preskip_size);
      pcm_frames = ia_intermediate_file_read(st, FILE_UPMIX, upmix_wav[layout], r_input, st->preskip_size);
    }

    st->sf->scalefactor_mode = st->scalefactor_mode;
    int s_channel = 0;
    int last_layout = 0;
    while (pcm_frames)
    {
      InScalableBuffer scalable_buff;
      memset(&scalable_buff, 0x00, sizeof(scalable_buff));
      scalable_buff.gaindown_map = st->gaindown_map;

      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        pcm_frames = ia_intermediate_file_read(st, FILE_DOWNMIX_M, downmix_m_wav[layout], m_input, st->frame_size);
        ia_intermediate_file_read(st, FILE_UPMIX, upmix_wav[layout], r_input, st->frame_size);

        scalable_buff.channels_s = s_channel;
        scalable_buff.inbuffer_s = (unsigned char*)s_input;
        scalable_buff.dtype_s = 1;

        scalable_buff.scalable_map = st->upmixer->scalable_map[layout];
        scalable_buff.channels_m = channel_map714[layout];
        scalable_buff.inbuffer_m = (unsigned char*)m_input;
        scalable_buff.dtype_m = 1;

        scalable_buff.channels_r = channel_map714[layout];
        scalable_buff.inbuffer_r = (unsigned char*)r_input;
        scalable_buff.dtype_r= 1;
        if(i != 0)
          cal_scalablefactor2(st->sf, &(st->mdhr), scalable_buff, layout, last_layout);
        ia_intermediate_file_write(st, FILE_SCALEFACTOR, scalefactor_cfg[layout], st->mdhr.scalablefactor[layout], channel_map714[layout]);

        s_channel = channel_map714[layout];
        last_layout = layout;
        memcpy(s_input, m_input, st->frame_size * MAX_CHANNELS*sizeof(float));
      }

      //printf("st->mdhr %lu %lu %lu\n", st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714]);

    }
    if (m_input)
      free(m_input);
    if (r_input)
      free(r_input);
    if (s_input)
      free(s_input);
   
    printf("Calculate Scalable Factor stop!!!\n\n");
    ia_intermediate_file_writeclose(st, FILE_SCALEFACTOR, "ALL");
    //ia_intermediate_file_readclose(st, FILE_DOWNMIX_S, "ALL");
    ia_intermediate_file_readclose(st, FILE_DOWNMIX_M, "ALL");
    ia_intermediate_file_readclose(st, FILE_UPMIX, "ALL");



    //
    ia_intermediate_file_readopen(st, FILE_SCALEFACTOR, "ALL");
    ia_intermediate_file_readopen(st, FILE_ENCODED, "ALL");

    /////////////USED FOR DEBUG
    st->fp_dmix = fopen(st->dmix_fn, "r");
    st->fp_weight = fopen(st->weight_fn, "r");
    if (st->fp_dmix == NULL)
    {
      printf("no *_dmix.txt, use default value:1\n");
    }
    if (st->fp_weight == NULL)
    {
      printf("no *_w.txt, use default value:0\n");
    }
	return 0;
FAILED:
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      if(up_input[layout])
        free(up_input[layout]);
    }
    if (m_input)
      free(m_input);
    if (r_input)
      free(r_input);
    if (s_input)
      free(s_input);
    return -1;

  }

  return 0;
}


int immersive_audio_encode(IAEncoder *st, 
  const int16_t *pcm, int frame_size, unsigned char* data, int *demix_mode, int32_t max_data_bytes)
{
  int ret_size = 0;
  unsigned char meta_info[255];
  unsigned char coded_data[MAX_PACKET_SIZE*3];
  int putsize_recon_gain = 0, putsize_sample = 0;
  if (st->recon_gain_flag == 1)
  {
    int dmix_index = default_dmix_index, w_index = default_w_index;
    if (st->fp_dmix)
      fscanf(st->fp_dmix, "%d", &dmix_index);
    if (st->fp_weight)
      fscanf(st->fp_weight, "%d", &w_index);
    st->mdhr.dmix_matrix_type = dmix_index;
    st->mdhr.weight_type = w_index;

    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      ia_intermediate_file_read(st, FILE_SCALEFACTOR, scalefactor_cfg[layout], st->mdhr.scalablefactor[layout], channel_map714[layout]);
    }


    int encoded_size = 0;
    int buffer_p = 0;

    if (w_index > 0)
      *demix_mode = dmix_index + 3;
    else
      *demix_mode = dmix_index - 1;

    //write recon gain obu
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      putsize_recon_gain += write_recon_gain(st, meta_info + putsize_recon_gain, i);
    }
    int recon_gain_obu_size = iac_write_obu_unit(meta_info, putsize_recon_gain, data, OBU_RECON_GAIN_INFO);

    //write substream obu
    int sub_stream_obu_size = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      /////////////////////////////TODO TODO, should update with laset Spec//////////////////////
      for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
      {
        ia_intermediate_file_read(st, FILE_ENCODED, encoded_ia[layout], &encoded_size, sizeof(int));
        ia_intermediate_file_read(st, FILE_ENCODED, encoded_ia[layout], coded_data, encoded_size);

        sub_stream_obu_size += iac_write_obu_unit(coded_data, encoded_size, data + recon_gain_obu_size + sub_stream_obu_size, OBU_SUBSTREAM);
      }
    }

    return (recon_gain_obu_size + sub_stream_obu_size);
  }
  else
  {
    int dmix_index = default_dmix_index, w_index = default_w_index;
    if (st->fp_dmix)
      fscanf(st->fp_dmix, "%d", &dmix_index);
    if (st->fp_weight)
      fscanf(st->fp_weight, "%d", &w_index);
    st->mdhr.dmix_matrix_type = dmix_index;
    st->mdhr.weight_type = w_index;

    if (w_index > 0)
      *demix_mode = dmix_index + 3;
    else
      *demix_mode = dmix_index - 1;

    int16_t gain_down_out[IA_FRAME_MAXSIZE * MAX_CHANNELS];
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    int pre_ch = 0;
    int16_t extract_pcm[IA_FRAME_MAXSIZE * 2];


    //write substream obu
    int sub_stream_obu_size = 0;
    if (st->channel_groups > 1)
    {
      downmix2(st->downmixer, pcm, dmix_index, w_index);
      gaindown(st->downmixer->downmix_s, st->channel_layout_map, st->gaindown_map, st->mdhr.dmixgain, st->frame_size);

      pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int lay_out = st->channel_layout_map[i];
        if (lay_out == CHANNEL_LAYOUT_MAX)
          break;
        conv_writtenpcm(st->downmixer->downmix_s[lay_out], gain_down_out, channel_map714[lay_out] - pre_ch, st->frame_size);
        reorder_channels(st->ia_encoder_dcg[i].enc_stream_map, st->ia_encoder_dcg[i].channel, st->frame_size, gain_down_out);
        int32_t encoded_size = 0;
        for (int j = 0; j < st->ia_encoder_dcg[i].stream_count; j++)
        {
          if (j < st->ia_encoder_dcg[i].coupled_stream_count)
          {
            extract_pcm_from_group(gain_down_out, extract_pcm, st->ia_encoder_dcg[i].channel, j * 2, 0, st->frame_size);
            encoded_size = st->encode_frame(st, i, j, 2, extract_pcm, coded_data);
          }
          else
          {
            extract_pcm_from_group(gain_down_out, extract_pcm, st->ia_encoder_dcg[i].channel, st->ia_encoder_dcg[i].coupled_stream_count + j, 1, st->frame_size);
            encoded_size = st->encode_frame(st, i, j, 1, extract_pcm, coded_data);
          }
          sub_stream_obu_size += iac_write_obu_unit(coded_data, encoded_size, data  + sub_stream_obu_size, OBU_SUBSTREAM);
        }
        pre_ch = channel_map714[lay_out];
      }
    }
    else
    {
      int lay_out = st->channel_layout_map[0];
      memcpy(gain_down_out, pcm, sizeof(int16_t)*channel_map714[lay_out]* st->frame_size);
      reorder_channels(st->ia_encoder_dcg[0].enc_stream_map, st->ia_encoder_dcg[0].channel, st->frame_size, gain_down_out);
      int32_t encoded_size = 0;
      for (int j = 0; j < st->ia_encoder_dcg[0].stream_count; j++)
      {
        if (j < st->ia_encoder_dcg[0].coupled_stream_count)
        {
          extract_pcm_from_group(gain_down_out, extract_pcm, st->ia_encoder_dcg[0].channel, j * 2, 0, st->frame_size);
          encoded_size = st->encode_frame(st, 0, j, 2, extract_pcm, coded_data);
        }
        else
        {
          extract_pcm_from_group(gain_down_out, extract_pcm, st->ia_encoder_dcg[0].channel, st->ia_encoder_dcg[0].coupled_stream_count + j, 1, st->frame_size);
          encoded_size = st->encode_frame(st, 0, j, 1, extract_pcm, coded_data);
        }
        sub_stream_obu_size += iac_write_obu_unit(coded_data, encoded_size, data + sub_stream_obu_size, OBU_SUBSTREAM);
      }
    }

    return sub_stream_obu_size;
  }
 
  return ret_size;
}

#define REMOVE_TEMP_WAV_FILES 1
#define REMOVE_TEMP_ASC_HEQ_FILES 1
void immersive_audio_encoder_destroy(IAEncoder *et)
{
  downmix_destroy(et->downmixer);
  immersive_audio_encoder_loudgain_destory(et->loudgain);
  upmix_destroy(et->upmixer);
  scalablefactor_destroy(et->sf);

#ifdef REMOVE_TEMP_ASC_HEQ_FILES
  if(et->fp_dmix)
    remove(et->dmix_fn);
  if (et->fp_weight)
    remove(et->weight_fn);
#endif
  if(et->fp_dmix)
    fclose(et->fp_dmix);
  if(et->fp_weight)
    fclose(et->fp_weight);

  ia_intermediate_file_readclose(et, FILE_ENCODED, "ALL");
  ia_intermediate_file_readclose(et, FILE_SCALEFACTOR, "ALL");

#ifdef REMOVE_TEMP_WAV_FILES
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    remove(downmix_m_wav[i]);
    remove(downmix_s_wav[i]);
    remove(gaindown_wav[i]);
    remove(upmix_wav[i]);
    remove(decoded_wav[i]);
    remove(encoded_ia[i]);
    remove(scalefactor_cfg[i]);
  }
#endif
  et->encode_close(et);
  if (et->recon_gain_flag)
    et->decode_close(et);
  free(et);

}
