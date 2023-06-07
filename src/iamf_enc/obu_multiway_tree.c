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
 * @file obu_multiway_tree.c
 * @brief Generation of different OBU ID
 * @version 0.1
 * @date Created 3/3/2023
 **/

#include "obu_multiway_tree.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://blog.csdn.net/misterdo/article/details/112377757

ObuIDManager *obu_id_manager_create(int mode) {
  ObuIDManager *obu_id_manager = (ObuIDManager *)malloc(sizeof(ObuIDManager));
  ObuNode *root_node = (ObuNode *)malloc(sizeof(ObuNode));
  if (!root_node || !obu_id_manager) return NULL;
  memset(root_node, 0x00, sizeof(ObuNode));
  memset(obu_id_manager, 0x00, sizeof(ObuIDManager));

  root_node->obu_name = "root";
  root_node->obu_id = -1;    // root
  root_node->obu_type = -1;  // root

  obu_id_manager->obu_note = root_node;
  obu_id_manager->obu_start_id[OBU_IA_Codec_Config] = CODEC_CONFIG_START_ID;
  obu_id_manager->obu_start_id[OBU_IA_Audio_Element] = AUDIO_ELEMENT_START_ID;
  obu_id_manager->obu_start_id[OBU_IA_Mix_Presentation] =
      MIX_RESENTATION_START_ID;
  obu_id_manager->obu_start_id[OBU_IA_Audio_Frame] = SUB_STREAM_START_ID;
  obu_id_manager->obu_start_id[OBU_IA_Parameter_Block] =
      PARAMETER_BLOCK_START_ID;

  obu_id_manager->mode =
      mode;  // 0: deafult, 1: id is increasing, previous won't be used/
  return obu_id_manager;
}

ObuNode *get_obu_node_pos(ObuNode *root_node, AUDIO_OBU_TYPE obu_type,
                          int obu_id) {
  if (root_node == NULL) return NULL;
  if (obu_id == -1) return root_node;
  ObuNode *pTemp = root_node;
  ObuNode *pTempBrother = root_node;

  while (pTemp) {
    if (pTemp->obu_id == obu_id && pTemp->obu_type == obu_type) {
      return pTemp;
    }
    ObuNode *find = get_obu_node_pos(pTemp->pChild, obu_type, obu_id);
    if (find) return find;
    pTemp = pTemp->pBrother;
  }
  return NULL;
}

int get_new_obu_id2(ObuNode *root_node, AUDIO_OBU_TYPE obu_type,
                    int obu_start_id) {
  int find_i = obu_start_id;

  for (find_i = obu_start_id;; find_i++) {
    ObuNode *ret_node = get_obu_node_pos(root_node, obu_type, find_i);
    if (!ret_node) break;
  }
  return find_i;
}

int insert_obu_node(ObuIDManager *obu_id_manager, AUDIO_OBU_TYPE obu_type,
                    AUDIO_OBU_TYPE parent_obu_type, int parent_obu_id) {
  ObuNode *root_node = obu_id_manager->obu_note;
  ObuNode *pNew = (ObuNode *)malloc(sizeof(ObuNode));
  if (!pNew) return -1;
  memset(pNew, 0x00, sizeof(ObuNode));

  ObuNode *pTemp = NULL;
  ObuNode *pParent = NULL;
  ObuNode *pChild = NULL;

  int obu_start_id = obu_id_manager->obu_start_id[obu_type];
  char *obu_name = NULL;
  if (obu_type == OBU_IA_Codec_Config) {
    obu_name = "codec config";
    pParent = root_node;
  } else if (obu_type == OBU_IA_Audio_Element) {
    obu_name = "audio element";
    pParent = root_node;
  } else if (obu_type == OBU_IA_Mix_Presentation) {
    obu_name = "mix presentation";
    pParent = root_node;
  } else if (obu_type == OBU_IA_Audio_Frame) {
    obu_name = "audio frame";
    pParent = get_obu_node_pos(root_node, parent_obu_type, parent_obu_id);
  } else if (obu_type == OBU_IA_Parameter_Block) {
    obu_name = "parameter block";
    pParent = get_obu_node_pos(root_node, parent_obu_type, parent_obu_id);
  }
  if (!pParent) {
    printf("wrong parent_obu_id inputing!\n");
    return -1;
  }
  pTemp = pParent->pChild;
  pChild = pParent->pChild;
  pNew->obu_id = get_new_obu_id2(root_node, obu_type, obu_start_id);
  if (obu_id_manager->mode)
    obu_id_manager->obu_start_id[obu_type] = pNew->obu_id + 1;
  if (!pTemp) {
    // pNew->obu_id = obu_start_id;
    pNew->obu_name = obu_name;
    pNew->obu_type = obu_type;
    pNew->pParent = pParent;
    pParent->pChild = pNew;
    return pNew->obu_id;
  }
  pTemp = pParent->pChild;
  while (pTemp->pBrother) {
    pTemp = pTemp->pBrother;
  }
  pNew->obu_name = obu_name;
  pNew->obu_type = obu_type;
  pTemp->pBrother = pNew;
  pNew->pParent = pTemp->pParent;
  return pNew->obu_id;
}

void traver_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type) {
  ObuNode *pTemp = root_node;
  ObuNode *pTempBrother = NULL;

  if (obu_type == OBU_IA_Codec_Config) {
    pTemp = root_node->pChild;
  } else if (obu_type == OBU_IA_Audio_Element) {
    pTemp = root_node->pChild->pChild;
  } else if (obu_type == OBU_IA_Audio_Frame) {
    pTemp = root_node->pChild->pChild->pChild;
  }

  while (pTemp) {
    pTempBrother = pTemp;
    printf("pTempBrother: name[%s] id[%d] obu_type[%d]", pTempBrother->obu_name,
           pTempBrother->obu_id, pTempBrother->obu_type);
    pTemp = pTempBrother->pBrother;
  }
}

void free_obu_node(ObuNode *root_node) {
  ObuNode *pTemp = root_node;
  ObuNode *pTempChild = NULL;
  ObuNode *PFree = NULL;
  pTempChild = pTemp->pChild;
  while (pTempChild) {
    printf("pTempChild: %d \n", pTempChild->obu_id);
    free_obu_node(pTempChild);
    PFree = pTempChild;
    pTempChild = pTempChild->pBrother;
    free(PFree);
    pTemp->pChild = NULL;
  }
}

void delete_obu_node(ObuIDManager *obu_id_manager, AUDIO_OBU_TYPE obu_type,
                     int obu_id) {
  ObuNode *root_node = obu_id_manager->obu_note;
  ObuNode *pTemp = NULL, *pParent = NULL, *pChild = NULL;
  pTemp = get_obu_node_pos(root_node, obu_type, obu_id);
  if (!pTemp) return NULL;
  free_obu_node(pTemp);

  if (pTemp->pParent) {
    pParent = pTemp->pParent;
    if (pParent->pChild == pTemp)  // first child
    {
      pParent->pChild = pParent->pChild->pBrother;
    } else {
      pChild = pParent->pChild;
      while (pChild) {
        if (pChild->pBrother == pTemp) {
          pChild->pBrother = pChild->pBrother->pBrother;
          break;
        }
        pChild = pChild->pBrother;
      }
    }
  }

  free(pTemp);
  pTemp = NULL;
}

void find_obu_node_with_type(ObuNode *root_node, AUDIO_OBU_TYPE obu_type,
                             int *obu_ids, int *size) {
  ObuNode *pTemp = root_node;
  ObuNode *pTempChild = NULL;
  while (pTemp) {
    if (pTemp && pTemp->obu_type == obu_type) {
      obu_ids[*size] = pTemp->obu_id;
      *size = *size + 1;
    }
    pTempChild = pTemp->pChild;
    find_obu_node_with_type(pTempChild, obu_type, obu_ids, size);
    pTemp = pTemp->pBrother;
  }
}

int get_obu_ids(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int *obu_ids) {
  int size = 0;
  find_obu_node_with_type(root_node, obu_type, obu_ids, &size);
  return size;
}

void obu_id_manager_destroy(ObuIDManager *obu_id_manager) {
  if (obu_id_manager) {
    delete_obu_node(obu_id_manager, OBU_IA_ROOT, -1);
    free(obu_id_manager);
  }
}