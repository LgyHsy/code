#include <stddef.h>
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

#include "anj_mw_file.h"
#include "anj_mw_errcode.h"
#include "rec_mov_write.h"
#include "rec_mov_index.h"
#include "anj_config.h"
#include "anj_osd.h"
#include "anj_sdcard.h"
#include "anj_record.h"
#include "record_log.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_mbuf.h"
#include "alarm_link.h"

#define SUPPORT_FALLOCATE (1)
#define REC_BASE_READERID (0x00CD0010)

#define REC_PB_CACHE_REFRESH_SEC (60)
#define ANJ_RECORD_PB_SEG_ERR_OPEN (-2)

#define HIDWORD(a) ((unsigned int)(((unsigned long long int)(a)) >> 32))
#define LODWORD(a) ((unsigned int)(unsigned long long int)(a))

#define ROUND_DIVIDE(dividend, divisor) (((dividend) + (divisor) / 2) / (divisor))

#define REC_TIME_MIN (60)
#define REC_TIMS_HOUR (60 * REC_TIME_MIN)
#define REC_TIME_DAY (24 * REC_TIMS_HOUR)
#define REC_ROOT_SUBDIR "record"
#define REC_PRE_ALLOC_FILES_PER_DIR (1024)
#define REC_PRE_ALLOC_DIR_NAME_FMT "%s/pre%03d"
#define REC_MEDIA_FILE_NAME_BY_DIR "%s/pre%03d/anj%06d.mp4"

// #define _WRITE_FILE_ONE_FD_
#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE 0x01
#endif

#define fallocate(fd, mode, offset, len) syscall(__NR_fallocate, fd, mode, LODWORD(offset), HIDWORD(offset), LODWORD(len), HIDWORD(len))

typedef struct
{
    int bEnable;
    int iRecChn;
    int iRecVenc;
} rec_chn;

typedef struct
{
    int bInit;
    int bStop;
    anj_thread_s stRecThread[REC_MAX_CH_NUM];
    rec_status_e iRecStatus;
    int iformatProcess; // 格式化完成度
    int iMaxPartition;  // 最大分区数
    unsigned int iPartitionMaxFiles[SDCARD_MAX_PARTITION];

    int bRecStopFlag[REC_MAX_CH_NUM];
    unsigned int tRecEvent[REC_MAX_CH_NUM];
    rec_mov_info_t *pstRecMovInfo[REC_MAX_CH_NUM];
    rec_file_index_param stRecIndexParam;
    char stRecfilePath[64];
    rec_chn chn_param[REC_MAX_CH_NUM];
    int iRecReaderId;
    unsigned long long tAlarmTmSec[REC_EVENT_MODE_MAX];
    unsigned int segChangTime;
} rec_param;

typedef struct
{
    void *pHandle;
    int timelapse;
    unsigned long long firstPts;
    rec_mov_info_t *pstMovInfo;
    rec_media_vcodec_param_t stRecVcodecParam;
    rec_media_acodec_param_t stRecAcodecParam;
    unsigned int tEndTime;
    char stRecfilePath[256];
} rec_pb_download_param;

rec_pb_poper s_stRecPbPoper[REC_MAX_PB_NUM];

static rec_param s_stRecParam;
static rec_media_vcodec_param_t s_stRecVcodecParam[REC_MAX_CH_NUM];
static rec_media_acodec_param_t s_stRecAcodecParam;
static pthread_mutex_t s_stRecMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stPbProcMutex = PTHREAD_MUTEX_INITIALIZER;
static rec_pb_cache_param s_stPbCache = {.mutex = PTHREAD_MUTEX_INITIALIZER};
static rec_pb_download_param s_stDownLoadParam = {0};
static int s_iRecUniniting = 0;
static unsigned char s_pendingBadBlockMap[REC_MEDIA_BAD_BLOCK_MAP_SIZE];
static OverlayTextEnum s_recordOsdBmp = OVERLAY_RECORD_BMP;

static void anj_record_osd_set(int bShow)
{
    osd_custom_content_s osdBmpCustom = {0};
    osdBmpCustom.custom_show = bShow;
    osdBmpCustom.overlayText = s_recordOsdBmp;
    osdBmpCustom.custom_x = 99;
    osdBmpCustom.custom_y = 99;
    osdBmpCustom.custom_location = POSITION_TYPE_BY_SCALE;
    anj_osd_bmp_set(&osdBmpCustom);
}

/* 优先级: 文件不完整(BMP0) > CRC异常卡(BMP2) > 坏块(BMP1) > 正常 */
static void anj_record_osd_apply_health(int bFileIncomplete, int bCrcAbnormal, int bHasBadBlock)
{
    OverlayTextEnum newBmp = OVERLAY_RECORD_BMP;
    int i = 0;
    int bRecording = 0;

    if (bFileIncomplete)
    {
        newBmp = OVERLAY_RECORD_BMP0;
    }
    else if (bCrcAbnormal)
    {
        newBmp = OVERLAY_RECORD_BMP2;
    }
    else if (bHasBadBlock)
    {
        newBmp = OVERLAY_RECORD_BMP1;
    }

    if (newBmp == s_recordOsdBmp)
    {
        return;
    }

    __WARN("record osd health change %d->%d incomplete:%d crcAbn:%d bad:%d\n",
           s_recordOsdBmp, newBmp, bFileIncomplete, bCrcAbnormal, bHasBadBlock);
    s_recordOsdBmp = newBmp;

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stRecParam.chn_param[i].bEnable && (0 == s_stRecParam.bRecStopFlag[i]))
        {
            bRecording = 1;
            break;
        }
    }
    if (bRecording)
    {
        anj_record_osd_set(1);
    }
}

#if 0
static int anj_record_time_get_str(char *pTimeBuf, int pTimeBufLen, time_t iTime)
{
    int iRet = -1;
    struct tm stTmNow;
    if ((NULL == pTimeBuf) || (0 >= pTimeBufLen))
    {
        __ERR("Invalid Input\n");
        goto endFunc;
    }
    memset(&stTmNow, 0, sizeof(stTmNow));
    localtime_r(&iTime, &stTmNow);

    snprintf(pTimeBuf, pTimeBufLen, "%02d%s%02d%s%02d %02d:%02d:%02d",
             stTmNow.tm_year + 1900, "-", stTmNow.tm_mon + 1, "-", stTmNow.tm_mday,
             stTmNow.tm_hour, stTmNow.tm_min, stTmNow.tm_sec);
    iRet = 0;
endFunc:
    return iRet;
}
#endif

static int anj_record_record_ready(int iRecChannel)
{
    int bAlarmStart = 0;
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();

    if (s_stRecParam.bRecStopFlag[iRecChannel])
    {
        return 0;
    }

    if ((anj_sdcard_status_get() == ANJ_SDCARD_STATUS_NORMAL) &&
        (anj_record_status_get() == REC_STATUS_NORMAL))
    {
        if (s_stRecParam.tRecEvent[iRecChannel] > REC_EVENT_NONE &&
            (pstRecConfig->scheduleRecordCfg.localStore || pstRecConfig->motionRecordCfg.localStore))
        {
            bAlarmStart = 1;
        }

        if (pstRecConfig->scheduleRecordCfg.localStore)
        {
            TimeSpanCfg *pTimeSpanCfg = &pstRecConfig->scheduleRecordCfg.timeSpan;
            if (0 == CheckNowIsInTimeSpan(pTimeSpanCfg))
            {
                return 0;
            }
        }
        else
        {
            if (bAlarmStart == 0)
            {
                return 0;
            }
        }
        return 1;
    }
    else
    {
        __ERR("Invalid Input\n");
        return 0;
    }
}

static void anj_record_record_event_uptime(int iRecChannel, unsigned long long framePts)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    pthread_mutex_lock(&s_stRecMutex);
    for (int tEvent = REC_EVENT_MOTION_ALARM_MASK; tEvent < REC_EVENT_MODE_MAX; tEvent++)
    {
        if (REC_EVENT_CHECK_MASK(s_stRecParam.tRecEvent[iRecChannel], tEvent))
        {
            if (s_stRecParam.tAlarmTmSec[tEvent] == 0)
            {
                s_stRecParam.tAlarmTmSec[tEvent] = framePts;
            }
            else
            {
                if ((framePts - s_stRecParam.tAlarmTmSec[tEvent]) > pstRecConfig->motionRecordCfg.recordTime * 1000)
                {
                    anj_record_stop_event(iRecChannel, (rec_event_mask_e)tEvent);
                    if (s_stRecParam.tRecEvent[iRecChannel] <= REC_EVENT_NONE)
                    {
                        __INFO("event finish!\n");
                        rec_mov_write_box_moov_trak_update(s_stRecParam.pstRecMovInfo[iRecChannel], 1);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&s_stRecMutex);
}

static int anj_record_pb_check_in_time(unsigned int tBeginTime, unsigned int tEndTime, unsigned int maxBeginTime, unsigned int maxEndTime)
{
    /* 查询窗 [maxBeginTime, maxEndTime) 与段 [tBeginTime, tEndTime) 相交（与按天查询、文件级重叠判断一致）。
     * 调用方传入的应是 moov 内「单段连续录像」的起止；若中间自然日实际无录像却共用一个长段包络，
     * 仅靠起止时间无法与「整段连续录满」区分，需在写入时按间隙拆段，否则日历会点亮中间日（cache 按天切分同理）。 */
    if (tBeginTime >= tEndTime || maxBeginTime >= maxEndTime)
    {
        return 0;
    }
    return (maxBeginTime < tEndTime) && (maxEndTime > tBeginTime);
}

static int anj_record_pre_create_file(const char *stPathName, unsigned long ifileSize)
{
    int iRet = 0;
    int fd = 0;

    if (NULL == stPathName)
    {
        __ERR("Invalid Input\n");
        return -1;
    }

    __INFO("create file :%s,%lu\n", stPathName, ifileSize);

    if (0 == access(stPathName, F_OK))
    {
        __ERR("fileName(%s) is exist, remove file\n", stPathName);
        unlink(stPathName);
    }
    fd = open(stPathName, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (0 >= fd)
    {
        __ERR("Invalid Input:%d,%s\n", fd, strerror(errno));
        return -1;
    }

#if SUPPORT_FALLOCATE
    // ifileSize = 1024;

    iRet = fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, ifileSize);
    if (0 != iRet)
    {
        __ERR("fallocate file err, fd:%d, ret:%d,%s\n", fd, iRet, strerror(errno));
        close(fd);
        return -1;
    }
    // iRet = ftruncate(fd, ifileSize);
    // if (0 != iRet)
    // {
    //     __ERR("ftruncate file err, fd:%d, ret:%d,%s\n", fd, iRet, strerror(errno));
    //     close(fd);
    //     return -1;
    // }
#endif
    close(fd);
    return iRet;
}

static int anj_record_build_root_path(char *out, size_t outLen, const char *mountPath)
{
    if ((NULL == out) || (0 == outLen) || (NULL == mountPath) || ('\0' == mountPath[0]))
    {
        return -1;
    }
    snprintf(out, outLen, "%s/%s", mountPath, REC_ROOT_SUBDIR);
    return 0;
}

static int anj_record_check_root_dir(const char *recRoot)
{
    struct stat st;

    if ((NULL == recRoot) || ('\0' == recRoot[0]))
    {
        return -1;
    }
    if ((0 != stat(recRoot, &st)) || (0 == S_ISDIR(st.st_mode)))
    {
        __ERR("sdcard record dir invalid: %s\n", recRoot);
        return -1;
    }
    return 0;
}

static int anj_record_write_save_index(int iRecChannel, int err)
{
    int iRet = 0;
    char fileIndexName[128] = {0};
    snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME, s_stRecParam.stRecfilePath);
    iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexName, iRecChannel, err);
    snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME_BAK, s_stRecParam.stRecfilePath);
    iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexName, iRecChannel, err);
    return iRet;
}

static int anj_record_write_file_destroy(int iRecChannel, int err)
{
    int iRet = 0;
    if (REC_STATUS_NORMAL != s_stRecParam.iRecStatus)
    {
        err = 1;
    }
    if (s_stRecParam.pstRecMovInfo[iRecChannel])
    {
        iRet = rec_mov_write_box_destroy(s_stRecParam.pstRecMovInfo[iRecChannel], err);
        s_stRecParam.pstRecMovInfo[iRecChannel] = NULL;
        if (REC_STATUS_NORMAL == s_stRecParam.iRecStatus)
        {
            __INFO("write save index\n");
            iRet = anj_record_write_save_index(iRecChannel, err);
        }
    }

    return iRet;
}

static int anj_record_bad_write(int bUpdate)
{
    int bWrite = 0;
    rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    if (pstIndexHeader == NULL)
    {
        return 0;
    }
    for (int i = 0; i < sizeof(s_pendingBadBlockMap); i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if ((pstIndexHeader->bad_block_map[i] >> j) & 1)
            {
                continue;
            }
            if ((s_pendingBadBlockMap[i] >> j) & 1)
            {
                bWrite = 1;
                if (bUpdate)
                {
                    pstIndexHeader->bad_block_map[i] |= (1 << j);
                }
            }
        }
    }
    if (bWrite)
    {
        __WARN("write bad bUpdate:%d\n", bUpdate);
    }
    return bWrite;
}

static int anj_record_stop_locked(int iRecChannel)
{
    int iRet = -1;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");

    anj_record_write_file_destroy(iRecChannel, 0);
    s_stRecParam.bRecStopFlag[iRecChannel] = 1;
    iRet = 0;
endFunc:
    return iRet;
}

static int anj_record_write_next_fileno(int iRecChannel)
{
    rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    unsigned int iNextRecMediaFileNo = pstIndexHeader->iNextRecMediaFileNo;
    unsigned int iFileRecNo = pstIndexHeader->iNextRecMediaFileNo;
    unsigned int max_attempts = pstIndexHeader->iMediaMaxFiles; // 最大尝试次数，避免无限循环
    unsigned int attempts = 0;
tryAgain:
    iNextRecMediaFileNo++;

    if (iNextRecMediaFileNo >= pstIndexHeader->iMediaMaxFiles)
    {
        iNextRecMediaFileNo = 0;
    }

    // 检查是否是坏块，如果是坏块则跳过
    unsigned int byte_index = iNextRecMediaFileNo / 8;
    unsigned int bit_index = iNextRecMediaFileNo % 8;
    if ((pstIndexHeader->bad_block_map[byte_index] >> bit_index) & 1)
    {
        __ERR("File %u is bad block, skipping\n", iNextRecMediaFileNo);

        if (attempts < max_attempts)
        {
            attempts++;
            goto tryAgain; // 跳过坏块，继续尝试下一个
        }
        else
        {
            __ERR("All files are bad blocks or in use, cannot find available file\n");
            // 标记异常卡 退出卡录
            s_stRecParam.iRecStatus = REC_STATUS_ERROR;
            anj_record_stop_locked(iRecChannel);
            return -1;
        }
    }

    __INFO("Change(%d-file%d) new:%u->next:%u\n", iRecChannel, pstIndexHeader->iFileRecNo[iRecChannel],
           iFileRecNo, iNextRecMediaFileNo);
    for (int i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stRecParam.chn_param[i].bEnable)
        {
            if (pstIndexHeader->iFileRecNo[i] == iNextRecMediaFileNo)
            {
                if (iNextRecMediaFileNo == pstIndexHeader->iNextRecMediaFileNo)
                {
                    __ERR("Invalid nextno %d, %d\n", iNextRecMediaFileNo, pstIndexHeader->iNextRecMediaFileNo);
                    return -1;
                }
                else
                {
                    goto tryAgain;
                }
            }
        }
    }
    pstIndexHeader->iRecordTimes++;
    pstIndexHeader->iFileRecNo[iRecChannel] = pstIndexHeader->iNextRecMediaFileNo;
    pstIndexHeader->iNextRecMediaFileNo = iNextRecMediaFileNo;
    return 0;
}

static void anj_record_bad_mark(unsigned int iFileNo)
{
    unsigned int byte_index = iFileNo / 8;
    unsigned int bit_index = iFileNo % 8;
    unsigned char mask = (unsigned char)(1 << bit_index);

    s_pendingBadBlockMap[byte_index] |= mask;
}

static int anj_record_write_file_create(int iRecChannel)
{
    int iRet = 0;
    rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, pstIndexHeader->iFileRecNo[iRecChannel]);
    char filename[128] = {0};
    iRet = anj_record_get_media_file_name(filename, sizeof(filename), pstIndexHeader->iFileRecNo[iRecChannel]);

    anj_alarm_event_handle(0, ALARM_CODE_RECORD_START, ALARM_FLAG_OCCUR,
                           ALARM_LEVEL_EVENT, 0, filename, NULL);

    memset(pstIndexRecord, 0, REC_FILE_INDEX_RECORD_SIZE);
    pstIndexRecord->iMediaFileCh = iRecChannel;

    s_stRecParam.pstRecMovInfo[iRecChannel] = rec_mov_write_box_create(filename, pstIndexRecord, &s_stRecVcodecParam[iRecChannel], &s_stRecAcodecParam);
    if (s_stRecParam.pstRecMovInfo[iRecChannel])
    {
        __INFO("create file %s, %p, ok\n", filename, s_stRecParam.pstRecMovInfo[iRecChannel]);
        iRet = 0;
    }
    else
    {
        __ERR("create file %s, error\n", filename);
        iRet = -1;
    }
    return iRet;
}

static int anj_record_write_file_next(int iRecChannel)
{
    int iRet = 0;
    rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, pstIndexHeader->iFileRecNo[iRecChannel]);
    pstIndexRecord->iMediaFileStatus = REC_STATUS_FULL;

    iRet = anj_record_write_file_destroy(iRecChannel, 0);
    iRet = anj_record_write_next_fileno(iRecChannel);
    if (iRet == 0)
    {
        iRet = anj_record_write_file_create(iRecChannel);
    }
    return iRet;
}

static int anj_record_write_file_update(int iRecChannel, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");
    ANJ_CHK(((NULL != s_stRecParam.pstRecMovInfo[iRecChannel])), ANJ_ERR_NOT_INIT, "not init rec");
    ANJ_CHK((0 == s_stRecParam.bRecStopFlag[iRecChannel]), ANJ_ERR_NOT_START, "stop flag");
    ANJ_CHK(((NULL != pFrameInfo) && (NULL != pFrameInfo->frameBuf)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");
    ANJ_CHK(((pFrameInfo->frameParam.frameLen > 0) && (pFrameInfo->frameParam.frameLen <= REC_MAX_FRAME_BUF_SIZE)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");

    iRet = rec_mov_write_box_write_update_index(s_stRecParam.pstRecMovInfo[iRecChannel], pFrameInfo, s_stRecParam.tRecEvent[iRecChannel], s_stRecParam.segChangTime);
    if (0 < iRet)
    {
        iRet = anj_record_write_save_index(iRecChannel, 0);
    }

endFunc:
    return iRet;
}

static int anj_record_write_file_check(int iRecChannel, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");
    ANJ_CHK((0 == s_stRecParam.bRecStopFlag[iRecChannel]), ANJ_ERR_NOT_START, "stop flag");
    ANJ_CHK(((NULL != pFrameInfo) && (NULL != pFrameInfo->frameBuf)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");
    ANJ_CHK(((pFrameInfo->frameParam.frameLen > 0) && (pFrameInfo->frameParam.frameLen <= REC_MAX_FRAME_BUF_SIZE)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");

    if (s_stRecParam.pstRecMovInfo[iRecChannel])
    {
        iRet = rec_mov_write_box_write_file_check(s_stRecParam.pstRecMovInfo[iRecChannel], pFrameInfo);
        if (iRet)
        {
            iRet = anj_record_write_file_next(iRecChannel);
        }
    }
    else
    {
        iRet = anj_record_write_next_fileno(iRecChannel);
        if (iRet == 0)
        {
            iRet = anj_record_write_file_create(iRecChannel);
        }
    }

endFunc:
    return iRet;
}

int anj_record_write_frame(int iRecChannel, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    int bRestart = 0;
    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");
    ANJ_CHK((0 == s_stRecParam.bRecStopFlag[iRecChannel]), ANJ_ERR_NOT_START, "stop flag");
    ANJ_CHK(((NULL != pFrameInfo) && (NULL != pFrameInfo->frameBuf)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");
    ANJ_CHK(((pFrameInfo->frameParam.frameLen > 0) && (pFrameInfo->frameParam.frameLen <= REC_MAX_FRAME_BUF_SIZE)), ANJ_ERR_INVALID_INPUT, "Invalid Input pFrameInfo");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");

    if (0 != anj_record_write_file_check(iRecChannel, pFrameInfo))
    {
        __ERR("check file err\n");
        goto endFunc;
    }

    if ((pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_PCM) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_G711A) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_AAC) ||
        (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_AUDIO_G711U))
    {
        iRet = rec_mov_write_box_moov_atrak_write_frame(s_stRecParam.pstRecMovInfo[iRecChannel], pFrameInfo);
        if (-1 == iRet)
        {
            rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
            __ERR("write file:%s err, iRecChannel:%d\n",
                s_stRecParam.pstRecMovInfo[iRecChannel]->iWriteFileName, iRecChannel);
            if (0 != anj_record_write_file_destroy(iRecChannel, 1))
            {
                __ERR("destroy file err\n");
                anj_record_bad_mark(pstIndexHeader->iFileRecNo[iRecChannel]);
                bRestart = 1;
            }
            goto endFunc;
        }
        else if (-2 == iRet)
        {
            goto endFunc;
        }
        if (0 != anj_record_write_file_update(iRecChannel, pFrameInfo))
        {
            __ERR("check file err\n");
            goto endFunc;
        }
    }
    else if ((pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_VIDEO_H264) || (pFrameInfo->frameParam.frameCodec == MEDIA_CODEC_VIDEO_H265))
    {
        iRet = rec_mov_write_box_moov_vtrak_write_frame(s_stRecParam.pstRecMovInfo[iRecChannel], pFrameInfo);
        if (-1 == iRet)
        {
            rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
            __ERR("write file:%s err, iRecChannel:%d\n",
                s_stRecParam.pstRecMovInfo[iRecChannel]->iWriteFileName, iRecChannel);
            if (0 != anj_record_write_file_destroy(iRecChannel, 1))
            {
                __ERR("destroy file err\n");
                anj_record_bad_mark(pstIndexHeader->iFileRecNo[iRecChannel]);
                bRestart = 1;
            }
            goto endFunc;
        }
        if (0 != anj_record_write_file_update(iRecChannel, pFrameInfo))
        {
            __ERR("check file err\n");
            goto endFunc;
        }
    }
    else
    {
        __ERR("Unsupport type %d\n", pFrameInfo->frameParam.frameCodec);
        goto endFunc;
    }

endFunc:
    pthread_mutex_unlock(&s_stRecMutex);
    if (bRestart)
    {
        anj_record_restart();
    }
    return iRet;
}

static int anj_record_write_proc(void *ctx, int *bStart)
{
    int iRet = 0;
    int bOsdShow = 0;
    unsigned char *pFrameBuf = NULL;
    ANJ_CHK(((ctx != NULL) && (bStart != NULL)), -1, "input Invalid");

    rec_chn *pstChnParam = (rec_chn *)ctx;
    int iRecChannel = pstChnParam->iRecChn;
    int VencChn = pstChnParam->iRecVenc;
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    struct tm tm_init = {0};
    tm_init.tm_year = TIME_INIT_YEAR - 1900 + 1;
    tm_init.tm_mon = TIME_INIT_MONTH - 1;
    tm_init.tm_mday = TIME_INIT_DAY;
    tm_init.tm_hour = TIME_INIT_HOUR;
    tm_init.tm_min = TIME_INIT_MIN;
    tm_init.tm_sec = TIME_INIT_SEC;
    time_t sec_init = mktime(&tm_init);

    media_frame_info_t stReadFrameInfo = {0};
    ANJ_MBUF_HANDLE *readerid = NULL;

    int iFrameSize = 0;
    if (ANJ_CAMERA_MAX_NUMS > 1)
    {
        iFrameSize = (VencChn % ANJ_CAMERA_MAX_NUMS) ? ANJ_CAMERA_VIDEO_SUB_MAX_SIZE : ANJ_CAMERA_VIDEO_MAX_SIZE;
    }
    else
    {
        iFrameSize = VencChn ? ANJ_CAMERA_VIDEO_SUB_MAX_SIZE : ANJ_CAMERA_VIDEO_MAX_SIZE;
    }
    pFrameBuf = anj_mw_malloc(iFrameSize);
    ANJ_CHK((pFrameBuf != NULL), -1, "malloc failed");

    int timelapseEnable = pstRecConfig->commonCfg.timelapseCfg.timelapseEnable;
    int audio_enable = (strcmp(pstRecConfig->scheduleRecordCfg.mediaType.typeName, "VIDEO") == 0) ? 0 : 1;
    int timelapsePts = 0;
    int timelapsePts1 = 0;
    while (bStart && *bStart)
    {
        rec_status_e recStatus = anj_record_status_get();
        if ((anj_sdcard_status_get() != ANJ_SDCARD_STATUS_NORMAL) ||
            (recStatus != REC_STATUS_NORMAL))
        {
            __ERR("record thread exit, ch:%d sd:%d rec:%d\n",
                  iRecChannel, anj_sdcard_status_get(), recStatus);
            break;
        }

        if (anj_record_record_ready(iRecChannel) == 0)
        {
            usleep(100 * 1000);
            continue;
        }

        //     SetForceIdr(0, VencChn);
        readerid = anj_mbuf_create_reader(VencChn, 1);
        if (readerid == NULL)
        {
            __INFO("anj_mbuf_create_reader chn:%d failed!\n", VencChn);
            usleep(100 * 1000);
            readerid = anj_mbuf_create_reader(VencChn, 1);
            if (readerid == NULL)
            {
                __INFO("anj_mbuf_create_reader chn:%d failed!\n", VencChn);
                continue;
            }
        }
        __INFO("anj_mbuf_create_reader chn:%d OK!\n", VencChn);

        int bFirstFrame = 1;
        unsigned int iLastVFrameIndex = 0;
        anj_record_start_event(iRecChannel, REC_EVENT_NULL_MASK);
        anj_record_start(iRecChannel);
        bOsdShow = 1;

        FILE *pFile = NULL;
        while (bStart && *bStart && anj_record_record_ready(iRecChannel))
        {
            if (0 < anj_mbuf_read_frame(readerid, bFirstFrame, &stReadFrameInfo, (bFirstFrame) ? 2000 : 200))
            {
                if (stReadFrameInfo.frameParam.frameLen > iFrameSize)
                {
                    pFrameBuf = anj_mw_realloc(pFrameBuf, stReadFrameInfo.frameParam.frameLen);
                    iFrameSize = stReadFrameInfo.frameParam.frameLen;
                }
                memset(pFrameBuf, 0, iFrameSize);
                memcpy(pFrameBuf, stReadFrameInfo.frameBuf, stReadFrameInfo.frameParam.frameLen);
                anj_mbuf_read_release(readerid, &stReadFrameInfo);
                stReadFrameInfo.frameBuf = pFrameBuf;
                if (0 == access("/tmp/rec", F_OK))
                {
                    if (pFile == NULL)
                    {
                        pFile = fopen("/tmp/nfs/rec.h265", "wb");
                    }
                }
                else
                {
                    if (pFile)
                    {
                        fclose(pFile);
                        pFile = NULL;
                    }
                }

                if (pFile)
                {
                    if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                    {
                        fwrite(stReadFrameInfo.frameBuf, stReadFrameInfo.frameParam.frameLen, 1, pFile);
                    }
                }

                // 确保缓存池的数据已校时
                if (stReadFrameInfo.frameParam.frameTime < (long)sec_init)
                {
                    continue;
                }

                // drop frame wait key
                if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                {
                    if ((iLastVFrameIndex != 0) && (stReadFrameInfo.frameParam.vframeIndex != iLastVFrameIndex + 1))
                    {
                        __INFO("DROP FRAME %d %d!\n", stReadFrameInfo.frameParam.vframeIndex, iLastVFrameIndex);
                        bFirstFrame = 1;
                    }
                    iLastVFrameIndex = stReadFrameInfo.frameParam.vframeIndex;
                    if (stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_I)
                    {
                        bFirstFrame = 0;
                    }
                }
                else if (audio_enable == 0)
                {
                    continue;
                }

                // first frame must I frame
                if (bFirstFrame == 0)
                {
                    if (timelapseEnable == 0)
                    {
                        anj_record_write_frame(iRecChannel, &stReadFrameInfo);
                        if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                            anj_record_record_event_uptime(iRecChannel, stReadFrameInfo.frameParam.framePts);
                    }
                    else
                    {
                        if (stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_I)
                        {
                            int timelapseSec = pstRecConfig->commonCfg.timelapseCfg.timelapseSec;
                            int timelapseDuration = 1000 / pstRecConfig->commonCfg.timelapseCfg.timelapseFps;
                            if (((stReadFrameInfo.frameParam.framePts - timelapsePts) * 1000) > timelapseSec)
                            {
                                timelapsePts = stReadFrameInfo.frameParam.framePts;
                                timelapsePts1 += timelapseDuration;
                                stReadFrameInfo.frameParam.framePts = timelapsePts1;
                                anj_record_write_frame(iRecChannel, &stReadFrameInfo);
                            }
                        }
                    }
                }
                else
                {
                    __INFO("DROP FRAME!\n");
                }
            }
            else
            {
                __ERR("Get VencChn:%d Frame Err\n", VencChn);
            }
        }
        if (bOsdShow)
        {
            anj_record_osd_set(0);
            bOsdShow = 0;
        }

        if (readerid)
        {
            anj_mbuf_destory_reader(readerid);
            readerid = NULL;
        }
    }
    anj_record_stop(iRecChannel);
    __INFO("exit!\n");
endFunc:
    if (pFrameBuf)
    {
        anj_mw_free(pFrameBuf);
    }
    return iRet;
}

static int anj_record_pb_media_file_segment(int iFileNo, rec_media_segment_index *pstSegMent)
{
    int iRet = 0;
    FILE *fp = NULL;
    rec_file_index_header *pstIndexHeader = NULL;
    char fileName[128] = {0};
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    if (rec_mov_index_is_bad_file(pstIndexHeader, (unsigned int)iFileNo))
    {
        __ERR("iFileNo:%d is bad file!\n", iFileNo);
        return ANJ_RECORD_PB_SEG_ERR_OPEN;
    }

    iRet = anj_record_get_media_file_name(fileName, sizeof(fileName), iFileNo);
    if (iRet != 0)
    {
        __ERR("check file err\n");
        iRet = ANJ_RECORD_PB_SEG_ERR_OPEN;
        goto endFunc;
    }

    fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        __ERR("fopen fileName:%s err\n", fileName);
        iRet = ANJ_RECORD_PB_SEG_ERR_OPEN;
        goto endFunc;
    }

    iRet = rec_mov_read_box_free_segment(fp, pstSegMent, REC_MEDIA_INDEX_MAX_SEGMENT);
    if (iRet != 0)
    {
        __ERR("read fileName:%s segment data error\n", fileName);
        iRet = -1;
        goto endFunc;
    }
    for (int i = 0; i < REC_MAX_CH_NUM; i++)
    {
        /*回放正在写入的文件*/
        if (pstIndexHeader->iFileRecNo[i] == (unsigned int)iFileNo)
        {
            rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
            if (pstIndexRecord->iMediaFileSegRecNums > 0)
            {
                rec_media_segment_index *curSegmnet = &pstSegMent[pstIndexRecord->iMediaFileSegRecNums - 1];
                if (curSegmnet->iMediaFileStatus != REC_STATUS_FINISH)
                {
                    if (curSegmnet->iVFrameNum < REC_FILE_FFLUSH_FPS)
                    {
                        memset(curSegmnet, 0, REC_MEDIA_INDEX_SEGMENT_SIZE);
                    }
                    else
                    {
                        // 回放减少1024fps播放
                        curSegmnet->iVFrameNum -= REC_FILE_FFLUSH_FPS;
                    }
                }
            }
            else
            {
                memset(pstSegMent, 0, REC_MEDIA_INDEX_SEGMENT_SIZE * REC_MEDIA_INDEX_MAX_SEGMENT);
            }
        }
    }

endFunc:
    if (fp)
    {
        fclose(fp);
    }

    return iRet;
}

static void anj_record_pb_add_pbseglist(int *pRecSegCount, rec_pb_segment_s **pSegHead, rec_pb_segment_s **pSegTail, unsigned int tBeginTime, unsigned int tEndTime, unsigned int iEvenType)
{
    rec_pb_segment_s *pSegNew;
    rec_pb_segment_s *pHead;
    rec_pb_segment_s *pTail;

    pSegNew = (rec_pb_segment_s *)anj_mw_malloc(sizeof(rec_pb_segment_s));
    if (pSegNew == NULL)
    {
        __ERR("Exit Error! anj_mw_malloc failed!\n");
        return;
    }
    memset((char *)pSegNew, 0, sizeof(rec_pb_segment_s));
    pSegNew->ptNext = NULL;
    pSegNew->tEvent = iEvenType;
    pSegNew->begin_time_s = tBeginTime;
    pSegNew->end_time_s = tEndTime;
    pHead = *pSegHead;
    pTail = *pSegTail;
    if (NULL == pHead)
    {
        pHead = pSegNew;
        pTail = pSegNew;
    }
    else
    {
        pTail->ptNext = pSegNew;
        pTail = pSegNew;
    }
    *pSegHead = pHead;
    *pSegTail = pTail;
    if (pRecSegCount)
    {
        *pRecSegCount = (*pRecSegCount) + 1;
    }
    return;
}

int anj_record_pb_is_valid(REC_HANDLE hPoperHandle)
{
    int i;
    for (i = 0; i < REC_MAX_PB_NUM; i++)
    {
        if (&s_stRecPbPoper[i] == hPoperHandle)
        {
            if (s_stRecPbPoper[i].bOpen)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
    return 0;
}

static int anj_record_pb_day_valid(unsigned int tBeginTime, unsigned int tStarDays,
                                   int iFileNo, rec_media_segment_index *pstSegMent, rec_file_index_record *pstIndexRecord)
{
    int iRet = 0;
    memset(pstSegMent, 0, sizeof(rec_media_segment_index) * REC_MEDIA_INDEX_MAX_SEGMENT);
    if (0 == anj_record_pb_media_file_segment(iFileNo, pstSegMent))
    {
        unsigned int tDayBeginTime = tBeginTime + (tStarDays * REC_TIME_DAY);
        unsigned int tDayEndTime = tDayBeginTime + REC_TIME_DAY;
        for (int iSegMent = 0; iSegMent < pstIndexRecord->iMediaFileSegRecNums; iSegMent++)
        {
            /*__INFO("rec seg time %u-%u,time %u-%u\n",
            tDayBeginTime, tDayEndTime,
            pstSegMent[iSegMent].tMediaFileBeginTime,
            pstSegMent[iSegMent].tMediaFileEndTime);*/

            if (anj_record_pb_check_in_time(pstSegMent[iSegMent].tMediaFileBeginTime, pstSegMent[iSegMent].tMediaFileEndTime, tDayBeginTime, tDayEndTime))
            {
                iRet = 1;
                break;
            }
        }
    }
    return iRet;
}

static void anj_record_pb_cache_free_segments(rec_pb_cache_segment_s *pstSegment)
{
    rec_pb_cache_segment_s *pstNext = NULL;

    while (pstSegment != NULL)
    {
        pstNext = pstSegment->ptNext;
        free(pstSegment);
        pstSegment = pstNext;
    }
}

static void anj_record_pb_cache_free_channel(rec_pb_cache_channel_s *pstChannel)
{
    rec_pb_cache_day_s *pstDay = NULL;
    rec_pb_cache_day_s *pstNext = NULL;

    if (pstChannel == NULL)
    {
        return;
    }

    pstDay = pstChannel->pstDay;
    while (pstDay != NULL)
    {
        pstNext = pstDay->ptNext;
        anj_record_pb_cache_free_segments(pstDay->pstSegment);
        free(pstDay);
        pstDay = pstNext;
    }
    pstChannel->pstDay = NULL;
}

static void anj_record_pb_cache_free_all(rec_pb_cache_channel_s *pstChannel)
{
    int i = 0;

    if (pstChannel == NULL)
    {
        return;
    }

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        anj_record_pb_cache_free_channel(&pstChannel[i]);
    }
}

static rec_pb_cache_day_s *anj_record_pb_cache_get_day(rec_pb_cache_channel_s *pstChannel, unsigned int year, unsigned int month, unsigned int day, int bCreate)
{
    rec_pb_cache_day_s *pstDay = NULL;
    rec_pb_cache_day_s *pstNew = NULL;

    if (pstChannel == NULL)
    {
        return NULL;
    }

    pstDay = pstChannel->pstDay;
    while (pstDay != NULL)
    {
        if ((pstDay->year == year) && (pstDay->month == month) && (pstDay->day == day))
        {
            return pstDay;
        }
        pstDay = pstDay->ptNext;
    }

    if (!bCreate)
    {
        return NULL;
    }

    pstNew = (rec_pb_cache_day_s *)calloc(1, sizeof(rec_pb_cache_day_s));
    if (pstNew == NULL)
    {
        __ERR("cache day malloc failed\n");
        return NULL;
    }

    pstNew->year = year;
    pstNew->month = month;
    pstNew->day = day;
    pstNew->ptNext = pstChannel->pstDay;
    pstChannel->pstDay = pstNew;

    return pstNew;
}

static int anj_record_pb_cache_add_segment(rec_pb_cache_channel_s *pstChannel, unsigned int tBeginTime, unsigned int tEndTime, unsigned int tEvent)
{
    time_t tCurTime = 0;
    struct tm stTm;
    unsigned int tDayBeginTime = 0;
    unsigned int tDayEndTime = 0;
    rec_pb_cache_day_s *pstDay = NULL;
    rec_pb_cache_segment_s *pstSegment = NULL;
    unsigned int tAddBeginTime = 0;
    unsigned int tAddEndTime = 0;

    if ((pstChannel == NULL) || (tBeginTime >= tEndTime))
    {
        return -1;
    }

    tCurTime = (time_t)tBeginTime;
    memset(&stTm, 0, sizeof(stTm));
    localtime_r(&tCurTime, &stTm);
    stTm.tm_hour = 0;
    stTm.tm_min = 0;
    stTm.tm_sec = 0;
    tDayBeginTime = mktime(&stTm);
    tDayEndTime = tDayBeginTime + REC_TIME_DAY - 1;

    pstDay = anj_record_pb_cache_get_day(pstChannel, stTm.tm_year + 1900, stTm.tm_mon + 1, stTm.tm_mday, 1);
    if (pstDay == NULL)
    {
        return -1;
    }

    tAddBeginTime = (tBeginTime < tDayBeginTime) ? tDayBeginTime : tBeginTime;
    tAddEndTime = (tEndTime > tDayEndTime) ? tDayEndTime : tEndTime;
    if (tAddBeginTime >= tAddEndTime)
    {
        return 0;
    }

    if ((pstDay->pstTail != NULL) && (pstDay->pstTail->tEvent == tEvent))
    {
        /* 3秒内不分片：仅合并同事件且时间不重叠的相邻段；重叠段（如普通+告警 I 帧回溯）各自保留 */
        if ((tAddBeginTime >= pstDay->pstTail->end_time_s) &&
            (tAddBeginTime <= pstDay->pstTail->end_time_s + 3))
        {
            if (tAddEndTime > pstDay->pstTail->end_time_s)
            {
                pstDay->pstTail->end_time_s = tAddEndTime;
            }
            return 0;
        }
        if ((tAddEndTime <= pstDay->pstTail->begin_time_s) &&
            (tAddEndTime + 3 >= pstDay->pstTail->begin_time_s))
        {
            if (tAddBeginTime < pstDay->pstTail->begin_time_s)
            {
                pstDay->pstTail->begin_time_s = tAddBeginTime;
            }
            return 0;
        }
    }

    pstSegment = (rec_pb_cache_segment_s *)calloc(1, sizeof(rec_pb_cache_segment_s));
    if (pstSegment == NULL)
    {
        __ERR("cache segment malloc failed\n");
        return -1;
    }

    pstSegment->tEvent = tEvent;
    pstSegment->begin_time_s = tAddBeginTime;
    pstSegment->end_time_s = tAddEndTime;
    if (pstDay->pstSegment == NULL)
    {
        pstDay->pstSegment = pstSegment;
        pstDay->pstTail = pstSegment;
    }
    else
    {
        pstDay->pstTail->ptNext = pstSegment;
        pstDay->pstTail = pstSegment;
    }
    pstDay->count++;

    return 0;
}

static int anj_record_pb_cache_build(rec_pb_cache_channel_s *pstChannel)
{
    int iFileNo = 0;
    rec_file_index_header *pstIndexHeader = NULL;
    rec_file_index_record *pstIndexRecord = NULL;
    rec_media_segment_index *pstSegMent = NULL;
    int bFileIncomplete = 0;
    int bCrcAbnormal = 0;
    int bHasBadBlock = 0;
    char fileName[128] = {0};

    if (pstChannel == NULL)
    {
        return -1;
    }

    rec_param stRecParam = {0};
    if (rec_mov_index_file_load(&stRecParam.stRecIndexParam, s_stRecParam.stRecfilePath) != 0)
    {
        __ERR("rec_mov_index_file_reload failed\n");
        return -1;
    }

    pstSegMent = malloc(sizeof(rec_media_segment_index) * REC_MEDIA_INDEX_MAX_SEGMENT);
    if (pstSegMent == NULL)
    {
        __ERR("pstSegMent malloc failed\n");
        if (stRecParam.stRecIndexParam.ptsDataBuf)
        {
            free(stRecParam.stRecIndexParam.ptsDataBuf);
        }
        return -1;
    }

    pstIndexHeader = rec_mov_index_file_get_header(&stRecParam.stRecIndexParam);
    for (iFileNo = 0; iFileNo < REC_MEDIA_BAD_BLOCK_MAP_SIZE; iFileNo++)
    {
        if (pstIndexHeader->bad_block_map[iFileNo])
        {
            bHasBadBlock = 1;
            break;
        }
    }

    for (iFileNo = 0; iFileNo < pstIndexHeader->iMediaMaxFiles; iFileNo++)
    {
        int iSegMent = 0;
        int iRet = 0;

        memset(fileName, 0, sizeof(fileName));
        if ((0 != anj_record_get_media_file_name(fileName, sizeof(fileName), iFileNo)) ||
            (0 == anj_sdcard_file_exists(fileName)))
        {
            __ERR("rec card file:%s not exist\n", fileName);
            bFileIncomplete = 1;
            continue;
        }

        if (rec_mov_index_is_bad_file(pstIndexHeader, iFileNo))
        {
            __WARN("iFileNo:%d is bad file!\n", iFileNo);
            continue;
        }

        pstIndexRecord = rec_mov_index_file_get_record(&stRecParam.stRecIndexParam, iFileNo);
        if ((pstIndexRecord->iMediaFileSegRecNums <= 0) ||
            (pstIndexRecord->iMediaFileSegRecNums > REC_MEDIA_INDEX_MAX_SEGMENT) ||
            ((pstIndexRecord->iMediaFileStatus != REC_STATUS_WRITE) && (pstIndexRecord->iMediaFileStatus != REC_STATUS_FULL)))
        {
            continue;
        }

        memset(pstSegMent, 0, sizeof(rec_media_segment_index) * REC_MEDIA_INDEX_MAX_SEGMENT);
        iRet = anj_record_pb_media_file_segment(iFileNo, pstSegMent);
        if (iRet != 0)
        {
            if (iRet == ANJ_RECORD_PB_SEG_ERR_OPEN)
            {
                bFileIncomplete = 1;
            }
            else if (pstIndexRecord->iMediaFileStatus == REC_STATUS_FULL)
            {
                /* 已封存文件 free-box CRC 失败：异常卡串写；WRITE 失败视为掉电残留不提示 */
                __ERR("sealed file crc/segment fail iFileNo:%d status:%u\n",
                      iFileNo, pstIndexRecord->iMediaFileStatus);
                bCrcAbnormal = 1;
            }
            continue;
        }

        for (iSegMent = 0; iSegMent < pstIndexRecord->iMediaFileSegRecNums; iSegMent++)
        {
            unsigned int tLoopTime = 0;

            if ((pstSegMent[iSegMent].iMediaFileCh < 0) || (pstSegMent[iSegMent].iMediaFileCh >= REC_MAX_CH_NUM) ||
                (pstSegMent[iSegMent].tMediaFileBeginTime >= pstSegMent[iSegMent].tMediaFileEndTime) ||
                (pstSegMent[iSegMent].iMediaFileStatus == REC_STATUS_NULL))
            {
                continue;
            }

            tLoopTime = pstSegMent[iSegMent].tMediaFileBeginTime;
            while (tLoopTime < pstSegMent[iSegMent].tMediaFileEndTime)
            {
                time_t tCurTime = (time_t)tLoopTime;
                struct tm stTm;
                unsigned int tDayBegin = 0;
                unsigned int tDayEnd = 0;
                unsigned int tSegBegin = 0;
                unsigned int tSegEnd = 0;

                memset(&stTm, 0, sizeof(stTm));
                localtime_r(&tCurTime, &stTm);
                stTm.tm_hour = 0;
                stTm.tm_min = 0;
                stTm.tm_sec = 0;
                tDayBegin = mktime(&stTm);
                tDayEnd = tDayBegin + REC_TIME_DAY - 1;
                tSegBegin = (pstSegMent[iSegMent].tMediaFileBeginTime > tDayBegin) ? pstSegMent[iSegMent].tMediaFileBeginTime : tDayBegin;
                tSegEnd = (pstSegMent[iSegMent].tMediaFileEndTime < tDayEnd) ? pstSegMent[iSegMent].tMediaFileEndTime : tDayEnd;

                if (anj_record_pb_cache_add_segment(&pstChannel[pstSegMent[iSegMent].iMediaFileCh], tSegBegin, tSegEnd, pstSegMent[iSegMent].tMediaFileEvent) != 0)
                {
                    free(pstSegMent);
                    if (stRecParam.stRecIndexParam.ptsDataBuf)
                    {
                        free(stRecParam.stRecIndexParam.ptsDataBuf);
                    }
                    return -1;
                }
                if (tDayEnd >= pstSegMent[iSegMent].tMediaFileEndTime)
                {
                    break;
                }
                tLoopTime = tDayEnd + 1;
            }
        }
    }

    anj_record_osd_apply_health(bFileIncomplete, bCrcAbnormal, bHasBadBlock);

    free(pstSegMent);
    if (stRecParam.stRecIndexParam.ptsDataBuf)
    {
        free(stRecParam.stRecIndexParam.ptsDataBuf);
    }
    return 0;
}

static int anj_record_pb_cache_refresh(void)
{
    int iRet = -1;
    rec_pb_cache_channel_s stNewChannel[REC_MAX_CH_NUM] = {0};

    pthread_mutex_lock(&s_stPbCache.mutex);
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");

    ANJ_CHK_FUNC(anj_record_pb_cache_build(stNewChannel), 0, "pb cache build failed");

    anj_record_pb_cache_free_all(s_stPbCache.stChannel);
    memcpy(s_stPbCache.stChannel, stNewChannel, sizeof(stNewChannel));
    iRet = 0;

endFunc:
    if (iRet != 0)
    {
        anj_record_pb_cache_free_all(stNewChannel);
    }
    pthread_mutex_unlock(&s_stPbCache.mutex);
    return iRet;
}

static int anj_record_pb_cache_proc(void *arg, int *bStart)
{
    while (bStart && *bStart)
    {
        anj_record_pb_cache_refresh();
        for (int i = 0; i < REC_PB_CACHE_REFRESH_SEC; i++)
        {
            if (!*bStart)
            {
                break;
            }
            sleep(1);
        }
    }

    return 0;
}

static int anj_record_pb_cache_init(void)
{
    memset(&s_stPbCache.stPbCacheThread, 0, sizeof(s_stPbCache.stPbCacheThread));
    s_stPbCache.stPbCacheThread.bAutoDestroy = 0;
    snprintf(s_stPbCache.stPbCacheThread.iThreadName, sizeof(s_stPbCache.stPbCacheThread.iThreadName), "pb_cache");
    s_stPbCache.stPbCacheThread.iThreadjob.ctx = &s_stPbCache;
    s_stPbCache.stPbCacheThread.iThreadjob.func = anj_record_pb_cache_proc;
    if (anj_thread_task_create(&s_stPbCache.stPbCacheThread) != 0)
    {
        __ERR("create cache thread failed\n");
        return -1;
    }

    return 0;
}

static void anj_record_pb_cache_uninit(void)
{
    if (s_stPbCache.stPbCacheThread.start)
    {
        anj_thread_task_destroy(&s_stPbCache.stPbCacheThread, 0);
    }

    pthread_mutex_lock(&s_stPbCache.mutex);
    anj_record_pb_cache_free_all(s_stPbCache.stChannel);
    memset(&s_stPbCache.stPbCacheThread, 0, sizeof(s_stPbCache.stPbCacheThread));
    pthread_mutex_unlock(&s_stPbCache.mutex);
}

static int anj_record_pb_cache_query_month(int iRecChannel, rec_pb_date_s *pstPbDate)
{
    rec_pb_cache_day_s *pstDay = NULL;
    int bFound = 0;

    pthread_mutex_lock(&s_stPbCache.mutex);

    pstPbDate->day = 0;
    pstDay = s_stPbCache.stChannel[iRecChannel].pstDay;
    while (pstDay != NULL)
    {
        if ((pstDay->year == pstPbDate->year) && (pstDay->month == pstPbDate->month) && (pstDay->day >= 1) && (pstDay->day <= 31))
        {
            pstPbDate->day |= (1U << (pstDay->day - 1));
            bFound = 1;
        }
        pstDay = pstDay->ptNext;
    }
    pthread_mutex_unlock(&s_stPbCache.mutex);

    return bFound ? 0 : -1;
}

static int anj_record_pb_cache_query_day(int iRecChannel, rec_pb_date_s *pstPbDate, rec_pb_list_s *pstPbList)
{
    rec_pb_cache_day_s *pstDay = NULL;
    rec_pb_cache_segment_s *pstSegment = NULL;
    rec_pb_segment_s *pSegHead = NULL;
    rec_pb_segment_s *pSegTail = NULL;
    int nRecSegCount = 0;
    int bFound = 0;

    pthread_mutex_lock(&s_stPbCache.mutex);
    pstDay = anj_record_pb_cache_get_day(&s_stPbCache.stChannel[iRecChannel], pstPbDate->year, pstPbDate->month, pstPbDate->day, 0);
    if (pstDay != NULL)
    {
        bFound = 1;
        pstSegment = pstDay->pstSegment;
        while (pstSegment != NULL)
        {
            if ((pstPbDate->tEvent == 0) || (pstSegment->tEvent & pstPbDate->tEvent))
            {
                anj_record_pb_add_pbseglist(&nRecSegCount, &pSegHead, &pSegTail, pstSegment->begin_time_s, pstSegment->end_time_s, pstSegment->tEvent);
            }
            pstSegment = pstSegment->ptNext;
        }
    }
    pthread_mutex_unlock(&s_stPbCache.mutex);

    pstPbList->count = nRecSegCount;
    pstPbList->pstSegment = pSegHead;
    return bFound ? 0 : -1;
}

static int anj_record_pb_get_next_fileno(int iRecChannel, unsigned int playTime, int *pFileNo, int iEndFileNo)
{
    int iRet = -1;
    int iFileNo = 0;
    rec_file_index_header *pstIndexHeader = NULL;
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);

    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");
    ANJ_CHK((*pFileNo >= 0 && *pFileNo < pstIndexHeader->iMediaMaxFiles), ANJ_ERR_INVALID_INPUT, "Invalid pFileNo");

    iFileNo = *pFileNo;

    for (unsigned int i = 0; i < pstIndexHeader->iMediaMaxFiles - 1; i++)
    {
        iFileNo = (iFileNo + 1) % pstIndexHeader->iMediaMaxFiles;
        if (iFileNo == iEndFileNo)
        {
            __INFO("restar FileNo %u\n", iEndFileNo);
            goto endFunc;
        }
        if (rec_mov_index_is_bad_file(pstIndexHeader, (unsigned int)iFileNo))
        {
            continue;
        }
        rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
        if ((pstIndexRecord->iMediaFileCh == iRecChannel) &&
            (pstIndexRecord->iMediaFileSegRecNums > 0 && pstIndexRecord->iMediaFileSegRecNums <= REC_MEDIA_INDEX_MAX_SEGMENT) &&
            (pstIndexRecord->iMediaFileStatus == REC_STATUS_WRITE || pstIndexRecord->iMediaFileStatus == REC_STATUS_FULL))
        {
            if (pstIndexRecord->tMediaFileEndTime > playTime)
            {
                *pFileNo = iFileNo;
                iRet = 0;
                __INFO("get pb file:%d,playTime %u\n", iFileNo, playTime);
                goto endFunc;
            }
        }
    }

endFunc:
    if (iRet)
    {
        __INFO("not pb data no:%d, time:%u\n", *pFileNo, playTime);
        *pFileNo = -1;
    }
    return iRet;
}

static int anj_record_pb_get_fileno_by_time(int iRecChannel, unsigned int seekTime, int *pFileNo)
{
    int iRet = -1;
    int iFileNo = 0;
    int iSameNo = -1;
    unsigned int iSameTime = -1;
    rec_file_index_header *pstIndexHeader = NULL;

    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);

    for (unsigned int i = 0; i < pstIndexHeader->iMediaMaxFiles; i++)
    {
        /*逆序获取MP4文件序号*/
        iFileNo = (pstIndexHeader->iFileRecNo[iRecChannel] + pstIndexHeader->iMediaMaxFiles - i) % pstIndexHeader->iMediaMaxFiles;
        if (rec_mov_index_is_bad_file(pstIndexHeader, (unsigned int)iFileNo))
        {
            continue;
        }
        rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
        if ((pstIndexRecord->iMediaFileCh == iRecChannel) &&
            ((pstIndexRecord->iMediaFileSegRecNums > 0) && (pstIndexRecord->iMediaFileSegRecNums <= REC_MEDIA_INDEX_MAX_SEGMENT)) &&
            ((pstIndexRecord->iMediaFileStatus == REC_STATUS_WRITE) || (pstIndexRecord->iMediaFileStatus == REC_STATUS_FULL)))
        {
            //__INFO("get pb file(%d) seek %u,t:%u~%u\n", iFileNo, seekTime, pstIndexRecord->tMediaFileBeginTime, pstIndexRecord->tMediaFileEndTime);
            if ((pstIndexRecord->tMediaFileBeginTime <= seekTime) &&
                (pstIndexRecord->tMediaFileEndTime >= seekTime))
            {
                if (pFileNo)
                {
                    *pFileNo = iFileNo;
                }
                iRet = 0;
                __INFO("get pb file:%d,seek %u, t:%u-%u\n", iFileNo, seekTime, pstIndexRecord->tMediaFileBeginTime, pstIndexRecord->tMediaFileEndTime);
                goto endFunc;
            }
            else if ((pstIndexRecord->tMediaFileBeginTime >= seekTime) &&
                     (pstIndexRecord->tMediaFileEndTime >= seekTime))
            {
                if (iSameTime > pstIndexRecord->tMediaFileBeginTime)
                {
                    iSameTime = pstIndexRecord->tMediaFileBeginTime;
                    iSameNo = iFileNo;
                }
            }
        }
    }

    /*寻找与 seekTime 最接近的MP4文件*/
    if (iSameNo >= 0)
    {
        if (pFileNo)
        {
            *pFileNo = iSameNo;
        }
        __INFO("get pb_ex file:%d,seek %u\n", iSameNo, seekTime);
        iRet = 0;
        goto endFunc;
    }

endFunc:
    return iRet;
}

static int anj_record_pb_media_get(FILE *fp, rec_pb_media_param_s *pstPbMediaParam, rec_media_segment_index *pstCurSegMent)
{
    int iRet = -1;
    if (NULL == pstPbMediaParam || NULL == fp || NULL == pstCurSegMent)
    {
        __ERR("Invalid Input\n");
        return iRet;
    }
    int isKeyFrame = 0;
    unsigned int iFrameDuration = 0;
    rec_mov_frame_info_t *pstVFrameInfo = NULL;
    rec_mov_read_frame_info_t stVFrameInfo;
    rec_mov_read_keyframe_info_t stKeyFrameinfo;
    stVFrameInfo.iFrameStart = 0;
    stKeyFrameinfo.iKeyFrameStart = 0;
    unsigned int iKeyFrameNo = 0;
    rec_mov_frame_info_t *pstAFrameInfo = NULL;
    rec_mov_read_frame_info_t stAFrameInfo;
    stAFrameInfo.iFrameStart = 0;
    __INFO("iVKeyFrameStartNo:%d VKeyFrameNum:%d\n", pstCurSegMent->iVKeyFrameStartNo, pstCurSegMent->iVKeyFrameNum);
    iKeyFrameNo = rec_mov_read_keyframe_info(fp, &stKeyFrameinfo, pstCurSegMent->iVKeyFrameStartNo);
    if (iKeyFrameNo < pstCurSegMent->iVFrameStartNo)
    {
        __ERR("Invalid K iFrameNo %d\n", pstCurSegMent->iVFrameStartNo);
        return iRet;
    }
    int gop = 0;
    for (int i = 0; i < pstCurSegMent->iVKeyFrameNum; i++)
    {
        if (gop == (stKeyFrameinfo.iKeyIndex[i + 1] - stKeyFrameinfo.iKeyIndex[i]))
        {
            iKeyFrameNo = stKeyFrameinfo.iKeyIndex[i];
            __INFO("find normal key frame No:%d\n", iKeyFrameNo);
            break;
        }
        gop = (stKeyFrameinfo.iKeyIndex[i + 1] - stKeyFrameinfo.iKeyIndex[i]);
    }
    pstPbMediaParam->gop = gop ? gop : 60;
    pstVFrameInfo = rec_mov_read_vframe_info(fp, &stVFrameInfo, iKeyFrameNo);
    if (NULL == pstVFrameInfo)
    {
        __ERR("Invalid iFrameNo1 %d\n", iKeyFrameNo);
        return iRet;
    }
    else
    {
        pstVFrameInfo->iFrameSize = sizeof(pstPbMediaParam->metaData);
        pstPbMediaParam->metaLen = rec_mov_read_frame_data(fp, (unsigned char *)pstPbMediaParam->metaData, sizeof(pstPbMediaParam->metaData), pstVFrameInfo, &isKeyFrame, 1);
        if (pstPbMediaParam->metaLen <= 0)
        {
            __ERR("Invalid K iFrameNo %d\n", iKeyFrameNo);
            return iRet;
        }
    }

    pstAFrameInfo = rec_mov_read_aframe_info(fp, &stAFrameInfo, pstCurSegMent->iAFrameStartNo);
    if (NULL == pstAFrameInfo)
    {
        __ERR("Invalid AFrameNo %d\n", pstCurSegMent->iAFrameStartNo);
    }
    else
    {
        pstPbMediaParam->acodecType = stAFrameInfo.stRecAcodecParam.acodecType;
        pstPbMediaParam->channels = stAFrameInfo.stRecAcodecParam.channels;
        pstPbMediaParam->bitWidth = stAFrameInfo.stRecAcodecParam.bitWidth;
        pstPbMediaParam->sampleRate = stAFrameInfo.stRecAcodecParam.sampleRate;
        __INFO("samplerate:%d channels:%d samplebitswidth:%d\n",
               pstPbMediaParam->sampleRate, pstPbMediaParam->channels, pstPbMediaParam->bitWidth);
    }

    for (int i = 0; i < REC_MEDIA_BUF_FPS; i++)
    {
        if (stVFrameInfo.iFrameInfo[i].iFrameDuration > 0)
        {
            if (iFrameDuration == stVFrameInfo.iFrameInfo[i].iFrameDuration)
            {
                break;
            }
            iFrameDuration = stVFrameInfo.iFrameInfo[i].iFrameDuration;
        }
    }
    __INFO("iFrameDuration:%u\n", iFrameDuration);
    if (iFrameDuration == 0)
    {
        __ERR("Invalid iFrameNo %d\n", iKeyFrameNo);
        return iRet;
    }
    pstPbMediaParam->framerate = ROUND_DIVIDE(1000, REC_PTS_TO_MSEC(iFrameDuration));
    if (pstPbMediaParam->framerate == 0)
    {
        __ERR("Invalid iFrameNo %d\n", iKeyFrameNo);
        return iRet;
    }
    pstPbMediaParam->vcodecType = stVFrameInfo.stRecVcodecParam.vcodecType;
    pstPbMediaParam->height = stVFrameInfo.stRecVcodecParam.height;
    pstPbMediaParam->width = stVFrameInfo.stRecVcodecParam.width;
    pstPbMediaParam->bitrate = 2000;

    iRet = 0;
    return iRet;
}

static int anj_record_pb_proc(void *ctx, int *bStart)
{
    int iRet = 0;
    int iFileNo = -1;
    int iEndFileNo = 0;
    int bFirstFrame = 0;
    rec_file_index_header *pstIndexHeader = NULL;
    rec_pb_poper stPoperTmp;
    rec_pb_poper *pPoper = (rec_pb_poper *)ctx;
    rec_media_segment_index pstSegMent[REC_MEDIA_INDEX_MAX_SEGMENT];
    rec_media_segment_index *pstCurSegMent = NULL;
    unsigned char *pDataBuf = NULL;
    unsigned int pDataBufLen = 0;
    ANJ_CHK(((NULL != pPoper) && (NULL != bStart) && (NULL != pPoper->pbCb)), ANJ_ERR_INVALID_INPUT, "Invalid Input");

    if (!anj_record_pb_is_valid(pPoper))
    {
        __ERR("{exit pb} invalid pPoper = 0x%p\n", pPoper);
        iRet = -1;
        goto endFunc;
    }
    __INFO("pPoper(%d), time:%u\n", pPoper->iPopCh, pPoper->tSeekTime);
    memcpy(&stPoperTmp, pPoper, sizeof(stPoperTmp));

    while (*bStart && (pPoper->bPopStop == 0) && pPoper->bOpen)
    {
        if (pPoper->tSeekTime)
        {
            pthread_mutex_lock(&pPoper->iPopMutex);
            stPoperTmp.tSeekTime = pPoper->tSeekTime;
            pPoper->tSeekTime = 0;
            pthread_mutex_unlock(&pPoper->iPopMutex);
        }

        ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
        ANJ_CHK(((0 != *bStart) && (0 != pPoper->bOpen)), ANJ_ERR_NOT_START, "not start");

        if (stPoperTmp.tSeekTime > 0)
        {
            // 跳转新时间段
            iRet = anj_record_pb_get_fileno_by_time(stPoperTmp.iPopCh, stPoperTmp.tSeekTime, &iFileNo);
            if (0 != iRet)
            {
                __ERR("Seek invalid %u\n", stPoperTmp.tSeekTime);
                iFileNo = -1;
            }
            stPoperTmp.tPlayTime = stPoperTmp.tSeekTime;
            stPoperTmp.tSeekTime = 0;
            bFirstFrame = 1;
            iEndFileNo = iFileNo;
        }

        pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
        if (iFileNo >= 0 && iFileNo < (int)pstIndexHeader->iMediaMaxFiles && *bStart)
        {
            rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
            if (*bStart && 0 == anj_record_pb_media_file_segment(iFileNo, pstSegMent))
            {
                char fileName[128] = {0};
                FILE *fp = NULL;
                unsigned int offsetTimes = 0;
                media_frame_info_t stFrameInfo;
                rec_mov_frame_info_t *pstVFrameInfo = NULL;
                rec_mov_read_frame_info_t stVFrameInfo;
                rec_mov_read_keyframe_info_t stKeyFrameinfo;
                int isKeyFrame = 0;
                stVFrameInfo.iFrameStart = 0;
                stKeyFrameinfo.iKeyFrameStart = 0;
                unsigned int iVFrameNo = 0;
                uint64_t iOffsetDuaTime = 0;
                int bGetKeyFrame = bFirstFrame;
                int bResumeByFrameIndex = 0;
                unsigned int iResumeFrameNo = 0;
                unsigned int iKeyIndex = 0;
                unsigned int iKeyFrameNo = 0;

                unsigned int iAFrameNo = 0;
                uint64_t iOffsetDuaTimeA = 0;
                unsigned int iNextFrameOffset = 0;
                rec_mov_frame_info_t *pstAFrameInfo = NULL;
                rec_mov_read_frame_info_t stAFrameInfo;
                stAFrameInfo.iFrameStart = 0;
                int bReportMedia = 1;
                if (rec_mov_index_is_bad_file(pstIndexHeader, (unsigned int)iFileNo))
                {
                    __WARN("skip bad file %d in playback\n", iFileNo);
                    iFileNo = -1;
                }
                else
                {
                    anj_record_get_media_file_name(fileName, sizeof(fileName), iFileNo);
                    fp = fopen(fileName, "rb");
                }
                if (fp)
                {
                    int iStartSegIndex = 0;
                    if (bFirstFrame)
                    {
                        // 文件里所有覆盖 tPlayTime的片段都扫一遍 选开始时间最晚的那个片段
                        for (int i = 0; i < pstIndexRecord->iMediaFileSegRecNums; i++)
                        {
                            rec_media_segment_index *pstSeekSeg = &pstSegMent[i];
                            if ((pstSeekSeg->iMediaFileCh != stPoperTmp.iPopCh) ||
                                (pstSeekSeg->iMediaFileStatus == REC_STATUS_NULL) ||
                                (pstSeekSeg->iVFrameStartNo <= 0) ||
                                (pstSeekSeg->iVFrameNum <= 0))
                            {
                                continue;
                            }
                            if ((pstSeekSeg->tMediaFileBeginTime <= stPoperTmp.tPlayTime) &&
                                (pstSeekSeg->tMediaFileEndTime >= stPoperTmp.tPlayTime))
                            {
                                iStartSegIndex = i;
                            }
                        }
                    }

                    for (int i = iStartSegIndex; i < pstIndexRecord->iMediaFileSegRecNums && *bStart; i++)
                    {
                        pstCurSegMent = &pstSegMent[i];
                        // printf("Play file:%d,seg:%d,%d.t:%u,%u\n", iFileNo, i, pstIndexRecord->iMediaFileSegRecNums, stPoperTmp.tPlayTime, pstCurSegMent->tMediaFileEndTime);
                        if ((pstCurSegMent->iMediaFileCh != stPoperTmp.iPopCh) ||
                            (pstCurSegMent->iMediaFileStatus == REC_STATUS_NULL) ||
                            (pstCurSegMent->iVFrameStartNo <= 0) ||
                            (pstCurSegMent->iVFrameNum <= 0))
                        {
                            __ERR("Invalid file %d segment %d\n", iFileNo, i);
                            rec_mov_index_file_segment_show(pstCurSegMent);
                            break;
                        }
                        if ((pstCurSegMent->tMediaFileEndTime <= stPoperTmp.tPlayTime) ||
                            (bFirstFrame && ((pstCurSegMent->iVKeyFrameNum <= 0) || (pstCurSegMent->iVKeyFrameStartNo <= 0))))
                        {
                            continue;
                        }

                        if (stPoperTmp.tPlayTime > pstCurSegMent->tMediaFileBeginTime)
                        {
                            offsetTimes = stPoperTmp.tPlayTime - pstCurSegMent->tMediaFileBeginTime;
                        }
                        else
                        {
                            offsetTimes = 0;
                        }

                        if (!bFirstFrame &&
                            pPoper->iLastVFrameIndex >= pstCurSegMent->iVFrameStartNo &&
                            pPoper->iLastVFrameIndex < (pstCurSegMent->iVFrameStartNo + pstCurSegMent->iVFrameNum))
                        {
                            bResumeByFrameIndex = 1;
                            iResumeFrameNo = pPoper->iLastVFrameIndex + 1;
                        }
                        iVFrameNo = pstCurSegMent->iVFrameStartNo;
                        iAFrameNo = pstCurSegMent->iAFrameStartNo;
                        iOffsetDuaTime = 0;
                        /*片段跳过多少帧开始回放*/
                        while (*bStart && (pPoper->bPopStop == 0) &&
                               ((bResumeByFrameIndex && (iVFrameNo < iResumeFrameNo)) ||
                                (!bResumeByFrameIndex && (offsetTimes > (REC_PTS_TO_MSEC(iOffsetDuaTime) / 1000)))))
                        {
                            pstVFrameInfo = rec_mov_read_vframe_info(fp, &stVFrameInfo, iVFrameNo);
                            if (NULL == pstVFrameInfo)
                            {
                                __INFO("Invalid iFrameNo %d\n", iVFrameNo);
                                break;
                            }

                            iOffsetDuaTime += pstVFrameInfo->iFrameDuration;
                            iVFrameNo++;
                        }
                        iKeyIndex = pstCurSegMent->iVKeyFrameStartNo;
                        while (*bStart && (pPoper->bPopStop == 0) &&
                               (iVFrameNo < (pstCurSegMent->iVFrameStartNo + pstCurSegMent->iVFrameNum)))
                        {
                            if (pPoper->bPause)
                            {
                                usleep(10 * 1000);
                                continue;
                            }
                            pstVFrameInfo = rec_mov_read_vframe_info(fp, &stVFrameInfo, iVFrameNo);
                            if (NULL == pstVFrameInfo)
                            {
                                __ERR("Invalid iFrameNo %d\n", iVFrameNo);
                                break;
                            }
                            iOffsetDuaTime += pstVFrameInfo->iFrameDuration;

                            if (bGetKeyFrame)
                            {
                                for (; iKeyIndex < (pstCurSegMent->iVKeyFrameStartNo + pstCurSegMent->iVKeyFrameNum); iKeyIndex++)
                                {
                                    iKeyFrameNo = rec_mov_read_keyframe_info(fp, &stKeyFrameinfo, iKeyIndex);
                                    if (iKeyFrameNo < iVFrameNo)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                                if (iKeyIndex >= (pstCurSegMent->iVKeyFrameStartNo + pstCurSegMent->iVKeyFrameNum))
                                {
                                    __ERR("invlid frame k:%d,%d,%d\n", iKeyIndex, pstCurSegMent->iVKeyFrameStartNo, iVFrameNo);
                                    break;
                                }
                                if (iKeyFrameNo > iVFrameNo)
                                {
                                    iVFrameNo++;
                                    continue;
                                }
                            }

                            if (!rec_mov_read_frame_is_valid(pstVFrameInfo))
                            {
                                __ERR("frame(%d(%d~%d)) Invalid offset:%u, size:%u\n", iVFrameNo,
                                      pstCurSegMent->iVFrameStartNo, pstCurSegMent->iVFrameNum,
                                      pstVFrameInfo->iFrameOffset, pstVFrameInfo->iFrameSize);
                                iVFrameNo++;
                                continue;
                            }

                            if ((NULL == pDataBuf) || (pDataBufLen < pstVFrameInfo->iFrameSize))
                            {
                                if (pDataBuf)
                                {
                                    anj_mw_free(pDataBuf);
                                    pDataBuf = NULL;
                                }
                                if (pstVFrameInfo->iFrameSize > REC_READ_FRAME_BUF_SIZE)
                                {
                                    pDataBufLen = pstVFrameInfo->iFrameSize;
                                }
                                else
                                {
                                    pDataBufLen = REC_READ_FRAME_BUF_SIZE;
                                }
                                do
                                {
                                    pDataBuf = (unsigned char *)anj_mw_malloc(pDataBufLen);
                                    if (NULL == pDataBuf)
                                    {
                                        __INFO("anj_mw_malloc err %p, %d, continue\n", pDataBuf, pDataBufLen);
                                        usleep(500 * 1000);
                                    }
                                    else
                                    {
                                        __INFO("anj_mw_malloc %p, %d\n", pDataBuf, pDataBufLen);
                                    }
                                } while (NULL == pDataBuf);
                            }

                            pthread_mutex_lock(&s_stPbProcMutex);
                            if (0 == rec_mov_read_frame_data(fp, pDataBuf, pDataBufLen, pstVFrameInfo, &isKeyFrame, 1))
                            {
                                pthread_mutex_unlock(&s_stPbProcMutex);
                                // __INFO("###interval:%d bGetKeyFrame:%d iVFrameNo:%d\n", pPoper->iPopKeyInterval, bGetKeyFrame, iVFrameNo);
                                if ((bReportMedia || pPoper->bForceMedia) &&
                                    (0 == anj_record_pb_media_get(fp, &pPoper->tPbMediaParam, pstCurSegMent)))
                                {
                                    memset(&stFrameInfo, 0, sizeof(stFrameInfo));
                                    pPoper->pbCb(pPoper, &stFrameInfo, PB_CB_NONE);
                                    pPoper->bForceMedia = 0;
                                    bReportMedia = 0;
                                }
                                if (bGetKeyFrame && isKeyFrame)
                                {
                                    bGetKeyFrame = 0;
                                }
                                if ((bGetKeyFrame == 0) &&
                                    (((pPoper->iPopKeyInterval > 0) && isKeyFrame) || (pPoper->iPopKeyInterval == 0)))
                                {
                                    memset(&stFrameInfo, 0, sizeof(stFrameInfo));

                                    stFrameInfo.frameBuf = pDataBuf;
                                    stFrameInfo.frameParam.frameLen = pstVFrameInfo->iFrameSize;
                                    stFrameInfo.frameParam.frameCodec = stVFrameInfo.stRecVcodecParam.vcodecType;
                                    stFrameInfo.frameParam.frameType = isKeyFrame ? MEDIA_VFRAME_I : MEDIA_VFRAME_P;
                                    stFrameInfo.frameParam.frameTime = pstCurSegMent->tMediaFileBeginTime + (REC_PTS_TO_MSEC(iOffsetDuaTime) / 1000);
                                    stFrameInfo.frameParam.frameTimeMs = ((uint64_t)pstCurSegMent->tMediaFileBeginTime * 1000) + REC_PTS_TO_MSEC(iOffsetDuaTime);
                                    stFrameInfo.frameParam.framePts = pstCurSegMent->tMediaFileBeginPts + iOffsetDuaTime;
                                    stFrameInfo.frameParam.vframeIndex = iVFrameNo;
                                    pPoper->pbCb(pPoper, &stFrameInfo, PB_CB_START);
                                    pPoper->iLastVFrameIndex = iVFrameNo;
                                    bFirstFrame = 0;

                                    /*pop audio*/
                                    iNextFrameOffset = pstVFrameInfo->iFrameOffset + pstVFrameInfo->iFrameSize;
                                    pstVFrameInfo = rec_mov_read_vframe_info(fp, &stVFrameInfo, (iVFrameNo + 1));
                                    if (pstVFrameInfo)
                                    {
                                        while (*bStart && (pPoper->bPopStop == 0) && (iNextFrameOffset < pstVFrameInfo->iFrameOffset))
                                        {
                                            pstAFrameInfo = rec_mov_read_aframe_info(fp, &stAFrameInfo, iAFrameNo);
                                            if (pstAFrameInfo)
                                            {
                                                if (pstAFrameInfo->iFrameOffset <= iNextFrameOffset)
                                                {
                                                    //__ERR("AFrame No:%u offset(%u %u) iOffsetDuaTimeA:%lu %u\n", iAFrameNo, iNextFrameOffset, pstAFrameInfo->iFrameOffset, iOffsetDuaTimeA, pstAFrameInfo->iFrameDuration);
                                                    if ((pstAFrameInfo->iFrameOffset == iNextFrameOffset) &&
                                                        (0 == rec_mov_read_frame_data(fp, pDataBuf, pDataBufLen, pstAFrameInfo, &isKeyFrame, 0)))
                                                    {
                                                        memset(&stFrameInfo, 0, sizeof(stFrameInfo));

                                                        stFrameInfo.frameBuf = pDataBuf;
                                                        stFrameInfo.frameParam.frameLen = pstAFrameInfo->iFrameSize;
                                                        stFrameInfo.frameParam.frameCodec = stAFrameInfo.stRecAcodecParam.acodecType;
                                                        stFrameInfo.frameParam.frameType = MEDIA_AFRAME_A;
                                                        stFrameInfo.frameParam.frameTime = pstCurSegMent->tMediaFileBeginTime + (iOffsetDuaTimeA / 1000);
                                                        stFrameInfo.frameParam.frameTimeMs = ((uint64_t)pstCurSegMent->tMediaFileBeginTime * 1000) + iOffsetDuaTimeA;
                                                        stFrameInfo.frameParam.framePts = pstCurSegMent->tMediaFileBeginPts + REC_MSEC_TO_PTS(iOffsetDuaTimeA);
                                                        pPoper->pbCb(pPoper, &stFrameInfo, PB_CB_START);
                                                        iNextFrameOffset += pstAFrameInfo->iFrameSize;
                                                    }
                                                    else
                                                    {
                                                        //__ERR("AFrame failed\n");
                                                    }
                                                    iAFrameNo++;
                                                    iOffsetDuaTimeA += pstAFrameInfo->iFrameDuration;
                                                }
                                                else
                                                {
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
                                    }
                                    // __INFO("get frame  no: %d~%llu, %u\n", pstCurSegMent->tMediaFileBeginTime, iOffsetDuaTime, stFrameInfo.frameParam.frameTime);
                                }
                            }
                            else
                            {
                                pthread_mutex_unlock(&s_stPbProcMutex);
                                __ERR("get frame err no:%d, %d~%d\n", iVFrameNo, pstCurSegMent->iVFrameStartNo, pstCurSegMent->iVFrameNum);
                            }

                            if (pDataBufLen > REC_READ_FRAME_BUF_SIZE)
                            {
                                if (pDataBuf)
                                {
                                    anj_mw_free(pDataBuf);
                                    pDataBuf = NULL;
                                }
                                pDataBufLen = 0;
                            }
                            iVFrameNo++;
                        }
                        stPoperTmp.tPlayTime = pstCurSegMent->tMediaFileEndTime;
                    }

                    __INFO("Play file:%d end,seg:%d.t:%u\n", iFileNo, pstIndexRecord->iMediaFileSegRecNums, stPoperTmp.tPlayTime);
                    fclose(fp);
                    fp = NULL;
                }
            }

            anj_record_pb_get_next_fileno(stPoperTmp.iPopCh, stPoperTmp.tPlayTime, &iFileNo, iEndFileNo);
        }

        usleep(50 * 1000);
        // again:
        continue;
    }

endFunc:
    pPoper->pbCb(pPoper, NULL, PB_CB_FINISH);
    __ERR("End proc(%d) (run:%d,open:%d,init:%d,stop:%d)\n", pPoper->iPopCh, *bStart, pPoper->bOpen, s_stRecParam.bInit, s_stRecParam.bStop);
    if (pDataBuf)
    {
        anj_mw_free(pDataBuf);
        pDataBuf = NULL;
    }

    return 0;
}

static int anj_record_check_init(int bRemountRecover)
{
    int iRet = -1;
    char fileIndexName[128] = {0};
    char fileIndexBakName[128] = {0};

    if (0 != anj_record_check_root_dir(s_stRecParam.stRecfilePath))
    {
        s_stRecParam.iRecStatus = REC_STATUS_UNINIT;
        goto endFunc;
    }

    snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME, s_stRecParam.stRecfilePath);
    snprintf(fileIndexBakName, sizeof(fileIndexBakName), "%s" REC_INDEX_MAIN_FILE_NAME_BAK, s_stRecParam.stRecfilePath);
    if ((0 == anj_sdcard_file_exists(fileIndexName)) ||
        (0 == anj_sdcard_file_exists(fileIndexBakName)))
    {
        __ERR("sdcard record file invalid\n");
        s_stRecParam.iRecStatus = REC_STATUS_UNINIT;
        goto endFunc;
    }

    iRet = rec_mov_index_file_load(&s_stRecParam.stRecIndexParam, s_stRecParam.stRecfilePath);
    if (0 == iRet)
    {
        unsigned int i = 0;
        rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
        for (i = 0; i < s_stRecParam.iMaxPartition; i++)
        {
            s_stRecParam.iPartitionMaxFiles[i] = pstIndexHeader->iPartitionMaxFiles[i];
            if (pstIndexHeader->iPartitionMaxFiles[i] > s_stRecParam.stRecIndexParam.iMediaMaxFiles)
            {
                __ERR("rec Invalid MAX(%d) file(patition %d) max:%d\n", s_stRecParam.stRecIndexParam.iMediaMaxFiles, i,
                      pstIndexHeader->iPartitionMaxFiles[i]);
                s_stRecParam.iRecStatus = REC_STATUS_ERROR;
                goto endFunc;
            }
        }

        int bHasBadBlock = 0;
        for (i = 0; i < REC_MEDIA_BAD_BLOCK_MAP_SIZE; i++)
        {
            if (pstIndexHeader->bad_block_map[i])
            {
                bHasBadBlock = 1;
                break;
            }
        }
        /* 文件不完整 / CRC 异常卡由 pb_cache 周期扫描提示；此处仅坏块与重挂载 */
        if (bHasBadBlock)
            s_recordOsdBmp = OVERLAY_RECORD_BMP1;
        else if (bRemountRecover)
            s_recordOsdBmp = OVERLAY_RECORD_BMP2;
        else
            s_recordOsdBmp = OVERLAY_RECORD_BMP;

        if (pstIndexHeader->iNextRecMediaFileNo >= pstIndexHeader->iMediaMaxFiles)
        {
            iRet = -1;
            __ERR("Invalid Rec file no %d>%d\n", pstIndexHeader->iNextRecMediaFileNo, pstIndexHeader->iMediaMaxFiles);
            pstIndexHeader->iNextRecMediaFileNo = 0;
            s_stRecParam.iRecStatus = REC_STATUS_ERROR;
            goto endFunc;
        }

        iRet = 0;
        s_stRecParam.iRecStatus = REC_STATUS_NORMAL;
    }
    else
    {
        s_stRecParam.iRecStatus = REC_STATUS_ERROR;
    }
endFunc:
    if (0 != iRet)
    {
        if (s_stRecParam.stRecIndexParam.ptsDataBuf)
        {
            rec_mov_index_file_free(&s_stRecParam.stRecIndexParam);
        }
    }
    return iRet;
}

static int anj_record_param_set()
{
    RecordConfig *pstRecConfigArray = (RecordConfig *)getRecordConfig();
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    s_stRecAcodecParam.bitWidth = 16;
    s_stRecAcodecParam.sampleRate = pstMediaConfig->audioConfig.audioEncode.sampleRate;
    if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "PCM"))
    {
        s_stRecAcodecParam.channels = 1;
        s_stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_PCM;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "G.711A"))
    {
        s_stRecAcodecParam.channels = 1;
        s_stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_G711A;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "G.711"))
    {
        s_stRecAcodecParam.channels = 1;
        s_stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_G711U;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "AAC"))
    {
        s_stRecAcodecParam.channels = 2;
        s_stRecAcodecParam.acodecType = MEDIA_CODEC_AUDIO_AAC;
    }
    else
    {
        __ERR("unsupport audioEncodeType:%s\n", pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName);
    }

    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        RecordConfig *pstRecConfig = &pstRecConfigArray[i];
        if (pstRecConfig->scheduleRecordCfg.localStore)
        {
            if ((ANJ_CAMERA_MAX_NUMS == 1) &&
                ((pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_DUAL) ||
                 (pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_DUAL)))
            {
                for (int j = 0; j < REC_MAX_CH_NUM; j++)
                {
                    s_stRecParam.chn_param[j].bEnable = 1;
                    s_stRecParam.chn_param[j].iRecChn = j;
                    s_stRecParam.chn_param[j].iRecVenc = j;
                }
            }
            else if ((pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_MAIN) ||
                     (pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_MAIN))
            {
                s_stRecParam.chn_param[i].bEnable = 1;
                s_stRecParam.chn_param[i].iRecChn = i;
                s_stRecParam.chn_param[i].iRecVenc = i * MAX_VENC_CHN;
            }
            else if ((pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_AUX) ||
                     (pstRecConfig->scheduleRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_AUX))
            {
                s_stRecParam.chn_param[i].bEnable = 1;
                s_stRecParam.chn_param[i].iRecChn = i;
                s_stRecParam.chn_param[i].iRecVenc = i * MAX_VENC_CHN + 1;
            }
            else
            {
                __ERR("invalid record config\n");
                return -1;
            }
        }
        else if (pstRecConfig->motionRecordCfg.localStore)
        {
            if ((ANJ_CAMERA_MAX_NUMS == 1) &&
                ((pstRecConfig->motionRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_DUAL) ||
                 (pstRecConfig->motionRecordCfg.stream == RECORD_SCHED_VIDEO_DUAL)))
            {
                for (int j = 0; j < REC_MAX_CH_NUM; j++)
                {
                    s_stRecParam.chn_param[j].bEnable = 1;
                    s_stRecParam.chn_param[j].iRecChn = j;
                    s_stRecParam.chn_param[j].iRecVenc = j;
                }
            }
            else if ((pstRecConfig->inputAlarmRecordCfg.stream == RECORD_SCHED_VIDEO_MAIN) ||
                     (pstRecConfig->inputAlarmRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_MAIN))
            {
                s_stRecParam.chn_param[i].bEnable = 1;
                s_stRecParam.chn_param[i].iRecChn = i;
                s_stRecParam.chn_param[i].iRecVenc = i * MAX_VENC_CHN;
            }
            else if ((pstRecConfig->inputAlarmRecordCfg.stream == RECORD_SCHED_VIDEO_AUX) ||
                     (pstRecConfig->inputAlarmRecordCfg.stream == RECORD_SCHED_VIDEO_JPG_AUX))
            {
                s_stRecParam.chn_param[i].bEnable = 1;
                s_stRecParam.chn_param[i].iRecChn = i;
                s_stRecParam.chn_param[i].iRecVenc = i * MAX_VENC_CHN + 1;
            }
            else
            {
                __ERR("invalid record config\n");
                return -1;
            }
        }
    }

    for (int i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stRecParam.chn_param[i].bEnable)
        {
            int iRecVenc = s_stRecParam.chn_param[i].iRecVenc;
            int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (iRecVenc / ANJ_CAMERA_MAX_NUMS);
            int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? iRecVenc : (iRecVenc % ANJ_CAMERA_MAX_NUMS);
            ANJ_SIZE_S picSize = getPicSize(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].resolution.name,
                                            pstMediaConfig->videoConfig[iCameraIdex].videoCapture.tvsystem,
                                            pstMediaConfig->videoConfig[iCameraIdex].videoCapture.rotate,
                                            0);
            s_stRecVcodecParam[i].width = picSize.u32Width;
            s_stRecVcodecParam[i].height = picSize.u32Height;
            s_stRecVcodecParam[i].framerate = pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].frameRate;
            s_stRecVcodecParam[i].gop = pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].initQuant;

            if (strstr(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name, "H265"))
            {
                s_stRecVcodecParam[i].vcodecType = MEDIA_CODEC_VIDEO_H265;
            }
            else if (strstr(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name, "H264"))
            {
                s_stRecVcodecParam[i].vcodecType = MEDIA_CODEC_VIDEO_H264;
            }
            else
            {
                __ERR("rec unspport vcodec type:%s\n", pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name);
                return -1;
            }
        }
    }

    return 0;
}

static int anj_record_restart_thread(void *ctx, int *bStart)
{
    int iRet = -1;

    (void)ctx;
    __INFO("record restart begin, iRecStatus:%d\n", s_stRecParam.iRecStatus);

    anj_record_uninit();

    /* Only re-init when card is already usable; do not force NORMAL on no-card. */
    if (anj_sdcard_status_get() >= ANJ_SDCARD_STATUS_MOUNT)
    {
        char stMountPath[64] = {0};
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
        iRet = anj_record_init(stMountPath, 1, 0);
        if (0 == iRet)
        {
            anj_sdcard_status_set(ANJ_SDCARD_STATUS_NORMAL, 0);
        }
    }
    __INFO("record restart end, ret:%d\n", iRet);

    *bStart = 0;
    return iRet;
}

int anj_record_init(const char *filePath, int iMaxPartition, int bRemountRecover)
{
    int iRet = -1;
    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK((0 == s_iRecUniniting), ANJ_ERR_INVALID_STATUS, "record is uniniting");
    ANJ_CHK((0 == s_stRecParam.bInit), 0, "had beed init");
    ANJ_CHK((SDCARD_MAX_PARTITION >= iMaxPartition && iMaxPartition > 0), 0, "partition err");

    ANJ_CHK(anj_mw_file_exists(filePath), ANJ_ERR_FILE_NOT_EXIST, "Invalid Input filepath");

    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();

    if (0 != anj_record_build_root_path(s_stRecParam.stRecfilePath, sizeof(s_stRecParam.stRecfilePath), filePath))
    {
        __ERR("build record root path failed\n");
        iRet = -1;
        goto endFunc;
    }
    s_stRecParam.iMaxPartition = iMaxPartition;

    s_stRecParam.segChangTime = 3;
    if (pstRecConfig->commonCfg.timelapseCfg.timelapseEnable)
    {
        s_stRecParam.segChangTime = 2 * pstRecConfig->commonCfg.timelapseCfg.timelapseSec;
    }

    iRet = anj_record_param_set();
    if (iRet != 0)
    {
        __ERR("rec_param_set failed\n");
        goto endFunc;
    }

    iRet = anj_record_check_init(bRemountRecover);
    if (iRet != 0)
    {
        anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INIT, 1);
        __ERR("anj_record_check_init failed\n");
        goto endFunc;
    }
    anj_sdcard_status_set(ANJ_SDCARD_STATUS_NORMAL, 1);

    for (int i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stRecParam.chn_param[i].bEnable && pstRecConfig->commonCfg.localEnable)
        {
            s_stRecParam.stRecThread[i].bAutoDestroy = 0;
            snprintf(s_stRecParam.stRecThread[i].iThreadName, sizeof(s_stRecParam.stRecThread[i].iThreadName),
                     "rec_%d", s_stRecParam.chn_param[i].iRecChn);
            s_stRecParam.stRecThread[i].iThreadjob.ctx = &s_stRecParam.chn_param[i];
            s_stRecParam.stRecThread[i].iThreadjob.func = anj_record_write_proc;
            iRet = anj_thread_task_create(&s_stRecParam.stRecThread[i]);
            if (iRet)
            {
                __ERR("Create thread rec:%s, err %d\n", s_stRecParam.stRecfilePath, iRet);
                rec_mov_index_file_free(&s_stRecParam.stRecIndexParam);
                memset(&s_stRecParam, 0, sizeof(s_stRecParam));
                goto endFunc;
            }
            else
            {
                __WARN("filePath %s, %d\n", s_stRecParam.stRecfilePath, s_stRecParam.iRecStatus);
                memset(&s_stRecPbPoper, 0, sizeof(s_stRecPbPoper));
                for (int j = 0; j < REC_MAX_PB_NUM; j++)
                {
                    pthread_mutex_init(&s_stRecPbPoper[j].iPopMutex, NULL);
                }
            }
        }
    }

    if (iRet == 0)
    {
        if (anj_record_pb_cache_init() != 0)
        {
            __ERR("pb cache init failed\n");
        }

        if (record_log_init() != 0)
        {
            __ERR("record log init failed\n");
        }
    }
    s_stRecParam.bInit = 1;
    memset(s_pendingBadBlockMap, 0, sizeof(s_pendingBadBlockMap));
endFunc:
    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

int anj_record_uninit()
{
    int iRet = -1;
    int i = 0;
    int bNeedUnlock = 0;

    pthread_mutex_lock(&s_stRecMutex);
    bNeedUnlock = 1;
    ANJ_CHK((1 == s_stRecParam.bInit), ANJ_ERR_NOT_INIT, "not init");
    ANJ_CHK((0 == s_iRecUniniting), 0, "already uniniting");
    s_iRecUniniting = 1;
    s_stRecParam.bStop = 1;
    pthread_mutex_unlock(&s_stRecMutex);
    bNeedUnlock = 0;

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stRecParam.stRecThread[i].start)
        {
            iRet = anj_thread_task_destroy(&s_stRecParam.stRecThread[i], -1);
        }
    }

    for (i = 0; i < REC_MAX_PB_NUM; i++)
    {
        if (s_stRecPbPoper[i].bOpen)
        {
            anj_record_pb_release(&s_stRecPbPoper[i]);
        }
    }

    anj_record_pb_cache_uninit();
    record_log_uninit();

    for (i = 0; i < REC_MAX_PB_NUM; i++)
    {
        pthread_mutex_destroy(&s_stRecPbPoper[i].iPopMutex);
    }
    memset(&s_stRecPbPoper, 0, sizeof(s_stRecPbPoper));

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        anj_record_write_file_destroy(i, 0);
    }

    if (anj_record_bad_write(0))
    {
        anj_sdcard_remount();
        anj_record_bad_write(1);
        anj_record_write_save_index(0, 0);
    }

    pthread_mutex_lock(&s_stRecMutex);
    bNeedUnlock = 1;
    rec_mov_index_file_free(&s_stRecParam.stRecIndexParam);
    memset(&s_stRecParam, 0, sizeof(s_stRecParam));
    memset(s_pendingBadBlockMap, 0, sizeof(s_pendingBadBlockMap));
    s_iRecUniniting = 0;
    iRet = 0;
endFunc:
    if (bNeedUnlock)
    {
        if (iRet != 0)
        {
            s_iRecUniniting = 0;
        }
        pthread_mutex_unlock(&s_stRecMutex);
    }
    return iRet;
}

int anj_record_start(int iRecChannel)
{
    int iRet = -1;
    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");

    s_stRecParam.bRecStopFlag[iRecChannel] = 0;
    iRet = 0;
    anj_record_osd_set(1);
endFunc:
    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

int anj_record_stop(int iRecChannel)
{
    int iRet = -1;
    anj_record_osd_set(0);
    pthread_mutex_lock(&s_stRecMutex);
    iRet = anj_record_stop_locked(iRecChannel);
    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

int anj_record_check_valid()
{
    int iRet = -1;
    char fileIndexName[128] = {0};
    char fileIndexBakName[128] = {0};

    rec_file_index_param stRecIndexParam = {0};

    if (0 != anj_record_check_root_dir(s_stRecParam.stRecfilePath))
    {
        iRet = REC_STATUS_UNINIT;
        goto endFunc;
    }

    snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME, s_stRecParam.stRecfilePath);
    snprintf(fileIndexBakName, sizeof(fileIndexBakName), "%s" REC_INDEX_MAIN_FILE_NAME_BAK, s_stRecParam.stRecfilePath);
    if ((0 != access(fileIndexName, F_OK)) ||
        (0 != access(fileIndexBakName, F_OK)))
    {
        __ERR("sdcard record file invalid %s %s\n", fileIndexName, fileIndexBakName);
        iRet = REC_STATUS_UNINIT;
        goto endFunc;
    }

    iRet = rec_mov_index_file_load(&stRecIndexParam, s_stRecParam.stRecfilePath);
    if (0 == iRet)
    {
        unsigned int i = 0;
        char fileName[128] = {0};
        rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(&stRecIndexParam);
        for (i = 0; i < s_stRecParam.iMaxPartition; i++)
        {
            s_stRecParam.iPartitionMaxFiles[i] = pstIndexHeader->iPartitionMaxFiles[i];
            if (pstIndexHeader->iPartitionMaxFiles[i] > s_stRecParam.stRecIndexParam.iMediaMaxFiles)
            {
                __ERR("rec Invalid MAX(%d) file(patition %d) max:%d\n", s_stRecParam.stRecIndexParam.iMediaMaxFiles, i,
                      pstIndexHeader->iPartitionMaxFiles[i]);
                iRet = REC_STATUS_ERROR;
                goto endFunc;
            }
        }

        for (i = 0; i < pstIndexHeader->iMediaMaxFiles; i++)
        {
            if ((0 != anj_record_get_media_file_name(fileName, sizeof(fileName), i)) ||
                (0 != access(fileName, F_OK)))
            {
                __ERR("rec card file:%s not exist\n", fileName);
            }
        }
        if (pstIndexHeader->iNextRecMediaFileNo >= pstIndexHeader->iMediaMaxFiles)
        {
            __ERR("Invalid Rec file no %d>%d\n", pstIndexHeader->iNextRecMediaFileNo, pstIndexHeader->iMediaMaxFiles);
            pstIndexHeader->iNextRecMediaFileNo = 0;
            iRet = REC_STATUS_ERROR;
            goto endFunc;
        }

        iRet = REC_STATUS_NORMAL;
    }
    else
    {
        iRet = REC_STATUS_ERROR;
    }
endFunc:
    rec_mov_index_file_free(&stRecIndexParam);
    pthread_mutex_lock(&s_stRecMutex);
    s_stRecParam.iRecStatus = iRet;
    pthread_mutex_unlock(&s_stRecMutex);
    return (iRet != REC_STATUS_NORMAL) ? -1 : 0;
}

int anj_record_fallocate(const char *filePath, int iMaxPartition)
{
    int iRet = -1;
    uint64_t iFileRemainSize = 0;
    unsigned int iMaxMediaFiles = 0;
    unsigned int iMaxMediaFilesIndex = 0;
    int iFormatPartition = 0;
    int iTryTimes = 0;
    char stFileName[128];
    char stDirName[128];
    char stRecRoot[64] = {0};
    char fileIndexName[128] = {0};
    char fileIndexBakName[128] = {0};
    unsigned int i = 0;
    rec_file_index_header *pstIndexHeader = NULL;

    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK((0 == s_stRecParam.bStop), ANJ_ERR_NOT_INIT, "not stop");
    ANJ_CHK((REC_STATUS_FORMAT != s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "format now");
    ANJ_CHK((SDCARD_MAX_PARTITION >= iMaxPartition && iMaxPartition > 0), ANJ_ERR_INVALID_INPUT, "partition err");
    ANJ_CHK((0 == anj_record_build_root_path(stRecRoot, sizeof(stRecRoot), filePath)), ANJ_ERR_INVALID_INPUT, "build record root");

    s_stRecParam.iRecStatus = REC_STATUS_FORMAT;
    s_stRecParam.iformatProcess = anj_sdcard_fomat_percent_get();
    s_stRecParam.iMaxPartition = iMaxPartition;
    memset(s_stRecParam.iPartitionMaxFiles, 0, sizeof(s_stRecParam.iPartitionMaxFiles));

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        anj_record_write_file_destroy(i, 0);
    }

    for (i = 0; i < REC_MAX_PB_NUM; i++)
    {
        if (s_stRecPbPoper[i].bOpen)
        {
            anj_record_pb_release(&s_stRecPbPoper[i]);
        }
    }

    rec_mov_index_file_free(&s_stRecParam.stRecIndexParam);
    for (iFormatPartition = 0; iFormatPartition < iMaxPartition; iFormatPartition++)
    {
        iTryTimes = 0;
    TryAgain:
        anj_sdcard_fomat_percent_set(s_stRecParam.iformatProcess);

        if (iTryTimes++ > 2) // 2次重试
        {
            goto endFunc;
        }

        iFileRemainSize = anj_sdcard_size_get(filePath);
        __INFO("init mount:%s,record:%s,total:%lu MB, times:%d, maxpartion:%d\n",
               filePath, stRecRoot, iFileRemainSize >> 20, iTryTimes, iMaxPartition);
        if (REC_SDCAR_MIN_SIZE > iFileRemainSize)
        {
            __ERR("remain size inadequacy\n");
            goto endFunc;
        }

        iFileRemainSize -= REC_SDCAR_REMAIN_SIZE;
        iMaxMediaFiles = iFileRemainSize / REC_MEDIA_FILE_SIZE;
        if (iMaxMediaFiles > REC_MEDIA_MAX_FILE_NUM)
        {
            iMaxMediaFiles = REC_MEDIA_MAX_FILE_NUM;
        }
        anj_mw_system_free_cache();

        if (0 == iFormatPartition)
        {
            if ((0 != mkdir(stRecRoot, S_IRWXU | S_IRWXG | S_IRWXO)) && (EEXIST != errno))
            {
                __ERR("create dir:%s failed, err:%d\n", stRecRoot, errno);
                goto TryAgain;
            }

            snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME, stRecRoot);
            snprintf(fileIndexBakName, sizeof(fileIndexBakName), "%s" REC_INDEX_MAIN_FILE_NAME_BAK, stRecRoot);
            // index 创建
            iRet = anj_record_pre_create_file(fileIndexName, REC_INDEX_MAIN_FILE_SIZE);
            if (0 != iRet)
            {
                goto TryAgain;
            }

            // index 创建
            iRet = anj_record_pre_create_file(fileIndexBakName, REC_INDEX_MAIN_FILE_SIZE);
            if (0 != iRet)
            {
                goto TryAgain;
            }
        }

        s_stRecParam.iformatProcess += 5;
        anj_sdcard_fomat_percent_set(s_stRecParam.iformatProcess);

        for (i = 0; i < iMaxMediaFiles; i++)
        {
            int iFileNo = (int)(iMaxMediaFilesIndex + i);
            int iDirNo = iFileNo / REC_PRE_ALLOC_FILES_PER_DIR;

            snprintf(stDirName, sizeof(stDirName), REC_PRE_ALLOC_DIR_NAME_FMT, stRecRoot, iDirNo);
            if ((0 != mkdir(stDirName, S_IRWXU | S_IRWXG | S_IRWXO)) && (EEXIST != errno))
            {
                __ERR("create dir:%s failed, err:%d\n", stDirName, errno);
                goto TryAgain;
            }

            snprintf(stFileName, sizeof(stFileName), REC_MEDIA_FILE_NAME_BY_DIR, stRecRoot, iDirNo, iFileNo);
            iRet = anj_record_pre_create_file(stFileName, REC_MEDIA_FILE_SIZE);
            if (0 != iRet)
            {
                goto TryAgain;
            }
            int iPercent = ((90 - s_stRecParam.iformatProcess) * (i + 1)) / iMaxMediaFiles + s_stRecParam.iformatProcess;
            anj_sdcard_fomat_percent_set(iPercent);
        }
        iMaxMediaFilesIndex += iMaxMediaFiles;
        s_stRecParam.iPartitionMaxFiles[iFormatPartition] = iMaxMediaFilesIndex;
    }
    sync();
    s_stRecParam.stRecIndexParam.iMediaMaxFiles = iMaxMediaFilesIndex;
    iRet = rec_mov_index_file_load_init(&s_stRecParam.stRecIndexParam);
    if (0 != iRet)
    {
        iRet = rec_mov_index_file_load_init(&s_stRecParam.stRecIndexParam);
        if (0 != iRet)
        {
            goto endFunc;
        }
    }
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    pstIndexHeader->iVersion = REC_INDEX_VERSION_128M;
    pstIndexHeader->iMaxPartition = s_stRecParam.iMaxPartition;
    for (iFormatPartition = 0; iFormatPartition < iMaxPartition; iFormatPartition++)
    {
        pstIndexHeader->iPartitionMaxFiles[iFormatPartition] = s_stRecParam.iPartitionMaxFiles[iFormatPartition];
    }

    iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexName, 0, 0);
    if (0 != iRet)
    {
        iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexName, 0, 0);
        if (0 != iRet)
        {
            goto endFunc;
        }
    }

    iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexBakName, 0, 0);
    if (0 != iRet)
    {
        if (0 != iRet)
            iRet = rec_mov_index_file_write(&s_stRecParam.stRecIndexParam, fileIndexBakName, 0, 0);
        {
            goto endFunc;
        }
    }

    iRet = 0;
    anj_sdcard_fomat_percent_set(90);

endFunc:
    rec_mov_index_file_free(&s_stRecParam.stRecIndexParam);
    if (0 != iRet)
    {
        __ERR("Format err %d\n", iRet);
        s_stRecParam.iRecStatus = REC_STATUS_ERROR;
    }
    else
    {
        __INFO("Format success\n");
        s_stRecParam.iRecStatus = REC_STATUS_NORMAL;
    }

    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

rec_status_e anj_record_status_get()
{
    if (anj_record_bad_write(0))
    {
        return REC_STATUS_BAD_RECOVER;
    }
    return s_stRecParam.iRecStatus;
}

int anj_record_start_event(int iRecChannel, rec_event_mask_e tEvent)
{
    int iRet = -1;
    int bLock = (0 == pthread_mutex_trylock(&s_stRecMutex)) ? 1 : 0;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");

    if ((REC_EVENT_CHECK_MASK(s_stRecParam.tRecEvent[iRecChannel], REC_EVENT_NONE_ALARM_MASK) == 0) ||
        (tEvent != REC_EVENT_NONE_ALARM_MASK))
    {
        s_stRecParam.tRecEvent[iRecChannel] = REC_EVENT_SET_MASK(s_stRecParam.tRecEvent[iRecChannel], tEvent);
    }
    s_stRecParam.tAlarmTmSec[tEvent] = 0;

    __INFO("Set event mask %d, event:%x\n", tEvent, s_stRecParam.tRecEvent[iRecChannel]);

    iRet = 0;

endFunc:
    if (bLock)
    {
        pthread_mutex_unlock(&s_stRecMutex);
    }
    return iRet;
}

int anj_record_stop_event(int iRecChannel, rec_event_mask_e tEvent)
{
    int iRet = -1;
    int bLock = (0 == pthread_mutex_trylock(&s_stRecMutex)) ? 1 : 0;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");

    s_stRecParam.tRecEvent[iRecChannel] = REC_EVENT_CLEAR_MASK(s_stRecParam.tRecEvent[iRecChannel], tEvent);
    __INFO("clear event mask %d, event:%x\n", tEvent, s_stRecParam.tRecEvent[iRecChannel]);

    iRet = 0;

endFunc:
    if (bLock)
    {
        pthread_mutex_unlock(&s_stRecMutex);
    }
    return iRet;
}

int anj_record_alarm_handle(int iRecChannel, int alarm_code, int alarm_level)
{
    rec_event_mask_e tEvent = REC_EVENT_HUMEN_ALARM_MASK;
    if (alarm_code == ALARM_CODE_VIDEO_AI)
    {
        if (alarm_level == ALARM_AI_PD)
        {
            tEvent = REC_EVENT_HUMEN_ALARM_MASK;
        }
        else if (alarm_level == ALARM_AI_VEHICLE_CAR)
        {
            tEvent = REC_EVENT_CAR_ALARM_MASK;
        }
    }
    else if (alarm_code == ALARM_CODE_MOTION_DETECT)
    {
        tEvent = REC_EVENT_MOTION_ALARM_MASK;
    }
    else if (alarm_code == ALARM_CODE_IO_ALARM)
    {
        tEvent = REC_EVENT_IO_ALARM_MASK;
    }

    anj_record_start_event(iRecChannel, tEvent);
    return 0;
}

int anj_record_lastest_time(struct tm *tm_rec)
{
    int iRet = -1;
    rec_file_index_header *pstIndexHeader = NULL;
    int bLock = (0 == pthread_mutex_trylock(&s_stRecMutex)) ? 1 : 0;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");

    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    localtime_r((time_t *)&pstIndexHeader->iModifyTime, tm_rec);
    iRet = 0;

endFunc:
    if (bLock)
    {
        pthread_mutex_unlock(&s_stRecMutex);
    }
    return iRet;
}

int anj_record_get_media_file_name(char *fileName, int fileNameLen, int iFileNo)
{
    int dirIndex = 0;

    if ((NULL == fileName) || (0 >= fileNameLen))
    {
        __ERR("Invalid Input\n");
        return -1;
    }

    dirIndex = iFileNo / REC_PRE_ALLOC_FILES_PER_DIR;
    snprintf(fileName, fileNameLen, REC_MEDIA_FILE_NAME_BY_DIR, s_stRecParam.stRecfilePath, dirIndex, iFileNo);
    if (0 == access(fileName, F_OK))
    {
        return 0;
    }
    return 0;
}

int anj_record_pb_query_mounth(int iRecChannel, rec_pb_date_s *pstPbDate)
{
    int iRet = -1;
    unsigned int iFileNo = 0;
    unsigned int tBeginTime = 0;
    unsigned int tEndTime = 0;
    struct tm tm;
    rec_file_index_header *pstIndexHeader = NULL;
    rec_file_index_record *pstIndexRecord = NULL;

    if (NULL == pstPbDate)
    {
        __ERR("Invalid Input\n");
        return -1;
    }

    if ((pstPbDate->year > 2037) || (pstPbDate->year < 1970) || (pstPbDate->month > 12) || (pstPbDate->month < 1))
    {
        __ERR("Invalid Input:%d-%d\n", pstPbDate->year, pstPbDate->month);
        return -1;
    }

    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    ANJ_CHK((iRecChannel >= 0 && iRecChannel < REC_MAX_CH_NUM), ANJ_ERR_INVALID_INPUT, "Invalid Input ch");

    if (anj_record_pb_cache_query_month(iRecChannel, pstPbDate) == 0)
    {
        iRet = 0;
        goto endFunc;
    }

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = pstPbDate->year - 1900;
    tm.tm_mon = pstPbDate->month - 1;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tBeginTime = mktime(&tm);
    __INFO("Stime %d-%d-%d %d:%d:%d %us\n",
           tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tBeginTime);

    if (12 == pstPbDate->month)
    {
        tm.tm_year = tm.tm_year + 1;
        tm.tm_mon = 0;
    }
    else
    {
        tm.tm_mon = tm.tm_mon + 1;
    }
    tEndTime = mktime(&tm);
    __INFO("Etime %d-%d-%d %d:%d:%d %us\n",
           tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tEndTime);

    pstPbDate->day = 0;

    rec_media_segment_index pstSegMent[REC_MEDIA_INDEX_MAX_SEGMENT];
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    for (iFileNo = 0; iFileNo < pstIndexHeader->iMediaMaxFiles; iFileNo++)
    {
        pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
        if ((pstIndexRecord->iMediaFileCh == iRecChannel) && (pstIndexRecord->iMediaFileSegRecNums > 0 && pstIndexRecord->iMediaFileSegRecNums <= REC_MEDIA_INDEX_MAX_SEGMENT) && (pstIndexRecord->iMediaFileStatus == REC_STATUS_WRITE || pstIndexRecord->iMediaFileStatus == REC_STATUS_FULL))
        {
            if (((pstIndexRecord->tMediaFileBeginTime >= tBeginTime) && (pstIndexRecord->tMediaFileBeginTime < tEndTime)) ||
                ((pstIndexRecord->tMediaFileEndTime >= tBeginTime) && (pstIndexRecord->tMediaFileEndTime < tEndTime)))
            {
                unsigned int tStarDays = (pstIndexRecord->tMediaFileBeginTime - tBeginTime) / REC_TIME_DAY;
                unsigned int tEndDays = (pstIndexRecord->tMediaFileEndTime - tBeginTime) / REC_TIME_DAY;

                if (tStarDays > 31)
                    tStarDays = 0;

                for (; tStarDays <= tEndDays; tStarDays++)
                {
                    if (anj_record_pb_day_valid(tBeginTime, tStarDays, iFileNo, pstSegMent, pstIndexRecord))
                        pstPbDate->day = pstPbDate->day | (1U << tStarDays);
                }
            }
        }
    }
    iRet = 0;
    __INFO("rec mounth(%#x)\n", pstPbDate->day);
endFunc:
    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

int anj_record_pb_query_day_create(int iRecChannel, rec_pb_date_s *pstPbDate, rec_pb_list_s *pstPbList)
{
    int iRet = -1;
    unsigned int iFileNo = 0;
    unsigned int tBeginTime = 0;
    unsigned int tEndTime = 0;
    unsigned int tEvent = 0;
    struct tm tm;

    int nRecSegCount = 0; // 满足条件的录像片段个数
    rec_pb_segment_s *pSegHead = NULL;
    rec_pb_segment_s *pSegTail = NULL;
    rec_file_index_header *pstIndexHeader = NULL;
    rec_file_index_record *pstIndexRecord = NULL;
    rec_media_segment_index *pstSegMent = NULL;

    if (NULL == pstPbDate || NULL == pstPbList)
    {
        __ERR("Invalid Input\n");
        return -1;
    }

    if ((pstPbDate->year > 2037) || (pstPbDate->year < 1970) || (pstPbDate->month > 12) || (pstPbDate->month < 1) || (pstPbDate->day > 31) || (pstPbDate->day < 1))
    {
        __ERR("Invalid Input:%d-%d\n", pstPbDate->year, pstPbDate->month);
        return -1;
    }

    pthread_mutex_lock(&s_stRecMutex);
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    ANJ_CHK((REC_STATUS_NORMAL == s_stRecParam.iRecStatus), ANJ_ERR_INVALID_STATUS, "not normal");
    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM)
    {
        __ERR("Invalid Input ch\n");
        iRecChannel = 0;
    }

    if (anj_record_pb_cache_query_day(iRecChannel, pstPbDate, pstPbList) == 0)
    {
        iRet = 0;
        goto endFunc;
    }
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = pstPbDate->year - 1900;
    tm.tm_mon = pstPbDate->month - 1;
    tm.tm_mday = pstPbDate->day;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tBeginTime = mktime(&tm);
    __INFO("Stime %d-%d-%d %d:%d:%d %us, tEvent:%x\n",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tBeginTime, pstPbDate->tEvent);

    tEvent = (0 == pstPbDate->tEvent) ? REC_EVENT_ALL : pstPbDate->tEvent;

    tEndTime = tBeginTime + REC_TIME_DAY;
    pstSegMent = anj_mw_malloc(sizeof(rec_media_segment_index) * REC_MEDIA_INDEX_MAX_SEGMENT);
    if (pstSegMent == NULL)
    {
        __ERR("pstSegMent anj_mw_malloc failed\n");
        goto endFunc;
    }

    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    int tLastEvent = REC_EVENT_NULL_MASK;
    for (iFileNo = 0; iFileNo < pstIndexHeader->iMediaMaxFiles; iFileNo++)
    {
        pstIndexRecord = rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
        // rec_mov_index_file_record_show(pstIndexRecord, iFileNo);
        if ((pstIndexRecord->iMediaFileCh == iRecChannel) && (pstIndexRecord->iMediaFileSegRecNums > 0 && pstIndexRecord->iMediaFileSegRecNums <= REC_MEDIA_INDEX_MAX_SEGMENT) && (pstIndexRecord->iMediaFileStatus == REC_STATUS_WRITE || pstIndexRecord->iMediaFileStatus == REC_STATUS_FULL))
        {
            if (tBeginTime < pstIndexRecord->tMediaFileEndTime && tEndTime > pstIndexRecord->tMediaFileBeginTime)
            {
                memset(pstSegMent, 0, sizeof(rec_media_segment_index) * REC_MEDIA_INDEX_MAX_SEGMENT);
                // rec_media_segment_index pstSegMent[REC_MEDIA_INDEX_MAX_SEGMENT];
                if (0 == anj_record_pb_media_file_segment(iFileNo, pstSegMent))
                {
                    for (int iSegMent = 0; iSegMent < pstIndexRecord->iMediaFileSegRecNums; iSegMent++)
                    {
                        unsigned int tAddBeginTime = 0;
                        unsigned int tAddEndTime = 0;
                        if ((pstSegMent[iSegMent].iMediaFileCh != iRecChannel) ||
                            (pstSegMent[iSegMent].tMediaFileBeginTime >= pstSegMent[iSegMent].tMediaFileEndTime) ||
                            (pstSegMent[iSegMent].iMediaFileStatus == REC_STATUS_NULL) ||
                            (0 == (pstSegMent[iSegMent].tMediaFileEvent & tEvent)))
                        {
                            // __ERR("Invalid Seg\n");
                            continue;
                        }

                        if (anj_record_pb_check_in_time(pstSegMent[iSegMent].tMediaFileBeginTime, pstSegMent[iSegMent].tMediaFileEndTime, tBeginTime, tEndTime))
                        {
                            tAddBeginTime = (pstSegMent[iSegMent].tMediaFileBeginTime < tBeginTime) ? tBeginTime : pstSegMent[iSegMent].tMediaFileBeginTime;
                            tAddEndTime = (pstSegMent[iSegMent].tMediaFileEndTime > tEndTime) ? tEndTime : pstSegMent[iSegMent].tMediaFileEndTime;
                            if (pSegTail && (tLastEvent == pstSegMent[iSegMent].tMediaFileEvent))
                            {
                                /* 3秒内不分片：仅合并同事件且时间不重叠的相邻段；重叠段各自保留 */
                                if ((tAddBeginTime >= pSegTail->end_time_s) &&
                                    (tAddBeginTime <= pSegTail->end_time_s + 3))
                                {
                                    if (tAddEndTime > pSegTail->end_time_s)
                                    {
                                        pSegTail->end_time_s = tAddEndTime;
                                    }
                                    continue;
                                }
                                if ((tAddEndTime <= pSegTail->begin_time_s) &&
                                    (tAddEndTime + 3 >= pSegTail->begin_time_s))
                                {
                                    if (tAddBeginTime < pSegTail->begin_time_s)
                                    {
                                        pSegTail->begin_time_s = tAddBeginTime;
                                    }
                                    continue;
                                }
                            }
                            tLastEvent = pstSegMent[iSegMent].tMediaFileEvent;
                            anj_record_pb_add_pbseglist(&nRecSegCount, &pSegHead, &pSegTail, tAddBeginTime, tAddEndTime, pstSegMent[iSegMent].tMediaFileEvent);
                        }
                    }
                }
            }
        }
    }
    iRet = 0;
    pstPbList->count = nRecSegCount;
    pstPbList->pstSegment = pSegHead;

#if 0
    {
        rec_pb_segment_s *pHead = pSegHead;
        while (pHead != NULL)
        {
#if 1
            __INFO("rec seg(%d,%p) even(%x),time %u-%u\n", nRecSegCount, pHead,
                   pHead->tEvent,
                   pHead->begin_time_s,
                   pHead->end_time_s);
            char pTimeBuf1[128];
            char pTimeBuf2[128];
            anj_record_time_get_str(pTimeBuf1, sizeof(pTimeBuf1), pHead->begin_time_s);
            anj_record_time_get_str(pTimeBuf2, sizeof(pTimeBuf2), pHead->end_time_s);
            __INFO("rec seg(%d,%p) even(%x),time %s-%s\n", nRecSegCount, pHead,
                   pHead->tEvent,
                   pTimeBuf1,
                   pTimeBuf2);
#else
            __INFO("rec seg(%d,%p) even(%x),time %u-%u\n", nRecSegCount, pHead,
                   pHead->tEvent,
                   pHead->begin_time_s,
                   pHead->end_time_s);
#endif
            pHead = pHead->ptNext;
        }
    }
#endif

endFunc:
    if (pstSegMent)
    {
        anj_mw_free(pstSegMent);
    }
    pthread_mutex_unlock(&s_stRecMutex);
    return iRet;
}

int anj_record_pb_query_day_release(int iRecChannel, rec_pb_list_s *pstPbList)
{
    rec_pb_segment_s *pHead = NULL;
    rec_pb_segment_s *p = NULL;
    __INFO("ch:%d release %p\n", iRecChannel, pstPbList);
    if (NULL == pstPbList)
    {
        __ERR("Invalid Input\n");
        return -1;
    }
    pHead = pstPbList->pstSegment;
    while (pHead != NULL)
    {
        p = pHead->ptNext;
        anj_mw_free(pHead);
        pHead = NULL;
        pHead = p;
    }
    pstPbList->count = 0;

    return 0;
}

REC_HANDLE anj_record_pb_create(int iRecChannel, unsigned int tStartTime, unsigned int tEndTime, unsigned int iEvenType, int iPopId, rec_pb_cb pbCb)
{
    REC_HANDLE pHandle = NULL;
    int i = 0;
    int iPoperIndex = -1;

    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM || NULL == pbCb)
    {
        __ERR("Invalid Input %d,%p\n", iRecChannel, pbCb);
        return NULL;
    }

    pthread_mutex_lock(&s_stRecMutex);
    if (0 == s_stRecParam.bInit || (0 != s_stRecParam.bStop))
    {
        __ERR("not init or stop\n");
        pthread_mutex_unlock(&s_stRecMutex);
        return NULL;
    }

    if (0 == tEndTime)
    {
        tEndTime = -1;
    }

    for (i = 0; i < REC_MAX_PB_NUM; i++)
    {
        if (pthread_mutex_trylock(&s_stRecPbPoper[i].iPopMutex))
            continue;
        if (!s_stRecPbPoper[i].bOpen)
        {
            iPoperIndex = i;
            __INFO("[PkgStream]idleBackupIndex: %d\n", iPoperIndex);
            break;
        }
        pthread_mutex_unlock(&s_stRecPbPoper[i].iPopMutex);
    }

    struct tm starTime;
    localtime_r((time_t *)&tStartTime, &starTime);
    starTime.tm_hour = 0;
    starTime.tm_min = 0;
    starTime.tm_sec = 0;

    if (iPoperIndex >= 0 && iPoperIndex < REC_MAX_PB_NUM)
    {
        s_stRecPbPoper[iPoperIndex].bOpen = 1;
        s_stRecPbPoper[iPoperIndex].bPopStop = 0;
        s_stRecPbPoper[iPoperIndex].bPause = 0;
        s_stRecPbPoper[iPoperIndex].iSpeed = 0;
        s_stRecPbPoper[iPoperIndex].tPlayTime = tStartTime;
        s_stRecPbPoper[iPoperIndex].tEndTime = tEndTime;
        s_stRecPbPoper[iPoperIndex].tSeekTime = tStartTime;
        s_stRecPbPoper[iPoperIndex].tLastPts = 0;
        s_stRecPbPoper[iPoperIndex].tSendTime = 0;
        s_stRecPbPoper[iPoperIndex].iLastVFrameIndex = 0;
        s_stRecPbPoper[iPoperIndex].tEvent = iEvenType;
        s_stRecPbPoper[iPoperIndex].iPopKeyInterval = 0;
        s_stRecPbPoper[iPoperIndex].pbCb = pbCb;
        s_stRecPbPoper[iPoperIndex].iPopCh = iRecChannel;
        s_stRecPbPoper[iPoperIndex].iPopId = iPopId;
        s_stRecPbPoper[iPoperIndex].tDayStartTime = mktime(&starTime);
        s_stRecPbPoper[iPoperIndex].stThread.bAutoDestroy = 0;
        snprintf(s_stRecPbPoper[iPoperIndex].stThread.iThreadName, sizeof(s_stRecPbPoper[iPoperIndex].stThread.iThreadName), "pb_%d", iPoperIndex);
        s_stRecPbPoper[iPoperIndex].stThread.iThreadjob.ctx = (void *)&s_stRecPbPoper[iPoperIndex];
        s_stRecPbPoper[iPoperIndex].stThread.iThreadjob.func = anj_record_pb_proc;
        anj_thread_task_create(&s_stRecPbPoper[iPoperIndex].stThread);

        pthread_mutex_unlock(&s_stRecPbPoper[iPoperIndex].iPopMutex);
        pHandle = (REC_HANDLE)&s_stRecPbPoper[iPoperIndex];
    }

    pthread_mutex_unlock(&s_stRecMutex);
    return pHandle;
}

int anj_record_pb_release(REC_HANDLE pHandle)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");

    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;

    anj_thread_task_destroy(&pPoper->stThread, 0);

    pthread_mutex_lock(&pPoper->iPopMutex);
    pPoper->bOpen = 0;
    pPoper->bPopStop = 1;
    pPoper->tPlayTime = 0;
    pPoper->tEndTime = 0;
    pPoper->tEvent = 0;
    pPoper->tSeekTime = 0;
    pPoper->tLastPts = 0;
    pPoper->tSendTime = 0;
    pPoper->iLastVFrameIndex = 0;
    pPoper->iPopKeyInterval = 0;
    pPoper->pbCb = NULL;
    pthread_mutex_unlock(&pPoper->iPopMutex);
    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_pb_pause_set(REC_HANDLE pHandle, int bPause)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    pthread_mutex_lock(&pPoper->iPopMutex);
    pPoper->bPause = bPause;
    pthread_mutex_unlock(&pPoper->iPopMutex);
    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_pb_speed_set(REC_HANDLE pHandle, int iSpeed)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    pthread_mutex_lock(&pPoper->iPopMutex);
    pPoper->iSpeed = iSpeed;
    pthread_mutex_unlock(&pPoper->iPopMutex);
    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_pb_download_set(REC_HANDLE pHandle, int iDownLoad)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    pthread_mutex_lock(&pPoper->iPopMutex);
    pPoper->iDownLoad = iDownLoad;
    pthread_mutex_unlock(&pPoper->iPopMutex);
    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_pb_seek(REC_HANDLE pHandle, unsigned int tSeekTime)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;

    anj_thread_task_destroy(&pPoper->stThread, 0);

    pPoper->bSeek = 1;
    pPoper->tPlayTime = tSeekTime;
    pPoper->tEndTime = -1;
    pPoper->tSeekTime = tSeekTime;
    pPoper->tLastPts = 0;
    pPoper->tSendTime = 0;
    pPoper->iLastVFrameIndex = 0;

    anj_thread_task_create(&pPoper->stThread);

    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_pb_frame_type(REC_HANDLE pHandle, int bKeyFrame)
{
    int iRet = -1;
    ANJ_CHK(((NULL != pHandle) && (0 != s_stRecParam.bInit)), ANJ_ERR_INVALID_INPUT, "Invalid Input");
    rec_pb_poper *pPoper = NULL;
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        goto endFunc;
    }
    pPoper = (rec_pb_poper *)pHandle;
    pthread_mutex_lock(&pPoper->iPopMutex);
    pPoper->iPopKeyInterval = bKeyFrame;
    pthread_mutex_unlock(&pPoper->iPopMutex);
    iRet = 0;
endFunc:
    return iRet;
}

int anj_record_max_file_get(int *bFull, int *nextFileNo)
{
    int iRet = 0;
    rec_file_index_header *pstIndexHeader = NULL;
    ANJ_CHK(((0 != s_stRecParam.bInit) && (0 == s_stRecParam.bStop)), ANJ_ERR_NOT_INIT, "not init or stop");
    pstIndexHeader = rec_mov_index_file_get_header(&s_stRecParam.stRecIndexParam);
    ANJ_CHK(((NULL != pstIndexHeader)), ANJ_ERR_NOT_INIT, "pstIndexHeader is NULL");

    *bFull = pstIndexHeader->bFull;
    *nextFileNo = pstIndexHeader->iNextRecMediaFileNo;
    iRet = pstIndexHeader->iMediaMaxFiles;
endFunc:
    return iRet;
}

rec_file_index_record *anj_record_file_info_get(int iFileNo)
{
    if (0 == s_stRecParam.bInit)
    {
        __ERR("not init");
        return NULL;
    }

    return rec_mov_index_file_get_record(&s_stRecParam.stRecIndexParam, iFileNo);
}

void anj_record_restart()
{
    static anj_thread_s s_stRecRestartThread = {0};
    if (s_stRecRestartThread.start)
    {
        __WARN("record restart is running\n");
        return;
    }
    memset(&s_stRecRestartThread, 0, sizeof(anj_thread_s));
    s_stRecRestartThread.bAutoDestroy = 1;
    strncpy(s_stRecRestartThread.iThreadName, "rec_restart", sizeof(s_stRecRestartThread.iThreadName) - 1);
    s_stRecRestartThread.iThreadjob.ctx = (void *)&s_stRecRestartThread;
    s_stRecRestartThread.iThreadjob.func = anj_record_restart_thread;
    anj_thread_task_create(&s_stRecRestartThread);
}

static int anj_record_pb_download_finish(int timelapse, const char *FileNamePath, int result)
{
    if (!FileNamePath)
    {
        __ERR("Invalid parameters\n");
        return -1;
    }

    __ERR("Create export record timelapse:%d %s result %d\n", timelapse, FileNamePath, result);

    if (result == 0)
    {
        anj_alarm_event_handle(0, ALARM_CODE_FILE_READY_FOR_DOWNLOAD, ALARM_FLAG_OCCUR,
                               ALARM_LEVEL_EVENT, 0, FileNamePath, NULL);
    }

    return 0;
}
static void anj_record_pb_download_close(rec_pb_poper *pPoper)
{
    if (s_stDownLoadParam.pstMovInfo)
    {
        rec_mov_close_mp4(s_stDownLoadParam.pstMovInfo);
        s_stDownLoadParam.pstMovInfo = NULL;
        s_stDownLoadParam.pHandle = NULL;
        s_stDownLoadParam.firstPts = 0;
        pPoper->stThread.start = 0;
    }
}

static int anj_record_pb_download_cb(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID)
{
    int iRet = -1;
    if (pHandle == NULL)
    {
        __ERR("input param invalid");
        return iRet;
    }
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        return iRet;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    if (pPoper->iPopId < 0)
    {
        __ERR("input param invalid");
        return iRet;
    }

    if (PB_CB_NONE == EventID)
    {
        if (s_stDownLoadParam.pstMovInfo == NULL)
        {
            s_stDownLoadParam.stRecVcodecParam.vcodecType = pPoper->tPbMediaParam.vcodecType;
            s_stDownLoadParam.stRecVcodecParam.width = pPoper->tPbMediaParam.width;
            s_stDownLoadParam.stRecVcodecParam.height = pPoper->tPbMediaParam.height;
            if (s_stDownLoadParam.timelapse)
            {
                s_stDownLoadParam.stRecVcodecParam.framerate = 25;
            }
            else
            {
                s_stDownLoadParam.stRecVcodecParam.framerate = pPoper->tPbMediaParam.framerate;
            }
            s_stDownLoadParam.stRecVcodecParam.gop = pPoper->tPbMediaParam.gop;
            s_stDownLoadParam.stRecVcodecParam.bitrate = pPoper->tPbMediaParam.bitrate;

            s_stDownLoadParam.stRecAcodecParam.acodecType = pPoper->tPbMediaParam.acodecType;
            s_stDownLoadParam.stRecAcodecParam.sampleRate = pPoper->tPbMediaParam.sampleRate;
            s_stDownLoadParam.stRecAcodecParam.bitWidth = pPoper->tPbMediaParam.bitWidth;
            s_stDownLoadParam.stRecAcodecParam.channels = pPoper->tPbMediaParam.channels;

            __INFO("pb cb Start, vcodecType:%d, width:%d, height:%d, framerate:%d, gop:%d, bitrate:%d\n",
                   s_stDownLoadParam.stRecVcodecParam.vcodecType,
                   s_stDownLoadParam.stRecVcodecParam.width,
                   s_stDownLoadParam.stRecVcodecParam.height,
                   s_stDownLoadParam.stRecVcodecParam.framerate,
                   s_stDownLoadParam.stRecVcodecParam.gop,
                   s_stDownLoadParam.stRecVcodecParam.bitrate);

            __INFO("pb cb Start, acodecType:%d, sampleRate:%d, bitWidth:%d, channels:%d\n",
                   s_stDownLoadParam.stRecAcodecParam.acodecType,
                   s_stDownLoadParam.stRecAcodecParam.sampleRate,
                   s_stDownLoadParam.stRecAcodecParam.bitWidth,
                   s_stDownLoadParam.stRecAcodecParam.channels);
            s_stDownLoadParam.pstMovInfo = rec_mov_create_mp4(s_stDownLoadParam.stRecfilePath, &s_stDownLoadParam.stRecVcodecParam, &s_stDownLoadParam.stRecAcodecParam);
        }
    }
    else if (PB_CB_START == EventID)
    {
        if ((NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) || (0 >= pFrameInfo->frameParam.frameLen) || (REC_MAX_FRAME_BUF_SIZE < pFrameInfo->frameParam.frameLen))
        {
            __ERR("Invalid Input Frame\n");
            return iRet;
        }

        if (s_stDownLoadParam.pstMovInfo)
        {
            if (pFrameInfo->frameParam.frameTime > s_stDownLoadParam.tEndTime)
            {
                __INFO("anj_record_pb_download_close\n");
                anj_record_pb_download_close(pPoper);
                iRet = 0;
            }
            else
            {
                media_frame_info_t stWriteFrame = *pFrameInfo;

                // Playback uses 90k PTS; MP4 export expects ms-based framePts.
                stWriteFrame.frameParam.framePts = REC_PTS_TO_MSEC(pFrameInfo->frameParam.framePts);
                if (s_stDownLoadParam.timelapse)
                {
                    if (stWriteFrame.frameParam.frameType == MEDIA_VFRAME_I)
                    {
                        if (s_stDownLoadParam.firstPts == 0)
                        {
                            s_stDownLoadParam.firstPts = stWriteFrame.frameParam.framePts;
                        }
                        else
                        {
                            s_stDownLoadParam.firstPts += 40;
                        }
                        stWriteFrame.frameParam.framePts = s_stDownLoadParam.firstPts;
                        rec_mov_write_mp4(&stWriteFrame, s_stDownLoadParam.pstMovInfo);
                    }
                }
                else
                {
                    rec_mov_write_mp4(&stWriteFrame, s_stDownLoadParam.pstMovInfo);
                }
            }
            usleep(10 * 1000);
        }
        else
        {
            iRet = -1;
        }
    }
    else if (PB_CB_FINISH == EventID)
    {
        if (s_stDownLoadParam.pstMovInfo && pPoper->stThread.start == 0)
        {
            __INFO("anj_record_pb_download_close\n");
            anj_record_pb_download_close(pPoper);
        }

        anj_record_pb_download_finish(s_stDownLoadParam.timelapse, s_stDownLoadParam.stRecfilePath, 0);
        iRet = 0;
        __INFO("pb cb End\n");
    }
    else
    {
        __INFO("pb cb error\n");
        anj_record_pb_download_finish(s_stDownLoadParam.timelapse, s_stDownLoadParam.stRecfilePath, -1);
    }

    return iRet;
}

void anj_record_pb_download_mp4(int iRecChannel, char *filename, unsigned int tStartTime, unsigned int tEndTime, int timelapse)
{
    if (s_stDownLoadParam.pHandle == NULL)
    {
        __INFO("ch:%d, filename:%s, tStartTime:%u, tEndTime:%u\n", iRecChannel, filename, tStartTime, tEndTime);
        memset(&s_stDownLoadParam, 0, sizeof(s_stDownLoadParam));
        s_stDownLoadParam.timelapse = timelapse;
        s_stDownLoadParam.tEndTime = tEndTime;
        snprintf(s_stDownLoadParam.stRecfilePath, sizeof(s_stDownLoadParam.stRecfilePath), "%s", filename);
        s_stDownLoadParam.pHandle = (void *)anj_record_pb_create(iRecChannel, tStartTime, tEndTime, 0, 1, anj_record_pb_download_cb);
    }
}
