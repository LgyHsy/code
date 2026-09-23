#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "protocol_queue.h"
#include "anj_mw_comm.h"
#include "anj_mw_mutex.h"
#include "anj_mw_mem.h"

int frame_mgr_init(FRAME_BUFFER_MANAGER *mgr, int maxSize)
{
	if (mgr == NULL)
		return -1;
	else
	{
		mgr->head = NULL;
		mgr->tail = NULL;
		mgr->count = 0;
		mgr->max = maxSize; // 50;
		anj_mutex_create(&mgr->lock, 0);

		return 0;
	}
}

int frame_mgr_release(FRAME_BUFFER_MANAGER *mgr)
{
	if (mgr == NULL)
		return -1;
	else
	{
		FRAME_BUFFER_ENTRY *pItem = mgr->head;
		FRAME_BUFFER_ENTRY *pItemNext;
		while (pItem)
		{
			pItemNext = pItem->next;

			if (pItem->frame.pFrame && pItem->frame.nFrameLen)
			{
				anj_mw_free(pItem->frame.pFrame);
			}

			anj_mw_free(pItem);
			pItem = pItemNext;
		}

		mgr->head = NULL;
		mgr->tail = NULL;
		mgr->count = 0;
		anj_mutex_destroy(&mgr->lock);
	}
	return 0;
}

int frame_mgr_push(FRAME_BUFFER_MANAGER *mgr, FRAME_ENTRY *frame)
{
	if (mgr && frame && frame->pFrame && frame->nFrameLen)
	{
		FRAME_BUFFER_ENTRY *entry = (FRAME_BUFFER_ENTRY *)anj_mw_malloc(sizeof(FRAME_BUFFER_ENTRY));

		memcpy(&entry->frame, frame, sizeof(FRAME_ENTRY));

		entry->next = NULL;
		entry->prev = NULL;

		// append it to tail
		anj_mutex_lock(&mgr->lock);
		if (mgr->tail)
		{
			//__ERR("append cmd to tail\n");

			mgr->tail->next = entry;
			entry->prev = mgr->tail;
			mgr->tail = entry;
			mgr->count++;

			//__ERR("append one msg, count = %d\n", mgr->count);

			if (mgr->count > mgr->max)
			{
				//__ERR("boa: buffer full, overwrite!!!\n");

				// delete this entry
				FRAME_BUFFER_ENTRY *entry = mgr->head;

				mgr->head = mgr->head->next;

				anj_mw_free(entry->frame.pFrame);
				anj_mw_free(entry);

				if (mgr->head)
					mgr->head->prev = NULL;

				mgr->count--;
			}
		}
		else
		{
			if (mgr->head)
			{
				//	__ERR("head not NULL but tail is NULL!\n");
			}
			else
			{
				mgr->head = entry;
				mgr->tail = entry;
				mgr->count = 1;

				//__ERR("append one msg as head, count = %d\n", mgr->count);
			}
		}

		anj_mutex_unlock(&mgr->lock);

		return frame->nFrameLen;
	}

	return -1;
}

int frame_mgr_pop(FRAME_BUFFER_MANAGER *mgr, FRAME_ENTRY *frame)
{
	if (mgr)
	{
		anj_mutex_lock(&mgr->lock);

		int ret = 0;
		if (mgr->head)
		{
			memcpy(frame, &(mgr->head->frame), sizeof(FRAME_ENTRY));

			FRAME_BUFFER_ENTRY *entry = mgr->head;

			mgr->head = mgr->head->next;

			anj_mw_free(entry);

			if (mgr->head)
				mgr->head->prev = NULL;
			else
			{
				mgr->tail = NULL;
			}
			ret = frame->nFrameLen;
			mgr->count--;
		}
		else
		{
			ret = 0;
		}

		anj_mutex_unlock(&mgr->lock);

		return ret;
	}

	return -1;
}

int frame_mgr_count(FRAME_BUFFER_MANAGER *mgr)
{
	int count = 0;
	if (mgr)
	{
		// get head
		anj_mutex_lock(&mgr->lock);
		/*
		FRAME_BUFFER_ENTRY *head 	= NULL;
		head = mgr->head;
		while(head)
		{
			count++;
			head = mgr->head->next;
		}
		*/
		count = mgr->count;
		anj_mutex_unlock(&mgr->lock);

		return count;
	}

	return count;
}

int frame_mgr_clear(FRAME_BUFFER_MANAGER *mgr)
{
	FRAME_BUFFER_ENTRY *head = NULL;
	FRAME_BUFFER_ENTRY *entry = NULL;
	if (mgr)
	{
		int anj_mw_freecount = 0;
		int delcount = 0;
		// get head
		anj_mutex_lock(&mgr->lock);

		head = mgr->head;
		while (head)
		{
			entry = head;
			head = entry->next;

			if (entry->frame.pFrame && entry->frame.nFrameLen)
			{
				anj_mw_freecount++;
				anj_mw_free(entry->frame.pFrame);
			}

			anj_mw_free(entry);
			delcount++;
		}

		mgr->head = NULL;
		mgr->tail = NULL;
		mgr->count = 0;

		anj_mutex_unlock(&mgr->lock);

		return 0;
	}

	return 0;
}
