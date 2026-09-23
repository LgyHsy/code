#include <stdio.h>
#include <string.h>

#include "ixml.h"
#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_net.h"
#include "anj_service_upgrade.h"

int anj_service_sysctl_basic_get_serialnumber(char **msg_body)
{
    unsigned char sn[256];
    char sn_str[64] = {0};

    if (anj_sysmng_get_sn(sn, sizeof(sn)) != 0)
    {
        return -1;
    }

    sprintf(sn_str, "%02X%02X%02X%02X%02X%02X%02X%02X",
            sn[0], sn[1], sn[2], sn[3], sn[4], sn[5], sn[6], sn[7]);
    *msg_body = anj_mw_malloc(256);
    if (*msg_body)
    {
        sprintf(*msg_body, "<RESPONSE_PARAM SerialNumber=\"%s\" />", sn_str);
    }
    return 0;
}

int anj_service_sysctl_basic_get_systemcontrolstring(char **msg_body)
{
    *msg_body = anj_mw_malloc(MAX_SYSTEM_CONTROL_STRING_LEN * 2);
    if (*msg_body)
    {
        sprintf(*msg_body, "<RESPONSE_PARAM SystemConfigString=\"%s\" />",
                anj_sysctl_get_capability_string());
    }
    return 0;
}

int anj_service_sysctl_basic_get_version_info(char **msg_body)
{
    DevInfo *pstDevInfo = getDevInfo();
    SYSTEM_VERSION_DATA *pstVersionInfo = &pstDevInfo->stVersionInfo;

    *msg_body = anj_mw_malloc(1024);
    if (*msg_body)
    {
        sprintf(*msg_body, "<RESPONSE_PARAM\nKernelVersion=\"%s\"\nFileSystemVersion=\"%s\"\n/>",
                pstVersionInfo->kernelVersion, pstVersionInfo->fsVersion);
    }
    return 0;
}

int anj_service_sysctl_basic_get_network_status(char **msg_body)
{
    NETWORK_STATUS_DATA networkStatus;

    if (anj_net_info_get(&networkStatus) != 0)
    {
        return -1;
    }

    *msg_body = anj_mw_malloc(2048);
    if (*msg_body)
    {
        sprintf(*msg_body,
                "<RESPONSE_PARAM>\n"
                "<WIRE_NETWORK\n"
                "MacAddress=\"%s\"\n"
                "IPType=\"%s\"\n"
                "IPAddress=\"%s\"\n"
                "Netmask=\"%s\"\n"
                "Gateway=\"%s\"\n"
                "Dns1=\"%s\"\n"
                "Dns2=\"%s\"\n"
                "/>\n"
                "</RESPONSE_PARAM>",
                networkStatus.wireMac,
                networkStatus.ipType,
                networkStatus.ip,
                networkStatus.netmask,
                networkStatus.gateway,
                networkStatus.dns1,
                networkStatus.dns2);
    }
    return 0;
}

int anj_service_sysctl_basic_get_media_capability(char **msg_body)
{
    char vidCap[4096] = {0};
    char audCap[256] = {0};
    char yuvCap[512] = {0};

    *msg_body = anj_mw_malloc(sizeof(vidCap) + sizeof(audCap) + sizeof(yuvCap) + 128);
    if (*msg_body)
    {
        anj_sysmng_videolist_get(vidCap, sizeof(vidCap));
        anj_sysmng_audiolist_get(audCap, sizeof(audCap));
        anj_sysmng_yuvlist_get(yuvCap, sizeof(yuvCap));
        sprintf(*msg_body,
                "<RESPONSE_PARAM\n><VideoCap CapList=\"%s\"/>\n<AudioCap CapList=\"%s\"/>\n<YuvCap CapList=\"%s\"/>\n</RESPONSE_PARAM>\n",
                vidCap, audCap, yuvCap);
    }
    return 0;
}

int anj_service_sysctl_basic_set_system_time(IXML_Document *pDoc)
{
    char *now_time = GetRequestParamValue(pDoc, (char *)"Time");
    char *time_zone = GetRequestParamValue(pDoc, (char *)"TimeZone");

    if ((now_time == NULL) || (time_zone == NULL))
    {
        if (now_time != NULL)
            anj_mw_free(now_time);
        if (time_zone != NULL)
            anj_mw_free(time_zone);
        return -1;
    }

    int tz = atoi(time_zone);
    struct tm time;
    int iRet = 0;

    if (GetTimeFromString(now_time, &time) < 0)
    {
        iRet = -2;
    }
    else
    {
        iRet = anj_systime_set_time_and_zone(time, tz, 1);
    }

    anj_mw_free(now_time);
    anj_mw_free(time_zone);
    return iRet;
}
