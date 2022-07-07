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

#ifndef __DEMIXER_H_
#define __DEMIXER_H_

#include <stdint.h>
#include "immersive_audio_defines.h"
#include "immersive_audio_types.h"

typedef struct Demixer Demixer;

Demixer *demixer_open(uint32_t frame_size, uint32_t delay);
void demixer_close (Demixer *);

int demixer_set_channel_layout (Demixer *, IAChannelLayoutType );
int demixer_set_channels_order (Demixer *, IAChannel *, int );
int demixer_set_output_gain (Demixer *, IAChannel *, float *, int);
int demixer_set_demixing_mode (Demixer *, int);
int demixer_set_recon_gain (Demixer *, int , IAChannel *, float *, uint32_t);
int demixer_demixing (Demixer *, float *, float *, uint32_t);

#endif /* __DEMIXER_H_ */
