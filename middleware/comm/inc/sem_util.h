#ifndef __SEM_UTIL_H__
#define __SEM_UTIL_H__

#if defined (__cplusplus)
extern "C" {
#endif

typedef void* SemHandle;

SemHandle SemCreate();
int SemRelease(SemHandle hndlSem);
int SemWait(SemHandle hndlSem);
int SemDestroy(SemHandle hndlSem);

#if defined (__cplusplus)
}
#endif

#endif
