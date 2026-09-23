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
#include "sys_log.h"
#include "ihash.h"

IHASHCTX * init_ihash(unsigned int hash_num, unsigned int link_num)
{
	IHASHCTX * p_ctx = (IHASHCTX *)malloc(sizeof(IHASHCTX));
	if(p_ctx == NULL)
	{
		return NULL;
	}

	memset(p_ctx,0,sizeof(IHASHCTX));

	p_ctx->hash_num = hash_num;
	p_ctx->link_num = link_num;

	p_ctx->hash_array = (IHASHNODE *)malloc(sizeof(IHASHNODE) * hash_num);
	if(p_ctx->hash_array == NULL)
	{
		free(p_ctx);
		return NULL;
	}

	p_ctx->link_array = (IHASHNODE *)malloc(sizeof(IHASHNODE) * link_num);
	if(p_ctx->link_array == NULL)
	{
		free(p_ctx->hash_array);
		free(p_ctx);
		return NULL;
	}

	p_ctx->hash_semMutex  = sys_os_create_mutex();
	p_ctx->link_semMutex  = sys_os_create_mutex();

	for(unsigned int i=1; i<p_ctx->link_num; i++)
	{
		p_ctx->link_array[i].next_index = i+1;
		if(i == (p_ctx->link_num - 1))
			p_ctx->link_array[i].next_index = 0;
	}

	p_ctx->_link_index = 1;

	return p_ctx;
}

unsigned int hash_link_pop(IHASHCTX * p_ctx)
{
	sys_os_mutex_enter(p_ctx->link_semMutex);

	unsigned int ret_index = p_ctx->_link_index;
	p_ctx->_link_index = p_ctx->link_array[p_ctx->_link_index].next_index;

	p_ctx->link_array[ret_index].bFreeList = 0;

	sys_os_mutex_leave(p_ctx->link_semMutex);

	return ret_index;
}

void hash_link_push(IHASHCTX * p_ctx,unsigned int push_index)
{
	if(push_index == 0)
	{
		log_print("hash_link_push::push_index == 0!!!\r\n");
		return;
	}

	if(push_index >= p_ctx->link_num)
	{
		log_print("hash_link_push::push_index[%u] >= MAX_NUM[%u]!!!\r\n",push_index,p_ctx->link_num);
		return;
	}

	if(p_ctx->link_array[push_index].bFreeList == 1)
	{
		log_print("hash_link_push::push_index[%u] bFreeList == 1!!!\r\n",push_index);
		return;
	}

	sys_os_mutex_enter(p_ctx->link_semMutex);

	memset(&p_ctx->link_array[push_index],0,sizeof(IHASHNODE));
	p_ctx->link_array[push_index].bFreeList = 1;

	p_ctx->link_array[push_index].next_index = p_ctx->_link_index;
	p_ctx->_link_index = push_index;

	sys_os_mutex_leave(p_ctx->link_semMutex);
}

unsigned int ihash_index(IHASHCTX * p_ctx, const char * key_str)
{
	int len = strlen(key_str);

	register unsigned int nr=1, nr2=4;
	register unsigned char ctmp;
	for(int i=0; i<len; i++)
	{
		ctmp = key_str[i];
		nr^= (((nr & 63)+nr2)*((unsigned int)ctmp))+ (nr << 8);
		nr2+=3;
	}

//	return(nr & (MAX_AGENT_NUM -1));
	return(nr & (p_ctx->hash_num -1));
}

BOOL ihash_add(IHASHCTX * p_ctx,char * key_str,unsigned int index, int type)
{
	if(p_ctx == NULL)
		return FALSE;

	unsigned int hash_key = ihash_index(p_ctx,key_str);

	sys_os_mutex_enter(p_ctx->hash_semMutex);

	if(p_ctx->hash_array[hash_key].bNodeValidate == 0)
	{
		strncpy(p_ctx->hash_array[hash_key].key_str, key_str, IHASH_KEY_MAX_LEN);
		p_ctx->hash_array[hash_key].index = index;
		p_ctx->hash_array[hash_key].next_index = 0;
		p_ctx->hash_array[hash_key].bLinkValidate = 0;
		p_ctx->hash_array[hash_key].bNodeValidate = 1;
	}
	else
	{
		IHASHNODE * p_node = &p_ctx->hash_array[hash_key];

		if(type == 0)
		{
			while((p_node->bLinkValidate == 1) && (strcmp(key_str,p_node->key_str) != 0))
			{
				p_node = &p_ctx->link_array[p_node->next_index];
				if((unsigned int)(p_node - p_ctx->link_array) > p_ctx->link_num)
				{
					sys_os_mutex_leave(p_ctx->hash_semMutex);
					log_print("ihash_add::link node address = 0x%x\r\n",p_node);
					return FALSE;
				}
			}

			if(strcmp(key_str,p_node->key_str) == 0)
			{
				if (p_node->index == index)
				{
					log_print("ihash_add::the same overlap key[%s], index[%d]\r\n", key_str, index);
				}
				else
				{
					log_print("ihash_add::modify the index key[%s], p_node_index[%d], index[%d]\r\n",
							key_str, p_node->index, index);
					p_node->index = index;
				}
			}
			else
			{
				unsigned int link_index = hash_link_pop(p_ctx);
				if(link_index == 0)
				{
					sys_os_mutex_leave(p_ctx->hash_semMutex);
					log_print("ihash_add:no free node!!!\r\n");
					return FALSE;
				}
	
				strncpy(p_ctx->link_array[link_index].key_str, key_str, IHASH_KEY_MAX_LEN);
				p_ctx->link_array[link_index].index = index;
				p_ctx->link_array[link_index].next_index = 0;
				p_ctx->link_array[link_index].bLinkValidate = 0;
				p_ctx->link_array[link_index].bNodeValidate = 1;
	
				p_node->next_index = link_index;
				p_node->bLinkValidate = 1;
			}
		}
		else //(type == 1)
		{
			while((p_node->bLinkValidate == 1) && ((strcmp(key_str,p_node->key_str) != 0) || p_node->index != index))
			{
				p_node = &p_ctx->link_array[p_node->next_index];
				if((unsigned int)(p_node - p_ctx->link_array) > p_ctx->link_num)
				{
					sys_os_mutex_leave(p_ctx->hash_semMutex);
					log_print("ihash_add::link node address = 0x%x\r\n",p_node);
					return FALSE;
				}
			}

			if((strcmp(key_str,p_node->key_str) == 0) && p_node->index == index)
			{
				log_print("ihash_add::the same overlap key[%s], index[%d]\r\n", key_str, index);
			}
			else
			{
				unsigned int link_index = hash_link_pop(p_ctx);
				if(link_index == 0)
				{
					sys_os_mutex_leave(p_ctx->hash_semMutex);
					log_print("ihash_add:no free node!!!\r\n");
					return FALSE;
				}
	
				strncpy(p_ctx->link_array[link_index].key_str, key_str, IHASH_KEY_MAX_LEN);
				p_ctx->link_array[link_index].index = index;
				p_ctx->link_array[link_index].next_index = 0;
				p_ctx->link_array[link_index].bLinkValidate = 0;
				p_ctx->link_array[link_index].bNodeValidate = 1;

				p_node->next_index = link_index;
				p_node->bLinkValidate = 1;
			}
		}
	}

	sys_os_mutex_leave(p_ctx->hash_semMutex);

	return TRUE;

}

BOOL ihash_del(IHASHCTX * p_ctx,char * key_str,unsigned int index)
{
	if(p_ctx == NULL)
		return FALSE;

	unsigned int hash_key = ihash_index(p_ctx,key_str);

	sys_os_mutex_enter(p_ctx->hash_semMutex);

	if(p_ctx->hash_array[hash_key].bNodeValidate == 0)
	{
		sys_os_mutex_leave(p_ctx->hash_semMutex);
		return FALSE;
	}


	IHASHNODE * p_node = &p_ctx->hash_array[hash_key];

	if((strcmp(key_str,p_node->key_str) == 0) && (p_node->index == index))
	{
		p_ctx->hash_array[hash_key].bNodeValidate = 0;

		if(p_ctx->hash_array[hash_key].bLinkValidate == 1)
		{
			unsigned int del_link_index = p_ctx->hash_array[hash_key].next_index;
			IHASHNODE * p_del_node = &p_ctx->link_array[del_link_index];

			memcpy(p_node,p_del_node,sizeof(IHASHNODE));

			hash_link_push(p_ctx,del_link_index);
		}

		sys_os_mutex_leave(p_ctx->hash_semMutex);
		return TRUE;
	}
	else
	{
		IHASHNODE * p_node_prev = p_node;

		while(p_node_prev->bLinkValidate == 1)
		{
			p_node = &p_ctx->link_array[p_node_prev->next_index];

			if((unsigned int)(p_node - p_ctx->link_array) > p_ctx->link_num)
			{
				sys_os_mutex_leave(p_ctx->hash_semMutex);
				log_print("ihash_del::link node address = 0x%x\r\n",p_node);
				return FALSE;
			}

			if((strcmp(key_str,p_node->key_str) == 0) && (p_node->index == index))
			{
				p_node->bNodeValidate = 0;

				p_node_prev->next_index = p_node->next_index;
				p_node_prev->bLinkValidate = p_node->bLinkValidate;

				unsigned int del_link_index = p_node - p_ctx->link_array;
				hash_link_push(p_ctx,del_link_index);

				break;
			}

			p_node_prev = p_node;
		}
	}

	sys_os_mutex_leave(p_ctx->hash_semMutex);

	return TRUE;
}

unsigned int find_index_from_keystr(IHASHCTX * p_ctx, const char * key_str)
{
	if(p_ctx == NULL)
		return 0xFFFFFFFF;

	unsigned int hash_key = ihash_index(p_ctx,key_str);
	IHASHNODE * p_node = &p_ctx->hash_array[hash_key];

	sys_os_mutex_enter(p_ctx->hash_semMutex);

	while(p_node->bNodeValidate == 1)
	{
		if(strcmp(key_str,p_node->key_str) == 0)
		{
			sys_os_mutex_leave(p_ctx->hash_semMutex);
			return p_node->index;
		}

		if(p_node->bLinkValidate == 1)
			p_node = &p_ctx->link_array[p_node->next_index];
		else
			break;
	}

	sys_os_mutex_leave(p_ctx->hash_semMutex);

	return 0xFFFFFFFF;
}

void lock_ihash(IHASHCTX * p_ctx)
{
	if(p_ctx != NULL)
		sys_os_mutex_enter(p_ctx->hash_semMutex);
}

void unlock_ihash(IHASHCTX * p_ctx)
{
	if(p_ctx != NULL)
		sys_os_mutex_leave(p_ctx->hash_semMutex);
}

unsigned int find_index_by_keystr_start(IHASHCTX * p_ctx, const char * key_str)
{
	if(p_ctx == NULL)
		return 0xFFFFFFFF;

	unsigned int hash_key = ihash_index(p_ctx,key_str);
	IHASHNODE * p_node = &p_ctx->hash_array[hash_key];

	while(p_node->bNodeValidate == 1)
	{
		if(strcmp(key_str,p_node->key_str) == 0)
		{
			p_ctx->p_node = &p_ctx->link_array[p_node->next_index];
			return p_node->index;
		}

		if(p_node->bLinkValidate == 1)
			p_node = &p_ctx->link_array[p_node->next_index];
		else
			break;
	}

	return 0xFFFFFFFF;
}

unsigned int find_index_by_keystr_next(IHASHCTX * p_ctx, const char * key_str)
{
	if(p_ctx == NULL || p_ctx->p_node == NULL)
		return 0xFFFFFFFF;

	IHASHNODE * p_node = p_ctx->p_node;

	while(p_node && p_node->bNodeValidate == 1)
	{
		if(strcmp(key_str,p_node->key_str) == 0)
		{
			p_ctx->p_node = &p_ctx->link_array[p_node->next_index];
			return p_node->index;
		}

		if(p_node->bLinkValidate == 1)
			p_node = &p_ctx->link_array[p_node->next_index];
		else
			break;
	}

	return 0xFFFFFFFF;
}

void * save_ihash_var(IHASHCTX * p_ctx)
{
	if(p_ctx == NULL)
		return NULL;

	return p_ctx->p_node;
}

void restore_ihash_var(IHASHCTX * p_ctx, void * p_node)
{
	if(p_ctx == NULL)
		return;

	p_ctx->p_node = (IHASHNODE *)p_node;
}


