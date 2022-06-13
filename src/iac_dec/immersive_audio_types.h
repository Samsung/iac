#ifndef _IMMERSIVE_AUDIO_TYPES_H_
#define _IMMERSIVE_AUDIO_TYPES_H_

typedef enum {
    IA_CH_RE_L,
    IA_CH_RE_C,
    IA_CH_RE_R,
    IA_CH_RE_LS,
    IA_CH_RE_RS,
    IA_CH_RE_LTF,
    IA_CH_RE_RTF,
    IA_CH_RE_LB,
    IA_CH_RE_RB,
    IA_CH_RE_LTB,
    IA_CH_RE_RTB,
    IA_CH_RE_LFE,
    IA_CH_RE_COUNT,

    IA_CH_RE_RSS = IA_CH_RE_RS,
    IA_CH_RE_LSS = IA_CH_RE_LS,
    IA_CH_RE_RTR = IA_CH_RE_RTB,
    IA_CH_RE_LTR = IA_CH_RE_LTB,
    IA_CH_RE_RSR = IA_CH_RE_RB,
    IA_CH_RE_LRS = IA_CH_RE_LB,
} IAReconChannel;

typedef enum {
    IA_CH_INVALID,
    IA_CH_L7,
    IA_CH_R7,
    IA_CH_C,
    IA_CH_LFE,
    IA_CH_SL7,
    IA_CH_SR7,
    IA_CH_BL7,
    IA_CH_BR7,
    IA_CH_HFL,
    IA_CH_HFR,
    IA_CH_HBL,
    IA_CH_HBR,
    IA_CH_MONO,
    IA_CH_L2,
    IA_CH_R2,
    IA_CH_TL,
    IA_CH_TR,
    IA_CH_L3,
    IA_CH_R3,
    IA_CH_SL5,
    IA_CH_SR5,
    IA_CH_HL,
    IA_CH_HR,
    IA_CH_COUNT,

    IA_CH_L5 = IA_CH_L7,
    IA_CH_R5 = IA_CH_R7,
} IAChannel;

typedef enum {
    AUDIO_FRAME_PLANE   =   0x1,
    AUDIO_FRAME_FLOAT   =   0x2,
} AFlag;


#define U8_MASK     0xFF
#define U16_MASK    0xFFFF

#define IA_CH_LAYOUT_MAX_CHANNELS     12

#define OPUS_FRAME_SIZE     960
#define AAC_FRAME_SIZE      1024

#define MAX_STREAMS         255
#define MAX_FRAME_SIZE      OPUS_FRAME_SIZE * 6

/**
 * opus delay : pre-skip 312
 * */
#define OPUS_DELAY          312

/**
 * aac delay : 1024 + 1024 + 576 + 144;
 * framing delay + MDCT delay + block switching delay (fl/2 + fs/2). fs = fl / 8.
 * fl???
 * */
#define AAC_DELAY           2768


#endif /* _IMMERSIVE_AUDIO_TYPES_H_ */
