#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "rec_mov_write.h"
#include "rec_mov_utility.h"

#define MOV_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define MOV_TRAK_VIDEO_ID (1)
#define MOV_TRAK_AUDIO_ID (2)
#define REC_SECONDS_PER_DAY (24 * 60 * 60)

#define MOV_MHLR_NAME_SIZE (12)
#define REC_MOV_MP4_WRITE_TIMEOUT_MS (500)
const char s_mov_mhlr_video_name[MOV_MHLR_NAME_SIZE + 1] = "VideoHandler";
const char s_mov_mhlr_audio_name[MOV_MHLR_NAME_SIZE + 1] = "SoundHandler";

static size_t rec_mov_fwrite(FILE *fp, const void *data, unsigned long long int bytes)
{
    unsigned long long start_ms = anj_mw_get_cputime_ms(NULL);
    size_t iRet = anj_mw_fwrite(fp, data, bytes);
    unsigned long long elapsed_ms = anj_mw_get_cputime_ms(NULL) - start_ms;

    if (iRet == (size_t)bytes && elapsed_ms >= REC_MOV_MP4_WRITE_TIMEOUT_MS)
    {
        __ERR("mp4 write timeout, bytes:%llu, elapsed:%llu ms\n", bytes, elapsed_ms);
        // return 0;
    }
    return iRet;
}

enum
{
    MOV_BRAND_FTYP = MOV_TAG('f', 't', 'y', 'p'),
    MOV_BRAND_ISOM = MOV_TAG('i', 's', 'o', 'm'),
    MOV_BRAND_ISO2 = MOV_TAG('i', 's', 'o', '2'),
    MOV_BRAND_MP41 = MOV_TAG('m', 'p', '4', '1'),
    MOV_BRAND_FREE = MOV_TAG('f', 'r', 'e', 'e'),
    MOV_BRAND_MDAT = MOV_TAG('m', 'd', 'a', 't'),

    MOV_BRAND_MOOV = MOV_TAG('m', 'o', 'o', 'v'),
    // MOOV 子节点
    MOV_BRAND_MVHD = MOV_TAG('m', 'v', 'h', 'd'),
    MOV_BRAND_TRAK = MOV_TAG('t', 'r', 'a', 'k'),
    // TRAK 子节点
    MOV_BRAND_TKHD = MOV_TAG('t', 'k', 'h', 'd'),
    MOV_BRAND_EDTS = MOV_TAG('e', 'd', 't', 's'),
    MOV_BRAND_MDIA = MOV_TAG('m', 'd', 'i', 'a'),

    // EDTS 子节点
    MOV_BRAND_ELST = MOV_TAG('e', 'l', 's', 't'),

    // MEDIA 子节点
    MOV_BRAND_MDHD = MOV_TAG('m', 'd', 'h', 'd'),
    MOV_BRAND_HDLR = MOV_TAG('h', 'd', 'l', 'r'),
    MOV_BRAND_MINF = MOV_TAG('m', 'i', 'n', 'f'),

    // MINF 子节点
    MOV_BRAND_VMHD = MOV_TAG('v', 'm', 'h', 'd'), // video
    MOV_BRAND_SMHD = MOV_TAG('s', 'm', 'h', 'd'), // audio
    MOV_BRAND_DINF = MOV_TAG('d', 'i', 'n', 'f'),
    MOV_BRAND_STBL = MOV_TAG('s', 't', 'b', 'l'),
    MOV_BRAND_VIDE = MOV_TAG('v', 'i', 'd', 'e'),
    MOV_BRAND_SOUN = MOV_TAG('s', 'o', 'u', 'n'),

    // DINF 子节点
    MOV_BRAND_DREF = MOV_TAG('d', 'r', 'e', 'f'),
    // DREF 子节点
    MOV_BRAND_URL = MOV_TAG('u', 'r', 'l', ' '),

    // STBL 子节点
    MOV_BRAND_STSD = MOV_TAG('s', 't', 's', 'd'),
    MOV_BRAND_STTS = MOV_TAG('s', 't', 't', 's'),
    MOV_BRAND_STSS = MOV_TAG('s', 't', 's', 's'),
    MOV_BRAND_STSC = MOV_TAG('s', 't', 's', 'c'),
    MOV_BRAND_STSZ = MOV_TAG('s', 't', 's', 'z'),
    MOV_BRAND_STCO = MOV_TAG('s', 't', 'c', 'o'),

    // STSD 子节点
    MOV_BRAND_HEV1 = MOV_TAG('h', 'e', 'v', '1'),
    MOV_BRAND_AVC1 = MOV_TAG('a', 'v', 'c', '1'),
    MOV_BRAND_ALAW = MOV_TAG('a', 'l', 'a', 'w'),
    MOV_BRAND_ULAW = MOV_TAG('u', 'l', 'a', 'w'),
    MOV_BRAND_HVCC = MOV_TAG('h', 'v', 'c', 'C'),
    MOV_BRAND_AVCC = MOV_TAG('a', 'v', 'c', 'C'),

    // MP4A 子节点
    MOV_BRAND_MP4A = MOV_TAG('m', 'p', '4', 'a'),
    MOV_BRAND_ESDS = MOV_TAG('e', 's', 'd', 's'),
    MOV_BRAND_BTRT = MOV_TAG('b', 't', 'r', 't'),
};

static int rec_mov_write_buffer(rec_file_write_param *pstRecWriteParam, const void *data, size_t len, FILE *fp)
{
    int iRet = 0;
    const char *ptr = data;

    while (len > 0)
    {
        size_t available = REC_MEDIA_WRITE_BUFFER - pstRecWriteParam->ptsDataBufLen;
        if (available > 0)
        {
            size_t to_copy = (len < available) ? len : available;
            memcpy(pstRecWriteParam->ptsDataBuf + pstRecWriteParam->ptsDataBufLen, ptr, to_copy);
            ptr += to_copy;
            pstRecWriteParam->ptsDataBufLen += to_copy;
            len -= to_copy;
        }

        if (pstRecWriteParam->ptsDataBufLen == REC_MEDIA_WRITE_BUFFER)
        {
            // __INFO("WRITE!\n");
            iRet = rec_mov_fwrite(fp, pstRecWriteParam->ptsDataBuf, REC_MEDIA_WRITE_BUFFER);
            if (iRet != REC_MEDIA_WRITE_BUFFER)
            {
                iRet = -1;
            }
            else
            {
                iRet = 1;
            }

            fflush(fp);
            memset(pstRecWriteParam->ptsDataBuf, 0, REC_MEDIA_WRITE_BUFFER);
            pstRecWriteParam->ptsDataBufLen = 0;
        }
    }
    return iRet;
}

static int rec_mov_write_buffer_flush(rec_file_write_param *pstRecWriteParam, FILE *fp)
{
    int iRet = 0;
    if (pstRecWriteParam->ptsDataBufLen)
    {
        __INFO("WRITE!\n");
        iRet = rec_mov_fwrite(fp, pstRecWriteParam->ptsDataBuf, pstRecWriteParam->ptsDataBufLen);
        if (iRet != pstRecWriteParam->ptsDataBufLen)
        {
            iRet = -1;
        }
        else
        {
            iRet = 1;
        }

        memset(pstRecWriteParam->ptsDataBuf, 0, REC_MEDIA_WRITE_BUFFER);
        pstRecWriteParam->ptsDataBufLen = 0;
    }
    pstRecWriteParam->iFileOffset = 0;
    return iRet;
}

static int rec_mov_write_box_free_segment_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    unsigned int iCrc32 = 0;
    int bChange = 0;

    pstMovInfo->stIndexSegment[pstMovInfo->pstIndexRecord->iMediaFileSegRecNums - 1].iMediaFileStatus = REC_STATUS_FINISH;
    for (int i = 0; i < pstMovInfo->pstIndexRecord->iMediaFileSegRecNums && i < REC_MEDIA_INDEX_MAX_SEGMENT; i++)
    {
        iCrc32 = anj_crc32_update(0, (unsigned char *)&pstMovInfo->stIndexSegment[i], REC_FILE_CRC_SIZE(REC_MEDIA_INDEX_SEGMENT_SIZE));
        if (pstMovInfo->stIndexSegment[i].iCrc32 != iCrc32)
        {
            __DBG("change crc to segment:%d, %d crc:%x %x\n", i, pstMovInfo->pstIndexRecord->iMediaFileSegRecNums, pstMovInfo->stIndexSegment[i].iCrc32, iCrc32);
            pstMovInfo->stIndexSegment[i].iCrc32 = iCrc32;
            rec_mov_index_file_segment_show(&pstMovInfo->stIndexSegment[i]);
            bChange = 1;
        }
    }
    if (bChange)
    {
        fseek(pstMovInfo->stMovfp, MOV_BOX_FREE_START_POS + MOV_BOX_STU_SIZE, SEEK_SET);

        /* write data */
        iRet = anj_mw_fwrite(pstMovInfo->stMovfp, pstMovInfo->stIndexSegment, sizeof(pstMovInfo->stIndexSegment));
        if (iRet != sizeof(pstMovInfo->stIndexSegment))
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), sizeof(pstMovInfo->stIndexSegment));
            return -1;
        }
    }
    else
    {
        __INFO("Not change no save\n");
    }

    return 0;
}

int rec_mov_read_box_free_segment(FILE *fp, rec_media_segment_index *pstSegMent, unsigned int count)
{
    int iRet = 0;
    unsigned int iCrc32 = 0;
    if ((NULL == fp) || (NULL == pstSegMent))
    {
        iRet = -1;
        __ERR("input param invalid\n");
        goto endFunc;
    }

    fseek(fp, MOV_BOX_FREE_START_POS + MOV_BOX_STU_SIZE, SEEK_SET);

    /* write data */
    iRet = anj_mw_fread(fp, pstSegMent, count * REC_MEDIA_INDEX_SEGMENT_SIZE);
    if (iRet != (int)(count * REC_MEDIA_INDEX_SEGMENT_SIZE))
    {
        __ERR("read file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), count * REC_MEDIA_INDEX_SEGMENT_SIZE);
        iRet = -1;
        goto endFunc;
    }

    for (unsigned int i = 0; i < count; i++)
    {
        if (pstSegMent[i].iMediaFileStatus != REC_STATUS_NULL)
        {
            // rec_mov_index_file_segment_show(&pstSegMent[i]);
            iCrc32 = anj_crc32_update(0, (unsigned char *)&pstSegMent[i], REC_FILE_CRC_SIZE(REC_MEDIA_INDEX_SEGMENT_SIZE));
            if (iCrc32 != pstSegMent[i].iCrc32)
            {
                __ERR("Invalid crc to invalid segment:%d, status:%d, count:%d,crc:%x,%x\n", i, pstSegMent->iMediaFileStatus, count,
                      iCrc32, pstSegMent[i].iCrc32);
                iRet = -1;
                goto endFunc;
            }
        }
    }
    iRet = 0;
endFunc:
    return iRet;
}

static int rec_mov_write_box_free_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    unsigned char DataBuf[256]; // 大于 MOV_BOX_FTYP_SIZE + MDAT 前期只写256
    int offset = 0;

    memset(DataBuf, 0, sizeof(DataBuf));

    // free
    hton_set_u32(&DataBuf[offset], MOV_BOX_FREE_SIZE); /* size */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_FREE); /* free */
    offset += 4;

    if (offset != (MOV_BOX_STU_SIZE))
    {
        __ERR("Invalid input data %d != %d\n", offset, MOV_BOX_FTYP_SIZE + MOV_BOX_STU_SIZE);
        return -1;
    }

    fseek(pstMovInfo->stMovfp, MOV_BOX_FREE_START_POS, SEEK_SET);

    /* write data */
    iRet = rec_mov_fwrite(pstMovInfo->stMovfp, DataBuf, sizeof(DataBuf));
    if (iRet != sizeof(DataBuf))
    {
        __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), offset);
        return -1;
    }

    return 0;
}

static int rec_mov_write_box_ftyp_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    unsigned char DataBuf[MOV_BOX_FTYP_SIZE]; // 大于 MOV_BOX_FTYP_SIZE + MDAT 前期只写256
    int offset = 0;

    memset(DataBuf, 0, sizeof(DataBuf));

    // ftyp
    hton_set_u32(&DataBuf[offset], MOV_BOX_FTYP_SIZE); /* size */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_FTYP); /* ftyp */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_ISOM); /* major_brand */
    offset += 4;
    hton_set_u32(&DataBuf[offset], 0x200); /* minor_version */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_ISOM); /* brands */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_MP41); /* brands */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_ISO2); /* brands */
    offset += 4;

    if (offset != sizeof(DataBuf))
    {
        __ERR("Invalid input data %d != %d\n", offset, sizeof(DataBuf));
        return -1;
    }

    fseek(pstMovInfo->stMovfp, MOV_BOX_FTYP_START_POS, SEEK_SET);

    /* write data */
    iRet = rec_mov_fwrite(pstMovInfo->stMovfp, DataBuf, sizeof(DataBuf));
    if (iRet != sizeof(DataBuf))
    {
        __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), offset);
        return -1;
    }

    return 0;
}

static int rec_mov_write_box_mdat_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo || NULL == pstMovInfo->stMovfp)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    unsigned char DataBuf[MOV_BOX_STU_SIZE]; // 大于 MOV_BOX_FTYP_SIZE + MDAT 前期只写256
    int offset = 0;
    unsigned int mdat_size = MOV_BOX_MDAT_SIZE;

    memset(DataBuf, 0, sizeof(DataBuf));

    /* 已有实际写入：按末尾偏移更新 size（close/NFS）；否则按预留布局初始化 */
    if (pstMovInfo->stMediaFileOffset > (MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE))
    {
        mdat_size = pstMovInfo->stMediaFileOffset - MOV_BOX_MDAT_START_POS;
    }
    else
    {
        pstMovInfo->stMediaFileOffset = MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE;
    }

    // mdat
    hton_set_u32(&DataBuf[offset], mdat_size); /* size */
    offset += 4;
    hton_set_u32(&DataBuf[offset], MOV_BRAND_MDAT); /* mdat */
    offset += 4;

    if (offset != sizeof(DataBuf))
    {
        __ERR("Invalid input data %d != %d\n", offset, sizeof(DataBuf));
        return -1;
    }

    fseek(pstMovInfo->stMovfp, MOV_BOX_MDAT_START_POS, SEEK_SET);
    __INFO("MOV_BOX_MDAT_START_POS = %d mdat_size = %u\n", MOV_BOX_MDAT_START_POS, mdat_size);

    /* write data */
    iRet = rec_mov_fwrite(pstMovInfo->stMovfp, DataBuf, sizeof(DataBuf));
    if (iRet != sizeof(DataBuf))
    {
        __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), offset);
        return -1;
    }

    return 0;
}

static int rec_mov_write_box_moov_mvhd(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    int offset = 0;
    unsigned char *pDataBuf = pstMovInfo->stMovVTrackBuf;

    // memset(pDataBuf+offset, 0, MOV_BOX_MOOV_MVHD_BUF_SIZE);
    /* MOOV */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_SIZE); /* moov size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MOOV);
    offset += 4;

    /* MVHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_MVHD_SIZE);
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MVHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // creation_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // modification_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, REC_MOV_PRE_MSEC); // time_scale
    offset += 4;
    pstMovInfo->stDurationBuf_mvhd = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration)); // duration
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x00010000); // rate
    offset += 4;
    hton_set_u16(pDataBuf + offset, 0x0100);     // volume
    offset += 12;                                // reserved 10位 + volume 2位
                                                 // matrix
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x40000000); /* u */
    offset += 4;
    offset += 24;                       // reserved 24bit
    hton_set_u32(pDataBuf + offset, 3); /* Next track id audio+video */
    offset += 4;
    if (offset != MOV_BOX_MOOV_MVHD_BUF_SIZE)
    {
        __ERR("Invalid input data %d != %d\n", offset, MOV_BOX_MOOV_MVHD_BUF_SIZE);
        return -1;
    }

    return iRet;
}

static int rec_mov_write_box_moov_vtrack(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    int offset = MOV_BOX_MOOV_MVHD_BUF_SIZE;
    unsigned char *pDataBuf = pstMovInfo->stMovVTrackBuf;

    // memset(pDataBuf+offset, 0, sizeof(pstMovInfo->stMovVTrackBuf)-offset);
    /* VTRAK */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_SIZE); /* V TRAK size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_TRAK);
    offset += 4;
    /* TKHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_TKHD_SIZE); /* TKHD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_TKHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0X00000003); /* VER:0 flasg:3 */
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // creation_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // modification_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_TRAK_VIDEO_ID); /* track_ID */
    offset += 4;
    offset += 4; /* reserved */
    pstMovInfo->stDurationBuf_vtrak_tkhd = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration)); /* duration */
    offset += 4;
    offset += 8; /* reserved */

    hton_set_u16(pDataBuf + offset, 0); // layer
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // alternate_group
    offset += 2;
    offset += 4; /* volume + res */
    // matrix
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x40000000); /* u */
    offset += 4;

    hton_set_u16(pDataBuf + offset, pstMovInfo->pstVcodecParam->width); // track_width
    offset += 2;
    offset += 2;
    hton_set_u16(pDataBuf + offset, pstMovInfo->pstVcodecParam->height); // track_height
    offset += 2;
    offset += 2;

    /* EDTS */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_EDTS_SIZE); /* EDTS size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_EDTS);
    offset += 4;
    /* ELST */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_EDTS_ELST_SIZE); /* ELST size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_ELST);
    offset += 4;

    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // counts
    offset += 4;
    pstMovInfo->stDurationBuf_vtrak_elst = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration)); // track_duration
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // media_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x00010000); // media_rate_integer media_rate_fraction
    offset += 4;

    /* MDIA */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_SIZE); /* MDIA size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MDIA);
    offset += 4;

    /* MDIA MDHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MDHD_SIZE); /* EDTS size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MDHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // creation_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // modification_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, REC_MOV_PRE_SEC); // time_scale
    offset += 4;
    pstMovInfo->stDurationBuf_vtrak_mdhd = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, pstMovInfo->vtime_duration); // track_duration
    offset += 4;
    hton_set_u16(pDataBuf + offset, 0); // language
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // quality
    offset += 2;

    /* MDIA HDLR */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_HDLR_SIZE); /* HDLR size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_HDLR);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // component_type
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_VIDE); // component_subtype
    offset += 4;
    offset += 12; // component_manufacturer, component_flags, component_flags_mask
    memcpy(pDataBuf + offset, s_mov_mhlr_video_name, MOV_MHLR_NAME_SIZE);
    offset += MOV_MHLR_NAME_SIZE + 1;
    /* MDIA MINF */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_SIZE); /* MINF size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MINF);
    offset += 4;
    /* MDIA MINF VMHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_VMHD_SIZE); /* VMHD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_VMHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x01); /* version & flags */
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // graphics_mode, opcolor_red
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // opcolor_green, opcolor_blue
    offset += 4;
    /* MDIA MINF DINF */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_DINF_SIZE); /* DINF size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_DINF);
    offset += 4;
    // dref
    hton_set_u32(pDataBuf + offset, 28); // size
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_DREF);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;
    // url
    hton_set_u32(pDataBuf + offset, 12); // size
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_URL);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;

    /* MDIA MINF STBL */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_SIZE); /* STBL size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_STBL);
    offset += 4;

    /* MDIA MINF STBL STSD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE); /* STSD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_STSD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 16); // description_size
    offset += 4;
    if (pstMovInfo->pstVcodecParam->vcodecType == MEDIA_CODEC_VIDEO_H265) // MOV_H265_TYPE
    {
        hton_set_u32(pDataBuf + offset, MOV_BRAND_HEV1); // data_format : h265
    }
    else
    {
        hton_set_u32(pDataBuf + offset, MOV_BRAND_AVC1); // data_format : h264
    }
    offset += 4;
    offset += 6;                        // res
    hton_set_u16(pDataBuf + offset, 1); // data_reference_index
    offset += 2;
    offset += 16;

    hton_set_u16(pDataBuf + offset, pstMovInfo->pstVcodecParam->width); // width
    offset += 2;
    hton_set_u16(pDataBuf + offset, pstMovInfo->pstVcodecParam->height); // height
    offset += 2;
    hton_set_u32(pDataBuf + offset, 0x480000); // h_resolution
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x480000); // v_resolution
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // data_size
    offset += 4;
    hton_set_u16(pDataBuf + offset, 1); // frame_count(frame count in each sample)
    offset += 2;
    offset += 32;                          // res
    hton_set_u16(pDataBuf + offset, 0x18); /* Reserved */
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0xffff); /* Reserved */
    offset += 2;
    if (pstMovInfo->pstVcodecParam->vcodecType == MEDIA_CODEC_VIDEO_H265) // MOV_H265_TYPE
    {
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 16 - 86); /* STSD size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_HVCC);
        offset += 4;
        // SPS..
        pstMovInfo->stMovVtrak_stsd_info = pDataBuf + offset;
        pstMovInfo->stMovVtrak_stsd_infoLen = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 16 - 86 - 8;
        offset += pstMovInfo->stMovVtrak_stsd_infoLen;
    }
    else
    {
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 16 - 86); /* STSD size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_AVCC);
        offset += 4;
        // SPS..
        pstMovInfo->stMovVtrak_stsd_info = pDataBuf + offset;
        pstMovInfo->stMovVtrak_stsd_infoLen = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 16 - 86 - 8;
        offset += pstMovInfo->stMovVtrak_stsd_infoLen;
    }

    if (offset != (MOV_BOX_MOOV_VTRACK_BUF_SIZE))
    {
        __ERR("Invalid input data %d != %d\n", offset, MOV_BOX_MOOV_VTRACK_BUF_SIZE);
        return -1;
    }

    return iRet;
}

static int rec_mov_write_box_moov_atrack(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    int offset = 0;
    unsigned char *pDataBuf = pstMovInfo->stATrackBuf;

    // memset(pDataBuf+offset, 0, sizeof(pstMovInfo->stATrackBuf)-offset);
    /* ATRAK */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_SIZE); /* A TRAK size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_TRAK);
    offset += 4;
    /* TKHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_TKHD_SIZE); /* TKHD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_TKHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0X00000003); /* VER:0 flasg:3 */
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // creation_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // modification_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_TRAK_AUDIO_ID); /* track_ID */
    offset += 4;
    offset += 4; /* reserved */
    pstMovInfo->stDurationBuf_atrak_tkhd = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, pstMovInfo->atime_duration); /* duration */
    // hton_set_u32(pDataBuf + offset, REC_PTS_TO_MSEC(pstMovInfo->atime_duration)); /* duration */
    offset += 4;
    offset += 8;                        /* reserved */
    hton_set_u16(pDataBuf + offset, 0); // layer
    offset += 2;
    hton_set_u16(pDataBuf + offset, 1); // alternate_group
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0x0100); // volume
    offset += 2;
    offset += 2; // res3
    // matrix
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x00010000); /* u */
    offset += 16;
    hton_set_u32(pDataBuf + offset, 0x40000000); /* u */
    offset += 4;

    hton_set_u16(pDataBuf + offset, 0); // track_width
    offset += 2;
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // track_height
    offset += 2;
    offset += 2;

    /* EDTS */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_EDTS_SIZE); /* EDTS size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_EDTS);
    offset += 4;
    /* ELST */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_EDTS_ELST_SIZE); /* ELST size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_ELST);
    offset += 4;

    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 2); // counts
    offset += 4;
    pstMovInfo->stDurationBuf_atrak_elst_delay = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, pstMovInfo->atime_duration_delay); // delay time
    // hton_set_u32(pDataBuf + offset, pstMovInfo->atime_duration_delay * REC_MOV_PRE_MSEC / REC_MOV_PRE_SEC); // delay time
    offset += 4;
    hton_set_u32(pDataBuf + offset, -1); // media_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x00010000); // media_rate_integer media_rate_fraction
    offset += 4;
    pstMovInfo->stDurationBuf_atrak_elst = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, pstMovInfo->atime_duration); // track_duration
    // hton_set_u32(pDataBuf + offset, REC_PTS_TO_MSEC(pstMovInfo->atime_duration)); // track_duration
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // media_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0x00010000); // media_rate_integer media_rate_fraction
    offset += 4;

    /* MDIA */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_SIZE); /* MDIA size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MDIA);
    offset += 4;

    /* MDIA MDHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MDHD_SIZE); /* EDTS size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MDHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // creation_time
    offset += 4;
    hton_set_u32(pDataBuf + offset, pstMovInfo->time_create); // modification_time
    offset += 4;
    // hton_set_u32(pDataBuf + offset, REC_MOV_PRE_SEC); // time_scale
    hton_set_u32(pDataBuf + offset, REC_MOV_PRE_MSEC); // time_scale
    offset += 4;
    pstMovInfo->stDurationBuf_atrak_mdhd = pDataBuf + offset;
    hton_set_u32(pDataBuf + offset, pstMovInfo->atime_duration); // track_duration
    offset += 4;
    hton_set_u16(pDataBuf + offset, 0); // language
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // quality
    offset += 2;

    /* MDIA HDLR */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_HDLR_SIZE); /* HDLR size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_HDLR);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // component_type
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_SOUN); // component_subtype
    offset += 4;
    offset += 12; // component_manufacturer, component_flags, component_flags_mask
    memcpy(pDataBuf + offset, s_mov_mhlr_audio_name, MOV_MHLR_NAME_SIZE);
    offset += MOV_MHLR_NAME_SIZE + 1;

    /* MDIA MINF */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_SIZE); /* MINF size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_MINF);
    offset += 4;
    /* MDIA MINF SMHD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_SMHD_SIZE); /* VMHD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_SMHD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); /* version & flags */
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); /* reserved (balance, normally = 0) */
    offset += 4;

    /* MDIA MINF DINF */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_DINF_SIZE); /* DINF size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_DINF);
    offset += 4;
    // dref
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_DINF_SIZE - MOV_BOX_STU_SIZE); // size
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_DREF);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;
    // url
    hton_set_u32(pDataBuf + offset, 12); // size
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_URL);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;

    /* MDIA MINF STBL */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_SIZE); /* STBL size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_STBL);
    offset += 4;

    /* MDIA MINF STBL STSD */
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_SIZE); /* STSD size */
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BRAND_STSD);
    offset += 4;
    hton_set_u32(pDataBuf + offset, 0); // version & flags
    offset += 4;
    hton_set_u32(pDataBuf + offset, 1); // count
    offset += 4;
    hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_SIZE - 16); // description_size
    offset += 4;

    __INFO("audio codec type %d\n", pstMovInfo->pstAcodecParam->acodecType);
    if (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_AAC)
    {
        hton_set_u32(pDataBuf + offset, MOV_BRAND_MP4A); // data_format : mp4a
    }
    else
    {
        if (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_G711U) // MOV_G711A_TYPE
        {
            hton_set_u32(pDataBuf + offset, MOV_BRAND_ULAW); // data_format : ulaw
        }
        else
        {
            hton_set_u32(pDataBuf + offset, MOV_BRAND_ALAW); // data_format : alaw
        }
    }
    offset += 4;
    offset += 6;                        // res
    hton_set_u16(pDataBuf + offset, 1); // data_reference_index
    offset += 2;
    offset += 8;
    hton_set_u16(pDataBuf + offset, pstMovInfo->pstAcodecParam->channels); // number_of_channels
    offset += 2;
    hton_set_u16(pDataBuf + offset, pstMovInfo->pstAcodecParam->bitWidth); //
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // compression_id
    offset += 2;
    hton_set_u16(pDataBuf + offset, 0); // packet_size
    offset += 2;
    hton_set_u16(pDataBuf + offset, pstMovInfo->pstAcodecParam->sampleRate); // sample_rate
    offset += 2;
    offset += 2; // res

    /* MDIA MINF STBL STSD MP4A ESDS*/
    if (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_AAC)
    {
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_MP4A_ESDS_SIZE); // description_size
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_ESDS);
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); // version & flags
        offset += 4;
        hton_set_u8(pDataBuf + offset, 0x03); // 0x03:ES_Descriptor
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x22);
        offset += 1;
        hton_set_u16(pDataBuf + offset, 0x0002); // ES_ID
        offset += 2;
        hton_set_u8(pDataBuf + offset, 0x0); // flags: streamDependenceFlag=0, URL_Flag=0, OCRstreamFlag=0
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x04); // 0x04: DecoderConfigDescriptor
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x14);
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x40); // objectTypeIndication = 0x40 (MPEG-4 Audio)
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x15); // streamType(6bit) = 0x05 (AudioStream) + upstream(1bit) + reserved(1bit)
        offset += 1;
        hton_set_u24(pDataBuf + offset, 0x000000); // bufferSizeDB = 0x000000
        offset += 3;
        hton_set_u32(pDataBuf + offset, 0x00200000); // maxBitrate = 2097152 bps,可以根据实际码率修改
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0x00010000); // avgBitrate = 65535 bps,可以根据实际码率修改
        offset += 4;
        hton_set_u8(pDataBuf + offset, 0x05); // tag = 0x05(DecoderSpecificInfo)
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x02);
        offset += 1;
        hton_set_u16(pDataBuf + offset, 0x1190); // AudioSpecificConfig(0001 0001 1001 0000),00010 → AudioObjectType = 2 (AAC LC), 0011 → SamplingFrequencyIndex = 3 (48kHz), 0010 → ChannelConfiguration = 2 (stereo)
        offset += 2;
        hton_set_u8(pDataBuf + offset, 0x06); // tag = 0x06(SLConfigDescriptor)
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x80); // Base-128 表示的长度
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x01);
        offset += 1;
        hton_set_u8(pDataBuf + offset, 0x02); // 预定义配置，表示使用默认 SL 配置
        offset += 1;

        /* MDIA MINF STBL STSD MP4A BTRT*/
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_MP4A_BTRT_SIZE); // description_size
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_BTRT);
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0x00000000); // buffer_size_db
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0x00200000); // max_bitrate
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0x00010000); // avg_bitrate
        offset += 4;
    }

    offset += MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_MP4A_ESDS_SIZE + MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_MP4A_BTRT_SIZE;
    if (offset != (MOV_BOX_MOOV_ATRACK_BUF_SIZE))
    {
        __ERR("Invalid input data %d != %d\n", offset, MOV_BOX_MOOV_ATRACK_BUF_SIZE);
        return -1;
    }

    return iRet;
}

int rec_mov_write_box_moov_track_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;
    fseek(pstMovInfo->stMovfp, MOV_BOX_MOOV_STSRT_POS, SEEK_SET);

    /* write data */
    iRet = rec_mov_fwrite(pstMovInfo->stMovfp, pstMovInfo->stMovVTrackBuf, MOV_BOX_MOOV_VTRACK_BUF_SIZE);
    if (iRet != MOV_BOX_MOOV_VTRACK_BUF_SIZE)
    {
        __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), MOV_BOX_MOOV_VTRACK_BUF_SIZE);
        return -1;
    }

    fseek(pstMovInfo->stMovfp, MOV_BOX_MOOV_ATRAK_START_POS, SEEK_SET);

    /* write data */
    iRet = rec_mov_fwrite(pstMovInfo->stMovfp, pstMovInfo->stATrackBuf, MOV_BOX_MOOV_ATRACK_BUF_SIZE);
    if (iRet != MOV_BOX_MOOV_ATRACK_BUF_SIZE)
    {
        __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), MOV_BOX_MOOV_ATRACK_BUF_SIZE);
        return -1;
    }
    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_stts_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    unsigned int iRet = 0;
    int iWriteIndex = 0;

    unsigned char *iWriteBuf = NULL;
    unsigned int iWriteSize = 0;
    unsigned int iWriteSeek = 0;

    unsigned char *iWriteHeadBuf = NULL;
    unsigned int iWriteHeadSize = 0;
    unsigned int iWriteHeadSeek = 0;

    if (pstMovInfo->stVTrackFrameIndex != pstMovInfo->stVTrackSaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stVTrackSttsBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STTS_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STTS_START_POS;
        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STTS_BUF_SIZE;
        if (pstMovInfo->stVTrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stVTrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STTS_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Vmp4 %p seek: %u,size: %u, h:%p hseek %u\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    if (pstMovInfo->stATrackFrameIndex != pstMovInfo->stATrackSaveFrameIndex)
    {
        // AUDIO
        iWriteHeadBuf = pstMovInfo->stATrackSttsBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STTS_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STTS_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STTS_BUF_SIZE;
        if (pstMovInfo->stATrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stATrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STTS_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Amp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_stss_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    unsigned int iRet = 0;
    int iWriteIndex = 0;

    unsigned char *iWriteBuf = NULL;
    unsigned int iWriteSize = 0;
    unsigned int iWriteSeek = 0;

    unsigned char *iWriteHeadBuf = NULL;
    unsigned int iWriteHeadSize = 0;
    unsigned int iWriteHeadSeek = 0;

    if (pstMovInfo->stVTrackKeyFrameIndex != pstMovInfo->stVTrackKeySaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stVTrackStssBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STSS_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSS_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        int writeKeyIndex = (pstMovInfo->stVTrackKeyFrameIndex - 1) % 1024;
        iWriteSize = MOV_BOX_MOOV_STBL_STSS_SIZE((writeKeyIndex + 1));
        if (pstMovInfo->stVTrackKeyFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stVTrackKeyFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STSS_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Vmp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }
    // audio NULL
    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_stsc_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    unsigned int iRet = 0;
    int iWriteIndex = 0;

    unsigned char *iWriteBuf = NULL;
    unsigned int iWriteSize = 0;
    unsigned int iWriteSeek = 0;

    unsigned char *iWriteHeadBuf = NULL;
    unsigned int iWriteHeadSize = 0;
    unsigned int iWriteHeadSeek = 0;

    // head与数据一起写
    if (pstMovInfo->stVTrackFrameIndex != pstMovInfo->stVTrackSaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stVTrackStscBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STSC_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSC_START_POS;
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STSC_BUF_SIZE;
        if (pstMovInfo->stVTrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stVTrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STSC_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Vmp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    // AUDIO
    if (pstMovInfo->stATrackFrameIndex != pstMovInfo->stATrackSaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stATrackStscBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STSC_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSC_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STSC_BUF_SIZE;
        if (pstMovInfo->stATrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stATrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STSC_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Amp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_stsz_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    unsigned int iRet = 0;
    int iWriteIndex = 0;
    unsigned char *iWriteBuf = NULL;
    unsigned int iWriteSize = 0;
    unsigned int iWriteSeek = 0;

    unsigned char *iWriteHeadBuf = NULL;
    unsigned int iWriteHeadSize = 0;
    unsigned int iWriteHeadSeek = 0;
    if (pstMovInfo->stVTrackFrameIndex != pstMovInfo->stVTrackSaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stVTrackStszBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STSZ_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSZ_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STSZ_BUF_SIZE;
        if (pstMovInfo->stVTrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stVTrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STSZ_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Vmp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }
    if (pstMovInfo->stATrackFrameIndex != pstMovInfo->stATrackSaveFrameIndex)
    {
        // AUDIO
        iWriteHeadBuf = pstMovInfo->stATrackStszBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STSZ_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSZ_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STSZ_BUF_SIZE;

        if (pstMovInfo->stATrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stATrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STSZ_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }

            __DBG("Amp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);

            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_stco_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    unsigned int iRet = 0;
    int iWriteIndex = 0;

    unsigned char *iWriteBuf = NULL;
    unsigned int iWriteSize = 0;
    unsigned int iWriteSeek = 0;

    unsigned char *iWriteHeadBuf = NULL;
    unsigned int iWriteHeadSize = 0;
    unsigned int iWriteHeadSeek = 0;
    if (pstMovInfo->stVTrackFrameIndex != pstMovInfo->stVTrackSaveFrameIndex)
    {
        iWriteHeadBuf = pstMovInfo->stVTrackStcoBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STCO_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STCO_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STCO_BUF_SIZE;
        if (pstMovInfo->stVTrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stVTrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STCO_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }
            __DBG("Vmp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);
            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }
    if (pstMovInfo->stATrackFrameIndex != pstMovInfo->stATrackSaveFrameIndex)
    {
        // AUDIO
        iWriteHeadBuf = pstMovInfo->stATrackStcoBuf;
        iWriteHeadSize = MOV_BOX_MOOV_STBL_STCO_HEAD_SIZE;
        iWriteHeadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_START_POS;

        // head与数据一起写
        iWriteBuf = iWriteHeadBuf;
        iWriteSeek = iWriteHeadSeek;
        iWriteSize = MOV_BOX_MOOV_TRACK_STCO_BUF_SIZE;
        if (pstMovInfo->stATrackFrameIndex > 0)
        {
            iWriteIndex = ANJ_ALIGN_DOWN(pstMovInfo->stATrackFrameIndex - 1, REC_MEDIA_BUF_FPS);
        }
        else
        {
            iWriteIndex = 0;
        }

        if (iWriteIndex > 0)
        {
            // 分开head 与 数据
            iWriteBuf += iWriteHeadSize;
            iWriteSeek += MOV_BOX_MOOV_STBL_STCO_SIZE(iWriteIndex);
            iWriteSize -= iWriteHeadSize;
            if (0 == bSaveHead)
            {
                iWriteHeadBuf = NULL;
            }
            __DBG("Amp4 %p seek: %d,size: %u, h:%p hseek %d\n", iWriteBuf, iWriteSeek, iWriteSize, iWriteHeadBuf, iWriteHeadSeek);
            if (iWriteHeadBuf)
            {
                fseek(pstMovInfo->stMovfp, iWriteHeadSeek, SEEK_SET);
                /* write data */
                iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteHeadBuf, iWriteHeadSize);
                if (iRet != iWriteHeadSize)
                {
                    __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteHeadSize);
                    return -1;
                }
            }
        }

        fseek(pstMovInfo->stMovfp, iWriteSeek, SEEK_SET);
        /* write data */
        iRet = rec_mov_fwrite(pstMovInfo->stMovfp, iWriteBuf, iWriteSize);
        if (iRet != iWriteSize)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), iWriteSize);
            return -1;
        }
    }

    return 0;
}

static int rec_mov_write_box_moov_trak_stbl_save(rec_mov_info_t *pstMovInfo, int bSaveHead)
{
    int iRet = -1;
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return iRet;
    }
    iRet = rec_mov_write_box_moov_trak_stbl_stts_save(pstMovInfo, bSaveHead);
    if (0 != iRet)
    {
        __ERR("stts\n");
        return iRet;
    }
    iRet = rec_mov_write_box_moov_trak_stbl_stss_save(pstMovInfo, bSaveHead);
    if (0 != iRet)
    {
        __ERR("stss\n");
        return iRet;
    }
    iRet = rec_mov_write_box_moov_trak_stbl_stsc_save(pstMovInfo, bSaveHead);
    if (0 != iRet)
    {
        __ERR("stsc\n");
        return iRet;
    }
    iRet = rec_mov_write_box_moov_trak_stbl_stsz_save(pstMovInfo, bSaveHead);
    if (0 != iRet)
    {
        __ERR("stsz\n");
        return iRet;
    }
    iRet = rec_mov_write_box_moov_trak_stbl_stco_save(pstMovInfo, bSaveHead);
    if (0 != iRet)
    {
        __ERR("stco\n");
        return iRet;
    }

    return iRet;
}

int rec_mov_write_box_moov_trak_update(rec_mov_info_t *pstMovInfo, int bSave)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    int iRet = 0;

    if ((pstMovInfo->stVTrackFrameIndex != pstMovInfo->stVTrackSaveFrameIndex) || (pstMovInfo->stATrackFrameIndex != pstMovInfo->stATrackSaveFrameIndex))
    {
        unsigned char *pDataBuf = NULL;
        int offset = 0;
        /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_mvhd, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration));       /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_vtrak_tkhd, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration)); /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_vtrak_elst, REC_PTS_TO_MSEC(pstMovInfo->vtime_duration)); /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_vtrak_mdhd, pstMovInfo->vtime_duration);                  /* duration */

        // hton_set_u32(pstMovInfo->stDurationBuf_atrak_tkhd, REC_PTS_TO_MSEC(pstMovInfo->atime_duration));                                      /* duration */
        // hton_set_u32(pstMovInfo->stDurationBuf_atrak_elst_delay, REC_PTS_TO_MSEC(pstMovInfo->atime_duration_delay)); /* duration */
        // hton_set_u32(pstMovInfo->stDurationBuf_atrak_elst, REC_PTS_TO_MSEC(pstMovInfo->atime_duration));                                      /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_atrak_tkhd, pstMovInfo->atime_duration);             /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_atrak_elst_delay, pstMovInfo->atime_duration_delay); /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_atrak_elst, pstMovInfo->atime_duration);             /* duration */
        hton_set_u32(pstMovInfo->stDurationBuf_atrak_mdhd, pstMovInfo->atime_duration);             /* duration */

        /* stbl */
        /* video*/
        /* stts size+stts+ver&flag+count+ count*(fps(4)+perchunktimes(4))*/
        pDataBuf = pstMovInfo->stVTrackSttsBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STTS_SIZE); /* size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STTS); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stVTrackFrameIndex); /* count */

        /* stss*/
        pDataBuf = pstMovInfo->stVTrackStssBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSS_SIZE); /* size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STSS); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stVTrackKeyFrameIndex); /* count */

        /* stsc*/
        pDataBuf = pstMovInfo->stVTrackStscBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSC_SIZE); /*size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STSC); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stVTrackFrameIndex); /* count */

        /* stsz*/
        pDataBuf = pstMovInfo->stVTrackStszBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSZ_SIZE); /*  size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STSZ); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* perflag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stVTrackFrameIndex); /* count */

        /* stco*/
        pDataBuf = pstMovInfo->stVTrackStcoBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STCO_SIZE); /*  size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STCO); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stVTrackFrameIndex); /* count */

        /* audio */
        /* stts*/
        pDataBuf = pstMovInfo->stATrackSttsBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STTS_SIZE); /* size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STTS); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stATrackFrameIndex); /* count */

        /* stsc*/
        pDataBuf = pstMovInfo->stATrackStscBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSC_SIZE); /*size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STSC); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stATrackFrameIndex); /* count */

        /* stsz*/
        pDataBuf = pstMovInfo->stATrackStszBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSZ_SIZE); /*  size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STSZ); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* perflag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stATrackFrameIndex); /* count */

        /* stco*/
        pDataBuf = pstMovInfo->stATrackStcoBuf;
        offset = 0;
        hton_set_u32(pDataBuf + offset, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_SIZE); /*  size */
        offset += 4;
        hton_set_u32(pDataBuf + offset, MOV_BRAND_STCO); /* BRAND */
        offset += 4;
        hton_set_u32(pDataBuf + offset, 0); /* ver&flag */
        offset += 4;
        hton_set_u32(pDataBuf + offset, pstMovInfo->stATrackFrameIndex); /* count */

        if (bSave)
        {
            iRet = rec_mov_write_buffer_flush(&pstMovInfo->stRecWriteParam, pstMovInfo->stMovfp);
            if (-1 == iRet)
            {
                __ERR("write buffer flush\n");
                return iRet;
            }

            iRet = rec_mov_write_box_moov_track_save(pstMovInfo);
            if (0 != iRet)
            {
                __ERR("track save\n");
                return iRet;
            }
            iRet = rec_mov_write_box_moov_trak_stbl_save(pstMovInfo, 1);
            if (0 != iRet)
            {
                __ERR("trak stbl\n");
                return iRet;
            }
            if (pstMovInfo->pstIndexRecord)
            {
                iRet = rec_mov_write_box_free_segment_save(pstMovInfo);
                if (0 != iRet)
                {
                    __ERR("trak segment\n");
                    return iRet;
                }
            }
            fflush(pstMovInfo->stMovfp);

            __INFO("Save index change V:%d,%d, KV:%d,%d, A:%d,%d, time:%u ms - %u!!!\n",
                   pstMovInfo->stVTrackFrameIndex, pstMovInfo->stVTrackSaveFrameIndex,
                   pstMovInfo->stVTrackKeyFrameIndex, pstMovInfo->stVTrackKeySaveFrameIndex,
                   pstMovInfo->stATrackFrameIndex, pstMovInfo->stATrackSaveFrameIndex,
                   REC_PTS_TO_MSEC(pstMovInfo->vtime_duration), pstMovInfo->vtime_duration);

            pstMovInfo->stVTrackSaveFrameIndex = pstMovInfo->stVTrackFrameIndex;
            pstMovInfo->stATrackSaveFrameIndex = pstMovInfo->stATrackFrameIndex;
            pstMovInfo->stVTrackKeySaveFrameIndex = pstMovInfo->stVTrackKeyFrameIndex;
        }
    }
    else
    {
        __INFO("Save index Not change V:%d,%d, A:%d,%d!!!\n", pstMovInfo->stVTrackFrameIndex, pstMovInfo->stVTrackSaveFrameIndex,
               pstMovInfo->stATrackFrameIndex, pstMovInfo->stATrackSaveFrameIndex);
    }
    return iRet;
}

static int rec_mov_write_box_moov_trak_stbl_init_save(rec_mov_info_t *pstMovInfo)
{
    if (NULL == pstMovInfo)
    {
        __ERR("input param invalid\n");
        return -1;
    }
    pstMovInfo->stVTrackFrameIndex = 0;
    pstMovInfo->stATrackFrameIndex = 0;
    pstMovInfo->stVTrackSaveFrameIndex = -1;
    pstMovInfo->stATrackSaveFrameIndex = -1;
    return 0;
}

int rec_mov_write_box_moov_vtrak_write_frame(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    if (pstMovInfo->stMediaFileOffset < MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE)
    {
        pstMovInfo->stMediaFileOffset = MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE;
        __ERR("Invalid media offset %d\n", pstMovInfo->stMediaFileOffset);
    }

    if (pstMovInfo->stMediaFileOffset + pFrameInfo->frameParam.frameLen >= REC_MEDIA_FILE_SIZE || pstMovInfo->stVTrackFrameIndex > REC_MEDIA_MAX_FPS)
    {
        __ERR("Skip frame, oversize %d+%d >= %d, fps:%d>%d\n", pstMovInfo->stMediaFileOffset,
              pFrameInfo->frameParam.frameLen, REC_MEDIA_FILE_SIZE,
              pstMovInfo->stVTrackFrameIndex, REC_MEDIA_MAX_FPS);
        return -1;
    }

    if ((pstMovInfo->pstVcodecParam->vcodecType == MEDIA_CODEC_VIDEO_H264) || (pstMovInfo->pstVcodecParam->vcodecType == MEDIA_CODEC_VIDEO_H265))
    {
        int offset = 0;
        uint64_t duration = 0;
        int writeIndex = pstMovInfo->stVTrackFrameIndex % REC_MEDIA_BUF_FPS;
        int writeKeyIndex = pstMovInfo->stVTrackKeyFrameIndex % REC_MEDIA_BUF_FPS;

        if (pstMovInfo->stVFramePts == 0)
        {
            if (pstMovInfo->pstVcodecParam->framerate != 0)
            {
                duration = REC_MSEC_TO_PTS(1000 / pstMovInfo->pstVcodecParam->framerate);
            }
            else
            {
                duration = REC_MSEC_TO_PTS(1000 / 15);
            }
        }
        else
        {
            duration = pFrameInfo->frameParam.framePts - pstMovInfo->stVFramePts;
            duration = REC_MSEC_TO_PTS(duration);
            if (duration > REC_MOV_PRE_SEC)
            {
                __ERR("Invalid over dua %llu,pts:%llu,%llu\n", duration, pFrameInfo->frameParam.framePts, pstMovInfo->stVFramePts);
                duration = REC_MSEC_TO_PTS(1000 / pstMovInfo->pstVcodecParam->framerate);
            }
        }

        pstMovInfo->stVTrackFrameIndex++;
        /* update stts */
        offset = MOV_BOX_MOOV_STBL_STTS_SIZE(writeIndex);
        hton_set_u32(&pstMovInfo->stVTrackSttsBuf[offset], 1); /* percount */
        offset += 4;
        hton_set_u32(&pstMovInfo->stVTrackSttsBuf[offset], duration); /* duration */

        if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
        {
            pstMovInfo->stVTrackKeyFrameIndex++;
            if (!pstMovInfo->bSaveStsdInfo)
            {
                rec_mov_update_keyInfoBuf(pFrameInfo, pstMovInfo->stMovVtrak_stsd_info, pstMovInfo->stMovVtrak_stsd_infoLen, pstMovInfo->pstVcodecParam->vcodecType);
                pstMovInfo->bSaveStsdInfo = 1;
            }

            /* upte stss */
            offset = MOV_BOX_MOOV_STBL_STSS_SIZE(writeKeyIndex);
            hton_set_u32(&pstMovInfo->stVTrackStssBuf[offset], pstMovInfo->stVTrackFrameIndex); /* key index */
        }

        /* upte stsc */
        offset = MOV_BOX_MOOV_STBL_STSC_SIZE(writeIndex);
        hton_set_u32(&pstMovInfo->stVTrackStscBuf[offset], pstMovInfo->stVTrackFrameIndex); /* first_chunk */
        offset += 4;
        hton_set_u32(&pstMovInfo->stVTrackStscBuf[offset], 1); /* samples_per_chunk */
        offset += 4;
        hton_set_u32(&pstMovInfo->stVTrackStscBuf[offset], 1); /* samples_description_id */
        offset += 4;

        /* upte stsz */
        offset = MOV_BOX_MOOV_STBL_STSZ_SIZE(writeIndex);
        hton_set_u32(&pstMovInfo->stVTrackStszBuf[offset], pFrameInfo->frameParam.frameLen); /* percount size*/

        /* upte stco */
        offset = MOV_BOX_MOOV_STBL_STCO_SIZE(writeIndex);
        hton_set_u32(&pstMovInfo->stVTrackStcoBuf[offset], pstMovInfo->stMediaFileOffset); /* percount pos*/

        if (pstMovInfo->stRecWriteParam.iFileOffset == 0)
        {
            fseek(pstMovInfo->stMovfp, pstMovInfo->stMediaFileOffset, SEEK_SET);
            pstMovInfo->stRecWriteParam.iFileOffset = pstMovInfo->stMediaFileOffset;
        }
        pstMovInfo->stMediaFileOffset += pFrameInfo->frameParam.frameLen;

        rec_mov_startcode_to_size(pFrameInfo, pstMovInfo->pstVcodecParam->vcodecType);
        pstMovInfo->vtime_duration += duration;

        /* write data */
        iRet = rec_mov_write_buffer(&pstMovInfo->stRecWriteParam, pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, pstMovInfo->stMovfp);
        if (iRet == -1)
        {
            __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), pFrameInfo->frameParam.frameLen);
            return -1;
        }
        else
        {
            //__INFO("write frame %d, %d %d ok\n", pstMovInfo->stMediaFileOffset, pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.frameType);
        }
        pstMovInfo->stVFramePts = pFrameInfo->frameParam.framePts;
        if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
        {
            pstMovInfo->stLastVKeyFrameNo = pstMovInfo->stVTrackFrameIndex;
            pstMovInfo->stLastVKeyIndex = pstMovInfo->stVTrackKeyFrameIndex;
            pstMovInfo->stLastVKeyPts = pFrameInfo->frameParam.framePts;
            pstMovInfo->stLastVKeyTime = pFrameInfo->frameParam.frameTime;
            pstMovInfo->stLastAFrameNo = pstMovInfo->stATrackFrameIndex;
        }
        iRet = 0;
    }
    else
    {
        __ERR("Unsupport type %d\n", pstMovInfo->pstVcodecParam->vcodecType);
    }
    return iRet;
}

int rec_mov_write_box_moov_atrak_write_frame(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    if (pstMovInfo->stMediaFileOffset < MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE)
    {
        pstMovInfo->stMediaFileOffset = MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE;
        __ERR("Invalid media offset %d\n", pstMovInfo->stMediaFileOffset);
    }

    if (pstMovInfo->stMediaFileOffset + pFrameInfo->frameParam.frameLen >= REC_MEDIA_FILE_SIZE)
    {
        __ERR("Skip frame, oversize %d+%d >= %d\n", pstMovInfo->stMediaFileOffset,
              pFrameInfo->frameParam.frameLen, REC_MEDIA_FILE_SIZE);
        return -1;
    }

    if ((pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_G711A) ||
        (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_G711U) ||
        (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_PCM) ||
        (pstMovInfo->pstAcodecParam->acodecType == MEDIA_CODEC_AUDIO_AAC))
    {
        if (pstMovInfo->stVTrackFrameIndex == 0)
        {
            __ERR("new file unsupport audio frame first\n");
            iRet = -2;
        }
        else
        {
            int offset = 0;
            uint64_t duration = 0;
            int writeIndex = pstMovInfo->stATrackFrameIndex % REC_MEDIA_BUF_FPS;

            if (pstMovInfo->stAFramePts == 0 || pFrameInfo->frameParam.framePts < pstMovInfo->stAFramePts)
            {
                duration = REC_MOV_PRE_MSEC / 25;
                __ERR("Invalid over dua %llu,pts:%llu,%llu\n", duration, pFrameInfo->frameParam.framePts, pstMovInfo->stAFramePts);
            }
            else
            {
                duration = (pFrameInfo->frameParam.framePts - pstMovInfo->stAFramePts);
            }
            if (duration > 1000)
            {
                __ERR("Invalid over dua %llu,pts:%llu,%llu\n", duration, pFrameInfo->frameParam.framePts, pstMovInfo->stAFramePts);
                duration = REC_MOV_PRE_MSEC / 25;
            }

            pstMovInfo->stATrackFrameIndex++;
            /* update stts */
            offset = MOV_BOX_MOOV_STBL_STTS_SIZE(writeIndex);
            hton_set_u32(&pstMovInfo->stATrackSttsBuf[offset], 1); /* percount */
            offset += 4;
            hton_set_u32(&pstMovInfo->stATrackSttsBuf[offset], duration); /* duration */

            /* upte stsc */
            offset = MOV_BOX_MOOV_STBL_STSC_SIZE(writeIndex);
            hton_set_u32(&pstMovInfo->stATrackStscBuf[offset], pstMovInfo->stATrackFrameIndex); /* first_chunk */
            offset += 4;
            hton_set_u32(&pstMovInfo->stATrackStscBuf[offset], 1); /* samples_per_chunk */
            offset += 4;
            hton_set_u32(&pstMovInfo->stATrackStscBuf[offset], 1); /* samples_description_id */
            offset += 4;

            /* upte stsz */
            offset = MOV_BOX_MOOV_STBL_STSZ_SIZE(writeIndex);
            hton_set_u32(&pstMovInfo->stATrackStszBuf[offset], pFrameInfo->frameParam.frameLen); /* percount */

            /* upte stco */
            offset = MOV_BOX_MOOV_STBL_STCO_SIZE(writeIndex);
            hton_set_u32(&pstMovInfo->stATrackStcoBuf[offset], pstMovInfo->stMediaFileOffset); /* percount */

            if (pstMovInfo->stRecWriteParam.iFileOffset == 0)
            {
                fseek(pstMovInfo->stMovfp, pstMovInfo->stMediaFileOffset, SEEK_SET);
                pstMovInfo->stRecWriteParam.iFileOffset = pstMovInfo->stMediaFileOffset;
            }
            pstMovInfo->stMediaFileOffset += pFrameInfo->frameParam.frameLen;
            pstMovInfo->atime_duration += duration;

            /* write data */
            iRet = rec_mov_write_buffer(&pstMovInfo->stRecWriteParam, pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, pstMovInfo->stMovfp);
            if (iRet == -1)
            {
                __ERR("write file failed, ret %d, err: %s. size:%d\n", iRet, strerror(errno), pFrameInfo->frameParam.frameLen);
                return -1;
            }
            else
            {
                //__INFO("write frame %d, %d %d ok\n", pstMovInfo->stMediaFileOffset, pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.frameType);
            }
            pstMovInfo->stAFramePts = pFrameInfo->frameParam.framePts;

            iRet = 0;
        }
    }
    else
    {
        __ERR("Unsupport type %d\n", pstMovInfo->pstAcodecParam->acodecType);
    }

    return iRet;
}

int rec_mov_write_box_write_update_index(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo, unsigned int tEvent, unsigned int segChangTime)
{
    int iRet = 0;
    int iSegIndex = 0;
    int bNewSegMent = 0;
    int bEventSlice = 0;

    if (pFrameInfo->frameParam.frameType == MEDIA_AFRAME_A)
    {
        // 音频只更新片段中音频参数
        if (0 == pstMovInfo->pstIndexRecord->iMediaFileSegRecNums)
        {
            iSegIndex = 0;
        }
        else
        {
            iSegIndex = pstMovInfo->pstIndexRecord->iMediaFileSegRecNums - 1;
        }
        if (0 == pstMovInfo->stIndexSegment[iSegIndex].iAFrameStartNo)
        {
            pstMovInfo->stIndexSegment[iSegIndex].iAFrameStartNo = pstMovInfo->stATrackFrameIndex;
        }
        pstMovInfo->stIndexSegment[iSegIndex].iAFrameNum++;
        if (0 == pstMovInfo->stIndexSegment[iSegIndex].iAFrameStartNo)
        {
            __ERR("segment(%d) first keyframe Index %d\n", iSegIndex, pstMovInfo->stATrackFrameIndex);
        }
    }
    else
    {
        if (0 == pstMovInfo->pstIndexRecord->iMediaFileSegRecNums)
        {
            iSegIndex = 0;
            bNewSegMent = 1;
            pstMovInfo->pstIndexRecord->iMediaFileStatus = REC_STATUS_WRITE;
        }
        else
        {
            iSegIndex = pstMovInfo->pstIndexRecord->iMediaFileSegRecNums - 1;
            // 切换片段都需要视频帧开启
            // 结束旧片段，写入新片段
            if ((tEvent != pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEvent) ||
                (pFrameInfo->frameParam.frameTime < pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEndTime) ||
                /* 跨自然日强制切段，避免长包络段影响按天/月查询准确性 */
                ((pFrameInfo->frameParam.frameTime / REC_SECONDS_PER_DAY) !=
                 (pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEndTime / REC_SECONDS_PER_DAY)) ||
                (pFrameInfo->frameParam.frameTime > pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEndTime + segChangTime) ||
                (pstMovInfo->stIndexSegment[iSegIndex].iVFrameNum >= REC_MEDIA_MAX_FPS))
            {
                if ((tEvent != pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEvent) &&
                    ((tEvent & ~REC_EVENT_NONE) != 0))
                {
                    bEventSlice = 1;
                }
                // 超过最大片段后无法新增片段
                if (pstMovInfo->pstIndexRecord->iMediaFileSegRecNums < REC_MEDIA_INDEX_MAX_SEGMENT)
                {
                    __INFO("(%s)New segment %d\n", pstMovInfo->iWriteFileName, pstMovInfo->pstIndexRecord->iMediaFileSegRecNums);
                    // rec_mov_index_file_segment_show(&pstMovInfo->stIndexSegment[iSegIndex]);
                    bNewSegMent = 1;
                    iSegIndex++;
                    pstMovInfo->stIndexSegment[iSegIndex].iMediaFileStatus = REC_STATUS_FULL;
                }
                else
                {
                    __INFO("(%s)Not segment %d\n", pstMovInfo->iWriteFileName, pstMovInfo->pstIndexRecord->iMediaFileSegRecNums);
                    rec_mov_index_file_segment_show(&pstMovInfo->stIndexSegment[iSegIndex]);
                }
            }
        }

        if (bNewSegMent)
        {
            unsigned int iSegmentStartFrameNo = pstMovInfo->stVTrackFrameIndex;
            unsigned int iSegmentStartKeyIndex = 0;
            uint64_t tSegmentStartPts = pFrameInfo->frameParam.framePts;
            unsigned int tSegmentStartTime = pFrameInfo->frameParam.frameTime;
            unsigned int iSegmentVFrameNum = 0;
            unsigned int iSegmentVKeyFrameNum = 0;
            unsigned int iSegmentAFrameStartNo = 0;
            unsigned int iSegmentAFrameNum = 0;

            if (bEventSlice &&
                (pFrameInfo->frameParam.frameType != MEDIA_VFRAME_I) &&
                (pstMovInfo->stLastVKeyFrameNo > 0) &&
                (pstMovInfo->stLastVKeyIndex > 0) &&
                (pstMovInfo->stLastVKeyFrameNo <= pstMovInfo->stVTrackFrameIndex))
            {
                iSegmentStartFrameNo = pstMovInfo->stLastVKeyFrameNo;
                iSegmentStartKeyIndex = pstMovInfo->stLastVKeyIndex;
                tSegmentStartPts = pstMovInfo->stLastVKeyPts;
                tSegmentStartTime = pstMovInfo->stLastVKeyTime;
                iSegmentVFrameNum = pstMovInfo->stVTrackFrameIndex - pstMovInfo->stLastVKeyFrameNo;
                iSegmentVKeyFrameNum = 1;
                if ((pstMovInfo->stLastAFrameNo > 0) &&
                    (pstMovInfo->stLastAFrameNo <= pstMovInfo->stATrackFrameIndex))
                {
                    iSegmentAFrameStartNo = pstMovInfo->stLastAFrameNo;
                    iSegmentAFrameNum = pstMovInfo->stATrackFrameIndex - pstMovInfo->stLastAFrameNo;
                }
            }

            memset(&pstMovInfo->stIndexSegment[iSegIndex], 0, sizeof(pstMovInfo->stIndexSegment[iSegIndex]));
            pstMovInfo->stIndexSegment[iSegIndex].iMediaFileStatus = REC_STATUS_WRITE;
            pstMovInfo->stIndexSegment[iSegIndex].iVFrameStartNo = iSegmentStartFrameNo;
            pstMovInfo->stIndexSegment[iSegIndex].iVFrameNum = iSegmentVFrameNum;
            pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameStartNo = iSegmentStartKeyIndex;
            pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameNum = iSegmentVKeyFrameNum;
            pstMovInfo->stIndexSegment[iSegIndex].iAFrameStartNo = iSegmentAFrameStartNo;
            pstMovInfo->stIndexSegment[iSegIndex].iAFrameNum = iSegmentAFrameNum;
            pstMovInfo->stIndexSegment[iSegIndex].tMediaFileBeginPts = tSegmentStartPts;
            pstMovInfo->stIndexSegment[iSegIndex].tMediaFileBeginTime = tSegmentStartTime;
            pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEvent = tEvent;
            pstMovInfo->stIndexSegment[iSegIndex].iMediaFileCh = pstMovInfo->pstIndexRecord->iMediaFileCh;

            if ((0 == pstMovInfo->pstIndexRecord->tMediaFileBeginTime) ||
                (tSegmentStartTime < pstMovInfo->pstIndexRecord->tMediaFileBeginTime))
            {
                pstMovInfo->pstIndexRecord->tMediaFileBeginTime = tSegmentStartTime;
            }
            pstMovInfo->pstIndexRecord->iMediaFileSegRecNums++;
        }

        pstMovInfo->stIndexSegment[iSegIndex].iVFrameNum++;
        if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
        {
            if (0 == pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameStartNo)
            {
                pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameStartNo = pstMovInfo->stVTrackKeyFrameIndex;
            }

            if (0 == pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameStartNo)
            {
                __ERR("segment(%d) first keyframe Index %d\n", iSegIndex, pstMovInfo->stVTrackKeyFrameIndex);
            }

            pstMovInfo->stIndexSegment[iSegIndex].iVKeyFrameNum++;
        }

        pstMovInfo->stIndexSegment[iSegIndex].tMediaFileEndTime = pFrameInfo->frameParam.frameTime;

        if ((0 == pstMovInfo->pstIndexRecord->tMediaFileEndTime) ||
            (pFrameInfo->frameParam.frameTime > pstMovInfo->pstIndexRecord->tMediaFileEndTime))
        {
            pstMovInfo->pstIndexRecord->tMediaFileEndTime = pFrameInfo->frameParam.frameTime;
        }
        if (REC_STATUS_NULL == pstMovInfo->pstIndexRecord->iMediaFileStatus)
        {
            pstMovInfo->pstIndexRecord->iMediaFileStatus = REC_STATUS_WRITE;
        }
    }

    if (((pstMovInfo->stVTrackFrameIndex > 0) && (pstMovInfo->stVTrackFrameIndex % REC_MEDIA_BUF_FPS == 0)) ||
        ((pstMovInfo->stATrackFrameIndex > 0) && (pstMovInfo->stATrackFrameIndex % REC_MEDIA_BUF_FPS == 0)) ||
        ((pstMovInfo->stVTrackKeyFrameIndex > 0) && (pstMovInfo->stVTrackKeyFrameIndex % REC_MEDIA_BUF_FPS == 0)))
    {
        // 分片最后一片保存更新数据
        iRet = rec_mov_write_box_moov_trak_update(pstMovInfo, 1);
        if (0 != iRet)
        {
            iRet = rec_mov_write_box_moov_trak_update(pstMovInfo, 1);
        }
        if (0 == iRet)
        {
            iRet = 1;
        }
    }
    // 每1024视频帧清空一次缓冲,不写入缓冲回放读无法获取
    // if (pstMovInfo->stVTrackFrameIndex % REC_FILE_FFLUSH_FPS)
    // {
    //     fflush(pstMovInfo->stMovfp);
    // }
    return iRet;
}

/* 切换文件逻辑：1.入帧需要为I帧 2.余量一点帧数量及容量时,因为MP4首帧需要I帧 */
/* 切换存索引逻辑：写满片段时更新索引 */
int rec_mov_write_box_write_file_check(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo)
{
    int iRet = 0;
    if ((pstMovInfo->stMediaFileOffset + pFrameInfo->frameParam.frameLen >= REC_MEDIA_FILE_SIZE) ||
        (pstMovInfo->stVTrackFrameIndex >= REC_MEDIA_MAX_FPS) ||
        (pstMovInfo->stATrackFrameIndex >= REC_MEDIA_MAX_FPS) || access("/tmp/nextr", F_OK) == 0)
    {
        remove("/tmp/nextr");
        iRet = 1;
        __ERR("Change file(%s) for full index:(%d,%d),offset:%d, frameType:%d, len:%d\n", pstMovInfo->iWriteFileName,
              pstMovInfo->stVTrackFrameIndex, pstMovInfo->stATrackFrameIndex,
              pstMovInfo->stMediaFileOffset, +pFrameInfo->frameParam.frameType, +pFrameInfo->frameParam.frameLen);
        goto endFunc;
    }

    if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
    {
        if (pstMovInfo->stMediaFileOffset + pFrameInfo->frameParam.frameLen >= REC_MEDIA_FILE_SIZE - REC_FILE_REMAIN_SIZE)
        {
            iRet = 1;
            __INFO("Change file(%s) for len index:(%d,%d),offset:%d, frameType:%d, len:%d\n", pstMovInfo->iWriteFileName,
                   pstMovInfo->stVTrackFrameIndex, pstMovInfo->stATrackFrameIndex,
                   pstMovInfo->stMediaFileOffset, +pFrameInfo->frameParam.frameType, +pFrameInfo->frameParam.frameLen);
            goto endFunc;
        }

        if ((pstMovInfo->stVTrackFrameIndex >= REC_MEDIA_MAX_FPS - REC_FILE_REMAIN_FPS) || (pstMovInfo->stATrackFrameIndex >= REC_MEDIA_MAX_FPS - REC_FILE_REMAIN_FPS) ||
            pstMovInfo->pstIndexRecord->iMediaFileSegRecNums >= (REC_MEDIA_INDEX_MAX_SEGMENT - 1))
        {
            iRet = 1;
            __INFO("Change file(%s) for frames index:(%d,%d),offset:%d, frameType:%d, len:%d\n", pstMovInfo->iWriteFileName,
                   pstMovInfo->stVTrackFrameIndex, pstMovInfo->stATrackFrameIndex,
                   pstMovInfo->stMediaFileOffset, +pFrameInfo->frameParam.frameType, +pFrameInfo->frameParam.frameLen);
            goto endFunc;
        }
    }

endFunc:
    return iRet;
}

rec_mov_info_t *rec_mov_write_box_create(const char *filename, rec_file_index_record *pstIndexRecord, rec_media_vcodec_param_t *pstVcodecParam, rec_media_acodec_param_t *pstAcodecParam)
{
    rec_mov_info_t *pstMovInfo = NULL;
    if ((NULL == filename) || (NULL == pstVcodecParam) || (NULL == pstAcodecParam) || (NULL == pstIndexRecord))
    {
        __ERR("Invalid Input NULL\n");
        return NULL;
    }
    pstMovInfo = (rec_mov_info_t *)anj_mw_malloc(sizeof(rec_mov_info_t));
    if (pstMovInfo)
    {
        memset(pstMovInfo, 0, sizeof(rec_mov_info_t));
        __INFO("mov(%s:%p) info size %d, %d, %d, (%d,%d, %d, %d)(%d, %d, %d,%d)(%d,%d,%d,%d,%d)\n", filename, pstMovInfo,
               MOV_BOX_MOOV_VTRACK_BUF_SIZE, MOV_BOX_MOOV_ATRACK_BUF_SIZE, sizeof(rec_mov_info_t),
               MOV_BOX_FTYP_SIZE, MOV_BOX_MDAT_SIZE, MOV_BOX_FREE_SIZE, MOV_BOX_MOOV_SIZE,
               MOV_BOX_MOOV_MVHD_SIZE, MOV_BOX_MOOV_VTRAK_SIZE, MOV_BOX_MOOV_ATRAK_SIZE, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_SIZE,
               MOV_BOX_FREE_START_POS, MOV_BOX_MOOV_STSRT_POS, MOV_BOX_MOOV_VTRAK_START_POS,
               MOV_BOX_MOOV_ATRAK_START_POS, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_START_POS);

        pstMovInfo->stRecWriteParam.ptsDataBuf = anj_mw_malloc(REC_MEDIA_WRITE_BUFFER);
        if (pstMovInfo->stRecWriteParam.ptsDataBuf == NULL)
        {
            __ERR("malloc failed!\n");
            anj_mw_free(pstMovInfo);
            return NULL;
        }
        memset(pstMovInfo->stRecWriteParam.ptsDataBuf, 0, REC_MEDIA_WRITE_BUFFER);

        pstMovInfo->pstVcodecParam = pstVcodecParam;
        pstMovInfo->pstAcodecParam = pstAcodecParam;
        pstMovInfo->pstIndexRecord = pstIndexRecord;
        pstMovInfo->time_create = time(NULL) + 0x7C25B080; // 1970 based -> 1904 based;

        pstMovInfo->stMovfp = fopen(filename, "rb+");
        if (NULL == pstMovInfo->stMovfp)
        {
            __ERR("fopen err %s\n", filename);
            anj_mw_free(pstMovInfo->stRecWriteParam.ptsDataBuf);
            pstMovInfo->stRecWriteParam.ptsDataBuf = NULL;
            anj_mw_free(pstMovInfo);
            pstMovInfo = NULL;
            return NULL;
        }
        strncpy(pstMovInfo->iWriteFileName, filename, sizeof(pstMovInfo->iWriteFileName) - 1);

        rec_mov_write_box_ftyp_save(pstMovInfo);
        rec_mov_write_box_moov_mvhd(pstMovInfo);
        rec_mov_write_box_moov_vtrack(pstMovInfo);
        rec_mov_write_box_moov_atrack(pstMovInfo);
        rec_mov_write_box_moov_track_save(pstMovInfo);
        rec_mov_write_box_moov_trak_stbl_init_save(pstMovInfo);
        rec_mov_write_box_free_save(pstMovInfo);
        rec_mov_write_box_mdat_save(pstMovInfo);
    }
    else
    {
        __ERR("fopen err %s\n", filename);
    }

    return pstMovInfo;
}

int rec_mov_write_box_destroy(rec_mov_info_t *pstMovInfo, int err)
{
    int iRet = -1;
    if (pstMovInfo)
    {
        if (pstMovInfo->stMovfp)
        {
            if (err == 0)
            {
                iRet = rec_mov_write_box_moov_trak_update(pstMovInfo, 1);
            }
            fclose(pstMovInfo->stMovfp);
            pstMovInfo->stMovfp = NULL;
        }

        if (pstMovInfo->stRecWriteParam.ptsDataBuf)
        {
            anj_mw_free(pstMovInfo->stRecWriteParam.ptsDataBuf);
            pstMovInfo->stRecWriteParam.ptsDataBuf = NULL;
        }

        anj_mw_free(pstMovInfo);
    }

    return iRet;
}

int rec_mov_read_frame_is_valid(rec_mov_frame_info_t *pstFrameinfo)
{
    if (pstFrameinfo)
    {
        if ((pstFrameinfo->iFrameSize <= REC_MAX_FRAME_BUF_SIZE) &&
            (pstFrameinfo->iFrameSize > 4) &&
            (pstFrameinfo->iFrameOffset >= (MOV_BOX_MDAT_START_POS + MOV_BOX_STU_SIZE)) &&
            ((pstFrameinfo->iFrameOffset + pstFrameinfo->iFrameSize) < REC_MEDIA_FILE_SIZE))
        {
            return 1;
        }
    }
    return 0;
}

rec_mov_frame_info_t *rec_mov_read_vframe_info(FILE *fp, rec_mov_read_frame_info_t *pstVFrameinfo, unsigned int iFrameNo)
{
    unsigned int iReadSeek = 0;
    unsigned char iReadBuf[8 * REC_MEDIA_BUF_FPS]; // 最大读取stts 8字节一个
    unsigned int iReadSize = 0;
    unsigned int iRet = 0;

    if (iFrameNo <= 0 || iFrameNo > REC_MEDIA_MAX_FPS)
    {
        __ERR("Invalid Input %d\n", iFrameNo);
        return NULL;
    }
    if ((0 == pstVFrameinfo->iFrameStart) ||
        (iFrameNo < pstVFrameinfo->iFrameStart) ||
        (iFrameNo >= (pstVFrameinfo->iFrameStart + REC_MEDIA_BUF_FPS)))
    {
        int iFrameIndex = 0;
        iFrameIndex = ANJ_ALIGN_DOWN(iFrameNo - 1, REC_MEDIA_BUF_FPS);

        // stsd
        iReadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_START_POS + 20;
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSD_SIZE - 20;
        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstVFrameinfo->iFrameStart = 0;
            return NULL;
        }
        int vcodecType = 0;
        vcodecType = iReadBuf[3];
        vcodecType |= iReadBuf[2] << 8;
        vcodecType |= iReadBuf[1] << 16;
        vcodecType |= iReadBuf[0] << 24;
        if (vcodecType == 0x68657631)
        {
            pstVFrameinfo->stRecVcodecParam.vcodecType = MEDIA_CODEC_VIDEO_H265;
        }
        else
        {
            pstVFrameinfo->stRecVcodecParam.vcodecType = MEDIA_CODEC_VIDEO_H264;
        }

        pstVFrameinfo->stRecVcodecParam.width = iReadBuf[29];
        pstVFrameinfo->stRecVcodecParam.width |= iReadBuf[28] << 8;
        pstVFrameinfo->stRecVcodecParam.height = iReadBuf[31];
        pstVFrameinfo->stRecVcodecParam.height |= iReadBuf[30] << 8;

        // stts
        iReadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STTS_START_POS + MOV_BOX_MOOV_STBL_STTS_SIZE(iFrameIndex);

        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 8 * REC_MEDIA_BUF_FPS; // 4byte percount 4byte duration

        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstVFrameinfo->iFrameStart = 0;
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstVFrameinfo->iFrameInfo[i].iFrameDuration = iReadBuf[8 * i + 7];
            pstVFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 6] << 8;
            pstVFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 5] << 16;
            pstVFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 4] << 24;
        }

        // stsz
        iReadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSZ_START_POS + MOV_BOX_MOOV_STBL_STSZ_SIZE(iFrameIndex);
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 4 * REC_MEDIA_BUF_FPS; // 4byte frame size

        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstVFrameinfo->iFrameStart = 0;
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstVFrameinfo->iFrameInfo[i].iFrameSize = iReadBuf[4 * i + 3];
            pstVFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 2] << 8;
            pstVFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 1] << 16;
            pstVFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 0] << 24;
        }

        // stco
        iReadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STCO_START_POS + MOV_BOX_MOOV_STBL_STCO_SIZE(iFrameIndex);
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 4 * REC_MEDIA_BUF_FPS; // 4byte frame pos

        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            pstVFrameinfo->iFrameStart = 0;
            __ERR("write file failed, ret %d\n", iRet);
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstVFrameinfo->iFrameInfo[i].iFrameOffset = iReadBuf[4 * i + 3];
            pstVFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 2] << 8;
            pstVFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 1] << 16;
            pstVFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 0] << 24;
        }
        pstVFrameinfo->iFrameStart = iFrameIndex + 1;
        if ((iFrameNo < pstVFrameinfo->iFrameStart) ||
            (iFrameNo >= pstVFrameinfo->iFrameStart + REC_MEDIA_BUF_FPS))
        {
            __ERR("Invalid Input %d\n", iFrameNo);
            return NULL;
        }
    }

    return &pstVFrameinfo->iFrameInfo[iFrameNo - pstVFrameinfo->iFrameStart];
}

rec_mov_frame_info_t *rec_mov_read_aframe_info(FILE *fp, rec_mov_read_frame_info_t *pstAFrameinfo, unsigned int iFrameNo)
{
    unsigned int iReadSeek = 0;
    unsigned char iReadBuf[8 * REC_MEDIA_BUF_FPS]; // 最大读取stts 8字节一个
    unsigned int iReadSize = 0;
    unsigned int iRet = 0;

    if ((iFrameNo <= 0) || (iFrameNo > REC_MEDIA_MAX_FPS))
    {
        __ERR("Invalid Input %d\n", iFrameNo);
        return NULL;
    }
    if ((0 == pstAFrameinfo->iFrameStart) ||
        (iFrameNo < pstAFrameinfo->iFrameStart) ||
        (iFrameNo >= (pstAFrameinfo->iFrameStart + REC_MEDIA_BUF_FPS)))
    {
        int iFrameIndex = 0;
        iFrameIndex = ANJ_ALIGN_DOWN(iFrameNo - 1, REC_MEDIA_BUF_FPS);

        // stsd
        iReadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_START_POS + 20;
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSD_SIZE - 20;
        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstAFrameinfo->iFrameStart = 0;
            return NULL;
        }
        int acodecType = 0;
        acodecType = iReadBuf[3];
        acodecType |= iReadBuf[2] << 8;
        acodecType |= iReadBuf[1] << 16;
        acodecType |= iReadBuf[0] << 24;
        if (acodecType == 0x616C6177)
        {
            pstAFrameinfo->stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_G711A;
        }
        else if (acodecType == 0x756C6177)
        {
            pstAFrameinfo->stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_G711U;
        }
        else if (acodecType == 0x6D703461)
        {
            pstAFrameinfo->stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_AAC;
        }
        else
        {
            pstAFrameinfo->stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_PCM;
        }

        pstAFrameinfo->stRecAcodecParam.channels = iReadBuf[21];
        pstAFrameinfo->stRecAcodecParam.channels |= iReadBuf[20] << 8;
        pstAFrameinfo->stRecAcodecParam.bitWidth = iReadBuf[23];
        pstAFrameinfo->stRecAcodecParam.bitWidth |= iReadBuf[22] << 8;
        pstAFrameinfo->stRecAcodecParam.sampleRate = iReadBuf[29];
        pstAFrameinfo->stRecAcodecParam.sampleRate |= iReadBuf[28] << 8;

        // stts
        iReadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STTS_START_POS + MOV_BOX_MOOV_STBL_STTS_SIZE(iFrameIndex);

        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 8 * REC_MEDIA_BUF_FPS;

        /* write data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstAFrameinfo->iFrameStart = 0;
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstAFrameinfo->iFrameInfo[i].iFrameDuration = iReadBuf[8 * i + 7];
            pstAFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 6] << 8;
            pstAFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 5] << 16;
            pstAFrameinfo->iFrameInfo[i].iFrameDuration |= iReadBuf[8 * i + 4] << 24;
        }

        // stsz
        iReadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STSZ_START_POS + MOV_BOX_MOOV_STBL_STSZ_SIZE(iFrameIndex);
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 4 * REC_MEDIA_BUF_FPS;

        /* write data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            pstAFrameinfo->iFrameStart = 0;
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstAFrameinfo->iFrameInfo[i].iFrameSize = iReadBuf[4 * i + 3];
            pstAFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 2] << 8;
            pstAFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 1] << 16;
            pstAFrameinfo->iFrameInfo[i].iFrameSize |= iReadBuf[4 * i + 0] << 24;
        }

        // stco
        iReadSeek = MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_START_POS + MOV_BOX_MOOV_STBL_STCO_SIZE(iFrameIndex);
        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 4 * REC_MEDIA_BUF_FPS;

        /* write data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            pstAFrameinfo->iFrameStart = 0;
            __ERR("write file failed, ret %d\n", iRet);
            return NULL;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstAFrameinfo->iFrameInfo[i].iFrameOffset = iReadBuf[4 * i + 3];
            pstAFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 2] << 8;
            pstAFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 1] << 16;
            pstAFrameinfo->iFrameInfo[i].iFrameOffset |= iReadBuf[4 * i + 0] << 24;
        }
        pstAFrameinfo->iFrameStart = iFrameIndex + 1;
        if ((iFrameNo < pstAFrameinfo->iFrameStart) ||
            (iFrameNo >= (pstAFrameinfo->iFrameStart + REC_MEDIA_BUF_FPS)))
        {
            __ERR("Invalid Input %d\n", iFrameNo);
            return NULL;
        }
    }

    return &pstAFrameinfo->iFrameInfo[iFrameNo - pstAFrameinfo->iFrameStart];
}

int rec_mov_read_keyframe_info(FILE *fp, rec_mov_read_keyframe_info_t *pstKeyFrameinfo, unsigned int iFrameNo)
{
    unsigned int iReadSeek = 0;
    unsigned char iReadBuf[4 * REC_MEDIA_BUF_FPS]; // 最大读取stts 8字节一个
    unsigned int iRet = 0;
    unsigned int iReadSize = 0;
    if (iFrameNo <= 0 || iFrameNo > REC_MEDIA_MAX_FPS)
    {
        __ERR("Invalid Input %d\n", iFrameNo);
        return -1;
    }
    if ((0 == pstKeyFrameinfo->iKeyFrameStart) ||
        (iFrameNo < pstKeyFrameinfo->iKeyFrameStart) ||
        (iFrameNo >= (pstKeyFrameinfo->iKeyFrameStart + REC_MEDIA_BUF_FPS)))
    {
        int iFrameIndex = 0;
        iFrameIndex = ANJ_ALIGN_DOWN(iFrameNo - 1, REC_MEDIA_BUF_FPS);

        // stss
        iReadSeek = MOV_BOX_MOOV_VTRAK_MDIA_MINF_STBL_STSS_START_POS + MOV_BOX_MOOV_STBL_STSS_SIZE(iFrameIndex);

        fseek(fp, iReadSeek, SEEK_SET);
        iReadSize = 4 * REC_MEDIA_BUF_FPS;

        /* read data */
        iRet = anj_mw_fread(fp, iReadBuf, iReadSize);
        if (iRet != iReadSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            return -1;
        }

        for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
        {
            pstKeyFrameinfo->iKeyIndex[i] = iReadBuf[4 * i + 3];
            pstKeyFrameinfo->iKeyIndex[i] |= iReadBuf[4 * i + 2] << 8;
            pstKeyFrameinfo->iKeyIndex[i] |= iReadBuf[4 * i + 1] << 16;
            pstKeyFrameinfo->iKeyIndex[i] |= iReadBuf[4 * i + 0] << 24;
        }
        pstKeyFrameinfo->iKeyFrameStart = iFrameIndex + 1;
        if ((iFrameNo < pstKeyFrameinfo->iKeyFrameStart) ||
            (iFrameNo >= (pstKeyFrameinfo->iKeyFrameStart + REC_MEDIA_BUF_FPS)))
        {
            __ERR("Invalid Input %d\n", iFrameNo);
            return -1;
        }
    }

    return pstKeyFrameinfo->iKeyIndex[iFrameNo - pstKeyFrameinfo->iKeyFrameStart];
}

int rec_mov_read_frame_data(FILE *fp, unsigned char *pData, unsigned int pDataLen, rec_mov_frame_info_t *pstFrameinfo, int *isKeyFlag, int bVideo)
{
    int iRet = -1;
    if (rec_mov_read_frame_is_valid(pstFrameinfo))
    {
        fseek(fp, pstFrameinfo->iFrameOffset, SEEK_SET);

        /* read data */
        iRet = anj_mw_fread(fp, pData, pstFrameinfo->iFrameSize);
        if (iRet != (int)pstFrameinfo->iFrameSize)
        {
            __ERR("write file failed, ret %d\n", iRet);
            return -1;
        }
        else
        {
            if (bVideo == 0)
            {
                return 0;
            }
            unsigned int iReaminSize = pstFrameinfo->iFrameSize;
            unsigned int iFrameSize = 0;
            unsigned int iFramePos = 0;

            do
            {
                iFrameSize = pData[iFramePos + 3];
                iFrameSize |= pData[iFramePos + 2] << 8;
                iFrameSize |= pData[iFramePos + 1] << 16;
                iFrameSize |= pData[iFramePos + 0] << 24;
                pData[iFramePos + 0] = 0x00;
                pData[iFramePos + 1] = 0x00;
                pData[iFramePos + 2] = 0x00;
                pData[iFramePos + 3] = 0x01;
                iReaminSize -= 4;

                if (iFrameSize == iReaminSize)
                {
                    *isKeyFlag = (iFramePos > 0) ? 1 : 0;
                    return 0;
                }
                else if (iFrameSize > iReaminSize)
                {
                    break;
                }
                else
                {
                    // iframe = vps+pps+sps+frame
                    iFramePos += iFrameSize + 4;
                    iReaminSize -= iFrameSize;
                }
            } while (iFramePos < 256 && iReaminSize > 4);

            __ERR("Invalid frame info:%d,%d, pos %d\n", pstFrameinfo->iFrameOffset, pstFrameinfo->iFrameSize, iFramePos);
            return iFramePos;
        }
    }
    else
    {
        __ERR("Invalid frame info:%d,%d\n", pstFrameinfo->iFrameOffset, pstFrameinfo->iFrameSize);
    }

    return -1;
}

rec_mov_info_t *rec_mov_create_mp4(char *filename, rec_media_vcodec_param_t *pstVcodecParam, rec_media_acodec_param_t *pstAcodecParam)
{
    if ((NULL == filename) || (NULL == pstVcodecParam) || (NULL == pstAcodecParam))
    {
        __ERR("Invalid Input NULL");
        return NULL;
    }

    rec_mov_info_t *pstMovInfo = NULL;
    pstMovInfo = (rec_mov_info_t *)malloc(sizeof(rec_mov_info_t));
    if (pstMovInfo)
    {
        memset(pstMovInfo, 0, sizeof(rec_mov_info_t));
        __INFO("mov(%s:%p) info size %d, %d, %ld, (%d,%d, %d, %d)(%d, %d, %d,%d)(%d,%d,%d,%d,%d)\n", filename, pstMovInfo,
               MOV_BOX_MOOV_VTRACK_BUF_SIZE, MOV_BOX_MOOV_ATRACK_BUF_SIZE, sizeof(rec_mov_info_t),
               MOV_BOX_FTYP_SIZE, MOV_BOX_MDAT_SIZE, MOV_BOX_FREE_SIZE, MOV_BOX_MOOV_SIZE,
               MOV_BOX_MOOV_MVHD_SIZE, MOV_BOX_MOOV_VTRAK_SIZE, MOV_BOX_MOOV_ATRAK_SIZE, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_SIZE,
               MOV_BOX_FREE_START_POS, MOV_BOX_MOOV_STSRT_POS, MOV_BOX_MOOV_VTRAK_START_POS,
               MOV_BOX_MOOV_ATRAK_START_POS, MOV_BOX_MOOV_ATRAK_MDIA_MINF_STBL_STCO_START_POS);

        pstMovInfo->stRecWriteParam.ptsDataBuf = malloc(REC_MEDIA_WRITE_BUFFER);
        if (pstMovInfo->stRecWriteParam.ptsDataBuf == NULL)
        {
            __ERR("malloc failed!\n");
            free(pstMovInfo);
            return NULL;
        }
        memset(pstMovInfo->stRecWriteParam.ptsDataBuf, 0, REC_MEDIA_WRITE_BUFFER);

        pstMovInfo->pstVcodecParam = pstVcodecParam;
        pstMovInfo->pstAcodecParam = pstAcodecParam;
        pstMovInfo->time_create = time(NULL) + 0x7C25B080; // 1970 based -> 1904 based;

        pstMovInfo->stMovfp = fopen(filename, "w");
        if (NULL == pstMovInfo->stMovfp)
        {
            __ERR("fopen err %s\n", filename);
            if (pstMovInfo->stRecWriteParam.ptsDataBuf)
            {
                free(pstMovInfo->stRecWriteParam.ptsDataBuf);
            }
            free(pstMovInfo);
            pstMovInfo = NULL;
            return NULL;
        }
        strncpy(pstMovInfo->iWriteFileName, filename, sizeof(pstMovInfo->iWriteFileName) - 1);

        rec_mov_write_box_ftyp_save(pstMovInfo);
        rec_mov_write_box_moov_mvhd(pstMovInfo);
        rec_mov_write_box_moov_vtrack(pstMovInfo);
        rec_mov_write_box_moov_atrack(pstMovInfo);
        rec_mov_write_box_moov_track_save(pstMovInfo);
        rec_mov_write_box_moov_trak_stbl_init_save(pstMovInfo);
        rec_mov_write_box_free_save(pstMovInfo);
        rec_mov_write_box_mdat_save(pstMovInfo);
    }
    else
    {
        __ERR("fopen err %s\n", filename);
        return NULL;
    }
    return pstMovInfo;
}

int rec_mov_write_mp4(media_frame_info_t *pFrameInfo, rec_mov_info_t *pstMovInfo)
{
    int iRet = -1;
    if ((pstMovInfo == NULL) || (NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) ||
        (0 >= pFrameInfo->frameParam.frameLen) || (REC_MAX_FRAME_BUF_SIZE < pFrameInfo->frameParam.frameLen))
    {
        __ERR("Invalid Input Frame\n");
        goto endFunc;
    }

    if ((pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_PCM) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_G711A) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_AAC) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_G711U))
    {
        if (0 != rec_mov_write_box_moov_atrak_write_frame(pstMovInfo, pFrameInfo))
        {
            __ERR("write file err\n");
            goto endFunc;
        }
    }
    else if ((pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_VIDEO_H264) || (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_VIDEO_H265))
    {
        if (0 != rec_mov_write_box_moov_vtrak_write_frame(pstMovInfo, pFrameInfo))
        {
            __ERR("write file err\n");
            goto endFunc;
        }
    }
    else
    {
        __ERR("Unsupport type %d\n", pFrameInfo->frameParam.frameCodec);
        goto endFunc;
    }

    if (((pstMovInfo->stVTrackFrameIndex > 0) && (pstMovInfo->stVTrackFrameIndex % REC_MEDIA_BUF_FPS == 0)) ||
        ((pstMovInfo->stATrackFrameIndex > 0) && (pstMovInfo->stATrackFrameIndex % REC_MEDIA_BUF_FPS == 0)) ||
        ((pstMovInfo->stVTrackKeyFrameIndex > 0) && (pstMovInfo->stVTrackKeyFrameIndex % REC_MEDIA_BUF_FPS == 0)))
    {
        iRet = rec_mov_write_box_moov_trak_update(pstMovInfo, 1);
    }

endFunc:
    return iRet;
}

int rec_mov_close_mp4(rec_mov_info_t *pstMovInfo)
{
    if (pstMovInfo == NULL)
    {
        __ERR("Invalid Input Frame\n");
        return -1;
    }

    if (0 != rec_mov_write_box_moov_trak_update(pstMovInfo, 1))
    {
        __ERR("check file err\n");
    }

    if (pstMovInfo->stMovfp)
    {
        if (0 != rec_mov_write_box_mdat_save(pstMovInfo))
        {
            __ERR("update mdat size failed\n");
        }
        fflush(pstMovInfo->stMovfp);
        fclose(pstMovInfo->stMovfp);
        pstMovInfo->stMovfp = NULL;
    }
    if(pstMovInfo->stRecWriteParam.ptsDataBuf)
    {
        free(pstMovInfo->stRecWriteParam.ptsDataBuf);
    }
    free(pstMovInfo);

    return 0;
}