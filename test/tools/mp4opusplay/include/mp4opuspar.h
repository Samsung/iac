#ifndef __MP4_OPUS_PAR_H_
#define __MP4_OPUS_PAR_H_

#include "mp4demux.h"
#include "opus_header.h"

typedef struct
{
    mp4r_t *m_mp4r;
    FILE *m_logger;

} MP4OpusParser;

MP4OpusParser *mp4_opus_parser_create();
void mp4_opus_parser_init(MP4OpusParser *);
void mp4_opus_parser_destroy(MP4OpusParser *);
int mp4_opus_parser_open_trak(MP4OpusParser *, char *mp4file,
        OpusHeader header[]);
int mp4_opus_parser_read_packet(MP4OpusParser *, int trakn, void *pkt_buf,
        int inbytes, int *pktlen_out, int64_t *sample_offs, int *ent_no);
void mp4_opus_parser_close(MP4OpusParser *);
int mp4_opus_parser_set_logger(MP4OpusParser *, FILE *logger);


#endif /* __MP4_OPUS_PAR_H_ */
