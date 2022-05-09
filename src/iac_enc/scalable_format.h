#ifndef __SCALABLE_FORMAT_H_
#define __SCALABLE_FORMAT_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef CHANNEL_LAYOUT_INVALID
#define CHANNEL_LAYOUT_INVALID -1
#endif

enum {
  enc_channel_mono,
  enc_channel_l2,
  enc_channel_r2,
  enc_channel_c,
  enc_channel_lfe,
  enc_channel_tl,
  enc_channel_tr,
  enc_channel_l3,
  enc_channel_r3,
  enc_channel_l5,
  enc_channel_r5,
  enc_channel_sl5,
  enc_channel_sr5,
  enc_channel_hl,
  enc_channel_hr,
  enc_channel_sl7,
  enc_channel_sr7,
  enc_channel_hfl,
  enc_channel_hfr,
  enc_channel_bl7,
  enc_channel_br7,
  enc_channel_hbl,
  enc_channel_hbr,
  enc_channel_cnt,

  enc_channel_l7 = enc_channel_l5,
  enc_channel_r7 = enc_channel_r5,
};



enum {
  enc_channel_mixed_s2_l,
  enc_channel_mixed_s2_r,
  enc_channel_mixed_s3_l,
  enc_channel_mixed_s3_r,
  enc_channel_mixed_s5_l,
  enc_channel_mixed_s5_r,
  enc_channel_mixed_s7_l,
  enc_channel_mixed_s7_r,
  enc_channel_mixed_t_l,
  enc_channel_mixed_t_r,
  enc_channel_mixed_h_l,
  enc_channel_mixed_h_r,
  enc_channel_mixed_h_bl,
  enc_channel_mixed_h_br,
  enc_channel_mixed_cnt
};

static int enc_gs_12channel[enc_channel_cnt] = {
  enc_channel_mono,
  enc_channel_bl7, // enc_channel_l2,
  enc_channel_br7, // enc_channel_r2,
  enc_channel_c,
  enc_channel_lfe,
  enc_channel_hbl, // enc_channel_tl,
  enc_channel_hbr, // enc_channel_tr,
  enc_channel_bl7, // enc_channel_l3,
  enc_channel_br7, // enc_channel_r3,
  enc_channel_l7,  // enc_channel_l5,
  enc_channel_r7,  // enc_channel_r5,
  enc_channel_bl7, // enc_channel_sl5,
  enc_channel_br7, // enc_channel_sr5,
  enc_channel_hbl, //  enc_channel_hl,
  enc_channel_hbr, //  enc_channel_hr,
  enc_channel_sl7,
  enc_channel_sr7,
  enc_channel_hfl,
  enc_channel_hfr,
  enc_channel_bl7,
  enc_channel_br7,
  enc_channel_hbl,
  enc_channel_hbr,
};

static uint8_t enc_gs_layout_channels[][12] = { // wav Channels (Speaker location orderings)
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5 },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7 },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_l3, enc_channel_r3, enc_channel_c, enc_channel_lfe, enc_channel_tl, enc_channel_tr }
};

#if 1
static uint8_t enc_gs_layout_channels2[][12] = { //Channels(Speaker location orderings), used for getting scalable channel order
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_c, enc_channel_lfe, enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5 },
  { enc_channel_c, enc_channel_lfe, enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr },
  { enc_channel_c, enc_channel_lfe, enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_c, enc_channel_lfe, enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7 },
  { enc_channel_c, enc_channel_lfe, enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr },
  { enc_channel_c, enc_channel_lfe, enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_c, enc_channel_lfe, enc_channel_l3, enc_channel_r3, enc_channel_tl, enc_channel_tr }
};
#else
static uint8_t enc_gs_layout_channels2[][12] = { //Channels(Speaker location orderings), used for getting scalable channel order
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_c, enc_channel_lfe },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7,enc_channel_c, enc_channel_lfe },
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l3, enc_channel_r3, enc_channel_tl, enc_channel_tr,enc_channel_c, enc_channel_lfe }
};
#endif

static uint8_t enc_gs_vorbis_layout_channels[][12] = { // vorbis Channels (Speaker location orderings)
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l3, enc_channel_c, enc_channel_r3, enc_channel_tl, enc_channel_tr, enc_channel_lfe }
};

static int get_recon_gain_flags_map[][12] = {
  { enc_channel_mono },
  { enc_channel_l2, -1, enc_channel_r2, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, -1, -1, -1, -1, -1, -1, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr, -1, -1, -1, -1, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, -1, -1, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, -1, -1, enc_channel_bl7, enc_channel_br7, -1, -1, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_hl, enc_channel_hr, enc_channel_bl7, enc_channel_br7, -1 , -1, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_hfl, enc_channel_hfr, enc_channel_bl7, enc_channel_br7, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l3, enc_channel_c, enc_channel_r3, -1, -1, enc_channel_tl, enc_channel_tr, -1, -1, -1, -1, enc_channel_lfe }
};

static int get_recon_gain_value_map[][12] = {
  { enc_channel_mono },
  { 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { 0, 2, 1, 4, 5, -1, -1, -1, -1, -1, -1, 3 },
  { 0, 2, 1, 4, 5, 6, 7, -1, -1, -1, -1, 3 },
  { 0, 2, 1, 4, 5, 6, 7, -1, -1, 8, 9, 3 },
  { 0, 2, 1, 4, 5, -1, -1, 6, 7, -1, -1, 3 },
  { 0, 2, 1, 4, 5, 8, 9, 6, 7, -1, -1, 3 },
  { 0, 2, 1, 4, 5, 8, 9, 6, 7, 10, 11, 3 },
  { 0, 2, 1, -1, -1, 4, 5, -1, -1, -1, -1, 3 }
};

static const int enc_gs_layout_channel_count[] = {
  1, 2, 6, 8, 10, 8, 10, 12, 6
};

static const char* enc_gs_ia_channel_name[] = {
  "mono", "l2", "r2", "c", "lfe", "tl", "tr", "l3", "r3", "l5/l7", "r5/r7",
  "sl5", "sr5", "hl", "hr", "sl7", "sr7", "hfl", "hfr", "bl7", "br7", "hbl",
  "hbr"
};

int enc_get_layout_channel_count(int type);
uint8_t* enc_get_layout_channels(int type);
uint8_t* enc_get_layout_channels2(int type);
int enc_convert_12channel(int ch);
int enc_has_c_channel(int cnt, uint8_t *channels);
const char* enc_get_channel_name(uint32_t ch);
int enc_get_new_channels(int base, int target, uint8_t* channels);
#endif
