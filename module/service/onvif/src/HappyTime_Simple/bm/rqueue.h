/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef	__H_RQUEUE_H__
#define	__H_RQUEUE_H__

/***********************************************************/
#define RQ_NUM	16
#define RQ_LEN	256

typedef struct r_queue
{
	unsigned int	front;
	unsigned int	rear;
	char 			queue_buffer[RQ_NUM][RQ_LEN];
	unsigned int	count_put_full;
}RQUEUE;


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************/
BOOL 	rqBufPut(RQUEUE * phq,char * buf,int wlen);
BOOL 	rqBufGet(RQUEUE * phq,char * buf,int * rlen);

char  * rqGetHead(RQUEUE * phq,int * rlen);

#ifdef __cplusplus
}
#endif

#endif


