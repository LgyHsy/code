#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <errno.h>

#ifdef _USE_MODULE_AOV_

#include "anj_mw_log.h"
#include "anj_mw_aov.h"
#include "anj_mw_aov_common.h"


int anj_mw_aov_notify_enc_status_chg(int status)
{
    aov_com_notify_encode_status_chg(status);
    return 0;
}

int anj_mw_aov_notify_enc_start()
{
    int iRet = 0;
    iRet = aov_com_notify_encode_start();
    return iRet;
}

int anj_mw_aov_notify_enc_done()
{
    int iRet = 0;
    iRet = aov_com_notify_encode_done();
    return iRet;
}

int anj_mw_aov_wait_notify_enc_start()
{
    int iRet = 0;
    iRet = aov_com_wait_notify_encode_start();
    return iRet;
}

int anj_mw_aov_wait_notify_enc_done()
{
    int iRet = 0;
    iRet = aov_com_wait_notify_encode_done();
    return iRet;
}


int anj_mw_aov_notify_algo_status_chg(int status)
{
    aov_com_notify_algo_detect_status_change(status);
    return 0;
}

int anj_mw_aov_notify_algo_start()
{
    int iRet = 0;
    iRet = aov_com_notify_algo_detect_start();
    return iRet;
}

int anj_mw_aov_notify_algo_done()
{
    int iRet = 0;
    iRet = aov_com_notify_algo_detect_done();
    return iRet;
}

int anj_mw_aov_wait_notify_algo_start()
{
    int iRet = 0;
    iRet = aov_com_wait_notify_algo_detect_start();
    return iRet;
}

int anj_mw_aov_wait_notify_algo_done()
{
    int iRet = 0;
    iRet = aov_com_wait_notify_algo_detect_done();
    return iRet;
}

int anj_mw_aov_sys_sleep_enter()
{
    ST_Common_Aov_Sleep_Enter();
    return 0;
}

int anj_mw_aov_sys_sleep_exit()
{
    ST_Common_Aov_Sleep_Exit();
    return 0;
}

#endif
