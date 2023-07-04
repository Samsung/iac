/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file obu_multiway_tree.h
 * @brief Generation of different OBU ID
 * @version 0.1
 * @date Created 3/3/2023
 **/

#ifndef OBU_MULTIWAY_TREE_H
#define OBU_MULTIWAY_TREE_H

#include "obuwrite.h"

#ifndef OBU_IA_ROOT
#define OBU_IA_ROOT OBU_IA_Invalid
#endif

#ifndef OBU_IA_ROOT_ID
#define OBU_IA_ROOT_ID -1
#endif

#ifndef CODEC_CONFIG_START_ID
#define CODEC_CONFIG_START_ID 0
#endif

#ifndef AUDIO_ELEMENT_START_ID
#define AUDIO_ELEMENT_START_ID 10
#endif

/*
"audio_substream_id" shall indicate a unique ID in an IA bitstream for a given
substream. All Audio Frame OBUs of the same substream shall have the same
audio_substream_id. This value must be greater or equal to 22, in order to avoid
collision with the reserved IDs for the OBU types OBU_IA_Audio_Frame_ID0 to
OBU_IA_Audio_Frame_ID21.
*/
#ifndef SUB_STREAM_START_ID
#define SUB_STREAM_START_ID 0
#define SUB_STREAM_ID_SHIFT 6
#endif

#ifndef MIX_RESENTATION_START_ID
#define MIX_RESENTATION_START_ID 30
#endif

#ifndef PARAMETER_BLOCK_START_ID
#define PARAMETER_BLOCK_START_ID 70
#endif

typedef struct ObuNode {
  const char *obu_name;
  int obu_type;
  int obu_id;
  struct ObuNode *pParent;
  struct ObuNode *pBrother;
  struct ObuNode *pChild;
} ObuNode;

typedef struct ObuIDManager {
  int obu_start_id[OBU_IA_MAX_Count];
  ObuNode *obu_note;
  int mode;
} ObuIDManager;

ObuIDManager *obu_id_manager_create(int mode);
int insert_obu_node(ObuIDManager *obu_id_manager, AUDIO_OBU_TYPE obu_type,
                    AUDIO_OBU_TYPE parent_obu_type, int parent_obu_id);
ObuNode *get_obu_node_pos(ObuNode *root_node, AUDIO_OBU_TYPE obu_type,
                          int obu_id);
void delete_obu_node(ObuIDManager *obu_id_manager, AUDIO_OBU_TYPE obu_type,
                     int obu_id);
int get_obu_ids(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int *obu_ids);
void free_obu_node(ObuNode *root_node);
void obu_id_manager_destroy(ObuIDManager *obu_id_manager);

#endif
