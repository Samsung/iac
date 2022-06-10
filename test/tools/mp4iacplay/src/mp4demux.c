#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "mp4demux.h"
#include "a2b_endian.h"
#include "atom.h"
#include "iac_header.h"
#include "dmemory.h"

enum MOV_ATOM_TYPE {
    MOV_ATOM_STOP = 0,  /* end of atoms */
    MOV_ATOM_NAME,      /* plain atom */
    MOV_ATOM_DESCENT,   /* starts group of children */
    MOV_ATOM_ASCENT,    /* ends group */
    MOV_ATOM_DATA,
};

static int parse(mp4r_t *mp4r, uint32_t *sizemax);
static int mov_read_sgpd(mp4r_t *mp4r, int size);
static int mov_read_sbgp(mp4r_t *mp4r, int size);


static avio_context s_sgpd[] = {
    { MOV_ATOM_NAME, "stco" },
    { MOV_ATOM_NAME, "sgpd" },
    { MOV_ATOM_DATA, mov_read_sgpd },
    { MOV_ATOM_NAME, "sbgp" },
    { MOV_ATOM_DATA, mov_read_sbgp },
    { 0 }
};

static int avio_rdata(FILE *fin, void *data, int size)
{
    if (fread(data, 1, size, fin) != size) {
        return ERR_FAIL;
    }
    return size;
}

static int avio_rstring(FILE *fin, char *txt, int sizemax)
{
    int size;
    for (size = 0; size < sizemax; size++) {
        if (fread(txt + size, 1, 1, fin) != 1) {
            return ERR_FAIL;
        }
        if (!txt[size]) {
            break;
        }
    }
    txt[sizemax - 1] = '\0';

    return size;
}

#define avio_rb32() avio_rb32_(mp4r)
static uint32_t avio_rb32_(mp4r_t *mp4r)
{
    FILE *fin = mp4r->fin;
    uint32_t val;
    avio_rdata(fin, &val, 4);
    val = bswap32(val);
    return val;
}

#define avio_rb16() avio_rb16_(mp4r)
static uint16_t avio_rb16_(mp4r_t *mp4r)
{
    FILE *fin = mp4r->fin;
    uint16_t val;
    avio_rdata(fin, &val, 2);
    val = bswap16(val);
    return val;
}

#define avio_r8() avio_r8_(mp4r)
static int avio_r8_(mp4r_t *mp4r)
{
    FILE *fin = mp4r->fin;
    uint8_t val;
    avio_rdata(fin, &val, 1);
    return val;
}

static int mov_read_ftyp(mp4r_t *mp4r, int size)
{
    enum { BUFSIZE = 40 };
    char buf[BUFSIZE];
    uint32_t val;

    buf[4] = 0;
    avio_rdata(mp4r->fin, buf, 4);
    val = avio_rb32();

    if (mp4r->logger) {
        fprintf(mp4r->logger, "Brand:\t\t\t%s(version %d)\n", buf, val);
    }
    avio_rstring(mp4r->fin, buf, BUFSIZE);
    if (mp4r->logger) {
        fprintf(mp4r->logger, "Compatible brands:\t%s\n", buf);
    }

    return size;
}

enum {
    SECSINDAY = 24 * 60 * 60
};

static char *mp4time(time_t t)
{
    int y;

    // subtract some seconds from the start of 1904 to the start of 1970
    for (y = 1904; y < 1970; y++) {
        t -= 365 * SECSINDAY;
        if (!(y & 3)) {
            t -= SECSINDAY;
        }
    }
    return ctime(&t);
}

static int mov_read_mvhd(mp4r_t *mp4r, int size)
{
    uint32_t x;
    // version/flags
    x = avio_rb32();
    // Creation time
    mp4r->ctime = avio_rb32();
    // Modification time
    mp4r->mtime = avio_rb32();
    // Time scale
    mp4r->timescale = avio_rb32();
    // Duration
    mp4r->duration =
        avio_rb32();
    // reserved
    x = avio_rb32();
    x = avio_rb32();
    for (int x = 0; x < 2; x++) {
        avio_rb32();
    }
    for (int x = 0; x < 9; x++) {
        avio_rb32();
    }
    for (int x = 0; x < 6; x++) {
        avio_rb32();
    }
    // next track id
    mp4r->tot_track_scan = 0;
    mp4r->next_track_id = avio_rb32();

    return size;
}

static int mov_read_mdhd(mp4r_t *mp4r, int size)
{
    int sel_a_trak;
    if (mp4r->trak_type[mp4r->cur_r_trak] == TRAK_TYPE_AUDIO) {
        sel_a_trak = mp4r->sel_a_trak;
        audio_rtr_t *atr = mp4r->a_trak;

        // version/flags
        avio_rb32();
        // Creation time
        atr[sel_a_trak].ctime = avio_rb32();
        // Modification time
        atr[sel_a_trak].mtime = avio_rb32();
        // Time scale
        atr[sel_a_trak].samplerate = avio_rb32();
        // Duration
        atr[sel_a_trak].samples = avio_rb32();
        // Language
        avio_rb16();
        // pre_defined
        avio_rb16();
    }
    return size;
}

static int mov_read_tkhd(mp4r_t *mp4r, int size)
{
    uint32_t x;
    // version/flags
    avio_rb32();
    // creation-time
    avio_rb32();
    // modification-time
    avio_rb32();
    // track id
    mp4r->tot_track_scan++;
    uint32_t track_id = avio_rb32();
    mp4r->cur_r_trak = track_id - 1;
    // reserved
    avio_rb32();
    // duration
    avio_rb32();
    // reserved * 3
    for (int i = 0; i < 3; i++) {
        avio_rb32();
    }
    // reserved 16bit, 16bit, x == 0x01000000
    x = avio_rb32();
    // reserved * 9
    for (int i = 0; i < 9; i++) {
        x = avio_rb32();
    }
    // reserved
    uint32_t w = avio_rb32();
    // reserved
    uint32_t h = avio_rb32();
    if (w == 0 && h == 0) {
        mp4r->a_trak = (audio_rtr_t *)_drealloc((char *)mp4r->a_trak,
                                                sizeof(audio_rtr_t)*(mp4r->num_a_trak + 1),__FILE__,__LINE__);
        memset(((uint8_t *)mp4r->a_trak) + sizeof(audio_rtr_t)*(mp4r->num_a_trak), 0,
               sizeof(audio_rtr_t));
        mp4r->sel_a_trak = mp4r->num_a_trak;
        mp4r->num_a_trak++;
        mp4r->a_trak[mp4r->sel_a_trak].track_id = track_id;
        mp4r->trak_type[mp4r->cur_r_trak] = TRAK_TYPE_AUDIO;
    } else {
        mp4r->trak_type[mp4r->cur_r_trak] = TRAK_TYPE_VIDEO;
    }

    return (size);
}

static int mov_read_stsd(mp4r_t *mp4r, int size)
{
    int x;
    // version/flags
    x = avio_rb32();
    // Number of entries(one 'mp4a' or 'opus')
    if (avio_rb32() != 1) { //fixme: error handling
        return ERR_FAIL;
    }
    return size;
}

static int mov_read_mp4a(mp4r_t *mp4r, int size)
{
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // Reserved (6 bytes)
    avio_rb32();
    avio_rb16();
    // Data reference index
    avio_rb16();
    // Version
    avio_rb16();
    // Revision level
    avio_rb16();
    // Vendor
    avio_rb32();
    // Number of channels
    atr[sel_a_trak].channels = avio_rb16();
    // Sample size (bits)
    atr[sel_a_trak].bits = avio_rb16();
    // Compression ID
    avio_rb16();
    // Packet size
    avio_rb16();
    // Sample rate (16.16)
    // fractional framerate, probably not for audio
    // rate integer part
    avio_rb16();
    // rate reminder part
    avio_rb16();

    return size;
}

static int mov_read_opus(mp4r_t *mp4r, int size)
{
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    atr[sel_a_trak].csc = _dcalloc(1, sizeof(IACHeader), __FILE__, __LINE__);
    OpusHeader *header = (OpusHeader *)atr[sel_a_trak].csc;

    header->magic_id = ATOM_TYPE_OPUS;

    // Reserved (6 bytes)
    avio_rb32(); // reserved
    avio_rb16(); // reserved
    avio_rb16(); // data_reference_index
    avio_rb32(); // reserved
    avio_rb32(); // reserved
    avio_rb16(); // channel count
    avio_rb16(); // sample bit depth
    avio_rb16(); // predefined/reserved
    avio_rb32() >> 16; // sample rate
    avio_rb16(); // .0000
    return size;
}

#define getsize() getsize_(mp4r)
static uint32_t getsize_(mp4r_t *mp4r)
{
    int cnt;
    uint32_t size = 0;
    for (cnt = 0; cnt < 4; cnt++) {
        int tmp = avio_r8();

        size <<= 7;
        size |= (tmp & 0x7f);
        if (!(tmp & 0x80)) {
            break;
        }
    }
    return size;
}

static int mov_read_esds(mp4r_t *mp4r, int size)
{
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;
    // descriptor tree:
    // MP4ES_Descriptor
    //   MP4DecoderConfigDescriptor
    //      MP4DecSpecificInfoDescriptor
    //   MP4SLConfigDescriptor
    enum {
        TAG_ES = 3, TAG_DC = 4, TAG_DSI = 5, TAG_SLC = 6
    };

    // version/flags
    avio_rb32();
    if (avio_r8() != TAG_ES) {
        return ERR_FAIL;
    }
    getsize();
    // ESID
    avio_rb16();
    // flags(url(bit 6); ocr(5); streamPriority (0-4)):
    avio_r8();

    if (avio_r8() != TAG_DC) {
        return ERR_FAIL;
    }
    getsize();
    if (avio_r8() != 0x40) { /* not MPEG-4 audio */
        return ERR_FAIL;
    }
    // flags
    avio_r8();
    // buffer size (24 bits)
    atr[sel_a_trak].buffersize = avio_rb16() << 8;
    atr[sel_a_trak].buffersize |= avio_r8();
    // bitrate
    atr[sel_a_trak].bitrate.max = avio_rb32();
    atr[sel_a_trak].bitrate.avg = avio_rb32();

    if (avio_r8() != TAG_DSI) {
        return ERR_FAIL;
    }
    atr[sel_a_trak].asc.size = getsize();
    if (atr[sel_a_trak].asc.size > sizeof(atr[sel_a_trak].asc.buf)) {
        return ERR_FAIL;
    }
    // get AudioSpecificConfig
    avio_rdata(mp4r->fin, atr[sel_a_trak].asc.buf, atr[sel_a_trak].asc.size);

    if (avio_r8() != TAG_SLC) {
        return ERR_FAIL;
    }
    getsize();
    // "predefined" (no idea)
    avio_r8();

    return size;
}

/**
 *  CodecSpecific: 'dOps'
 *
 *  unsigned int(8) Version;
 *  unsigned int(8) OutputChannelCount;
 *  unsigned int(16) PreSkip;
 *  unsigned int(32) InputSampleRate;
 *  signed int(16) OutputGain;
 *  unsigned int(8) ChannelMappingFamily;
 *  if (ChannelMappingFamily != 0) {
 *      ChannelMappingTable(OutputChannelCount);
 *
 *      class ChannelMappingTable (unsigned int(8) OutputChannelCount){
 *          unsigned int(8) StreamCount;
 *          unsigned int(8) CoupledCount;
 *          unsigned int(8 * OutputChannelCount) ChannelMapping;
 *      }
 *  }
 * */

static int mov_read_dops(mp4r_t *mp4r, int size)
{
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;
    OpusHeader *header = (OpusHeader *)atr[sel_a_trak].csc;

    // fprintf(stderr, "dopsin\n");
    header->magic_id = ATOM_TYPE_OPUS;

    // version
    header->version = avio_r8();
    // OutputChannelCount
    header->channels = avio_r8();
    // PreSkip
    header->preskip = avio_rb16();
    // InputSampleRate
    header->input_sample_rate = avio_rb32();
    // OutputGain
    header->gain = avio_rb16();
    // ChannelMappingFamily
    header->channel_mapping = avio_r8();

    /* fprintf(stderr, */
            /* "dopsin: version %d, channels %d, preskip %d, rate %d, gain %d, cmf %d\n", */
            /* header->version, header->channels, header->preskip, */
            /* header->input_sample_rate, header->gain, header->channel_mapping); */

    if (header->channel_mapping != 0
            && header->channel_mapping != 255) {
        // StreamCount
        header->nb_streams = avio_r8();
        // CoupledCount
        header->nb_coupled = avio_r8();
        for (int i = 0; i < header->channels; i++) {
            header->stream_map[i] = avio_r8();
        }
    }

    return size;
}

static int mov_read_aiac(mp4r_t *mp4r, uint32_t size)
{
    int sel_a_trak;
    IACHeader *header;
    int offset;

    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // Reserved (6 bytes)
    avio_rb32(); // reserved
    avio_rb16(); // reserved
    uint32_t data_reference_index = avio_rb16(); // data_reference_index
    avio_rb32(); // reserved
    avio_rb32(); // reserved
    uint16_t channel_count = avio_rb16();
    atr[sel_a_trak].channels = channel_count;
    uint16_t sample_size = avio_rb16();
    atr[sel_a_trak].bits = sample_size; // bitdepth
    avio_rb16(); // predefined
    uint32_t sample_rate = avio_rb32() >> 16;
    atr[sel_a_trak].samplerate = sample_rate;
    avio_rb16(); // reserved

    atr[sel_a_trak].csc = _dcalloc(1, sizeof(IACHeader), __FILE__, __LINE__);
    header = (IACHeader *)atr[sel_a_trak].csc;
    header->version = avio_r8();
    uint8_t param = avio_r8();
    header->ambix = param >> 6 & 0x3;
    header->layers = param >> 3 & 0x7;
    header->ambix_chs = avio_r8();

    for (int i=0; i<header->layers; ++i) {
        if (!(i%2)) {
            param = avio_r8();
            header->layout[i] = param >> 4 & 0xf;
        } else {
            header->layout[i] = param & 0xf;
        }
    }

    offset = 28 + 3 + (header->layers + 1)/2;
    header->clen = size - offset;
    header->codec_config = (uint8_t *)_dmalloc(header->clen, __FILE__, __LINE__);
    avio_rdata(mp4r->fin, header->codec_config, header->clen);
    iac_header_parse_codec_spec (header);
    return size;
}

static int mov_read_stts(mp4r_t *mp4r, int size)
{
    uint32_t versionflags;
    uint32_t entry_count;
    uint32_t sample_count, sample_delta;
    uint32_t *count_buf, *delta_buf;
    uint32_t x, cnt;

    /* if (size < 16) { //min stts size */
        /* return ERR_FAIL; */
    /* } */

    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    versionflags = avio_rb32();

    // entry count
    entry_count = avio_rb32();
    /* fprintf(stderr, "sttsin: entry_count %d\n", entry_count); */
    if (!entry_count)
        return size;

    if (!(entry_count + 1)) {
        return ERR_FAIL;
    }

    count_buf = (uint32_t *)_dcalloc(sizeof(uint32_t), entry_count + 1, __FILE__,
                                     __LINE__);
    delta_buf = (uint32_t *)_dcalloc(sizeof(uint32_t), entry_count + 1, __FILE__,
                                     __LINE__);

    int count_detas = 0;
    for (cnt = 0, x = 0; cnt < entry_count; cnt++) {
        // chunk entry loop
        sample_count = avio_rb32();
        sample_delta = avio_rb32();
        count_buf[cnt] = sample_count;
        delta_buf[cnt] = sample_delta;
        count_detas += sample_count;
        // fprintf(stderr, "sttsin: entry_count %d, sample_count %u\n", entry_count,
        // sample_count);
        // fprintf(stderr, "sttsin: entry_count %d, sample_delta %u\n", entry_count,
        // sample_delta);
    }

    // fprintf(stderr, "trak id %d\n", sel_a_trak);
    atr[sel_a_trak].frame.deltas = (uint32_t *)_dcalloc(count_detas + 1,
                                   sizeof(*atr[sel_a_trak].frame.deltas), __FILE__, __LINE__);
    if (!atr[sel_a_trak].frame.deltas) {
        return ERR_FAIL;
    }

    for (cnt = 0, x = 0; cnt < entry_count; cnt++) {
        // chunk entry loop
        sample_count = count_buf[cnt];
        sample_delta = delta_buf[cnt];
        for (int i = 0; i < sample_count; i++) {
            //
            atr[sel_a_trak].frame.deltas[x] = sample_delta;
            x++;
        }
    }

    if (count_buf) {
        _dfree(count_buf, __FILE__, __LINE__);
    }
    if (delta_buf) {
        _dfree(delta_buf, __FILE__, __LINE__);
    }

    return size;
}

static int mov_read_stsc(mp4r_t *mp4r, int size)
{
    uint32_t versionflags;
    uint32_t entry_count;
    uint32_t first_chunk;
    uint32_t samples_in_chunk;
    uint32_t sample_desc_index;
    int used_bytes = 0;

    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    versionflags = avio_rb32();
    used_bytes += 4;
    // Sample size
    entry_count = avio_rb32();
    /* fprintf(stderr, "stscin: entry_count %d\n", entry_count); */
    if (!entry_count)
        return size;
    used_bytes += 4;
    atr[sel_a_trak].frame.chunk_count = entry_count;
    atr[sel_a_trak].frame.chunks = (chunkinfo *)_dcalloc(sizeof(chunkinfo),
                                   entry_count + 1, __FILE__, __LINE__);
    if (!atr[sel_a_trak].frame.chunks) {
        return ERR_FAIL;
    }

    for (int i = 0; used_bytes < size && i < entry_count; i++) {
        first_chunk = avio_rb32();
        samples_in_chunk = avio_rb32();
        sample_desc_index = avio_rb32();
        atr[sel_a_trak].frame.chunks[i].first_chunk = first_chunk;
        atr[sel_a_trak].frame.chunks[i].sample_per_chunk = samples_in_chunk;
        // fprintf(stderr, "stscin: entry_count %d, first_chunk %u\n", entry_count,
        // first_chunk);
        // fprintf(stderr, "stscin: entry_count %d, samples_in_chunk %u\n", entry_count,
        // samples_in_chunk);
        // fprintf(stderr, "stscin: entry_count %d, sample_count %u\n", entry_count,
        // sample_desc_index);
    }
    atr[sel_a_trak].frame.chunks[entry_count].first_chunk = 0xFFFFFFFF; // dummy
    atr[sel_a_trak].frame.chunks[entry_count].sample_per_chunk = 0;

    return size;
}

static int mov_read_stsz(mp4r_t *mp4r, int size)
{
    int cnt;
    uint32_t versionflags;
    uint32_t sample_size;
    uint32_t sample_count;
    int used_bytes = 0;

    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    versionflags = avio_rb32();
    used_bytes += 4;
    // Sample size
    sample_size = avio_rb32();
    used_bytes += 4;
    atr[sel_a_trak].sample_size = sample_size;
    /* atr[sel_a_trak].frame.maxsize = sample_size; */
    // Number of entries
    sample_count = avio_rb32();
    /* fprintf(stderr, "stszin: entry_count %d, sample_size %u\n", sample_count, sample_size); */
    if (!sample_count)
        return size;
    used_bytes += 4;
    atr[sel_a_trak].frame.ents = sample_count;

    if (!(atr[sel_a_trak].frame.ents + 1)) {
        return ERR_FAIL;
    }


    atr[sel_a_trak].frame.sizes = (uint32_t *)_dcalloc(atr[sel_a_trak].frame.ents +
                                  1,
                                  sizeof(*atr[sel_a_trak].frame.sizes), __FILE__, __LINE__);
    if (!atr[sel_a_trak].frame.sizes) {
        return ERR_FAIL;
    }

    uint32_t fsize;
    for (cnt = 0; cnt < atr[sel_a_trak].frame.ents; cnt++) {
        if (sample_size == 0) {
            fsize = avio_rb32();
            if (atr[sel_a_trak].frame.maxsize < fsize) {
                atr[sel_a_trak].frame.maxsize = fsize;
            }
            atr[sel_a_trak].frame.sizes[cnt] = fsize;
        } else {
            fsize = sample_size;
        }
        atr[sel_a_trak].frame.sizes[cnt] = fsize;
    }

    return size;
}

static int mov_read_stco(mp4r_t *mp4r, int size)
{
    uint32_t versionflags;
    uint32_t fofs, x;
    uint32_t cnt;

    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    versionflags = avio_rb32();
    // Number of entries
    uint32_t entry_count = avio_rb32();
    /* fprintf(stderr, "stcoin: entry_count %d\n", entry_count); */
    if (entry_count < 1) {
        mp4r->atom = s_sgpd;
        return size;
    }
    // first chunk offset

    atr[sel_a_trak].frame.offs = (uint32_t *)_dcalloc(atr[sel_a_trak].frame.ents +
                                 1,
                                 sizeof(*atr[sel_a_trak].frame.offs), __FILE__, __LINE__);
    if (!atr[sel_a_trak].frame.offs) {
        return ERR_FAIL;
    }

    for (cnt = 0, x = 0; cnt < entry_count; cnt++) {
        // chunk entry loop
        fofs = avio_rb32();
        // fprintf(stderr, "stcoin: entry (%d) offset %d\n", cnt, fofs);
        uint32_t sample_size;
        for (int i = 0; i < atr[sel_a_trak].frame.chunk_count; i++) {
            sample_size = atr[sel_a_trak].frame.chunks[i].sample_per_chunk;
            if (atr[sel_a_trak].frame.chunks[i].first_chunk <= cnt + 1 &&
                    cnt + 1 < atr[sel_a_trak].frame.chunks[i + 1].first_chunk) {
                // if first_chunk == 0xffffffff then end of entries...
                break;
            }
        }
        for (int i = 0; i < sample_size; i++, x++) {
            if (i > 0) {
                fofs += atr[sel_a_trak].frame.sizes[x - 1];
            }
            atr[sel_a_trak].frame.offs[x] = fofs;
        }
    }

    return size;
}

int mov_read_sgpd (mp4r_t *mp4r, int size)
{
    uint8_t buf[5];
    uint32_t v;
    uint32_t len, count, default_len;
    int i;
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;
    v = avio_r8();

    // flags
    avio_r8();
    avio_rb16();

    buf[4] = 0;
    avio_rdata(mp4r->fin, buf, 4);

    if (v > 0)
        default_len = avio_rb32();
    count = avio_rb32();

    /* fprintf(stderr, "sgpd : count %d\n", count); */
    // demixing mode information.
    if (!memcmp("admi", buf, 4)) {
        IACHeader *header = (IACHeader *)atr[sel_a_trak].csc;
        for (int i=0; i<header->entry_count; ++i) {
            _dfree(header->demix_entries[i], __FILE__, __LINE__);
        }
        _dfree(header->demix_entries, __FILE__, __LINE__);
        _dfree(header->entry_len, __FILE__, __LINE__);

        header->entry_count = count;
        header->demix_entries = (uint8_t **)_dmalloc(sizeof (uint8_t *) * count, __FILE__, __LINE__);
        header->entry_len = (int *)_dmalloc(sizeof (int) * count, __FILE__, __LINE__);
        for (i=0; i<count; ++i) {
            len = default_len;
            if (v == 1 && !default_len)
                len = avio_rb32();

            header->demix_entries[i] = (uint8_t*)_dmalloc(sizeof (uint8_t) * len, __FILE__, __LINE__);
            avio_rdata(mp4r->fin, header->demix_entries[i], len);
            header->entry_len[i] = len;
            /* fprintf(stderr, "sgpd (dmix): entry %d: %d\n", i, header->demix_entries[i]); */
        }
    } else if (!memcmp("iasm", buf, 4)) {
        IACHeader *header = (IACHeader *)atr[sel_a_trak].csc;
        len = default_len;
        if (v == 1 && !default_len)
            len = avio_rb32();

        header->metadata = (uint8_t*)_dmalloc(sizeof (uint8_t) * len, __FILE__, __LINE__);
        avio_rdata(mp4r->fin, header->metadata, len);
        header->mlen = len;
        /* fprintf(stderr, "sgpd (iasm): meta data size %d\n", len); */
    } else {
        fprintf(stderr, "Do not support %s\n", buf);
    }

    return size;
}

int mov_read_sbgp (mp4r_t *mp4r, int size)
{
    uint8_t buf[5];
    uint32_t v;
    uint32_t count;
    int i;
    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;
    v = avio_r8();

    // flags
    avio_r8();
    avio_rb16();

    buf[4] = 0;
    avio_rdata(mp4r->fin, buf, 4);
    if (v == 1)
        avio_rb32();
    count = avio_rb32();

    /* fprintf(stderr, "sbgp : count %d, samples %d\n", count, atr[sel_a_trak].frame.ents); */
    if (!memcmp("admi", buf, 4)) {
        int scnt = 0, ridx, cnt;
        IACHeader *header = (IACHeader *)atr[sel_a_trak].csc;

        header->demix_modes = (uint8_t *)_drealloc(header->demix_modes,
                    sizeof(*header->demix_modes) * atr[sel_a_trak].frame.ents,
                __FILE__, __LINE__);
        for (i=0; i<count; ++i) {
            cnt = avio_rb32();
            ridx = avio_rb32();

            /* fprintf(stderr, "sbgp (dmix): entry %d, count %d, reference index %d\n", */
                    /* i, cnt, ridx); */
            for (int k=0; k<cnt; ++k)
                header->demix_modes[header->dents + scnt + k] = ridx - 1;
            scnt += cnt;
        }
        /* fprintf(stderr, "sbgp : sample count %d\n", scnt); */

        if (header->dents + scnt != atr[sel_a_trak].frame.ents) {
            fprintf(stderr, "last demix mode count %d, and new %d ones, is not equal %d\n",
                    header->dents, scnt, atr[sel_a_trak].frame.ents);
        } else
            header->dents += scnt;
    } else if (!memcmp("iasm", buf, 4)) {
        int ridx, cnt;
        for (i=0; i<count; ++i) {
            cnt = avio_rb32();
            ridx = avio_rb32();

            /* fprintf(stderr, "sbgp (iant): entry %d, count %d, reference index %d\n", */
                    /* i, cnt, ridx); */
        }
    } else {
        fprintf(stderr, "Do not support %s\n", buf);
    }

    return size;
}

static int mov_read_moof (mp4r_t *mp4r, int size)
{
    mp4r->moof_position = ftell(mp4r->fin) - 8;
    /* fprintf(stderr, "moof pos %ld, size %d\n", mp4r->moof_position, size); */
    return size;
}

static int mov_read_tfhd (mp4r_t *mp4r, int size)
{
    int vf = avio_rb32();
    int id = avio_rb32();

    /* fprintf(stderr, "find track id %d.\n", id); */
    for (int i=0; i<mp4r->num_a_trak; ++i) {
        if (mp4r->a_trak[i].track_id == id) {
            mp4r->sel_a_trak = i;
            /* fprintf(stderr, "track id %d is found.\n", id); */
            break;
        }
    }

    /* base_data_offset */
    if (vf & 0x1) {
        avio_rb32();
        avio_rb32();
    }

    // sample_description_index
    if (vf & 0x2) avio_rb32();
    // default_sample_duration
    if (vf & 0x8)
        mp4r->a_trak[mp4r->sel_a_trak].sample_duration = avio_rb32();
    // default_sample_size
    if (vf & 0x10)
        mp4r->a_trak[mp4r->sel_a_trak].sample_size = avio_rb32();
    // default_sample_flags
    /* if (vf & 0x20) avio_rb32(); */

    return size;
}

static int mov_read_trun (mp4r_t *mp4r, int size)
{
    int cnt;
    uint32_t vf;
    uint32_t sample_size;
    uint32_t sample_count;
    int used_bytes = 0;
    uint32_t offset = 0;

    int sel_a_trak;
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    vf = avio_rb32();
    used_bytes += 4;
    // Number of entries
    sample_count = avio_rb32();
    /* fprintf(stderr, "trunin: sle_a_trak %d, ents %d, entry_count %d, sample_size %u\n", */
            /* sel_a_trak, atr[sel_a_trak].frame.ents, sample_count, sample_size); */
    if (!sample_count)
        return size;
    used_bytes += 4;
    cnt = atr[sel_a_trak].frame.ents;
    atr[sel_a_trak].frame.ents += sample_count;

    if (!(atr[sel_a_trak].frame.ents + 1)) {
        return ERR_FAIL;
    }

    atr[sel_a_trak].frame.sizes =
        (uint32_t *)_drealloc(atr[sel_a_trak].frame.sizes,
                sizeof(*atr[sel_a_trak].frame.sizes) * atr[sel_a_trak].frame.ents,
                __FILE__, __LINE__);
    atr[sel_a_trak].frame.offs =
        (uint32_t *)_drealloc(atr[sel_a_trak].frame.offs,
                sizeof(*atr[sel_a_trak].frame.offs) * atr[sel_a_trak].frame.ents,
                __FILE__, __LINE__);


    if (!atr[sel_a_trak].frame.sizes) {
        return ERR_FAIL;
    }

    if (vf & 0x1)
        offset = avio_rb32();
    /* fprintf(stderr, "offset %u\n", offset); */

    if (vf & 0x04) avio_rb32();

    uint32_t fsize;
    offset += mp4r->moof_position;
    for (; cnt < atr[sel_a_trak].frame.ents; cnt++) {
        if (vf & 0x100) avio_rb32();
        if (vf & 0x200) {
            fsize = avio_rb32();
            if (atr[sel_a_trak].frame.maxsize < fsize) {
                atr[sel_a_trak].frame.maxsize = fsize;
            }
            atr[sel_a_trak].frame.sizes[cnt] = fsize;
            atr[sel_a_trak].frame.offs[cnt] = offset;
            /* fprintf(stderr, "entry %d : size  %d, offset %u\n", cnt, fsize, offset); */
            offset += fsize;
        }
        if (vf & 0x400) avio_rb32();
        if (vf & 0x800) avio_rb32();
    }

    return size;
}

int parse(mp4r_t *mp4r, uint32_t *sizemax)
{
    long apos = 0;
    long aposmax = ftell(mp4r->fin) + *sizemax;
    uint32_t size;

    if (mp4r->atom->atom_type != MOV_ATOM_NAME) {
        if (mp4r->logger) {
            fprintf(mp4r->logger, "parse error: root is not a 'name' atom_type\n");
        }
        return ERR_FAIL;
    }
    /* fprintf(stderr, "looking for '%s'\n", (char *)mp4r->atom->data); */

    // search for atom in the file
    while (1) {
        char name[4];
        uint32_t tmp;

        apos = ftell(mp4r->fin);
        // fprintf(stderr, "parse: while pos %ld\n", apos);
        if (apos >= (aposmax - 8)) {
            fprintf(stderr, "parse error: atom '%s' not found\n", (char *)mp4r->atom->opaque);
            return ERR_FAIL;
        }
        if ((tmp = avio_rb32()) < 8) {
            fprintf(stderr, "invalid atom size %x @%lx\n", tmp, ftell(mp4r->fin));
            return ERR_FAIL;
        }

        size = tmp;
        if (avio_rdata(mp4r->fin, name, 4) != 4) {
            // EOF
            fprintf(stderr, "can't read atom name @%lx\n", ftell(mp4r->fin));
            return ERR_FAIL;
        }

        /* fprintf(mp4r->logger, "atom: '%.4s'(%x)\n", name, size); */
        /* fprintf(stderr, "atom: '%.4s'(%x)\n", name, size); */

        if (!memcmp(name, mp4r->atom->opaque, 4)) {
            // fprintf(stderr, "OK\n");
#if 1
            atom_dump(mp4r->fin, apos, tmp);
#endif
            fseek(mp4r->fin, apos + 8, SEEK_SET);
            break;
        }
        //fprintf(stderr, "\n");

        fseek(mp4r->fin, apos + size, SEEK_SET);
    }
    *sizemax = size;
    mp4r->atom++;
    /* fprintf(stderr, "parse: pos %ld\n", apos); */
    if (mp4r->atom->atom_type == MOV_ATOM_DATA) {
        int err = ((int(*)(mp4r_t *, int)) mp4r->atom->opaque)(mp4r, size - 8);
        if (err < ERR_OK) {
            fseek(mp4r->fin, apos + size, SEEK_SET);
            return err;
        }
        mp4r->atom++;
    }
    if (mp4r->atom->atom_type == MOV_ATOM_DESCENT) {
        long apos = ftell(mp4r->fin);;

        //fprintf(stderr, "descent\n");
        mp4r->atom++;
        while (mp4r->atom->atom_type != MOV_ATOM_STOP) {
            uint32_t subsize = size - 8;
            int ret;
            if (mp4r->atom->atom_type == MOV_ATOM_ASCENT) {
                mp4r->atom++;
                break;
            }
            // fseek(mp4r->fin, apos, SEEK_SET);
            if ((ret = parse(mp4r, &subsize)) < 0) {
                return ret;
            }
        }
        //fprintf(stderr, "ascent\n");
    }

    /* fprintf(stderr, "parse: pos+size %ld\n", apos + size); */
    fseek(mp4r->fin, apos + size, SEEK_SET);

    return ERR_OK;
}

static int mov_read_moov(mp4r_t *mp4r, int sizemax);
static int mov_read_hdlr(mp4r_t *mp4r, int size);

static avio_context g_head[] = {
    { MOV_ATOM_NAME, "ftyp" },
    { MOV_ATOM_DATA, mov_read_ftyp },
    { 0 }
};

static avio_context g_moov[] = {
    { MOV_ATOM_NAME, "moov" },
    { MOV_ATOM_DATA, mov_read_moov },
    { 0 }
};

static avio_context g_moof[] = {
    { MOV_ATOM_NAME, "moof" },
    { MOV_ATOM_DATA, mov_read_moof },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "traf" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "tfhd" },
    { MOV_ATOM_DATA, mov_read_tfhd },
    { MOV_ATOM_NAME, "trun" },
    { MOV_ATOM_DATA, mov_read_trun },
    { MOV_ATOM_NAME, "sgpd" },
    { MOV_ATOM_DATA, mov_read_sgpd },
    { MOV_ATOM_NAME, "sbgp" },
    { MOV_ATOM_DATA, mov_read_sbgp },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_ASCENT },
    { 0 }
};


static avio_context g_mvhd[] = {
    { MOV_ATOM_NAME, "mvhd" },
    { MOV_ATOM_DATA, mov_read_mvhd },
    { 0 }
};

static avio_context g_trak[] = {
    { MOV_ATOM_NAME, "trak" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "tkhd" },
    { MOV_ATOM_DATA, mov_read_tkhd },
    { MOV_ATOM_NAME, "mdia" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "mdhd" },
    { MOV_ATOM_DATA, mov_read_mdhd },
    { MOV_ATOM_NAME, "hdlr" },
    { MOV_ATOM_DATA, mov_read_hdlr },
    { 0 }
};

static avio_context trak_opus[] = {
    { MOV_ATOM_NAME, "hdlr" },
    { MOV_ATOM_NAME, "minf" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "smhd" },
    { MOV_ATOM_NAME, "dinf" },
    { MOV_ATOM_NAME, "stbl" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "stsd" },
    { MOV_ATOM_DATA, mov_read_stsd },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "Opus" },
    { MOV_ATOM_DATA, mov_read_opus },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "dOps" },
    { MOV_ATOM_DATA, mov_read_dops },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_NAME, "stts" },
    { MOV_ATOM_DATA, mov_read_stts },
    { MOV_ATOM_NAME, "stsc" },
    { MOV_ATOM_DATA, mov_read_stsc },
    { MOV_ATOM_NAME, "stsz" },
    { MOV_ATOM_DATA, mov_read_stsz },
    { MOV_ATOM_NAME, "stco" },
    { MOV_ATOM_DATA, mov_read_stco },
    { 0 }
};

static avio_context trak_aac[] = {
    { MOV_ATOM_NAME, "hdlr" },
    { MOV_ATOM_NAME, "minf" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "smhd" },
    { MOV_ATOM_NAME, "dinf" },
    { MOV_ATOM_NAME, "stbl" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "stsd" },
    { MOV_ATOM_DATA, mov_read_stsd },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "mp4a" },
    { MOV_ATOM_DATA, mov_read_mp4a },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "esds" },
    { MOV_ATOM_DATA, mov_read_esds },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_NAME, "stts" },
    { MOV_ATOM_DATA, mov_read_stts },
    { MOV_ATOM_NAME, "stsc" },
    { MOV_ATOM_DATA, mov_read_stsc },
    { MOV_ATOM_NAME, "stsz" },
    { MOV_ATOM_DATA, mov_read_stsz },
    { MOV_ATOM_NAME, "stco" },
    { MOV_ATOM_DATA, mov_read_stco },
    { 0 }
};

static avio_context trak_aiac[] = {
    { MOV_ATOM_NAME, "hdlr" },
    { MOV_ATOM_NAME, "minf" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "smhd" },
    { MOV_ATOM_NAME, "dinf" },
    { MOV_ATOM_NAME, "stbl" },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "stsd" },
    { MOV_ATOM_DATA, mov_read_stsd },
    { MOV_ATOM_DESCENT },
    { MOV_ATOM_NAME, "aiac" },
    { MOV_ATOM_DATA, mov_read_aiac },
    { MOV_ATOM_ASCENT },
    { MOV_ATOM_NAME, "stts" },
    { MOV_ATOM_DATA, mov_read_stts },
    { MOV_ATOM_NAME, "stsc" },
    { MOV_ATOM_DATA, mov_read_stsc },
    { MOV_ATOM_NAME, "stsz" },
    { MOV_ATOM_DATA, mov_read_stsz },
    { MOV_ATOM_NAME, "stco" },
    { MOV_ATOM_DATA, mov_read_stco },
    { MOV_ATOM_NAME, "sgpd" },
    { MOV_ATOM_DATA, mov_read_sgpd },
    { MOV_ATOM_NAME, "sbgp" },
    { MOV_ATOM_DATA, mov_read_sbgp },
    { MOV_ATOM_NAME, "sgpd" },
    { MOV_ATOM_DATA, mov_read_sgpd },
    { MOV_ATOM_NAME, "sbgp" },
    { MOV_ATOM_DATA, mov_read_sbgp },
    { 0 }
};




int mov_read_hdlr(mp4r_t *mp4r, int size)
{
    uint8_t buf[5];

    buf[4] = 0;
    // version/flags
    avio_rb32();
    // pre_defined
    avio_rb32();
    // Component subtype
    avio_rdata(mp4r->fin, buf, 4);
    if (mp4r->logger) {
        fprintf(mp4r->logger, "*track media type: '%s': ", buf);
    }
    fprintf(stderr, "*track media type: '%s'\n", buf);

    if (!memcmp("vide", buf, 4)) {
        fprintf(stderr, "unsupported, skipping in hdlrin\n");
    } else if (!memcmp("soun", buf, 4)) {
        // fprintf(stderr, "sound\n");
        mp4r->atom = trak_aiac;
    }
    // reserved
    avio_rb32();
    avio_rb32();
    avio_rb32();
    // name
    // null terminate
    avio_r8();

    return size;
}

int mov_read_moov(mp4r_t *mp4r, int sizemax)
{
    long apos = ftell(mp4r->fin);
    uint32_t atomsize;
    avio_context *old_atom = mp4r->atom;
    int err, ret = sizemax;
    int pos;
    int ntrack = 0;

    mp4r->atom = g_mvhd;
    atomsize = sizemax + apos - (pos = ftell(mp4r->fin));
    if (parse(mp4r, &atomsize) < 0) {
        mp4r->atom = old_atom;
        return ERR_FAIL;
    }

    while (1) {
        fprintf(stderr, "TRAK:\n");
        mp4r->atom = g_trak;
        atomsize = sizemax + apos - ftell(mp4r->fin);
        if (atomsize < 8) {
            break;
        }
        // fprintf(stderr, "PARSE(%x)\n", atomsize);
        err = parse(mp4r, &atomsize);
        //fprintf(stderr, "SIZE: %x/%x\n", atomsize, sizemax);
        if (err >= 0) {
            if (mp4r->tot_track_scan < mp4r->next_track_id-1) {
                continue;
            }
        }
        if (err != ERR_UNSUPPORTED) {
            ret = err;
            break;
        }
        //fprintf(stderr, "UNSUPP\n");
    }

    // mp4r->atom = old_atom;
    return ret;
}

int mp4demux_audio(mp4r_t *mp4r, int trakn, int *delta)
{
    audio_rtr_t *atr = mp4r->a_trak;

    if (atr[trakn].frame.current > atr[trakn].frame.ents) {
        return ERR_FAIL;
    }


    if (atr[trakn].sample_size != 0) {
        // CBR.. stsz
        atr[trakn].bitbuf.size = atr[trakn].sample_size;
    } else {
        // VBR
        if (atr[trakn].frame.sizes)
            atr[trakn].bitbuf.size = atr[trakn].frame.sizes[atr[trakn].frame.current];
    }

    mp4demux_seek(mp4r, trakn, atr[trakn].frame.current);

    if (fread(atr[trakn].bitbuf.data, 1, atr[trakn].bitbuf.size, mp4r->fin)
            != atr[trakn].bitbuf.size) {
        fprintf(stderr, "can't read frame data(frame %d@0x%x)\n",
                atr[trakn].frame.current,
                atr[trakn].frame.sizes[atr[trakn].frame.current]);
        return ERR_FAIL;
    }

    if (delta) {
        if (atr[trakn].frame.deltas)
            *delta = atr[trakn].frame.deltas[atr[trakn].frame.current];
        else
            *delta = atr[trakn].sample_duration;
    }

    atr[trakn].frame.current++;
    return ERR_OK;
}

int mp4demux_seek(mp4r_t *mp4r, int trakn, int framenum)
{
    audio_rtr_t *atr = mp4r->a_trak;

    if (framenum > atr[trakn].frame.ents) {
        return ERR_FAIL;
    }
    if (atr[trakn].frame.offs) {
        fseek(mp4r->fin, atr[trakn].frame.offs[framenum], SEEK_SET);
    }
    atr[trakn].frame.current = framenum;

    return ERR_OK;
}

int mp4demux_getframenum(mp4r_t *mp4r, int trakn, uint32_t offs)
{
    audio_rtr_t *atr = mp4r->a_trak;
    int ents = atr[trakn].frame.ents;
    int m, l, r;
    uint32_t m_offs, l_offs, r_offs;

    m = ents / 2;
    l = 0;
    r = ents;
    while (0 < m && m < ents) {
        m_offs = atr[trakn].frame.offs[m];
        l_offs = atr[trakn].frame.offs[l];
        r_offs = (r == ents) ? (0xffffffff) : (atr[trakn].frame.offs[r] +
                                               atr[trakn].frame.sizes[r] - 1);
        if (l != m && m != r) {
            if (offs < m_offs) {
                r = m;
                m = m / 2;
                continue;
            }
            if (m_offs < offs) {
                l = m;
                m = m + (r - m) / 2;
                continue;
            }
        }
        if (l == m || m == r || m_offs == offs) {
            break;
        }
    }

    return (m);
}

void mp4demux_info(mp4r_t *mp4r, int trakn, int print)
{
    audio_rtr_t *atr = mp4r->a_trak;
    fprintf(stderr, "Modification Time:\t\t%s\n", mp4time(mp4r->mtime));
    if (trakn < mp4r->num_a_trak) {
        int s = (trakn < 0) ? 0 : trakn;
        int e = (trakn < 0) ? mp4r->num_a_trak - 1: trakn;
        for (int i = s; i <= e; i++) {
            if (print) {
                fprintf(stderr, "Audio track#%d -------------\n", i);
                fprintf(stderr, "Samplerate:\t\t%u\n", atr[i].samplerate);
                fprintf(stderr, "Total samples:\t\t%d\n", atr[i].samples);
                fprintf(stderr, "Total channels:\t\t%d\n", atr[i].channels);
                fprintf(stderr, "Bits per sample:\t%d\n", atr[i].bits);
                fprintf(stderr, "Frames:\t\t\t%d\n", atr[i].frame.ents);
            }
        }
    }
}

int mp4demux_close(mp4r_t *mp4r)
{
    if (mp4r) {
#define FREE(x) if(x){_dfree(x,__FILE__,__LINE__);x=0;}
        audio_rtr_t *atr = mp4r->a_trak;
        for (int i = 0; i < mp4r->num_a_trak; i++) {
            FREE(atr[i].frame.chunks);
            FREE(atr[i].frame.offs);
            FREE(atr[i].frame.sizes);
            FREE(atr[i].frame.deltas);
            FREE(atr[i].bitbuf.data);
            FREE(atr[i].csc);
        }
        if (mp4r->fin) {
            fclose(mp4r->fin);
        }
        mp4r->fin = NULL;

        for (int i = 0; i < mp4r->tag.extnum; i++) {
            free((void *)mp4r->tag.ext[i].name);
            free((void *)mp4r->tag.ext[i].data);
        }
        mp4r->tag.extnum = 0;
        FREE(mp4r);
    }
    return ERR_OK;
}

mp4r_t *mp4demux_open(char *name, FILE *logger)
{
    mp4r_t *mp4r = NULL;
    FILE *fin;
    uint32_t atomsize;
    int ret;
    int frag = 0;

    fin = fopen(name, "rb");
    if (!fin) {
        goto err;
    }

    mp4r = (mp4r_t *)_dcalloc(1, sizeof(mp4r_t), __FILE__, __LINE__);
    if (mp4r == NULL) {
        goto err;
    }
    mp4r->fin = fin;
    mp4r->logger = logger;
    atom_set_logger(logger);

    if (mp4r->logger) {
        fprintf(mp4r->logger, "**** MP4 header ****\n");
    }
    mp4r->atom = g_head; // ftyp
    atomsize = INT_MAX;
    if (parse(mp4r, &atomsize) < 0) {
        goto err;
    }

    //////////////////////////
    mp4r->atom = g_moov;
    atomsize = INT_MAX;
    rewind(mp4r->fin);
    if ((ret = parse(mp4r, &atomsize)) < 0) {
        fprintf(stderr, "parse:%d\n", ret);
        goto err;
    }

    /* fprintf(stderr, "parse end.\n"); */
    // alloc frame buffer
    for (int i = 0; i < mp4r->num_a_trak; i++) {
        /* fprintf(stderr, "audio track %d, sample count %d.\n", i, mp4r->a_trak[i].frame.ents); */
        if (!mp4r->a_trak[i].frame.ents)
            frag = 1;
    }

    if (frag) {
        mp4r->moof_flag = 1;
        while (!ret) {
            mp4r->atom = g_moof;
            atomsize = INT_MAX;
            if ((ret = parse(mp4r, &atomsize)) < 0) {
                fprintf(stderr, "parse:%d\n", ret);
                break;
            }
        }
    }

    for (int i = 0; i < mp4r->num_a_trak; i++) {
        /* fprintf(stderr, "trak %d : allocate buffer size %d.\n", i, mp4r->a_trak[i].frame.maxsize); */
        mp4r->a_trak[i].bitbuf.data = (uint8_t *)_dcalloc(1,
                                      mp4r->a_trak[i].frame.maxsize + 1, __FILE__, __LINE__);
        if (!mp4r->a_trak[i].bitbuf.data) {
            goto err;
        }
    }


    //////////////////////////

    mp4demux_info(mp4r, -1, 1);
    fprintf(stderr, "********************\n");

    return mp4r;

err:
    if (mp4r != NULL) {
        mp4demux_close(mp4r);
    }
    return NULL;
}

int mp4demux_gettag(mp4r_t *mp4r, const char *name, char *data, int size)
{
    for (int i = 0; i < mp4r->tag.extnum; i++) {
        if (strcmp(mp4r->tag.ext[i].name, name) == 0) {
            strncpy(data, mp4r->tag.ext[i].data, size);
            data[size - 1] = 0;
            return ERR_OK;
        }
    }
    return ERR_FAIL;
}
