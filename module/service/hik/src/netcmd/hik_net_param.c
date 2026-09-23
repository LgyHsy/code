#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "anj_config.h"
#include "anj_config_media.h"
#include "anj_config_network.h"
#include "anj_config_system.h"
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "project_option.h"

#include "hik_net_cmd.h"
#include "hik_net_media_map.h"
#include "hik_net_param.h"

#define HIK_BASE_WIDTH 704
#define HIK_BASE_HEIGHT 576

static UINT32 hik_inet_addr_host(const char *ip)
{
    struct in_addr addr;

    if (ip == NULL || ip[0] == '\0')
    {
        return 0;
    }
    if (inet_aton(ip, &addr) == 0)
    {
        return 0;
    }
    return ntohl(addr.s_addr);
}

static void hik_fill_pic_common(NETPARAM_PIC_CFG *pic, MediaConfig *media)
{
    VideoConfig *pVideo = NULL;
    int show = 1;

    memset(pic, 0, sizeof(*pic));
    pic->length = htonl(sizeof(NETPARAM_PIC_CFG));
    strncpy((char *)pic->chanName, "Camera 01", NAME_LEN - 1);
    pic->videoFormat = htonl(1);
    pic->brightness = 128;
    pic->contrast = 128;
    pic->saturation = 128;
    pic->hue = 128;

    if (media == NULL)
    {
        pic->bShowChanName = htonl(1);
        pic->bShowOsd = htonl(1);
        return;
    }

    pVideo = &media->videoConfig[0];
    show = pVideo->overlay.enable ? 1 : 0;
    if (pVideo->overlay.titleOverlay.title_utf8[0] != '\0')
    {
        snprintf((char *)pic->chanName, NAME_LEN, "%.*s", NAME_LEN - 1,
                 pVideo->overlay.titleOverlay.title_utf8);
    }
    pic->brightness = (UINT8)pVideo->videoCapture.brightness;
    pic->contrast = (UINT8)pVideo->videoCapture.contrast;
    pic->saturation = (UINT8)pVideo->videoCapture.saturation;
    pic->bShowChanName = htonl((UINT32)show);
    pic->bShowOsd = htonl((UINT32)show);
    if (pVideo->overlay.titleOverlay.posType == POSITION_TYPE_BY_SCALE)
    {
        pic->chanNameX = htons((UINT16)(pVideo->overlay.titleOverlay.posX * HIK_BASE_WIDTH / 100));
        pic->chanNameY = htons((UINT16)(pVideo->overlay.titleOverlay.posY * HIK_BASE_HEIGHT / 100));
    }
    else
    {
        pic->chanNameX = htons(pVideo->overlay.titleOverlay.posX == 0 ? 16 : (HIK_BASE_WIDTH - 16 - 100));
        pic->chanNameY = htons(pVideo->overlay.titleOverlay.posY == 0 ? 16 : (HIK_BASE_HEIGHT - 16));
    }
    if (pVideo->overlay.timeOverlay.posType == POSITION_TYPE_BY_SCALE)
    {
        pic->osdX = htons((UINT16)(pVideo->overlay.timeOverlay.posX * HIK_BASE_WIDTH / 100));
        pic->osdY = htons((UINT16)(pVideo->overlay.timeOverlay.posY * HIK_BASE_HEIGHT / 100));
    }
    else
    {
        pic->osdX = htons(pVideo->overlay.timeOverlay.posX == 0 ? 16 : (HIK_BASE_WIDTH - 16 - 200));
        pic->osdY = htons(pVideo->overlay.timeOverlay.posY == 0 ? 16 : (HIK_BASE_HEIGHT - 16));
    }
    pic->bDispWeek = (UINT8)pVideo->overlay.bDsplayWeek;
}

int hik_cmd_get_netcfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_NETWORK_CFG networkCfg;
    } resp;
    NetworkConfigNew *pNet = (NetworkConfigNew *)getNetWorkConfig();
    MediaStreamConfig *pStream = (MediaStreamConfig *)getMediaStreamConfig();
    char szIp[MAX_IP_NAME_LEN] = {0};
    char szMask[MAX_IP_NAME_LEN] = {0};
    char szGw[MAX_IP_NAME_LEN] = {0};
    char szMac[32] = {0};
    unsigned int mac[6] = {0};
    int i = 0;
    UINT16 cmd_port = 8000;
    UINT16 http_port = 80;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.networkCfg.length = htonl(sizeof(NETPARAM_NETWORK_CFG));

    if (pStream != NULL)
    {
        if (pStream->hikConfig.port > 0)
        {
            cmd_port = pStream->hikConfig.port;
        }
        if (pStream->webConfig.webPort > 0)
        {
            http_port = (UINT16)pStream->webConfig.webPort;
        }
    }

    if (pNet != NULL)
    {
        strncpy(szMac, (char *)pNet->lanCfg.MACAddress, sizeof(szMac) - 1);
        if (pNet->lanCfg.dhcpEnable)
        {
            struct NET_CONFIG netcfg;
            memset(&netcfg, 0, sizeof(netcfg));
            net_get_info(WIRE_INTERFACE_NAME, &netcfg);
            get_ip_str(netcfg.ifaddr, szIp, MAX_IP_NAME_LEN);
            get_ip_str(netcfg.netmask, szMask, MAX_IP_NAME_LEN);
            get_ip_str(netcfg.gateway, szGw, MAX_IP_NAME_LEN);
        }
        else
        {
            snprintf(szIp, sizeof(szIp), "%s", pNet->lanCfg.IPAddress);
            snprintf(szMask, sizeof(szMask), "%s", pNet->lanCfg.netMask);
            snprintf(szGw, sizeof(szGw), "%s", pNet->lanCfg.gateWay);
        }
    }

    resp.networkCfg.etherCfg[0].devIp = htonl(hik_inet_addr_host(szIp));
    resp.networkCfg.etherCfg[0].devIpMask = htonl(hik_inet_addr_host(szMask));
    resp.networkCfg.etherCfg[0].mediaType = htonl(4);
    resp.networkCfg.etherCfg[0].ipPortNo = htons(cmd_port);
    if (sscanf(szMac, "%02X:%02X:%02X:%02X:%02X:%02X", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4],
               &mac[5]) == 6)
    {
        for (i = 0; i < 6; i++)
        {
            resp.networkCfg.etherCfg[0].macAddr[i] = (UINT8)(mac[i] & 0xff);
        }
    }
    resp.networkCfg.httpPort = htons(http_port);
    resp.networkCfg.gatewayIp = htonl(hik_inet_addr_host(szGw));

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_NETCFG ip=%s mask=%s gw=%s http=%u cmd=%u\n", szIp, szMask, szGw, http_port, cmd_port);
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_netcfg(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        NETPARAM_NETWORK_CFG networkCfg;
    } req;
    NetworkConfigNew *pNet = (NetworkConfigNew *)getNetWorkConfig();
    MediaStreamConfig *pStream = (MediaStreamConfig *)getMediaStreamConfig();
    LANConfig lan;
    char ip_str[32] = {0};
    char mask_str[32] = {0};
    char gw_str[32] = {0};
    UINT16 http_port = 0;
    UINT32 media_type = 0;
    UINT32 cmd_port = 0;

    if (recvlen < (int)sizeof(req) || pNet == NULL)
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    if (ntohl(req.networkCfg.length) != sizeof(NETPARAM_NETWORK_CFG))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    media_type = ntohl(req.networkCfg.etherCfg[0].mediaType);
    cmd_port = (UINT32)(ntohs(req.networkCfg.etherCfg[0].ipPortNo) & 0xffff);
    http_port = ntohs(req.networkCfg.httpPort);
    if (media_type < 1 || media_type > 5 || cmd_port < 2000 || cmd_port > 65535)
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    if (pStream != NULL && http_port > 0 && pStream->webConfig.webPort != (int)http_port)
    {
        pStream->webConfig.webPort = (int)http_port;
    }

    memcpy(&lan, &pNet->lanCfg, sizeof(lan));
    lan.dhcpEnable = 0;
    get_ip_str(req.networkCfg.etherCfg[0].devIp, ip_str, sizeof(ip_str));
    get_ip_str(req.networkCfg.etherCfg[0].devIpMask, mask_str, sizeof(mask_str));
    get_ip_str(req.networkCfg.gatewayIp, gw_str, sizeof(gw_str));
    strncpy(lan.IPAddress, ip_str, sizeof(lan.IPAddress) - 1);
    strncpy(lan.netMask, mask_str, sizeof(lan.netMask) - 1);
    strncpy(lan.gateWay, gw_str, sizeof(lan.gateWay) - 1);

    if (anj_config_network_lan_set(&lan) != 0)
    {
        __WARN("hik SET_NETCFG lan_set failed\n");
    }
    else
    {
        __INFO("hik SET_NETCFG ip=%s mask=%s gw=%s http=%u\n", ip_str, mask_str, gw_str, http_port);
    }
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_netappcfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_NETAPP_CFG netAppCfg;
    } resp;
    char dns1[MAX_IP_NAME_LEN] = {0};
    char dns2[MAX_IP_NAME_LEN] = {0};

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.netAppCfg.length = htonl(sizeof(NETPARAM_NETAPP_CFG));
    net_get_two_dns(dns1, MAX_IP_NAME_LEN, dns2, MAX_IP_NAME_LEN);
    resp.netAppCfg.dnsIp = inet_addr(dns1);

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_NETAPPCFG dns=%s\n", dns1);
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_get_piccfg(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_PIC_CFG picCfg;
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();

    (void)recvbuf;
    (void)recvlen;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    hik_fill_pic_common(&resp.picCfg, media);
    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_PICCFG done\n");
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_piccfg_ex(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        UINT32 channel;
        NETPARAM_PIC_CFG_V14 picCfg;
    } req;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoOverlay overlay;
    VideoCaptureCfg capture;
    UINT32 channel = 1;
    int cam = 0;

    if (recvlen < (int)sizeof(req) || media == NULL)
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    channel = ntohl(req.channel);
    if (channel < 1 || channel > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        channel = 1;
    }
    cam = (int)channel - 1;

    if (ntohl(req.picCfg.length) != sizeof(NETPARAM_PIC_CFG_V14))
    {
        /* accept anyway for best-effort title/brightness apply */
        __WARN("hik SET_PICCFG_EX length mismatch %u vs %zu\n", ntohl(req.picCfg.length),
               sizeof(NETPARAM_PIC_CFG_V14));
    }

    memcpy(&overlay, &media->videoConfig[cam].overlay, sizeof(overlay));
    memcpy(&capture, &media->videoConfig[cam].videoCapture, sizeof(capture));

    if (req.picCfg.chanName[0] != '\0')
    {
        snprintf(overlay.titleOverlay.title_utf8, sizeof(overlay.titleOverlay.title_utf8), "%s",
                 (char *)req.picCfg.chanName);
    }
    overlay.enable = 1;
    overlay.bDsplayWeek = req.picCfg.bDispWeek;
    if (ntohl(req.picCfg.bShowChanName) == 0)
    {
        overlay.titleOverlay.posX = 2;
        overlay.titleOverlay.posY = 2;
    }
    else
    {
        overlay.titleOverlay.posType = POSITION_TYPE_BY_SCALE;
        overlay.titleOverlay.posX = ntohs(req.picCfg.chanNameX) * 100 / HIK_BASE_WIDTH;
        overlay.titleOverlay.posY = ntohs(req.picCfg.chanNameY) * 100 / HIK_BASE_HEIGHT;
    }
    if (ntohl(req.picCfg.bShowOsd) == 0)
    {
        overlay.timeOverlay.posX = 2;
        overlay.timeOverlay.posY = 2;
    }
    else
    {
        overlay.timeOverlay.posType = POSITION_TYPE_BY_SCALE;
        overlay.timeOverlay.posX = ntohs(req.picCfg.osdX) * 100 / HIK_BASE_WIDTH;
        overlay.timeOverlay.posY = ntohs(req.picCfg.osdY) * 100 / HIK_BASE_HEIGHT;
    }

    capture.brightness = req.picCfg.brightness;
    capture.contrast = req.picCfg.contrast;
    capture.saturation = req.picCfg.saturation;

    anj_config_overlay_set(&overlay, cam);
    if (capture.brightness != media->videoConfig[cam].videoCapture.brightness ||
        capture.contrast != media->videoConfig[cam].videoCapture.contrast ||
        capture.saturation != media->videoConfig[cam].videoCapture.saturation)
    {
        anj_config_video_capture_set(&capture, cam);
    }

    __INFO("hik SET_PICCFG_EX title=%s bright=%u\n", overlay.titleOverlay.title_utf8, capture.brightness);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_videoeffect(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_VIDEOPARA videoPara;
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    NETCMD_CHAN_HEADER chan_hdr;
    UINT32 channel = 1;

    memset(&resp, 0, sizeof(resp));
    if (recvlen >= (int)sizeof(chan_hdr))
    {
        memcpy(&chan_hdr, recvbuf, sizeof(chan_hdr));
        channel = ntohl(chan_hdr.channel);
    }
    if (channel < 1 || channel > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        channel = 1;
    }

    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.videoPara.brightness = 128;
    resp.videoPara.contrast = 128;
    resp.videoPara.saturation = 128;
    resp.videoPara.hue = 128;
    if (media != NULL)
    {
        resp.videoPara.brightness = (UINT8)media->videoConfig[channel - 1].videoCapture.brightness;
        resp.videoPara.contrast = (UINT8)media->videoConfig[channel - 1].videoCapture.contrast;
        resp.videoPara.saturation = (UINT8)media->videoConfig[channel - 1].videoCapture.saturation;
    }
    hik_fill_checksum(&resp, sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_videoeffect(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        UINT32 channel;
        NETPARAM_VIDEOPARA videoPara;
    } req;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg capture;
    UINT32 channel = 1;
    int cam = 0;

    if (recvlen < (int)sizeof(req) || media == NULL)
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    channel = ntohl(req.channel);
    if (channel < 1 || channel > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }
    cam = (int)channel - 1;
    memcpy(&capture, &media->videoConfig[cam].videoCapture, sizeof(capture));
    capture.brightness = req.videoPara.brightness;
    capture.contrast = req.videoPara.contrast;
    capture.saturation = req.videoPara.saturation;
    anj_config_video_capture_set(&capture, cam);
    __INFO("hik SET_VIDEOEFFECT %u %u %u\n", capture.brightness, capture.contrast, capture.saturation);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_alarmincfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_ALARMIN_CFG alarmInCfg;
    } resp;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_ALARMINCFG zeroed\n");
    return hik_writen(fd, &resp, sizeof(resp));
}

static int hik_apply_user_list(NETPARAM_USER *users, int is_ex, NETPARAM_USER_EX *users_ex)
{
    SystemConfig *pSys = (SystemConfig *)getSystemConfig();
    UserConfig userCfg;
    int i = 0;
    int count = 0;

    if (pSys == NULL)
    {
        return -1;
    }

    memset(&userCfg, 0, sizeof(userCfg));
    for (i = 0; i < MAX_USERNUM && count < MAX_ACCOUNT_COUNT; i++)
    {
        const char *uname = NULL;
        const char *upass = NULL;
        int j = 0;
        int invalid = 0;

        if (is_ex)
        {
            uname = (const char *)users_ex[i].username;
            upass = (const char *)users_ex[i].password;
        }
        else
        {
            uname = (const char *)users[i].username;
            upass = (const char *)users[i].password;
        }
        if (uname == NULL || uname[0] == '\0')
        {
            continue;
        }
        for (j = 0; uname[j] != '\0' && j < NAME_LEN; j++)
        {
            if (!isprint((unsigned char)uname[j]))
            {
                invalid = 1;
                break;
            }
        }
        if (invalid)
        {
            continue;
        }

        strncpy(userCfg.accounts[count].userName, uname, ACCOUNT_NAME_MAX_LEN - 1);
        strncpy(userCfg.accounts[count].password, upass ? upass : "", ACCOUNT_PASSWORD_MAX_LEN - 1);
        if (count == 0)
        {
            strncpy(userCfg.accounts[count].group.groupName, "Administrator", GROUP_NAME_MAX_LEN - 1);
        }
        else
        {
            strncpy(userCfg.accounts[count].group.groupName, "Operator", GROUP_NAME_MAX_LEN - 1);
        }
        strncpy(userCfg.accounts[count].status, "Enable", ACCOUNT_STATUS_MAX_LEN - 1);
        count++;
    }
    userCfg.count = count;
    return anj_config_system_user_set(&userCfg);
}

int hik_cmd_set_usercfg(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        NETPARAM_USER_CFG userCfg;
    } req;

    if (recvlen < (int)sizeof(req))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }
    memcpy(&req, recvbuf, sizeof(req));
    if (ntohl(req.userCfg.length) != sizeof(NETPARAM_USER_CFG))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }
    if (hik_apply_user_list(req.userCfg.user, 0, NULL) != 0)
    {
        __WARN("hik SET_USERCFG failed\n");
    }
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_usercfg_ex(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_USER_CFG_EX userCfg;
    } resp;
    SystemConfig *pSys = (SystemConfig *)getSystemConfig();
    int i = 0;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.userCfg.length = htonl(sizeof(NETPARAM_USER_CFG_EX));

    if (pSys != NULL)
    {
        for (i = 0; i < MAX_ACCOUNT_COUNT && i < pSys->userCfg.count && i < MAX_USERNUM; i++)
        {
            if (pSys->userCfg.accounts[i].userName[0] == '\0')
            {
                continue;
            }
            strncpy((char *)resp.userCfg.user[i].username, pSys->userCfg.accounts[i].userName, NAME_LEN - 1);
            strncpy((char *)resp.userCfg.user[i].password, pSys->userCfg.accounts[i].password, PASSWD_LEN - 1);
            resp.userCfg.user[i].permission = htonl(0xffffffffu);
            resp.userCfg.user[i].localPlayPermission = htonl(0xffffffffu);
            resp.userCfg.user[i].netPlayPermission = htonl(0xffffffffu);
            resp.userCfg.user[i].netPreviewPermission = htonl(0xffffffffu);
        }
    }

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_USERCFG_EX done\n");
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_usercfg_ex(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        NETPARAM_USER_CFG_EX userCfg;
    } req;

    if (recvlen < (int)sizeof(req))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }
    memcpy(&req, recvbuf, sizeof(req));
    if (ntohl(req.userCfg.length) != sizeof(NETPARAM_USER_CFG_EX))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }
    if (hik_apply_user_list(NULL, 1, req.userCfg.user) != 0)
    {
        __WARN("hik SET_USERCFG_EX failed\n");
    }
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_workstatus(int fd)
{
    struct
    {
        NETRET_HEADER header;
        DVR_WORKSTATUS workStatus;
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    int i = 0;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.workStatus.deviceStatus = htonl(0);
    resp.workStatus.localDispStatus = htonl(0);

    for (i = 0; i < MAX_CHANNUM; i++)
    {
        UINT32 br = 0;
        if (media != NULL)
        {
            br = (UINT32)(media->videoConfig[0].videoEncode.encodeCfg[0].bitRate * 1024);
        }
        resp.workStatus.chanStatus[i].bitRate = htonl(br);
        resp.workStatus.chanStatus[i].netLinks = htonl(1);
    }
    if (MAX_ALARMIN > 0)
    {
        resp.workStatus.alarmInStatus[0] = 1;
    }
    if (MAX_ALARMOUT > 0)
    {
        resp.workStatus.alarmOutStatus[0] = 1;
    }

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_WORKSTATUS done\n");
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_get_scalecfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        UINT32 bEnableScaler;
    } resp;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.bEnableScaler = htonl(0);
    hik_fill_checksum(&resp, sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_scalecfg(int fd, const char *recvbuf, int recvlen)
{
    (void)recvbuf;
    (void)recvlen;
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_get_ccdparamcfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_CCDPARA_CFG ccdPara;
        char reserve[HIK_CCD_GET_RESP_RESERVE];
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pCap = NULL;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.ccdPara.length = htonl(sizeof(NETPARAM_CCDPARA_CFG));

    if (media != NULL)
    {
        pCap = &media->videoConfig[0].videoCapture;
        resp.ccdPara.powerLineFrequencyMode = 0;
        resp.ccdPara.whiteBalanceMode = ((pCap->whitebalance >> 24) > 0) ? 1 : 0;
        resp.ccdPara.whiteBalanceModeBGain = (UINT8)((pCap->whitebalance >> 0) & 0xff);
        resp.ccdPara.whiteBalanceModeRGain = (UINT8)((pCap->whitebalance >> 16) & 0xff);
        resp.ccdPara.brightnessLevel = (UINT8)pCap->brightness;
        resp.ccdPara.contrastLevel = (UINT8)pCap->contrast;
        resp.ccdPara.saturationLevel = (UINT8)pCap->saturation;
        resp.ccdPara.sharpnessLevel = (UINT8)pCap->sharpness;
        resp.ccdPara.hueLevel = 128;
        resp.ccdPara.WDREnabled = pCap->wdr_mode > 0 ? 1 : 0;
        resp.ccdPara.WDRContrastLevel = (UINT8)(pCap->wdr_value * 100 / 255);
        resp.ccdPara.dayNightFilterType = 2;
        resp.ccdPara.Mirror = (UINT8)((pCap->hflip ? 1 : 0) | (pCap->vflip ? 2 : 0));
    }

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_CCDPARAMCFG sendlen=%zu\n", sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_set_ccdparamcfg(int fd, const char *recvbuf, int recvlen)
{
    (void)recvbuf;
    (void)recvlen;
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_set_compress_v30(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        UINT32 channel;
        NETPARAM_COMPRESS_CFG_V30 compressCfg;
    } req;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoEncode videoEnc;
    AudioEncode audioEnc;
    UINT32 channel = 1;
    int cam = 0;
    int i = 0;

    if (recvlen < (int)sizeof(req) || media == NULL)
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    channel = ntohl(req.channel);
    if (channel < 1 || channel > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }
    cam = (int)channel - 1;

    if (ntohl(req.compressCfg.length) != sizeof(NETPARAM_COMPRESS_CFG_V30))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&videoEnc, &media->videoConfig[cam].videoEncode, sizeof(videoEnc));
    memcpy(&audioEnc, &media->audioConfig.audioEncode, sizeof(audioEnc));

    for (i = 0; i < 2 && i < MAX_VENC_CHN; i++)
    {
        NETPARAM_COMP_PARA_V30 *pConfig =
            (i == 0) ? &req.compressCfg.struNormHighRecordPara : &req.compressCfg.struNetPara;
        UINT32 fps = hik_framerate_from_index(ntohl(pConfig->dwVideoFrameRate));
        UINT32 maxBitRate = ntohl(pConfig->dwVideoBitrate);
        UINT32 bitRate = 0;
        int width = 0;
        int height = 0;
        char res_name[64];

        if ((maxBitRate & 0x80000000u) != 0)
        {
            bitRate = (maxBitRate & 0x7fffffffu) / 1024;
        }
        else
        {
            bitRate = hik_bitrate_kbps_from_index(maxBitRate & 0x7fffffffu);
        }

        if (i == 0)
        {
            if (pConfig->byAudioEncType == HIK_AUDIOTYPE_G711U)
            {
                strncpy(audioEnc.audioEncodeType.typeName, "G.711", sizeof(audioEnc.audioEncodeType.typeName) - 1);
                audioEnc.sampleRate = 8000;
                audioEnc.bitRate = 64000;
            }
            else if (pConfig->byAudioEncType == HIK_AUDIOTYPE_G711A)
            {
                strncpy(audioEnc.audioEncodeType.typeName, "G.711A", sizeof(audioEnc.audioEncodeType.typeName) - 1);
                audioEnc.sampleRate = 8000;
                audioEnc.bitRate = 64000;
            }
        }

        if (pConfig->byVideoEncType == STD_H265)
        {
            if (strcmp(videoEnc.encodeCfg[i].encodeFormat.name, "H265+") == 0)
            {
                strncpy(videoEnc.encodeCfg[i].encodeFormat.name, "H265+",
                        sizeof(videoEnc.encodeCfg[i].encodeFormat.name) - 1);
            }
            else
            {
                strncpy(videoEnc.encodeCfg[i].encodeFormat.name, "H265",
                        sizeof(videoEnc.encodeCfg[i].encodeFormat.name) - 1);
            }
        }
        else
        {
            strncpy(videoEnc.encodeCfg[i].encodeFormat.name, "H264",
                    sizeof(videoEnc.encodeCfg[i].encodeFormat.name) - 1);
        }

        hik_wh_by_resolution(pConfig->byResolution, &width, &height);
        hik_resolution_name_by_wh(width, height, res_name, (int)sizeof(res_name));
        snprintf(videoEnc.encodeCfg[i].resolution.name,
                 sizeof(videoEnc.encodeCfg[i].resolution.name), "%.*s",
                 (int)sizeof(videoEnc.encodeCfg[i].resolution.name) - 1, res_name);

        if (fps > 0)
        {
            videoEnc.encodeCfg[i].display_frameRate = (int)fps;
            videoEnc.encodeCfg[i].frameRate = (int)fps;
        }
        videoEnc.encodeCfg[i].bitRate = (int)bitRate;
        if (pConfig->byBitrateType == CONSTANT_BITRATE)
        {
            strncpy(videoEnc.encodeCfg[i].bitRateControl.name, "CBR",
                    sizeof(videoEnc.encodeCfg[i].bitRateControl.name) - 1);
        }
        else
        {
            strncpy(videoEnc.encodeCfg[i].bitRateControl.name, "VBR",
                    sizeof(videoEnc.encodeCfg[i].bitRateControl.name) - 1);
        }
        videoEnc.encodeCfg[i].bitRateQuality = VIDEO_QUALITY_CUSTOM;
        videoEnc.encodeCfg[i].initQuant = ntohs(pConfig->wIntervalFrameI);

        __INFO("hik SET_COMPRESS[%d] res=%s bitrate=%u fps=%u venc=%u\n", i, res_name, bitRate, fps,
               pConfig->byVideoEncType);
    }

    anj_config_video_encode_set(&videoEnc, cam);
    anj_config_audio_encode_set(&audioEnc);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

static int hik_cmd_get_compress_aud_common(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_COMPRESS_CFG_AUDIO_V30 voicetalkparam;
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    if (media != NULL)
    {
        resp.voicetalkparam.byAudioEncType =
            hik_audio_enc_type(media->audioConfig.audioEncode.audioEncodeType.typeName);
    }
    hik_fill_checksum(&resp, sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

int hik_cmd_get_compress_aud(int fd)
{
    return hik_cmd_get_compress_aud_common(fd);
}

int hik_cmd_get_compress_aud_current(int fd)
{
    return hik_cmd_get_compress_aud_common(fd);
}

