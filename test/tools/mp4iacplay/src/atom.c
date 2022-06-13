#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <memory.h>
#include "atom.h"
#include "a2b_endian.h"
#include "dmemory.h"

static FILE *_logger;
static void hex_dump(char *desc, void *addr, int len);

static struct tm *gmtime_rf(const time_t *timep, struct tm *result)
{
    struct tm *p = gmtime(timep);
    memset(result, 0, sizeof(*result));
    if (p) {
        *result = *p;
        p = result;
    }
    return p;
}

void atom_set_logger(FILE *logger)
{
    _logger = logger;
}

static double atom_parse_fixed_point_32(uint32_t number)
{
    return (number >> 16) + ((double)(number & 0xffff) / (1 << 8));
}

static double atom_parse_fixed_point_16(uint32_t number)
{
    return (number >> 16) + ((double)(number & 0xffff) / (1 << 8));
}

static int atom_parse_header(atom *atom)
{
    atom_header *header;

    header = (atom_header *)atom->data;
    atom->type = header->type;
    atom->header_size = bswap32(header->size) == 1 ? 16 : 8;
    atom->size = bswap32(header->size);
    //printf("%*s", atom->depth * 2, "");
    if (_logger) {
        fprintf(_logger, "[%.*s %u@%08x]\n", 4, (char *)&atom->type,
                bswap32(*(uint32_t *)atom->data), atom->apos);
    }

    return 0;
}

static void atom_free(atom *atom)
{
    if (atom->data) {
        _dfree(atom->data, __FILE__, __LINE__);
        atom->data = NULL;
        atom->data_end = NULL;
    }
}
static int atom_parse_ftyp(atom *atom)
{
    char *label;

    //printf("%*s", atom->depth * 2, "");
    if (_logger) {
        fprintf(_logger, "- labels\n");
    }
    hex_dump(NULL, atom->data, atom->data_end - atom->data);
    return 0;
}

static int atom_parse_styp(atom *atom)
{
    char *label;

    //printf("%*s", atom->depth * 2, "");
    if (_logger) {
        fprintf(_logger, "- labels\n");
    }
    hex_dump(NULL, atom->data, atom->data_end - atom->data);
    return 0;
}

static int atom_parse_mdat(atom *atom)
{
    return 0;
}

typedef struct atom_sidx atom_sidx;
struct atom_sidx {
    uint8_t  v;
    uint8_t  x[3];
    uint32_t id;
    uint32_t timescale;
    uint64_t ept;
    uint64_t offset;
    uint16_t reserved;
    uint16_t ref;
    uint32_t type : 1;
    uint32_t size : 31;
    uint32_t duration;
    uint32_t sap_type : 3;
    uint32_t sap_delta_time : 28;
};

static int atom_parse_sidx(atom *atom)
{
    atom_sidx *sidx;

    sidx = (atom_sidx *)(atom->data + atom->header_size);

    //printf("%*s", atom->depth * 2, "");
    if (_logger) fprintf(_logger, "- %u %u %u %u %u %u %u\n", sidx->v,
                             bswap32(sidx->id), bswap32(sidx->timescale), bswap64(sidx->ept),
                             bswap64(sidx->offset),
                             bswap16(sidx->reserved), bswap16(sidx->ref));
    if (bswap16(sidx->ref) != 1) {
        return -1;
    }
    if (_logger) {
        fprintf(_logger, "%*s", atom->depth * 2, "");
    }
    if (_logger) {
        fprintf(_logger, "- %u %u %u %u %u\n", sidx->type, sidx->size,
                bswap32(sidx->duration), sidx->sap_type, sidx->sap_delta_time);
    }

    return 0;
}

static int atom_parse_mvhd(atom *atom)
{
    atom_mvhd_v1 *v1;
    time_t t;
    struct tm tm;
    char created[80], modified[80];

    v1 = (atom_mvhd_v1 *)(atom->data + atom->header_size);

    t = bswap32(v1->created) - 2082844800;
    gmtime_rf(&t, &tm);
    strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", &tm);
    t = bswap32(v1->created) - 2082844800;
    gmtime_rf(&t, &tm);
    strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M:%S", &tm);
    //printf("%*s", atom->depth * 2, "");
    if (_logger) fprintf(_logger,
                             "- version %d, created %s, modified %s, scale %u, duration %g, speed %g, volume %g, next track %u\n",
                             v1->version,
                             created,
                             modified,
                             bswap32(v1->scale),
                             (double)bswap32(v1->duration) / (double)bswap32(v1->scale),
                             atom_parse_fixed_point_32(bswap32(v1->speed)),
                             atom_parse_fixed_point_16(bswap16(v1->volume)),
                             bswap32(v1->next_track_id));
    return 0;
}

static int atom_parse_meta(atom *atom)
{
    atom_meta *meta;

    meta = (atom_meta *)(atom->data + atom->header_size);
    //printf("%*s", atom->depth * 2, "");
    if (_logger) {
        fprintf(_logger, "- version %d\n", meta->version);
    }
    return 0;
}

typedef struct atom_opus {
    uint8_t  reserved1;
    uint32_t data_reference_index;
    uint8_t  reserved2[9];
    uint8_t  channelcount;
    uint16_t samplesize;
    uint8_t  predefined;
    uint8_t  reserved3;
    uint8_t samplerate_dec[4];
    uint8_t samplerate_dpnt[2];
} atom_opus_t;

static int atom_parse_opus(atom *atom)
{
    atom_opus_t *opus;
    uint8_t reserved1;
    uint32_t data_reference_index;
    uint8_t channelcount;
    uint8_t samplesize;
    float samplerate;
    uint32_t *sr_dec;
    uint16_t *sr_dpnt;

    opus = (atom_opus_t *)(atom->data + atom->header_size);
    data_reference_index = bswap32(opus->data_reference_index);
    channelcount = opus->channelcount;
    samplesize = bswap16(opus->samplesize);
    sr_dec = (uint32_t *)opus->samplerate_dec;
    sr_dpnt = (uint16_t *)opus->samplerate_dpnt;
    uint32_t x = bswap32(*sr_dec);
    uint16_t y = bswap16(*sr_dpnt);
    samplerate = (float)x + (float)y/1000;
    if (_logger) {
        fprintf(_logger, "data_reference_index = %d\n", data_reference_index);
    }
    if (_logger) {
        fprintf(_logger, "channelcount = %d\n", channelcount);
    }
    if (_logger) {
        fprintf(_logger, "samplesize = %d\n", samplesize);
    }
    if (_logger) {
        fprintf(_logger, "samplerate = %.4f\n", samplerate);
    }
    return 0;
}

typedef struct atom_dops {
    uint8_t  Version;
    uint8_t  OutputChannelCount;
    uint16_t PreSkip;
    uint32_t InputSampleRate;
    int16_t OutputGain;
    uint8_t ChannelMappingFamily;
    uint8_t StreamCount;
    uint8_t CoupleCount;
    uint8_t ChannelMapping[255];
} atom_dops_t;

static int atom_parse_dops(atom *atom)
{
    atom_dops_t *dops;
    uint8_t  Version;
    uint8_t  OutputChannelCount;
    uint16_t PreSkip;
    uint32_t InputSampleRate;
    int16_t OutputGain;
    uint8_t ChannelMappingFamily;
    uint8_t StreamCount = 0;
    uint8_t CoupleCount = 0;

    dops = (atom_dops_t *)(atom->data + atom->header_size);
    Version = dops->Version;
    OutputChannelCount = dops->OutputChannelCount;
    PreSkip = bswap16(dops->PreSkip);
    InputSampleRate = bswap32(dops->InputSampleRate);
    OutputGain = bswap16(dops->OutputGain);
    ChannelMappingFamily = dops->ChannelMappingFamily;
    if (ChannelMappingFamily != 0) {
        StreamCount = dops->StreamCount;
        CoupleCount = dops->CoupleCount;
    }
    if (_logger) {
        fprintf(_logger, "Version = %d\n", Version);
    }
    if (_logger) {
        fprintf(_logger, "OutputChannelCount = %d\n", OutputChannelCount);
    }
    if (_logger) {
        fprintf(_logger, "PreSkip = %d\n", PreSkip);
    }
    if (_logger) {
        fprintf(_logger, "InputSampleRate = %d\n", InputSampleRate);
    }
    if (_logger) {
        fprintf(_logger, "OutputGain = %d\n", OutputGain);
    }
    if (_logger) {
        fprintf(_logger, "ChannelMappingFamily = %d\n", ChannelMappingFamily);
    }
    if (ChannelMappingFamily != 0) {
        if (_logger) {
            fprintf(_logger, "StreamCount = %d\n", StreamCount);
        }
        if (_logger) {
            fprintf(_logger, "CoupleCount = %d\n", CoupleCount);
        }
        for (int i = 0; i < OutputChannelCount; i++) {
            if (_logger) {
                fprintf(_logger, "ChannelMapping[%d] = %d\n", i, dops->ChannelMapping[i]);
            }
        }
    }
    return 0;
}

static int atom_parse_stts(atom *atom)
{
    uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
    char *data = (char *)atom->data + atom->header_size;
    uint32_t versionflags;
    uint32_t entry_count;
    uint32_t sample_count;
    uint32_t sample_delta;
    int used_bytes = 0;

    versionflags = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
    }
    entry_count = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "Entry-count = %d\n", entry_count);
    }
    for (int i = 0; used_bytes < size && i < entry_count; i++) {
        sample_count = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        if (_logger) {
            fprintf(_logger, "Sample-count[%d] = %d\n", i, sample_count);
        }
        sample_delta = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        if (_logger) {
            fprintf(_logger, "Sample-delta[%d] = %d\n", i, sample_delta);
        }
    }
    return 0;
}

static int atom_parse_stsc(atom *atom)
{
    uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
    char *data = (char *)atom->data + atom->header_size;
    uint32_t versionflags;
    uint32_t entry_count;
    uint32_t first_chunk;
    uint32_t sample_per_chunk;
    uint32_t sample_per_desc_index;
    int used_bytes = 0;

    versionflags = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
    }
    entry_count = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "Entry-count = %d\n", entry_count);
    }
    for (int i = 0; used_bytes < size && i < entry_count; i++) {
        first_chunk = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        if (_logger) {
            fprintf(_logger, "First-chunk[%d] = %d\n", i, first_chunk);
        }
        sample_per_chunk = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        if (_logger) {
            fprintf(_logger, "Sample-per-chunk[%d] = %d\n", i, sample_per_chunk);
        }
        sample_per_desc_index = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        if (_logger) {
            fprintf(_logger, "Sample-per-desc-index[%d] = %d\n", i, sample_per_desc_index);
        }
    }
    return 0;
}

static int atom_parse_stsz(atom *atom)
{
    uint32_t size = bswap32(*(uint32_t *)atom->data) - atom->header_size;
    char *data = (char *)atom->data + atom->header_size;
    uint32_t versionflags;
    uint32_t sample_size;
    uint32_t sample_count;
    uint32_t entry_size;
    int used_bytes = 0;

    versionflags = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
    }
    sample_size = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "Sample-size = %d\n", sample_size);
    }
    sample_count = bswap32(*(uint32_t *)data);
    data += 4;
    used_bytes += 4;
    if (_logger) {
        fprintf(_logger, "Sample-count = %d\n", sample_count);
    }
    for (int i = 0; used_bytes < size && i < sample_count; i++) {
        entry_size = bswap32(*(uint32_t *)data);
        data += 4;
        used_bytes += 4;
        //printf("Entry-size[%d] = 0x%08x\n", i, entry_size);
    }
    return 0;
}

static int atom_parse_stco(atom *atom)
{
    char *data = (char *)atom->data + atom->header_size;
    uint32_t versionflags;
    uint32_t entry_count;
    uint32_t chunk_offset;

    versionflags = bswap32(*(uint32_t *)data);
    data += 4;
    if (_logger) {
        fprintf(_logger, "VersionFlags = 0x%08x\n", versionflags);
    }
    entry_count = bswap32(*(uint32_t *)data);
    data += 4;
    if (_logger) {
        fprintf(_logger, "Entry-count = %d\n", entry_count);
    }
    for (int i = 0; i < entry_count; i++) {
        chunk_offset = bswap32(*(uint32_t *)data);
        data += 4;
        //printf("Chunk-offset[%d] = 0x%08x\n", i, chunk_offset);
    }
    return 0;
}

int atom_dump(FILE *fp, long apos, uint32_t tmp)
{
    atom atom;
    uint32_t cur_loc;
    int print_header = 0;
    int ret;

    if (!_logger) {
        _logger = stderr;
    }
    cur_loc = ftell(fp);
    fseek(fp, apos, SEEK_SET);
    atom.apos = apos;
    atom.data = (unsigned char *)_dmalloc(8,__FILE__,__LINE__);
    atom.data_end = atom.data + 8;
    ret = fread(atom.data, 8, 1, fp);
    atom_parse_header(&atom);
    print_header = 1;
    if (atom.header_size < atom.size) {
        int len = (atom.size > 32) ? 32 : atom.size;
        atom.data = (unsigned char *)_drealloc((char *)atom.data, len, __FILE__,
                                               __LINE__);
        atom.data_end = atom.data + len;
        ret = fread(atom.data + 8, len - 8, 1, fp);
    }

    switch (atom.type) {
    case ATOM_TYPE_MVHD:
    case ATOM_TYPE_TKHD:
    case ATOM_TYPE_OPUS:
    case ATOM_TYPE_DOPS:
    case ATOM_TYPE_AC_3:
    case ATOM_TYPE_DAC3:
    case ATOM_TYPE_EC_3:
    case ATOM_TYPE_DEC3:
    case ATOM_TYPE_STTS:
    case ATOM_TYPE_STSC:
    case ATOM_TYPE_STSZ:
    case ATOM_TYPE_STCO:
        if (atom.type == ATOM_TYPE_STSC) {
            int x = 0;
            x++;
        }
        if (atom.data_end - atom.data < atom.size) {
            int oldlen = (atom.data_end - atom.data);
            int len = atom.size - oldlen;
            atom.data = (unsigned char *)_drealloc((char *)atom.data, atom.size, __FILE__,
                                                   __LINE__);
            atom.data_end = atom.data + oldlen;
            ret = fread(atom.data_end, len, 1, fp);
            atom.data_end = atom.data_end + len;
        }
    }
    switch (atom.type) {
    case ATOM_TYPE_FTYP:
        return atom_parse_ftyp(&atom);
    case ATOM_TYPE_MVHD:
        return atom_parse_mvhd(&atom);
    case ATOM_TYPE_META:
        return atom_parse_meta(&atom);
    case ATOM_TYPE_STYP:
        return atom_parse_styp(&atom);
    case ATOM_TYPE_SIDX:
        return atom_parse_sidx(&atom);
    case ATOM_TYPE_MDAT:
        return atom_parse_mdat(&atom);
    case ATOM_TYPE_MOOV:
    case ATOM_TYPE_TRAK:
    case ATOM_TYPE_TKHD:
    case ATOM_TYPE_TRAF:
    case ATOM_TYPE_MDIA:
    case ATOM_TYPE_UDTA:
    case ATOM_TYPE_MINF:
    case ATOM_TYPE_STBL:
    case ATOM_TYPE_MOOF:
    case ATOM_TYPE_ILST:
    case ATOM_TYPE_CALC:
        if (print_header == 0)
            if (_logger) {
                fprintf(_logger, "[%.*s %lu]\n", 4, (char *)&atom.type,
                        atom.data_end - atom.data);
            }
        if (_logger) {
            fprintf(_logger, "- labels\n");
        }
        hex_dump(NULL, atom.data, atom.data_end - atom.data);
        break;
    case ATOM_TYPE_OPUS:
        atom_parse_opus(&atom);
        break;
    case ATOM_TYPE_DOPS:
        atom_parse_dops(&atom);
        break;
    case ATOM_TYPE_STTS:
        atom_parse_stts(&atom);
        break;
    case ATOM_TYPE_STSC:
        atom_parse_stsc(&atom);
        break;
    case ATOM_TYPE_STSZ:
        atom_parse_stsz(&atom);
        break;
    case ATOM_TYPE_STCO:
        atom_parse_stco(&atom);
        break;
    case ATOM_TYPE_TRUN:
    case ATOM_TYPE_FREE:
    default:
        break;
    }
    atom_free(&atom);
    fseek(fp, cur_loc, SEEK_SET);
    return 0;
}

static void hex_dump(char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        if (_logger) {
            fprintf(_logger, "%s:\n", desc);
        }

    if (len == 0) {
        fprintf(stderr, "  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        fprintf(stderr, "  NEGATIVE LENGTH: %i\n", len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                if (_logger) {
                    fprintf(_logger, "  %s\n", buff);
                }

            // Output the offset.
            if (_logger) {
                fprintf(_logger, "  %04x ", i);
            }
        }

        // Now the hex code for the specific character.
        if (_logger) {
            fprintf(_logger, " %02x", pc[i]);
        }

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        if (_logger) {
            fprintf(_logger, "   ");
        }
        i++;
    }

    // And print the final ASCII bit.
    if (_logger) {
        fprintf(_logger, "  %s\n", buff);
    }
}
