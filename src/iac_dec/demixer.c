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

#define _USE_MATH_DEFINES
#include <math.h>

#include "demixer.h"
#include "immersive_audio_debug.h"
#include "immersive_audio_decoder.h"
#include "immersive_audio_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IADMX"

typedef struct {
    float w_x;
    float w_z;
} w_info;

/**
 * alpha and beta are gain values used for S7to5 down-mixer,
 * gamma for T4to2 downmixer,
 * delta for S5to3 down-mixer and
 * w_idx_offset is the offset to generate a gain value w used for T2toTF2 down-mixer.
 * */

static struct DemixingTypeMat {
    float alpha;
    float beta;
    float gamma;
    float delta;
    int   w_idx_offset;
} demixing_type_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1},
    {0.707, 0.707, 0.707, 0.707, -1},
    {1.0, 0.866, 0.866, 0.866, -1},
    {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1},
    {0.707, 0.707, 0.707, 0.707, 1},
    {1.0, 0.866, 0.866, 0.866, 1},
    {0, 0, 0, 0, 0}
};

struct Demixer {
    float  *ch_data[IA_CH_COUNT];
    int     last_dmixtypenum;
    float   last_weight_state_value_x_prev; // n-1 packet
    float   last_weight_state_value_x_prev2; // n-2 packet

    int     frame_size;
    int     skip;
    float  *hanning_filter;
    float  *start_window;
    float  *stop_window;
    float  *large_buffer;
    float   ch_last_sf[IA_CH_COUNT];
    float   ch_last_sfavg[IA_CH_COUNT];

    IAChannelLayoutType     layout;
    IAChannel               chs_in[IA_CH_LAYOUT_MAX_CHANNELS];
    IAChannel               chs_out[IA_CH_LAYOUT_MAX_CHANNELS];
    int                     chs_count;

    struct {
        struct {
            IAChannel   ch;
            float       gain;
        }               ch_gain[IA_CH_LAYOUT_MAX_CHANNELS];
        uint8_t         count;
    }                   chs_gain_list;

    int                 demixing_mode;
    struct {
        struct {
            IAChannel   ch;
            float       recon_gain;
        }               ch_recon_gain[IA_CH_RE_COUNT];
        uint8_t         count;
        uint32_t        flags;
    }                   chs_recon_gain_list;
};


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


enum {
    CH_MX_S_L,
    CH_MX_S_R,
    CH_MX_S5_L,
    CH_MX_S5_R,
    CH_MX_T_L,
    CH_MX_T_R,
    CH_MX_COUNT
};


static int dmx_s2 (Demixer *ths)
{
    if (!ths->ch_data[IA_CH_L2])
        return IA_ERR_INTERNAL;

    /**
     * S1to2 de-mixer: R2 = 2 x Mono - L2
     * */
    if (!ths->ch_data[IA_CH_R2]) {
        float      *r = 0;

        if (!ths->ch_data[IA_CH_MONO])
            return IA_ERR_INTERNAL;

        ia_logd("---- s1to2 ----");

        r = &ths->large_buffer[ths->frame_size * CH_MX_S_R];

        for (int i=0; i<ths->frame_size; ++i)
            r[i] = 2 * ths->ch_data[IA_CH_MONO][i] - ths->ch_data[IA_CH_L2][i];

        ths->ch_data[IA_CH_R2] = r;

        ia_logd("reconstructed channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_R2), IA_CH_R2, ths->ch_data[IA_CH_R2],
                ths->large_buffer);
    }
    return 0;
}

static int dmx_s3 (Demixer *ths)
{
    /**
     * S2to3 de-mixer: L3 = L2 - 0.707 x C and R3 = R2 - 0.707 x C
     * */
    if (!ths->ch_data[IA_CH_R3]) {
        float       *l, *r;
        uint32_t    fs = ths->frame_size;

        if (dmx_s2(ths))
            return IA_ERR_INTERNAL;

        if (!ths->ch_data[IA_CH_C])
            return IA_ERR_INTERNAL;

        ia_logt("---- s2to3 ----");

        l = &ths->large_buffer[CH_MX_S_L * fs];
        r = &ths->large_buffer[CH_MX_S_R * fs];

        for (int i=0; i<fs; i++) {
            l[i] = ths->ch_data[IA_CH_L2][i] - 0.707 * ths->ch_data[IA_CH_C][i];
            r[i] = ths->ch_data[IA_CH_R2][i] - 0.707 * ths->ch_data[IA_CH_C][i];
        }
        ths->ch_data[IA_CH_L3] = l;
        ths->ch_data[IA_CH_R3] = r;

        ia_logd("reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_L3), IA_CH_L3, ths->ch_data[IA_CH_L3],
                ia_channel_name(IA_CH_R3), IA_CH_R3, ths->ch_data[IA_CH_R3],
                ths->large_buffer);
    }

    return 0;
}


static int dmx_s5(Demixer *ths)
{
    /**
     * S3to5 de-mixer: Ls = 1/δ(k) x (L3 - L5) and Rs = 1/δ(k) x (R3 - R5)
     * */
    if (!ths->ch_data[IA_CH_SR5]) {
        float       *l, *r;
        uint32_t    fs = ths->frame_size;

        int Typeid = ths->demixing_mode;
        int last_Typeid = ths->last_dmixtypenum;
        int i = 0;

        if (dmx_s3(ths))
            return IA_ERR_INTERNAL;

        if (!ths->ch_data[IA_CH_L5] || !ths->ch_data[IA_CH_R5])
            return IA_ERR_INTERNAL;

        ia_logt("---- s3to5 ----");
        ia_logd("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

        l = &ths->large_buffer[CH_MX_S5_L * fs];
        r = &ths->large_buffer[CH_MX_S5_R * fs];

        for (; i < ths->skip; i++) {
            l[i] = (ths->ch_data[IA_CH_L3][i] - ths->ch_data[IA_CH_L5][i]) / demixing_type_mat[last_Typeid].delta;
            r[i] = (ths->ch_data[IA_CH_R3][i] - ths->ch_data[IA_CH_R5][i]) / demixing_type_mat[last_Typeid].delta;
        }

        for (; i < fs; i++) {
            l[i] = (ths->ch_data[IA_CH_L3][i] - ths->ch_data[IA_CH_L5][i]) / demixing_type_mat[Typeid].delta;
            r[i] = (ths->ch_data[IA_CH_R3][i] - ths->ch_data[IA_CH_R5][i]) / demixing_type_mat[Typeid].delta;
        }

        ths->ch_data[IA_CH_SL5] = l;
        ths->ch_data[IA_CH_SR5] = r;

        ia_logd("reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_SL5), IA_CH_SL5, ths->ch_data[IA_CH_SL5],
                ia_channel_name(IA_CH_SR5), IA_CH_SR5, ths->ch_data[IA_CH_SR5],
                ths->large_buffer);
    }
    return 0;
}

static int dmx_s7(Demixer *ths)
{
    /**
     * S5to7 de-mixer: Lrs = 1/β(k) x (Ls - α(k) x Lss) and Rrs = 1/β(k) x (Rs - α(k) x Rss)
     * */
    if (!ths->ch_data[IA_CH_BR7]) {
        float       *l, *r;
        uint32_t    fs = ths->frame_size;

        int i=0;
        int Typeid = ths->demixing_mode;
        int last_Typeid = ths->last_dmixtypenum;

        if (dmx_s5(ths) < 0)
            return IA_ERR_INTERNAL;

        if (!ths->ch_data[IA_CH_SL7] || !ths->ch_data[IA_CH_SR7])
            return IA_ERR_INTERNAL;


        ia_logt("---- s5to7 ----");
        ia_logd("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

        l = &ths->large_buffer[CH_MX_S_L * fs];
        r = &ths->large_buffer[CH_MX_S_R * fs];

        for (; i < ths->skip; i++) {
            l[i] = (ths->ch_data[IA_CH_SL5][i] - ths->ch_data[IA_CH_SL7][i] * demixing_type_mat[last_Typeid].alpha) / demixing_type_mat[last_Typeid].beta;
            r[i] = (ths->ch_data[IA_CH_SR5][i] - ths->ch_data[IA_CH_SR7][i] * demixing_type_mat[last_Typeid].alpha) / demixing_type_mat[last_Typeid].beta;
        }

        for (; i < ths->frame_size; i++) {
            l[i] = (ths->ch_data[IA_CH_SL5][i] - ths->ch_data[IA_CH_SL7][i] * demixing_type_mat[Typeid].alpha) / demixing_type_mat[Typeid].beta;
            r[i] = (ths->ch_data[IA_CH_SR5][i] - ths->ch_data[IA_CH_SR7][i] * demixing_type_mat[Typeid].alpha) / demixing_type_mat[Typeid].beta;
        }

        ths->ch_data[IA_CH_BL7] = l;
        ths->ch_data[IA_CH_BR7] = r;

        ia_logd("reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_BL7), IA_CH_BL7, ths->ch_data[IA_CH_BL7],
                ia_channel_name(IA_CH_BR7), IA_CH_BR7, ths->ch_data[IA_CH_BR7],
                ths->large_buffer);
    }
    return 0;
}


static int dmx_h2 (Demixer *ths)
{
    /**
     * TF2toT2 de-mixer: Ltf2 = Ltf3 - w(k) x (L3 - L5) and Rtf2 = Rtf3 - w(k) x (R3 - R5)
     * */
    if (!ths->ch_data[IA_CH_HR]) {
        float       *l, *r;
        uint32_t    fs = ths->frame_size;
        int i=0;

        int Typeid = ths->demixing_mode;
        int last_Typeid = ths->last_dmixtypenum;
        int32_t WeightTypeNum;

        if (!ths->ch_data[IA_CH_TL] || !ths->ch_data[IA_CH_TR])
            return IA_ERR_INTERNAL;

        if (dmx_s5(ths))
            return IA_ERR_INTERNAL;


        WeightTypeNum = demixing_type_mat[ths->demixing_mode].w_idx_offset;
        w_info w = calc_w_v2(WeightTypeNum, ths->last_weight_state_value_x_prev);
        w_info last_w =
            calc_w_v2(demixing_type_mat[last_Typeid].w_idx_offset, ths->last_weight_state_value_x_prev2);

        ia_logt("---- hf2to2 ----");
        ia_logd("Typeid %d, WeightTypeNum %d, w_x %f, w_z %f, Lasttypeid %d, last_w_x %f, last_w_z %f",
                Typeid, WeightTypeNum, w.w_x, w.w_z, last_Typeid, last_w.w_x, last_w.w_z);

        l = &ths->large_buffer[CH_MX_T_L * fs];
        r = &ths->large_buffer[CH_MX_T_R * fs];

        for (; i < ths->skip; i++) {
            l[i] = ths->ch_data[IA_CH_TL][i] -
                demixing_type_mat[last_Typeid].delta * last_w.w_z * ths->ch_data[IA_CH_SL5][i];
            r[i] = ths->ch_data[IA_CH_TR][i] -
                demixing_type_mat[last_Typeid].delta * last_w.w_z * ths->ch_data[IA_CH_SR5][i];
        }

        for (; i < ths->frame_size; i++) {
            l[i] = ths->ch_data[IA_CH_TL][i] -
                demixing_type_mat[Typeid].delta * w.w_z * ths->ch_data[IA_CH_SL5][i];
            r[i] = ths->ch_data[IA_CH_TR][i] -
                demixing_type_mat[Typeid].delta * w.w_z * ths->ch_data[IA_CH_SR5][i];
        }

        ths->ch_data[IA_CH_HL] = l;
        ths->ch_data[IA_CH_HR] = r;

        ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_HL), IA_CH_HL, ths->ch_data[IA_CH_HL],
                ia_channel_name(IA_CH_HR), IA_CH_HR, ths->ch_data[IA_CH_HR],
                ths->large_buffer);

    }
    return 0;
}

static int dmx_h4 (Demixer *ths)
{
    /**
     * Ltb = 1/γ(k) x (Ltf2 - Ltf4) and Rtb = 1/γ(k) x (Rtf2 - Rtf4)
     * */
    if (!ths->ch_data[IA_CH_HBR]) {
        float       *l, *r;
        uint32_t    fs = ths->frame_size;
        int i=0;
        int Typeid = ths->demixing_mode;
        int last_Typeid = ths->last_dmixtypenum;

        if (dmx_h2(ths))
            return IA_ERR_INTERNAL;

        if (!ths->ch_data[IA_CH_HFR] || !ths->ch_data[IA_CH_HFL])
            return IA_ERR_INTERNAL;


        ia_logt("---- h2to4 ----");
        ia_logd("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

        l = &ths->large_buffer[CH_MX_T_L * fs];
        r = &ths->large_buffer[CH_MX_T_R * fs];

        for (; i < ths->skip; i++) {
            l[i] = (ths->ch_data[IA_CH_HL][i] - ths->ch_data[IA_CH_HFL][i]) / demixing_type_mat[last_Typeid].gamma;
            r[i] = (ths->ch_data[IA_CH_HR][i] - ths->ch_data[IA_CH_HFR][i]) / demixing_type_mat[last_Typeid].gamma;
        }

        for (; i < ths->frame_size; i++) {
            l[i] = (ths->ch_data[IA_CH_HL][i] - ths->ch_data[IA_CH_HFL][i]) / demixing_type_mat[Typeid].gamma;
            r[i] = (ths->ch_data[IA_CH_HR][i] - ths->ch_data[IA_CH_HFR][i]) / demixing_type_mat[Typeid].gamma;
        }

        ths->ch_data[IA_CH_HBL] = l;
        ths->ch_data[IA_CH_HBR] = r;

        ia_logt("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
                ia_channel_name(IA_CH_HBL), IA_CH_HBL, ths->ch_data[IA_CH_HBL],
                ia_channel_name(IA_CH_HBR), IA_CH_HBR, ths->ch_data[IA_CH_HBR],
                ths->large_buffer);

    }
    return 0;
}

static int dmx_channel (Demixer *ths, IAChannel ch)
{
    int ret = IA_ERR_INTERNAL;
    ia_logd ("demix channel %s(%d) pos %p", ia_channel_name(ch), ch,
            ths->ch_data[ch]);

    if (ths->ch_data[ch])
        return IA_OK;

    switch (ch) {
        case IA_CH_R2:
            ret = dmx_s2(ths);
            break;
        case IA_CH_L3:
        case IA_CH_R3:
            ret = dmx_s3(ths);
            break;
        case IA_CH_SL5:
        case IA_CH_SR5:
            ret = dmx_s5(ths);
            break;
        case IA_CH_BL7:
        case IA_CH_BR7:
            ret = dmx_s7(ths);
            break;
        case IA_CH_HL:
        case IA_CH_HR:
            ret = dmx_h2(ths);
            break;
        case IA_CH_HBL:
        case IA_CH_HBR:
            ret = dmx_h4(ths);
            break;
        default:
            break;
    }
    return ret;
}

static void dmx_gainup (Demixer *ths)
{
    for (int c=0; c<ths->chs_gain_list.count; ++c) {
        for (int i=0; i<ths->frame_size; ++i) {
            if (ths->ch_data[ths->chs_gain_list.ch_gain[c].ch]) {
                ths->ch_data[ths->chs_gain_list.ch_gain[c].ch][i] /=
                    ths->chs_gain_list.ch_gain[c].gain;
            }
        }
    }
}

static int dmx_demix (Demixer *ths)
{
    int chcnt = ia_channel_layout_get_channels_count (ths->layout);

    for (int c=0; c<chcnt; ++c) {
        if (dmx_channel(ths, ths->chs_out[c]) < 0)
            return IA_ERR_INTERNAL;
    }
    return IA_OK;
}

static void dmx_rms (Demixer *ths)
{
    float N = 7; //7 frame
    float sf, sfavg;
    float filtBuf;
    float *out;
    IAChannel ch;

    ia_logt("---- demixer_equalizeRMS ----");

    for (int c = 0; c < ths->chs_recon_gain_list.count; c++) {
        ch = ths->chs_recon_gain_list.ch_recon_gain[c].ch;
        sf = ths->chs_recon_gain_list.ch_recon_gain[c].recon_gain;
        out = ths->ch_data[ch];

        if (N > 0) {
            sfavg = (2 / (N + 1)) * sf + (1 - 2 / (N + 1)) * ths->ch_last_sfavg[ch];
        } else {
            sfavg = sf;
        }

        ia_logd("channel %s(%d) is smoothed within %f.", ia_channel_name(ch),
                ch, sf);
        /* different scale factor in overapping area */
        for (int i = 0; i < ths->frame_size; i++) {
            filtBuf = ths->ch_last_sfavg[ch] *
                ths->stop_window[i] + sfavg * ths->start_window[i];
            out[i] *= filtBuf;
        }

        ths->ch_last_sf[ch] = sf;
        ths->ch_last_sfavg[ch] = sfavg;
    }
}


Demixer *demixer_open(uint32_t frame_size, uint32_t delay)
{
    Demixer *ths = 0;
    IAErrCode ec = IA_OK;
    ths = (Demixer *) ia_mallocz (sizeof(Demixer));
    if (ths) {

        int preskip = delay % frame_size;
        int windowLen = frame_size / 8;
        int overlapLen = windowLen / 2;

        if (preskip < overlapLen) {
            ec = IA_ERR_UNIMPLEMENTED;
            ia_loge ("%s : I don't know how to do for overlab more than preskip.",
                    ia_error_code_string(ec));
            goto termination;
        }

        ia_logt("Demixer %p size %ld", ths, sizeof(Demixer));

        ths->last_dmixtypenum = 0;
        ths->last_weight_state_value_x_prev = 0.0; // n-1 packet
        ths->last_weight_state_value_x_prev2 = 0.0; // n-2 packet
        ths->frame_size = frame_size;
        ths->skip = preskip;
        ths->layout = IA_CHANNEL_LAYOUT_INVALID;

        ths->hanning_filter = (float *)ia_mallocz(sizeof (float) * windowLen);
        ths->start_window = (float *)malloc(sizeof (float) * frame_size);
        ths->stop_window = (float *)malloc(sizeof (float) * frame_size);
        ths->large_buffer =
            (float *)malloc(sizeof (float) * CH_MX_COUNT * frame_size);

        if (!ths->hanning_filter || !ths->start_window || !ths->stop_window
                || !ths->large_buffer) {
            ec = IA_ERR_ALLOC_FAIL;
            ia_loge ("%s : hanning window, start & stop windows.",
                    ia_error_code_string(ec));
            goto termination;
        }
        /**
         * init hanning window.
         * */
        for (int i = 0; i < windowLen; i++)
            ths->hanning_filter[i] = (0.5 * (1.0 - cos(2.0*M_PI*(double)i / (double)(windowLen - 1))));

        for (int i = 0; i < preskip - overlapLen; i++) {
            ths->start_window[i] = 0;
            ths->stop_window[i] = 1;
        }

        for (int i = preskip - overlapLen, j = 0; i < preskip; i++, j++) {
            ths->start_window[i] = ths->hanning_filter[j];
            ths->stop_window[i] = ths->hanning_filter[j+overlapLen];
        }

        for (int i = preskip; i < frame_size; i++) {
            ths->start_window[i] = 1;
            ths->stop_window[i] = 0;
        }

        for (int i=0; i<IA_CH_COUNT; ++i) {
            ths->ch_last_sf[i] = 1.0;
            ths->ch_last_sfavg[i] = 1.0;
        }
    }

termination:

    if (ec != IA_OK) {
        demixer_close (ths);
        ths = 0;
    }
    return ths;

}


void demixer_close (Demixer *ths)
{
    if (ths->hanning_filter)
        free (ths->hanning_filter);

    if (ths->start_window)
        free (ths->start_window);

    if (ths->stop_window)
        free (ths->stop_window);

    if (ths->large_buffer)
        free (ths->large_buffer);
    free (ths);
}


int demixer_set_channel_layout (Demixer *ths, IAChannelLayoutType layout)
{
    if (ia_channel_layout_get_channels(layout, ths->chs_out,
                IA_CH_LAYOUT_MAX_CHANNELS) > 0) {
        ths->layout = layout;
        return IA_OK;
    }
    return IA_ERR_BAD_ARG;
}


int demixer_set_channels_order (Demixer *ths, IAChannel *chs, int count)
{
    memcpy (ths->chs_in, chs, sizeof(IAChannel) * count);
    ths->chs_count = count;
    return IA_OK;
}

int demixer_set_output_gain (Demixer *ths, IAChannel *chs, float *gain,
                             int count)
{
    for (int i=0; i<count; ++i) {
        ths->chs_gain_list.ch_gain[i].ch = chs[i];
        ths->chs_gain_list.ch_gain[i].gain = gain[i];
        ia_logi("channel %s(%d) gain is %f", ia_channel_name(chs[i]), chs[i],
                gain[i]);
    }
    ths->chs_gain_list.count = count;
    return IA_OK;
}

int demixer_set_demixing_mode (Demixer *ths, int mode)
{
    ia_logd("dmixtypenum: %d -> %d", ths->last_dmixtypenum, mode);
    ia_logd("weight_state_value_x_prev2: %f -> %f",
            ths->last_weight_state_value_x_prev2,
            ths->last_weight_state_value_x_prev);

    ths->last_dmixtypenum = ths->demixing_mode;
    ths->last_weight_state_value_x_prev2 = ths->last_weight_state_value_x_prev;

    w_info w = calc_w_v2(demixing_type_mat[ths->demixing_mode].w_idx_offset,
            ths->last_weight_state_value_x_prev);
    ia_logd("last_weight_state_value_x_prev: %f -> %f",
            ths->last_weight_state_value_x_prev, w.w_x);
    ths->last_weight_state_value_x_prev = w.w_x;

    ths->demixing_mode = mode;
    return 0;
}

int demixer_set_recon_gain (Demixer *ths, int count,
                            IAChannel *chs, float *recon_gain, uint32_t flags)
{
    if (flags && flags ^ ths->chs_recon_gain_list.flags) {
        for (int i=0; i<count; ++i) {
            ths->chs_recon_gain_list.ch_recon_gain[i].ch = chs[i];
        }
        ths->chs_recon_gain_list.count = count;
    }
    for (int i=0; i<count; ++i) {
        ths->chs_recon_gain_list.ch_recon_gain[i].recon_gain = recon_gain[i];
    }
    return IA_OK;
}

int demixer_demixing (Demixer *ths, float *dst, float *src, uint32_t size)
{
    IAChannel ch;

    if (size != ths->frame_size)
        return IA_ERR_BAD_ARG;

    if (ia_channel_layout_get_channels_count (ths->layout) != ths->chs_count)
        return IA_ERR_INTERNAL;

    memset (ths->ch_data, 0, sizeof (float *) * IA_CH_COUNT);
    for (int c=0; c<ths->chs_count; ++c) {
        ths->ch_data[ths->chs_in[c]] = src + size * c;
    }

    ia_logt ("prepare.");
    dmx_gainup(ths);
    ia_logt ("gainup.");
    if (dmx_demix (ths) < 0)
        return IA_ERR_INTERNAL;
    ia_logt ("demix.");
    dmx_rms(ths);
    ia_logt ("rms.");

    for (int c = 0; c<ths->chs_count; ++c) {
        ch = ths->chs_out[c];
        if (!ths->ch_data[ch]) {
            ia_loge("channel %s(%d) doesn't has data.", ia_channel_name(ch), ch);
            continue;
        }
        ia_logt("output channel %s(%d) at %p",
                ia_channel_name(ch), ch, ths->ch_data[ch]);
        memcpy((void *)&dst[c * size], (void *)ths->ch_data[ch],
                sizeof(float) * size);
    }
    ia_logt ("reorder.");
    return IA_OK;
}

