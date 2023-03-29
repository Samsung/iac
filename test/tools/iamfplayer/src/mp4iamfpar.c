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
 * @file mp4iamfpar.c
 * @brief MP4 and fMP4 file parser.
 * @version 0.1
 * @date Created 03/03/2023
 **/


#include "mp4iamfpar.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "atom.h"

MP4IAMFParser *mp4_iamf_parser_create() {
  MP4IAMFParser *ths;
  ths = (MP4IAMFParser *)malloc(sizeof(MP4IAMFParser));
  return ths;
}

void mp4_iamf_parser_init(MP4IAMFParser *ths) {
  ths->m_mp4r = NULL;
  ths->m_logger = stderr;
}

void mp4_iamf_parser_destroy(MP4IAMFParser *ths) {
  mp4_iamf_parser_close(ths);
  ths->m_mp4r = NULL;
  ths->m_logger = stderr;
}

int mp4_iamf_parser_open_trak(MP4IAMFParser *ths, const char *mp4file,
                              IAMFHeader **header) {
  IAMFHeader *iamf_header;
  audio_rtr_t *atr = 0;
  //////////////////////////////////////

  if ((ths->m_mp4r = mp4demux_open(mp4file, ths->m_logger)) == NULL) {
    fprintf(stderr, "file open error\n");
    goto done;
  }

  fprintf(stderr, " %d track.\n", ths->m_mp4r->num_a_trak);
  atr = ths->m_mp4r->a_trak;
  if (!atr) {
    fprintf(stderr, "MP4 file does not have track.\n");
    goto done;
  }

  iamf_header = (IAMFHeader *)atr->csc;
  if (iamf_header == NULL || atr == NULL) {
    fprintf(stderr, "MP4 file does not have OPUS audio track.\n");
    goto done;
  }
  *header = iamf_header;

  //////////////////////////////////////

done:
  if (!ths->m_mp4r) {
    return (-1);
  }

  return 1;
}

int mp4_iamf_parser_read_packet(MP4IAMFParser *ths, int trakn, void *pkt_buf,
                                int inbytes, int *pktlen_out,
                                int64_t *sample_offs, int *cur_entno) {
  //////////////////////////////////////
  audio_rtr_t *atr = ths->m_mp4r->a_trak;
  int sample_delta = 0, idx = 0;
  int ret = 0, used = 0;
  IAMFHeader *header;

  if (atr[trakn].frame.ents_offset + atr[trakn].frame.ents <=
      atr[trakn].frame.current) {
    if (ths->m_mp4r->moof_flag) {
      if (mp4demux_parse(ths->m_mp4r, trakn) < 0)
        return (-1);
      else {
        header = (IAMFHeader *)atr[trakn].csc;
        if (ths->m_mp4r->next_moov > 0 &&
            ths->m_mp4r->next_moov < ths->m_mp4r->next_moof &&
            header->description) {
          ret = iamf_header_read_description_OBUs(header, pkt_buf, inbytes);
          used += ret;
        }
      }
    } else
      return (-1);
  }

  idx = atr[trakn].frame.current - atr[trakn].frame.ents_offset;
  if (sample_offs && atr[trakn].frame.offs) {
    *sample_offs = atr[trakn].frame.offs[idx];
  }

  if (cur_entno) {
    *cur_entno = atr[trakn].frame.current + 1;
  }

  if (mp4demux_audio(ths->m_mp4r, trakn, &sample_delta) != ERR_OK) {
    fprintf(stderr, "mp4demux_frame() error\n");
    return (-1);
  }

  /* printf("Parameter length %d\n", ret); */
  if (inbytes - used < atr[trakn].bitbuf.size) {
    memcpy((uint8_t *)pkt_buf + ret, atr[trakn].bitbuf.data, inbytes - used);
    *pktlen_out = atr[trakn].bitbuf.size + used;
    return (sample_delta);
  } else {
    memcpy((uint8_t *)pkt_buf + used, atr[trakn].bitbuf.data,
           atr[trakn].bitbuf.size);
    *pktlen_out = atr[trakn].bitbuf.size + used;
    return (sample_delta);
  }

  //////////////////////////////////////
}

void mp4_iamf_parser_close(MP4IAMFParser *ths) {
  if (ths->m_mp4r) {
    mp4demux_close(ths->m_mp4r);  // close all track ids, too.
  }
  ths->m_mp4r = NULL;
}

int mp4_iamf_parser_set_logger(MP4IAMFParser *ths, FILE *logger) {
  ths->m_logger = logger;
  return (0);
}
