#ifndef OBU_MULTIWAY_TREE_H
#define OBU_MULTIWAY_TREE_H

#include "obuwrite.h"

#ifndef CODEC_CONFIG_START_ID
#define CODEC_CONFIG_START_ID 0
#endif

#ifndef AUDIO_ELEMENT_START_ID
#define AUDIO_ELEMENT_START_ID 10
#endif

/*
"audio_substream_id" shall indicate a unique ID in an IA bitstream for a given substream.
All Audio Frame OBUs of the same substream shall have the same audio_substream_id.
This value must be greater or equal to 22,
in order to avoid collision with the reserved IDs for the OBU types OBU_IA_Audio_Frame_ID0 to OBU_IA_Audio_Frame_ID21.
*/
#ifndef SUB_STREAM_START_ID
#define SUB_STREAM_START_ID 0
#define SUB_STREAM_ID_SHIFT 9
#endif

#ifndef PARAMETER_BLOCK_START_ID
#define PARAMETER_BLOCK_START_ID 70
#endif



typedef struct ObuNode {
  const char* obu_name;
  int obu_type;
  int obu_id;
  struct ObuNode *pParent;
  struct ObuNode *pBrother;
  struct ObuNode *pChild;
}ObuNode;

ObuNode * insert_obu_root_node();
int insert_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int parent_obu_id);
ObuNode *get_obu_node_pos(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int obu_id);
void delete_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int obu_id);
int get_obu_ids(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int *obu_ids);
void free_obu_node(ObuNode *root_node);



#endif//OBU_MULTIWAY_TREE_H

