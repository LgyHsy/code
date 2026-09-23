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

#ifndef	__H_INDEX_HASH_H__
#define	__H_INDEX_HASH_H__


//#define MAX_AGENT_NUM 		(256 * 1024)
#define IHASH_KEY_MAX_LEN		127

typedef struct index_hash_node
{
	unsigned int	bNodeValidate	:	1;
	unsigned int	bLinkValidate	:	1;
	unsigned int	bFreeList		:	1;
	unsigned int	reserved		:	29;
	char			key_str[IHASH_KEY_MAX_LEN+1];
	unsigned long	index;
	unsigned int	next_index;
}IHASHNODE;

typedef struct index_hash_ctx
{
	unsigned int	hash_num;
	unsigned int	link_num;
	IHASHNODE	*	hash_array;
	IHASHNODE	*	link_array;
	void *			hash_semMutex;
	void *			link_semMutex;
	unsigned int	_link_index;
	IHASHNODE *		p_node;
	unsigned int	conflict_cnt;
}IHASHCTX;

#ifdef __cplusplus
extern "C" {
#endif

IHASHCTX * init_ihash(unsigned int hash_num, unsigned int link_num);

unsigned int hash_link_pop(IHASHCTX * p_ctx);
void hash_link_push(IHASHCTX * p_ctx,unsigned int push_index);

//unsigned int ihash_index(char * key_str);
unsigned int ihash_index(IHASHCTX * p_ctx, const char * key_str);

BOOL ihash_add(IHASHCTX * p_ctx,char * key_str,unsigned int index, int type);
BOOL ihash_del(IHASHCTX * p_ctx,char * key_str,unsigned int index);

unsigned int find_index_from_keystr(IHASHCTX * p_ctx, const char * key_str);

void lock_ihash(IHASHCTX * p_ctx);
void unlock_ihash(IHASHCTX * p_ctx);

unsigned int find_index_by_keystr_start(IHASHCTX * p_ctx, const char * key_str);
unsigned int find_index_by_keystr_next(IHASHCTX * p_ctx, const char * key_str);

void * save_ihash_var(IHASHCTX * p_ctx);
void restore_ihash_var(IHASHCTX * p_ctx, void * p_node);


#ifdef __cplusplus
}
#endif

#endif	//	__H_INDEX_HASH_H__


