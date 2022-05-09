#ifndef __CHANNEL_H_
#define __CHANNEL_H_

enum {
    channel_l7,
    channel_r7,
    channel_c,
    channel_lfe,
    channel_sl7,
    channel_sr7,
    channel_bl7,
    channel_br7,
    channel_hfl,
    channel_hfr,
    channel_hbl,
    channel_hbr,
    channel_mono,
    channel_l2,
    channel_r2,
    channel_tl,
    channel_tr,
    channel_l3,
    channel_r3,
    channel_sl5,
    channel_sr5,
    channel_hl,
    channel_hr,
    channel_cnt,

    channel_l5 = channel_l7,
    channel_r5 = channel_r7,
};

enum {
    rg_channel_l,
    rg_channel_c,
    rg_channel_r,
    rg_channel_ls,
    rg_channel_lss = rg_channel_ls,
    rg_channel_rs,
    rg_channel_rss = rg_channel_rs,
    rg_channel_ltf,
    rg_channel_rtf,

    rg_channel_lb,
    rg_channel_lrs = rg_channel_lb,
    rg_channel_rb,
    rg_channel_rrs = rg_channel_rb,
    rg_channel_ltb,
    rg_channel_ltr = rg_channel_ltb,
    rg_channel_rtb,
    rg_channel_rtr = rg_channel_rtb,
    rg_channel_lfe,

    rg_channel_cnt
};

int get_layout_channel_count (int type);
const uint8_t* get_layout_channels (int type);
const char* get_channel_name (uint32_t ch);
const char* get_recon_gain_channel_name (uint32_t ch);


#endif /* __CHANNEL_H_ */
