#ifndef ATOM_H_INCLUDED
#define ATOM_H_INCLUDED

#include <stdio.h>

#pragma pack(push, 1)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define atom_type(c1, c2, c3, c4) (((c1) << 0) + ((c2) << 8) + ((c3) << 16) + ((c4) << 24))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define atom_type(c1, c2, c3, c4) (((c1) << 24) + ((c2) << 16) + ((c3) << 8) + ((c4) << 0))
#endif

#define atom_type_string(t) ((char[5]){((t) >> 0) & 0xff,0,0,0,0})

enum {
    ATOM_TYPE_FTYP = atom_type('f', 't', 'y', 'p'),
    ATOM_TYPE_STYP = atom_type('s', 't', 'y', 'p'),
    ATOM_TYPE_SIDX = atom_type('s', 'i', 'd', 'x'),
    ATOM_TYPE_FREE = atom_type('f', 'r', 'e', 'e'),
    ATOM_TYPE_MDAT = atom_type('m', 'd', 'a', 't'),
    ATOM_TYPE_MOOV = atom_type('m', 'o', 'o', 'v'),
    ATOM_TYPE_MOOF = atom_type('m', 'o', 'o', 'f'),
    ATOM_TYPE_TRAK = atom_type('t', 'r', 'a', 'k'),
    ATOM_TYPE_TKHD = atom_type('t', 'k', 'h', 'd'),
    ATOM_TYPE_TRAF = atom_type('t', 'r', 'a', 'f'),
    ATOM_TYPE_TRUN = atom_type('t', 'r', 'u', 'n'),
    ATOM_TYPE_MVHD = atom_type('m', 'v', 'h', 'd'),
    ATOM_TYPE_MDIA = atom_type('m', 'd', 'i', 'a'),
    ATOM_TYPE_UDTA = atom_type('u', 'd', 't', 'a'),
    ATOM_TYPE_META = atom_type('m', 'e', 't', 'a'),
    ATOM_TYPE_MINF = atom_type('m', 'i', 'n', 'f'),
    ATOM_TYPE_STBL = atom_type('s', 't', 'b', 'l'),
    ATOM_TYPE_ILST = atom_type('i', 'l', 's', 't'),
    ATOM_TYPE_MP4A = atom_type('m', 'p', '4', 'a'),
    ATOM_TYPE_ESDS = atom_type('e', 's', 'd', 's'),
    ATOM_TYPE_OPUS = atom_type('O', 'p', 'u', 's'),
    ATOM_TYPE_DOPS = atom_type('d', 'O', 'p', 's'),
    ATOM_TYPE_EC_3 = atom_type('e', 'c', '-', '3'),
    ATOM_TYPE_DEC3 = atom_type('d', 'e', 'c', '3'),
    ATOM_TYPE_AC_3 = atom_type('a', 'c', '-', '3'),
    ATOM_TYPE_DAC3 = atom_type('d', 'a', 'c', '3'),

    /* Immersive audio atom */
    ATOM_TYPE_IATM = atom_type('i', 'a', 't', 'm'),
    ATOM_TYPE_I3DA = atom_type('i', '3', 'd', 'a'),
    ATOM_TYPE_IASM = atom_type('i', 'a', 's', 'm'),
    ATOM_TYPE_CALC = atom_type('c', 'a', 'l', 'c'),

    ATOM_TYPE_STTS = atom_type('s', 't', 't', 's'),
    ATOM_TYPE_STSC = atom_type('s', 't', 's', 'c'),
    ATOM_TYPE_STSZ = atom_type('s', 't', 's', 'z'),
    ATOM_TYPE_STCO = atom_type('s', 't', 'c', 'o'),
    ATOM_TYPE_SGPD = atom_type('s', 'g', 'p', 'd'),
    ATOM_TYPE_SBGP = atom_type('s', 'b', 'g', 'p')
};

typedef struct atom_header_s {
    uint32_t size;
    uint32_t type;
    uint64_t size_extended;
} atom_header;

typedef struct atom_s {
    unsigned char *data;
    unsigned char *data_end;
    uint32_t type;
    uint32_t header_size;
    uint32_t size;
    uint32_t apos;
    int depth;
} atom;

typedef struct atom_mvhd_v1_s {
    uint8_t  version;
    uint8_t  flags[3];
    uint32_t created;
    uint32_t modified;
    uint32_t scale;
    uint32_t duration;
    uint32_t speed;
    uint16_t volume;
    uint16_t reserved[5];
    uint32_t matrix[9];
    uint64_t quicktime_preview;
    uint32_t quicktime_poster;
    uint64_t quicktime_selection;
    uint32_t quicktime_current_time;
    uint32_t next_track_id;
} atom_mvhd_v1;

typedef struct atom_meta_s {
    uint8_t  version;
    uint8_t  flags[3];
} atom_meta;

void atom_set_logger(FILE *logger);
int atom_dump(FILE *fp, long apos, uint32_t tmp);

#pragma pack(pop)

#endif /* ATOM_H_INCLUDED */


