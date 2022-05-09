#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "mp4opuspar.h"
#include "atom.h"

MP4OpusParser *mp4_opus_parser_create()
{
    MP4OpusParser *ths;
    ths = (MP4OpusParser *)malloc(sizeof(MP4OpusParser));
    return ths;
}

void mp4_opus_parser_init(MP4OpusParser *ths)
{
    ths->m_mp4r = NULL;
    ths->m_logger = stderr;
}

void mp4_opus_parser_destroy(MP4OpusParser *ths)
{
    mp4_opus_parser_close(ths);
    ths->m_mp4r = NULL;
    ths->m_logger = stderr;
}

int mp4_opus_parser_open_trak(MP4OpusParser *ths, char *mp4file,
        OpusHeader header[])
{
    OpusHeader *opus_header;
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

    opus_header = (OpusHeader *)atr[0].csc;
    if (opus_header == NULL || opus_header->magic_id != ATOM_TYPE_OPUS
            || atr == NULL) {
        fprintf(stderr, "MP4 file does not have OPUS audio track.\n");
        goto done;
    }
    header[0] = *opus_header;
    fprintf(stderr, "track 1 is %6s track\n",
            ths->m_mp4r->trak_type[0] == TRAK_TYPE_AUDIO ? "audio" : "meta");

    if (ths->m_mp4r->num_a_trak == 2) {
        opus_header = (OpusHeader *)atr[1].csc;
        if (opus_header == NULL || opus_header->magic_id != ATOM_TYPE_OPUS
                || atr == NULL) {
            fprintf(stderr, "MP4 file does not have OPUS metadata track.\n");
            goto done;
        }
        header[1] = *opus_header;
        fprintf(stderr, "track 2 is %6s track\n",
                ths->m_mp4r->trak_type[1] == TRAK_TYPE_AUDIO ? "audio" : "meta");
    }

    //////////////////////////////////////

done:
    if (!ths->m_mp4r) {
        return (-1);
    }

    return (ths->m_mp4r->num_a_trak);
}

int mp4_opus_parser_read_packet(MP4OpusParser *ths, int trakn, void *pkt_buf,
        int inbytes, int *pktlen_out, int64_t *sample_offs, int *cur_entno)
{
    //////////////////////////////////////
    audio_rtr_t *atr = ths->m_mp4r->a_trak;
    int sample_delta = 0;

    if (atr[trakn].frame.ents <= atr[trakn].frame.current) {
        return (-1);
    }
    if (sample_offs && atr[trakn].frame.offs) {
        int entno = atr[trakn].frame.current;
        *sample_offs = atr[trakn].frame.offs[entno];
    }

    if (cur_entno) {
        *cur_entno = atr[trakn].frame.current + 1;
    }

    if (mp4demux_audio(ths->m_mp4r, trakn, &sample_delta) != ERR_OK) {
        fprintf(stderr, "mp4demux_frame() error\n");
        return (-1);
    }

    if (inbytes < atr[trakn].bitbuf.size) {
        memcpy(pkt_buf, atr[trakn].bitbuf.data, inbytes);
        *pktlen_out = atr[trakn].bitbuf.size;
        return (sample_delta);
    } else {
        memcpy(pkt_buf, atr[trakn].bitbuf.data, atr[trakn].bitbuf.size);
        *pktlen_out = atr[trakn].bitbuf.size;
        return (sample_delta);
    }

    //////////////////////////////////////
}

void mp4_opus_parser_close(MP4OpusParser *ths)
{
    if (ths->m_mp4r) {
        mp4demux_close(ths->m_mp4r);    // close all track ids, too.
    }
    ths->m_mp4r = NULL;
}

int mp4_opus_parser_set_logger(MP4OpusParser *ths, FILE *logger)
{
    ths->m_logger = logger;
    return (0);
}

