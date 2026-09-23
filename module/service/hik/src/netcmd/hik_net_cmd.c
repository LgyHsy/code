#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "anj_config.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_sysmng.h"
#include "anj_systime.h"
#include "anj_video.h"
#include "project_option.h"

#include "hik_capability.h"
#include "hik_net_cmd.h"
#include "hik_net_ctrl.h"
#include "hik_net_media_map.h"
#include "hik_net_param.h"

UINT32 hik_check_byte_sum(const char *buf, int len)
{
    int i = 0;
    UINT32 sum = 0;

    if (buf == NULL || len <= 0)
    {
        return 0;
    }
    for (i = 0; i < len; i++)
    {
        sum += (UINT8)buf[i];
    }
    return sum;
}

int hik_writen(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    size_t left = len;

    while (left > 0)
    {
        ssize_t n = write(fd, p, left);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (n == 0)
        {
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

int hik_readn(int fd, void *buf, size_t len)
{
    char *p = (char *)buf;
    size_t left = len;

    while (left > 0)
    {
        ssize_t n = read(fd, p, left);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (n == 0)
        {
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

void hik_fill_checksum(void *pkt, size_t total_len)
{
    NETRET_HEADER *hdr = (NETRET_HEADER *)pkt;

    if (hdr == NULL || total_len < sizeof(NETRET_HEADER))
    {
        return;
    }
    hdr->checkSum = htonl(hik_check_byte_sum((char *)&hdr->retVal, (int)(total_len - 8)));
}

int hik_send_retval(int fd, UINT32 retVal)
{
    NETRET_HEADER ret;

    memset(&ret, 0, sizeof(ret));
    ret.length = htonl(sizeof(NETRET_HEADER));
    ret.retVal = htonl(retVal);
    hik_fill_checksum(&ret, sizeof(ret));
    return hik_writen(fd, &ret, sizeof(ret));
}

static int hik_cmd_get_rtspport(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_RTSP_CFG rtsp;
    } resp;
    MediaStreamConfig *pStream = (MediaStreamConfig *)getMediaStreamConfig();
    UINT16 port = 554;

    memset(&resp, 0, sizeof(resp));
    if (pStream != NULL && pStream->rtspConfig.videoPort > 0)
    {
        port = (UINT16)pStream->rtspConfig.videoPort;
    }

    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.rtsp.length = htonl(sizeof(NETPARAM_RTSP_CFG));
    resp.rtsp.RTSPPort = htons(port);
    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_RTSPPORT port=%u\n", port);
    return hik_writen(fd, &resp, sizeof(resp));
}

static void hik_fill_comp_para(NETPARAM_COMP_PARA_V30 *pConfig, VideoEncodeCfg *pEncode, VideoConfig *pVideo,
                               const char *audio_name)
{
    ANJ_SIZE_S pic;
    int fps = 0;

    memset(pConfig, 0, sizeof(*pConfig));
    pic = getPicSize(pEncode->resolution.name, pVideo->videoCapture.tvsystem, pVideo->videoCapture.rotate, 0);
    fps = pEncode->display_frameRate > 0 ? pEncode->display_frameRate : pEncode->frameRate;

    pConfig->byStreamType = 3;
    pConfig->byResolution = hik_resolution_by_wh((int)pic.u32Width, (int)pic.u32Height);
    pConfig->byBitrateType =
        (strcmp(pEncode->bitRateControl.name, "CBR") == 0) ? CONSTANT_BITRATE : VARIABLE_BITRATE;
    pConfig->byPicQuality = 0;
    pConfig->dwVideoBitrate = htonl(0x80000000u | (UINT32)(1024 * pEncode->bitRate));
    pConfig->dwVideoFrameRate = htonl(hik_framerate_index(fps));
    pConfig->wIntervalFrameI = htons((UINT16)pEncode->initQuant);
    pConfig->byIntervalBPFrame = 2;
    pConfig->byVideoEncType = hik_video_enc_type(pEncode->encodeFormat.name);
    pConfig->byAudioEncType = hik_audio_enc_type(audio_name);
}

static int hik_cmd_get_compress_v30(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_COMPRESS_CFG_V30 compressCfg;
    } resp;
    NETCMD_CHAN_HEADER chan_hdr;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoConfig *pVideo = NULL;
    const char *audio_name = NULL;
    UINT32 channel = 1;
    int i = 0;

    if (recvlen < (int)sizeof(NETCMD_CHAN_HEADER))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&chan_hdr, recvbuf, sizeof(chan_hdr));
    channel = ntohl(chan_hdr.channel);
    if (channel < 1 || channel > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        channel = 1;
    }

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.compressCfg.length = htonl(sizeof(NETPARAM_COMPRESS_CFG_V30));

    if (media != NULL)
    {
        pVideo = &media->videoConfig[channel - 1];
        audio_name = media->audioConfig.audioEncode.audioEncodeType.typeName;
        for (i = 0; i < 2 && i < MAX_VENC_CHN; i++)
        {
            NETPARAM_COMP_PARA_V30 *pConfig =
                (i == 0) ? &resp.compressCfg.struNormHighRecordPara : &resp.compressCfg.struNetPara;
            hik_fill_comp_para(pConfig, &pVideo->videoEncode.encodeCfg[i], pVideo, audio_name);
            __INFO("hik COMPRESS[%d] res=%u bitrate=0x%x fpsIdx=%u I=%u venc=%u aenc=%u\n", i,
                   pConfig->byResolution, ntohl(pConfig->dwVideoBitrate), ntohl(pConfig->dwVideoFrameRate),
                   ntohs(pConfig->wIntervalFrameI), pConfig->byVideoEncType, pConfig->byAudioEncType);
        }
    }

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_COMPRESSCFG_EX_V30 done sendlen=%zu chan=%u\n", sizeof(resp), channel);
    return hik_writen(fd, &resp, sizeof(resp));
}

static int hik_cmd_get_piccfg_ex(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_PIC_CFG_V14 picCfg;
    } resp;
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    int show = 1;

    (void)recvbuf;
    (void)recvlen;

    memset(&resp, 0, sizeof(resp));
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.picCfg.length = htonl(sizeof(NETPARAM_PIC_CFG_V14));
    strncpy((char *)resp.picCfg.chanName, "Camera 01", NAME_LEN - 1);
    resp.picCfg.videoFormat = htonl(1);
    resp.picCfg.brightness = 128;
    resp.picCfg.contrast = 128;
    resp.picCfg.saturation = 128;
    resp.picCfg.hue = 128;

    if (media != NULL)
    {
        show = media->videoConfig[0].overlay.enable ? 1 : 0;
        if (media->videoConfig[0].overlay.titleOverlay.title_utf8[0] != '\0')
        {
            snprintf((char *)resp.picCfg.chanName, NAME_LEN, "%.*s", NAME_LEN - 1,
                     media->videoConfig[0].overlay.titleOverlay.title_utf8);
        }
    }
    resp.picCfg.bShowChanName = htonl((UINT32)show);
    resp.picCfg.bShowOsd = htonl((UINT32)show);

    hik_fill_checksum(&resp, sizeof(resp));
    __DBG("hik GET_PICCFG_EX done sendlen=%zu\n", sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

static int hik_cmd_get_devicecfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_DEVICE_CFG deviceCfg;
    } resp;
    char sn[SERIALNO_LEN] = {0};

    memset(&resp, 0, sizeof(resp));
    anj_sysmng_load_sn(sn, sizeof(sn));

    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.deviceCfg.length = htonl(sizeof(NETPARAM_DEVICE_CFG));
    strncpy((char *)resp.deviceCfg.DVRName, "IPCamera", NAME_LEN - 1);
    memcpy(resp.deviceCfg.serialno, sn, SERIALNO_LEN);
    resp.deviceCfg.channelNums = (UINT8)ANJ_CAMERA_MAX_NUMS;
    resp.deviceCfg.firstChanNo = 1;
    resp.deviceCfg.devType = HIK_IPCAMERA_TYPE_BYTE;
    resp.deviceCfg.alarmInNums = 1;
    resp.deviceCfg.alarmOutNums = 1;
    resp.deviceCfg.netIfNums = 1;
    resp.deviceCfg.hdiskNums = 0;
    resp.deviceCfg.rs232Nums = 1;
    resp.deviceCfg.rs485Nums = 1;

    hik_fill_checksum(&resp, sizeof(resp));
    __INFO("hik GET_DEVICECFG done\n");
    return hik_writen(fd, &resp, sizeof(resp));
}

static int hik_cmd_set_timecfg(int fd, const char *recvbuf, int recvlen)
{
    struct
    {
        NETCMD_HEADER header;
        NETPARAM_TIME_CFG timeCfg;
    } req;
    struct tm tm_time;
    SystemConfig *pSys = (SystemConfig *)getSystemConfig();
    int tz = 0;

    if (recvlen < (int)sizeof(req))
    {
        return hik_send_retval(fd, NETRET_QUALIFIED);
    }

    memcpy(&req, recvbuf, sizeof(req));
    memset(&tm_time, 0, sizeof(tm_time));
    tm_time.tm_year = (int)ntohl(req.timeCfg.year) - 1900;
    tm_time.tm_mon = (int)ntohl(req.timeCfg.month) - 1;
    tm_time.tm_mday = (int)ntohl(req.timeCfg.day);
    tm_time.tm_hour = (int)ntohl(req.timeCfg.hour);
    tm_time.tm_min = (int)ntohl(req.timeCfg.min);
    tm_time.tm_sec = (int)ntohl(req.timeCfg.sec);

    if (pSys != NULL)
    {
        tz = pSys->timeCfg.timeZone;
    }

    if (anj_systime_set_time_and_zone(tm_time, tz, 1) != 0)
    {
        __WARN("hik SET_TIMECFG failed %04d-%02d-%02d %02d:%02d:%02d\n", tm_time.tm_year + 1900,
               tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    else
    {
        __INFO("hik SET_TIMECFG ok %04d-%02d-%02d %02d:%02d:%02d\n", tm_time.tm_year + 1900, tm_time.tm_mon + 1,
               tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

static int hik_cmd_get_timecfg(int fd)
{
    struct
    {
        NETRET_HEADER header;
        NETPARAM_TIME_CFG timeCfg;
    } resp;
    time_t now = time(NULL);
    struct tm t1;

    memset(&resp, 0, sizeof(resp));
    gmtime_r(&now, &t1);
    resp.header.length = htonl(sizeof(resp));
    resp.header.retVal = htonl(NETRET_QUALIFIED);
    resp.timeCfg.year = htonl((UINT32)(t1.tm_year + 1900));
    resp.timeCfg.month = htonl((UINT32)(t1.tm_mon + 1));
    resp.timeCfg.day = htonl((UINT32)t1.tm_mday);
    resp.timeCfg.hour = htonl((UINT32)t1.tm_hour);
    resp.timeCfg.min = htonl((UINT32)t1.tm_min);
    resp.timeCfg.sec = htonl((UINT32)t1.tm_sec);
    hik_fill_checksum(&resp, sizeof(resp));
    return hik_writen(fd, &resp, sizeof(resp));
}

static int hik_cmd_make_iframe(int fd, const char *recvbuf, int recvlen, int only_sub)
{
    NETCMD_CHAN_HEADER chan_hdr;
    UINT32 channel = 1;

    if (recvlen >= (int)sizeof(chan_hdr))
    {
        memcpy(&chan_hdr, recvbuf, sizeof(chan_hdr));
        channel = ntohl(chan_hdr.channel);
        (void)channel;
    }

    if (only_sub)
    {
        anj_video_request_idr(0, 1);
    }
    else
    {
        anj_video_request_idr(0, 0);
        anj_video_request_idr(0, 1);
    }
    __INFO("hik MAKEIFRAME only_sub=%d\n", only_sub);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

static int hik_cmd_get_capabilities(int fd, const char *recvbuf, int recvlen)
{
    UINT32 cap_type = 0;
    char xml[8192];
    int xml_len = 0;
    NETRET_HEADER header;

    if (recvlen >= (int)(sizeof(NETCMD_HEADER) + sizeof(UINT32)))
    {
        memcpy(&cap_type, recvbuf + sizeof(NETCMD_HEADER), sizeof(cap_type));
        cap_type = ntohl(cap_type);
    }

    xml_len = hik_capability_build_xml(cap_type, xml, (int)sizeof(xml));
    if (xml_len == -2)
    {
        __WARN("hik GET_CAPABILITES type=%u not support\n", cap_type);
        return hik_send_retval(fd, NETRET_NOT_SUPPORT);
    }
    if (xml_len <= 0)
    {
        __WARN("hik GET_CAPABILITES type=%u build failed\n", cap_type);
        return hik_send_retval(fd, NETRET_NOT_SUPPORT);
    }

    memset(&header, 0, sizeof(header));
    header.length = htonl((UINT32)(sizeof(NETRET_HEADER) + xml_len));
    header.retVal = htonl(NETRET_QUALIFIED);
    /* checksum covers retVal..end including xml */
    {
        UINT32 sum = hik_check_byte_sum((char *)&header.retVal, (int)sizeof(header) - 8);
        sum += hik_check_byte_sum(xml, xml_len);
        header.checkSum = htonl(sum);
    }

    __INFO("hik GET_CAPABILITES type=%u xml_len=%d\n", cap_type, xml_len);
    if (hik_writen(fd, &header, sizeof(header)) != 0)
    {
        return -1;
    }
    return hik_writen(fd, xml, (size_t)xml_len);
}

int hik_net_dispatch(int fd, UINT32 netCmd, const char *recvbuf, int recvlen)
{
    int ret = 0;

    __DBG("hik cmd 0x%x begin len=%d\n", netCmd, recvlen);

    switch (netCmd)
    {
    case NETCMD_LOGOUT:
    case NETCMD_USEREXCHANGE:
        ret = hik_send_retval(fd, NETRET_QUALIFIED);
        break;

    case NETCMD_ALARMCHAN:
        ret = hik_cmd_alarmchan(fd, recvbuf, recvlen);
        break;

    case NETCMD_STARTVOICECOM:
        ret = hik_cmd_start_voicecom(fd, recvbuf, recvlen);
        break;

    case NETCMD_PTZ:
        ret = hik_cmd_ptz(fd, recvbuf, recvlen);
        break;

    case DVR_PTZWITHSPEED:
        ret = hik_cmd_ptz_with_speed(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_JPEGPICTURE:
        ret = hik_cmd_get_jpeg(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_RTSPPORT:
        ret = hik_cmd_get_rtspport(fd);
        break;

    case NETCMD_GET_COMPRESSCFG_EX_V30:
        ret = hik_cmd_get_compress_v30(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_PICCFG_EX:
        ret = hik_cmd_get_piccfg_ex(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_DEVICECFG:
        ret = hik_cmd_get_devicecfg(fd);
        break;

    case NETCMD_SET_TIMECFG:
        ret = hik_cmd_set_timecfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_TIMECFG:
        ret = hik_cmd_get_timecfg(fd);
        break;

    case NETCMD_MAKEIFRAME:
        ret = hik_cmd_make_iframe(fd, recvbuf, recvlen, 0);
        break;

    case NETCMD_MAKEIFRAME2:
        ret = hik_cmd_make_iframe(fd, recvbuf, recvlen, 1);
        break;

    case NETCMD_GET_CAPABILITES:
        ret = hik_cmd_get_capabilities(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_NETCFG:
        ret = hik_cmd_get_netcfg(fd);
        break;

    case NETCMD_SET_NETCFG:
        ret = hik_cmd_set_netcfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_NETAPPCFG:
        ret = hik_cmd_get_netappcfg(fd);
        break;

    case NETCMD_GET_PICCFG:
        ret = hik_cmd_get_piccfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_SET_PICCFG_EX:
        ret = hik_cmd_set_piccfg_ex(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_VIDEOEFFECT:
        ret = hik_cmd_get_videoeffect(fd, recvbuf, recvlen);
        break;

    case NETCMD_SET_VIDEOEFFECT:
        ret = hik_cmd_set_videoeffect(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_ALARMINCFG:
        ret = hik_cmd_get_alarmincfg(fd);
        break;

    case NETCMD_SET_USERCFG:
        ret = hik_cmd_set_usercfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_USERCFG_EX:
        ret = hik_cmd_get_usercfg_ex(fd);
        break;

    case NETCMD_SET_USERCFG_EX:
        ret = hik_cmd_set_usercfg_ex(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_WORKSTATUS:
        ret = hik_cmd_get_workstatus(fd);
        break;

    case DVR_GET_SCALECFG:
        ret = hik_cmd_get_scalecfg(fd);
        break;

    case DVR_SET_SCALECFG:
        ret = hik_cmd_set_scalecfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_CCDPARAMCFG:
        ret = hik_cmd_get_ccdparamcfg(fd);
        break;

    case NETCMD_SET_CCDPARAMCFG:
        ret = hik_cmd_set_ccdparamcfg(fd, recvbuf, recvlen);
        break;

    case NETCMD_SET_COMPRESSCFG_EX_V30:
        ret = hik_cmd_set_compress_v30(fd, recvbuf, recvlen);
        break;

    case NETCMD_GET_COMPRESSCFG_AUD:
        ret = hik_cmd_get_compress_aud(fd);
        break;

    case NETCMD_GET_COMPRESSCFG_AUD_CURRENT:
        ret = hik_cmd_get_compress_aud_current(fd);
        break;

    case NETCMD_GET_NEW_CAPABILITES:
        __WARN("hik unsupported 0x%x\n", netCmd);
        ret = hik_send_retval(fd, NETRET_NOT_SUPPORT);
        break;

    default:
        __WARN("hik unsupported 0x%x\n", netCmd);
        ret = hik_send_retval(fd, NETRET_NOT_SUPPORT);
        break;
    }

    if (ret == 1)
    {
        return 1;
    }
    return (ret < 0) ? -1 : 0;
}
