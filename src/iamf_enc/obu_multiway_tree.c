#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "obu_multiway_tree.h"

//https://blog.csdn.net/misterdo/article/details/112377757

ObuNode * insert_obu_root_node()
{
  ObuNode * root_node = (ObuNode*)malloc(sizeof(ObuNode));
  if (!root_node)
    return NULL;
  memset(root_node, 0x00, sizeof(ObuNode));

  root_node->obu_name = "root";
  root_node->obu_id = -1; // root
  root_node->obu_type = -1; // root
  return root_node;
}

ObuNode *get_obu_node_pos(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int obu_id)
{
  if (root_node == NULL)
    return NULL;
  if (obu_id == -1)
    return root_node;
  ObuNode *pTemp = root_node;
  ObuNode * pTempBrother = root_node;

  while (pTemp)
  {
    if (pTemp->obu_id == obu_id && pTemp->obu_type == obu_type)
    {
      return pTemp;
    }
    ObuNode * find = get_obu_node_pos(pTemp->pChild, obu_type, obu_id);
    if (find)
      return find;
    pTemp = pTemp->pBrother;
  }
  return NULL;
}


int get_new_obu_id(ObuNode *pParent, int obu_start_id)
{
  ObuNode* pChild = NULL, *pTemp = NULL;
  ObuNode* pParentParent = pParent->pParent;
  if (pParentParent == NULL)
    return obu_start_id;
  pChild = pParentParent->pChild;

  int find_i = obu_start_id;

  for (find_i = obu_start_id;; find_i++)
  {
    while (pChild)
    {
      pTemp = pChild->pChild;
      while (pTemp)
      {
        if (find_i == pTemp->obu_id)
          break;
        pTemp = pTemp->pBrother;
      }
      if (pTemp)
        break;
      pChild = pChild->pBrother;
    }
    if (pTemp)
    {
      pChild = pParentParent->pChild;
    }
    if (!pChild)
      break;
  }
  return find_i;
}

int get_new_obu_id2(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int obu_start_id)
{
  int find_i = obu_start_id;

  for (find_i = obu_start_id;; find_i++)
  {
    ObuNode * ret_node = get_obu_node_pos(root_node, obu_type, find_i);
    if (!ret_node)
      break;
  }
  return find_i;
}

int insert_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int parent_obu_id)
{
  ObuNode * pNew = (ObuNode*)malloc(sizeof(ObuNode));
  if (!pNew)
    return -1;
  memset(pNew, 0x00, sizeof(ObuNode));

  ObuNode* pTemp = NULL;
  ObuNode* pParent = NULL;
  ObuNode* pChild = NULL;
  AUDIO_OBU_TYPE parent_obu_type = OBU_IA_Invalid;

  int obu_start_id = 0;
  char* obu_name = NULL;
  if (obu_type == OBU_IA_Codec_Config)
  {
    obu_start_id = CODEC_CONFIG_START_ID;
    obu_name = "codec config";
    parent_obu_type = OBU_IA_Invalid;
  }
  else if (obu_type == OBU_IA_Audio_Element)
  {
    obu_start_id = AUDIO_ELEMENT_START_ID;
    obu_name = "audio element";
    parent_obu_type = OBU_IA_Codec_Config;
  }
  else if (obu_type == OBU_IA_Audio_Frame)
  {
    obu_start_id = SUB_STREAM_START_ID;
    obu_name = "audio frame";
    parent_obu_type = OBU_IA_Audio_Element;
  }
  else if (obu_type == OBU_IA_Parameter_Block)
  {
    obu_start_id = PARAMETER_BLOCK_START_ID;
    obu_name = "parameter block";
    parent_obu_type = OBU_IA_Audio_Element;
  }
  pParent = get_obu_node_pos(root_node, parent_obu_type, parent_obu_id);
  if (pParent == NULL)
  {
    printf("wrong parent_obu_id inputing!\n");
    return -1;
  }
  pTemp = pParent->pChild;
  pChild = pParent->pChild;
  if (pTemp == NULL)
  {
    pNew->obu_id = get_new_obu_id2(root_node, obu_type, obu_start_id);
    //pNew->obu_id = obu_start_id;
    pNew->obu_name = obu_name;
    pNew->obu_type = obu_type;
    pNew->pParent = pParent;
    pParent->pChild = pNew;
    return pNew->obu_id;
  }

  if (!strncmp(pParent->obu_name, "root", 4))
  {
    pTemp = pParent->pChild;
    int find_i = obu_start_id;
    for (find_i = obu_start_id;; find_i++)
    {
      while (pTemp)
      {
        if (find_i == pTemp->obu_id)
          break;
        pTemp = pTemp->pBrother;
      }
      if (!pTemp)
        break;
    }
    pNew->obu_id = find_i;
  }
  else
    pNew->obu_id = get_new_obu_id2(root_node, obu_type, obu_start_id);

  pTemp = pParent->pChild;
  while (pTemp->pBrother)
  {
    pTemp = pTemp->pBrother;
  }
  pNew->obu_name = obu_name;
  pNew->obu_type = obu_type;
  pTemp->pBrother = pNew;
  pNew->pParent = pTemp->pParent;
  return pNew->obu_id;
}


void traver_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type)
{
  ObuNode * pTemp = root_node;
  ObuNode * pTempBrother = NULL;

  if (obu_type == OBU_IA_Codec_Config)
  {
    pTemp = root_node->pChild;
  }
  else if (obu_type == OBU_IA_Audio_Element)
  {
    pTemp = root_node->pChild->pChild;
  }
  else if (obu_type == OBU_IA_Audio_Frame)
  {
    pTemp = root_node->pChild->pChild->pChild;
  }

  while (pTemp)
  {
    pTempBrother = pTemp;
    printf("pTempBrother: name[%s] id[%d] obu_type[%d]", pTempBrother->obu_name, pTempBrother->obu_id, pTempBrother->obu_type);
    pTemp = pTempBrother->pBrother;
  }
}


void free_obu_node(ObuNode *root_node)
{
  ObuNode *pTemp = root_node;
  ObuNode *pTempChild = NULL;
  ObuNode *PFree = NULL;
  pTempChild = pTemp->pChild;
  while (pTempChild)
  {
    printf("pTempChild: %d \n", pTempChild->obu_id);
    free_obu_node(pTempChild);
    PFree = pTempChild;
    pTempChild = pTempChild->pBrother;
    free(PFree);
    pTemp->pChild = NULL;
  }
}


void delete_obu_node(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int obu_id)
{
  ObuNode * pTemp = NULL, *pParent = NULL, *pChild = NULL;
  pTemp = get_obu_node_pos(root_node, obu_type, obu_id);
  free_obu_node(pTemp);

  if (pTemp->pParent)
  {
    pParent = pTemp->pParent;
    if (pParent->pChild == pTemp) // first child
    {
      pParent->pChild = pParent->pChild->pBrother;
    }
    else
    {
      pChild = pParent->pChild;
      while (pChild)
      {
        if (pChild->pBrother == pTemp)
        {
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


void find_obu_node_with_type(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int *obu_ids, int *size)
{
  ObuNode *pTemp = root_node;
  ObuNode *pTempChild = NULL;
  while (pTemp)
  {
    if (pTemp && pTemp->obu_type == obu_type)
    {
      obu_ids[*size] = pTemp->obu_id;
      *size = *size + 1;
    }
    pTempChild = pTemp->pChild;
    find_obu_node_with_type(pTempChild, obu_type, obu_ids, size);
    pTemp = pTemp->pBrother;
  }
}

int get_obu_ids(ObuNode *root_node, AUDIO_OBU_TYPE obu_type, int *obu_ids)
{
  int size = 0;
  find_obu_node_with_type(root_node, obu_type, obu_ids, &size);
  return size;
}