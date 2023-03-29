/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file mp4mux.h
 * @brief mp4/fmp4 mux function
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef MP4MUX_H
#define MP4MUX_H

#include <stdint.h>

#include "bitstreamrw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t atom_type;
  void *opaque;
} avio_context;

#ifndef DEMIXING_MATRIX_SIZE_MAX
#define DEMIXING_MATRIX_SIZE_MAX (18 * 18 * 2)
#endif

#ifndef MAX_CHANNEL_LAYOUTS
#define MAX_CHANNEL_LAYOUTS 9
#endif

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 12
#endif

typedef enum { CODEC_UNKNOWN, CODEC_OPUS, CODEC_AAC, CODEC_MAX } CODEC_TYPE;

// immersive audio option for flags
#define IA_MOV_FLAG_RTP_HINT (1 << 0)
#define IA_MOV_FLAG_FRAGMENT (1 << 1)
#define IA_MOV_FLAG_EMPTY_MOOV (1 << 2)
#define IA_MOV_FLAG_FRAG_KEYFRAME (1 << 3)
#define IA_MOV_FLAG_SEPARATE_MOOF (1 << 4)
#define IA_MOV_FLAG_FRAG_CUSTOM (1 << 5)
#define IA_MOV_FLAG_ISML (1 << 6)
#define IA_MOV_FLAG_FASTSTART (1 << 7)
#define IA_MOV_FLAG_OMIT_TFHD_OFFSET (1 << 8)
#define IA_MOV_FLAG_DISABLE_CHPL (1 << 9)
#define IA_MOV_FLAG_DEFAULT_BASE_MOOF (1 << 10)
#define IA_MOV_FLAG_DASH (1 << 11)
#define IA_MOV_FLAG_FRAG_DISCONT (1 << 12)
#define IA_MOV_FLAG_DELAY_MOOV (1 << 13)
#define IA_MOV_FLAG_GLOBAL_SIDX (1 << 14)
#define IA_MOV_FLAG_WRITE_COLR (1 << 15)
#define IA_MOV_FLAG_WRITE_GAMA (1 << 16)
#define IA_MOV_FLAG_USE_MDTA (1 << 17)
#define IA_MOV_FLAG_SKIP_TRAILER (1 << 18)
#define IA_MOV_FLAG_NEGATIVE_CTS_OFFSETS (1 << 19)
#define IA_MOV_FLAG_FRAG_EVERY_FRAME (1 << 20)
#define IA_MOV_FLAG_SKIP_SIDX (1 << 21)
#define IA_MOV_FLAG_CMAF (1 << 22)
#define IA_MOV_FLAG_PREFER_ICC (1 << 23)

typedef struct AVPacket {
  int size;
  uint8_t *buf;
  int samples;
} AVPacket;

typedef struct {
  uint32_t samplerate;
  uint32_t samples;  // total sound samples
  uint32_t channels;
  uint32_t bits;  // sample depth
  // buffer config
  uint16_t buffersize;
  struct {
    uint32_t max;
    uint32_t avg;
    int size;
    int samples;
  } bitrate;
  uint32_t framesamples;  // samples per frame
  uint32_t sample_size;   // variable bitrate mode = 0, constant bitrate mode =
                          // size > 0
  // AudioSpecificConfig data:
  struct {
    uint8_t *data;
    unsigned long size;
  } asc;

  struct {
    uint32_t *offsets;  // (stco)
    uint32_t *sizes;    // (stsz)
    uint32_t *deltas;   // (stts)
    uint32_t entries;
    uint32_t buffersize;

    // add for fragment mp4
    uint32_t start_dts;
    uint32_t moofoffset;
    uint32_t trun_dataoffset;
    uint32_t
        fragments;  // the ordinal number of this fragment, in increasing order
    uint32_t fmdat_size;
    uint32_t fmdat_offset;
    unsigned char *fmdat;
  } frame;

  struct {
    // codec specific
    void *csc;
    // version
    int version;
    // profile_version
    int profile_version;
    // roll_distance
    int roll_distance;
    // initial padding
    int initial_padding;
  } iamf;

  struct {
    int size_of_mix_presentations_group_entry;
    uint8_t *mix_presentations_group_entry;

    int size_of_audio_elements_group_entry;
    uint8_t *audio_elements_group_entry;

    uint8_t *data;
    int size;

    int is_changed;
  } ia_descriptor;

} mov_audio_track;

typedef struct {
  FILE *mp4file;
  avio_context *atom;

  mov_audio_track *audio_trak;  // audio track
  uint32_t mdatoffset;
  uint32_t mdatsize;
  uint32_t moovoffset;

  int num_video_traks;
  int num_audio_traks;
  int audio_trak_select;
  int video_trak_select;

  int codec_id;  // 1:opus, 2:aac

  // below is for fragment mp4 writing.
  int flags;  // mp4/fmp4
  uint32_t max_fragment_duration;

} MOVMuxContext;

enum { ERROR_NONE = 0, ERROR_FAIL = -1, ERROR_NOTSUPPORT = -2 };

MOVMuxContext *mov_write_open(char *filename);
int mov_write_head(MOVMuxContext *movm);
int mov_write_trak(MOVMuxContext *movm, int num_video_traks,
                   int num_audio_traks);
int mov_write_audio(MOVMuxContext *movm, int trak, uint8_t *buffer, int size,
                    int samples);
int mov_write_audio2(MOVMuxContext *movm, int trak, AVPacket *pkt);
int mp4_flush_segment(MOVMuxContext *movm);
int mov_write_tail(MOVMuxContext *movm);
int mov_write_close(MOVMuxContext *movm);

#ifdef __cplusplus
}
#endif

#endif
