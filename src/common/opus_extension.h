/******************************************************************************
*                       Samsung Electronics Co., Ltd.                        *
*                                                                            *
*                           Copyright (C) 2021                               *
*                          All rights reserved.                              *
*                                                                            *
* This software is the confidential and proprietary information of Samsung   *
* Electronics Co., Ltd. ("Confidential Information"). You shall not disclose *
* such Confidential Information and shall use it only in accordance with the *
* terms of the license agreement you entered into with Samsung Electronics   *
* Co., Ltd.                                                                  *
*                                                                            *
* Removing or modifying of the above copyright notice or the following       *
* descriptions will terminate the right of using this software.              *
*                                                                            *
* As a matter of courtesy, the authors request to be informed about uses of  *
* this software and about bugs in this software.                             *
******************************************************************************/


#ifndef OPUS_EXTENSION_H
#define OPUS_EXTENSION_H

#define gL      0
#define gML3    0
#define gML5    0
#define gA      0
#define gR      1
#define gMR3    1
#define gMR5    1
#define gB      1
#define gC      2
#define gMC     2
#define gT      2
#define gLFE    3
#define gMLFE   3
#define gP      3
#define gSL     4
#define gMHL3   4
#define gMSL5   4
#define gQ1     4
#define gSR     5
#define gMHR3   5
#define gMSR5   5
#define gQ2     5
#define gBL     6
#define gMHL5   6
#define gMHFL5  6
#define gS1     6
#define gBR     7
#define gMHR5   7
#define gMHFR5  7
#define gS2     7
#define gHL     8
#define gU1     8
#define gMHBL5  8
#define gHR     9
#define gU2     9
#define gMHBR5  9
#define gHBL    10
#define gV1     10
#define gHBR    11
#define gV2     11

#define PRESKIP_SIZE 312
#define CHUNK_SIZE 960
#define FRAME_SIZE 960
#define MAX_CHANNELS 12
#define MAX_PACKET_SIZE  (MAX_CHANNELS*sizeof(int16_t)*FRAME_SIZE) // 960*2/channel
#define WINDOW_SIZE (FRAME_SIZE/8)

#define MAX_METADATA_SIZE 128

#endif /* OPUS_EXTENSION_H */
