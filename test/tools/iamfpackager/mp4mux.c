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
 * @file mp4mux.c
 * @brief mp4/fmp4 mux function
 * @version 0.1
 * @date Created 3/3/2023
 **/

// Mp4 muxer sample code.
#if defined(_WIN32)
#include <io.h>
#else
#include <sys/io.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef WORDS_BIGENDIAN
#endif
#include <string.h>
#include <time.h>

#include "a2b_endian.h"
#include "dmemory.h"
#include "mp4mux.h"

// internal flags
#define MOV_TFHD_BASE_DATA_OFFSET 0x01
#define MOV_TFHD_STSD_ID 0x02
#define MOV_TFHD_DEFAULT_DURATION 0x08
#define MOV_TFHD_DEFAULT_SIZE 0x10
#define MOV_TFHD_DEFAULT_FLAGS 0x20
#define MOV_TFHD_DURATION_IS_EMPTY 0x010000
#define MOV_TFHD_DEFAULT_BASE_IS_MOOF 0x020000

#define MOV_TRUN_DATA_OFFSET 0x01
#define MOV_TRUN_FIRST_SAMPLE_FLAGS 0x04
#define MOV_TRUN_SAMPLE_DURATION 0x100
#define MOV_TRUN_SAMPLE_SIZE 0x200
#define MOV_TRUN_SAMPLE_FLAGS 0x400
#define MOV_TRUN_SAMPLE_CTS 0x800

#define IS_FRAGMENT_MP4 (movm->flags & IA_MOV_FLAG_FRAGMENT)

enum MOV_ATOM_TYPE {
  MOV_ATOM_EXIT = 0,
  MOV_ATOM_NAME,
  MOV_ATOM_DOWN,
  MOV_ATOM_UP,
  MOV_ATOM_DATA,
  MOV_ATOM_TYPE_COUNT,
};

#define avio_wdata(a, b) avio_wdata_((movm)->mp4file, a, b)
static int avio_wdata_(FILE *mp4file, const void *data, int size) {
  if (fwrite(data, 1, size, mp4file) != size) {
    perror("mp4out");
    return ERROR_FAIL;
  }
  return size;
}

#define avio_wstring(a) avio_wstring_(movm, a)
static int avio_wstring_(MOVMuxContext *movm, const char *txt) {
  return avio_wdata(txt, strlen(txt));
}

#define avio_wb32(a) avio_wb32_(movm, a)
static int avio_wb32_(MOVMuxContext *movm, uint32_t u32) {
  u32 = be32(u32);
  return avio_wdata(&u32, 4);
}

#define avio_wb16(a) avio_wb16_(movm, a)
static int avio_wb16_(MOVMuxContext *movm, uint16_t u16) {
  u16 = be16(u16);
  return avio_wdata(&u16, 2);
}

#define avio_w8(a) avio_w8_(movm, a)
static int avio_w8_(MOVMuxContext *movm, uint8_t u8) {
  if (fwrite(&u8, 1, 1, movm->mp4file) != 1) {
    perror("mp4 out");
    return 0;
  }
  return 1;
}

#define avio_wb24(a) avio_wb24_(movm, a)
static int avio_wb24_(MOVMuxContext *movm, uint32_t u24) {
  return avio_wb16((int)u24 >> 8) + avio_w8((uint8_t)u24);
}

static int mov_write_ftyp_tag(MOVMuxContext *movm) {
  int size = 0;

  size += avio_wstring("isom");
  size += avio_wb32(0);
  size += avio_wstring("iamf");
  size += avio_wstring("isom");
  size += avio_wstring("iso6");

  return size;
}

enum { SECSONEDAY = 24 * 60 * 60 };
static time_t mov_get_time() {
  int y;
  time_t current_t = 10000;

  // time(&current_t);

  for (y = 2000; y < 2022; y++) {
    current_t += 365 * SECSONEDAY;
    if (!(y & 3)) current_t += SECSONEDAY;
  }

  return current_t;
}

#define MOV_TIMESCALE 1000
static int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c) {
  int64_t r = 0;
  r = c - 1;
  return (a * b + r) / c;
}

static int mov_write_mvhd_tag(MOVMuxContext *movm) {  // movie information
  int size = 0;
  int cnt;

  // version
  size += avio_w8(0);
  // flags
  size += avio_w8(0);
  size += avio_wb16(0);
  // Mov creation time
  size += avio_wb32(mov_get_time());
  // Mov modification time
  size += avio_wb32(mov_get_time());
  // Mov time scale
  size += avio_wb32(movm->audio_trak[0].samplerate);
  // size += avio_wb32(MOV_TIMESCALE);
  // Mov duration
  size += avio_wb32(movm->audio_trak[0].samples -
                    movm->audio_trak[0].iamf.initial_padding);
  // size += avio_wb32(av_rescale_rnd(movm->audio_trak[0].samples,
  // MOV_TIMESCALE, movm->audio_trak[0].samplerate));
  // Mov rate
  size += avio_wb32(0x00010000);
  // Mov volume
  size += avio_wb16(0x0100);
  // reserved
  size += avio_wb16(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  // matrix
  size += avio_wb32(0x00010000);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0x00010000);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0x40000000);

  for (cnt = 0; cnt < 6; cnt++) size += avio_wb32(0);
  // Mov Next track ID
  size += avio_wb32(movm->num_video_traks + movm->num_audio_traks + 1);

  return size;
};

static int mov_write_tkhd_tag(MOVMuxContext *movm) {  //
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];
  int size = 0;

  // version
  size += avio_w8(0);
  // Mov flags
  // bits 8-23
  size += avio_wb16(0);
  // Mov bits 0-7
  size += avio_w8(1 /*track enabled */);
  // Mov creation time
  size += avio_wb32(mov_get_time());
  // Mov modification time
  size += avio_wb32(mov_get_time());
  // Mov track ID
  size += avio_wb32(movm->num_video_traks + movm->audio_trak_select + 1);
  // Mov reserved
  size += avio_wb32(0);
  // Mov duration
  size += avio_wb32(audio_t->samples - audio_t->iamf.initial_padding);
  // Mov reserved
  size += avio_wb32(0);
  size += avio_wb32(0);
  // Layer
  size += avio_wb16(0);
  // Mov alternate group
  size += avio_wb16(0);
  // Mov volume
  size += avio_wb16(0x0100);
  // Mov reserved
  size += avio_wb16(0);
  // Movmatrix
  size += avio_wb32(0x00010000);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0x00010000);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0x40000000);

  // Mov track width
  size += avio_wb32(0);
  // Mov track height
  size += avio_wb32(0);

  return size;
};

static int mov_write_edts_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];
  int size = 0;

  // elst
  size += avio_wb32(28);
  size += avio_wstring("elst");
  size += avio_w8(0);   /* version */
  size += avio_wb24(0); /* flags */
  size += avio_wb32(1); /* entry_count */
  // size += avio_wb32(av_rescale_rnd(audio_t->samples, MOV_TIMESCALE,
  // audio_t->samplerate));/* duration */
  size += avio_wb32(audio_t->samples);              /* duration */
  size += avio_wb32(audio_t->iamf.initial_padding); /* media_time */
  size += avio_wb32(0x00010000);                    /* media_rate */
  return size;
}

static int mov_write_mdhd_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  int size = 0;

  // Mov version/flags
  size += avio_wb32(0);
  // Mov creation time
  size += avio_wb32(mov_get_time());
  // Mov modification time
  size += avio_wb32(mov_get_time());
  // Mov time scale
  size += avio_wb32(movm->audio_trak[audio_trak_select].samplerate);
  // Mov duration
  size += avio_wb32(movm->audio_trak[audio_trak_select].samples -
                    movm->audio_trak[0].iamf.initial_padding);
  // Mov language
  size += avio_wb16(0 /*0=English */);
  // Mov pre_defined
  size += avio_wb16(0);

  return size;
};

static int mov_write_hdlr1_tag(MOVMuxContext *movm) {
  int size = 0;

  // Mov version/flags
  size += avio_wb32(0);
  // Mov pre_defined
  size += avio_wb32(0);
  // Mov component subtype
  size += avio_wstring("soun");
  // Mov reserved
  size += avio_wb32(0);
  size += avio_wb32(0);
  size += avio_wb32(0);
  // Mov name
  // Mov null terminate
  size += avio_w8(0);

  return size;
};

static int mov_write_smhd_tag(MOVMuxContext *movm) {
  int size = 0;

  // Mov version/flags
  size += avio_wb32(0);
  // Mov balance
  size += avio_wb16(0 /*center */);
  // reserved
  size += avio_wb16(0);

  return size;
};

static int mov_write_dref_tag(MOVMuxContext *movm) {
  int size = 0;

  // Mov version/flags
  size += avio_wb32(0);
  // Mov number of entries
  size += avio_wb32(1 /*url reference */);

  return size;
};

static int mov_write_url_tag(MOVMuxContext *movm) {
  int size = 0;

  size += avio_wb32(1);

  return size;
};

static int mov_write_multi_entries(MOVMuxContext *movm) {
  int size = 0;
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = &movm->audio_trak[audio_trak_select];
  int cnt = audio_t->descriptor_entries;

  for (int i = 0; i < cnt; i++) {
    int64_t pos_iamf = ftell(movm->mp4file);
    int size_iamf = 0;
    size_iamf += avio_wb32(0);  // write iamf size
    size_iamf += avio_wstring("iamf");
    // Reserved (6 bytes)
    size_iamf += avio_wb32(0);
    size_iamf += avio_wb16(0);
    // Data reference index
    size_iamf += avio_wb16(1);
    // Version
    size_iamf += avio_wb16(0);
    // Revision level
    size_iamf += avio_wb16(0);
    // Vendor
    size_iamf += avio_wb32(0);
    // Number of channels
    size_iamf += avio_wb16(audio_t->channels);
    // Sample size (bits)
    size_iamf += avio_wb16(audio_t->bits);
    // Compression ID
    size_iamf += avio_wb16(0);
    // Packet size
    size_iamf += avio_wb16(0);
    // Sample rate (16.16)
    // rate integer part
    size_iamf += avio_wb16(audio_t->samplerate);
    // rate reminder part
    size_iamf += avio_wb16(0);

    int64_t pos_iamd = ftell(movm->mp4file);
    int size_iamd = 0;
    size_iamd += avio_wb32(0);  // write iamd size
    size_iamd += avio_wstring("iamd");
    size_iamd += avio_wdata(audio_t[0].av_descriptor[i].data,
                            audio_t[0].av_descriptor[i].size);
    fseek(movm->mp4file, pos_iamd, SEEK_SET);
    avio_wb32(size_iamd);
    size_iamf += size_iamd;
    fseek(movm->mp4file, pos_iamf, SEEK_SET);
    avio_wb32(size_iamf);
    fseek(movm->mp4file, pos_iamf + size_iamf, SEEK_SET);
    size += size_iamf;
  }

  return size;
}

static int mov_write_entries(MOVMuxContext *movm);

static int mov_write_stsd_tag(MOVMuxContext *movm) {
  int size = 0;
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  // Movversion/flags
  size += avio_wb32(0);
  // Mov number of entries
  size += avio_wb32(movm->audio_trak[audio_trak_select].descriptor_entries);

  // size += mov_write_multi_entries(movm);
  avio_context *pre_atom = movm->atom;

  size += mov_write_entries(movm);
  movm->atom = pre_atom;
  return size;
};

static int mov_write_mp4a_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  int size = 0;
  // reserved (6 bytes)
  size += avio_wb32(0);
  size += avio_wb16(0);
  // Mov data reference index
  size += avio_wb16(1);
  // Mov version
  size += avio_wb16(0);
  // Mov revision level
  size += avio_wb16(0);
  // Mov vendor
  size += avio_wb32(0);
  // Mov number of channels
  size += avio_wb16(movm->audio_trak[audio_trak_select].channels);
  // Mov sample size (bits)
  size += avio_wb16(movm->audio_trak[audio_trak_select].bits);
  // Mov compression ID
  size += avio_wb16(0);
  // packet size
  size += avio_wb16(0);
  // sample rate (16.16)
  // rate integer part
  size += avio_wb16(movm->audio_trak[audio_trak_select].samplerate);
  // rate reminder part
  size += avio_wb16(0);

  return size;
}

static int mov_write_iamf_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = &movm->audio_trak[audio_trak_select];

  int size = 0;
  // Reserved (6 bytes)
  size += avio_wb32(0);
  size += avio_wb16(0);
  // Data reference index
  size += avio_wb16(1);
  // Version
  size += avio_wb16(0);
  // Revision level
  size += avio_wb16(0);
  // Vendor
  size += avio_wb32(0);
  // Number of channels
  size += avio_wb16(audio_t->channels);
  // Sample size (bits)
  size += avio_wb16(audio_t->bits);
  // Compression ID
  size += avio_wb16(0);
  // Packet size
  size += avio_wb16(0);
  // Sample rate (16.16)
  // rate integer part
  size += avio_wb16(audio_t->samplerate);
  // rate reminder part
  size += avio_wb16(0);

  /*
    uint8_t bitstr[256] = { 0, };
    bitstream_t bs;
    bs_init(&bs, bitstr, sizeof(bitstr));
    bs_setbits(&bs, 0, 8); //version
    bs_setbits(&bs, audio_t->iamf.profile_version, 8); //profile version

    size += avio_wdata(bitstr, bs.m_posBase);
  */
  return size;
}

static int mov_write_iamd_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = &movm->audio_trak[audio_trak_select];
  int entry_select = audio_t->entry_select;
  int size = 0;
  size += avio_wdata(audio_t[0].av_descriptor[entry_select].data,
                     audio_t[0].av_descriptor[entry_select].size);
  return size;
}

static int mov_write_stts_tag(MOVMuxContext *movm) {
  int size = 0;

  size += avio_wb32(0);

  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  if (!audio_t->frame.entries) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.deltas) {
    size += avio_wb32(0);
    return size;
  }

  int cnt = 0;
  int entry_count = 1;
  int sample_delta = audio_t->frame.deltas[0];
  for (cnt = 1; cnt < audio_t->frame.entries; cnt++) {
    if (sample_delta != audio_t->frame.deltas[cnt]) {
      entry_count++;
      sample_delta = audio_t->frame.deltas[cnt];
    }
  }

  size += avio_wb32(entry_count);

  int sample_count = 1;
  int ent = 0;
  sample_delta = audio_t->frame.deltas[0];
  for (cnt = 1; cnt < audio_t->frame.entries; cnt++) {
    if (sample_delta != audio_t->frame.deltas[cnt]) {
      size += avio_wb32(sample_count);
      size += avio_wb32(sample_delta);
      ent++;
      sample_delta = audio_t->frame.deltas[cnt];
      sample_count = 1;
    } else {
      sample_count++;
    }
  }

  if (ent < entry_count) {
    size += avio_wb32(sample_count);
    size += avio_wb32(sample_delta);
  }

  return size;
}

static int mov_write_stsz_tag(MOVMuxContext *movm) {
  int size = 0;
  int cnt;

  size += avio_wb32(0);
  size += avio_wb32(0);

  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  if (!audio_t->frame.entries) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.deltas) {
    size += avio_wb32(0);
    return size;
  }

  size += avio_wb32(audio_t->frame.entries);
  for (cnt = 0; cnt < audio_t->frame.entries; cnt++)
    size += avio_wb32(audio_t->frame.sizes[cnt]);

  return size;
}

static int mov_write_stsc_tag(MOVMuxContext *movm) {
  int size = 0;

  size += avio_wb32(0);

  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  if (!audio_t->frame.entries) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.deltas) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.first_chunk) {
    size += avio_wb32(0);
    return size;
  }

  uint32_t *samples_per_chunk = (uint32_t *)_dmalloc(1, __FILE__, __LINE__);

  int cnt = 0;
  int entry_count = 1;
  samples_per_chunk[0] = 1;
  int sample_chunk = audio_t->frame.first_chunk[0];
  for (cnt = 1; cnt < audio_t->frame.entries; cnt++) {
    if (sample_chunk != audio_t->frame.first_chunk[cnt]) {
      entry_count++;
      sample_chunk = audio_t->frame.first_chunk[cnt];
      samples_per_chunk = (uint32_t *)_drealloc(
          (char *)samples_per_chunk, entry_count, __FILE__, __LINE__);
      samples_per_chunk[entry_count - 1] = 1;
    } else {
      samples_per_chunk[entry_count - 1]++;
    }
  }
  size += avio_wb32(entry_count);
  for (int i = 0; i < entry_count; i++) {
    size += avio_wb32(i + 1);
    size += avio_wb32(samples_per_chunk[i]);
    size += avio_wb32(i + 1);
  }
  if (samples_per_chunk) {
    _dfree(samples_per_chunk, __FILE__, __LINE__);
  }
  return size;
}

static int mov_write_stco_tag(MOVMuxContext *movm) {
  int size = 0;
  int cnt;

  size += avio_wb32(0);

  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  if (!audio_t->frame.entries) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.deltas) {
    size += avio_wb32(0);
    return size;
  }

  if (!audio_t->frame.first_chunk) {
    size += avio_wb32(0);
    return size;
  }

  uint32_t *chunk_offset = (uint32_t *)_dmalloc(1, __FILE__, __LINE__);

  int entry_count = 1;
  chunk_offset[0] = audio_t->frame.offsets[0];
  int sample_chunk = audio_t->frame.first_chunk[0];
  for (cnt = 1; cnt < audio_t->frame.entries; cnt++) {
    if (sample_chunk != audio_t->frame.first_chunk[cnt]) {
      entry_count++;
      sample_chunk = audio_t->frame.first_chunk[cnt];
      chunk_offset = (uint32_t *)_drealloc((char *)chunk_offset, entry_count,
                                           __FILE__, __LINE__);
      chunk_offset[entry_count - 1] = audio_t->frame.offsets[cnt];
    }
  }

  size += avio_wb32(entry_count);
  for (cnt = 0; cnt < entry_count; cnt++) size += avio_wb32(chunk_offset[cnt]);

  if (chunk_offset) {
    _dfree(chunk_offset, __FILE__, __LINE__);
  }

  return size;
}

/*
class AudioPreRollEntry() extends AudioSampleGroupEntry (’prol’)
{
signed int(16) roll_distance;
}
*/
static int mov_write_sgpd(
    MOVMuxContext *movm)  // Global Descriptor Sample Group
{
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  int size = 0;

  // version&flags
  size += avio_wb32(0x01000000);
  // grouping type
  size += avio_wstring("prol");
  // Default length
  size += avio_wb32(2);  // signed int(16) roll_distance;
  // entry count
  size += avio_wb32(1);
  size += avio_wb16(audio_t->iamf.roll_distance);
  return size;
}

static int mov_write_sbgp(
    MOVMuxContext *movm)  // Global Descriptor Sample Group
{
  int size = 0;

  // version and flags
  size += avio_wb32(0x00000000);
  // grouping type
  size += avio_wstring("prol");
  size += avio_wb32(1);
  // table data
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  size += avio_wb32(movm->audio_trak[audio_trak_select].frame.entries);
  size += avio_wb32(1);

  return size;
}

static int mov_write_mvex_tag(MOVMuxContext *movm) {
  int size = 0;

  // version & flags
  size += avio_wb32(0);
  // Track ID
  size += avio_wb32(movm->num_video_traks + movm->audio_trak_select + 1);
  // default sample description index
  size += avio_wb32(1);
  // default sample duration
  size += avio_wb32(0);
  // default sample size
  size += avio_wb32(0);
  // default sample flags
  size += avio_wb32(0);

  return size;
}

static int mov_write_mfhd_tag(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  int size = 0;
  // version&flag
  size += avio_wb32(0);
  size += avio_wb32(audio_t->frame.fragments);

  return size;
}

static int mov_write_tfhd_tag(MOVMuxContext *movm) {
  int size = 0;

  uint32_t flags = MOV_TFHD_DEFAULT_DURATION | MOV_TFHD_DEFAULT_BASE_IS_MOOF |
                   MOV_TFHD_DEFAULT_FLAGS | MOV_TFHD_STSD_ID;

  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  // version
  size += avio_w8(0);
  // version
  size += avio_wb24(flags);
  // Track ID
  size += avio_wb32(movm->num_video_traks + movm->audio_trak_select + 1);
  // mov default sample description index
  size += avio_wb32(audio_t->stsd_id + 1);
  // size += avio_wb32(1);
  // default sample duration
  size += avio_wb32(audio_t->framesamples);
  // default sample flags
  size += avio_wb32(0);

  return size;
}

static int mov_write_tfdt_tag(MOVMuxContext *movm) {
  int size = 0;
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  // version&flag
  size += avio_wb32(0);
  size += avio_wb32(audio_t->frame.start_dts);
  return size;
}

static int mov_write_trun_tag(MOVMuxContext *movm) {
  int size = 0;
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  uint32_t flags = MOV_TRUN_DATA_OFFSET;
  flags |= MOV_TRUN_SAMPLE_SIZE;
  flags |= MOV_TRUN_SAMPLE_DURATION;
  // version
  size += avio_w8(0);
  // version
  size += avio_wb24(flags);
  // sample count
  size += avio_wb32(audio_t->frame.entries);
  // data offset
  audio_t->frame.trun_dataoffset = ftell(movm->mp4file);
  size += avio_wb32(0);  // empty offset
  for (int i = 0; i < audio_t->frame.entries; i++) {
    if (flags & MOV_TRUN_SAMPLE_DURATION)
      size += avio_wb32(audio_t->frame.deltas[i]);
    size += avio_wb32(audio_t->frame.sizes[i]);
  }
  return size;
}

static avio_context atoms_head[] = {{MOV_ATOM_NAME, "ftyp"},
                                    {MOV_ATOM_DATA, mov_write_ftyp_tag},
                                    {MOV_ATOM_NAME, "free"},
                                    {MOV_ATOM_NAME, "mdat"},
                                    {0}};

static avio_context atoms_fhead[] = {{MOV_ATOM_NAME, "ftyp"},
                                     {MOV_ATOM_DATA, mov_write_ftyp_tag},
                                     {MOV_ATOM_NAME, "free"},
                                     {0}};

static avio_context atoms_fragment[] = {{MOV_ATOM_NAME, "moof"},
                                        {MOV_ATOM_DOWN},
                                        {MOV_ATOM_NAME, "mfhd"},
                                        {MOV_ATOM_DATA, mov_write_mfhd_tag},
                                        {MOV_ATOM_NAME, "traf"},
                                        {MOV_ATOM_DOWN},
                                        {MOV_ATOM_NAME, "tfhd"},
                                        {MOV_ATOM_DATA, mov_write_tfhd_tag},
                                        {MOV_ATOM_NAME, "tfdt"},
                                        {MOV_ATOM_DATA, mov_write_tfdt_tag},
                                        {MOV_ATOM_NAME, "trun"},
                                        {MOV_ATOM_DATA, mov_write_trun_tag},
                                        {MOV_ATOM_UP},
                                        {MOV_ATOM_UP},
                                        {0}};

static avio_context atoms_tail[] = {{MOV_ATOM_NAME, "moov"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "mvhd"},
                                    {MOV_ATOM_DATA, mov_write_mvhd_tag},
                                    {0}};

static avio_context atoms_entry[] = {{MOV_ATOM_NAME, "iamf"},
                                     {MOV_ATOM_DATA, mov_write_iamf_tag},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "iamd"},
                                     {MOV_ATOM_DATA, mov_write_iamd_tag},
                                     {MOV_ATOM_UP},
                                     {0}};

static avio_context atoms_trak[] = {{MOV_ATOM_NAME, "trak"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "tkhd"},
                                    {MOV_ATOM_DATA, mov_write_tkhd_tag},
                                    {MOV_ATOM_NAME, "edts"},
                                    {MOV_ATOM_DATA, mov_write_edts_tag},
                                    {MOV_ATOM_NAME, "mdia"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "mdhd"},
                                    {MOV_ATOM_DATA, mov_write_mdhd_tag},
                                    {MOV_ATOM_NAME, "hdlr"},
                                    {MOV_ATOM_DATA, mov_write_hdlr1_tag},
                                    {MOV_ATOM_NAME, "minf"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "smhd"},
                                    {MOV_ATOM_DATA, mov_write_smhd_tag},
                                    {MOV_ATOM_NAME, "dinf"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "dref"},
                                    {MOV_ATOM_DATA, mov_write_dref_tag},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "url "},
                                    {MOV_ATOM_DATA, mov_write_url_tag},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_NAME, "stbl"},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "stsd"},
                                    {MOV_ATOM_DATA, mov_write_stsd_tag},
                                    /*{MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "iamf"},
                                    {MOV_ATOM_DATA, mov_write_iamf_tag},
                                    {MOV_ATOM_DOWN},
                                    {MOV_ATOM_NAME, "iamd"},
                                    {MOV_ATOM_DATA, mov_write_iamd_tag},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_UP},*/
                                    {MOV_ATOM_NAME, "stts"},
                                    {MOV_ATOM_DATA, mov_write_stts_tag},
                                    {MOV_ATOM_NAME, "stsc"},
                                    {MOV_ATOM_DATA, mov_write_stsc_tag},
                                    {MOV_ATOM_NAME, "stsz"},
                                    {MOV_ATOM_DATA, mov_write_stsz_tag},
                                    {MOV_ATOM_NAME, "stco"},
                                    {MOV_ATOM_DATA, mov_write_stco_tag},
                                    {MOV_ATOM_NAME, "sgpd"},
                                    {MOV_ATOM_DATA, mov_write_sgpd},
                                    {MOV_ATOM_NAME, "sbgp"},
                                    {MOV_ATOM_DATA, mov_write_sbgp},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_UP},
                                    {MOV_ATOM_UP},
                                    {0}};

static avio_context atoms_ftrak[] = {{MOV_ATOM_NAME, "trak"},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "tkhd"},
                                     {MOV_ATOM_DATA, mov_write_tkhd_tag},
                                     {MOV_ATOM_NAME, "edts"},
                                     {MOV_ATOM_DATA, mov_write_edts_tag},
                                     {MOV_ATOM_NAME, "mdia"},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "mdhd"},
                                     {MOV_ATOM_DATA, mov_write_mdhd_tag},
                                     {MOV_ATOM_NAME, "hdlr"},
                                     {MOV_ATOM_DATA, mov_write_hdlr1_tag},
                                     {MOV_ATOM_NAME, "minf"},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "smhd"},
                                     {MOV_ATOM_DATA, mov_write_smhd_tag},
                                     {MOV_ATOM_NAME, "dinf"},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "dref"},
                                     {MOV_ATOM_DATA, mov_write_dref_tag},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "url "},
                                     {MOV_ATOM_DATA, mov_write_url_tag},
                                     {MOV_ATOM_UP},
                                     {MOV_ATOM_UP},
                                     {MOV_ATOM_NAME, "stbl"},
                                     {MOV_ATOM_DOWN},
                                     {MOV_ATOM_NAME, "stsd"},
                                     {MOV_ATOM_DATA, mov_write_stsd_tag},
                                     //{MOV_ATOM_DOWN},
                                     //{MOV_ATOM_NAME, "iamf"},
                                     //{MOV_ATOM_DATA, mov_write_iamf_tag},
                                     //{MOV_ATOM_DOWN},
                                     //{MOV_ATOM_NAME, "iamd"},
                                     //{MOV_ATOM_DATA, mov_write_iamd_tag},
                                     //{MOV_ATOM_UP},
                                     //{MOV_ATOM_UP},
                                     {MOV_ATOM_NAME, "stts"},
                                     {MOV_ATOM_DATA, mov_write_stts_tag},
                                     {MOV_ATOM_NAME, "stsc"},
                                     {MOV_ATOM_DATA, mov_write_stsc_tag},
                                     {MOV_ATOM_NAME, "stsz"},
                                     {MOV_ATOM_DATA, mov_write_stsz_tag},
                                     {MOV_ATOM_NAME, "stco"},
                                     {MOV_ATOM_DATA, mov_write_stco_tag},
                                     {MOV_ATOM_NAME, "sgpd"},
                                     {MOV_ATOM_DATA, mov_write_sgpd},
                                     {MOV_ATOM_NAME, "sbgp"},
                                     {MOV_ATOM_DATA, mov_write_sbgp},
                                     {MOV_ATOM_UP},
                                     {MOV_ATOM_UP},
                                     {MOV_ATOM_UP},
                                     {MOV_ATOM_UP},
                                     {0}};

static int write_atom_default(MOVMuxContext *movm, uint64_t *atom_pos) {
  uint64_t apos = ftell(movm->mp4file);
  int size;

  size = avio_wb32(8);
  size += avio_wdata(movm->atom->opaque, 4);

  movm->atom++;
  if (movm->atom->atom_type == MOV_ATOM_DATA) {
    size += ((int (*)(MOVMuxContext *))movm->atom->opaque)(movm);
    movm->atom++;
  }
  if (movm->atom->atom_type == MOV_ATOM_DOWN) {
    movm->atom++;
    while (movm->atom->atom_type != MOV_ATOM_EXIT) {
      if (movm->atom->atom_type == MOV_ATOM_UP) {
        movm->atom++;
        break;
      }
      uint64_t ap;
      size += write_atom_default(movm, &ap);
    }
  }

  fseek(movm->mp4file, apos, SEEK_SET);
  avio_wb32(size);
  fseek(movm->mp4file, apos + size, SEEK_SET);

  *atom_pos = apos;
  return size;
}

static uint32_t f_duration(mov_audio_track *audio_t) {
  return (1000 * audio_t->framesamples * audio_t->frame.entries /
          audio_t->samplerate);
}

int mp4_flush_segment(MOVMuxContext *movm) {
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[audio_trak_select];

  if (audio_t->frame.entries == 0) return 0;

  audio_t->frame.fragments++;

  audio_t->frame.moofoffset = ftell(movm->mp4file);  // offset of current moof

  uint64_t moof_pos, moof_size = 0;
  movm->atom = atoms_fragment;
  uint64_t mvex_pos, mvex_size = 0;
  while (movm->atom->atom_type != MOV_ATOM_EXIT)
    moof_size += write_atom_default(movm, &moof_pos);

  // sample_composition_time_offset of trun box
  uint32_t sample_ctoffs = moof_size + 8;  // moof size + 'mdat' header
  fseek(movm->mp4file, audio_t->frame.trun_dataoffset, SEEK_SET);
  avio_wb32(sample_ctoffs);

  fseek(movm->mp4file, moof_pos, SEEK_SET);
  avio_wb32(moof_size);
  fseek(movm->mp4file, moof_pos + moof_size, SEEK_SET);

  // write mdat after moof
  avio_wb32(audio_t->frame.fmdat_offset + 8);
  avio_wstring("mdat");
  avio_wdata(audio_t->frame.fmdat, audio_t->frame.fmdat_offset);

  audio_t->frame.start_dts += audio_t->frame.entries * audio_t->framesamples;
  audio_t->frame.entries = 0;
  audio_t->frame.fmdat_offset = 0;

  if (audio_t->ia_descriptor.is_changed) {
    mov_write_moov(movm);
    audio_t->ia_descriptor.is_changed = 0;
  }

  return 0;
}
enum { BUFSTEP = 0x4000 };

int mov_write_audio(MOVMuxContext *movm, int trak, uint8_t *buf, int size,
                    int samples) {
  uint32_t pos;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[trak];
  if (audio_t->framesamples <= samples) {  //
    int bitrate;

    audio_t->bitrate.samples += samples;
    audio_t->bitrate.size += size;

    if (audio_t->bitrate.samples >= audio_t->samplerate) {
      bitrate = 8.0 * audio_t->bitrate.size * audio_t->samplerate /
                audio_t->bitrate.samples;
      audio_t->bitrate.size = 0;
      audio_t->bitrate.samples = 0;

      if (audio_t->bitrate.max < bitrate) audio_t->bitrate.max = bitrate;
    }
    audio_t->framesamples = samples;
  }

  if (audio_t->buffersize < size) audio_t->buffersize = size;
  audio_t->samples += samples;

  if (!IS_FRAGMENT_MP4) {
    pos = movm->mdatsize;
    movm->mdatsize += avio_wdata(buf, size);

    if (((audio_t->frame.entries + 1) * sizeof(*(audio_t->frame.sizes))) >
        audio_t->frame.buffersize) {
      audio_t->frame.buffersize += BUFSTEP;
      audio_t->frame.sizes =
          (uint32_t *)_drealloc((char *)audio_t->frame.sizes,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.offsets =
          (uint32_t *)_drealloc((char *)audio_t->frame.offsets,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.deltas =
          (uint32_t *)_drealloc((char *)audio_t->frame.deltas,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.first_chunk =
          (uint32_t *)_drealloc((char *)audio_t->frame.first_chunk,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
    }
    audio_t->frame.offsets[audio_t->frame.entries] = movm->mdatoffset + pos;
    audio_t->frame.sizes[audio_t->frame.entries] = size;
    audio_t->frame.deltas[audio_t->frame.entries] = samples;
    audio_t->frame.first_chunk[audio_t->frame.entries] =
        audio_t->descriptor_entries;
    audio_t->frame.entries++;
  } else {
    if (audio_t->frame.fmdat_offset + size > audio_t->frame.fmdat_size) {
      audio_t->frame.fmdat_size += size;
      audio_t->frame.fmdat = (unsigned char *)_drealloc(
          (char *)audio_t->frame.fmdat, audio_t->frame.fmdat_size, __FILE__,
          __LINE__);
    }
    memcpy(audio_t->frame.fmdat + audio_t->frame.fmdat_offset, buf, size);
    audio_t->frame.fmdat_offset += size;
    if (((audio_t->frame.entries + 1) * sizeof(*(audio_t->frame.sizes))) >
        audio_t->frame.buffersize) {
      audio_t->frame.buffersize += BUFSTEP;
      audio_t->frame.sizes =
          (uint32_t *)_drealloc((char *)audio_t->frame.sizes,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
    }
    audio_t->frame.sizes[audio_t->frame.entries++] = size;
    if (f_duration(audio_t) >= movm->max_fragment_duration) {
      mp4_flush_segment(movm);
    }
  }

  return ERROR_NONE;
}

int mov_write_audio2(MOVMuxContext *movm, int trak, AVPacket *pkt) {
  uint32_t pos = 0;
  mov_audio_track *audio_t = NULL;
  audio_t = &movm->audio_trak[trak];
  if (audio_t->framesamples <= pkt->samples) {  //
    int bitrate;

    audio_t->bitrate.samples += pkt->samples;
    audio_t->bitrate.size += pkt->size;

    if (audio_t->bitrate.samples >= audio_t->samplerate) {
      bitrate = 8.0 * audio_t->bitrate.size * audio_t->samplerate /
                audio_t->bitrate.samples;
      audio_t->bitrate.size = 0;
      audio_t->bitrate.samples = 0;

      if (audio_t->bitrate.max < bitrate) audio_t->bitrate.max = bitrate;
    }
    audio_t->framesamples = pkt->samples;
  }

  if (audio_t->buffersize < pkt->size) audio_t->buffersize = pkt->size;
  audio_t->samples += pkt->samples;

  if (!IS_FRAGMENT_MP4) {
    pos = movm->mdatsize;
    movm->mdatsize += avio_wdata(pkt->buf, pkt->size);

    if (((audio_t->frame.entries + 1) * sizeof(*(audio_t->frame.sizes))) >
        audio_t->frame.buffersize) {
      audio_t->frame.buffersize += BUFSTEP;
      audio_t->frame.sizes =
          (uint32_t *)_drealloc((char *)audio_t->frame.sizes,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.offsets =
          (uint32_t *)_drealloc((char *)audio_t->frame.offsets,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.deltas =
          (uint32_t *)_drealloc((char *)audio_t->frame.deltas,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.first_chunk =
          (uint32_t *)_drealloc((char *)audio_t->frame.first_chunk,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
    }
    audio_t->frame.offsets[audio_t->frame.entries] = movm->mdatoffset + pos;
    audio_t->frame.sizes[audio_t->frame.entries] = pkt->size;
    audio_t->frame.deltas[audio_t->frame.entries] = pkt->samples;
    audio_t->frame.first_chunk[audio_t->frame.entries] =
        audio_t->descriptor_entries;
    audio_t->frame.entries++;
  } else {
    if (audio_t->frame.fmdat_offset + pkt->size > audio_t->frame.fmdat_size) {
      audio_t->frame.fmdat_size += pkt->size;
      audio_t->frame.fmdat = (unsigned char *)_drealloc(
          (char *)audio_t->frame.fmdat, audio_t->frame.fmdat_size, __FILE__,
          __LINE__);
    }
    memcpy(audio_t->frame.fmdat + audio_t->frame.fmdat_offset, pkt->buf,
           pkt->size);
    audio_t->frame.fmdat_offset += pkt->size;
    if (((audio_t->frame.entries + 1) * sizeof(*(audio_t->frame.sizes))) >
        audio_t->frame.buffersize) {
      audio_t->frame.buffersize += BUFSTEP;
      audio_t->frame.sizes =
          (uint32_t *)_drealloc((char *)audio_t->frame.sizes,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
      audio_t->frame.deltas =
          (uint32_t *)_drealloc((char *)audio_t->frame.deltas,
                                audio_t->frame.buffersize, __FILE__, __LINE__);
    }
    audio_t->frame.sizes[audio_t->frame.entries] = pkt->size;
    audio_t->frame.deltas[audio_t->frame.entries++] = pkt->samples;

    if (f_duration(audio_t) >= movm->max_fragment_duration) {
      mp4_flush_segment(movm);
    }
  }

  return ERROR_NONE;
}

int mov_write_close(MOVMuxContext *movm) {
  if (!IS_FRAGMENT_MP4) {
    if (movm->mp4file) {
      fseek(movm->mp4file, movm->mdatoffset - 8, SEEK_SET);
      avio_wb32(movm->mdatsize + 8);
      fclose(movm->mp4file);
      movm->mp4file = 0;
    }
  } else {
    fclose(movm->mp4file);
    movm->mp4file = 0;
  }

  for (int i = 0; i < movm->num_audio_traks; i++) {
    mov_audio_track *audio_t = &movm->audio_trak[i];
    if (audio_t->frame.sizes) {
      _dfree(audio_t->frame.sizes, __FILE__, __LINE__);
      audio_t->frame.sizes = NULL;
    }
    if (audio_t->frame.offsets) {
      _dfree(audio_t->frame.offsets, __FILE__, __LINE__);
      audio_t->frame.offsets = NULL;
    }
    if (audio_t->frame.deltas) {
      _dfree(audio_t->frame.deltas, __FILE__, __LINE__);
      audio_t->frame.deltas = NULL;
    }
    if (audio_t->frame.first_chunk) {
      _dfree(audio_t->frame.first_chunk, __FILE__, __LINE__);
      audio_t->frame.first_chunk = NULL;
    }
    if (audio_t->frame.fmdat) {
      _dfree(audio_t->frame.fmdat, __FILE__, __LINE__);
      audio_t->frame.fmdat = NULL;
    }

    for (int j = 0; j < MAX_DESCRIPTOR_ENTRIES; j++) {
      if (audio_t[i].av_descriptor[j].data) {
        _dfree(audio_t[i].av_descriptor[j].data, __FILE__, __LINE__);
        audio_t[i].av_descriptor[j].data = NULL;
      }
    }

    if (audio_t->ia_descriptor.data) {
      _dfree(audio_t->ia_descriptor.data, __FILE__, __LINE__);
    }
  }
  if (movm->audio_trak) {
    _dfree(movm->audio_trak, __FILE__, __LINE__);
    movm->audio_trak = NULL;
  }
  _dfree(movm, __FILE__, __LINE__);
  return ERROR_NONE;
}

MOVMuxContext *mov_write_open(char *filename) {
  FILE *mp4file;
  MOVMuxContext *movm;

  if (!(mp4file = fopen(filename, "wb"))) {
    perror(filename);
    return NULL;
  }

  movm =
      (MOVMuxContext *)_dcalloc(1, sizeof(MOVMuxContext), __FILE__, __LINE__);
  if (movm == NULL) {
    goto failure;
  }
  movm->mp4file = mp4file;
  movm->mdatsize = 0;
  movm->num_video_traks = 0;
  movm->num_audio_traks = 0;
  movm->flags = 0;
  return (movm);

failure:
  if (movm != NULL) {
    mov_write_close(movm);
  }

  return NULL;
}

int mov_write_head(MOVMuxContext *movm) {
  uint64_t head_pos, head_size = 0;
  if (!IS_FRAGMENT_MP4)  // non - fragment mp4
  {
    movm->atom = atoms_head;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      head_size += write_atom_default(movm, &head_pos);
    movm->mdatoffset = ftell(movm->mp4file);
  } else {
    movm->atom = atoms_fhead;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      head_size += write_atom_default(movm, &head_pos);
    movm->moovoffset = ftell(movm->mp4file);

    // write empty moov
    uint64_t moov_pos, moov_size = 0;
    movm->atom = atoms_tail;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      moov_size += write_atom_default(movm, &moov_pos);

    // trak
    uint64_t trak_pos, trak_size = 0;
    for (int i = 0; i < movm->num_audio_traks; i++) {  // audio trak
      movm->atom = atoms_ftrak;
      movm->audio_trak_select = i;
      while (movm->atom->atom_type != MOV_ATOM_EXIT)
        trak_size += write_atom_default(movm, &trak_pos);
      // moov size
      moov_size += trak_size;
      fseek(movm->mp4file, moov_pos, SEEK_SET);
      avio_wb32(moov_size);
      fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);
    }

    // moov size¸
    fseek(movm->mp4file, moov_pos, SEEK_SET);
    avio_wb32(moov_size);
    fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);

    movm->mdatoffset = ftell(movm->mp4file);
  }
  return ERROR_NONE;
}

int mov_write_moov(MOVMuxContext *movm) {
  movm->moovoffset = ftell(movm->mp4file);
  // write empty moov
  uint64_t moov_pos, moov_size = 0;
  movm->atom = atoms_tail;
  while (movm->atom->atom_type != MOV_ATOM_EXIT)
    moov_size += write_atom_default(movm, &moov_pos);

  // trak
  uint64_t trak_pos, trak_size = 0;
  for (int i = 0; i < movm->num_audio_traks; i++) {  // audio trak
    movm->atom = atoms_ftrak;
    movm->audio_trak_select = i;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      trak_size += write_atom_default(movm, &trak_pos);
    // moov size
    moov_size += trak_size;
    fseek(movm->mp4file, moov_pos, SEEK_SET);
    avio_wb32(moov_size);
    fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);
  }

  // moov size¸
  fseek(movm->mp4file, moov_pos, SEEK_SET);
  avio_wb32(moov_size);
  fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);

  return ERROR_NONE;
}

int mov_write_trak(MOVMuxContext *movm, int num_video_traks,
                   int num_audio_traks) {
  movm->num_audio_traks = num_audio_traks;
  if (0 < num_audio_traks && num_audio_traks < 8) {  //
    movm->audio_trak = (mov_audio_track *)_dcalloc(
        num_audio_traks, sizeof(mov_audio_track), __FILE__, __LINE__);
    mov_audio_track *audio_t = movm->audio_trak;
    for (int i = 0; i < num_audio_traks; i++) {
      audio_t[i].frame.buffersize = BUFSTEP;
      audio_t[i].frame.sizes =
          (uint32_t *)_dmalloc(audio_t[i].frame.buffersize, __FILE__, __LINE__);
      audio_t[i].frame.offsets =
          (uint32_t *)_dmalloc(audio_t[i].frame.buffersize, __FILE__, __LINE__);
      audio_t[i].frame.deltas =
          (uint32_t *)_dmalloc(audio_t[i].frame.buffersize, __FILE__, __LINE__);
      audio_t[i].frame.first_chunk =
          (uint32_t *)_dmalloc(audio_t[i].frame.buffersize, __FILE__, __LINE__);
      if (IS_FRAGMENT_MP4) {
        audio_t[i].frame.fmdat_size = 2 * 1024 * 1024;  // 2M Bytes
        audio_t[i].frame.fmdat = (unsigned char *)_dmalloc(
            audio_t[i].frame.fmdat_size, __FILE__, __LINE__);
      }
      for (int j = 0; j < MAX_DESCRIPTOR_ENTRIES; j++)
        audio_t[i].av_descriptor[j].data =
            (uint8_t *)_dmalloc(1024, __FILE__, __LINE__);

      audio_t[i].ia_descriptor.data =
          (uint8_t *)_dmalloc(1024, __FILE__, __LINE__);
    }
  } else {
    movm->num_audio_traks = 0;
    movm->audio_trak = NULL;
    return (ERROR_FAIL);
  }
  return ERROR_NONE;
}

int mov_write_tail(MOVMuxContext *movm) {
  if (IS_FRAGMENT_MP4) {
    mp4_flush_segment(movm);
    return 0;
    // fragment mp4, write real moov.
    // fseek(movm->mp4file, movm->moovoffset, SEEK_SET);
  }
  mov_audio_track *audio_t;
  for (int i = 0; i < movm->num_audio_traks; i++) {  // bitstream calculation.
    audio_t = &movm->audio_trak[i];
    audio_t[i].bitrate.avg =
        8.0 * movm->mdatsize * audio_t[i].samplerate / audio_t[i].samples;
    if (!audio_t[i].bitrate.max)
      audio_t[i].bitrate.max = audio_t[i].bitrate.avg;
  }

  // moov
  uint64_t moov_pos, moov_size = 0;
  movm->atom = atoms_tail;
  while (movm->atom->atom_type != MOV_ATOM_EXIT)
    moov_size += write_atom_default(movm, &moov_pos);

  // trak
  uint64_t trak_pos, trak_size = 0;
  for (int i = 0; i < movm->num_audio_traks; i++) {  // audio trak

    if (!IS_FRAGMENT_MP4) {
      movm->atom = atoms_trak;
    } else {
      movm->atom = atoms_ftrak;
    }
    movm->audio_trak_select = i;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      trak_size += write_atom_default(movm, &trak_pos);
    // moov size
    moov_size += trak_size;
    fseek(movm->mp4file, moov_pos, SEEK_SET);
    avio_wb32(moov_size);
    fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);
  }

  // moov size¸
  fseek(movm->mp4file, moov_pos, SEEK_SET);
  avio_wb32(moov_size);
  fseek(movm->mp4file, moov_pos + moov_size, SEEK_SET);

  return ERROR_NONE;
}

static int mov_write_entries(MOVMuxContext *movm) {
  int size = 0;
  int audio_trak_select;
  audio_trak_select = movm->audio_trak_select;

  for (int i = 0; i < movm->audio_trak[audio_trak_select].descriptor_entries;
       i++) {
    movm->atom = atoms_entry;
    uint64_t entry_pos = 0;
    movm->audio_trak[audio_trak_select].entry_select = i;
    while (movm->atom->atom_type != MOV_ATOM_EXIT)
      size += write_atom_default(movm, &entry_pos);
  }
  return size;
}