/******************************************************************************
*                       Samsung Electronics Co., Ltd.                        *
*                                                                            *
*                           Copyright (C) 2021                               *
*                          All rights reserved.                              *
*                                                                            *
* This software is the confidential and proprietary information of Samsung   *
* Electronics Co., Ltd. ("Confidential Information"). You shall not disclose *
* such Confidential Information and shall use it only in accordance with the *
* terms of the license agreement you entered into with Samsung Electronics   *
* Co., Ltd.                                                                  *
*                                                                            *
* Removing or modifying of the above copyright notice or the following       *
* descriptions will terminate the right of using this software.              *
*                                                                            *
* As a matter of courtesy, the authors request to be informed about uses of  *
* this software and about bugs in this software.                             *
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bitstreamrw.h"
#include "demixer.h"
#include "fixedp11_5.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_decoder.h"
#include "channel.h"
#include "opus_extension.h"

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#define MAX_MIXED_CHANNEL_NUM  4


enum {
    channel_mixed_s2_l,
    channel_mixed_s2_r,
    channel_mixed_s3_l,
    channel_mixed_s3_r,
    channel_mixed_s5_l,
    channel_mixed_s5_r,
    channel_mixed_s7_l,
    channel_mixed_s7_r,
    channel_mixed_h_l,
    channel_mixed_h_r,
    channel_mixed_cnt
};


typedef struct {
    float w_x;
    float w_z;
} w_info;

static struct DemixingTypeMat {
    float alpha;
    float beta;
    float gamma;
    float delta;
    int   w_idx_offset;
} demixing_type_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1.0},
    {0.707, 0.707, 0.707, 0.707, -1.0},
    {1.0, 0.866, 0.866, 0.866, -1.0},
    {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1.0},
    {0.707, 0.707, 0.707, 0.707, 1.0},
    {1.0, 0.866, 0.866, 0.866, 1.0},
    {0, 0, 0, 0, 0}
};

struct Demixer {
    float hanning[WINDOW_SIZE];
    float startWin[FRAME_SIZE];
    float stopWin[FRAME_SIZE];
    int   last_dmixtypenum;
    float last_weight_state_value_x_prev; // n-1 packet
    float last_weight_state_value_x_prev2; // n-2 packet
    int   cstep; // current step

    float last_sf[channel_layout_type_count][MAX_MIXED_CHANNEL_NUM];
    float last_sfavg[channel_layout_type_count][MAX_MIXED_CHANNEL_NUM];
    float *ch_data[channel_cnt];
    float buffer[channel_mixed_cnt*FRAME_SIZE];
};


typedef int (*DemixFunc)(Demixer*, DemixingParam*);
typedef void (*EqualizeRMSFunc)(Demixer*, DemixingParam*);

static void demixer_equalizeRMS(Demixer *this, uint32_t layout, int fs,
        int count, int *channel, uint32_t scales);
static void demixer_update_last_values(Demixer*, DemixingParam*);


#define maxf(a,b) ((a) > (b)? (a):(b))
#define minf(a,b) ((a) < (b)? (a):(b))
static w_info calc_w_v2(int weighttypenum, float w_x_prev)
{
    w_info wi;
    float w_x;
    float w_y;
    float w_z;

    if (weighttypenum == 1)
        w_x = minf(w_x_prev + 0.1, 1.0);
    else // weighttypenum == 0
        w_x = maxf(w_x_prev - 0.1, 0.0);

    if (w_x <= 1.0)
        w_y = (float)(cbrt((w_x - 0.5) / 4.0) + 0.5);
    else
        w_y = (float)(cbrt((w_x - 0.5 - 1.0) / 4.0) + 0.5 + 1);

    w_z = w_y * 0.5;

    wi.w_x = w_x;
    wi.w_z = w_z;

    return wi;
}


static void gain_up(Demixer *this, int count, int *in, int *out,
        int frame_size, float gain)
{
    int ich, och, i;

    ia_logi("gain-up: count %d, frame size %d, gain %f",
            count, frame_size, gain);
    for (int c=0; c<count; ++c) {
        ich = in[c];
        och = out[c];

        for (i = 0; i < frame_size; i++)
            this->buffer[och * frame_size + i] = this->ch_data[ich][i] / gain;
        this->ch_data[ich] = this->buffer + och * frame_size;

        ia_logt("channel %s(%d) at %p, buffer at %p", get_channel_name(ich),
                ich, this->ch_data[ich], this->buffer);
    }
}

static void gain_up_h (Demixer *this, int frame_size, float g)
{
    static int htin[] = {channel_tl, channel_tr};
    static int hin[] = {channel_hl, channel_hr};
    int *in = 0;
    int out[] = {channel_mixed_h_l, channel_mixed_h_r};

    if (this->ch_data[channel_tl])
        in = htin;
    else if (this->ch_data[channel_hl])
        in = hin;

    if (in && g)
        gain_up(this, 2, in, out, frame_size, g);
}

static void gain_up_s (Demixer *this, int frame_size, float g)
{
    static int s2in[] = {channel_l2, channel_r2};
    static int s3in[] = {channel_l3, channel_r3};
    static int s5in[] = {channel_sl5, channel_sr5};
    static int s2out[] = {channel_mixed_s2_l, channel_mixed_s2_r};
    static int s3out[] = {channel_mixed_s3_l, channel_mixed_s3_r};
    static int s5out[] = {channel_mixed_s5_l, channel_mixed_s5_r};
    int *in, *out;
    in = out = 0;

    if (this->ch_data[channel_l2]) {
        in = s2in;
        out = s2out;
    } else if (this->ch_data[channel_l3]) {
        in = s3in;
        out = s3out;
    } else if (this->ch_data[channel_sl5]) {
        in = s5in;
        out = s5out;
    }

    if (in && g)
        gain_up(this, 2, in, out, frame_size, g);
}

static int demixer_s2to3(Demixer *this, DemixingParam *param)
{
    int fs = param->frame_size;

    ia_logd("---- s2to3 ----");
    for (int i=0; i<fs; i++) {
        this->buffer[channel_mixed_s3_l * fs + i] =
            this->ch_data[channel_l2][i] - 0.707 * this->ch_data[channel_c][i];
        this->buffer[channel_mixed_s3_r * fs + i] =
            this->ch_data[channel_r2][i] - 0.707 * this->ch_data[channel_c][i];
    }
    this->ch_data[channel_l3] = this->buffer + channel_mixed_s3_l * fs;
    this->ch_data[channel_r3] = this->buffer + channel_mixed_s3_r * fs;

    ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
            get_channel_name(channel_l3), channel_l3, this->ch_data[channel_l3],
            get_channel_name(channel_r3), channel_r3, this->ch_data[channel_r3],
            this->buffer);
    return 0;
}

static int demixer_s3(Demixer *this, DemixingParam *param)
{
    return demixer_s2to3(this, param);
}

static int demixer_s3to5(Demixer *this, DemixingParam *param)
{
    int fs = param->frame_size;
    int i=0;

    int Typeid = param->demixing_mode;
    int last_Typeid = this->last_dmixtypenum;

    ia_logt("---- s3to5 ----");
    ia_logi("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

    for (; i < PRESKIP_SIZE; i++) {
        this->buffer[channel_mixed_s5_l * fs + i] =
            (this->ch_data[channel_l3][i] - this->ch_data[channel_l5][i]) / demixing_type_mat[last_Typeid].delta;
        this->buffer[channel_mixed_s5_r * fs + i] =
            (this->ch_data[channel_r3][i] - this->ch_data[channel_r5][i]) / demixing_type_mat[last_Typeid].delta;
    }

    for (; i < FRAME_SIZE; i++) {
        this->buffer[channel_mixed_s5_l * fs + i] =
            (this->ch_data[channel_l3][i] - this->ch_data[channel_l5][i]) / demixing_type_mat[Typeid].delta;
        this->buffer[channel_mixed_s5_r * fs + i] =
            (this->ch_data[channel_r3][i] - this->ch_data[channel_r5][i]) / demixing_type_mat[Typeid].delta;
    }

    this->ch_data[channel_sl5] = this->buffer + channel_mixed_s5_l * fs;
    this->ch_data[channel_sr5] = this->buffer + channel_mixed_s5_r * fs;

    ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
            get_channel_name(channel_sl5), channel_sl5, this->ch_data[channel_sl5],
            get_channel_name(channel_sr5), channel_sr5, this->ch_data[channel_sr5],
            this->buffer);

    return 0;
}

static int demixer_s5(Demixer *this, DemixingParam *param)
{
    if (!this->ch_data[channel_l3])
        demixer_s3(this, param);
    return demixer_s3to5(this, param);
}

static int demixer_s5to7(Demixer *this, DemixingParam *param)
{
    int fs = param->frame_size;
    int i=0;

    int Typeid = param->demixing_mode;
    int last_Typeid = this->last_dmixtypenum;

    ia_logt("---- s5to7 ----");
    ia_logi("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

    for (; i < PRESKIP_SIZE; i++) {
        this->buffer[channel_mixed_s7_l * fs + i] =
            (this->ch_data[channel_sl5][i] - this->ch_data[channel_sl7][i] * demixing_type_mat[last_Typeid].alpha) / demixing_type_mat[last_Typeid].beta;
        this->buffer[channel_mixed_s7_r * fs + i] =
            (this->ch_data[channel_sr5][i] - this->ch_data[channel_sr7][i] * demixing_type_mat[last_Typeid].alpha) / demixing_type_mat[last_Typeid].beta;
    }

    for (; i < FRAME_SIZE; i++) {
        this->buffer[channel_mixed_s7_l * fs + i] =
            (this->ch_data[channel_sl5][i] - this->ch_data[channel_sl7][i] * demixing_type_mat[Typeid].alpha) / demixing_type_mat[Typeid].beta;
        this->buffer[channel_mixed_s7_r * fs + i] =
            (this->ch_data[channel_sr5][i] - this->ch_data[channel_sr7][i] * demixing_type_mat[Typeid].alpha) / demixing_type_mat[Typeid].beta;
    }

    this->ch_data[channel_bl7] = this->buffer + channel_mixed_s7_l * fs;
    this->ch_data[channel_br7] = this->buffer + channel_mixed_s7_r * fs;

    ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
            get_channel_name(channel_bl7), channel_bl7, this->ch_data[channel_bl7],
            get_channel_name(channel_br7), channel_br7, this->ch_data[channel_br7],
            this->buffer);
    return 0;
}

static int demixer_s7(Demixer *this, DemixingParam *param)
{
    if (!this->ch_data[channel_sl5])
        demixer_s5(this, param);
    return demixer_s5to7(this, param);
}

static int demixer_ht2to2(Demixer *this, DemixingParam *param)
{
    int fs = param->frame_size;
    int i=0;

    int Typeid = param->demixing_mode;
    int last_Typeid = this->last_dmixtypenum;

    int32_t WeightTypeNum = demixing_type_mat[param->demixing_mode].w_idx_offset;
    w_info w = calc_w_v2(WeightTypeNum, this->last_weight_state_value_x_prev);
    w_info last_w =
        calc_w_v2(demixing_type_mat[last_Typeid].w_idx_offset, this->last_weight_state_value_x_prev2);

    ia_logt("---- hf2to2 ----");
    ia_logi("Typeid %d, WeightTypeNum %d, w_x %f, w_z %f, Lasttypeid %d, last_w_x %f, last_w_z %f",
            Typeid, WeightTypeNum, w.w_x, w.w_z, last_Typeid, last_w.w_x, last_w.w_z);

    for (; i < PRESKIP_SIZE; i++) {
        this->buffer[channel_mixed_h_l * fs + i] = this->ch_data[channel_tl][i] -
            demixing_type_mat[last_Typeid].delta * last_w.w_z * this->ch_data[channel_sl5][i];
        this->buffer[channel_mixed_h_r * fs + i] = this->ch_data[channel_tr][i] -
            demixing_type_mat[last_Typeid].delta * last_w.w_z * this->ch_data[channel_sr5][i];
    }

    for (; i < FRAME_SIZE; i++) {
        this->buffer[channel_mixed_h_l * fs + i] = this->ch_data[channel_tl][i] -
            demixing_type_mat[Typeid].delta * w.w_z * this->ch_data[channel_sl5][i];
        this->buffer[channel_mixed_h_r * fs + i] = this->ch_data[channel_tr][i] -
            demixing_type_mat[Typeid].delta * w.w_z * this->ch_data[channel_sr5][i];
    }

    this->ch_data[channel_hl] = this->buffer + channel_mixed_h_l * fs;
    this->ch_data[channel_hr] = this->buffer + channel_mixed_h_r * fs;

    ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
            get_channel_name(channel_hl), channel_hl, this->ch_data[channel_hl],
            get_channel_name(channel_hr), channel_hr, this->ch_data[channel_hr],
            this->buffer);
    return 0;
}

static int demixer_h2(Demixer *this, DemixingParam *param)
{
    return demixer_ht2to2(this, param);
}

static int demixer_h2to4(Demixer *this, DemixingParam *param)
{
    int fs = param->frame_size;
    int i=0;

    int Typeid = param->demixing_mode;
    int last_Typeid = this->last_dmixtypenum;

    ia_logt("---- h2to4 ----");
    ia_logi("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

    for (; i < PRESKIP_SIZE; i++) {
        this->buffer[channel_mixed_h_l * fs + i] =
            (this->ch_data[channel_hl][i] - this->ch_data[channel_hfl][i]) / demixing_type_mat[last_Typeid].gamma;
        this->buffer[channel_mixed_h_r * fs + i] =
            (this->ch_data[channel_hr][i] - this->ch_data[channel_hfr][i]) / demixing_type_mat[last_Typeid].gamma;
    }

    for (; i < FRAME_SIZE; i++) {
        this->buffer[channel_mixed_h_l * fs + i] =
            (this->ch_data[channel_hl][i] - this->ch_data[channel_hfl][i]) / demixing_type_mat[Typeid].gamma;
        this->buffer[channel_mixed_h_r * fs + i] =
            (this->ch_data[channel_hr][i] - this->ch_data[channel_hfr][i]) / demixing_type_mat[Typeid].gamma;
    }

    this->ch_data[channel_hbl] = this->buffer + channel_mixed_h_l * fs;
    this->ch_data[channel_hbr] = this->buffer + channel_mixed_h_r * fs;

    ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
            get_channel_name(channel_hbl), channel_hbl, this->ch_data[channel_hbl],
            get_channel_name(channel_hbr), channel_hbr, this->ch_data[channel_hbr],
            this->buffer);
    return 0;
}

static int demixer_h4(Demixer *this, DemixingParam *param)
{
    if (!this->ch_data[channel_hl])
        demixer_h2(this, param);
    return demixer_h2to4(this, param);
}


int demixer_demix_200(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix2 ----");
    if (!this->cstep)
        gain_up_s(this, param->frame_size, param->gain[this->cstep]);
    return 0;
}

int demixer_demix_312(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix312 ----");
    if (!this->cstep)
        gain_up_s(this, param->frame_size, param->gain[this->cstep]);
    gain_up_h(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_l3])
        demixer_s3(this, param);
    return 0;
}

int demixer_demix_510(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix510 ----");
    if (!this->cstep)
        gain_up_s(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_sl5])
        demixer_s5(this, param);
    return 0;
}

int demixer_demix_512(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix512 ----");
    if (!this->cstep)
        gain_up_s(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_tl] && this->ch_data[channel_hl])
        gain_up_h(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_sl5])
        demixer_s5(this, param);
    if (!this->ch_data[channel_hl])
        demixer_h2(this, param);
    return 0;
}

int demixer_demix_514(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix514 ----");
    if (!this->cstep)
        gain_up_s(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_sl5])
        demixer_s5(this, param);
    if (!this->ch_data[channel_hbl])
        demixer_h4(this, param);
    return 0;
}

int demixer_demix_710(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix710 ----");
    if (!this->ch_data[channel_bl7])
        demixer_s7(this, param);
    return 0;
}

int demixer_demix_712(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix712 ----");
    if (!this->ch_data[channel_tl] && this->ch_data[channel_hl])
        gain_up_h(this, param->frame_size, param->gain[this->cstep]);
    if (!this->ch_data[channel_bl7])
        demixer_s7(this, param);
    if (!this->ch_data[channel_hl])
        demixer_h2(this, param);
    return 0;
}

int demixer_demix_714(Demixer *this, DemixingParam *param)
{
    ia_logd("---- demix714 ----");
    if (!this->ch_data[channel_bl7])
        demixer_s7(this, param);
    if (!this->ch_data[channel_hbl])
        demixer_h4(this, param);
    return 0;
}

static DemixFunc demix_func[] = {
    0,
    demixer_demix_200,
    demixer_demix_510,
    demixer_demix_512,
    demixer_demix_514,
    demixer_demix_710,
    demixer_demix_712,
    demixer_demix_714,
    demixer_demix_312,
};

#define ADD_MIXED_CHANNELS_NAME(t, n) add_mixed_##t##n##channels
#define ADD_MIXED_CHANNELS_FUNC(t, n, ch, chl, chr) \
static int ADD_MIXED_CHANNELS_NAME(t, n)(Demixer* this, int* array) \
{ \
    int cnt = 0; \
    if (channel_##ch != channel_##chl && this->ch_data[channel_##ch] && \
            this->ch_data[channel_##chl]) { \
        array[cnt++] = channel_##chl; \
        array[cnt++] = channel_##chr; \
    } \
    return cnt; \
}

ADD_MIXED_CHANNELS_FUNC(s, 3, l2, l3, r3)
ADD_MIXED_CHANNELS_FUNC(s, 5, l3, sl5, sr5)
ADD_MIXED_CHANNELS_FUNC(s, 7, sl5, bl7, br7)
ADD_MIXED_CHANNELS_FUNC(h, 0, tl, tl, tr)
ADD_MIXED_CHANNELS_FUNC(h, 2, tl, hl, hr)
ADD_MIXED_CHANNELS_FUNC(h, 4, hl, hbl, hbr)

static int g_recon_channel[] = {
    rg_channel_l,
    rg_channel_r,
    rg_channel_c,
    rg_channel_lfe,
    rg_channel_lss,
    rg_channel_rss,
    rg_channel_lrs,
    rg_channel_rrs,
    rg_channel_ltf,
    rg_channel_rtf,
    rg_channel_ltb,
    rg_channel_rtb,
    rg_channel_l,
    rg_channel_l,
    rg_channel_r,
    rg_channel_ltf,
    rg_channel_rtf,
    rg_channel_l,
    rg_channel_r,
    rg_channel_ls,
    rg_channel_rs,
    rg_channel_ltf,
    rg_channel_rtf,
};

static uint32_t get_scales (DemixingParam *param, int num, int *array)
{
    uint32_t scales = 0;
    int rch;
    int shift = 0;
    for (int i=0; i<num; ++i) {
        rch = g_recon_channel[array[i]];
        if (param->recon_gain[rch]) {
            scales = scales | param->recon_gain[rch] << shift;
            shift += 8;
        }
    }
    return scales;
}

#define ERMS_FUNC_NAME(S, L, H) demixer_equalizeRMS_##S##L##H
#define ERMS_FUNC(S, L, H) \
static void ERMS_FUNC_NAME(S, L, H)(Demixer* this, DemixingParam* param) \
{ \
    int mixch_list[MAX_MIXED_CHANNEL_NUM]; \
    uint32_t scales; \
    int ret = 0; \
    ia_logd("---- equalizeRMS%d%d%d ----", S, L, H); \
    ret = ADD_MIXED_CHANNELS_NAME(s, S)(this, mixch_list); \
    ret += ADD_MIXED_CHANNELS_NAME(h, H)(this, &mixch_list[ret]); \
    for (int i=0; i<ret; ++i) {\
        ia_logd("mixed channel %s(%d), recon gain channel %s(%d)", \
                get_channel_name(mixch_list[i]), mixch_list[i], \
                get_recon_gain_channel_name(g_recon_channel[mixch_list[i]]), \
                g_recon_channel[mixch_list[i]]); \
    } \
    scales = get_scales(param, ret, mixch_list); \
    demixer_equalizeRMS(this, channel_layout_type_##S##_##L##_##H, \
            param->frame_size, ret, mixch_list, scales); \
}

ERMS_FUNC(3,1,2)
ERMS_FUNC(5,1,0)
ERMS_FUNC(5,1,2)
ERMS_FUNC(5,1,4)
ERMS_FUNC(7,1,0)
ERMS_FUNC(7,1,2)
ERMS_FUNC(7,1,4)

static EqualizeRMSFunc equalizeRMS_func[] = {
    0, 0,
    ERMS_FUNC_NAME(5, 1, 0),
    ERMS_FUNC_NAME(5, 1, 2),
    ERMS_FUNC_NAME(5, 1, 4),
    ERMS_FUNC_NAME(7, 1, 0),
    ERMS_FUNC_NAME(7, 1, 2),
    ERMS_FUNC_NAME(7, 1, 4),
    ERMS_FUNC_NAME(3, 1, 2),
};

void demixer_equalizeRMS(Demixer *this, uint32_t layout, int fs,
        int count, int *channel, uint32_t scales)
{
    float N = 7; //7 frame
    float sf, sfavg;
    float filtBuf;
    float *out;
    int bitshift = 0;
    int ch;
    qf_t scale;

    ia_logt("---- demixer_equalizeRMS ----");
    if (fs != FRAME_SIZE) {
        ia_logw("Frame size (%d) is not %d", fs, FRAME_SIZE);
        return;
    }

    for (int i = 0; i < count; i++) {
        ch = channel[i];
        scale = (scales >> bitshift) & 0xFF;

        out = this->ch_data[ch];
        sf = qf_to_float(scale, 8);

        if (N > 0) {
            sfavg = (2 / (N + 1)) * sf + (1 - 2 / (N + 1)) *
                this->last_sfavg[layout][i];
        } else {
            sfavg = sf;
        }

        ia_logt("channel %s(%d) is smoothed with %d(0x%0x) -> %f.",
                get_channel_name(ch), ch, scale, scale, sf);
        /* different scale factor in overapping area */
        for (int j = 0; j < fs; j++) {
            filtBuf = this->last_sfavg[layout][i] *
                this->stopWin[j] + sfavg * this->startWin[j];
            out[j] *= filtBuf;
        }

        this->last_sf[layout][i] = sf;
        this->last_sfavg[layout][i] = sfavg;
        bitshift += 8;
    }

}

void demixer_update_last_values(Demixer *this, DemixingParam *param)
{
    ia_logd("update demixer parameters: ");
    ia_logd("dmixtypenum: %d -> %d", this->last_dmixtypenum, param->demixing_mode);
    ia_logd("weight_state_value_x_prev2: %f -> %f",
            this->last_weight_state_value_x_prev2,
            this->last_weight_state_value_x_prev);

    this->last_dmixtypenum = param->demixing_mode;
    this->last_weight_state_value_x_prev2 = this->last_weight_state_value_x_prev;
    w_info w = calc_w_v2(demixing_type_mat[param->demixing_mode].w_idx_offset,
            this->last_weight_state_value_x_prev);

    ia_logd("last_weight_state_value_x_prev: %f -> %f",
            this->last_weight_state_value_x_prev, w.w_x);
    this->last_weight_state_value_x_prev = w.w_x;
}




Demixer* demixer_create(void)
{
    Demixer *this = NULL;
    this = (Demixer *)malloc(sizeof(struct Demixer));
    ia_logt("Demixer %p size %ld", this, sizeof(struct Demixer));
    if (this)
        demixer_init(this);
    return this;
}

int demixer_init(Demixer *this)
{
    int preskip = 312;
    int windowLen = FRAME_SIZE / 8;
    int n = windowLen;
    int overlapLen = windowLen / 2;
    memset(this, 0x00, sizeof(Demixer));

    this->last_dmixtypenum = 0;
    this->last_weight_state_value_x_prev = 0.0; // n-1 packet
    this->last_weight_state_value_x_prev2 = 0.0; // n-2 packet

    /**
     * init hanning window.
     * */
    for (int i = 0; i < n; i++) {
        this->hanning[i] = (0.5 * (1.0 - cos(2.0*M_PI*(double)i / (double)(n - 1))));
    }

    /* [0, 252<312-60>) */
    for (int i = 0; i < preskip - overlapLen; i++) {
        this->startWin[i] = 0;
        this->stopWin[i] = 1;
    }

    /* [252, 312) */
    for (int i = preskip - overlapLen, j = 0; i < preskip; i++, j++) {
        this->startWin[i] = this->hanning[j];
        this->stopWin[i] = this->hanning[j+overlapLen];
    }

    /* [312, 960) */
    for (int i = preskip; i < FRAME_SIZE; i++) {
        this->startWin[i] = 1;
        this->stopWin[i] = 0;
    }

    for (int i=0; i<channel_layout_type_count; ++i) {
        for (int j=0; j<MAX_MIXED_CHANNEL_NUM; ++j) {
            this->last_sf[i][j] = 1.0;
            this->last_sfavg[i][j] = 1.0;
        }
    }

    return (0);
}

int demixer_demix(Demixer *this, void *buf, int frame_size, void *pcm,
        DemixingParam *param)
{
    int ret = 0;
    int channel;

    float *data = (float *)buf;
    float *out = (float *)pcm;
    const uint8_t *playout;
    int chs, layout;

    if (frame_size != FRAME_SIZE) {
        ia_loge("the decoded frame size (%d) is not %d", frame_size, FRAME_SIZE);
        return -1;
    }

    layout = param->layout[param->steps - 1];
    ia_logi("target layout %d and need %d steps", layout, param->steps);
    if (!demix_func[layout]) {
        ia_loge("Can not support layout (%d) to demix.", layout);
        return -1;
    }

    memset(this->ch_data, 0x00, sizeof(float *) * channel_cnt);

    chs = get_layout_channel_count(layout);
    ia_logi("demixing: frame size %d", frame_size);
    for (int ch = 0; ch<chs; ++ch) {
        channel = param->channel_order[ch];
        this->ch_data[channel] = &data[ch * frame_size];
        ia_logt("input mixed channel %s(%d) at %p",
                get_channel_name(channel), channel, &data[ch * frame_size]);
    }

    param->frame_size = frame_size;
    for (int s = 0; s<param->steps; ++s)  {
        ia_logt("demix channels with layout %d", param->layout[s]);
        this->cstep = s;
        if (demix_func[param->layout[s]])
            demix_func[param->layout[s]](this, param);
    }

    if (equalizeRMS_func[layout] && param->recon_gain_flag)
        equalizeRMS_func[layout] (this, param);
    /* else { */
        /* ia_logw("Can not support layout (%d) to smooth.", layout); */
    /* } */

    playout = get_layout_channels(layout);
    for (int ch = 0; ch<chs; ++ch) {
        channel = playout[ch];
        if (!this->ch_data[channel]) {
            ia_loge("channel %d doesn't has data.", playout[ch]);
            continue;
        }
        ia_logt("output channel %s(%d) at %p", get_channel_name(channel),
                channel, this->ch_data[channel]);
        memcpy((void *)&out[ch * frame_size], (void *)this->ch_data[channel],
                sizeof(float) * frame_size);
    }
    demixer_update_last_values (this, param);
    return ret;
}

void demixer_destroy(Demixer *this)
{
    ia_logt("Demixer %p", this);
    if (this) {
        free(this);
    }
}
