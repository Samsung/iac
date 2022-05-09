#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
    case 1: case 2: // 미정
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
    case 1: case 2: // 미정
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
    case 1: case 2: // 미정
      break;
    }
    break;
  }
  if (done == 0)
  { // 알 수 없으면 모두 mute 처리
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
    case 1: case 2: // 미정
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
    // 알 수 없으면 모두 mono 처리
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

  // remap된 channel map 정보에 따라 sample을 reorder한다.
  for (int i = 0; i < pcm_frames; i++)
  {
    for (int ch = 0; ch < channels; ch++)
    { // remap되 channel에서 sample을 가지고 와서 임시적으로 t에 담아둔다.
      map_ch = channel_map[ch];
      t[ch] = samples[i*channels + map_ch];
    }
    for (int ch = 0; ch < channels; ch++)
    { // 임시적으로 담아둔 t에 sample을 samples에 담아서 반환한다.
      samples[i*channels + ch] = t[ch];
    }
  }
}

LoudGainMeasure * immersive_audio_encoder_loudgain_create(const unsigned char *channel_layout_map, int sample_rate)
{
  LoudGainMeasure *lm = (LoudGainMeasure *)malloc(sizeof(LoudGainMeasure));
  memset(lm, 0x00, sizeof(LoudGainMeasure));
  memcpy(lm->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_DMAX);
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
  lm->msize25pct = sample_rate / 10 / FRAME_SIZE;

  lm->measure_end = 0;
  return lm;
}

int immersive_audio_encoder_loudness_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout)
{
  int ret = 0;
  resultData_s result;
  result = lm->loudmeter[channel_layout].processFrameLoudness(&(lm->loudmeter[channel_layout]), inbuffer, lm->msize25pct, FRAME_SIZE);
  return 0;
}

int immersive_audio_encoder_gain_measure(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int begin_ch, int nch)
{
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < FRAME_SIZE; fr++)
  {
    for (int ch = begin_ch; ch < begin_ch + nch; ch++)
    {
      dsig = inbuffer[ch*FRAME_SIZE + fr];
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

int immersive_audio_encoder_gain_measure2(LoudGainMeasure *lm, float * inbuffer, int channel_layout, int ch)
{
  int ret = 0;
  float dsig, tsig;
  for (int fr = 0; fr < FRAME_SIZE; fr++)
  {
    dsig = inbuffer[ch*FRAME_SIZE + fr];
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


void conv_writtenpcm(float pcmbuf[][FRAME_SIZE], void *wavbuf, int nch)
{
  int16_t *wbuf = (int16_t *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      wbuf[i + j*nch] = (int16_t)(pcmbuf[i][j] * 32767.0);
    }
  }
}

void conv_writtenpcm1(float *pcmbuf, void *wavbuf, int nch)
{
  int16_t *wbuf = (int16_t *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      wbuf[i + j*nch] = (int16_t)(pcmbuf[i + j*nch] * 32767.0);
    }
  }
}
void conv_writtenfloat(float pcmbuf[][FRAME_SIZE], void *wavbuf, int nch)
{
  unsigned char *wbuf = (unsigned char *)wavbuf;
  for (int i = 0; i < nch; i++)
  {
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      union trans2char trans;
      trans.f = pcmbuf[i][j];
      wbuf[(i + j*nch) * 4] = trans.c[0];
      wbuf[(i + j*nch) * 4 + 1] = trans.c[1];
      wbuf[(i + j*nch) * 4 + 2] = trans.c[2];
      wbuf[(i + j*nch) * 4 + 3] = trans.c[3];
    }
  }
}

int write_mdhr(unsigned char* buffer, int size,  Mdhr mhdr, int type, int stream_count) //0 common, 1 base, 2 advance, ret write size
{
#undef MHDR_LEN
#define MHDR_LEN 128
  unsigned char bitstr[MAX_PACKET_SIZE * 3 + MHDR_LEN] = { 0, };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));

  if (type == CHANNEL_GROUP_TYPE_BCG)
  {
    // ab header
    //
    bs_setbits(&bs, stream_count, 8); //Stream Count (8bits, SC)
    bs_setbits(&bs, 0, 1); //Stream Size Flag (SC bits)
    bs_setbits(&bs, 1, 2); //Size Mode(2 bits)
    bs_setpadbits(&bs); //Reserved
    bs_setbits(&bs, size, 16); //Sub-sample Size (16 or 32 bits) 
// no stream size
    bs_setbits(&bs, 0, 12); // Recon Gain Flag (12bits) 
    bs_setbits(&bs, 0, 4); // Reserved (4 bits)
// no Recon Gain
  }
  else if (type == CHANNEL_GROUP_TYPE_DCG1)
  {
    // tpq header
    bs_setbits(&bs, stream_count, 8); //Stream Count (8bits, SC)
    bs_setbits(&bs, 0, 3); //Stream Size Flag (SC bits)
    bs_setbits(&bs, 1, 2); //Size Mode(2 bits)
    bs_setpadbits(&bs); //Reserved
    bs_setbits(&bs, size, 16); //Sub-sample Size (16 or 32 bits) 
// no stream size
    bs_setbits(&bs, 0, 12); // Recon Gain Flag (12bits) 
    bs_setbits(&bs, 0, 4); // Reserved (4 bits)
// no Recon Gain
  }
  else if (type == CHANNEL_GROUP_TYPE_DCG2)
  {
    //  s header 
    bs_setbits(&bs, stream_count, 8); //Stream Count (8bits, SC)
    bs_setbits(&bs, 0, 1); //Stream Size Flag (SC bits)
    bs_setbits(&bs, 1, 2); //Size Mode(2 bits)
    bs_setpadbits(&bs); //Reserved
    bs_setbits(&bs, size, 16); //Sub-sample Size (16 or 32 bits) 
// no stream size
    bs_setbits(&bs, 0, 12); // Recon Gain Flag (12bits) 
    bs_setbits(&bs, 0, 4); // Reserved (4 bits)
// no Recon Gain
  }
  else if (type == CHANNEL_GROUP_TYPE_DCG3)
  {
    // uv header 
    bs_setbits(&bs, stream_count, 8); //Stream Count (8bits, SC)
    bs_setbits(&bs, 0, 2); //Stream Size Flag (SC bits)
    bs_setbits(&bs, 1, 2); //Size Mode(2 bits)
    bs_setpadbits(&bs); //Reserved
    bs_setbits(&bs, size, 16); //Sub-sample Size (16 or 32 bits) 
// no stream size
    bs_setbits(&bs, 0, 12); // Recon Gain Flag (12bits) 
    bs_setbits(&bs, 0, 4); // Reserved (4 bits)
// no Recon Gain
  }
  else
    return 0;
  memcpy(buffer, bitstr, bs.m_posBase);

  return bs.m_posBase;
}

int write_mdhr2(IAEncoder *st, unsigned char* buffer, int channel_group_size, int type) //0 common, 1 base, 2 advance, ret write size
{
#undef MHDR_LEN
#define MHDR_LEN 255
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char bitstr[MHDR_LEN] = { 0, };
  bitstream_t bs;
  bs_init(&bs, bitstr, sizeof(bitstr));
  unsigned char coded_data_leb[10];
  int coded_size = 0;
  bs_setbits(&bs, 0, 1); //Info_Extension_Flag (f(1))
  bs_setbits(&bs, 0, 7); //reserved (f(7))
  if (uleb_encode(channel_group_size, sizeof(channel_group_size), coded_data_leb,
    &coded_size) != 0) { // Channel_Group_Size (leb128())
    return 0;
  }
  for (int i = 0; i < coded_size; i++)
  {
    bs_setbits(&bs, coded_data_leb[i], 8);
  }
  /*
  if (st->substream_size_flag)
  {
  }
  */
  int layout = st->channel_layout_map[type];
  if (st->recon_gain_flag)
  {
    uint16_t recon_gain_flag = 0;
    int max_recon_gain_fields = 12;
    for (int i = 0; i < max_recon_gain_fields; i++)
    {
      // scalable_map is based on wav channel order.
      // get_recon_gain_flags_map convert wav channel order to vorbis channel order.
/*
    b1(LSB) b2      b3      b4      b5      b6      b7      b8      b9      b10      b11      b12(MSB)
    L	    C       R	    Ls(Lss) Rs(Rss)	Ltf	    Rtf     Lb(Lrs)	Rb(Rrs)	Ltb(Ltr) Rtb(Rtr) LFE
*/
      int channel = get_recon_gain_flags_map[layout][i];
      if (channel >= 0 && st->upmixer->scalable_map[layout][channel] == 1)
      {
        recon_gain_flag = recon_gain_flag | (0x01 << i);
      }
    }

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
    printf("recon_gain_flag: %d\n", recon_gain_flag);
    if (recon_gain_flag < 0)
    {
      goto bad_arg;
    }
    et->recon_gain_flag = recon_gain_flag;
    et->upmixer->recon_gain_flag = recon_gain_flag;
    int error;
    //
    unsigned char def_stream_map[255] = { 0, };
    for (int i = 0; i < 255; i++)
      def_stream_map[i] = i;
    int stream_count, coupled_stream_count;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      if (et->ia_encoder_dcg[i].opus_encoder_dcg == NULL)
        break;

      stream_count = et->ia_decoder_dcg[i].stream_count_dcg;
      coupled_stream_count = et->ia_decoder_dcg[i].coupled_stream_count_dcg;

      //get_dec_map(et->ia_decoder_dcg[i].channel_dcg, et->ia_encoder_dcg[i].meta_mode_dcg, &stream_count, &coupled_stream_count, et->ia_decoder_dcg[i].dec_stream_map_dcg);
      et->ia_decoder_dcg[i].opus_decoder_dcg = opus_multistream_decoder_create(et->input_sample_rate, et->ia_decoder_dcg[i].channel_dcg,
        stream_count,
        coupled_stream_count,
        def_stream_map,
        &error);
      if (error != 0)
      {
        fprintf(stderr, "opus_decoder_create failed %d", error);
      }
    }
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
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[i].opus_encoder_dcg;
      int bitrate = value * (channel_map714[layout] - pre_ch);
      opus_multistream_encoder_ctl(opus_encoder_dcg, IA_SET_BITRATE(bitrate));
      pre_ch = channel_map714[layout];
    }
  }
  break;
  case IA_SET_BANDWIDTH_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[i].opus_encoder_dcg;
      opus_multistream_encoder_ctl(opus_encoder_dcg, IA_SET_BANDWIDTH(value));
    }
  }
  break;
  case IA_SET_VBR_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[i].opus_encoder_dcg;
      opus_multistream_encoder_ctl(opus_encoder_dcg, IA_SET_VBR(value));
    }
  }
  break;
  case IA_SET_COMPLEXITY_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[i].opus_encoder_dcg;
      opus_multistream_encoder_ctl(opus_encoder_dcg, IA_SET_COMPLEXITY(value));
    }
  }
  break;
  case IA_GET_LOOKAHEAD_REQUEST:
  {
    int32_t *value = va_arg(ap, int32_t*);
    OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[0].opus_encoder_dcg;
    opus_multistream_encoder_ctl(opus_encoder_dcg, IA_GET_LOOKAHEAD(value));
  }
  break;
  case IA_SET_FORCE_MODE_REQUEST:
  {
    int32_t value = va_arg(ap, int32_t);
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = et->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      OpusMSEncoder *opus_encoder_dcg = et->ia_encoder_dcg[i].opus_encoder_dcg;
      opus_multistream_encoder_ctl(opus_encoder_dcg, IA_SET_FORCE_MODE(IA_MODE_CELT_ONLY));
    }
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

static int get_surround_channels(CHANNEL_LAYOUT lay_out)
{
  int ret;
  switch (lay_out)
  {
  case CHANNEL_LAYOUT_100:
    ret = 1;
    break;
  case CHANNEL_LAYOUT_200:
    ret = 2;
    break;
  case CHANNEL_LAYOUT_510:
    ret = 5;
    break;
  case CHANNEL_LAYOUT_512:
    ret = 5;
    break;
  case CHANNEL_LAYOUT_514:
    ret = 5;
    break;
  case CHANNEL_LAYOUT_710:
    ret = 7;
    break;
  case CHANNEL_LAYOUT_712:
    ret = 7;
    break;
  case CHANNEL_LAYOUT_714:
    ret = 7;
    break;
  case CHANNEL_LAYOUT_312:
    ret = 3;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
  return ret;
}

static int get_height_channels(CHANNEL_LAYOUT lay_out)
{
  int ret;
  switch (lay_out)
  {
  case CHANNEL_LAYOUT_100:
    ret = 0;
    break;
  case CHANNEL_LAYOUT_200:
    ret = 0;
    break;
  case CHANNEL_LAYOUT_510:
    ret = 0;
    break;
  case CHANNEL_LAYOUT_512:
    ret = 2;
    break;
  case CHANNEL_LAYOUT_514:
    ret = 4;
    break;
  case CHANNEL_LAYOUT_710:
    ret = 0;
    break;
  case CHANNEL_LAYOUT_712:
    ret = 2;
    break;
  case CHANNEL_LAYOUT_714:
    ret = 4;
    break;
  case CHANNEL_LAYOUT_312:
    ret = 2;
    break;
  default:
    printf("wrong inputing\n");
    break;
  }
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
    if (get_surround_channels(layout) < last_s_channels || get_height_channels(layout) < last_h_channels)
    {
      printf("The combination is illegal, please confirm the rule!!!\n");
      return 0;
    }
    last_s_channels = get_surround_channels(layout);
    last_h_channels = get_height_channels(layout);
  }

#if 1
  int idx = 0, ret = 0;;
  int last_cl_layout = CHANNEL_LAYOUT_INVALID;
  uint8_t new_channels[256];
  printf("New channels order: \n");
  printf("---\n");
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    ret = enc_get_new_channels(last_cl_layout, layout, new_channels);
    int channel_c = enc_has_c_channel(ret, new_channels);
////print new channels
    if (ret > 0) {
      for (int ch = 0; ch<ret; ++ch) {
        printf("%s\n", enc_get_channel_name(new_channels[ch]));
      }
    }
/////////////////////
    if (channel_c >= 0)
    {
      //get encoder channel order map
      int index = 0;
      for (index = 0; index < channel_c; index++)
      {
        st->ia_encoder_dcg[i].enc_stream_map_dcg[index] = index;
      }
      for (index = channel_c; index < ret - 2; index++)
      {
        st->ia_encoder_dcg[i].enc_stream_map_dcg[index] = index + 2;
      }
      st->ia_encoder_dcg[i].enc_stream_map_dcg[index++] = channel_c;
      st->ia_encoder_dcg[i].enc_stream_map_dcg[index++] = channel_c + 1;

      //get decoder channel order map
      for (index = 0; index < channel_c; index++)
      {
        st->ia_decoder_dcg[i].dec_stream_map_dcg[index] = index;
      }
      st->ia_decoder_dcg[i].dec_stream_map_dcg[index++] = ret - 2;
      st->ia_decoder_dcg[i].dec_stream_map_dcg[index++] = ret - 1;

      for (; index < ret; index++)
      {
        st->ia_decoder_dcg[i].dec_stream_map_dcg[index] = index - 2;
      }



      st->ia_encoder_dcg[i].stream_count_dcg = (ret -2) / 2 + 2;
      st->ia_encoder_dcg[i].coupled_stream_count_dcg = (ret - 2) / 2;
      st->ia_encoder_dcg[i].channel_dcg = ret;

      st->ia_decoder_dcg[i].stream_count_dcg = st->ia_encoder_dcg[i].stream_count_dcg;
      st->ia_decoder_dcg[i].coupled_stream_count_dcg = st->ia_encoder_dcg[i].coupled_stream_count_dcg;
      st->ia_decoder_dcg[i].channel_dcg = st->ia_encoder_dcg[i].channel_dcg;
    }
    else
    {
      for (int j = 0; j < ret; j++)
      {
        st->ia_encoder_dcg[i].enc_stream_map_dcg[j] = j;
        st->ia_decoder_dcg[i].dec_stream_map_dcg[j] = j;
      }
      st->ia_encoder_dcg[i].stream_count_dcg = ret / 2;
      st->ia_encoder_dcg[i].coupled_stream_count_dcg = ret / 2;
      st->ia_encoder_dcg[i].channel_dcg = ret;

      st->ia_decoder_dcg[i].stream_count_dcg = st->ia_encoder_dcg[i].stream_count_dcg;
      st->ia_decoder_dcg[i].coupled_stream_count_dcg = st->ia_encoder_dcg[i].coupled_stream_count_dcg;
      st->ia_decoder_dcg[i].channel_dcg = st->ia_encoder_dcg[i].channel_dcg;
    }
    printf("---\n");
    idx += ret;

    last_cl_layout = layout;
  }
#endif

  memcpy(st->channel_layout_map, channel_layout_map, CHANNEL_LAYOUT_MAX);
  return channel_groups;
}

IAEncoder *immersive_audio_encoder_create(int32_t Fs,
  int channel_layout_in,
  const unsigned char *channel_layout_cb,
  int application, //OPUS_APPLICATION_AUDIO
  int *error)
{
  IAEncoder *st = (IAEncoder*)malloc(sizeof(IAEncoder));
  memset(st, 0x00, sizeof(IAEncoder));


  int channel_groups = 1;
  channel_groups = get_scalable_format(st, channel_layout_in, channel_layout_cb);
  if(channel_groups == 0)
    exit(-1);
  if (channel_groups == 1)
  {
    st->scalable_format = 0; // non-scalable format
  }
  else
  {
    st->scalable_format = 1;
  }
  unsigned char def_stream_map[255] = { 0,1 };
  for (int i = 0; i < 255; i++)
    def_stream_map[i] = i;

  unsigned channel_mapping = 4;

  int channels = 0;
 
  
  for (int i = 0; i < channel_groups; i++)
  {

    //st->ia_encoder_dcg[i].channel_dcg = channel_map[i];
    //st->ia_encoder_dcg[i].meta_mode_dcg = meta_mode_dcg[i];
    //get_enc_map(st->ia_encoder_dcg[i].channel_dcg, st->ia_encoder_dcg[i].meta_mode_dcg,
    //  &st->ia_encoder_dcg[i].stream_count_dcg, &st->ia_encoder_dcg[i].coupled_stream_count_dcg, st->ia_encoder_dcg[i].enc_stream_map_dcg);

    if (st->ia_encoder_dcg[i].channel_dcg == 2 || st->scalable_format == 0 || st->ia_encoder_dcg[i].channel_dcg > 8)
    {
      if ((st->ia_encoder_dcg[i].opus_encoder_dcg = opus_multistream_encoder_create(Fs,
        st->ia_encoder_dcg[i].channel_dcg,
        st->ia_encoder_dcg[i].stream_count_dcg,
        st->ia_encoder_dcg[i].coupled_stream_count_dcg,
        def_stream_map,
        application,
        error)) == NULL)
      {
        fprintf(stderr, "can not initialize opus encoder.\n");
        exit(-1);
      }
    }
    else
    {
      if ((st->ia_encoder_dcg[i].opus_encoder_dcg =
        opus_multistream_surround_encoder_create(
          Fs,
          st->ia_encoder_dcg[i].channel_dcg,
          channel_mapping, // 0: mono, stereo, 1:multichannel(<8), 2,3:ambisonic, 4: metamode, 255: custom
          &st->ia_encoder_dcg[i].stream_count_dcg,
          &st->ia_encoder_dcg[i].coupled_stream_count_dcg,
          def_stream_map /* [out] */, // CMF=4,CUSTOM
          application,
          error)) == NULL)
      {
        fprintf(stderr, "can not initialize opus encoder.\n");
        exit(-1);
      }
    }
    channels += st->ia_encoder_dcg[i].channel_dcg;
  }
 

  //copmpression part
  st->input_sample_rate = Fs;
  st->recon_gain_flag = 0;
  st->scalefactor_mode = 2;

  st->downmixer = downmix_create(st->channel_layout_map);
  st->loudgain = immersive_audio_encoder_loudgain_create(st->channel_layout_map, Fs);
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
    st->mdhr.dmixgain[i] = 255;
    st->mdhr.chsilence[i] = 0xFFFFFFFF;
    for (int j = 0; j < 12;j++)
      st->mdhr.scalablefactor[i][j] = 0xFF;
  }

  st->upmixer = upmix_create(0, st->channel_layout_map);
  st->upmixer->mdhr_l = st->mdhr;
  st->upmixer->mdhr_c = st->mdhr;

  scalablefactor_init();
  st->sf = scalablefactor_create(st->channel_layout_map);

  st->fp_dmix = NULL;
  st->fp_weight = NULL;

  sprintf(st->dmix_fn, "%s_dmix.txt", "audio");
  sprintf(st->weight_fn, "%s_w.txt", "audio");
  memset(&(st->fc), 0x00, sizeof(st->fc));
  return st;
}

//
int immersive_audio_encoder_dmpd_start(IAEncoder *st)
{
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
  ia_asc_stop(st->asc);
  ia_heq_stop(st->heq);
  st->asc = NULL;
  st->heq = NULL;
  return 0;
}
//

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

  int16_t temp[FRAME_SIZE * 12 * 2];
  downmix2(st->downmixer, pcm, dmix_index, w_index);

  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    immersive_audio_encoder_loudness_measure(st->loudgain, st->downmixer->downmix_m[lay_out], lay_out);
    conv_writtenfloat(st->downmixer->downmix_m[lay_out], temp, channel_map714[lay_out]);
    ia_intermediate_file_write(st, FILE_DOWNMIX_M, downmix_m_wav[lay_out], temp, CHUNK_SIZE);

    conv_writtenfloat(st->downmixer->downmix_s[lay_out], temp, channel_map714[lay_out] - pre_ch);
    ia_intermediate_file_write(st, FILE_DOWNMIX_S, downmix_s_wav[lay_out], temp, CHUNK_SIZE);
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
      if (st->downmixer->gaindown_map[lay_out][cl])
      {
        st->gaindown_map[cl_index] = 1;
        immersive_audio_encoder_gain_measure2(st->loudgain, st->downmixer->downmix_s[lay_out], lay_out, j);
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
    st->mdhr.dmixgain[lay_out] = float_to_qf(db2lin(st->loudgain->dmixgain_lin[lay_out]), 8);
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
}
IA_STATIC_METADATA get_immersive_audio_encoder_ia_static_metadata(IAEncoder *st)
{
  IA_STATIC_METADATA ret;
  memset(&ret, 0x00, sizeof(ret));
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  ret.ambisonics_mode = 0;
  for (int i = 0; i < CHANNEL_LAYOUT_DMAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_DMAX)
      break;
    ret.channel_audio_layer++;
    ret.channel_audio_layer_config[i].loudspeaker_layout = layout;
    ret.channel_audio_layer_config[i].output_gain_is_present_flag = st->output_gain_flag;
    ret.channel_audio_layer_config[i].recon_gain_is_present_flag = st->recon_gain_flag;
    ret.channel_audio_layer_config[i].output_channel_count = channel_map714[layout];
    ret.channel_audio_layer_config[i].substream_count = st->ia_encoder_dcg[i].stream_count_dcg;
    ret.channel_audio_layer_config[i].coupled_substream_count = st->ia_encoder_dcg[i].coupled_stream_count_dcg;
    ret.channel_audio_layer_config[i].loudness = st->mdhr.LKFSch[layout];
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

  ia_intermediate_file_readopen(st, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeopen(st, FILE_GAIN_DOWN, "ALL");

  float *downmix_s[CHANNEL_LAYOUT_MAX]; //common method, don't rely on encode/decode,just gain down
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    downmix_s[layout] = (float *)malloc(FRAME_SIZE * MAX_CHANNELS * sizeof(float));
    memset(downmix_s[layout], 0x00, FRAME_SIZE * MAX_CHANNELS * sizeof(float));
  }

  int pcm_data_s;
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char pre_ch = 0;

  int16_t gain_down_out[FRAME_SIZE * MAX_CHANNELS];

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;

    pcm_data_s = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], downmix_s[layout], CHUNK_SIZE);
  }


  while (pcm_data_s)
  {
    gaindown2(downmix_s, st->channel_layout_map, st->gaindown_map, st->mdhr.dmixgain);

    unsigned char pre_ch = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      conv_writtenpcm1(downmix_s[layout], gain_down_out, channel_map714[layout] - pre_ch);
      ia_intermediate_file_write(st, FILE_GAIN_DOWN, gaindown_wav[layout], gain_down_out, CHUNK_SIZE);
      pre_ch = channel_map714[layout];
    }


    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;

      pcm_data_s = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], downmix_s[layout], CHUNK_SIZE);
    }
  }

  ia_intermediate_file_readclose(st, FILE_DOWNMIX_S, "ALL");
  ia_intermediate_file_writeclose(st, FILE_GAIN_DOWN, "ALL");


  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int layout = st->channel_layout_map[i];
    if (layout == CHANNEL_LAYOUT_MAX)
      break;
    if (downmix_s[layout])
      free(downmix_s[layout]);
  }

  return 0;

}

void extension_encode_priv(IAEncoder *st)
{

  ia_intermediate_file_readopen(st, FILE_GAIN_DOWN, "ALL");
  ia_intermediate_file_writeopen(st, FILE_DECODED, "ALL");
  ia_intermediate_file_writeopen(st, FILE_ENCODED, "ALL");

  int16_t gain_down_in[FRAME_SIZE * MAX_CHANNELS];
  int pcm_data;
  unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
  unsigned char encoded_frame[MAX_PACKET_SIZE] = { 0, };
  int16_t decoded_frame[MAX_PACKET_SIZE];
  int presize[CHANNEL_LAYOUT_MAX];
  
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    presize[i] = PRESKIP_SIZE;
  }

  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    int lay_out = st->channel_layout_map[i];
    if (lay_out == CHANNEL_LAYOUT_MAX)
      break;
    pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, CHUNK_SIZE);
    reorder_channels(st->ia_encoder_dcg[i].enc_stream_map_dcg, st->ia_encoder_dcg[i].channel_dcg, FRAME_SIZE, gain_down_in);
    int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[i].opus_encoder_dcg,
      gain_down_in,
      FRAME_SIZE,
      encoded_frame,
      MAX_PACKET_SIZE);
    
    ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
    int pcm_size = sizeof(int16_t) * st->fc.f_gaindown_wav[lay_out].info.channels * FRAME_SIZE;
    int ret = opus_multistream_decode(st->ia_decoder_dcg[i].opus_decoder_dcg,
      encoded_frame, encoded_size, (int16_t*)decoded_frame, pcm_size, 0);

    reorder_channels(st->ia_decoder_dcg[i].dec_stream_map_dcg, st->ia_decoder_dcg[i].channel_dcg, ret, (int16_t*)decoded_frame);

    if (presize[lay_out] > 0)
    {
      memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
      presize[lay_out] = 0;
    }
    ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, FRAME_SIZE);
  }


  while (pcm_data)
  {
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int lay_out = st->channel_layout_map[i];
      if (lay_out == CHANNEL_LAYOUT_MAX)
        break;
      pcm_data = ia_intermediate_file_read(st, FILE_GAIN_DOWN, gaindown_wav[lay_out], gain_down_in, CHUNK_SIZE);
      reorder_channels(st->ia_encoder_dcg[i].enc_stream_map_dcg, st->ia_encoder_dcg[i].channel_dcg, FRAME_SIZE, gain_down_in);
      int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[i].opus_encoder_dcg,
        gain_down_in,
        FRAME_SIZE,
        encoded_frame,
        MAX_PACKET_SIZE);

      ia_intermediate_file_write(st, FILE_ENCODED, encoded_ia[lay_out], encoded_frame, encoded_size);
      int pcm_size = sizeof(int16_t) * st->fc.f_gaindown_wav[lay_out].info.channels * FRAME_SIZE;
      int ret = opus_multistream_decode(st->ia_decoder_dcg[i].opus_decoder_dcg,
        encoded_frame, encoded_size, (int16_t*)decoded_frame, pcm_size, 0);

      reorder_channels(st->ia_decoder_dcg[i].dec_stream_map_dcg, st->ia_decoder_dcg[i].channel_dcg, ret, (int16_t*)decoded_frame);

      if (presize[lay_out] > 0)
      {
        memset(decoded_frame, 0x00, presize[lay_out] * st->fc.f_gaindown_wav[lay_out].info.channels * sizeof(int16_t));
        presize[lay_out] = 0;
      }

      ia_intermediate_file_write(st, FILE_DECODED, decoded_wav[lay_out], decoded_frame, FRAME_SIZE);
    }
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


  int16_t temp[FRAME_SIZE * 12 * 2];
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
      up_input[layout] = (int16_t *)malloc(FRAME_SIZE * MAX_CHANNELS * sizeof(int16_t));
      memset(up_input[layout], 0x00, FRAME_SIZE * MAX_CHANNELS * sizeof(int16_t));
      st->upmixer->up_input[layout] = up_input[layout];
      pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], CHUNK_SIZE);
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
        //pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], CHUNK_SIZE);

        conv_writtenfloat(st->upmixer->upmix[layout], temp, st->fc.f_upmix_wav[layout].info.channels);
        ia_intermediate_file_write(st, FILE_UPMIX, upmix_wav[layout], temp, CHUNK_SIZE);
      }


      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        pcm_frames = ia_intermediate_file_read(st, FILE_DECODED, decoded_wav[layout], up_input[layout], CHUNK_SIZE);
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
    m_input = (float*)malloc(FRAME_SIZE * MAX_CHANNELS*sizeof(float));
    r_input = (float*)malloc(FRAME_SIZE * MAX_CHANNELS*sizeof(float));
    s_input = (float*)malloc(FRAME_SIZE * MAX_CHANNELS*sizeof(float));
    
    float  tmp_s[FRAME_SIZE * MAX_CHANNELS];
    ia_intermediate_file_writeopen(st, FILE_SCALEFACTOR, "ALL");
    ia_intermediate_file_readopen(st, FILE_DOWNMIX_M, "ALL");
    ia_intermediate_file_readopen(st, FILE_DOWNMIX_S, "ALL");
    ia_intermediate_file_readopen(st, FILE_UPMIX, "ALL");
    
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    int read_size = 0;
    int s_channel = 0;
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      pcm_frames = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], tmp_s, FRAME_SIZE);
      int channel = st->fc.f_downmix_s_wav[layout].info.channels;
      for (int j = 0; j < channel; j++)
      {
        for (int k = 0; k < FRAME_SIZE; k++)
        {
          s_input[(j + s_channel)*FRAME_SIZE + k] = tmp_s[j + k*channel];
        }
      }
      read_size = FRAME_SIZE * st->fc.f_downmix_s_wav[layout].info.channels;
      s_channel += channel;
    }
    printf("calculate scalable factor...\n");
    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      ia_intermediate_file_read(st, FILE_DOWNMIX_M, downmix_m_wav[layout], m_input, PRESKIP_SIZE);
      ia_intermediate_file_read(st, FILE_UPMIX, upmix_wav[layout], r_input, PRESKIP_SIZE);
    }

    st->sf->scalefactor_mode = st->scalefactor_mode;

    while (pcm_frames)
    {
      InScalableBuffer scalable_buff;
      scalable_buff.gaindown_map = st->gaindown_map;

      scalable_buff.channels_s = s_channel;
      scalable_buff.inbuffer_s = (unsigned char*)s_input;
      scalable_buff.dtype_s = 2;

      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        ia_intermediate_file_read(st, FILE_DOWNMIX_M, downmix_m_wav[layout], m_input, FRAME_SIZE);
        ia_intermediate_file_read(st, FILE_UPMIX, upmix_wav[layout], r_input, FRAME_SIZE);

        scalable_buff.scalable_map = st->upmixer->scalable_map[layout];
        scalable_buff.channels_m = channel_map714[layout];
        scalable_buff.inbuffer_m = (unsigned char*)m_input;
        scalable_buff.dtype_m = 1;

        scalable_buff.channels_r = channel_map714[layout];
        scalable_buff.inbuffer_r = (unsigned char*)r_input;
        scalable_buff.dtype_r= 1;
        cal_scalablefactor2(st->sf, &(st->mdhr), scalable_buff, layout);
        ia_intermediate_file_write(st, FILE_SCALEFACTOR, scalefactor_cfg[layout], st->mdhr.scalablefactor[layout], channel_map714[layout]);
      }

      //printf("st->mdhr %lu %lu %lu\n", st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714]);



      read_size = 0;
      s_channel = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int layout = st->channel_layout_map[i];
        if (layout == CHANNEL_LAYOUT_MAX)
          break;
        pcm_frames = ia_intermediate_file_read(st, FILE_DOWNMIX_S, downmix_s_wav[layout], tmp_s, FRAME_SIZE);
        int channel = st->fc.f_downmix_s_wav[layout].info.channels;
        for (int j = s_channel; j < s_channel + channel; j++)
        {
          for (int k = 0; k < FRAME_SIZE; k++)
          {
            s_input[j*FRAME_SIZE + k] = tmp_s[j + k*channel];
          }
        }
        read_size = FRAME_SIZE * st->fc.f_downmix_s_wav[layout].info.channels;
        s_channel += channel;
      }

    }
    if (m_input)
      free(m_input);
    if (r_input)
      free(r_input);
    if (s_input)
      free(s_input);
   

    ia_intermediate_file_writeclose(st, FILE_SCALEFACTOR, "ALL");
    ia_intermediate_file_readclose(st, FILE_DOWNMIX_S, "ALL");
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

  }

  return 0;
}


int immersive_audio_encode(IAEncoder *st, 
  const int16_t *pcm, int frame_size, unsigned char* data, int *demix_mode, int32_t max_data_bytes)
{
  int ret_size = 0;
  unsigned char meta_info[255];
  unsigned char coded_data[MAX_PACKET_SIZE*3];
  int putsize_meta = 0, putsize_sample = 0;
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

#if 0
    uint32_t test_value312 = 0;
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312] = 0;
    test_value312  = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_312][0];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312] | test_value312;
    test_value312 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_312][1];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312] | (test_value312<<8);

    uint32_t test_value512 = 0;
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] = 0;
    test_value512 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_512][4];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] | test_value512;
    test_value512 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_512][5];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] | (test_value512 << 8);
    test_value512 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_512][6];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] | (test_value512 << 16);
    test_value512 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_512][7];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512] | (test_value512 << 24);

    uint32_t test_value714 = 0;
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] = 0;
    test_value714 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_714][6];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] | test_value714;
    test_value714 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_714][7];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] | (test_value714 << 8);
    test_value714 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_714][10];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] | (test_value714 << 16);
    test_value714 = st->mdhr.scalablefactor[CHANNEL_LAYER_MDHR_714][11];
    st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] = st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714] | (test_value714 << 24);
    //ia_intermediate_file_read(st, FILE_SCALEFACTOR, "f312_scale.cfg", &(st->mdhr.chsilence[CHANNEL_LAYOUT_312]), sizeof(uint32_t));
    //ia_intermediate_file_read(st, FILE_SCALEFACTOR, "f512_scale.cfg", &(st->mdhr.chsilence[CHANNEL_LAYOUT_512]), sizeof(uint32_t));
    //ia_intermediate_file_read(st, FILE_SCALEFACTOR, "f714_scale.cfg", &(st->mdhr.chsilence[CHANNEL_LAYOUT_714]), sizeof(uint32_t));
    printf("read,st->mdhr %lu %lu %lu\n", st->mdhr.chsilence[CHANNEL_LAYER_MDHR_312], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_512], st->mdhr.chsilence[CHANNEL_LAYER_MDHR_714]);
#endif

    int encoded_size = 0;
    int buffer_p = 0;

    if (w_index > 0)
      *demix_mode = dmix_index + 3;
    else
      *demix_mode = dmix_index - 1;


    for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
    {
      int layout = st->channel_layout_map[i];
      if (layout == CHANNEL_LAYOUT_MAX)
        break;
      ia_intermediate_file_read(st, FILE_ENCODED, encoded_ia[layout], &encoded_size, sizeof(int));
      ia_intermediate_file_read(st, FILE_ENCODED, encoded_ia[layout], coded_data + putsize_sample, encoded_size);
      // write timed metada info except demix_mode which is writen into sample group box singly
      putsize_meta += write_mdhr2(st, meta_info + putsize_meta, encoded_size, i); 
      putsize_sample += encoded_size;
    }
    //write timed-meta obu and coded-data obu
    int meta_obu_size = opus_write_metadata_obu((uint8_t *const *)meta_info, putsize_meta, data);
    int coded_obu_size = opus_write_codeddata_obu(coded_data, putsize_sample, data + meta_obu_size);
    return (meta_obu_size + coded_obu_size);
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

    int16_t gain_down_out[FRAME_SIZE * MAX_CHANNELS];
    unsigned char channel_map714[] = { 1,2,6,8,10,8,10,12,6 };
    int pre_ch = 0;
    
    if (st->scalable_format > 0)
    {
      downmix2(st->downmixer, pcm, dmix_index, w_index);

      int cl_index = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int lay_out = st->channel_layout_map[i];
        if (lay_out == CHANNEL_LAYOUT_MAX)
          break;
        for (int j = 0; j < channel_map714[lay_out] - pre_ch; j++)
        {
          int cl = st->downmixer->channel_order[cl_index];
          if (st->downmixer->gaindown_map[lay_out][cl])
          {
            st->gaindown_map[cl_index] = 1;
          }
          cl_index++;
        }
        pre_ch = channel_map714[lay_out];
      }
      gaindown(st->downmixer->downmix_s, st->channel_layout_map, st->gaindown_map, st->mdhr.dmixgain);

      pre_ch = 0;
      for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
      {
        int lay_out = st->channel_layout_map[i];
        if (lay_out == CHANNEL_LAYOUT_MAX)
          break;
        conv_writtenpcm(st->downmixer->downmix_s[lay_out], gain_down_out, channel_map714[lay_out] - pre_ch);
        reorder_channels(st->ia_encoder_dcg[i].enc_stream_map_dcg, st->ia_encoder_dcg[i].channel_dcg, FRAME_SIZE, gain_down_out);
        int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[i].opus_encoder_dcg,
          gain_down_out,
          FRAME_SIZE,
          coded_data + putsize_sample,
          MAX_PACKET_SIZE);

        putsize_meta += write_mdhr2(st, meta_info + putsize_meta, encoded_size, i);
        putsize_sample += encoded_size;

        pre_ch = channel_map714[lay_out];
      }
    }
    else
    {
      int lay_out = st->channel_layout_map[0];
      memcpy(gain_down_out, pcm, sizeof(int16_t)*channel_map714[lay_out]*FRAME_SIZE);
      reorder_channels(st->ia_encoder_dcg[0].enc_stream_map_dcg, st->ia_encoder_dcg[0].channel_dcg, FRAME_SIZE, gain_down_out);
      int32_t encoded_size = opus_multistream_encode(st->ia_encoder_dcg[0].opus_encoder_dcg,
        gain_down_out,
        FRAME_SIZE,
        coded_data + putsize_sample,
        MAX_PACKET_SIZE);

      putsize_meta += write_mdhr2(st, meta_info + putsize_meta, encoded_size, 0);
      putsize_sample += encoded_size;
    }



    //write timed-meta obu and coded-data obu
    int meta_obu_size = opus_write_metadata_obu((uint8_t *const *)meta_info, putsize_meta, data);
    int coded_obu_size = opus_write_codeddata_obu(coded_data, putsize_sample, data + meta_obu_size);
    return (meta_obu_size + coded_obu_size);
  }
 
  return ret_size;
}

OpusMSEncoder *get_immersive_audio_encoder_by_type(IAEncoder *et, CHANNEL_GROUP_TYPE type)
{
  OpusMSEncoder *ret = NULL;
  switch (type)
  {
  case CHANNEL_GROUP_TYPE_BCG:
    ret = et->ia_encoder_dcg[CHANNEL_GROUP_TYPE_BCG].opus_encoder_dcg;
    break;
  case CHANNEL_GROUP_TYPE_DCG1:
    ret = et->ia_encoder_dcg[CHANNEL_GROUP_TYPE_DCG1].opus_encoder_dcg;
    break;
  case CHANNEL_GROUP_TYPE_DCG2:
    ret = et->ia_encoder_dcg[CHANNEL_GROUP_TYPE_DCG2].opus_encoder_dcg;
    break;
  case CHANNEL_GROUP_TYPE_DCG3:
    ret = et->ia_encoder_dcg[CHANNEL_GROUP_TYPE_DCG3].opus_encoder_dcg;
    break;
  default:
    printf("wrong type input\n");
    break;
  }
  return ret;
}

#define REMOVE_TEMP_WAV_FILES 1
#define REMOVE_TEMP_ASC_HEQ_FILES 1
void immersive_audio_encoder_destroy(IAEncoder *et)
{
  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    if(et->ia_encoder_dcg[i].opus_encoder_dcg)
      et->ia_encoder_dcg[i].opus_encoder_dcg;
  }

  downmix_destroy(et->downmixer);
  immersive_audio_encoder_loudgain_destory(et->loudgain);
  upmix_destroy(et->upmixer);
  scalablefactor_destroy(et->sf);


  for (int i = 0; i < CHANNEL_LAYOUT_MAX; i++)
  {
    if (et->ia_decoder_dcg[i].opus_decoder_dcg)
      et->ia_decoder_dcg[i].opus_decoder_dcg;
  }

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
  free(et);

}