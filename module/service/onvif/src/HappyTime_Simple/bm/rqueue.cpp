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

#include "sys_inc.h"
#include "rqueue.h"

/***************************************************************************************/
BOOL rqBufPut(RQUEUE * phq, char * buf, int wlen)
{
	unsigned int real_rear,queue_count;
	char * mem_addr;

	if (phq == NULL || buf == NULL || wlen > (RQ_LEN -4))
		return FALSE;

	queue_count = phq->rear - phq->front;

	if (queue_count == (RQ_NUM - 1))
	{
		phq->count_put_full++;
		return FALSE;
	}

	real_rear = phq->rear % RQ_NUM;
	mem_addr = phq->queue_buffer[real_rear];
	memcpy(mem_addr,&wlen,4);
	memcpy(mem_addr+4,buf,wlen);
	phq->rear++;

	return TRUE;
}

BOOL rqBufGet(RQUEUE * phq,char * buf,int * rlen)
{
	unsigned int real_front;

	if (phq == NULL || buf == NULL)
		return FALSE;

	if (phq->front == phq->rear)
	{
		return FALSE;
	}

	real_front = phq->front % RQ_NUM;
	phq->front++;

	memcpy(rlen,phq->queue_buffer[real_front],4);
	
	if ((*rlen) > (RQ_LEN-4))
	{
		return FALSE;
	}

	memcpy(buf, phq->queue_buffer[real_front]+4, *rlen);

	return TRUE;
}

char * rqGetHead(RQUEUE * phq,int * rlen)
{
	char * ret_ptr;

	unsigned int real_front = phq->front % RQ_NUM;

	if (phq->front == phq->rear)
		return NULL;

	ret_ptr = phq->queue_buffer[real_front];
	*rlen = *(int *)ret_ptr;

	return (ret_ptr+4);
}




