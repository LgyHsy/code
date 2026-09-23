#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_net.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_net.h"
#include "anj_ser.h"
#include "anj_ser_api.h"
#include "anj_ser_provider.h"
#include "anj_bind.h"
#include "anj_aiot.h"
#include "anj_aiot_register.h"
#include "aiot_cmd_recovery.h"
#include "AjP2pApi.h"
#include "cJSON.h"
#include "project_option.h"

#include "gct_apiv4.h"
#include "gct_common.h"

#define LIB_AUTH_KEY "XXXXXXXXXXXXXXXXXXXX"
#define LIB_COMID "XXXXXXXXXXXXXXXXXXXX"
#define PRODCUT_KEY "aiot_cam"

#define WAIT_TID_OVER_TIME (3 * 60)
#define AIOT_THREAD_SLEEP_TIME_MS (50 * 1000)
#define WAIT_NET_OVER_TIME (60)
#define INIT_RETRY_MAX (10)
#define INIT_RETRY_INTERVAL_SEC (3)

static anj_thread_s s_stAiotThread = {0};
static int s_bAjP2pInited = 0;

static const anj_ser_p2p_ops s_stAiotOps = {
    .p2p_type = P2P_TYPE_AIOT,
    .init = anj_aiot_init,
    .uninit = anj_aiot_uninit,
    .alarm_handle = NULL,
    .response = NULL,
    .bind = NULL,
    .unbind = NULL,
    .reset_conn = NULL,
    .push_video = NULL,
    .push_audio = NULL,
    .manual_unbind = NULL,
    .remove_unbind_device = NULL,
    .bind_status_get = NULL,
};

ANJ_LINK_KEEP(anj_keep_aiot_provider);

__attribute__((constructor)) static void anj_aiot_provider_register(void)
{
    anj_ser_p2p_provider_register(&s_stAiotOps);
}

static void debuglog(const char *fmt, ...)
{
    char content_buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, sizeof(content_buf), fmt, ap);
    va_end(ap);

    char timestr[64];
    struct tm *t, tbuf;
    time_t tsec = time(0);

    t = localtime_r(&tsec, &tbuf);

    int iRet = sprintf(timestr, "%04d-%02d-%02d %02d:%02d:%02d",
                       2000 + t->tm_year - 100, t->tm_mon + 1,
                       t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    if (iRet < 0)
    {
        memset(timestr, 0, sizeof(timestr));
    }

    __INFO("%s %s\n", timestr, content_buf);
}

static int anj_aiot_tid_load(char *buff, int size)
{
    int iRet = 0;
    ANJ_CHK(((buff != NULL) && (size > 0)), -1, "input Invalid");

    memset(buff, 0, size);
    if (anj_mw_read_file_limit_len(P2P_ID_FILE_NAME, buff, size) < 0)
    {
        __ERR("read %s failed.\n", P2P_ID_FILE_NAME);
        goto endFunc;
    }

    string_remove(buff, ' ');
    string_trim_tail(buff);
    if (strlen(buff) == 0)
    {
        __ERR("read %s is empty.\n", P2P_ID_FILE_NAME);
        goto endFunc;
    }

    __INFO("%s\n", buff);
endFunc:
    return iRet;
}

static int anj_aiot_tid_save(const char *buff, int size, const char *p2pid)
{
    (void)p2pid;
    int iRet = 0;
    ANJ_CHK(((buff != NULL) && (size > 0)), -1, "input Invalid");
    anj_mw_write_file(P2P_ID_FILE_NAME, 0, buff, strlen(buff));
endFunc:
    return iRet;
}

static int anj_aiot_tid_ready(char *szDeviceName, char *szSecret)
{
    int iRet = 0;
    cJSON *obj = NULL;
    ANJ_CHK(((szDeviceName != NULL) && (szSecret != NULL)), -1, "input Invalid");

    char buffer[256] = {0};
    cJSON *devicename = NULL;
    cJSON *devicesecret = NULL;

    ANJ_CHK_FUNC(anj_aiot_tid_load(buffer, sizeof(buffer)), 0, "load file path failed");

    obj = cJSON_Parse(buffer);
    if (obj == NULL)
    {
        if (strncmp(buffer, "antest00", 8) != 0)
        {
            iRet = -1;
            goto endFunc;
        }
        else
        {
            string_trim_tail(buffer);
            strcpy(szDeviceName, buffer);
            strcpy(szSecret, "654321");
            goto endFunc;
        }
    }

    devicename = cJSON_GetObjectItem(obj, "dn");
    devicesecret = cJSON_GetObjectItem(obj, "ds");
    if (devicename == NULL || devicesecret == NULL)
    {
        iRet = -1;
        goto endFunc;
    }

    if ((devicename->type != cJSON_String || devicename->valuestring == NULL || strlen(devicename->valuestring) == 0) ||
        (devicesecret->type != cJSON_String || devicesecret->valuestring == NULL || strlen(devicesecret->valuestring) == 0))
    {
        iRet = -1;
        goto endFunc;
    }

    strcpy(szDeviceName, devicename->valuestring);
    strcpy(szSecret, devicesecret->valuestring);

endFunc:
    if (obj)
        cJSON_Delete(obj);

    if (iRet)
    {
        __ERR("json parse failed\n");
    }
    else
    {
        __INFO("gid: %s\n", szDeviceName);
    }

    return iRet;
}

static void anj_aiot_tid_getok(void)
{
    char buffer[256] = {0};
    anj_ser_info *pstSerInfo = getSerInfo();
    int iRet = anj_aiot_tid_load(buffer, sizeof(buffer));
    __INFO("read p2pid file length %d\n lisence:%s\n", iRet, buffer);

    char szDn[64] = {0};
    char szSecret[64] = {0};
    if (anj_aiot_tid_ready(szDn, szSecret) == 0)
    {
        __INFO("get p2pid OK: %s/%s\n", szDn, szSecret);
        pstSerInfo->cloud_ready = 1;
    }
    else
    {
        __ERR("parse p2pid failed\n");
    }
}

static void anj_aiot_tid_reget(void)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    pstSerInfo->cloud_ready = 0;

    __ERR("get tid re get!!!\n");
    remove(P2P_ID_FILE_NAME);
}

static void anj_aiot_tid_settime(int gmt_seconds)
{
    (void)gmt_seconds;
}

static void anj_aiot_cloud_info_set(const char *buffer)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    snprintf(pstSerInfo->stP2pLoginState.devid, sizeof(pstSerInfo->stP2pLoginState.devid), "%s", buffer);
    strcat(pstSerInfo->stP2pLoginState.devid, "@");
    strcat(pstSerInfo->stP2pLoginState.devid, PRODCUT_KEY);

    char szChannels[16] = {0};
    sprintf(szChannels, "_%02u", ANJ_CAMERA_MAX_NUMS);
    strcat(pstSerInfo->stP2pLoginState.devid, szChannels);
    if (CUSTOMER_WTD == ANJ_CUSTOMER_TYPE)
    {
        strcat(pstSerInfo->stP2pLoginState.devid, "@001");
    }

    snprintf(pstSerInfo->report_dn, sizeof(pstSerInfo->report_dn), PRODCUT_KEY "%s", szChannels);

    __INFO("%s\n", pstSerInfo->stP2pLoginState.devid);
}

static void anj_aiot_cloud_online_set(int State)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    pstSerInfo->stP2pLoginState.logined = State;

    __INFO("%s:  %d\n", pstSerInfo->stP2pLoginState.devid, State);
    if (s_bAjP2pInited)
    {
        AjP2pSetLoginStatus(State);
    }
}

static void anj_aiot_devinfo_prepare(DevInfo *pstDevInfo)
{
    if (pstDevInfo == NULL)
    {
        return;
    }

    if (pstDevInfo->version_name[0] == '\0')
    {
        const char *fsver = pstDevInfo->stVersionInfo.fsVersion;
        const char *vp = strrchr(fsver, '_');

        if (vp != NULL && vp[1] == 'V')
        {
            snprintf(pstDevInfo->version_name, sizeof(pstDevInfo->version_name), "%s", vp + 1);
        }
        else
        {
            strncpy(pstDevInfo->version_name, "V0", sizeof(pstDevInfo->version_name) - 1);
        }
    }
}

static int anj_aiot_iface_usable(const char *ifname, char *ip, int ip_len)
{
    struct NET_CONFIG netcfg;

    if (ifname == NULL || net_get_info(ifname, &netcfg) != 0)
    {
        return -1;
    }

    if (netcfg.ifaddr == 0 || netcfg.gateway == 0)
    {
        return -1;
    }

    if (ip != NULL && ip_len > 0)
    {
        _inet_ntoa_r(netcfg.ifaddr, ip, ip_len);
    }

    return 0;
}

static int anj_aiot_wait_network(int *bStart)
{
    int wait_time = WAIT_NET_OVER_TIME;
    char ip[32] = {0};

    while (bStart && *bStart && (wait_time-- > 0))
    {
        if (anj_aiot_iface_usable(WIRE_INTERFACE_NAME, ip, sizeof(ip)) == 0)
        {
            __INFO("recovery aiot: network ready, if=%s ip=%s\n", WIRE_INTERFACE_NAME, ip);
            return 0;
        }

        anj_net_status_e netStatus = anj_net_status_check();
        if (netStatus == ANJ_NET_STATUS_WIFI &&
            anj_aiot_iface_usable(WIFI_INTERFACE_NAME, ip, sizeof(ip)) == 0)
        {
            __INFO("recovery aiot: network ready, if=%s ip=%s\n", WIFI_INTERFACE_NAME, ip);
            return 0;
        }

        if (netStatus == ANJ_NET_STATUS_4G &&
            anj_aiot_iface_usable(WIRE_INTERFACE_NAME1, ip, sizeof(ip)) == 0)
        {
            __INFO("recovery aiot: network ready, if=%s ip=%s\n", WIRE_INTERFACE_NAME1, ip);
            return 0;
        }

        sleep(1);
    }

    return -1;
}

static int anj_aiot_set_ifname(anj_net_status_e netStatus)
{
    if (netStatus == ANJ_NET_STATUS_WIRE)
    {
        return gct_apiv4_set_ifname(WIRE_INTERFACE_NAME);
    }
    if (netStatus == ANJ_NET_STATUS_WIFI)
    {
        return gct_apiv4_set_ifname(WIFI_INTERFACE_NAME);
    }
    if (netStatus == ANJ_NET_STATUS_4G)
    {
        return gct_apiv4_set_ifname(WIRE_INTERFACE_NAME1);
    }

    __ERR("recovery aiot: unknown net status:%d\n", netStatus);
    return -1;
}

static int anj_aiot_gct_init(anj_ser_info *pstSerInfo)
{
    gct_param gctparam = {0};

    strcpy(gctparam.szGid, pstSerInfo->uid);
    strcpy(gctparam.szGidPwd, pstSerInfo->secret);
    gctparam.nSupportModule = GCT_MODULE_IPC | GCT_MODULE_CLOUD | GCT_MODULE_DOORBELL;
    gctparam.nChannelCount = 1;
    gctparam.nVideoSessionLimit = 2 + ANJ_CAMERA_MAX_NUMS;
    gctparam.nReplaySessionLimit = ANJ_CAMERA_MAX_NUMS;
    gctparam.nStreamSupportType = STREAM_SUPPORT_TYPE_MAIN | STREAM_SUPPORT_TYPE_SUB;
    gctparam.nConnectionCount = gctparam.nVideoSessionLimit * 2;
    strcpy(gctparam.szLocalCfgFilePath, LOCAL_CFG_PATH);
    strcpy(gctparam.szSdcardAbsPath, SDCARD_PATH);
    strcpy(gctparam.szAuthKey, LIB_AUTH_KEY);
    strcpy(gctparam.szComId, LIB_COMID);

    return gct_apiv4_init(gctparam);
}

static int anj_aiot_load_capability(void)
{
    DevInfo *pstDevInfo = getDevInfo();
    char *capability_str = anj_sysctl_get_capability_string();
    cJSON *rootObj = cJSON_CreateObject();
    if (rootObj == NULL || capability_str == NULL)
    {
        if (rootObj)
        {
            cJSON_Delete(rootObj);
        }
        return -1;
    }

    cJSON_AddStringToObject(rootObj, "cap", capability_str);
    if (pstDevInfo != NULL)
    {
        cJSON_AddStringToObject(rootObj, "fsver", pstDevInfo->stVersionInfo.fsVersion);
    }
    char *jsonStr = cJSON_PrintUnformatted(rootObj);
    cJSON_Delete(rootObj);
    if (jsonStr == NULL)
    {
        return -1;
    }

    __INFO("recovery capability:%s\n", jsonStr);
    gct_apiv4_device_ability_v2(jsonStr, strlen(jsonStr));
    anj_mw_free(jsonStr);
    return 0;
}

static int anj_aiot_info_init(int bStart)
{
    int iRet = 0;
    int wait_time = WAIT_TID_OVER_TIME;
    anj_ser_info *pstSerInfo = getSerInfo();
    DevInfo *pstDevInfo = getDevInfo();
    NetworkConfigNew *pstNetWorkConfig = getNetWorkConfig();

    if (bStart == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    anj_aiot_devinfo_prepare(pstDevInfo);

    if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0)
    {
        __INFO("exist p2p conf already, skip AjP2pApi.\n");
        pstSerInfo->cloud_ready = 1;
        anj_aiot_cloud_info_set(pstSerInfo->uid);
        return 0;
    }

    AjP2pApiExit();

    __INFO("SN:%s dev:%s cus:%s ver:%s uuid:%s\n", pstDevInfo->sn, pstDevInfo->devType,
           pstDevInfo->custom_name, pstDevInfo->version_name, pstDevInfo->uuid);
    iRet = AjP2pApiInit(AJ_P2P_TYPE_AIOT, 1, 1, pstDevInfo->sn, pstDevInfo->devType,
                        pstDevInfo->custom_name, pstDevInfo->version_name, PRODCUT_CODE, pstDevInfo->uuid);
    if (iRet != AJ_P2P_SUCC)
    {
        __ERR("AjP2pApiInit failed , iRet = %d\n", iRet);
        iRet = -1;
        goto endFunc;
    }

    AjP2pSetAuthCode(pstNetWorkConfig->p2pCfg.authcode);

    iRet = AjP2pApiSetCallback(anj_aiot_tid_load, anj_aiot_tid_save, anj_aiot_tid_getok,
                               anj_aiot_tid_reget, anj_aiot_tid_settime, debuglog);
    if (iRet != AJ_P2P_SUCC)
    {
        __ERR("AjP2pApiSetCallback failed , iRet = %d\n", iRet);
        iRet = -1;
        goto endFunc;
    }

    iRet = AjP2pApiStart();
    if (iRet != AJ_P2P_SUCC)
    {
        __ERR("AjP2pApiStart failed , iRet = %d\n", iRet);
        iRet = -1;
        goto endFunc;
    }

    while ((wait_time--) > 0)
    {
        if (bStart == 0)
        {
            iRet = -1;
            goto endFunc;
        }

        if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0)
            break;

        __INFO("Wait P2PID\n");
        sleep(1);
    }

    if (wait_time <= 0)
    {
        __ERR("load cloud ID failed!!!\n");
        iRet = -1;
        goto endFunc;
    }

    AjP2pStopGetP2pIDFromCloud();
    anj_aiot_cloud_info_set(pstSerInfo->uid);
    anj_aiot_cloud_online_set(0);
    AjP2pSetP2pID(pstSerInfo->uid);
    s_bAjP2pInited = 1;

    __INFO("p2pIdInfoInit success\n");

endFunc:
    return iRet;
}

static int anj_aiot_thread(void *ctx, int *bStart)
{
    (void)ctx;
    sleep(3);

    DevInfo *pInfo = getDevInfo();
    while (bStart && *bStart && pInfo && (pInfo->activated == 0))
    {
        sleep(1);
    }

    if (anj_aiot_wait_network(bStart) != 0)
    {
        __ERR("recovery aiot: network not ready\n");
        return -1;
    }

    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    iRet = anj_aiot_info_init(*bStart);
    if (iRet != AJ_P2P_SUCC)
    {
        __ERR("anj_aiot_info_init failed\n");
        return -1;
    }

    gct_apiv4_log_savelocal(GCT_FALSE);
    gct_apiv4_log_savepath(LOCAL_PATH_LOG);
    anj_aiot_callback_init();
    anjrec_aiot_cmd_init();
    gct_common_time_syn_type(GCT_TIME_SYN_TYPE_SYN_ZONE_RIGHT);

    int retry = 0;
    anj_net_status_e netStatus = ANJ_NET_STATUS_WIRE;
    while (bStart && *bStart && retry < INIT_RETRY_MAX)
    {
        if (anj_aiot_wait_network(bStart) != 0)
        {
            __ERR("recovery aiot: network not ready, retry=%d/%d\n",
                  retry + 1, INIT_RETRY_MAX);
            sleep(INIT_RETRY_INTERVAL_SEC);
            retry++;
            continue;
        }

        netStatus = anj_net_status_check();
        iRet = anj_aiot_gct_init(pstSerInfo);
        if (iRet >= 0)
        {
            iRet = anj_aiot_set_ifname(netStatus);
        }

        if (iRet >= 0)
        {
            __INFO("recovery gct_apiv4_init success, retry=%d\n", retry);
            break;
        }

        __ERR("recovery gct_apiv4_init failed, iRet=%d, retry=%d/%d\n",
              iRet, retry + 1, INIT_RETRY_MAX);
        gct_apiv4_release();
        sleep(INIT_RETRY_INTERVAL_SEC);
        retry++;
    }

    if (iRet < 0)
    {
        __ERR("recovery gct_apiv4_init gave up after %d retries\n", INIT_RETRY_MAX);
        return -1;
    }

    anj_aiot_load_capability();

    GCT_STATE_TO_SERVER LastConnectStatus = GCT_STATE_TO_SERVER_NO_START;
    while (bStart && *bStart)
    {
        usleep(AIOT_THREAD_SLEEP_TIME_MS);

        GCT_STATE_TO_SERVER ConnectStatus = gct_apiv4_get_state_to_server();
        if (ConnectStatus != LastConnectStatus)
        {
            __INFO("ConnectStatus:%d\n", ConnectStatus);
            if (ConnectStatus == GCT_STATE_TO_SERVER_REG_SUCESS)
            {
                __INFO("recovery aiot cloud online: %s\n", pstSerInfo->stP2pLoginState.devid);
                anj_aiot_cloud_online_set(1);
            }
            else if (LastConnectStatus == GCT_STATE_TO_SERVER_REG_SUCESS)
            {
                __ERR("recovery aiot cloud offline, status=%d\n", ConnectStatus);
                anj_aiot_cloud_online_set(0);
            }
            LastConnectStatus = ConnectStatus;
        }
    }

    return iRet;
}

int anj_aiot_init(void)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0)
    {
        anj_aiot_cloud_info_set(pstSerInfo->uid);
    }

    s_stAiotThread.bAutoDestroy = 1;
    strncpy(s_stAiotThread.iThreadName, "aiot_recovery", sizeof(s_stAiotThread.iThreadName) - 1);
    s_stAiotThread.iThreadjob.ctx = &s_stAiotThread;
    s_stAiotThread.iThreadjob.func = anj_aiot_thread;
    return anj_thread_task_create(&s_stAiotThread);
}

void anj_aiot_uninit(void)
{
    anj_thread_task_destroy(&s_stAiotThread, -1);
    if (s_bAjP2pInited)
    {
        AjP2pApiExit();
        s_bAjP2pInited = 0;
    }
}
