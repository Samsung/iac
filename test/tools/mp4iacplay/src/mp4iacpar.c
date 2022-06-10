#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "mp4iacpar.h"
#include "atom.h"

MP4IACParser *mp4_iac_parser_create()
{
    MP4IACParser *ths;
    ths = (MP4IACParser *)malloc(sizeof(MP4IACParser));
    return ths;
}

void mp4_iac_parser_init(MP4IACParser *ths)
{
    ths->m_mp4r = NULL;
    ths->m_logger = stderr;
}

void mp4_iac_parser_destroy(MP4IACParser *ths)
{
    mp4_iac_parser_close(ths);
    ths->m_mp4r = NULL;
    ths->m_logger = stderr;
}

int mp4_iac_parser_open_trak(MP4IACParser *ths, char *mp4file,
        IACHeader** header)
{
    IACHeader *iac_header;
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

    iac_header = (IACHeader *)atr->csc;
    if (iac_header == NULL || atr == NULL) {
        fprintf(stderr, "MP4 file does not have OPUS audio track.\n");
        goto done;
    }
    *header = iac_header;

    //////////////////////////////////////

done:
    if (!ths->m_mp4r) {
        return (-1);
    }

    return 1;
}

int mp4_iac_parser_read_packet(MP4IACParser *ths, int trakn, void *pkt_buf,
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

void mp4_iac_parser_close(MP4IACParser *ths)
{
    if (ths->m_mp4r) {
        mp4demux_close(ths->m_mp4r);    // close all track ids, too.
    }
    ths->m_mp4r = NULL;
}

int mp4_iac_parser_set_logger(MP4IACParser *ths, FILE *logger)
{
    ths->m_logger = logger;
    return (0);
}

