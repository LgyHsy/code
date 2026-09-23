#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#include "anj_mw_comm.h"
#include "sem_util.h"

SemHandle SemCreate()
{
	SemHandle hndlSem = malloc(sizeof(pthread_mutex_t));
	if(hndlSem == NULL)
	{
    	__ERR("Not enough memory!!\n");
		return NULL;
	}
	/* Initialize the mutex which protects the global data */
    if( pthread_mutex_init(hndlSem, NULL) != 0 )
    {
    	__ERR("Sem_Creat init faill!!\n");
		free(hndlSem);
		return NULL;
    }
	return hndlSem;
}

int SemRelease(SemHandle hndlSem)
{
	if(hndlSem == NULL){
		__ERR("Invalid Semaphore handler\n");
		return -1;
	}
	pthread_mutex_unlock(hndlSem);
	return 0;
}

int SemWait(SemHandle hndlSem)
{
	if(hndlSem == NULL){
		__ERR("Invalid Semaphore handler\n");
		return -1;
	}
	pthread_mutex_lock(hndlSem);
	return 0;
}

int SemDestroy(SemHandle hndlSem)
{
	if(hndlSem == NULL){
		__ERR("Invalid Semaphore handler\n");
		return -1;
	}
	if( pthread_mutex_destroy(hndlSem)!= 0 )
	{
		__ERR("Sem_kill faill!!\n");
	}
	free(hndlSem);
	return 0;
}
