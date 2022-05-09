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

typedef struct Demixer Demixer;
typedef struct DemixingParam {
    int         demixing_mode;
    int         steps;
    float*      gain;
    uint8_t*    recon_gain;
    int         recon_gain_flag;
    int*        layout;
    uint8_t*    channel_order;
    int         frame_size;
} DemixingParam;

Demixer* demixer_create(void);
int demixer_init(Demixer* );
int demixer_demix(Demixer* , void* , int , void* , DemixingParam* );
void demixer_destroy(Demixer* );

#endif /* __DEMIXER_H_ */
