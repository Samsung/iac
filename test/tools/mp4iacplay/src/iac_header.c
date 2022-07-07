#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "iac_header.h"
#include "atom.h"
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
    *val = *val << 8 | (uint32_t)p->data[p->pos+1];
    *val = *val << 8 | (uint32_t)p->data[p->pos+2];
    *val = *val << 8 | (uint32_t)p->data[p->pos+3];
    p->pos += 4;
    return 1;
}

static int read_uint16(ROPacket *p, uint16_t *val)
{
    if (p->pos>p->maxlen-2) {
        return 0;
    }
    *val =  (uint16_t)p->data[p->pos  ];
    *val = *val << 8 | (uint16_t)p->data[p->pos+1];
    p->pos += 2;
    return 1;
}

static int read_chars(ROPacket *p, unsigned char *str, int nb_chars)
{
    int i;
    if (p->pos>p->maxlen-nb_chars) {
        return 0;
    }
    if (!str)
        p->pos += nb_chars;
    else {
        for (i=0; i<nb_chars; i++) {
            str[i] = p->data[p->pos++];
        }
    }
    return 1;
}

static uint32_t read_tag_size(ROPacket *p)
{
    uint8_t ch;
    uint32_t size = 0;

    for (int cnt = 0; cnt < 4; cnt++) {
        read_chars(p, &ch, 1);

        size <<= 7;
        size |= (ch & 0x7f);
        if (!(ch & 0x80)) {
            break;
        }
    }
    return size;
}



static void parse_opus_spec (IACHeader *h)
{
    OpusHeader *oh = &h->opus;
    ROPacket p;
    uint8_t     ch = 0;
    uint16_t    shortval = 0;

    p.data = h->codec_config;
    p.maxlen = h->clen;
    p.pos = 8;

    read_chars(&p, &ch, 1);
    oh->version = ch;

    read_chars(&p, &ch, 1);
    oh->channels = ch;

    read_uint16(&p, &shortval);
    oh->preskip = shortval;

    read_uint32(&p, &oh->input_sample_rate);
    read_uint16(&p, &shortval);
    oh->gain = (short)shortval;

    read_chars(&p, &ch, 1);
    oh->channel_mapping = ch;

    if (!oh->channel_mapping) {
        oh->nb_streams = 1;
        oh->nb_coupled = oh->channels>1;
        oh->stream_map[0]=0;
        oh->stream_map[1]=1;
    } else if (oh->channel_mapping < 4) {
        read_chars(&p, &ch, 1);
        oh->nb_streams = ch;
        read_chars(&p, &ch, 1);
        oh->nb_coupled = ch;

        /* Multi-stream support */
        if (oh->channel_mapping == 3) {
            int dmatrix_size = oh->channels * (oh->nb_streams + oh->nb_coupled) * 2;
            if (dmatrix_size > OPUS_DEMIXING_MATRIX_SIZE_MAX) {
                p.pos += dmatrix_size;
            } else {
                read_chars(&p, oh->dmatrix, dmatrix_size);
            }
            for (int i=0; i<oh->channels; i++) {
                oh->stream_map[i] = i;
            }
        } else if (oh->channel_mapping == 1) {
            read_chars(&p, oh->stream_map, oh->channels);
        }
    }
}

static void parse_es_descriptor(IACHeader *h)
{
    AACHeader *ah = &h->aac;
    ROPacket p;
    uint8_t     ch = 0;
    uint16_t    shortval = 0;
    uint32_t    intval = 0;

    p.data = h->codec_config;
    p.maxlen = h->clen;
    p.pos = 8;

    static int sf[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000, 7350, 0, 0, 0
    };

    read_uint32(&p, &intval);

    // MP4ES_Descriptor (3)
    /**
     *  class ES_Descriptor extends BaseDescriptor : bit(8) tag=ES_DescrTag {
     *      bit(16) ES_ID;
     *      bit(1) streamDependenceFlag;
     *      bit(1) URL_Flag;
     *      bit(1) OCRstreamFlag;
     *      bit(5) streamPriority;
     *      ...
     *      DecoderConfigDescriptor decConfigDescr;
     *      ...
     *  }
     * */
    read_chars(&p, &ch, 1);
    if (ch != 3)
        return;

    read_tag_size(&p);

    // ESID
    read_uint16(&p, &shortval);
    // flags
    read_chars(&p, &ch, 1);

    // MP4DecoderConfigDescriptor (4)
    /**
     *  class DecoderConfigDescriptor extends BaseDescriptor : bit(8) tag=DecoderConfigDescrTag {
     *      bit(8) objectTypeIndication;
     *      bit(6) streamType;
     *      bit(1) upStream;
     *      const bit(1) reserved=1;
     *      bit(24) bufferSizeDB;
     *      bit(32) maxBitrate;
     *      bit(32) avgBitrate;
     *      DecoderSpecificInfo decSpecificInfo[0 .. 1];
     *      profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDescr [0..255];
     *  }
     * */
    read_chars(&p, &ch, 1);
    if (ch != 4)
        return;

    read_tag_size(&p);
    // object type : 0x40 - Audio ISO/IEC 14496-3
    read_chars(&p, &ch, 1);
    ah->object_type_indication = ch;
    if (ch == 0x40) {
        h->codec_id = ATOM_TYPE_MP4A;
        fprintf(stderr, "AAC \n");
    }

    read_chars(&p, &ch, 1);
    ah->stream_type = ch >> 2 & 0x3f;
    if (ah->stream_type != 0x5)
        return;

    ah->upstream = ch >> 1 & 0x1;
    read_chars(&p, 0, 11);

    // MP4DecSpecificInfoDescriptor (5)
    /**
     *  AudioSpecificConfig () {
     *      audioObjectType = GetAudioObjectType();
     *      samplingFrequencyIndex;                     4 bslbf
     *      if ( samplingFrequencyIndex == 0xf ) {
     *          samplingFrequency;                      24 uimsbf
     *      }
     *      channelConfiguration;                       4 bslbf
     *      ...
     *  }
     *
     *  GetAudioObjectType()
     *  {
     *      audioObjectType; 5 uimsbf
     *      if (audioObjectType == 31) {
     *          audioObjectType = 32 + audioObjectTypeExt; 6 uimsbf
     *      }
     *      return audioObjectType;
     *  }
     *
     *  GASpecificConfig (samplingFrequencyIndex,
     *                    channelConfiguration,
     *                    audioObjectType)
     *  {
     *      frameLengthFlag;                            1 bslbf
     *      dependsOnCoreCoder;                         1 bslbf
     *      if (dependsOnCoreCoder) {
     *          coreCoderDelay;                         14 uimsbf
     *      }
     *      extensionFlag;                              1 bslbf
     *      ...
     *  }
     * */
    read_chars(&p, &ch, 1);
    if (ch != 5)
        return;
    read_tag_size(&p);

    read_uint16(&p, &shortval);
    ah->audio_object_type = shortval >> 11 & 0x1F;
    ah->sample_rate = sf[shortval >> 7 & 0xF];
    ah->channels = shortval >> 3 & 0xF;

    ah->frame_length_flag = shortval >> 2 & 0x01;
    ah->depends_on_core_coder = shortval >> 1 & 0x01;
    ah->extension_flag = shortval & 0x01;
}

int iac_header_parse_codec_spec(IACHeader *h)
{
    int i, size;
    char str[9];
    ROPacket p;
    uint32_t val;

    p.data = h->codec_config;
    p.maxlen = h->clen;
    p.pos = 4;

    read_chars(&p, str, 4);
    if (!strncmp(str, "dOps", 4)) {
        fprintf(stderr, "dOps\n");
        h->codec_id = ATOM_TYPE_OPUS;
        parse_opus_spec(h);
    } else if (!strncmp(str, "esds", 4)) {
        fprintf(stderr, "esds\n");
        parse_es_descriptor(h);
    } else
        fprintf(stderr, "unknown\n");

    return 1;
}

