#ifndef _AUDIO_DEFINES_H_
#define _AUDIO_DEFINES_H_


#define LIMITER_MaximumTruePeak     -1.0f
#define LIMITER_AttackSec           0.001f
#define LIMITER_ReleaseSec          0.200f
#define LIMITER_LookAhead           240

typedef enum {
  CHANNELUNKNOWN = 0,
  CHANNELMONO,
  CHANNELSTEREO,
  CHANNEL51,
  CHANNEL512,
  CHANNEL514,
  CHANNEL71,
  CHANNEL712,
  CHANNEL714,
  CHANNEL312,
  CHANNELBINAURAL
}channelLayout;

#define MAX_CHANNELS 12
#define MAX_OUTPUT_CHANNELS 24
#define MAX_DELAYSIZE 4096
#define CHUNK_SIZE 960
#define FRAME_SIZE 960

#define IA_FRAME_MAXSIZE 2048
#define MAX_PACKET_SIZE  (MAX_CHANNELS*sizeof(int16_t)*IA_FRAME_MAXSIZE) // IA_FRAME_MAXSIZE*2/channel


#endif
