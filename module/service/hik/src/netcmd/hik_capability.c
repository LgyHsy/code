#include <stdio.h>
#include <string.h>

#include "anj_config.h"
#include "anj_mw_comm.h"
#include "project_option.h"

#include "hik_capability.h"
#include "hik_net_media_map.h"

static int hik_cap_fill_av_compress(char *buf, int buf_len, int add_xml_head)
{
    MediaConfig *media = (MediaConfig *)getMediaConfig();
    VideoEncodeCfg *main_enc = NULL;
    int len = 0;
    UINT8 main_res = 19;
    UINT8 main_enc_type = STD_H264;
    int w = 1280;
    int h = 720;

    if (buf == NULL || buf_len <= 0)
    {
        return -1;
    }

    if (media != NULL)
    {
        main_enc = &media->videoConfig[0].videoEncode.encodeCfg[0];
        ANJ_SIZE_S pic = getPicSize(main_enc->resolution.name, media->videoConfig[0].videoCapture.tvsystem,
                                    media->videoConfig[0].videoCapture.rotate, 0);
        w = pic.u32Width;
        h = pic.u32Height;
        main_res = hik_resolution_by_wh(w, h);
        main_enc_type = hik_video_enc_type(main_enc->encodeFormat.name);
    }

    if (add_xml_head)
    {
        len += snprintf(buf + len, buf_len - len, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    }

    len += snprintf(buf + len, buf_len - len,
                    "<AudioVideoCompressInfo version=\"1.0\">\n"
                    "<AudioCompressInfo>\n"
                    "<AudioEncodeType>\n"
                    "<Range>1,2</Range>\n"
                    "</AudioEncodeType>\n"
                    "<VoiceTalkEncodeType>\n"
                    "<Range>0</Range>\n"
                    "</VoiceTalkEncodeType>\n"
                    "</AudioCompressInfo>\n"
                    "<VideoCompressInfo>\n"
                    "<ChannelList>\n"
                    "<ChannelEntry>\n"
                    "<ChannelNumber>1</ChannelNumber>\n"
                    "<VideoEncodeType>\n"
                    "<Range>1,10</Range>\n"
                    "</VideoEncodeType>\n"
                    "<MainResolutionList>\n"
                    "<ResolutionEntry>\n"
                    "<Index>%u</Index>\n"
                    "<Name>%dx%d</Name>\n"
                    "</ResolutionEntry>\n"
                    "<ResolutionEntry>\n"
                    "<Index>27</Index>\n"
                    "<Name>1920x1080</Name>\n"
                    "</ResolutionEntry>\n"
                    "<ResolutionEntry>\n"
                    "<Index>19</Index>\n"
                    "<Name>1280x720</Name>\n"
                    "</ResolutionEntry>\n"
                    "<ResolutionEntry>\n"
                    "<Index>67</Index>\n"
                    "<Name>2592x1944</Name>\n"
                    "</ResolutionEntry>\n"
                    "</MainResolutionList>\n"
                    "<SubResolutionList>\n"
                    "<ResolutionEntry>\n"
                    "<Index>3</Index>\n"
                    "<Name>704x576</Name>\n"
                    "</ResolutionEntry>\n"
                    "<ResolutionEntry>\n"
                    "<Index>16</Index>\n"
                    "<Name>640x480</Name>\n"
                    "</ResolutionEntry>\n"
                    "<ResolutionEntry>\n"
                    "<Index>15</Index>\n"
                    "<Name>320x240</Name>\n"
                    "</ResolutionEntry>\n"
                    "</SubResolutionList>\n"
                    "<CurrentVideoEncodeType>%u</CurrentVideoEncodeType>\n"
                    "<CurrentMainResolution>%u</CurrentMainResolution>\n"
                    "</ChannelEntry>\n"
                    "</ChannelList>\n"
                    "</VideoCompressInfo>\n"
                    "</AudioVideoCompressInfo>\n",
                    main_res, w, h, main_enc_type, main_res);

    if (len <= 0 || len >= buf_len)
    {
        return -1;
    }
    return len;
}

static int hik_cap_fill_ccd(char *buf, int buf_len, int add_xml_head)
{
    int len = 0;

    if (buf == NULL || buf_len <= 0)
    {
        return -1;
    }
    if (add_xml_head)
    {
        len += snprintf(buf + len, buf_len - len, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    }
    len += snprintf(buf + len, buf_len - len,
                    "<CAMERAPARA version=\"1.0\">\n"
                    "<PowerLineFrequencyMode>\n"
                    "<Range>0,1</Range>\n"
                    "</PowerLineFrequencyMode>\n"
                    "<WhiteBalance>\n"
                    "<WhiteBalanceMode>\n"
                    "<Range>1,3</Range>\n"
                    "</WhiteBalanceMode>\n"
                    "</WhiteBalance>\n"
                    "</CAMERAPARA>\n");
    if (len <= 0 || len >= buf_len)
    {
        return -1;
    }
    return len;
}

static int hik_cap_fill_total(char *buf, int buf_len)
{
    int len = 0;
    int n = 0;

    if (buf == NULL || buf_len <= 0)
    {
        return -1;
    }

    len += snprintf(buf + len, buf_len - len,
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<TotalCapability version=\"1.0\">\n"
                    "<BasicCapability version=\"1.0\">\n"
                    "<SoftwareVersion>V1.0.0</SoftwareVersion>\n"
                    "<DeviceType>IPCamera</DeviceType>\n"
                    "<ChannelNums>1</ChannelNums>\n"
                    "</BasicCapability>\n"
                    "<NetworkAbility version=\"1.0\">\n"
                    "<Bonding>false</Bonding>\n"
                    "<PPPoE>false</PPPoE>\n"
                    "</NetworkAbility>\n");

    n = hik_cap_fill_av_compress(buf + len, buf_len - len, 0);
    if (n < 0)
    {
        return -1;
    }
    len += n;

    len += snprintf(buf + len, buf_len - len, "</TotalCapability>\n");
    if (len <= 0 || len >= buf_len)
    {
        return -1;
    }
    return len;
}

int hik_capability_build_xml(unsigned int capability_type, char *buf, int buf_len)
{
    unsigned int type = capability_type & 0xff;

    switch (type)
    {
    case 0:
        return hik_cap_fill_total(buf, buf_len);
    case 3:
        return hik_cap_fill_av_compress(buf, buf_len, 1);
    case 5:
        return hik_cap_fill_ccd(buf, buf_len, 1);
    default:
        return -2; /* not support */
    }
}
