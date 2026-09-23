#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "anj_mw_comm.h"
#include "rec_mov_write.h"
#include "rec_mov_utility.h"

//#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MAX_SPATIAL_SEGMENTATION 4096 // max. value of u(12) field

typedef struct HVCCProfileTierLevel
{
    unsigned char profile_space;
    unsigned char tier_flag;
    unsigned char profile_idc;
    unsigned int profile_compatibility_flags;
    uint64_t constraint_indicator_flags;
    unsigned char level_idc;
} HVCCProfileTierLevel;

typedef struct HVCCNALUnitArray
{
    unsigned char array_completeness;
    unsigned char NAL_unit_type;
    unsigned short numNalus;
    unsigned short *nalUnitLength;
    unsigned char **nalUnit;
} HVCCNALUnitArray;

typedef struct HEVCDecoderConfigurationRecord
{
    unsigned char configurationVersion;
    unsigned char general_profile_space;
    unsigned char general_tier_flag;
    unsigned char general_profile_idc;
    unsigned int general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    unsigned char general_level_idc;
    unsigned short min_spatial_segmentation_idc;
    unsigned char parallelismType;
    unsigned char chromaFormat;
    unsigned char bitDepthLumaMinus8;
    unsigned char bitDepthChromaMinus8;
    unsigned short avgFrameRate;
    unsigned char constantFrameRate;
    unsigned char numTemporalLayers;
    unsigned char temporalIdNested;
    unsigned char lengthSizeMinusOne;
    unsigned char numOfArrays;
    HVCCNALUnitArray *array;
} HEVCDecoderConfigurationRecord;

typedef enum
{
    HEVC_NAL_NULL = 0,
    HEVC_NAL_VPS = 32,
    HEVC_NAL_SPS = 33,
    HEVC_NAL_PPS = 34,
} HEVC_NAL_TYPE;

void print_buf(unsigned char *buf, unsigned int len)
{
    unsigned char i = 0;
    for (i = 0; i < len; i++)
    {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

void hton_set_u32(void *pp, unsigned int w)
{
    unsigned char *p = pp;

    p[0] = (w >> 24) & 0xff;
    p[1] = (w >> 16) & 0xff;
    p[2] = (w >> 8) & 0xff;
    p[3] = (w >> 0) & 0xff;
}

void hton_set_u16(void *pp, unsigned short w)
{
    unsigned char *p = pp;

    p[0] = (w >> 8) & 0xff;
    p[1] = (w >> 0) & 0xff;
}

void hton_set_u8(void *pp, unsigned char w)
{
    unsigned char *p = pp;

    p[0] = (w >> 0) & 0xff;
}

void hton_set_u24(void *pp, unsigned int w)
{
    unsigned char *p = pp;
    
    p[0] = (w >> 16) & 0xff;
    p[1] = (w >> 8) & 0xff;
    p[2] = (w >> 0) & 0xff;
}


int iframe_get_pps_sps(unsigned char *buf, int len, unsigned char *sps, unsigned int *sps_len, unsigned char *pps, unsigned int *pps_len)
{
    unsigned char *flag;
    int offset = 0;

    int config_len = 0;
    int iframe_offset = 0;

    while (offset < (len - 5))
    {
        flag = (unsigned char *)(buf + offset);

        if (flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 1)
        {
            if ((flag[4] & 0x1F) == 0x06)
            {
                config_len = offset;
                // break;
            }
            else if ((flag[4] & 0x1F) == 0x05) // nal_type == I frame
            {
                if (config_len == 0)
                    config_len = offset;

                iframe_offset = offset + 4;
                break;
            }
        }
        offset++;
    }

    // printf("config len = %d, iframe_offset = %d\n", config_len, iframe_offset);

    offset = 0;
    while (offset < (config_len - 5))
    {
        flag = (unsigned char *)(buf + offset);

        if (flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 1)
        {
            if ((flag[4] & 0x1F) == 0x08) // nal_type == PPS, johnnyling 20100628
            {
                memcpy(sps, buf + 4, offset - 4);
                *sps_len = offset - 4;

                memcpy(pps, buf + offset + 4, config_len - offset - 4);
                *pps_len = config_len - offset - 4;

                return iframe_offset;
            }
        }
        offset++;
    }

    return iframe_offset;
}

static void hvcc_init(HEVCDecoderConfigurationRecord *hvcc)
{
    memset(hvcc, 0, sizeof(HEVCDecoderConfigurationRecord));
    hvcc->configurationVersion = 1;
    hvcc->lengthSizeMinusOne = 3; // 4 bytes

    hvcc->general_profile_compatibility_flags = 0xffffffff;
    hvcc->general_constraint_indicator_flags = 0xffffffffffff;

    hvcc->min_spatial_segmentation_idc = MAX_SPATIAL_SEGMENTATION + 1;
}

int find_start_code(unsigned char *buf, int len, unsigned char **start)
{
    int offset = 0;
    unsigned char *start1 = buf;

    while (offset < (len - 5))
    {
        start1 = (unsigned char *)(buf + offset);

        if (start1[0] == 0 && start1[1] == 0 && start1[2] == 0 && start1[3] == 1)
        {
            // printf("flag = %p\r\n",flag);
            *start = start1;
            return 0;
        }
        offset++;
    }
    __ERR("not find vps/sps/pps frame\r\n");
    return -1;
}

int iframe_get_vps_sps_pps(unsigned char *buf, int len, unsigned char *vps, unsigned int *vps_len, unsigned char *sps, unsigned int *sps_len, unsigned char *pps, unsigned int *pps_len)
{
    int s32Ret = 0;
    int vps_sps_pps_len = 0;
    int len_offset = len;

    unsigned char *start = NULL;
    unsigned char *end = NULL;
    unsigned char *input_buff = buf;
    unsigned char *start_step = NULL;
    int len_temp = len;

    // find_start_code_test(buf,len);
    while (len_offset > 0)
    {
        s32Ret = find_start_code(input_buff, len_temp, &start);
        if (s32Ret < 0)
        {
            __ERR("not find start code\r\n");
            return -1;
        }
        start_step = input_buff + 4;
        s32Ret = find_start_code(start_step, len_temp - 4, &end);
        if (s32Ret < 0)
        {
            __ERR("not find end code\r\n");
            return -1;
        }
        len_offset = (unsigned int)(end - start) - 4;

        if (((start[4] >> 1) & 0x3F) == 0x20) // vps
        {
            memcpy(vps, start + 4, len_offset);
            *vps_len = len_offset;
        }
        else if (((start[4] >> 1) & 0x3F) == 0x21) // sps
        {
            memcpy(sps, start + 4, len_offset);
            *sps_len = len_offset;
        }
        else if (((start[4] >> 1) & 0x3F) == 0x22) // pps
        {
            memcpy(pps, start + 4, len_offset);
            *pps_len = len_offset;
            vps_sps_pps_len = *pps_len + *sps_len + *vps_len;
            return vps_sps_pps_len;
        }

        input_buff = end;
        len_temp -= len_offset;
    }
    __INFO("*vps_len = %d, sps_len = %d, pps_len = %d\r\n", *vps_len, *sps_len, *pps_len);
    vps_sps_pps_len = *pps_len + *sps_len + *vps_len;
    return vps_sps_pps_len;
}

void hvcc_update_ptl(HEVCDecoderConfigurationRecord *hvcc, HVCCProfileTierLevel *ptl)
{
    hvcc->general_profile_space = ptl->profile_space;

    if (hvcc->general_tier_flag < ptl->tier_flag)
        hvcc->general_level_idc = ptl->level_idc;
    else
        hvcc->general_level_idc = MAX(hvcc->general_level_idc, ptl->level_idc);
    hvcc->general_tier_flag = MAX(hvcc->general_tier_flag, ptl->tier_flag);
    hvcc->general_profile_idc = MAX(hvcc->general_profile_idc, ptl->profile_idc);
    hvcc->general_profile_compatibility_flags &= ptl->profile_compatibility_flags;
    hvcc->general_constraint_indicator_flags &= ptl->constraint_indicator_flags;
}

void hvcc_parse_ptl(unsigned char *data_buff, HEVCDecoderConfigurationRecord *hvcc)
{
    HVCCProfileTierLevel general_ptl;

    general_ptl.profile_space = (data_buff[0] >> 6) & 0x03; // 2bit
    general_ptl.tier_flag = (data_buff[0] >> 5) & 0x01;     // 1bit
    general_ptl.profile_idc = data_buff[0] & 0x1f;          // 5bit

    general_ptl.profile_compatibility_flags = 0;
    general_ptl.profile_compatibility_flags |= data_buff[1] << 24;
    general_ptl.profile_compatibility_flags |= data_buff[2] << 16;
    general_ptl.profile_compatibility_flags |= data_buff[3] << 8;
    general_ptl.profile_compatibility_flags |= data_buff[4];

    general_ptl.constraint_indicator_flags = 0;
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[5] << 40);
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[6] << 32);
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[7] << 24);
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[8] << 16);
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[9] << 8);
    general_ptl.constraint_indicator_flags |= ((uint64_t)data_buff[10]);

    general_ptl.level_idc = data_buff[11] & 0xff; // 8

    hvcc_update_ptl(hvcc, &general_ptl);
}

int iframe_write_hvcc(HEVCDecoderConfigurationRecord *iframe_hvcc, unsigned char *vps, unsigned char *sps)
{
    unsigned char vps_max_sub_layers_minus1;
    unsigned char sps_max_sub_layers_minus1;
    unsigned char vps_temporal_id_nesting_flag;
    unsigned char temporalIdNested;

    hvcc_init(iframe_hvcc);

    vps_max_sub_layers_minus1 = (vps[3] >> 1) & 0x07;
    vps_temporal_id_nesting_flag = vps[3] & 0x01;
    iframe_hvcc->numTemporalLayers = MAX(iframe_hvcc->numTemporalLayers, vps_max_sub_layers_minus1 + 1);
    hvcc_parse_ptl(vps + 6, iframe_hvcc);

    sps_max_sub_layers_minus1 = (sps[2] >> 1) & 0x7;
    iframe_hvcc->numTemporalLayers = MAX(iframe_hvcc->numTemporalLayers, sps_max_sub_layers_minus1 + 1);


    temporalIdNested = (sps[2]) & 0x1;
    iframe_hvcc->temporalIdNested = (temporalIdNested || vps_temporal_id_nesting_flag) ? 1 : 0;
    hvcc_parse_ptl(sps + 3, iframe_hvcc);

    if (iframe_hvcc->min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION)
        iframe_hvcc->min_spatial_segmentation_idc = 0;

    if (!iframe_hvcc->min_spatial_segmentation_idc)
        iframe_hvcc->parallelismType = 0;

    iframe_hvcc->avgFrameRate = 0;
    iframe_hvcc->constantFrameRate = 0;
    return 0;
}

int rec_mov_update_keyInfoBuf(media_frame_info_t *pFrameInfo, unsigned char *pkeyInfoBuf, int pkeyInfoBufLen, int frameCodec)
{
    int iRet = -1;
    if ((NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) || (NULL == pkeyInfoBuf) ||
        (0 >= pFrameInfo->frameParam.frameLen) || (0 >= pkeyInfoBufLen))
    {
        __ERR("Invalid Input Frame\n");
        goto endFunc;
    }

    unsigned char pps[256] = {0};
    unsigned char sps[256] = {0};
    unsigned char vps[256] = {0}; // h.265
    unsigned int pps_len = 0;
    unsigned int sps_len = 0;
    unsigned int vps_len = 0;
    unsigned int offset = 0;

    memset(pps, 0, sizeof(pps));
    memset(sps, 0, sizeof(sps));
    memset(pkeyInfoBuf, 0, pkeyInfoBufLen);

    if (frameCodec == MEDIA_CODEC_VIDEO_H265)
    {
        HEVCDecoderConfigurationRecord iframe_hvcc;
        unsigned char general_offset = 0;
        unsigned char constant_offset = 0;
        memset(&iframe_hvcc, 0, sizeof(iframe_hvcc));
        iframe_get_vps_sps_pps(pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, vps, &(vps_len), sps, &(sps_len), pps, &(pps_len));
        if (vps_len == 0 || sps_len == 0 || pps_len == 0 || (unsigned int)pkeyInfoBufLen < vps_len + sps_len + pps_len + 38)
        {
            __ERR("Invalid pkeyInfoBufLen %d,vsp %d,%d,%d\n", pkeyInfoBufLen, vps_len, sps_len, pps_len);
            goto endFunc;
        }
        iframe_write_hvcc(&iframe_hvcc, vps, sps);

        hton_set_u8(pkeyInfoBuf + offset, 1); // configurationVersion
        offset += 1;
        general_offset = ((iframe_hvcc.general_profile_space & 0x03) << 6) | ((iframe_hvcc.general_tier_flag & 0x01) << 5) | (iframe_hvcc.general_profile_idc & 0x1F);
        hton_set_u8(pkeyInfoBuf + offset, general_offset); //?? general_profile_space << 6 | general_tier_flag << 5 | general_profile_idc
        offset += 1;
        hton_set_u32(pkeyInfoBuf + offset, iframe_hvcc.general_profile_compatibility_flags); // general_profile_compatibility_flags
        offset += 4;
        hton_set_u32(pkeyInfoBuf + offset, iframe_hvcc.general_constraint_indicator_flags >> 16); // general_constraint_indicator_flags >> 16
        offset += 4;
        hton_set_u16(pkeyInfoBuf + offset, iframe_hvcc.general_constraint_indicator_flags); // general_constraint_indicator_flags
        offset += 2;
        hton_set_u8(pkeyInfoBuf + offset, iframe_hvcc.general_level_idc); //??general_level_idc
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, iframe_hvcc.min_spatial_segmentation_idc | 0xf000); //?? min_spatial_segmentation_idc | 0xf000
        offset += 2;
        hton_set_u8(pkeyInfoBuf + offset, iframe_hvcc.parallelismType | 0xfc); //??parallelismType | 0xfc
        offset += 1;
        hton_set_u8(pkeyInfoBuf + offset, iframe_hvcc.chromaFormat | 0xfc); //??chromaFormat | 0xfc
        offset += 1;
        hton_set_u8(pkeyInfoBuf + offset, iframe_hvcc.bitDepthLumaMinus8 | 0xf8); //??bitDepthLumaMinus8 | 0xf8
        offset += 1;
        hton_set_u8(pkeyInfoBuf + offset, iframe_hvcc.bitDepthChromaMinus8 | 0xf8); //??bitDepthChromaMinus8 | 0xf8
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, iframe_hvcc.avgFrameRate); // avgFrameRate
        offset += 2;
        constant_offset = (iframe_hvcc.constantFrameRate << 6) | ((iframe_hvcc.numTemporalLayers & 0x07) << 3) | ((iframe_hvcc.temporalIdNested & 0x01) << 2) | (iframe_hvcc.lengthSizeMinusOne & 0x03);
        hton_set_u8(pkeyInfoBuf + offset, constant_offset); //??constantFrameRate << 6 | numTemporalLayers << 3 | temporalIdNested  << 2 | lengthSizeMinusOne
        offset += 1;

        hton_set_u8(pkeyInfoBuf + offset, 3); // numOfArrays : vps/sps/pps
        offset += 1;
        hton_set_u8(pkeyInfoBuf + offset, (1 << 7) | (0x20 & 0x3f)); // vps
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, 1); // vps numNalus
        offset += 2;
        hton_set_u16(pkeyInfoBuf + offset, vps_len); // vps nalUnitLength
        offset += 2;
        memcpy(pkeyInfoBuf + offset, vps, vps_len);

        offset += vps_len;
        hton_set_u8(pkeyInfoBuf + offset, (1 << 7) | (0x21 & 0x3f)); // sps
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, 1); // sps numNalus
        offset += 2;
        hton_set_u16(pkeyInfoBuf + offset, sps_len); // sps nalUnitLength
        offset += 2;
        memcpy(pkeyInfoBuf + offset, sps, sps_len);

        offset += sps_len;
        hton_set_u8(pkeyInfoBuf + offset, (1 << 7) | (0x22 & 0x3f)); // pps
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, 1); // pps numNalus
        offset += 2;
        hton_set_u16(pkeyInfoBuf + offset, pps_len); // pps nalUnitLength
        offset += 2;
        memcpy(pkeyInfoBuf + offset, pps, pps_len);
        offset += pps_len;
        __INFO("Update vps(%d) sps(%d) pps(%d) infolen:%d\n", vps_len, sps_len, pps_len, offset);
        iRet = offset;
    }
    else if (frameCodec == MEDIA_CODEC_VIDEO_H264)
    {
        iframe_get_pps_sps(pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, sps, &(sps_len), pps, &(pps_len));
        if (sps_len == 0 || pps_len == 0 || (unsigned int)pkeyInfoBufLen < sps_len + pps_len + 11)
        {
            __ERR("Invalid pkeyInfoBufLen %d,sp %d,%d\n", pkeyInfoBufLen, sps_len, pps_len);
            goto endFunc;
        }
        hton_set_u8(pkeyInfoBuf + offset, 1);
        offset += 1;
        hton_set_u8(pkeyInfoBuf + offset, sps[1]);
        offset += 1;
        hton_set_u32(pkeyInfoBuf + offset, 0x001fffe1);
        offset += 4;
        // sps + pps
        hton_set_u16(pkeyInfoBuf + offset, sps_len);
        offset += 2;
        memcpy(pkeyInfoBuf + offset, sps, sps_len);
        offset += sps_len;
        hton_set_u8(pkeyInfoBuf + offset, 1);
        offset += 1;
        hton_set_u16(pkeyInfoBuf + offset, pps_len);
        offset += 2;
        memcpy(pkeyInfoBuf + offset, pps, pps_len);
        offset += pps_len;
        __INFO("Update sps(%d) pps(%d) infolen:%d\n", sps_len, pps_len, offset);
        iRet = offset;
    }
    else
    {
        __ERR("Invalid frame type %d\n", pFrameInfo->frameParam.frameCodec);
    }
endFunc:

    return iRet;
}

int rec_mov_startcode_to_size(media_frame_info_t *pFrameInfo, int frameCodec)
{
    int iRet = -1;
    if ((NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) || (0 >= pFrameInfo->frameParam.frameLen))
    {
        __ERR("Invalid Input Frame\n");
        goto endFunc;
    }

    unsigned char pps[256] = {0};
    unsigned char sps[256] = {0};
    unsigned char vps[256] = {0}; // h.265
    unsigned int pps_len = 0;
    unsigned int sps_len = 0;
    unsigned int vps_len = 0;
    unsigned int offset = 0;

    if (pFrameInfo->frameBuf[0] == 0 && pFrameInfo->frameBuf[1] == 0 && pFrameInfo->frameBuf[2] == 0 && pFrameInfo->frameBuf[3] == 1)
    {
        if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
        {
            if (frameCodec == MEDIA_CODEC_VIDEO_H265)
            {
                iframe_get_vps_sps_pps(pFrameInfo->frameBuf, 256, vps, &(vps_len), sps, &(sps_len), pps, &(pps_len));
                if (vps_len == 0 || sps_len == 0 || pps_len == 0 || pFrameInfo->frameParam.frameLen <= vps_len + sps_len + pps_len)
                {
                    __ERR("Invalid vsp %d,%d,%d, size:%d\n", vps_len, sps_len, pps_len, pFrameInfo->frameParam.frameLen);
                    goto endFunc;
                }
                hton_set_u32(&pFrameInfo->frameBuf[offset], vps_len); /* key index */
                offset += vps_len + 4;
                hton_set_u32(&pFrameInfo->frameBuf[offset], sps_len); /* key index */
                offset += sps_len + 4;
                hton_set_u32(&pFrameInfo->frameBuf[offset], pps_len); /* key index */
                offset += pps_len + 4;
                hton_set_u32(&pFrameInfo->frameBuf[offset], pFrameInfo->frameParam.frameLen - offset-4); /* key index */

                iRet = 0;
            }
            else if (frameCodec == MEDIA_CODEC_VIDEO_H264)
            {
                iframe_get_pps_sps(pFrameInfo->frameBuf, 256, sps, &(sps_len), pps, &(pps_len));
                if (sps_len == 0 || pps_len == 0 || pFrameInfo->frameParam.frameLen <= sps_len + pps_len)
                {
                    __ERR("Invalid vsp %d,%d, size:%d\n", sps_len, pps_len, pFrameInfo->frameParam.frameLen);
                    goto endFunc;
                }
                hton_set_u32(&pFrameInfo->frameBuf[offset], sps_len); /* key index */
                offset += sps_len + 4;
                hton_set_u32(&pFrameInfo->frameBuf[offset], pps_len); /* key index */
                offset += pps_len + 4;
                hton_set_u32(&pFrameInfo->frameBuf[offset], pFrameInfo->frameParam.frameLen - offset - 4); /* key index */
                iRet = 0;
            }
            else
            {
                __ERR("Invalid frame type %d\n", frameCodec);
            }
        }
        else
        {
            hton_set_u32(pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen - 4); /* key index */
        }
    }
    else
    {
        __ERR("Invalid frame No startcode [%x %x %x %x]\n", pFrameInfo->frameBuf[0], pFrameInfo->frameBuf[1], pFrameInfo->frameBuf[2], pFrameInfo->frameBuf[3]);
    }

endFunc:

    return iRet;
}
