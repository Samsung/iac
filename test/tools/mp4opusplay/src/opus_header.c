#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "opus_header.h"
#include <string.h>
#include <stdio.h>

/* Header contents:
  - "OpusHead" (64 bits)
  - version number (8 bits)
  - Channels C (8 bits)
  - Pre-skip (16 bits)
  - Sampling rate (32 bits)
  - Gain in dB (16 bits, S7.8)
  - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
             2..254: reserved, 255: multistream with no mapping)

  - if (mapping != 0)
     - N = total number of streams (8 bits)
     - M = number of paired streams (8 bits)
     - C times channel origin
          - if (C<2*M)
             - stream = byte/2
             - if (byte&0x1 == 0)
                 - left
               else
                 - right
          - else
             - stream = byte-M
*/

typedef struct {
    const unsigned char *data;
    int maxlen;
    int pos;
} ROPacket;

static int read_uint32(ROPacket *p, uint32_t *val)
{
    if (p->pos>p->maxlen-4) {
        return 0;
    }
    *val =  (uint32_t)p->data[p->pos  ];
    *val |= (uint32_t)p->data[p->pos+1]<< 8;
    *val |= (uint32_t)p->data[p->pos+2]<<16;
    *val |= (uint32_t)p->data[p->pos+3]<<24;
    p->pos += 4;
    return 1;
}

static int read_uint16(ROPacket *p, uint16_t *val)
{
    if (p->pos>p->maxlen-2) {
        return 0;
    }
    *val =  (uint16_t)p->data[p->pos  ];
    *val |= (uint16_t)p->data[p->pos+1]<<8;
    p->pos += 2;
    return 1;
}

static int read_chars(ROPacket *p, unsigned char *str, int nb_chars)
{
    int i;
    if (p->pos>p->maxlen-nb_chars) {
        return 0;
    }
    for (i=0; i<nb_chars; i++) {
        str[i] = p->data[p->pos++];
    }
    return 1;
}

int opus_header_parse(const unsigned char *packet, int len, OpusHeader *h)
{
    int i;
    char str[9];
    ROPacket p;
    unsigned char ch;
    uint16_t shortval;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    str[8] = 0;
    if (len<19) {
        return 0;
    }
    read_chars(&p, (unsigned char *)str, 8);
    if (memcmp(str, "OpusHead", 8)!=0) {
        return 0;
    }

    if (!read_chars(&p, &ch, 1)) {
        return 0;
    }
    h->version = ch;
    if((h->version&240) != 0) { /* Only major version 0 supported. */
        return 0;
    }

    if (!read_chars(&p, &ch, 1)) {
        return 0;
    }
    h->channels = ch;
    if (h->channels == 0) {
        return 0;
    }

    if (!read_uint16(&p, &shortval)) {
        return 0;
    }
    h->preskip = shortval;

    if (!read_uint32(&p, &h->input_sample_rate)) {
        return 0;
    }

    if (!read_uint16(&p, &shortval)) {
        return 0;
    }
    h->gain = (short)shortval;

    if (!read_chars(&p, &ch, 1)) {
        return 0;
    }
    h->channel_mapping = ch;

    if (h->channel_mapping != 0) {
        if (!read_chars(&p, &ch, 1)) {
            return 0;
        }

        if (ch<1) {
            return 0;
        }
        h->nb_streams = ch;

        if (!read_chars(&p, &ch, 1)) {
            return 0;
        }

        if (ch>h->nb_streams || (ch+h->nb_streams)>255) {
            return 0;
        }
        h->nb_coupled = ch;

        /* Multi-stream support */
        if (h->channel_mapping == 3) {
            int dmatrix_size = h->channels * (h->nb_streams + h->nb_coupled) * 2;
            if (dmatrix_size > len - p.pos) {
                return 0;
            }
            if (dmatrix_size > OPUS_DEMIXING_MATRIX_SIZE_MAX) {
                p.pos += dmatrix_size;
            } else if (!read_chars(&p, h->dmatrix, dmatrix_size)) {
                return 0;
            }
            for (i=0; i<h->channels; i++) {
                h->stream_map[i] = i;
            }
        } else if (h->channel_mapping == 1) {
            for (i=0; i<h->channels; i++) {
                if (!read_chars(&p, &h->stream_map[i], 1)) {
                    return 0;
                }
                if (h->stream_map[i]>(h->nb_streams+h->nb_coupled) && h->stream_map[i]!=255) {
                    return 0;
                }
            }
        } else if (h->channel_mapping == 4) {
            for (i = 0; i<h->channels; i++) {
                if (!read_chars(&p, &h->stream_map[i], 1)) {
                    return 0;
                }
            }
        }
    } else { // (h->channel_mapping == 0)
        if(h->channels>2) {
            return 0;
        }
        h->nb_streams = 1;
        h->nb_coupled = h->channels>1;
        h->stream_map[0]=0;
        h->stream_map[1]=1;
    }
    /*For version 0/1 we know there won't be any more data
      so reject any that have data past the end.*/
    if ((h->version==0 || h->version==1) && p.pos != len) {
        return 0;
    }
    return 1;
}

/* This is just here because it's a convenient file linked by both opusenc and
   opusdec (to guarantee this maps stays in sync). */
const int wav_permute_matrix[8][8] = {
    {0},              /* 1.0 mono   */
    {0,1},            /* 2.0 stereo */
    {0,2,1},          /* 3.0 channel ('wide') stereo */
    {0,1,2,3},        /* 4.0 discrete quadraphonic */
    {0,2,1,3,4},      /* 5.0 surround */
    {0,2,1,4,5,3},    /* 5.1 surround */
    {0,2,1,5,6,4,3},  /* 6.1 surround */
    {0,2,1,6,7,4,5,3} /* 7.1 surround (classic theater 8-track) */
};
