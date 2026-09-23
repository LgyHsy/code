#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <errno.h>

#include "anj_mw_aov_common.h"


//通知编码
static pthread_cond_t s_EncodeStart_Cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_EncodeStart_Mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char s_EncodeStart_Flag = TRUE;


//通知编码完成
static pthread_cond_t s_EncodeDone_Cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_EncodeDone_Mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char s_EncodeDone_Flag = FALSE;


//通知算法检测
static pthread_cond_t s_AlgoDetStart_Cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_AlgoDetStart_Mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char s_AlgoDetStart_Flag = TRUE;

//通知算法检测完成
static pthread_cond_t  s_AlgoDetDone_Cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_AlgoDetDone_Mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char s_AlgoDetDone_Flag	= FALSE;


static unsigned char s_EncodeStatus = TRUE;       // 编码状态 TRUE-在编码 FALSE-停止编码
static unsigned char s_AlgoDetectStatus = TRUE;   // 算法检测状态 TRUE-在检测 FALSE-停止检测

void aov_com_notify_encode_status_chg(int status)
{
    int tmp_status = (status > 0) ? TRUE : FALSE;

    if (tmp_status != s_EncodeStatus)
    {
        __INFO("notify encode status:%d to %d\n", s_EncodeStatus, tmp_status);
        s_EncodeStatus = tmp_status;
    }
}


// 通知编码开始
int aov_com_notify_encode_start()
{
	pthread_mutex_lock(&s_EncodeStart_Mutex);
	s_EncodeStart_Flag = TRUE;
	pthread_cond_signal(&s_EncodeStart_Cond);
	pthread_mutex_unlock(&s_EncodeStart_Mutex);

	return 0;
}

// 通知编码完成
int aov_com_notify_encode_done()
{
	pthread_mutex_lock(&s_EncodeDone_Mutex);
	s_AlgoDetStart_Flag = TRUE;
	pthread_cond_signal(&s_AlgoDetStart_Cond);
	pthread_mutex_unlock(&s_EncodeDone_Mutex); 

	return 0;
}


// 等待通知开始编码
int aov_com_wait_notify_encode_start()
{
	pthread_mutex_lock(&s_EncodeStart_Mutex);
	while (FALSE == s_EncodeStart_Flag && TRUE == s_EncodeStatus)
	{
		pthread_cond_wait(&s_EncodeStart_Cond, &s_EncodeStart_Mutex);
	}

	s_EncodeStart_Flag = FALSE;
	if(FALSE == s_EncodeStatus)
	{
		s_EncodeStart_Flag = TRUE;
	}
	pthread_mutex_unlock(&s_EncodeStart_Mutex);

	return 0;
}

// 等待通知编码完成
int aov_com_wait_notify_encode_done()
{
    pthread_mutex_lock(&s_EncodeDone_Mutex);
    while (FALSE == s_EncodeDone_Flag && TRUE == s_EncodeStatus)
    {
        pthread_cond_wait(&s_EncodeDone_Cond, &s_EncodeDone_Mutex);
    }

    s_EncodeDone_Flag = FALSE;
    pthread_mutex_unlock(&s_EncodeDone_Mutex);	

	return 0;
}


void aov_com_notify_algo_detect_status_change(int status)
{
    int tmp_status = (status > 0) ? TRUE : FALSE;
    if (tmp_status != s_AlgoDetectStatus)
    {
        __INFO("notify algo detect status:%d to %d\n", s_AlgoDetectStatus, tmp_status);
        s_AlgoDetectStatus = tmp_status;
    }
}

int aov_com_notify_algo_detect_start()
{
	pthread_mutex_lock(&s_AlgoDetStart_Mutex);
	s_AlgoDetStart_Flag = TRUE;
	pthread_cond_signal(&s_AlgoDetStart_Cond);
	pthread_mutex_unlock(&s_AlgoDetStart_Mutex);

	return 0;
}


int aov_com_notify_algo_detect_done()
{
	pthread_mutex_lock(&s_AlgoDetDone_Mutex);
	s_AlgoDetDone_Flag = TRUE;
	pthread_cond_signal(&s_AlgoDetDone_Cond);
	pthread_mutex_unlock(&s_AlgoDetDone_Mutex);

	return 0;
}


int aov_com_wait_notify_algo_detect_start()
{
	pthread_mutex_lock(&s_AlgoDetStart_Mutex);
	while (FALSE == s_AlgoDetStart_Flag && TRUE == s_AlgoDetectStatus)
	{
		pthread_cond_wait(&s_AlgoDetStart_Cond, &s_AlgoDetStart_Mutex);	
	}
	s_AlgoDetStart_Flag = FALSE;

	if(0 == s_AlgoDetectStatus)
	{
		s_AlgoDetStart_Flag = TRUE;
	}
	pthread_mutex_unlock(&s_AlgoDetStart_Mutex);

	return 0;
}

int aov_com_wait_notify_algo_detect_done()
{
	pthread_mutex_lock(&s_AlgoDetDone_Mutex);
	while (FALSE == s_AlgoDetDone_Flag && TRUE == s_AlgoDetectStatus)
	{
		pthread_cond_wait(&s_AlgoDetDone_Cond, &s_AlgoDetDone_Mutex);
	}
	s_AlgoDetDone_Flag = FALSE;
	pthread_mutex_unlock(&s_AlgoDetDone_Mutex);

	return 0;
}


static ST_VIF_SNRSleepParam_t s_stSNRSleepParam = {0};


MI_S32 ST_Common_Aov_Sleep_Enter()
{
    MI_S32 s32Ret = MI_SUCCESS;

    if (FALSE == s_stSNRSleepParam.bSleepEnable)
    {
        s_stSNRSleepParam.bSleepEnable = TRUE;
        s_stSNRSleepParam.u32FrameCntBeforeSleep = 1;
        s32Ret = MI_VIF_CustFunction(0, E_MI_VIF_CUSTCMD_SLEEPPARAM_SET, sizeof(ST_VIF_SNRSleepParam_t), &s_stSNRSleepParam);
    }

    return s32Ret;
}


MI_S32 ST_Common_Aov_Sleep_Exit()
{
    MI_S32 s32Ret = MI_SUCCESS;

    s_stSNRSleepParam.bSleepEnable = FALSE;
    s32Ret = MI_VIF_CustFunction(0, E_MI_VIF_CUSTCMD_SLEEPPARAM_SET, sizeof(ST_VIF_SNRSleepParam_t), &s_stSNRSleepParam);

    return s32Ret;
}

