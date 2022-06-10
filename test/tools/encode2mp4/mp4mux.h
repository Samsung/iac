#ifndef MP4MUX_H
#define MP4MUX_H

#include <stdint.h>
#include "bitstreamrw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t atom_type;
  void *opaque;
} avio_context;

#ifndef DEMIXING_MATRIX_SIZE_MAX
#define DEMIXING_MATRIX_SIZE_MAX (18 * 18 * 2)
#endif

#ifndef MAX_CHANNEL_LAYOUTS
#define MAX_CHANNEL_LAYOUTS 9
#endif

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 12
#endif

typedef enum {
  CODEC_OPUS,
  CODEC_AAC,
  CODEC_MAX
} CODEC_TYPE;

// immersive audio option for flags
#define IA_MOV_FLAG_RTP_HINT              (1 <<  0)
#define IA_MOV_FLAG_FRAGMENT              (1 <<  1)
#define IA_MOV_FLAG_EMPTY_MOOV            (1 <<  2)
#define IA_MOV_FLAG_FRAG_KEYFRAME         (1 <<  3)
#define IA_MOV_FLAG_SEPARATE_MOOF         (1 <<  4)
#define IA_MOV_FLAG_FRAG_CUSTOM           (1 <<  5)
#define IA_MOV_FLAG_ISML                  (1 <<  6)
#define IA_MOV_FLAG_FASTSTART             (1 <<  7)
#define IA_MOV_FLAG_OMIT_TFHD_OFFSET      (1 <<  8)
#define IA_MOV_FLAG_DISABLE_CHPL          (1 <<  9)
#define IA_MOV_FLAG_DEFAULT_BASE_MOOF     (1 << 10)
#define IA_MOV_FLAG_DASH                  (1 << 11)
#define IA_MOV_FLAG_FRAG_DISCONT          (1 << 12)
#define IA_MOV_FLAG_DELAY_MOOV            (1 << 13)
#define IA_MOV_FLAG_GLOBAL_SIDX           (1 << 14)
#define IA_MOV_FLAG_WRITE_COLR            (1 << 15)
#define IA_MOV_FLAG_WRITE_GAMA            (1 << 16)
#define IA_MOV_FLAG_USE_MDTA              (1 << 17)
#define IA_MOV_FLAG_SKIP_TRAILER          (1 << 18)
#define IA_MOV_FLAG_NEGATIVE_CTS_OFFSETS  (1 << 19)
#define IA_MOV_FLAG_FRAG_EVERY_FRAME      (1 << 20)
#define IA_MOV_FLAG_SKIP_SIDX             (1 << 21)
#define IA_MOV_FLAG_CMAF                  (1 << 22)
#define IA_MOV_FLAG_PREFER_ICC            (1 << 23)


typedef struct
{

	uint32_t samplerate;
	uint32_t samples; // total sound samples
	uint32_t channels;
	uint32_t bits; // sample depth
	// buffer config
	uint16_t buffersize;
	struct {
		uint32_t max;
		uint32_t avg;
		int size;
		int samples;
	} bitrate;
	uint32_t framesamples; // samples per frame
	uint32_t sample_size; // variable bitrate mode = 0, constant bitrate mode = size > 0
	// AudioSpecificConfig data:
	struct
	{
		uint8_t *data;
		unsigned long size;
	} asc;

	struct
	{
		uint32_t *offsets; // (stco)
		uint32_t *sizes;   // (stsz)
		uint32_t *deltas;  // (stts)
		uint32_t entries;
		uint32_t buffersize;

    // add for fragment mp4
    uint32_t start_dts;
    uint32_t moofoffset;
    uint32_t trun_dataoffset;
    uint32_t fragments;// the ordinal number of this fragment, in increasing order
    uint32_t fmdat_size;
    uint32_t fmdat_offset;
    unsigned char * fmdat;
	} frame;

  struct
  {
    uint32_t sub_bitstream_count;
    void *csc; // codec specific
    uint32_t entry_count;
    uint32_t codec_id; // OPUS=0, AAC=1, AC3=2, etc

    // entry_count >0
    uint32_t sub_bitstream_number[MAX_CHANNELS];
  }aiac;

 /*
 class IA_Static_Metadata {
   unsigned int (8) Version;
   unsigned int (2) Ambisonics_Mode;
   unsigned int (3) Channel_Audio_Layer;
   unsigned int (2) reserved;
   unsigned int (1) Substream_Size_Is_Present_Flag;
   if (Ambisoics_Mode == 1 or 2)
   AmbisonicsLayerConfiguration (Ambisonics_Mode);
   for (i=0; i < Channel_Audio_Layer; i++) {
   ChannelAudioLayerConfiguration (i);
   }
 }

 class AmbisonicsLayerConfiguration (Ambisonics_Mode) {
   unsigned int (8) Output_Channel_Count (C);
   unsigned int (8) Substream_Count (N);
   unsigned int (8) Coupled_Substream_Count (M);
   unsigned int (8*C) Channel_Mapping if  Ambisonics_Mode == 1 Or
   unsigned int (16*(N+M)*C bits) Demixing_Matrix if if  Ambisonics_Mode == 2
 }

 class ChannelAudioLayerConfiguration () {
   unsigned int (4) Loudspeaker_Layout;
   unsigned int (1) Output_Gain_Is_Present_Flag;
   unsigned int (1) Recon_Gain_Is_Present_Flag;
   unsigned int (2) reserved;
   unsinged int (8) Substream_Count;
   unsigned int (8) Coupled_Substream_Count;
   signed int (16) Loudness;
   if (Output_Gain_Is_Present_Flag == 1)
   signed int (16) Output_Gain
 }
 */ 
  struct
  {
    uint32_t version;
    uint32_t ambisonics_mode;
    uint32_t channel_audio_layer;
    uint32_t substream_size_is_present_flag;
    struct
    {
      uint32_t output_channel_count;
      uint32_t substream_count;
      uint32_t coupled_substream_count;
      uint32_t channel_mapping[MAX_CHANNELS];
      uint32_t demixing_matrix[DEMIXING_MATRIX_SIZE_MAX];
    }ambix_layer_config; //Ambisonics Layer Config
    struct
    {
      uint32_t loudspeaker_layout;
      uint32_t output_gain_is_present_flag;
      uint32_t recon_gain_is_present_flag;
      uint32_t output_channel_count;
      uint32_t substream_count;
      uint32_t coupled_substream_count;
      int16_t loudness;
      uint16_t output_gain_flags;
      int16_t output_gain;
    }ch_audio_layer_config[MAX_CHANNEL_LAYOUTS]; //Channel Audio Layer Config
  }ia_static_meta;
  struct
  {
    uint8_t dmixp_mode[8];
    uint32_t dmixp_mode_count;
    uint32_t dmixp_mode_ponter;// last mode

    uint32_t *dmixp_mode_group[2];
    uint32_t dmixp_mode_group_size;
  }demixing_info;

} mov_audio_track;


typedef struct
{
	FILE *mp4file;
	avio_context *atom;


  mov_audio_track *audio_trak; // audio track
  uint32_t mdatoffset;
  uint32_t mdatsize;
  uint32_t moovoffset;

	int num_video_traks;
	int num_audio_traks;
	int audio_trak_select;
  int video_trak_select;

  int codec_id;// 0:opus, 1:aac

  // below is for fragment mp4 writing.
  int flags; // mp4/fmp4
  uint32_t max_fragment_duration;

} MOVMuxContext;

enum 
{ 
  ERROR_NONE = 0, 
  ERROR_FAIL = -1, 
  ERROR_NOTSUPPORT = -2 
};


MOVMuxContext *mov_write_open(char *filename);
int mov_write_head(MOVMuxContext *movm);
int mov_write_trak(MOVMuxContext *movm, int num_video_traks, int num_audio_traks);
int mov_write_audio(MOVMuxContext *movm, int trak, uint8_t * buffer, int size, int samples);
int mov_write_audio2(MOVMuxContext *movm, int trak, uint8_t * buffer, int size, int samples, int demix_mode);
int mov_write_tail(MOVMuxContext *movm);
int mov_write_close(MOVMuxContext *movm);

#ifdef __cplusplus
}
#endif

#endif
