#include <stdarg.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_net.h"
#include "anj_ser_api.h"
#include "anj_ser_provider.h"
#include "anj_sys.h"
#include "anj_bind.h"
#include "anj_audio.h"
#include "anj_osd.h"
#include "anj_aiot.h"
#include "anj_aiot_cmd.h"
#include "anj_aiot_register.h"
#include "anj_aiot_report.h"
#include "anj_aiot_stream.h"
#include "AjP2pApi.h"
#include "function_list.h"
#include "cmd_def.h"
#include "eventhub.h"
#include "file_receiver.h"
#include "dev_bind.h"
#include "cJSON.h"

#include "gct_apiv4.h"
#include "gct_sdcard_playback_callback.h"
#include "gct_sdcard_playback_apiv4.h"

#define LIB_AUTH_KEY "XXXXXXXXXXXXXXXXXXXX" // 授权key (浪涛授权,每个公司都不一样,请注意保存)
#define LIB_COMID "XXXXXXXXXXXXXXXXXXXX"    // 公司id (浪涛授权,每个公司都不一样,请注意保存)

#define PRODCUT_KEY "aiot_cam"

#define WAIT_TID_OVER_TIME (3 * 60)

#define AIOT_THREAD_SLEEP_TIME_MS (50 * 1000)
#define AIOT_THREAD_SECOND_TIME (1000 * 1000) / AIOT_THREAD_SLEEP_TIME_MS
#define CHECK_DEV_REPOTY_TIMES (60 * AIOT_THREAD_SECOND_TIME) // 1MIN

static const anj_ser_p2p_ops s_stAiotOps = {
    .p2p_type = P2P_TYPE_AIOT,
    .init = anj_aiot_init,
    .uninit = anj_aiot_uninit,
    .alarm_handle = anj_aiot_report_alarm_handle,
    .response = anj_aiot_reponse,
    .bind = anj_aiot_bind,
    .unbind = anj_aiot_unbind,
    .reset_conn = anj_aiot_reset_conn,
    .push_video = anj_aiot_push_video,
    .push_audio = anj_aiot_push_audio,
    .manual_unbind = anj_aiot_manual_unbind,
    .remove_unbind_device = anj_aiot_remove_unbind_device,
    .bind_status_get = anj_aiot_bind_status_get,
};

ANJ_LINK_KEEP(anj_keep_aiot_provider);

__attribute__((constructor)) static void anj_aiot_provider_register(void)
{
    anj_ser_p2p_provider_register(&s_stAiotOps);
}
#define CHECK_SDK_LOGTIMES (10 * AIOT_THREAD_SECOND_TIME)    // 10s
#define CHECK_CLOUD_USERTIMES (10 * AIOT_THREAD_SECOND_TIME) // 10s

#define ANJ_PARTNER_REPORT_FILE_NAME DATA_BLOCK_MOUNT_PATH "/partner_report.xml"

typedef struct
{
    char szPartner[128];
    char szIccid[128];
    char szMsisdn[128];
    char szTimestamp[128];
    char szPk[128];
    char szDn[128];
    char szUuid[128];
    char szSn[128];
    struct list_head list;
} DevReportStruct;

typedef struct
{
    int bDevReported;
    char DevReportedPartner[128];
    char DevReportedIccId[128];
    anj_thread_s stReportThread;
} DevReportInfo;

static anj_thread_s s_stAiotThread = {0};
static DevReportInfo s_stDevReportInfo = {0};
static int s_stCheckReport = 0;

static void debuglog(const char *fmt, ...)
{
    char content_buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, 1024, fmt, ap);
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
        memset(timestr, 0, 32);
    }

    // __INFO("%s %s\n", timestr, content_buf);
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

    //	{"dn":"antest0001","ds":"654321"}
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

static void anj_aiot_tid_getok()
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

static void anj_aiot_tid_reget()
{
    anj_ser_info *pstSerInfo = getSerInfo();
    pstSerInfo->cloud_ready = 0;

    __ERR("get tid re get!!!\n");
    remove(P2P_ID_FILE_NAME);
}

static void anj_aiot_tid_settime(int gmt_seconds)
{
    return;
}

static int anj_aiot_dev_report_set(struct list_head *report_list)
{
    // 限制最多保留5个报告
    while (1)
    {
        int count = 0;
        struct list_head *pos;
        list_for_each(pos, report_list)
        {
            count++;
        }

        if (count <= 5)
            break;

        // 删除链表头部的节点（最旧的报告）
        if (!list_empty(report_list))
        {
            struct list_head *first = report_list->next;
            DevReportStruct *data = list_entry(first, DevReportStruct, list);
            list_del(first);
            anj_mw_free(data);
        }
    }

    int maxSize = 1024;
    char *pe;
    char *pb;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<REPORTROOT>\r\n");

    DevReportStruct *data;
    list_for_each_entry(data, report_list, list)
    {
        pb += snprintf(pb, pe - pb, "<REPORT ");
        pb += snprintf(pb, pe - pb, "partner=\"%s\" ", data->szPartner);
        pb += snprintf(pb, pe - pb, "iccid=\"%s\" ", data->szIccid);
        pb += snprintf(pb, pe - pb, "msiscn=\"%s\" ", data->szMsisdn);
        pb += snprintf(pb, pe - pb, "timestamp=\"%s\" ", data->szTimestamp);
        pb += snprintf(pb, pe - pb, "pk=\"%s\" ", data->szPk);
        pb += snprintf(pb, pe - pb, "dn=\"%s\" ", data->szDn);
        pb += snprintf(pb, pe - pb, "sn=\"%s\" ", data->szSn);
        pb += snprintf(pb, pe - pb, "uuid=\"%s\" ", data->szUuid);
        pb += snprintf(pb, pe - pb, "/>");
        pb += snprintf(pb, pe - pb, "\r\n");
    }

    pb += snprintf(pb, pe - pb, "</REPORTROOT>\r\n");
    *pb = '\0';

    anj_mw_write_file(ANJ_PARTNER_REPORT_FILE_NAME, 0, buf, strlen(buf));
    anj_mw_free(buf);

    return 0;
}

/*
ANJ_PARTNER_REPORT_FILE_NAME:

<REPORTROOT>
    <REPORT iccid="" msiscn="" timestamp="" p2pid="" sn="" uuid="" result="" />
</REPORTROOT>
*/
static void anj_aiot_dev_report_get(struct list_head *report_list)
{
    int iRet = 0;
    IXML_Document *pDoc = NULL;
    IXML_NodeList *pNodeRoot = NULL;

    if (anj_mw_file_exists(ANJ_PARTNER_REPORT_FILE_NAME) == 0)
        return;

    unsigned long long buflen = 0;
    anj_mw_read_file_len(ANJ_PARTNER_REPORT_FILE_NAME, &buflen);
    char *buffer = (char *)anj_mw_malloc(buflen);
    if (buffer == NULL)
        return;

    memset(buffer, 0, buflen);

    iRet = anj_mw_read_file_limit_len(ANJ_PARTNER_REPORT_FILE_NAME, buffer, buflen);
    if (iRet != 0)
    {
        goto endFunc;
    }

    pDoc = ixmlParseBuffer(buffer);
    if (pDoc == NULL)
    {
        __ERR("ixmlParseBuffer failed, xml=\n%s\n", buffer);
        goto endFunc;
    }

    pNodeRoot = ixmlDocument_getElementsByTagName(pDoc, "REPORTROOT");
    if (pNodeRoot != NULL)
    {
        IXML_Node *pNodeChild = NULL;
        pNodeChild = pNodeRoot->nodeItem->firstChild;
        while (pNodeChild != NULL)
        {
            if (0 == strcmp(pNodeChild->nodeName, "REPORT"))
            {
                DevReportStruct *data = (DevReportStruct *)anj_mw_malloc(sizeof(DevReportStruct));
                if (!data)
                {
                    goto endFunc;
                }
                memset(data, 0, sizeof(DevReportStruct));
                INIT_LIST_HEAD(&data->list);

                IXML_Node *tmpAttr;
                tmpAttr = pNodeChild->firstAttr; // 先比较UUID与SN
                while (tmpAttr)
                {
                    //				<REPORT iccid="" msiscn="" timestamp="" p2pid="" sn="" uuid="" result="" />
                    if (0 == strcmp(tmpAttr->nodeName, "partner"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szPartner, tmpAttr->nodeValue, sizeof(data->szPartner) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "iccid"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szIccid, tmpAttr->nodeValue, sizeof(data->szIccid) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "msiscn"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szMsisdn, tmpAttr->nodeValue, sizeof(data->szMsisdn) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "timestamp"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szTimestamp, tmpAttr->nodeValue, sizeof(data->szTimestamp) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "pk"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szPk, tmpAttr->nodeValue, sizeof(data->szPk) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "dn"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szDn, tmpAttr->nodeValue, sizeof(data->szDn) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "sn"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szSn, tmpAttr->nodeValue, sizeof(data->szSn) - 1);
                    }
                    else if (0 == strcmp(tmpAttr->nodeName, "uuid"))
                    {
                        if (tmpAttr->nodeValue != NULL)
                            strncpy(data->szUuid, tmpAttr->nodeValue, sizeof(data->szUuid) - 1);
                    }
                    tmpAttr = tmpAttr->nextSibling;
                }
                list_add_tail(&data->list, report_list);
            }

            pNodeChild = pNodeChild->nextSibling;
        }
    }
endFunc:
    if (buffer != NULL)
        anj_mw_free(buffer);
    if (NULL != pNodeRoot)
        ixmlNodeList_free(pNodeRoot);
    if (NULL != (pDoc))
        ixmlDocument_free(pDoc);
    return;
}

static int anj_aiot_check_dev_report()
{
    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    char szPartner[128] = {0};
    char szDatestr[128] = {0};
    char szMacaddr[128] = {0};
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
    if (event_result.result == NULL)
    {
        return 0;
    }
    anj_sysmng_partner_info_get(szPartner, szDatestr, szMacaddr);
    if (strlen(szPartner) == 0)
    {
        __ERR("Parter not ready. set to default: %s\n", ANJ_DEFAULT_PARTER);
        strcpy(szPartner, ANJ_DEFAULT_PARTER);
    }

    G4InfoStruct networkStatus = *(G4InfoStruct *)event_result.result;
    char szIccid[G4_STR_LEN_256] = {0};
    if (IPC_4G_SIMCARD_NUM > 1)
    {
        strcpy(szIccid, networkStatus.IccidList);
    }
    else
    {
        strcpy(szIccid, networkStatus.ICCID);
    }

    char szMsiscn[G4_STR_LEN_256] = {0};
    strcpy(szMsiscn, networkStatus.MSISDN);
    if (strlen(szIccid) == 0)
    {
        if (s_stDevReportInfo.bDevReported &&
            strcmp(s_stDevReportInfo.DevReportedPartner, szPartner) == 0) // 已经报过,不需要再上报
        {
            goto endFunc;
        }
    }
    else
    {
        if (strcmp(s_stDevReportInfo.DevReportedPartner, szPartner) == 0 &&
            strcmp(s_stDevReportInfo.DevReportedIccId, szIccid) == 0 &&
            s_stDevReportInfo.bDevReported)
        {
            goto endFunc;
        }
    }

    LIST_HEAD(report_list);
    anj_aiot_dev_report_get(&report_list);

    int bReported = 0;
    DevReportStruct *data;
    if (strlen(szIccid) == 0)
    {
        list_for_each_entry(data, &report_list, list)
        {
            if (strcmp(data->szPk, pstSerInfo->uid) == 0 &&
                strcmp(data->szDn, pstSerInfo->report_dn) == 0)
            {
                if (strcmp(data->szPartner, ANJ_DEFAULT_PARTER) == 0 &&
                    strcmp(szPartner, ANJ_DEFAULT_PARTER) != 0)
                {
                    __ERR("%s@%s already reported bind %s. Need to report\n",
                          pstSerInfo->uid, pstSerInfo->report_dn, data->szPartner);
                }
                else
                {
                    __ERR("%s@%s already reported bind %s, no need to report again.\n",
                          pstSerInfo->uid, pstSerInfo->report_dn, data->szPartner);
                    bReported = 1;
                    break;
                }
            }
            else
            {
                __ERR("%s@%s != %s@%s.\n", data->szPk, data->szDn, pstSerInfo->uid, pstSerInfo->report_dn);
            }
        }
    }
    else
    {
        list_for_each_entry(data, &report_list, list)
        {
            if (strcmp(data->szIccid, szIccid) == 0)
            {
                if (strcmp(data->szPartner, ANJ_DEFAULT_PARTER) == 0 &&
                    strcmp(szPartner, ANJ_DEFAULT_PARTER) != 0)
                {
                    __ERR("iccid %s %s@%s already reported bind %s. Need to report\n",
                          szIccid, pstSerInfo->uid, pstSerInfo->report_dn, data->szPartner);
                }
                else if (strcmp(data->szPk, pstSerInfo->uid) != 0 ||
                         strcmp(data->szDn, pstSerInfo->report_dn) != 0)
                {
                    __ERR("p2pid changed(new p2pid(%s:%s), old p2pid(%s:%s),  Need to report",
                          data->szPk, data->szDn, pstSerInfo->uid, pstSerInfo->report_dn);
                }
                else
                {
                    __ERR("iccid %s %s@%s already bind %s, no need to report again.\n",
                          szIccid, pstSerInfo->uid, pstSerInfo->report_dn, data->szPartner);
                    bReported = 1;
                    break;
                }
            }
        }
    }

    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &report_list)
    {
        DevReportStruct *data = list_entry(pos, DevReportStruct, list);
        list_del(pos);
        anj_mw_free(data);
    }

    if (bReported)
    {
        // 读出来已经上报过，需要设置成员变量，这样不用每次都进来读文件
        s_stDevReportInfo.bDevReported = 1;
        strcpy(s_stDevReportInfo.DevReportedIccId, szIccid);
        strcpy(s_stDevReportInfo.DevReportedPartner, szPartner);
        iRet = 0;
    }
    else
        iRet = 1;

endFunc:
    if (iRet == 0)
    {
        // iccid没cloud storage需要上报
        iRet = dev_bind_check_iccid_cs(szIccid) ? 0 : 1;
    }
    return iRet;
}

static int anj_aiot_dev_report_thread(void *ctx, int *bStart)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    DevInfo *pstDevInfo = getDevInfo();
    if (bStart && *bStart && anj_aiot_check_dev_report())
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
        if (event_result.result == NULL)
        {
            return 0;
        }
        G4InfoStruct networkStatus = *(G4InfoStruct *)event_result.result;
        if (strlen(pstSerInfo->uid) == 0 || strlen(pstSerInfo->report_dn) == 0)
        {
            __ERR("P2PID not ready.\n");
            return 0;
        }
        char szPartner[128] = {0};
        char szDatestr[128] = {0};
        char szMacaddr[128] = {0};
        anj_sysmng_partner_info_get(szPartner, szDatestr, szMacaddr);
        if (strlen(szPartner) == 0)
        {
            __ERR("Parter not ready. set to default: %s\n", ANJ_DEFAULT_PARTER);
            strcpy(szPartner, ANJ_DEFAULT_PARTER);
        }

        LIST_HEAD(report_list);
        anj_aiot_dev_report_get(&report_list);

        char szIccid[G4_STR_LEN_256] = {0};
        if (IPC_4G_SIMCARD_NUM > 1)
        {
            strcpy(szIccid, networkStatus.IccidList);
        }
        else
        {
            strcpy(szIccid, networkStatus.ICCID);
        }

        char szMsiscn[G4_STR_LEN_256] = {0};
        strcpy(szMsiscn, networkStatus.MSISDN);

        int ret = dev_bind_report(szPartner, pstSerInfo->uid, pstSerInfo->report_dn, szIccid, szMsiscn, NULL, NULL);
        if (ret != 0)
        {
            __ERR("device report failed.\n");
        }
        else
        {
            // 保存上报结果，避免再次上报
            s_stDevReportInfo.bDevReported = 1;
            strcpy(s_stDevReportInfo.DevReportedIccId, szIccid);
            DevReportStruct *data = (DevReportStruct *)anj_mw_malloc(sizeof(DevReportStruct));
            strcpy(data->szPartner, szPartner);
            strcpy(data->szIccid, szIccid);
            strcpy(data->szMsisdn, szMsiscn);
            anj_mw_time_getstr(data->szTimestamp, sizeof(data->szTimestamp), time(NULL));
            strcpy(data->szPk, pstSerInfo->uid);
            strcpy(data->szDn, pstSerInfo->report_dn);
            strcpy(data->szSn, pstDevInfo->sn);
            strcpy(data->szUuid, pstDevInfo->uuid);

            list_add_tail(&data->list, &report_list);
            anj_aiot_dev_report_set(&report_list);
        }
    }

    return 0;
}

static int anj_aiot_load_capability()
{
    int iRet = 0;
    DevInfo *pstDevInfo = getDevInfo();
    char *capability_str = anj_sysctl_get_capability_string();
    cJSON *rootObj = cJSON_CreateObject();
    cJSON_AddStringToObject(rootObj, "cap", capability_str);

    // 获取mcu固件字符串
    char mcu_str[128] = {0};
    if (anj_mw_file_exists("/tmp/mcu_ver.txt"))
    {
        anj_mw_read_file_limit_len("/tmp/mcu_ver.txt", mcu_str, sizeof(mcu_str));
    }
    cJSON_AddStringToObject(rootObj, "mcuver", mcu_str);

    // 获取设备版本字符串
    cJSON_AddStringToObject(rootObj, "fsver", pstDevInfo->stVersionInfo.fsVersion);

    char *jsonStr = cJSON_PrintUnformatted(rootObj);
    if (jsonStr != NULL)
    {
        __INFO("jsonStr:%s\n", jsonStr);
        gct_apiv4_device_ability_v2(jsonStr, strlen(jsonStr));
        anj_mw_free(jsonStr);
    }
    else
    {
        __ERR("cJSON_PrintUnformatted FAIL!\n");
        iRet = -1;
    }
    cJSON_Delete(rootObj);

    return iRet;
}

static void anj_aiot_cloud_osd_set(int bShow)
{
    osd_custom_content_s stCloudCustom = {0};
    stCloudCustom.custom_show = bShow;
    stCloudCustom.overlayText = OVERLAY_CLOUD_BMP;
    stCloudCustom.custom_x = 0;
    stCloudCustom.custom_y = 0;
    stCloudCustom.custom_location = POSITION_TYPE_BY_SCALE;
    anj_osd_cloud_set(&stCloudCustom);
}

static void anj_aiot_cloud_online_set(int State)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    int lastState = pstSerInfo->stP2pLoginState.logined;
    pstSerInfo->stP2pLoginState.logined = State;

    __INFO("%s:  %d\n", pstSerInfo->stP2pLoginState.devid, State);

    AjP2pSetLoginStatus(State);

    anj_net_status_e eNetStatus = anj_net_status_check();
    if (eNetStatus == ANJ_NET_STATUS_WIRE && State == 1 && lastState != State)
    {
        anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_CONNECTED_WIRE, 1);
    }
    if (CUSTOMER_WTD != ANJ_CUSTOMER_TYPE && lastState != State)
    {
        if (State)
        {
            anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_NETWORK_CONNECTED, 1);
        }
        else
        {
            anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_NETWORK_DISCONNECTED, 1);
        }
    }
    anj_aiot_cloud_osd_set(State);
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

    if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0)
    {
        __INFO("exist p2p conf already.\n");
        pstSerInfo->cloud_ready = 1;
    }

    AjP2pApiExit();

    AjP2pType type = AJ_P2P_TYPE_NOTDEFINED;

    // 调试不自动取ID时，将这里注释掉，手动上传ID
    type = AJ_P2P_TYPE_AIOT;

    __INFO("SN:%s dev:%s cus:%s ver:%s uuid:%s\n", pstDevInfo->sn, pstDevInfo->devType, pstDevInfo->custom_name, pstDevInfo->version_name, pstDevInfo->uuid);
    // szProductCode默认为空，取公版P2PID
    iRet = AjP2pApiInit(type, 1, 1, pstDevInfo->sn, pstDevInfo->devType,
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

    /* 等待TID获取成功 检测时长*/
    while ((wait_time--) > 0)
    {
        if (bStart == 0)
        {
            iRet = -1;
            goto endFunc;
        }

        if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0) // 不从云端拿ID的，可以手动上传
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

    AjP2pStopGetP2pIDFromCloud(); // 手动上传文件后，需要停止向云服务器请求ID
    anj_aiot_cloud_info_set(pstSerInfo->uid);
    anj_aiot_cloud_online_set(0);
    AjP2pSetP2pID(pstSerInfo->uid);

    __INFO("p2pIdInfoInit success\n");

endFunc:
    return iRet;
}

static void anj_aiot_sdklog_set()
{
    static unsigned long long lasttimestamp = 0;
    static char lastReadStr[512] = {0};
    unsigned long long interval = 10 * 1000; // 没有读到的话10秒一次，读到后改为1分钟检查一次

    unsigned long long nowtime = anj_mw_get_cputime_ms(NULL);
    if ((lasttimestamp == 0) || (nowtime < lasttimestamp) || ((nowtime - lasttimestamp) > interval))
    {
        lasttimestamp = nowtime;
        char readStr[512] = {0};
        const char *pFile = "/tmp/p2psdksetting.txt";
        if (anj_mw_file_exists(pFile))
        {
            interval = 60 * 1000;
            anj_mw_read_file_limit_len(pFile, readStr, sizeof(readStr));
            if (strcmp(readStr, lastReadStr) != 0)
            {
                __INFO("read: %s\n", readStr);
                strcpy(lastReadStr, readStr);
                gct_slog_proc_cmd(readStr);
                gct_apiv4_log_switch(1, 1);
            }
        }
        else
        {
            pFile = "/mnt/mmc0/p2psdksetting.txt";
            if (anj_mw_file_exists(pFile))
            {
                interval = 60000;
                anj_mw_read_file_limit_len(pFile, readStr, sizeof(readStr));
                if (strcmp(readStr, lastReadStr) != 0)
                {
                    __INFO("read: %s\n", readStr);
                    strcpy(lastReadStr, readStr);
                    gct_apiv4_log_switch(1, 1);
                    gct_slog_proc_cmd(readStr);
                }
            }
            else
            {
                interval = 10000;
                if (strlen(lastReadStr) > 0)
                {
                    __INFO("close log: %s\n", readStr);
                    lastReadStr[0] = 0;
                    gct_apiv4_log_switch(0, 0);
                    gct_slog_proc_cmd("");
                }
            }
        }
    }
}

static int anj_aiot_thread(void *ctx, int *bStart)
{
    // 等待网卡准备就绪
    sleep(3);

    DevInfo *pInfo = getDevInfo();
    while (bStart && *bStart && (pInfo->activated == 0))
    {
        sleep(1);
    }

    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    ANJ_CHK_FUNC(anj_aiot_info_init(*bStart), AJ_P2P_SUCC, "anj_aiot_info_init failed");

    anj_net_status_e netStatus = anj_net_status_check();

    // 初始化日志开关(尽量在初始化之前调用，不然会错过初始化的日志)
    gct_apiv4_log_savelocal(GCT_FALSE);
    gct_apiv4_log_savepath(LOCAL_PATH_LOG);

    if (netStatus != ANJ_NET_STATUS_WIRE)
    {
        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
        {
            gct_apiv4_set_aov_flag();
        }
    }

    if ((IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_4G) ||
        (IPC_NETWORK_TYPE == NET_DEV_TYPE_4G))
    {
        gct_apiv4_set_4g_flag();
    }
    // 注册必要的回调
    anj_aiot_callback_init();

    // 设置时区类型（要根据实际情况调整）
    gct_common_time_syn_type(GCT_TIME_SYN_TYPE_SYN_ZONE_RIGHT);

    gct_param gctparam = {0};
    strcpy(gctparam.szGid, pstSerInfo->uid);
    strcpy(gctparam.szGidPwd, pstSerInfo->secret);
    gctparam.nSupportModule = GCT_MODULE_IPC | GCT_MODULE_CLOUD | GCT_MODULE_DOORBELL;
    gctparam.nChannelCount = 1;
    gctparam.nVideoSessionLimit = 2 + ANJ_CAMERA_MAX_NUMS;
    gctparam.nReplaySessionLimit = ANJ_CAMERA_MAX_NUMS;
    gctparam.nStreamSupportType = STREAM_SUPPORT_TYPE_MAIN | STREAM_SUPPORT_TYPE_SUB;
    gctparam.nConnectionCount = gctparam.nVideoSessionLimit * 2; // 10//最大连接数
    strcpy(gctparam.szLocalCfgFilePath, LOCAL_CFG_PATH);
    strcpy(gctparam.szSdcardAbsPath, SDCARD_PATH);
    strcpy(gctparam.szAuthKey, LIB_AUTH_KEY);
    strcpy(gctparam.szComId, LIB_COMID);
    iRet = gct_apiv4_init(gctparam);
    if (iRet < 0)
    {
        gct_common_printf_error("gct_init fail,iRet = %d\n", iRet);
        iRet = -1;
        goto endFunc;
    }
    // 设置网卡名称
    gct_apiv4_set_ifname(anj_net_wan_ifname());

    // gct_apiv4_firmware_update_set_savepath("/tmp", "FIRM_UPLOAD_FILENAME", GCT_FALSE, gctparam.szGid);
    anj_aiot_load_capability();
    anj_aiot_stream_pb_init();

    anj_aiot_report_init();
    anj_aiot_cmd_init();

    // gct_apiv4_log_switch(1, 1);
    gct_apiv4_log_switch(0, 0);

    GCT_STATE_TO_SERVER LastConnectStatus = GCT_STATE_TO_SERVER_NO_START;
    int iCheckSdkLog = 0;
    int iCheckCloudUser = 0;
    char szWanIf[32] = {0};
    strncpy(szWanIf, anj_net_wan_ifname(), sizeof(szWanIf) - 1);
    while (bStart && *bStart)
    {
        usleep(AIOT_THREAD_SLEEP_TIME_MS);
        iCheckSdkLog++;
        iCheckCloudUser++;
        s_stCheckReport++;

        const char *ifname = anj_net_wan_ifname();
        if (ifname != NULL && strcmp(szWanIf, ifname) != 0)
        {
            __INFO("wan ifname %s -> %s\n", szWanIf, ifname);
            strncpy(szWanIf, ifname, sizeof(szWanIf) - 1);
            szWanIf[sizeof(szWanIf) - 1] = '\0';
            gct_apiv4_set_ifname(szWanIf);
        }

        GCT_STATE_TO_SERVER ConnectStatus = gct_apiv4_get_state_to_server();
        // aov的4G设备 是在4G模块连接p2p
        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV && pstSerInfo->stP2pLoginState.logined)
        {
            ConnectStatus = GCT_STATE_TO_SERVER_REG_SUCESS;
        }

        if (ConnectStatus != LastConnectStatus)
        {
            __INFO("ConnectStatus:%d\n", ConnectStatus);
            if (ConnectStatus == GCT_STATE_TO_SERVER_REG_SUCESS)
            {
                // login ok
                __DBG("cloud %s online!\n", pstSerInfo->stP2pLoginState.devid);

                if (anj_mw_file_exists(P2P_RESET_FLAG))
                {
                    // IOTBindConfig *pstBindInfo = getBindInfo();
                    // if (pstBindInfo->bindType != BIND_TYPE_ZXING)
                    {
                        anj_aiot_unbind();
                    }
                    remove(P2P_RESET_FLAG);
                }

                anj_aiot_cloud_online_set(1);
            }
            else
            {
                if (GCT_STATE_TO_SERVER_REG_SUCESS == LastConnectStatus)
                {
                    __DBG("cloud %s offline, status=%d\n", pstSerInfo->stP2pLoginState.devid, ConnectStatus);
                    anj_aiot_cloud_online_set(0);
                }
            }

            LastConnectStatus = ConnectStatus;
        }

        if (s_stCheckReport > CHECK_DEV_REPOTY_TIMES)
        {
            s_stCheckReport = 0;
            if (s_stDevReportInfo.stReportThread.start != 0)
            {
                __ERR("Report thread is running\n");
            }
            else
            {
                if (0 == anj_aiot_check_dev_report())
                {
                    __ERR("Not need to execute device report\n");
                }
                else
                {
                    s_stDevReportInfo.stReportThread.bAutoDestroy = 1;
                    s_stDevReportInfo.stReportThread.iThreadjob.ctx = (void *)&s_stDevReportInfo.stReportThread;
                    s_stDevReportInfo.stReportThread.iThreadjob.func = anj_aiot_dev_report_thread;
                    anj_thread_task_create(&s_stDevReportInfo.stReportThread);
                }
            }
        }

        if (iCheckSdkLog > CHECK_SDK_LOGTIMES)
        {
            anj_aiot_sdklog_set();
            iCheckSdkLog = 0;
        }

        if (iCheckCloudUser > CHECK_CLOUD_USERTIMES)
        {
            char szBindUser[256] = {0};
            gct_apiv4_get_binduser(szBindUser);
            anj_ser_cloud_binduser_check(szBindUser);
            iCheckCloudUser = 0;
        }

        anj_aiot_cmd_data_init();
    }
endFunc:
    anj_aiot_report_uninit();
    anj_aiot_cmd_uninit();
    AjP2pApiExit();
    return iRet;
}

int anj_aiot_init()
{
    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();

    anj_sysctl_capability_add(FUNCTION_PROPERTIES_CLOUD);

    // 先读取一次P2PID，用于没有WIFI模块的情况下先能搜到P2PID
    if (anj_aiot_tid_ready(pstSerInfo->uid, pstSerInfo->secret) == 0)
    {
        anj_aiot_cloud_info_set(pstSerInfo->uid);
    }
    s_stAiotThread.bAutoDestroy = 1;
    strncpy(s_stAiotThread.iThreadName, "aiot_thread", sizeof(s_stAiotThread.iThreadName) - 1);
    s_stAiotThread.iThreadjob.ctx = &s_stAiotThread;
    s_stAiotThread.iThreadjob.func = anj_aiot_thread;
    iRet = anj_thread_task_create(&s_stAiotThread);

    return iRet;
}

void anj_aiot_uninit()
{
    anj_thread_task_destroy(&s_stDevReportInfo.stReportThread, -1);
    anj_thread_task_destroy(&s_stAiotThread, -1);
    // gct_apiv4_release();
    __INFO("anj_aiot_uninit Success !!!\n");
}

void anj_aiot_media_info_get(int iIndex, gct_video_data_format *pstVideoFormat, gct_audio_data_format *pstAudioFormat)
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    VideoEncodeCfg *pstVideoEncodeCfg = &pstMediaConfig->videoConfig[0].videoEncode.encodeCfg[iIndex];
    AudioEncode *pstAudioEncode = &pstMediaConfig->audioConfig.audioEncode;

    int width = 0, height = 0;
    ANJ_SIZE_S picSize = getPicSize(pstVideoEncodeCfg->resolution.name,
                                    pstVideoCapture->tvsystem,
                                    pstVideoCapture->rotate,
                                    0);
    width = picSize.u32Width;
    height = picSize.u32Height;

    media_codec_type_e video_type = video_encode_type_get(pstVideoEncodeCfg->encodeFormat.name);

    pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_H264;
    if (video_type == MEDIA_CODEC_VIDEO_H265)
    {
        pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_H265;
    }
    else if (video_type == MEDIA_CODEC_VIDEO_H264)
    {
        pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_H264;
    }
    else if (video_type == MEDIA_CODEC_VIDEO_MJPG)
    {
        pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_MJPEG;
    }

    pstVideoFormat->reserve = 0;
    pstVideoFormat->width = width;
    pstVideoFormat->height = height;
    pstVideoFormat->bitrate = pstVideoEncodeCfg->bitRate;
    pstVideoFormat->framerate = pstVideoEncodeCfg->frameRate;
    pstVideoFormat->frameInterval = pstVideoEncodeCfg->initQuant;

    media_codec_type_e audio_type = audio_encode_type_get(pstAudioEncode->audioEncodeType.typeName);

    pstAudioFormat->reserve = 0;
    pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_G711U;
    pstAudioFormat->bitrate = pstAudioEncode->bitRate / 1000; // 64;
    pstAudioFormat->bitsPerSample = 16;
    pstAudioFormat->channelNumber = 1;                        //(audio_type == MEDIA_CODEC_AUDIO_AAC) ? 2 : 1;
    pstAudioFormat->samplesRate = pstAudioEncode->sampleRate; // 8000;

    if (audio_type == MEDIA_CODEC_AUDIO_AAC)
    {
        pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_AAC; // g711a: 7A19 g711u: 7A25
    }
    else if (audio_type == MEDIA_CODEC_AUDIO_G711U)
    {
        pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_G711U; // g711a: 7A19 g711u: 7A25
    }
    else if (audio_type == MEDIA_CODEC_AUDIO_G711A)
    {
        pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_G711A; // g711a: 7A19 g711u: 7A25
    }

    __INFO("video params codec:%#x %uX%u bps:%u fps:%d frameInterval:%d.\n",
           pstVideoFormat->euGCT_VIDEO_CODEC_TYPE,
           pstVideoFormat->width,
           pstVideoFormat->height,
           pstVideoFormat->bitrate,
           pstVideoFormat->framerate,
           pstVideoFormat->frameInterval);
    __INFO("audio params codec:%#x bps:%u samplesRate:%d channelNumber:%u. bitsPerSample:%u\n",
           pstAudioFormat->euGCT_AUDIO_CODEC_TYPE,
           pstAudioFormat->bitrate,
           pstAudioFormat->samplesRate,
           pstAudioFormat->channelNumber,
           pstAudioFormat->bitsPerSample);
}

int anj_aiot_bind(const char *product_key, const char *device_name, const char *accountName, const char *clientCode)
{
    int iRet = 0;
    iRet = dev_bind_task(product_key, device_name, accountName, clientCode, NULL, NULL);
    if (iRet == 0)
    {
        anj_aiot_cmd_location_on_bind(accountName);
    }
    return iRet;
}

void anj_aiot_unbind()
{
    anj_aiot_cmd_location_on_unbind();
    __DBG("gct_apiv4_unbind_device start\n");
    gct_apiv4_unbind_device();
    gct_apiv4_reset_device();
    __DBG("gct_apiv4_unbind_device end\n");
    remove(P2P_DEVICEBIND_FLAG);
}

void anj_aiot_reponse(int func, void *data)
{
    switch (func)
    {
    case SER_RESPONSE_SDCARD:
        anj_aiot_cmd_sd_format_reponse();
        break;
    case SER_RESPONSE_ABILITY:
        anj_aiot_load_capability();
        break;
    case SER_RESPONSE_PTZ_PRESET:
        anj_aiot_cmd_ptz_preset_reponse();
        break;
    case SER_RESPONSE_PTZ_ADVANCE_STATE:
        anj_aiot_cmd_ptz_advance_state_reponse(data);
        break;
    case SER_RESPONSE_4G_INIT_DONE:
        s_stCheckReport = CHECK_DEV_REPOTY_TIMES;
        break;

    default:
        break;
    }
}

void anj_aiot_reset_conn()
{
    gct_apiv4_server_reset();
}

void anj_aiot_push_video(int camera_type, int streamtype, int iskey,
                         unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs)
{
    gct_apiv4_stream_push_video_stream(camera_type, streamtype, iskey, frameBuf, frameLen, frameTimeMs);
}

void anj_aiot_push_audio(int camera_type, unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs)
{
    gct_apiv4_stream_push_audio_stream(camera_type, frameBuf, frameLen, frameTimeMs);
}

void anj_aiot_manual_unbind()
{
    gct_apiv4_unbind_device();
}

void anj_aiot_remove_unbind_device()
{
    gct_apiv4_remove_unbind_device();
}

int anj_aiot_bind_status_get()
{
    int bind_status = -1;
    bind_status = gct_apiv4_get_bind_state();
    return bind_status;
}
