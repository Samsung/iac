#ifndef __MP4_IAC_PAR_H_
#define __MP4_IAC_PAR_H_

#include "mp4demux.h"
#include "iac_header.h"

typedef struct
{
    mp4r_t *m_mp4r;
    FILE *m_logger;
} MP4IACParser;

MP4IACParser *mp4_iac_parser_create();
void mp4_iac_parser_init(MP4IACParser *);
void mp4_iac_parser_destroy(MP4IACParser *);
int mp4_iac_parser_open_trak(MP4IACParser *, char *mp4file,
        IACHeader** header);
int mp4_iac_parser_read_packet(MP4IACParser *, int trakn, void *pkt_buf,
        int inbytes, int *pktlen_out, int64_t *sample_offs, int *ent_no);
void mp4_iac_parser_close(MP4IACParser *);
int mp4_iac_parser_set_logger(MP4IACParser *, FILE *logger);


#endif /* __MP4_IAC_PAR_H_ */
