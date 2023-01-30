#ifndef __SCALABLE_FORMAT_H_
#define __SCALABLE_FORMAT_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef enum {
  CHANNEL_LAYOUT_INVALID = -1,
  CHANNEL_LAYOUT_100,         //1.0.0
  CHANNEL_LAYOUT_200,         //2.0.0 
  CHANNEL_LAYOUT_510,         //5.1.0
  CHANNEL_LAYOUT_512,         //5.1.2
  CHANNEL_LAYOUT_514,         //5.1.4
  CHANNEL_LAYOUT_710,         //7.1.0
  CHANNEL_LAYOUT_712,         //7.1.2
  CHANNEL_LAYOUT_714,         //7.1.4
  CHANNEL_LAYOUT_312,         //3.1.2
  CHANNEL_LAYOUT_BINAURAL,    //binaural
  CHANNEL_LAYOUT_MAX
}CHANNEL_LAYOUT_TYPE;

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
  enc_channel_mixed_s1_m,
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

static uint8_t enc_gs_layout_channels[][12] = { // wav Channels (Speaker location orderings)
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5 },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr },
  { enc_channel_l5, enc_channel_r5, enc_channel_c, enc_channel_lfe, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7 },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr },
  { enc_channel_l7, enc_channel_r7, enc_channel_c, enc_channel_lfe, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr },
  { enc_channel_l3, enc_channel_r3, enc_channel_c, enc_channel_lfe, enc_channel_tl, enc_channel_tr },
  { enc_channel_l2, enc_channel_r2 }
};

#if 1
static uint8_t enc_gs_layout_channels2[][12] = { //Channels(Speaker location orderings), used for getting scalable channel order
  { enc_channel_mono },
  { enc_channel_l2, enc_channel_r2 },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_c, enc_channel_lfe },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l5, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_c, enc_channel_lfe},
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_c, enc_channel_lfe},
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hl, enc_channel_hr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l7, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_bl7, enc_channel_br7, enc_channel_hfl, enc_channel_hfr, enc_channel_hbl, enc_channel_hbr, enc_channel_c, enc_channel_lfe},
  { enc_channel_l3, enc_channel_r3, enc_channel_tl, enc_channel_tr, enc_channel_c, enc_channel_lfe },
  { enc_channel_l2, enc_channel_r2 }
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

static int get_recon_gain_flags_map_msb[][14] = { // convert to MSB
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { -1, -1, -1, -1, enc_channel_r2, -1, enc_channel_l2, /*|*/ -1, -1, -1, -1, -1, -1, -1 },
  { -1, -1, enc_channel_sr5, enc_channel_sl5, enc_channel_r5, enc_channel_c, enc_channel_l5, /*|*/ enc_channel_lfe, -1, -1, -1, -1, -1, -1  },
  { enc_channel_hr, enc_channel_hl, enc_channel_sr5, enc_channel_sl5, enc_channel_r5, enc_channel_c, enc_channel_l5, /*|*/enc_channel_lfe, -1, -1, -1, -1, -1, -1  },
  { enc_channel_hfr, enc_channel_hfl, enc_channel_sr5, enc_channel_sl5, enc_channel_r5, enc_channel_c, enc_channel_l5, /*|*/enc_channel_lfe, enc_channel_hbr, enc_channel_hbl, -1, -1, -1, -1  },
  { -1, -1, enc_channel_sr7, enc_channel_sl7, enc_channel_r7, enc_channel_c, enc_channel_l7, /*|*/enc_channel_lfe, -1, -1, enc_channel_br7, enc_channel_bl7, -1, -1  },
  { enc_channel_hr, enc_channel_hl, enc_channel_sr7, enc_channel_sl7, enc_channel_r7, enc_channel_c, enc_channel_l7, /*|*/enc_channel_lfe, -1, -1, enc_channel_br7, enc_channel_bl7, -1, -1, /*|*/  },
  { enc_channel_hfr, enc_channel_hfl, enc_channel_sr7, enc_channel_sl7, enc_channel_r7, enc_channel_c, enc_channel_l7, /*|*/enc_channel_lfe, enc_channel_hbr, enc_channel_hbl, enc_channel_br7, enc_channel_bl7, -1, -1  },
  { enc_channel_tr, enc_channel_tl, -1, -1, enc_channel_r3, enc_channel_c, enc_channel_l3, /*|*/enc_channel_lfe, -1, -1, -1, -1, -1, -1  },
  { -1, -1, -1, -1, enc_channel_r2, -1, enc_channel_l2, /*|*/ -1, -1, -1, -1, -1, -1, -1 }
};

static int get_recon_gain_flags_map[][12] = {
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { enc_channel_l2, -1, enc_channel_r2, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, -1, -1, -1, -1, -1, -1, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hl, enc_channel_hr, -1, -1, -1, -1, enc_channel_lfe },
  { enc_channel_l5, enc_channel_c, enc_channel_r5, enc_channel_sl5, enc_channel_sr5, enc_channel_hfl, enc_channel_hfr, -1, -1, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, -1, -1, enc_channel_bl7, enc_channel_br7, -1, -1, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_hl, enc_channel_hr, enc_channel_bl7, enc_channel_br7, -1 , -1, enc_channel_lfe },
  { enc_channel_l7, enc_channel_c, enc_channel_r7, enc_channel_sl7, enc_channel_sr7, enc_channel_hfl, enc_channel_hfr, enc_channel_bl7, enc_channel_br7, enc_channel_hbl, enc_channel_hbr, enc_channel_lfe },
  { enc_channel_l3, enc_channel_c, enc_channel_r3, -1, -1, enc_channel_tl, enc_channel_tr, -1, -1, -1, -1, enc_channel_lfe },
  { enc_channel_l2, -1, enc_channel_r2, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
};

static int get_recon_gain_value_map[][12] = {
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },// mono
  {  0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },// 2.0.0
  {  0,  2,  1,  4,  5, -1, -1, -1, -1, -1, -1,  3 },// 5.1.0
  {  0,  2,  1,  4,  5,  6,  7, -1, -1, -1, -1,  3 },// 5.1.2
  {  0,  2,  1,  4,  5,  6,  7, -1, -1,  8,  9,  3 },// 5.1.4
  {  0,  2,  1,  4,  5, -1, -1,  6,  7, -1, -1,  3 },// 7.1.0
  {  0,  2,  1,  4,  5,  8,  9,  6,  7, -1, -1,  3 },// 7.1.2
  {  0,  2,  1,  4,  5,  8,  9,  6,  7, 10, 11,  3 },// 7.1.4
  {  0,  2,  1, -1, -1,  4,  5, -1, -1, -1, -1,  3 },// 3.1.2
  { 0, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // binaural
};

static int get_output_gain_flags_map[] ={
  5/*Mono*/, 5/*l2*/, 4/*r2*/, -1/*c*/, -1/*lfe*/, 1/*tl*/, 0/*tr*/, 5/*l3*/, 4/*r3*/, -1/*l5/l7*/, -1/*r5/r7*/,
  3/*sl5*/, 2/*sr5*/, 1/*hl*/, 0/*hr*/, -1/*sl7*/, -1/*sr7*/, -1/*hfl*/, -1/*hfr*/, -1/*bl7*/, -1/*br7*/, -1/*hbl*/, 1/*hbr*/
};

static const int enc_gs_layout_channel_count[] = {
  1, 2, 6, 8, 10, 8, 10, 12, 6, 2
};

static const char* enc_gs_ia_channel_name[] = {
  "mono", "l2", "r2", "c", "lfe", "tl", "tr", "l3", "r3", "l5/l7", "r5/r7",
  "sl5", "sr5", "hl", "hr", "sl7", "sr7", "hfl", "hfr", "bl7", "br7", "hbl",
  "hbr"
};

static const char* channel_layout_names[] = {
  "1.0.0", "2.0.0", "5.1.0", "5.1.2", "5.1.4", "7.1.0", "7.1.2", "7.1.4", "3.1.2", "binaural" };

int enc_get_layout_channel_count(int type);
uint8_t* enc_get_layout_channels(int type);
uint8_t* enc_get_layout_channels2(int type);
int enc_has_c_channel(int cnt, uint8_t *channels);
const char* enc_get_channel_name(uint32_t ch);
int enc_get_new_channels2(int base, int target, uint8_t* channels);
int get_surround_channels(int lay_out);
int get_height_channels(int lay_out);
int get_lfe_channels(int lay_out);
#endif
