#ifndef __PROTOCOL_QUEUE__H__
#define __PROTOCOL_QUEUE__H__

#include <pthread.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct
{
	char *pFrame;
	int nFrameLen;
	int nFlag;
	int nFrameType; // 0: video p frame, 1: video keyframe, 2: audio
	int session;
	unsigned long ulLeadCode;
	unsigned long nTimestamp;
} FRAME_ENTRY;

struct FRAME_BUFFER_ENTRY
{
	FRAME_ENTRY frame;
	struct FRAME_BUFFER_ENTRY *next;
	struct FRAME_BUFFER_ENTRY *prev;
};
typedef struct FRAME_BUFFER_ENTRY FRAME_BUFFER_ENTRY;

typedef struct FRAME_BUFFER_MANAGER
{
	FRAME_BUFFER_ENTRY *head;
	FRAME_BUFFER_ENTRY *tail;
	int count;
	int max;
	pthread_mutex_t lock;
} FRAME_BUFFER_MANAGER;

int frame_mgr_init(FRAME_BUFFER_MANAGER *mgr, int maxSize);
int frame_mgr_release(FRAME_BUFFER_MANAGER *mgr);
int frame_mgr_push(FRAME_BUFFER_MANAGER *mgr, FRAME_ENTRY *frame);
int frame_mgr_pop(FRAME_BUFFER_MANAGER *mgr, FRAME_ENTRY *frame);
int frame_mgr_count(FRAME_BUFFER_MANAGER *mgr);
int frame_mgr_clear(FRAME_BUFFER_MANAGER *mgr);

#if defined(__cplusplus)
}
#endif

#endif
