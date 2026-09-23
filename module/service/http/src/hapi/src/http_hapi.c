#include <stdio.h>


#include "anj_mw_comm.h"
#include "anj_config.h"
#include "alarm_link.h"

#include "http_hapi.h"
#include "hapi_handle.h"
#include "hapi_subs.h"

int http_hapi_init()
{
    int iRet = 0;
    hapi_add_alarmserver();

    return iRet;
}

void http_hapi_uninit()
{
    hapi_notify_uninit();
}


