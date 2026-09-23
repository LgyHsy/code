#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <time.h>

#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_log.h"
#include "anj_mw_ping.h"
#include "anj_mw_thread.h"
#include "anj_mw_time.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_mbuf.h"
#include "anj_record.h"
#include "alarm_link.h"
#include "rec_mov_write.h"
#include "rec_mov_index.h"
#include "function_list.h"
#include "eventhub.h"
#include "anj_nfs.h"

#define NFS_MOUNT_PATH "/tmp/nfs"
#define NFS_DEFAULT_FILE_DURATION_SEC (10 * 60)
#define NFS_PATH_MAX 256

typedef struct
{
    int bEnable;
    int iRecChn;
    int iRecVenc;
} nfs_chn_t;

typedef struct
{
    int bInit;
    int bMounted;
    int fileDurationSec;
    nfs_chn_t chn_param[REC_MAX_CH_NUM];
    anj_thread_s stWriteThread[REC_MAX_CH_NUM];
    rec_mov_info_t *pstMovInfo[REC_MAX_CH_NUM];
    time_t alarmEndSec[REC_MAX_CH_NUM];
    unsigned int tRecEvent[REC_MAX_CH_NUM];
    unsigned long long tAlarmTmSec[REC_EVENT_MODE_MAX];
    unsigned int segChangTime;
    rec_media_vcodec_param_t stVcodec[REC_MAX_CH_NUM];
    rec_media_acodec_param_t stAcodec;
} nfs_ctx_t;

static nfs_ctx_t s_stNfsCtx = {0};
static event_nfs_storage_s s_stNfsStorageInfo = {0};
static int s_bEventSubscribed = 0;
static int s_bRestarting = 0;
static anj_thread_s s_stRestartThread = {0};

static void anj_nfs_storage_info_clear(void)
{
    memset(&s_stNfsStorageInfo, 0, sizeof(s_stNfsStorageInfo));
}

static void anj_nfs_storage_info_refresh(void)
{
    struct statfs diskInfo = {0};
    unsigned long long blocksize = 0;
    unsigned long long totalsize = 0;
    unsigned long long freeDisk = 0;

    anj_nfs_storage_info_clear();

    if (s_stNfsCtx.bMounted == 0)
    {
        return;
    }

    if (statfs(NFS_MOUNT_PATH, &diskInfo) < 0)
    {
        __ERR("nfs statfs failed:%s\n", strerror(errno));
        return;
    }

    blocksize = diskInfo.f_bsize;
    totalsize = blocksize * diskInfo.f_blocks;
    freeDisk = blocksize * diskInfo.f_bfree;

    s_stNfsStorageInfo.mounted = 1;
    s_stNfsStorageInfo.total = (int)(totalsize >> 20);
    s_stNfsStorageInfo.used = (int)((totalsize - freeDisk) >> 20);
    s_stNfsStorageInfo.free = (int)(freeDisk >> 20);
    if (s_stNfsStorageInfo.total > 0)
    {
        s_stNfsStorageInfo.percent = (int)((double)s_stNfsStorageInfo.used * 100.0 /
                                           (double)s_stNfsStorageInfo.total);
    }
    else
    {
        s_stNfsStorageInfo.percent = 100;
    }

    __INFO("nfs storage total=%dMB used=%dMB free=%dMB percent=%d\n",
           s_stNfsStorageInfo.total, s_stNfsStorageInfo.used,
           s_stNfsStorageInfo.free, s_stNfsStorageInfo.percent);
}

static int anj_nfs_mkdir_p(const char *path)
{
    char tmp[NFS_PATH_MAX] = {0};
    size_t len = 0;
    size_t i = 0;

    if (path == NULL || path[0] == '\0')
    {
        return -1;
    }

    strncpy(tmp, path, sizeof(tmp) - 1);
    len = strlen(tmp);
    if (len == 0)
    {
        return -1;
    }

    if (tmp[len - 1] == '/')
    {
        tmp[len - 1] = '\0';
    }

    for (i = 1; i < strlen(tmp); i++)
    {
        if (tmp[i] == '/')
        {
            tmp[i] = '\0';
            if (access(tmp, F_OK) != 0)
            {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                {
                    __ERR("mkdir %s failed:%s\n", tmp, strerror(errno));
                    return -1;
                }
            }
            tmp[i] = '/';
        }
    }

    if (access(tmp, F_OK) != 0)
    {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        {
            __ERR("mkdir %s failed:%s\n", tmp, strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int anj_nfs_is_enabled(void)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();

    if (pstRecConfig == NULL)
    {
        return 0;
    }

    if (pstRecConfig->commonCfg.remoteEnable != NETWORK_STORAGE_TYPE_NFS)
    {
        return 0;
    }

    if (pstRecConfig->commonCfg.mountParam[0] == '\0')
    {
        return 0;
    }

    return 1;
}

static void anj_nfs_umount(void)
{
    if (s_stNfsCtx.bMounted == 0)
    {
        anj_nfs_storage_info_clear();
        return;
    }

    anj_mw_system("umount -fl " NFS_MOUNT_PATH);
    s_stNfsCtx.bMounted = 0;
    anj_nfs_storage_info_clear();
    __INFO("nfs umount\n");
}

static int anj_nfs_try_mount(void)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    int iRet = 0;

    if (anj_nfs_is_enabled() == 0)
    {
        anj_nfs_umount();
        return -1;
    }

    if (s_stNfsCtx.bMounted)
    {
        return 0;
    }

    if (anj_nfs_mkdir_p(NFS_MOUNT_PATH) != 0)
    {
        return -1;
    }

    __INFO("mount %s %s\n", pstRecConfig->commonCfg.mountParam, NFS_MOUNT_PATH);
    sleep(1);
    iRet = anj_mw_system_with_param("mount %s %s", pstRecConfig->commonCfg.mountParam, NFS_MOUNT_PATH);
    if (iRet != 0)
    {
        __ERR("nfs mount failed ret:%d param:%s\n", iRet, pstRecConfig->commonCfg.mountParam);
        return -1;
    }

    s_stNfsCtx.bMounted = 1;
    anj_nfs_storage_info_refresh();
    __INFO("nfs mount ok: %s -> %s\n", pstRecConfig->commonCfg.mountParam, NFS_MOUNT_PATH);
    return 0;
}

static void anj_nfs_mark_stale(void)
{
    anj_nfs_umount();
}

static int anj_nfs_map_stream(int stream, int cameraIdx, int *piRecChn, int *piRecVenc)
{
    if (piRecChn == NULL || piRecVenc == NULL)
    {
        return -1;
    }

    if ((ANJ_CAMERA_MAX_NUMS == 1) &&
        ((stream == RECORD_SCHED_VIDEO_JPG_DUAL) || (stream == RECORD_SCHED_VIDEO_DUAL) || (stream == 2)))
    {
        *piRecChn = 0;
        *piRecVenc = 0;
        return 0;
    }

    if ((stream == RECORD_SCHED_VIDEO_MAIN) || (stream == RECORD_SCHED_VIDEO_JPG_MAIN) || (stream == 0))
    {
        *piRecChn = cameraIdx;
        *piRecVenc = cameraIdx * MAX_VENC_CHN;
        return 0;
    }

    if ((stream == RECORD_SCHED_VIDEO_AUX) || (stream == RECORD_SCHED_VIDEO_JPG_AUX) || (stream == 1))
    {
        *piRecChn = cameraIdx;
        *piRecVenc = cameraIdx * MAX_VENC_CHN + 1;
        return 0;
    }

    __ERR("invalid nfs stream:%d\n", stream);
    return -1;
}

static int anj_nfs_param_set(void)
{
    RecordConfig *pstRecConfigArray = (RecordConfig *)getRecordConfig();
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    int i = 0;

    memset(s_stNfsCtx.chn_param, 0, sizeof(s_stNfsCtx.chn_param));
    memset(s_stNfsCtx.stVcodec, 0, sizeof(s_stNfsCtx.stVcodec));
    memset(&s_stNfsCtx.stAcodec, 0, sizeof(s_stNfsCtx.stAcodec));

    s_stNfsCtx.segChangTime = 3;
    if (pstRecConfigArray->commonCfg.timelapseCfg.timelapseEnable)
    {
        s_stNfsCtx.segChangTime = 2 * pstRecConfigArray->commonCfg.timelapseCfg.timelapseSec;
    }

    s_stNfsCtx.fileDurationSec = pstRecConfigArray->commonCfg.recordFileSize * 60;
    if (s_stNfsCtx.fileDurationSec <= 0)
    {
        s_stNfsCtx.fileDurationSec = NFS_DEFAULT_FILE_DURATION_SEC;
    }

    s_stNfsCtx.stAcodec.bitWidth = 16;
    s_stNfsCtx.stAcodec.sampleRate = pstMediaConfig->audioConfig.audioEncode.sampleRate;
    if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "PCM"))
    {
        s_stNfsCtx.stAcodec.channels = 1;
        s_stNfsCtx.stAcodec.acodecType = MEDIA_CODEC_AUDIO_PCM;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "G.711A"))
    {
        s_stNfsCtx.stAcodec.channels = 1;
        s_stNfsCtx.stAcodec.acodecType = MEDIA_CODEC_AUDIO_G711A;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "G.711"))
    {
        s_stNfsCtx.stAcodec.channels = 1;
        s_stNfsCtx.stAcodec.acodecType = MEDIA_CODEC_AUDIO_G711U;
    }
    else if (strstr(pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName, "AAC"))
    {
        s_stNfsCtx.stAcodec.channels = 2;
        s_stNfsCtx.stAcodec.acodecType = MEDIA_CODEC_AUDIO_AAC;
    }
    else
    {
        __ERR("unsupport audioEncodeType:%s\n", pstMediaConfig->audioConfig.audioEncode.audioEncodeType.typeName);
    }

    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        RecordConfig *pstRecConfig = &pstRecConfigArray[i];
        int stream = 0;
        int iRecChn = 0;
        int iRecVenc = 0;
        int j = 0;

        if (pstRecConfig->commonCfg.remoteEnable != NETWORK_STORAGE_TYPE_NFS)
        {
            continue;
        }

        if (pstRecConfig->scheduleRecordCfg.localStore)
        {
            stream = pstRecConfig->scheduleRecordCfg.stream;
        }
        else if (pstRecConfig->motionRecordCfg.localStore)
        {
            stream = pstRecConfig->motionRecordCfg.stream;
        }
        else
        {
            continue;
        }

        if ((ANJ_CAMERA_MAX_NUMS == 1) &&
            ((stream == RECORD_SCHED_VIDEO_JPG_DUAL) || (stream == RECORD_SCHED_VIDEO_DUAL) || (stream == 2)))
        {
            for (j = 0; j < REC_MAX_CH_NUM; j++)
            {
                s_stNfsCtx.chn_param[j].bEnable = 1;
                s_stNfsCtx.chn_param[j].iRecChn = j;
                s_stNfsCtx.chn_param[j].iRecVenc = j;
            }
            continue;
        }

        if (anj_nfs_map_stream(stream, i, &iRecChn, &iRecVenc) != 0)
        {
            return -1;
        }

        s_stNfsCtx.chn_param[iRecChn].bEnable = 1;
        s_stNfsCtx.chn_param[iRecChn].iRecChn = iRecChn;
        s_stNfsCtx.chn_param[iRecChn].iRecVenc = iRecVenc;
    }

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stNfsCtx.chn_param[i].bEnable)
        {
            int iRecVenc = s_stNfsCtx.chn_param[i].iRecVenc;
            int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (iRecVenc / ANJ_CAMERA_MAX_NUMS);
            int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? iRecVenc : (iRecVenc % ANJ_CAMERA_MAX_NUMS);
            ANJ_SIZE_S picSize = getPicSize(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].resolution.name,
                                            pstMediaConfig->videoConfig[iCameraIdex].videoCapture.tvsystem,
                                            pstMediaConfig->videoConfig[iCameraIdex].videoCapture.rotate,
                                            0);

            s_stNfsCtx.stVcodec[i].width = picSize.u32Width;
            s_stNfsCtx.stVcodec[i].height = picSize.u32Height;
            s_stNfsCtx.stVcodec[i].framerate = pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].frameRate;
            s_stNfsCtx.stVcodec[i].gop = pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].initQuant;

            if (strstr(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name, "H265"))
            {
                s_stNfsCtx.stVcodec[i].vcodecType = MEDIA_CODEC_VIDEO_H265;
            }
            else if (strstr(pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name, "H264"))
            {
                s_stNfsCtx.stVcodec[i].vcodecType = MEDIA_CODEC_VIDEO_H264;
            }
            else
            {
                __ERR("nfs unspport vcodec type:%s\n",
                      pstMediaConfig->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn].encodeFormat.name);
                return -1;
            }
        }
    }

    //把s_stNfsCtx的参数都打印出来
    __INFO("nfs param set ok fileDurationSec:%d\n", s_stNfsCtx.fileDurationSec);
    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        __INFO("nfs chn:%d enable:%d recChn:%d recVenc:%d\n", i, s_stNfsCtx.chn_param[i].bEnable, s_stNfsCtx.chn_param[i].iRecChn, s_stNfsCtx.chn_param[i].iRecVenc);
    }
    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        __INFO("nfs vcodec:%d width:%d height:%d framerate:%d gop:%d vcodecType:%d\n", i, s_stNfsCtx.stVcodec[i].width, s_stNfsCtx.stVcodec[i].height, s_stNfsCtx.stVcodec[i].framerate, s_stNfsCtx.stVcodec[i].gop, s_stNfsCtx.stVcodec[i].vcodecType);
    }
    return 0;
}

/* mountParam 格式: -t nfs -o nolock host:/path */
static int anj_nfs_get_server_host(char *host, int hostLen)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    const char *param = NULL;
    const char *p = NULL;
    const char *colon = NULL;
    int len = 0;

    if (host == NULL || hostLen <= 0 || pstRecConfig == NULL)
    {
        return -1;
    }

    param = pstRecConfig->commonCfg.mountParam;
    p = strrchr(param, ' ');
    p = (p == NULL) ? param : (p + 1);

    colon = strchr(p, ':');
    if (colon == NULL || colon == p)
    {
        return -1;
    }

    len = (int)(colon - p);
    if (len <= 0 || len >= hostLen)
    {
        return -1;
    }

    memcpy(host, p, len);
    host[len] = '\0';
    return 0;
}

/* 是否应录像：条件满足时 ping 服务器成功再 mount */
static int anj_nfs_need_record(int iRecChannel)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    time_t now = time(NULL);
    int bNeed = 0;
    char host[64] = {0};

    if (100000000 > now)
    {
        return 0;
    }

    if (anj_nfs_is_enabled() == 0)
    {
        __INFO("nfs is not enabled\n");
        return 0;
    }

    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM)
    {
        __INFO("nfs is not enabled\n");
        return 0;
    }

    if (s_stNfsCtx.chn_param[iRecChannel].bEnable == 0)
    {
        __INFO("nfs is not enabled\n");
        return 0;
    }

    if (pstRecConfig->scheduleRecordCfg.localStore)
    {
        if (0 == CheckNowIsInTimeSpan(&pstRecConfig->scheduleRecordCfg.timeSpan))
        {
            __INFO("nfs is not enabled\n");
            return 0;
        }
        bNeed = 1;
    }
    else if (pstRecConfig->motionRecordCfg.localStore)
    {
        bNeed = (s_stNfsCtx.alarmEndSec[iRecChannel] > now) ? 1 : 0;
    }

    if (bNeed == 0)
    {
        return 0;
    }

    if (s_stNfsCtx.bMounted)
    {
        return 1;
    }

    if (anj_nfs_get_server_host(host, sizeof(host)) != 0)
    {
        __ERR("nfs parse server host failed param:%s\n", pstRecConfig->commonCfg.mountParam);
        return 0;
    }

    if (try_ping(host, 1000, 1, NULL, NULL) != 0)
    {
        __INFO("nfs ping server %s failed\n", host);
        return 0;
    }

    if (anj_nfs_try_mount() != 0)
    {
        return 0;
    }

    return 1;
}

static void anj_nfs_event_uptime(int iRecChannel, unsigned long long framePts)
{
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    int tEvent = 0;

    for (tEvent = REC_EVENT_MOTION_ALARM_MASK; tEvent < REC_EVENT_MODE_MAX; tEvent++)
    {
        if (REC_EVENT_CHECK_MASK(s_stNfsCtx.tRecEvent[iRecChannel], tEvent) == 0)
        {
            continue;
        }

        if (s_stNfsCtx.tAlarmTmSec[tEvent] == 0)
        {
            s_stNfsCtx.tAlarmTmSec[tEvent] = framePts;
        }
        else if ((framePts - s_stNfsCtx.tAlarmTmSec[tEvent]) > (unsigned long long)pstRecConfig->motionRecordCfg.recordTime * 1000)
        {
            s_stNfsCtx.tRecEvent[iRecChannel] = REC_EVENT_CLEAR_MASK(s_stNfsCtx.tRecEvent[iRecChannel], tEvent);
            s_stNfsCtx.tAlarmTmSec[tEvent] = 0;
            __INFO("nfs event finish ch:%d mask:%d\n", iRecChannel, tEvent);
        }
    }
}

static int anj_nfs_build_filepath(int iRecChannel, time_t tStart, char *outPath, int outLen)
{
    unsigned char sn[64] = {0};
    struct tm tmLocal = {0};
    char dirPath[NFS_PATH_MAX] = {0};

    if (outPath == NULL || outLen <= 0)
    {
        return -1;
    }

    if (anj_sysmng_get_sn(sn, sizeof(sn)) < 0)
    {
        __ERR("get sn failed\n");
        return -1;
    }

    localtime_r(&tStart, &tmLocal);
    snprintf(dirPath, sizeof(dirPath), NFS_MOUNT_PATH "/%s/%04d/%02d/%02d/%02d",
             (char *)sn, tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday, iRecChannel);

    if (anj_nfs_mkdir_p(dirPath) != 0)
    {
        return -1;
    }

    snprintf(outPath, outLen, "%s/%02d%02d%02d.mp4", dirPath,
             tmLocal.tm_hour, tmLocal.tm_min, tmLocal.tm_sec);
    return 0;
}

static void anj_nfs_close_file(int iRecChannel)
{
    rec_mov_info_t *pstMovInfo = NULL;

    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM)
    {
        return;
    }

    pstMovInfo = s_stNfsCtx.pstMovInfo[iRecChannel];
    if (pstMovInfo == NULL)
    {
        return;
    }

    if (pstMovInfo->pstIndexRecord)
    {
        free(pstMovInfo->pstIndexRecord);
        pstMovInfo->pstIndexRecord = NULL;
    }
    rec_mov_close_mp4(pstMovInfo);
    s_stNfsCtx.pstMovInfo[iRecChannel] = NULL;
}

static int anj_nfs_open_file(int iRecChannel, time_t tStart)
{
    char filePath[NFS_PATH_MAX] = {0};
    rec_mov_info_t *pstMovInfo = NULL;
    rec_file_index_record *pstIndexRecord = NULL;

    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM)
    {
        return -1;
    }

    anj_nfs_close_file(iRecChannel);

    if (anj_nfs_build_filepath(iRecChannel, tStart, filePath, sizeof(filePath)) != 0)
    {
        return -1;
    }

    pstMovInfo = rec_mov_create_mp4(filePath, &s_stNfsCtx.stVcodec[iRecChannel], &s_stNfsCtx.stAcodec);
    if (pstMovInfo == NULL)
    {
        __ERR("rec_mov_create_mp4 %s failed\n", filePath);
        return -1;
    }

    pstIndexRecord = (rec_file_index_record *)calloc(1, sizeof(rec_file_index_record));
    if (pstIndexRecord == NULL)
    {
        rec_mov_close_mp4(pstMovInfo);
        return -1;
    }
    pstIndexRecord->iMediaFileCh = (unsigned char)iRecChannel;
    pstMovInfo->pstIndexRecord = pstIndexRecord;

    s_stNfsCtx.pstMovInfo[iRecChannel] = pstMovInfo;
    __INFO("nfs open %s\n", filePath);
    return 0;
}

static int anj_nfs_write_frame(int iRecChannel, media_frame_info_t *pFrameInfo)
{
    rec_mov_info_t *pstMovInfo = NULL;
    int iRet = 0;

    if (iRecChannel < 0 || iRecChannel >= REC_MAX_CH_NUM || pFrameInfo == NULL)
    {
        return -1;
    }

    pstMovInfo = s_stNfsCtx.pstMovInfo[iRecChannel];
    if (pstMovInfo == NULL)
    {
        return -1;
    }

    if (rec_mov_write_mp4(pFrameInfo, pstMovInfo) < 0)
    {
        /* write_mp4 多数路径成功时也可能返回 -1，仅在后续 index 失败时判坏盘 */
    }

    iRet = rec_mov_write_box_write_update_index(pstMovInfo, pFrameInfo,
                                               s_stNfsCtx.tRecEvent[iRecChannel],
                                               s_stNfsCtx.segChangTime);
    if (iRet < 0)
    {
        return -1;
    }

    if (access(NFS_MOUNT_PATH, W_OK) != 0)
    {
        return -1;
    }

    return 0;
}

static int anj_nfs_write_proc(void *ctx, int *bStart)
{
    nfs_chn_t *pstChnParam = (nfs_chn_t *)ctx;
    int iRecChannel = 0;
    int VencChn = 0;
    int iFrameSize = 0;
    unsigned char *pFrameBuf = NULL;
    ANJ_MBUF_HANDLE *readerid = NULL;
    media_frame_info_t stReadFrameInfo = {0};
    RecordConfig *pstRecConfig = (RecordConfig *)getRecordConfig();
    int audio_enable = 1;
    time_t tSegStart = 0;      /* RecordFileSize 逻辑时段起点 */
    int bNeedNewFile = 1;
    int bRefreshSegStart = 1;  /* 时长切分/新开录时刷新 tSegStart；128MB 切分不刷 */

    if (pstChnParam == NULL || bStart == NULL)
    {
        return -1;
    }

    iRecChannel = pstChnParam->iRecChn;
    VencChn = pstChnParam->iRecVenc;
    audio_enable = (strcmp(pstRecConfig->scheduleRecordCfg.mediaType.typeName, "VIDEO") == 0) ? 0 : 1;

    if (ANJ_CAMERA_MAX_NUMS > 1)
    {
        iFrameSize = (VencChn % ANJ_CAMERA_MAX_NUMS) ? ANJ_CAMERA_VIDEO_SUB_MAX_SIZE : ANJ_CAMERA_VIDEO_MAX_SIZE;
    }
    else
    {
        iFrameSize = VencChn ? ANJ_CAMERA_VIDEO_SUB_MAX_SIZE : ANJ_CAMERA_VIDEO_MAX_SIZE;
    }

    pFrameBuf = anj_mw_malloc(iFrameSize);
    if (pFrameBuf == NULL)
    {
        __ERR("malloc failed\n");
        return -1;
    }

    while (bStart && *bStart)
    {
        /* need_record 内：需录像时 ping 通再 try_mount */
        if (anj_nfs_need_record(iRecChannel) == 0)
        {
            anj_nfs_close_file(iRecChannel);
            bNeedNewFile = 1;
            bRefreshSegStart = 1;
            if (readerid)
            {
                anj_mbuf_destory_reader(readerid);
                readerid = NULL;
            }
            usleep(200 * 1000);
            continue;
        }

        if (readerid == NULL)
        {
            readerid = anj_mbuf_create_reader(VencChn, 1);
            if (readerid == NULL)
            {
                usleep(100 * 1000);
                continue;
            }
            __INFO("nfs mbuf reader chn:%d ok\n", VencChn);
        }

        int bFirstFrame = 1;
        unsigned int iLastVFrameIndex = 0;
        while (bStart && *bStart && anj_nfs_need_record(iRecChannel) && s_stNfsCtx.bMounted)
        {
            if (0 >= anj_mbuf_read_frame(readerid, bFirstFrame, &stReadFrameInfo, (bFirstFrame) ? 2000 : 200))
            {
                continue;
            }

            if (stReadFrameInfo.frameParam.frameLen > iFrameSize)
            {
                pFrameBuf = anj_mw_realloc(pFrameBuf, stReadFrameInfo.frameParam.frameLen);
                iFrameSize = stReadFrameInfo.frameParam.frameLen;
            }
            memcpy(pFrameBuf, stReadFrameInfo.frameBuf, stReadFrameInfo.frameParam.frameLen);
            anj_mbuf_read_release(readerid, &stReadFrameInfo);
            stReadFrameInfo.frameBuf = pFrameBuf;

            if (stReadFrameInfo.frameParam.frameTime < (time(NULL) - 3600))
            {
                continue;
            }

            if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
            {
                if ((iLastVFrameIndex != 0) && (stReadFrameInfo.frameParam.vframeIndex != iLastVFrameIndex + 1))
                {
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

            /* 文件未开时必须等到视频 I 帧；已开文件后丢帧恢复也等 I */
            if (bFirstFrame)
            {
                continue;
            }

            /* 仅在 I 帧上判定切段；新文件首帧也必须是 I */
            if (stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_I)
            {
                rec_mov_info_t *pstMov = s_stNfsCtx.pstMovInfo[iRecChannel];
                time_t now = time(NULL);

                if (pstMov != NULL)
                {
                    if ((tSegStart != 0) && ((now - tSegStart) >= s_stNfsCtx.fileDurationSec))
                    {
                        anj_nfs_close_file(iRecChannel);
                        bNeedNewFile = 1;
                        bRefreshSegStart = 1;
                    }
                    else if (pstMov->stMediaFileOffset >= (REC_MEDIA_FILE_SIZE - REC_FILE_REMAIN_SIZE))
                    {
                        /* 同逻辑时段内 128MB 续开下一文件，不刷新 tSegStart */
                        anj_nfs_close_file(iRecChannel);
                        bNeedNewFile = 1;
                    }
                }
            }

            if (bNeedNewFile)
            {
                time_t tOpen = 0;

                if (stReadFrameInfo.frameParam.frameType != MEDIA_VFRAME_I)
                {
                    continue;
                }
                tOpen = time(NULL);
                if (anj_nfs_open_file(iRecChannel, tOpen) != 0)
                {
                    anj_nfs_mark_stale();
                    break;
                }
                if (bRefreshSegStart || (tSegStart == 0))
                {
                    tSegStart = tOpen;
                    bRefreshSegStart = 0;
                }
                bNeedNewFile = 0;
            }

            if (s_stNfsCtx.pstMovInfo[iRecChannel])
            {
                if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                {
                    anj_nfs_event_uptime(iRecChannel, stReadFrameInfo.frameParam.framePts);
                }

                if (anj_nfs_write_frame(iRecChannel, &stReadFrameInfo) != 0)
                {
                    __ERR("nfs write failed, wait restart\n");
                    anj_nfs_close_file(iRecChannel);
                    bNeedNewFile = 1;
                    bRefreshSegStart = 1;
                    anj_nfs_mark_stale();
                    break;
                }
            }
        }

        anj_nfs_close_file(iRecChannel);
        bNeedNewFile = 1;
        bRefreshSegStart = 1;
        if (readerid)
        {
            anj_mbuf_destory_reader(readerid);
            readerid = NULL;
        }
    }

    anj_nfs_close_file(iRecChannel);
    if (readerid)
    {
        anj_mbuf_destory_reader(readerid);
    }
    if (pFrameBuf)
    {
        anj_mw_free(pFrameBuf);
    }

    __INFO("nfs write exit ch:%d\n", iRecChannel);
    return 0;
}

static rec_event_mask_e anj_nfs_alarm_to_event(int code, int level)
{
    if (code == ALARM_CODE_VIDEO_AI)
    {
        if (level == ALARM_AI_PD)
        {
            return REC_EVENT_HUMEN_ALARM_MASK;
        }
        if (level == ALARM_AI_VEHICLE_CAR)
        {
            return REC_EVENT_CAR_ALARM_MASK;
        }
        return REC_EVENT_HUMEN_ALARM_MASK;
    }
    if (code == ALARM_CODE_MOTION_DETECT)
    {
        return REC_EVENT_MOTION_ALARM_MASK;
    }
    if (code == ALARM_CODE_IO_ALARM)
    {
        return REC_EVENT_IO_ALARM_MASK;
    }
    return REC_EVENT_MOTION_ALARM_MASK;
}

static void anj_nfs_alarm_handler(EventResult *event_result, void *data)
{
    event_alarm_s *pstAlarm = (event_alarm_s *)data;
    int chn = 0;
    rec_event_mask_e tEvent = REC_EVENT_MOTION_ALARM_MASK;

    (void)event_result;

    if (pstAlarm == NULL || s_stNfsCtx.bInit == 0)
    {
        return;
    }

    chn = pstAlarm->chn;
    if (chn < 0 || chn >= REC_MAX_CH_NUM)
    {
        chn = 0;
    }

    tEvent = anj_nfs_alarm_to_event(pstAlarm->code, pstAlarm->level);
    s_stNfsCtx.tRecEvent[chn] = REC_EVENT_SET_MASK(s_stNfsCtx.tRecEvent[chn], tEvent);
    s_stNfsCtx.tAlarmTmSec[tEvent] = 0;
    s_stNfsCtx.alarmEndSec[chn] = time(NULL) + s_stNfsCtx.fileDurationSec;

    __INFO("nfs alarm chn:%d code:%d level:%d event:%d end:%ld\n",
           chn, pstAlarm->code, pstAlarm->level, tEvent, (long)s_stNfsCtx.alarmEndSec[chn]);
}

static int anj_nfs_restart_proc(void *ctx, int *bStart)
{
    (void)ctx;
    (void)bStart;

    __INFO("nfs restart begin bInit:%d\n", s_stNfsCtx.bInit);
    if (s_stNfsCtx.bInit)
    {
        anj_nfs_uninit();
    }
    anj_nfs_init();
    s_bRestarting = 0;
    __INFO("nfs restart end\n");
    return 0;
}

static void anj_nfs_restart_handler(EventResult *event_result, void *data)
{
    (void)event_result;
    (void)data;

    if (s_bRestarting)
    {
        return;
    }

    s_bRestarting = 1;
    memset(&s_stRestartThread, 0, sizeof(s_stRestartThread));
    s_stRestartThread.bAutoDestroy = 1;
    strncpy(s_stRestartThread.iThreadName, "nfs_rst", sizeof(s_stRestartThread.iThreadName) - 1);
    s_stRestartThread.iThreadjob.ctx = NULL;
    s_stRestartThread.iThreadjob.func = anj_nfs_restart_proc;
    if (anj_thread_task_create(&s_stRestartThread) != 0)
    {
        s_bRestarting = 0;
        __ERR("create nfs restart thread failed\n");
    }
}

static void anj_nfs_storage_get_handler(EventResult *event_result, void *data)
{
    (void)data;

    if (event_result == NULL)
    {
        return;
    }

    event_result->result = (void *)&s_stNfsStorageInfo;
}

int anj_nfs_mounted_get(void)
{
    return s_stNfsStorageInfo.mounted ? 1 : 0;
}

int anj_nfs_init(void)
{
    int i = 0;

    if (s_stNfsCtx.bInit)
    {
        return 0;
    }

    memset(&s_stNfsCtx, 0, sizeof(s_stNfsCtx));
    anj_nfs_storage_info_clear();

    anj_sysctl_capability_add(FUNCTION_NETWORK_STORAGE);

    if (anj_nfs_param_set() != 0)
    {
        __ERR("anj_nfs_param_set failed\n");
    }

    if (s_bEventSubscribed == 0)
    {
        eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_NFS_ALARM, anj_nfs_alarm_handler);
        eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_NFS_RESTART, anj_nfs_restart_handler);
        eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_NFS_STORAGE_GET, anj_nfs_storage_get_handler);
        s_bEventSubscribed = 1;
    }

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stNfsCtx.chn_param[i].bEnable == 0)
        {
            continue;
        }

        memset(&s_stNfsCtx.stWriteThread[i], 0, sizeof(anj_thread_s));
        s_stNfsCtx.stWriteThread[i].bAutoDestroy = 1;
        snprintf(s_stNfsCtx.stWriteThread[i].iThreadName, sizeof(s_stNfsCtx.stWriteThread[i].iThreadName),
                 "nfs_%d", i);
        s_stNfsCtx.stWriteThread[i].iThreadjob.ctx = &s_stNfsCtx.chn_param[i];
        s_stNfsCtx.stWriteThread[i].iThreadjob.func = anj_nfs_write_proc;
        if (anj_thread_task_create(&s_stNfsCtx.stWriteThread[i]) != 0)
        {
            __ERR("create nfs write thread %d failed\n", i);
            return -1;
        }
    }

    s_stNfsCtx.bInit = 1;
    __INFO("anj_nfs_init ok\n");
    return 0;
}

int anj_nfs_uninit(void)
{
    int i = 0;

    if (s_stNfsCtx.bInit == 0)
    {
        return 0;
    }

    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        if (s_stNfsCtx.stWriteThread[i].start)
        {
            anj_thread_task_destroy(&s_stNfsCtx.stWriteThread[i], 10000);
            memset(&s_stNfsCtx.stWriteThread[i], 0, sizeof(anj_thread_s));
        }
    }

    /* 线程退出后兜底关文件，再 umount，避免 mdat/moov 未落盘 */
    for (i = 0; i < REC_MAX_CH_NUM; i++)
    {
        anj_nfs_close_file(i);
    }

    anj_nfs_umount();
    s_stNfsCtx.bInit = 0;
    __INFO("anj_nfs_uninit ok\n");
    return 0;
}

REGISTER_MODULE(anj_nfs, MODULE_PRIORITY_NFS);
