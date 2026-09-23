/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef ONVIF_TIMER_H
#define ONVIF_TIMER_H


#define TIMER_MODE_SINGLE   0
#define TIMER_MODE_REPEAT   1

typedef void (*timer_func)(void *);

typedef struct
{
    time_t      start;
    uint32      interval;
    int         mode;
    timer_func  func;
    void *      param;
} TIMER_UA;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @desc : Add timer processing tasks, Up to ONVIF_MAX_TIMER_NUMS timer tasks are supported
 * 
 * @params :
 *  interval : timer interval, unit is second
 *  func     : Timer processing function
 *  param    : User parameter, returned in timer processing function
 *  mode     : timer mode
 *      TIMER_MODE_SINGLE : Timer task is triggered only once
 *      TIMER_MODE_REPEAT : Timer task repeated trigger
 *
 * @return :
 *  The timer index is returned successfully, otherwise - 1 is returned
 **/
int     timer_add(uint32 interval, timer_func func, void * param, int mode);

/**
 * @desc : Delete timer processing task
 *
 * @params : 
 *  idx     : The timer index
 **/
void    timer_del(int idx);

void    onvif_timer_init();
void    onvif_timer_deinit();
void    onvif_timer();

#ifdef __cplusplus
}
#endif

#endif


