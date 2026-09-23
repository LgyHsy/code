#include "anj_mw_comm.h"
#include "media_util.h"

media_codec_type_e audio_encode_type_get(char *codec)
{
    media_codec_type_e iRet = 0;
    ANJ_CHK((codec != NULL), MEDIA_CODEC_NONE, "input Invalid");
    if (strcasecmp(codec, "PCMA") == 0 ||
        strcasecmp(codec, "G711A") == 0 ||
        strcasecmp(codec, "G.711A") == 0)
    {
        iRet = MEDIA_CODEC_AUDIO_G711A;
    }
    else if (strcasecmp(codec, "PCMU") == 0 ||
             strcasecmp(codec, "G711") == 0 ||
             strcasecmp(codec, "G.711") == 0 ||
             strcasecmp(codec, "G711U") == 0 ||
             strcasecmp(codec, "G.711U") == 0 ||
             strcasecmp(codec, "G.722") == 0 ||
             strcasecmp(codec, "G722") == 0 ||
             strcasecmp(codec, "G.726") == 0 ||
             strcasecmp(codec, "G726") == 0)
    {
        iRet = MEDIA_CODEC_AUDIO_G711U;
    }
    else if (strcasecmp(codec, "PCM") == 0)
    {
        iRet = MEDIA_CODEC_AUDIO_PCM;
    }
    else if (strcasecmp(codec, "MPEG4-GENERIC") == 0 ||
             strcasecmp(codec, "AACG4-GENERIC") == 0 ||
             strcasecmp(codec, "AAC") == 0 ||
             strcasecmp(codec, "MP4A") == 0)
    {
        iRet = MEDIA_CODEC_AUDIO_AAC;
    }
    else
    {
        iRet = MEDIA_CODEC_NONE;
    }
endFunc:
    return iRet;
}

const char *audio_encode_type_str(media_codec_type_e audio_encode)
{
    switch (audio_encode)
    {
    case MEDIA_CODEC_AUDIO_G711A:
        return "G.711A";
        break;
    case MEDIA_CODEC_AUDIO_G711U:
        return "G.711U";
        break;
    case MEDIA_CODEC_AUDIO_PCM:
        return "PCM";
        break;
    case MEDIA_CODEC_AUDIO_AAC:
        return "AAC";
        break;
    case MEDIA_CODEC_AUDIO_MP3:
        return "MP3";
        break;
    default:
        return "unknown";
        break;
    }
}

int video_encode_type_get(char *codec)
{
    media_codec_type_e iRet = MEDIA_CODEC_NONE;
    ANJ_CHK((codec != NULL), MEDIA_CODEC_NONE, "input Invalid");

    if (strcasecmp(codec, "H264") == 0 || strcasecmp(codec, "H.264") == 0)
    {
        iRet = MEDIA_CODEC_VIDEO_H264;
    }
    else if (strcasecmp(codec, "H265") == 0 ||
             strcasecmp(codec, "H.265") == 0 ||
             strcasecmp(codec, "H265+") == 0)
    {
        iRet = MEDIA_CODEC_VIDEO_H265;
    }
    else if (strcasecmp(codec, "MJPEG") == 0)
    {
        iRet = MEDIA_CODEC_VIDEO_MJPG;
    }
    else
    {
        iRet = MEDIA_CODEC_NONE;
    }
endFunc:
    return iRet;
}

int stream_h264_pps_offset_get(unsigned char *buf, int len)
{
    if (len < 5)
    {
        __ERR("len[%d] is smaller than 5.\n", len);
        return -1;
    }

    int i = 0;
    while (i++ < (len - 5))
    {
        if ((buf[i] == 0) && (buf[i + 1] == 0) && (buf[i + 2] == 0) && (buf[i + 3] == 1))
        {
            /* nal type = PPS */
            if ((buf[i + 4] & 0x1F) == 0x08)
                return i;
        }

        if (i >= 1024)
        {
            break;
        }
    }

    return 0;
}

int stream_h265_pps_offset_get(unsigned char *buf, int len)
{
    if (len < 6)
    {
        __ERR("len[%d] is smaller than 5.\n", len);
        return -1;
    }

    int i = 0;
    while (i++ < (len - 5))
    {
        if ((buf[i] == 0) && (buf[i + 1] == 0) && (buf[i + 2] == 0) && (buf[i + 3] == 1))
        {
            if (((buf[i + 4] & 0x7E) >> 1) == 34)
            {
                return i;
            }
        }
        if (i >= 1024)
        {
            break;
        }
    }

    return 0;
}

int h264_get_pframe_offset(char *buf, int len, int iKeepStartCode)
{
    int iStartCodeLen = 0;

    if (iKeepStartCode)
    {
        iStartCodeLen = 0;
    }
    else
    {
        iStartCodeLen = 4;
    }

    return iStartCodeLen;
}

int h265_get_pframe_offset(char *buf, int len, int iKeepStartCode)
{
    int iStartCodeLen = 0;

    if (iKeepStartCode)
    {
        iStartCodeLen = 0;
    }
    else
    {
        iStartCodeLen = 4;
    }

    return iStartCodeLen;
}

// buf sps+pps+idr
// Frame format: 00 00 00 01 68 AA AA AA AA AA AA 00 00 00 01 67 BB BB BB BB BB BB
//               00 00 00 01 06 CC CC CC CC CC CC 00 00 00 01 65 DD DD DD DD DD DD
int h264_get_sps_pps_sei(char *pcBuf, int iLen,
                         int iXpsKeepStartCode, int iIFrameKeepStartCode,
                         char *pcSps, int *pnSpsLen,
                         char *pcPps, int *pnPpsLen,
                         char *pcSei, int *pnSeiLen)
{

    unsigned char *flag;
    int offset = 0;
    int spsOffset = 0;
    int spsLen = 0;
    int ppsOffset = 0;
    int ppsLen = 0;

#if SUPPORT_SEI_ENABLE
    int seiOffset = 0;
    int seiLen = 0;
#endif

    int iframeOffset = 0;
    int iXpsStartCodeLen = 0;
    int iIFrameStartCodeLen = 0;
    u_int8_t nal_unit_type = 0;

    if (iXpsKeepStartCode)
    {
        iXpsStartCodeLen = 0;
    }
    else
    {
        iXpsStartCodeLen = 4;
    }

    if (iIFrameKeepStartCode)
    {
        iIFrameStartCodeLen = 0;
    }
    else
    {
        iIFrameStartCodeLen = 4;
    }

    while (offset < (iLen - 5))
    {
        flag = (unsigned char *)(pcBuf + offset);

        if (flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 1)
        {
            nal_unit_type = (flag[4] & 0x1F);
            if (nal_unit_type == 0x07) // SPS
            {
                spsOffset = offset + iXpsStartCodeLen;
            }
            else if (nal_unit_type == 0x08) // PPS
            {
                ppsOffset = offset + iXpsStartCodeLen;
                spsLen = offset - spsOffset;
            }
#if SUPPORT_SEI_ENABLE
            else if (nal_unit_type == 0x06) // SEI
            {
                seiOffset = offset + iXpsStartCodeLen;
                ppsLen = offset - ppsOffset;
            }
            else if (nal_unit_type == 0x05) // nal_type == IDR
            {
                iframeOffset = offset + iIFrameStartCodeLen;
                seiLen = offset - seiOffset;
                break;
            }
#else
            else if (nal_unit_type == 0x05) // nal_type == IDR
            {
                iframeOffset = offset + iIFrameStartCodeLen;
                ppsLen = offset - ppsOffset;
                break;
            }
#endif
        }

        offset++;
    }

    if ((spsOffset != 0) && (spsLen != 0))
    {
        memcpy(pcSps, pcBuf + spsOffset, spsLen);
        *pnSpsLen = spsLen;
    }

    if ((ppsOffset != 0) && (ppsLen != 0))
    {
        memcpy(pcPps, pcBuf + ppsOffset, ppsLen);
        *pnPpsLen = ppsLen;
    }

#if SUPPORT_SEI_ENABLE
    if ((seiOffset != 0) && (seiLen != 0))
    {
        memcpy(pcSei, pcBuf + seiOffset, seiLen);
        *pnSeiLen = seiLen;
    }
#endif

    return iframeOffset;
}

// buf vps+sps+pps+SEI+vcl
// Frame format: 00 00 00 01 40 01 AA AA AA AA AA AA 00 00 00 01 42 01 BB BB BB BB BB BB
//               00 00 00 01 44 01 CC CC CC CC CC CC 00 00 00 01 4E 01 DD DD DD DD DD DD
//               00 00 00 01 26 01 EE EE EE EE EE EE
int h265_get_vps_sps_pps_sei(char *pcBuf, int iLen,
                             int iXpsKeepStartCode, int iIFrameKeepStartCode,
                             char *pcVps, int *pnVpsLen,
                             char *pcSps, int *pnSpsLen,
                             char *pcPps, int *pnPpsLen,
                             char *pcSei, int *pnSeiLen)
{
    unsigned char *flag;
    int offset = 0;
    int vpsOffset = 0;
    int vpsLen = 0;
    int spsOffset = 0;
    int spsLen = 0;
    int ppsOffset = 0;
    int ppsLen = 0;

#if SUPPORT_SEI_ENABLE
    int seiOffset = 0;
    int seiLen = 0;
#endif
    int iframeOffset = 0;
    int iXpsStartCodeLen = 0;
    int iIFrameStartCodeLen = 0;
    u_int8_t nal_unit_type = 0;

    if (iXpsKeepStartCode)
    {
        iXpsStartCodeLen = 0;
    }
    else
    {
        iXpsStartCodeLen = 4;
    }

    if (iIFrameKeepStartCode)
    {
        iIFrameStartCodeLen = 0;
    }
    else
    {
        iIFrameStartCodeLen = 4;
    }

    while (offset < (iLen - 6))
    {
        flag = (unsigned char *)(pcBuf + offset);

        if (flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 1)
        {
            nal_unit_type = ((flag[4] & 0x7E) >> 1);
            if (nal_unit_type == 32) // VPS
            {
                vpsOffset = offset + iXpsStartCodeLen;
            }
            else if (nal_unit_type == 33) // SPS
            {
                spsOffset = offset + iXpsStartCodeLen;
                vpsLen = offset - vpsOffset;
            }
            else if (nal_unit_type == 34) // PPS
            {
                ppsOffset = offset + iXpsStartCodeLen;
                spsLen = offset - spsOffset;
            }
#if SUPPORT_SEI_ENABLE
            else if (nal_unit_type == 39) // SEI
            {
                seiOffset = offset + iXpsStartCodeLen;
                ppsLen = offset - ppsOffset;
            }
            else if ((nal_unit_type == 19) || (nal_unit_type == 20)) // nal_type == IDR_W_RADL || IDR_N_LP
            {
                iframeOffset = offset + iIFrameStartCodeLen;
                seiLen = offset - seiOffset;
                break;
            }
#else
            else if ((nal_unit_type == 19) || (nal_unit_type == 20)) // nal_type == IDR_W_RADL || IDR_N_LP
            {
                iframeOffset = offset + iIFrameStartCodeLen;
                ppsLen = offset - ppsOffset;
                break;
            }
#endif
        }

        offset++;
    }

    if ((vpsOffset != 0) && (vpsLen != 0))
    {
        memcpy(pcVps, pcBuf + vpsOffset, vpsLen);
        *pnVpsLen = vpsLen;
    }

    if ((spsOffset != 0) && (spsLen != 0))
    {
        memcpy(pcSps, pcBuf + spsOffset, spsLen);
        *pnSpsLen = spsLen;
    }

    if ((ppsOffset != 0) && (ppsLen != 0))
    {
        memcpy(pcPps, pcBuf + ppsOffset, ppsLen);
        *pnPpsLen = ppsLen;
    }

#if SUPPORT_SEI_ENABLE
    if ((seiOffset != 0) && (seiLen != 0))
    {
        memcpy(pcSei, pcBuf + seiOffset, seiLen);
        *pnSeiLen = seiLen;
    }
#endif

    return iframeOffset;
}