#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_config.h"

#include "eventhub.h"

#define MAX_PORTS_NUM 20

static int check_port_valid(int port)
{
    if (port <= 0)
        return -1;
    if (port >= 65535)
        return -1;
    if (23 == port) // telnet
        return -1;
    if (3000 == port)
        return -1;

    return 0;
}

static int anj_config_stream_port_valid(int *portlist, int count)
{
    int iIndex = 0;
    for (iIndex = 0; iIndex < MAX_PORTS_NUM && iIndex < count; iIndex++)
    {
        if (check_port_valid(portlist[iIndex]) < 0)
        {
            __ERR("%d port %d error\n", iIndex, portlist[iIndex]);
            return -1;
        }
    }

    for (iIndex = 0; iIndex < MAX_PORTS_NUM - 1 && iIndex < count - 1; iIndex++)
    {
        int jIndex = iIndex + 1;
        for (; jIndex < MAX_PORTS_NUM && jIndex < count; jIndex++)
        {
            if (portlist[iIndex] == portlist[jIndex])
            {
                __ERR("%d and %d port %d is same\n", iIndex, jIndex, portlist[iIndex]);
                return -1;
            }
        }
    }

    return 0;
}

int anj_config_stream_default(MediaStreamConfig *pMediaStreamCfg)
{
    int iRet = 0;
    ANJ_CHK(pMediaStreamCfg != NULL, -1, "input Invalid");
    memset(pMediaStreamCfg, 0, sizeof(MediaStreamConfig));

    pMediaStreamCfg->rtspConfig.enable_rtsp = 1;
    pMediaStreamCfg->rtspConfig.rtsp_auth = 1;
    pMediaStreamCfg->rtspConfig.rtpoverrtsp = 1;
    pMediaStreamCfg->rtspConfig.videoPort = 554;

    pMediaStreamCfg->commConfig.enable = 1;
    pMediaStreamCfg->commConfig.ptzPort = 8091;

    pMediaStreamCfg->webConfig.enable_web = 1;
    pMediaStreamCfg->webConfig.enable_onvif = 1;
    pMediaStreamCfg->webConfig.onvif_auth = 0;
    pMediaStreamCfg->webConfig.webPort = 80;
    pMediaStreamCfg->webConfig.httpsPort = 443;
    pMediaStreamCfg->webConfig.h5Port = 12351;
    pMediaStreamCfg->webConfig.httpsCertificate[0] = 0;
    pMediaStreamCfg->webConfig.httpsKey[0] = 0;

    pMediaStreamCfg->hikConfig.enable = 1;
    pMediaStreamCfg->hikConfig.port = 8000;
    pMediaStreamCfg->hikConfig.auth = 0;

    pMediaStreamCfg->dhConfig.enable = 1;
    pMediaStreamCfg->dhConfig.port = 37777;
    pMediaStreamCfg->dhConfig.auth = 0;

    pMediaStreamCfg->tstConfig.enable = 0;
    pMediaStreamCfg->unvConfig.onvif_expand = 0;
    pMediaStreamCfg->unvConfig.smart_nvr = 0;
    pMediaStreamCfg->unvConfig.privatetype = 0;

    pMediaStreamCfg->rtmpConfig.enable = 0;
    pMediaStreamCfg->rtmpConfig.port = 1935;
    SetAllTimeSpan(&pMediaStreamCfg->rtmpConfig.timeSpan);

endFunc:
    return iRet;
}

int anj_config_stream_get(IXML_Node *pNode, MediaStreamConfig *pMediaStreamCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pMediaStreamCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpChild = pNode->firstChild;
    IXML_Node *tmpAttr = NULL;

    while (tmpChild != NULL)
    {
        if (!strcmp(tmpChild->nodeName, "StreamAccess"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr != NULL)
            {
                // rtsp --
                if (!strcmp(tmpAttr->nodeName, "Auth"))
                {
                    pMediaStreamCfg->rtspConfig.rtsp_auth = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "RTPOverRTSP"))
                {
                    pMediaStreamCfg->rtspConfig.rtpoverrtsp = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "VideoPort"))
                {
                    pMediaStreamCfg->rtspConfig.videoPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_rtsp"))
                {
                    pMediaStreamCfg->rtspConfig.enable_rtsp = Str2Num(tmpAttr->nodeValue);
                }
                // rtsp --

                // web --
                else if (!strcmp(tmpAttr->nodeName, "OnvifAuth"))
                {
                    pMediaStreamCfg->webConfig.onvif_auth = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "WEBPort"))
                {
                    pMediaStreamCfg->webConfig.webPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_onvif"))
                {
                    pMediaStreamCfg->webConfig.enable_onvif = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_web"))
                {
                    pMediaStreamCfg->webConfig.enable_web = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "httpsPort"))
                {
                    pMediaStreamCfg->webConfig.httpsPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "h5Port"))
                {
                    pMediaStreamCfg->webConfig.h5Port = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "httpsCertificate"))
                {
                    StrCpy(pMediaStreamCfg->webConfig.httpsCertificate, MAX_IPC_FILENAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "httpsKey"))
                {
                    StrCpy(pMediaStreamCfg->webConfig.httpsKey, MAX_IPC_FILENAME_LEN, tmpAttr->nodeValue);
                }

                // web --

                // comm --
                else if (!strcmp(tmpAttr->nodeName, "PTZPort"))
                {
                    pMediaStreamCfg->commConfig.ptzPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_comm"))
                {
                    pMediaStreamCfg->commConfig.enable = Str2Num(tmpAttr->nodeValue);
                }
                // comm --

                // hik --
                else if (!strcmp(tmpAttr->nodeName, "hik_port"))
                {
                    pMediaStreamCfg->hikConfig.port = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "hik_auth"))
                {
                    pMediaStreamCfg->hikConfig.auth = (short)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_hik"))
                {
                    pMediaStreamCfg->hikConfig.enable = Str2Num(tmpAttr->nodeValue);
                }
                // hik --

                // hik --
                else if (!strcmp(tmpAttr->nodeName, "dh_port"))
                {
                    pMediaStreamCfg->dhConfig.port = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "dh_auth"))
                {
                    pMediaStreamCfg->dhConfig.auth = (short)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "enable_dh"))
                {
                    pMediaStreamCfg->dhConfig.enable = Str2Num(tmpAttr->nodeValue);
                }
                // hik --

                else if (!strcmp(tmpAttr->nodeName, "enable_tst"))
                {
                    pMediaStreamCfg->tstConfig.enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "expand"))
                {
                    pMediaStreamCfg->unvConfig.onvif_expand = (char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "smart"))
                {
                    pMediaStreamCfg->unvConfig.smart_nvr = (char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "private"))
                {
                    pMediaStreamCfg->unvConfig.privatetype = (char)Str2Num(tmpAttr->nodeValue);
                }

                // rtmp --
                else if (!strcmp(tmpAttr->nodeName, "enable_rtmp"))
                {
                    pMediaStreamCfg->rtmpConfig.enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "rtmp_type"))
                {
                    pMediaStreamCfg->rtmpConfig.type = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "rtmp_port"))
                {
                    pMediaStreamCfg->rtmpConfig.port = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "streamno"))
                {
                    pMediaStreamCfg->rtmpConfig.streamno = (short)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "rtmp_server"))
                {
                    StrCpy(pMediaStreamCfg->rtmpConfig.server, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "rtmp_appname"))
                {
                    if (tmpAttr->nodeValue != NULL)
                    {
                        char *tmpDst = restore_with_escape(tmpAttr->nodeValue);
                        if (tmpDst != NULL)
                        {
                            StrCpy(pMediaStreamCfg->rtmpConfig.appname, MAX_RTMP_APP_NAME_LEN, tmpDst);
                            free(tmpDst);
                            tmpDst = NULL;
                        }
                        else
                        {
                            pMediaStreamCfg->rtmpConfig.appname[0] = '\0';
                        }
                    }
                }
                else if (!strcmp(tmpAttr->nodeName, "rtmp_streamid"))
                {
                    if (tmpAttr->nodeValue != NULL)
                    {
                        char *tmpDst = restore_with_escape(tmpAttr->nodeValue);
                        if (tmpDst != NULL)
                        {
                            StrCpy(pMediaStreamCfg->rtmpConfig.streamid, MAX_RTMP_STREAMID_LEN, tmpDst);
                            free(tmpDst);
                            tmpDst = NULL;
                        }
                        else
                        {
                            pMediaStreamCfg->rtmpConfig.streamid[0] = '\0';
                        }
                    }
                }
                // rtmp --
                else if (!strcmp(tmpAttr->nodeName, "Multicast0_enable"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[0].enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Multicast0_port"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[0].Port = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Multicast0_IP"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[0].Ip = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Multicast1_enable"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[1].enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Multicast1_port"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[1].Port = CheckAtoU(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Multicast1_IP"))
                {
                    pMediaStreamCfg->multicastConfig.StreamMulticast[1].Ip = CheckAtoU(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            IXML_Node *rtmpChild = tmpChild->firstChild;
            while (rtmpChild)
            {
                if (!strcmp(rtmpChild->nodeName, "RtmpCfg"))
                {
                    IXML_Node *timespanChild = rtmpChild->firstChild;
                    while (timespanChild)
                    {
                        if (!strcmp(timespanChild->nodeName, "TimeSpanCfg"))
                        {
                            anj_config_timespan_get(timespanChild, &pMediaStreamCfg->rtmpConfig.timeSpan);
                            break;
                        }
                        timespanChild = timespanChild->nextSibling;
                    }

                    break;
                }
                rtmpChild = rtmpChild->nextSibling;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

endFunc:
    return iRet;
}

char *anj_config_stream_conver_xml(MediaStreamConfig *pMediaStreamCfg)
{
    int initSize = 1024;
    int incrSize;
    char *tmp = NULL;
    char *pe;
    char *pb;
    char *pBuf;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    pb = pBuf;
    pe = pBuf + initSize - 1;

    char escapeBuf[1000] = {0};

    pb += snprintf(pb, pe - pb, "<MediaStreamConfig>\r\n");

    pb += snprintf(pb, pe - pb, "<StreamAccess ");
    pb += snprintf(pb, pe - pb, "Auth=\"%d\" ", pMediaStreamCfg->rtspConfig.rtsp_auth);
    pb += snprintf(pb, pe - pb, "VideoPort=\"%d\" ", pMediaStreamCfg->rtspConfig.videoPort);
    pb += snprintf(pb, pe - pb, "RTPOverRTSP=\"%d\" ", pMediaStreamCfg->rtspConfig.rtpoverrtsp);
    pb += snprintf(pb, pe - pb, "enable_rtsp=\"%d\" ", pMediaStreamCfg->rtspConfig.enable_rtsp);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_onvif=\"%d\" ", pMediaStreamCfg->webConfig.enable_onvif);
    pb += snprintf(pb, pe - pb, "enable_web=\"%d\" ", pMediaStreamCfg->webConfig.enable_web);
    pb += snprintf(pb, pe - pb, "OnvifAuth=\"%d\" ", pMediaStreamCfg->webConfig.onvif_auth);
    pb += snprintf(pb, pe - pb, "WEBPort=\"%d\" ", pMediaStreamCfg->webConfig.webPort);
    pb += snprintf(pb, pe - pb, "httpsPort=\"%d\" ", pMediaStreamCfg->webConfig.httpsPort);
    pb += snprintf(pb, pe - pb, "h5Port=\"%d\" ", pMediaStreamCfg->webConfig.h5Port);
    pb += snprintf(pb, pe - pb, "httpsCertificate=\"%s\" ", pMediaStreamCfg->webConfig.httpsCertificate);
    pb += snprintf(pb, pe - pb, "httpsKey=\"%s\" ", pMediaStreamCfg->webConfig.httpsKey);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_comm=\"%d\" ", pMediaStreamCfg->commConfig.enable);
    pb += snprintf(pb, pe - pb, "PTZPort=\"%d\" ", pMediaStreamCfg->commConfig.ptzPort);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_hik=\"%d\" ", pMediaStreamCfg->hikConfig.enable);
    pb += snprintf(pb, pe - pb, "hik_port=\"%u\" ", pMediaStreamCfg->hikConfig.port);
    pb += snprintf(pb, pe - pb, "hik_auth=\"%d\" ", pMediaStreamCfg->hikConfig.auth);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_dh=\"%d\" ", pMediaStreamCfg->dhConfig.enable);
    pb += snprintf(pb, pe - pb, "dh_port=\"%u\" ", pMediaStreamCfg->dhConfig.port);
    pb += snprintf(pb, pe - pb, "dh_auth=\"%d\" ", pMediaStreamCfg->dhConfig.auth);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_tst=\"%d\" ", pMediaStreamCfg->tstConfig.enable);
    pb += snprintf(pb, pe - pb, "\r\n");
    pb += snprintf(pb, pe - pb, "onvif_expand=\"%d\" ", pMediaStreamCfg->unvConfig.onvif_expand);
    pb += snprintf(pb, pe - pb, "smart_nvr=\"%d\" ", pMediaStreamCfg->unvConfig.smart_nvr);
    pb += snprintf(pb, pe - pb, "privatetype=\"%d\" ", pMediaStreamCfg->unvConfig.privatetype);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_rtmp=\"%d\" ", pMediaStreamCfg->rtmpConfig.enable);
    pb += snprintf(pb, pe - pb, "rtmp_port=\"%d\" ", pMediaStreamCfg->rtmpConfig.port);
    pb += snprintf(pb, pe - pb, "streamno=\"%d\" ", pMediaStreamCfg->rtmpConfig.streamno);
    pb += snprintf(pb, pe - pb, "rtmp_type=\"%d\" ", pMediaStreamCfg->rtmpConfig.type);
    pb += snprintf(pb, pe - pb, "rtmp_server=\"%s\" ", pMediaStreamCfg->rtmpConfig.server);
    pb += snprintf(pb, pe - pb, "rtmp_appname=\"%s\" ", copy_with_escape(escapeBuf, (char *)pMediaStreamCfg->rtmpConfig.appname));
    pb += snprintf(pb, pe - pb, "rtmp_streamid=\"%s\" ", copy_with_escape(escapeBuf, (char *)pMediaStreamCfg->rtmpConfig.streamid));
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "Multicast0_enable=\"%d\" ", pMediaStreamCfg->multicastConfig.StreamMulticast[0].enable);
    pb += snprintf(pb, pe - pb, "Multicast0_port=\"%u\" ", pMediaStreamCfg->multicastConfig.StreamMulticast[0].Port);
    pb += snprintf(pb, pe - pb, "Multicast0_IP=\"%u\" ", pMediaStreamCfg->multicastConfig.StreamMulticast[0].Ip);
    pb += snprintf(pb, pe - pb, "Multicast1_enable=\"%d\" ", pMediaStreamCfg->multicastConfig.StreamMulticast[1].enable);
    pb += snprintf(pb, pe - pb, "Multicast1_port=\"%u\" ", pMediaStreamCfg->multicastConfig.StreamMulticast[1].Port);
    pb += snprintf(pb, pe - pb, "Multicast1_IP=\"%u\" >", pMediaStreamCfg->multicastConfig.StreamMulticast[1].Ip);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "<RtmpCfg>\r\n");

    curPos = pb - pBuf;
    tmp = anj_config_timespan_conver_xml(&(pMediaStreamCfg->rtmpConfig.timeSpan));
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</RtmpCfg>\r\n");
    pb += snprintf(pb, pe - pb, "</StreamAccess>\r\n");
    pb += snprintf(pb, pe - pb, "</MediaStreamConfig>\r\n");

    return pBuf;
}

char *anj_config_stream_search_conver_xml(MediaStreamConfig *pMediaStreamCfg)
{
    int maxSize = 1024;
    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<MediaStreamConfig>\r\n");

    pb += snprintf(pb, pe - pb, "<StreamAccess ");
    pb += snprintf(pb, pe - pb, "Auth=\"%d\" ", pMediaStreamCfg->rtspConfig.rtsp_auth);
    pb += snprintf(pb, pe - pb, "VideoPort=\"%d\" ", pMediaStreamCfg->rtspConfig.videoPort);
    pb += snprintf(pb, pe - pb, "RTPOverRTSP=\"%d\" ", pMediaStreamCfg->rtspConfig.rtpoverrtsp);
    pb += snprintf(pb, pe - pb, "enable_rtsp=\"%d\" ", pMediaStreamCfg->rtspConfig.enable_rtsp);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_web=\"%d\" ", pMediaStreamCfg->webConfig.enable_web);
    pb += snprintf(pb, pe - pb, "WEBPort=\"%d\" ", pMediaStreamCfg->webConfig.webPort);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "enable_comm=\"%d\" ", pMediaStreamCfg->commConfig.enable);
    pb += snprintf(pb, pe - pb, "PTZPort=\"%d\" ", pMediaStreamCfg->commConfig.ptzPort);
    pb += snprintf(pb, pe - pb, "\r\n");

    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "</MediaStreamConfig>\r\n");

    return buf;
}

char *anj_config_stream_rtmp_conver_xml(RtmpConfig *pRtmpConfig)
{
    unsigned int maxSize = 1024;
	int rtmp_enable = 0;
	int rtmp_streamno = 0;
	int rtmp_port = 1935;
	int rtmp_type = 0;

	char *rtmp_server = "";
	char *rtmp_appname = "";
	char *rtmp_streamid="";

	if(pRtmpConfig != NULL)
	{
		rtmp_enable = pRtmpConfig->enable;
		rtmp_streamno = (int)pRtmpConfig->streamno;
		rtmp_port = (int)pRtmpConfig->port;
		rtmp_server = pRtmpConfig->server;
		rtmp_appname = pRtmpConfig->appname;
		rtmp_streamid = pRtmpConfig->streamid;
		rtmp_type = (int)pRtmpConfig->type;
	}

	char *xmlBuf = anj_mw_malloc(maxSize);
	memset(xmlBuf, 0, maxSize);

	snprintf(xmlBuf, maxSize, "<RtmpConfig Enable=\"%d\" Streamno=\"%d\" Port=\"%d\" Server=\"%s\" Appname=\"%s\" Streamid=\"%s\" Type=\"%d\" />",
		rtmp_enable, rtmp_streamno, rtmp_port, 
		rtmp_server, rtmp_appname, rtmp_streamid, rtmp_type);

	return xmlBuf;
}

int anj_config_stream_save(MediaStreamConfig *pMediaStreamCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_stream_conver_xml(pMediaStreamCfg);
    iRet = anj_config_save_node(pDataXml, "<MediaStreamConfig>", "</MediaStreamConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_stream_set(MediaStreamConfig *pstStreamConfig)
{
    int restart_rtsp = 0;
    int restart_h5 = 0;
    int restart_rtmp = 0;
    int restart_onvif = 0;
    int restart_web = 0;
    int restart_hik = 0;
    int restart_unv = 0;

    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();
    pthread_rwlock_wrlock(rwlock);
    MediaStreamConfig *streamConfig = (MediaStreamConfig *)getMediaStreamConfig();
    if (memcmp(streamConfig, pstStreamConfig, sizeof(MediaStreamConfig)))
    {
        __WARN("Change!!!\n");

        if (pstStreamConfig->rtspConfig.enable_rtsp != streamConfig->rtspConfig.enable_rtsp ||
            pstStreamConfig->rtspConfig.rtsp_auth != streamConfig->rtspConfig.rtsp_auth ||
            pstStreamConfig->rtspConfig.videoPort != streamConfig->rtspConfig.videoPort ||
            pstStreamConfig->rtspConfig.rtpoverrtsp != streamConfig->rtspConfig.rtpoverrtsp)
        {
            restart_rtsp = 1;
        }

        if (pstStreamConfig->webConfig.h5Port != streamConfig->webConfig.h5Port)
        {
            restart_h5 = 1;
        }

        if (memcmp(&pstStreamConfig->rtmpConfig, &streamConfig->rtmpConfig, sizeof(RtmpConfig)))
        {
            restart_rtmp = 1;
        }

        if (pstStreamConfig->webConfig.enable_onvif != streamConfig->webConfig.enable_onvif ||
            pstStreamConfig->webConfig.onvif_auth != streamConfig->webConfig.onvif_auth ||
            pstStreamConfig->webConfig.webPort != streamConfig->webConfig.webPort ||
            pstStreamConfig->webConfig.httpsPort != streamConfig->webConfig.httpsPort ||
            strcmp(pstStreamConfig->webConfig.httpsCertificate, streamConfig->webConfig.httpsCertificate) ||
            strcmp(pstStreamConfig->webConfig.httpsKey, streamConfig->webConfig.httpsKey) ||
            pstStreamConfig->unvConfig.onvif_expand != streamConfig->unvConfig.onvif_expand)
        {
            restart_onvif = 1;
        }

        if (pstStreamConfig->webConfig.enable_web != streamConfig->webConfig.enable_web ||
            pstStreamConfig->webConfig.webPort != streamConfig->webConfig.webPort ||
            pstStreamConfig->webConfig.httpsPort != streamConfig->webConfig.httpsPort ||
            strcmp(pstStreamConfig->webConfig.httpsCertificate, streamConfig->webConfig.httpsCertificate) ||
            strcmp(pstStreamConfig->webConfig.httpsKey, streamConfig->webConfig.httpsKey))
        {
            restart_web = 1;
        }

        if (memcmp(&pstStreamConfig->hikConfig, &streamConfig->hikConfig, sizeof(HikConfig)))
        {
            restart_hik = 1;
        }

        if (memcmp(&pstStreamConfig->unvConfig, &streamConfig->unvConfig, sizeof(UnvConfig)))
        {
            restart_unv = 1;
        }

        memcpy(streamConfig, pstStreamConfig, sizeof(MediaStreamConfig));
        anj_config_stream_save(pstStreamConfig);
    }
    pthread_rwlock_unlock(rwlock);

    if (restart_rtsp)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTSP_RESTART, &event_result, NULL);
    }

    if (restart_h5)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_H5_RESTART, &event_result, NULL);
    }

    if (restart_rtmp)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTMP_RESTART, &event_result, NULL);
    }

    if (restart_onvif)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_ONVIF_RESTART, &event_result, NULL);
    }

    if (restart_web)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WEB_RESTART, &event_result, NULL);
    }

    if (restart_hik)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_HIK_RESTART, &event_result, NULL);
    }

    if (restart_unv)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_UNV_RESTART, &event_result, NULL);
    }

    return 0;
}

int anj_config_stream_load(MediaStreamConfig *pstStreamConfig)
{
    return anj_config_load("MediaStreamConfig", pstStreamConfig, CONFIG_FILE_PATH);
}

int anj_config_stream_get_by_xml(MediaStreamConfig *pMediaStream, char *xmlBuf, int bHaveOldCfg)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MediaStreamConfig");
    if (pNodelist != NULL)
    {
        if (0 == bHaveOldCfg)
        {
            anj_config_stream_default(pMediaStream);
        }

        anj_config_stream_get(pNodelist->nodeItem, pMediaStream);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_stream_port_check(MediaStreamConfig *pCfg)
{
    int portscount = 0;
    int portlist[MAX_PORTS_NUM];
    memset(portlist, 0, sizeof(portlist));
    if (pCfg->rtspConfig.enable_rtsp > 0)
    {
        portlist[portscount++] = pCfg->rtspConfig.videoPort;
    }
    if (pCfg->commConfig.enable > 0)
    {
        portlist[portscount++] = pCfg->commConfig.ptzPort;
    }
    if (pCfg->webConfig.enable_web > 0 || pCfg->webConfig.enable_onvif)
    {
        portlist[portscount++] = pCfg->webConfig.webPort;
        portlist[portscount++] = pCfg->webConfig.httpsPort;
        portlist[portscount++] = pCfg->webConfig.h5Port;
    }
    if (pCfg->hikConfig.enable > 0)
    {
        portlist[portscount++] = pCfg->hikConfig.port;
        portlist[portscount++] = pCfg->hikConfig.port + 200;
    }
    if (pCfg->dhConfig.enable > 0)
    {
        portlist[portscount++] = pCfg->dhConfig.port;
    }

    return anj_config_stream_port_valid(portlist, portscount);
}
