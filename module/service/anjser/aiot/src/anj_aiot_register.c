#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_audio.h"
#include "anj_sysmng.h"
#include "anj_systime.h"
#include "anj_sys.h"
#include "anj_ser_api.h"
#include "anj_bind.h"
#include "anj_module.h"
#include "anj_aiot.h"
#include "anj_aiot_cmd.h"
#include "anj_aiot_stream.h"
#include "anj_aiot_trans.h"

#include "media_util.h"
#include "audio_utils.h"
#include "file_receiver.h"
#include "anj_sdcard.h"
#include "audio_receiver.h"
#include "eventhub.h"
#include "record_log.h"

#include "gct_common.h"
#include "gct_types.h"

#include "gct_callback.h"
#include "gct_dd_callback.h"
#include "gct_dd_apiv4.h"
#include "gct_apiv4.h"

#define FIRM_UPLOAD_FILENAME "ota.bin" // 固件升级保存文件名称

void FindSavePathForBigFile(int nFileLen, char *filepath)
{
    anj_sdcard_info *pstSdInfo = anj_sdcard_info_get();
    if (pstSdInfo->eStatus == ANJ_SDCARD_STATUS_NORMAL ||
        pstSdInfo->eStatus == ANJ_SDCARD_STATUS_NOT_INIT)
    {
        if (pstSdInfo->iRemainSize >= nFileLen * 2)
        {
            sprintf(filepath, SDCARD_MOUNT_PATH, pstSdInfo->iIndex);
            return;
        }
    }

    __INFO("Have no SDCard, use /tmp for big file length %u\n", nFileLen);
    strcpy(filepath, "/tmp");
}

/*	时间同步回调接口 (一般是手机做同步使用)
 *  app -->device
 *  nTimeZone:			时区值,[-12,12]
 *  nSvrTs: 			服务器的时间戳 1970年到现在的秒数
    return :			1 = 成功 0 = 失败
 */
GCT_INT32 anj_aiot_timesyn_cb(const GCT_INT32 nTimeZone, const GCT_UINT64 nSvrTs)
{
    __INFO("nTimeZone=%d, UTC=%u\n", nTimeZone, nSvrTs);
    // 调试发现LBS下发的同一个SVR时间戳，先调用GLNK_TimeSyn_CallBack2，然后调用GLNK_TimeSyn_CallBack的时间差有3秒。为避免校时后往回调整时间，因此去掉这个回调的实现
    return 0;
}

GCT_VOID anj_aiot_timesyn_cb2(const GCT_UINT64 nSvrTs)
{
    __INFO("UTC=%llu\n", nSvrTs);
    struct tm time;
    SystemConfig *pSystemConfig = (SystemConfig *)getSystemConfig();
    TimeConfig *pTimeConfig = &pSystemConfig->timeCfg;
    localtime_r((time_t *)&nSvrTs, &time);
    anj_systime_set_only(time, pTimeConfig->timeZone, 1);

    return;
}

GCT_VOID anj_aiot_stream_info_cb(const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype, gct_stream_data_format *pstream_info)
{
    gct_video_data_format *pstVideoFormat = &pstream_info->videoFormat;
    gct_audio_data_format *pstAudioFormat = &pstream_info->audioFormat;

    int iIndex = (streamtype == 0) ? 0 : 1;

    anj_aiot_media_info_get(iIndex, pstVideoFormat, pstAudioFormat);
}

GCT_VOID anj_aiot_request_idr_cb(const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype, const GCT_IFRAME_REASON euGCT_IFRAME_REASON)
{
    anj_video_request_idr(nChannelNo, streamtype);
    __INFO("nChannelNo = %d,streamtype = %d\n", nChannelNo, streamtype);
}

GCT_VOID anj_aiot_get_4g_iccid_cb(GCT_CHAR *piccid1, GCT_UINT8 *pniccid1_status, GCT_CHAR *piccid2, GCT_UINT8 *pniccid2_status, GCT_CHAR *pSignalStr)
{
    strcpy(piccid1, "898607B9192070060984");
    *pniccid1_status = 1;

    // strcpy(piccid2,"5555555555555555555");
    (void)piccid2;
    *pniccid2_status = 0;

    strcpy(pSignalStr, "{\"time\":\"2022-04-20 23:45:11\",\"signal\":65}");
}

GCT_VOID anj_aiot_get_4g_imei_cb(GCT_CHAR *pImei)
{
    strcpy(pImei, "865281051235729");
}

GCT_INT32 anj_aiot_ota_version_cb(GCT_CHAR *appbuf, GCT_CHAR *solbuf, GCT_CHAR *date, GCT_CHAR *hardware)
{
    DevInfo *pstDevInfo = getDevInfo();
    strcpy(hardware, pstDevInfo->subDevType);
    strcpy(appbuf, "public");
    strcpy(solbuf, "AIOT");
    strcpy(date, pstDevInfo->release_date);
    *(date + 8) = 0;

    __INFO("%s %s %s %s\n", appbuf, solbuf, hardware, date);

    return 0;
}

GCT_INT32 anj_aiot_ota_req_cb(const GCT_UINT64 nFirmFileLen, const GCT_CHAR *pMd5Value)
{
    __INFO("nFirmFileLen = %lld,pMd5Value = %s\n", (long long int)nFirmFileLen, pMd5Value);
    anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_START_UPDATE, 1);
    modules_uninit("anj_ser", "anj_net");
    return file_recver_init(NULL, nFirmFileLen, UPLOAD_FIRMWARE_FILE_TYPE, 0, MSG_SRC_SER);
}

/*	固件下载中(可选对接,如果对接了该接口则不会写入固件文件了)
 *	pBody 该片段数据
 *	nBodyLen  该片段数据长度
 *	return 0 = 成功,其他为失败
 */
GCT_INT32 anj_aiot_ota_feed_cb(const GCT_CHAR *pBody, const GCT_UINT64 nBodyLen)
{
    __INFO("nBodyLen = %lld\n", (long long int)nBodyLen);

    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
    {
        // 固件下载getmma会kill掉media_server,这里每次回调发一次心跳保活，在固件开始ota再关掉
        char cmd[100] = {0};
        sprintf(cmd, "echo -e -n \"\\xFC\\x01\\xA3\\x01\\x00\\xA1\" > /dev/ttyS2");
        anj_mw_system(cmd);
    }

    file_recver_feed_data(pBody, nBodyLen);

    return 0;
}

GCT_INT32 anj_aiot_ota_start_cb(const GCT_CHAR *pFileAbsPath, const GCT_CHAR *pWebFileName)
{
    // pFileAbsPath就是 gct_apiv4_firmware_update_set_savepath 设置的路径
    __INFO("pFileAbsPath = %s\n", pFileAbsPath);
    file_recver_t *pFileReceiver = getFileRecver();
    if (anj_mw_file_exists(pFileAbsPath) && pFileReceiver == NULL)
    {
        APPBIN_UPDATE_DATA updateData = {0};
        snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", pFileAbsPath);
        updateData.nPhyAddr = 0;
        updateData.nFileLen = 0;
        anj_sysmng_app_update(&updateData);
    }

    if (pFileReceiver)
    {
        if (pFileReceiver->pMappedAddr)
        {
            anj_sys_munmap(pFileReceiver->pMappedAddr, pFileReceiver->filelen);
            pFileReceiver->pMappedAddr = NULL;
        }
        file_recver_stop(0);

        APPBIN_UPDATE_DATA updateData = {0};
        snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", pFileReceiver->filename);
        updateData.nPhyAddr = pFileReceiver->u32PhyAddr;
        updateData.nFileLen = pFileReceiver->writelen;
        anj_sysmng_app_update(&updateData);
    }
    return 0;
}

GCT_INT32 anj_aiot_talking_cb(const GCT_UINT32 nChannelNo, const GCT_BOOL bOpen)
{
    __INFO("ch:%d, open=%d\n", nChannelNo, bOpen);

    return 1;
}

GCT_VOID anj_aiot_talking_feed_cb(const GCT_UINT32 nChannelNo, const GCT_UINT32 nStreamTime, const GCT_VOID *pData, const GCT_UINT32 nDataLen)
{
    if (pData == NULL || nDataLen == 0)
    {
        return;
    }

    // __INFO("recv audio data len = %d\n", nDataLen);
    audio_talk_feed_audio((char *)pData, nDataLen, MEDIA_CODEC_AUDIO_G711U, 0, 0);
}

int32_t anj_aiot_bigfile_req_cb(const int nBuffAllLen, const char *pFileName)
{
    char BigDataFilePath[64] = {0};
    char BigDataFileName[128] = {0};

    // 找个地方存储固件
    FindSavePathForBigFile(nBuffAllLen, BigDataFilePath);

    sprintf(BigDataFileName, "%s/", BigDataFilePath);
    if (NULL != pFileName && strlen(pFileName) > 0)
    { // 有文件名传输过来，为文件传输方式
        strcat(BigDataFileName, pFileName);
    }
    else
    { // 没有文件名传输过来的，一般为大buff传输
        strcat(BigDataFileName, "big_data.bin");
    }
    __INFO("nBuffAllLen = %d,filename = %s\n", nBuffAllLen, BigDataFileName);

    remove(BigDataFileName);

    file_recver_big_init(BigDataFileName, nBuffAllLen);

    return 0;
}

int32_t anj_aiot_bigfile_feed_cb(const int nBuffLen, const char *pBuff)
{
    __INFO("nBuffLen = %d\n", nBuffLen);
    file_recver_feed_data(pBuff, nBuffLen);
    return 0;
}

int32_t anj_aiot_bigfile_stop_cb()
{
    file_recver_uninit(0);
    return 0;
}

GCT_VOID anj_aiot_snap_jpg_cb(const GCT_UINT32 nChannelNo, const GCT_UINT32 nAlarmType, GCT_CHAR **ppData, GCT_UINT32 *pnLen)
{
    // 根据报警类型查看是否需要图片
    int bSnap = 1; // todo GetEventNeedSnap(nAlarmType);
    __INFO("ch:%u, alarmtype %d, bSnap=%d\n", nChannelNo, nAlarmType, bSnap);
    if (1 != bSnap)
    {
        *ppData = NULL;
        *pnLen = 0;
        return;
    }

    int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (nChannelNo / ANJ_CAMERA_MAX_NUMS);
    char filename[128] = {0};

    struct timeval tv;
    SystemGetTimeofRun(&tv, NULL);
    struct tm ptm;
    SystemLocalTime(&ptm);
    snprintf(filename, sizeof(filename), "snap_ch%d_%04d%02d%02d%02d%02d%02d_%03d.jpg", nChannelNo, 
        ptm.tm_year + 1900,
        ptm.tm_mon + 1,
        ptm.tm_mday,
        ptm.tm_hour,
        ptm.tm_min,
        ptm.tm_sec,
        (int)(tv.tv_usec / 1000));
    anj_snap_jpg(iCameraIdex, 1, 50, "/tmp", filename, NULL);
    char pathname[256] = {0};
    snprintf(pathname, sizeof(pathname), "/tmp/%s", filename);
    if (0 == anj_snap_wait_complete(pathname, 500))
    {
        unsigned long long len = 0;
        anj_mw_read_file_len(pathname, &len);
        *ppData = malloc(len);
        anj_mw_read_file_limit_len(pathname, *ppData, len);
        *pnLen = (GCT_UINT32)len;
        __INFO("snap OK, data=%#x, datalen=%u.\n", *ppData, *pnLen);
        remove(pathname);
    }
    else
    {
        __ERR("snap failed.\n");
    }
}

/*	接收数据回调函数接口，透明通道接收数据
 * 	sid	 	           连接ID
 * 	pBuf：              接收到的数据
 * 	nBufLen：			接收到的数据长度
 * 	retutn : 			0--失败，数据继续走老接口
 * 			   			1--成功，数据提取成功
 */
GCT_INT32 anj_aiot_trans_push_cb(const GCT_CHAR *pBuf, const GCT_UINT32 nBufLen, const GCT_UINT32 sid)
{
    anj_aiot_cmd_push((char *)pBuf, nBufLen, sid);
    return 1;
}

GCT_INT32 anj_aiot_ptzctl_cb(const GLNK_PTZControlCmd ptzcmd, const GCT_UINT32 channel, const ControlArgData *arg)
{
    __INFO("PTZ channel[%d] cmd:%d \n", channel, ptzcmd);

    int speed_target = 5;
    if (arg != NULL)
        speed_target = arg->arg1;

    EventResult event_result = {0};
    PtzCmdParse stPtzCmdParse = {0};

    switch (ptzcmd)
    {
    case GLNK_PTZ_MV_STOP:
    {
        strncpy(stPtzCmdParse.ptzCmd, "stop", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_ZOOM_DEC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "zoomwide", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_ZOOM_INC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "zoomtele", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_FOCUS_INC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "FocusNearAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_FOCUS_DEC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "FocusFarAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_MV_UP:
    {
        strncpy(stPtzCmdParse.ptzCmd, "up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_DOWN:
    {
        strncpy(stPtzCmdParse.ptzCmd, "down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_LEFT:
    {
        strncpy(stPtzCmdParse.ptzCmd, "left", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_RIGHT:
    {
        strncpy(stPtzCmdParse.ptzCmd, "right", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_IRIS_INC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "IrisCloseAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_IRIS_DEC:
    {
        strncpy(stPtzCmdParse.ptzCmd, "IrisOpenAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    }
    case GLNK_PTZ_MV_LEFTUP:
    {
        strncpy(stPtzCmdParse.ptzCmd, "left_up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_LEFTDOWN:
    {
        strncpy(stPtzCmdParse.ptzCmd, "left_down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_RIGHTUP:
    {
        strncpy(stPtzCmdParse.ptzCmd, "right_up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_MV_RIGHTDOWN:
    {
        strncpy(stPtzCmdParse.ptzCmd, "right_down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = speed_target;
        stPtzCmdParse.tiltSpeed = speed_target;
        break;
    }
    case GLNK_PTZ_SET_PRESET:
    {
        strncpy(stPtzCmdParse.ptzCmd, "setpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = arg->arg1;
        stPtzCmdParse.flag = 1;
        break;
    }
    case GLNK_PTZ_GOTO_PRESET:
    {
        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = arg->arg1;
        stPtzCmdParse.flag = 1;
        break;
    }
    case GLNK_PTZ_CLEAR_PRESET:
    {
        strncpy(stPtzCmdParse.ptzCmd, "clearpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = arg->arg1;
        stPtzCmdParse.flag = 1;
        break;
    }
    /*
    case GLNK_PTZ_AUTO_CRUISE:
    {
        __INFO("auto cruise!\n");
        break;
    }
    case GLNK_PTZ_ACTION_RESET :
    {
        __INFO("action reset!\n");
        break;
    }
    case GLNK_PTZ_CLEAR_TOUR :
    {
        __INFO("clear tour!\n");
        break;
    }
    case GLNK_PTZ_ADD_PRESET_TO_TOUR :
    {
        __INFO("preset tour!\n");
        break;
    }
    case GLNK_PTZ_DEL_PRESET_TO_TOUR :
    {
        __INFO("delete tour!\n");
        break;
    }*/
    default:
    {
        __INFO("Not handled PTZ cmd %d\n", ptzcmd);
        break;
    }
    }
    if (strlen(stPtzCmdParse.ptzCmd))
    {
        __INFO("cmd:%s\n", stPtzCmdParse.ptzCmd);
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }
    return 1;
}

// 回调设备是否被APP绑定
// bIsBind, GCT_TRUE = 已被绑定, GCT_FALSE = 已被解绑
GCT_VOID anj_aiot_bind_state_cb(const GCT_BOOL bIsBind)
{
    __INFO("bIsBind=%d\n", bIsBind);
    if (bIsBind)
    {
        anj_bind_success();
    }
    else
    {
        remove(P2P_DEVICEBIND_FLAG);
    }
}

GCT_INT32 anj_aiot_restore_cb()
{
    __INFO("FactoryReset\n");
    return 1;
}

GCT_INT32 anj_aiot_search_wifi_cb(GCT_CHAR **ppBuff)
{
    __INFO("SearchWifi\n");

    GCT_UINT32 nWifiCount = 2;
    GCT_CHAR *pBuff = (GCT_CHAR *)malloc(sizeof(gct_wifi_info) * nWifiCount);
    if (NULL == pBuff)
    {
        return 0;
    }

    gct_wifi_info wifiInfo;
    memset(&wifiInfo, 0, sizeof(wifiInfo));
    strcpy(wifiInfo.name, "XXXX1");
    strcpy(wifiInfo.ssid, "XXXX1");
    wifiInfo.euGCT_WIFI_SIGNAL_LEVEL = GCT_WIFI_SIGNAL_LEVEL_STRONG;

    GCT_UINT32 nOffsetLen = 0;
    memcpy(pBuff, (GCT_VOID *)&wifiInfo, sizeof(wifiInfo));
    nOffsetLen += sizeof(wifiInfo);

    memset(&wifiInfo, 0, sizeof(wifiInfo));
    strcpy(wifiInfo.name, "XXXX1");
    strcpy(wifiInfo.ssid, "XXXX1");
    wifiInfo.euGCT_WIFI_SIGNAL_LEVEL = GCT_WIFI_SIGNAL_LEVEL_STRONG;

    memcpy(pBuff + nOffsetLen, (GCT_VOID *)&wifiInfo, sizeof(wifiInfo));
    nOffsetLen += sizeof(wifiInfo);

    *ppBuff = pBuff;
    return nWifiCount;
}

GCT_INT32 anj_aiot_wifi_config_cb(const gct_wifi_config_req *preq)
{
    __INFO("WifiConfig ssid = %s,password = %s\n", preq->ssid, preq->password);
    return 1;
}

/*	格式化硬盘(SD卡)列表接口
 *  app -->device
 *  StorageID：		GLNK_DeviceStorageList->StorageID，硬盘(sd卡)ID，和获取时的id保持一致
 *  return		格式化结果回复0:格式化失败，1:格式化成功，2:无权限
 */
GCT_INT32 anj_aiot_format_storage_cb(const GCT_INT32 StorageID)
{
    __INFO("SD Format start \n");
    anj_sdcard_format(0);
    __INFO("SD Format End \n");
    return 1;
}

GCT_VOID anj_aiot_remount_sdcard_cb()
{
    __INFO("SD Remount start \n");
    anj_sdcard_umount();
    anj_sdcard_mount();
    __INFO("SD Remount End \n");
}

GCT_VOID anj_aiot_network_status_cb(GCT_CHAR *Internet_status, GCT_CHAR *Addr, GCT_CHAR *Connection, GCT_CHAR *Signal)
{
    strcpy(Internet_status, "Connected");
    strcpy(Addr, "192.168.1.103");
    strcpy(Connection, "Wi-Fi/Wired");
    strcpy(Signal, "95");
}

// 检查sdcard是否正常
// return ,0 = 卡正常(sd卡插好+sd卡目录(比如 /mnt)上面的容量显示真实sd卡的容量),1 = 卡异常(sd卡插上了+ sd卡目录(比如 /mnt)上面的容量显示真实sd卡的容量,设备库sd卡录像会停止), 2 = sd卡没插
GCT_INT32 anj_aiot_sdcard_status_cb()
{
    // 以下代码仅供参考,不要照搬,根据实际情况填写
    // 第一个步骤: 检查sd卡是否插上了

    // 第二个步骤: 检查sd卡是否异常(比如没有加载驱动成功，没有mount成功等)
    // if (XX){
    //	return 1;  //sdcard异常
    // }

    // 第三个步骤:sdcard正常
    return 2;
}

static GCT_INT32 anj_aiot_reboot_cb(const GCT_BOOL bImmediatelyDoIt)
{
    __WARN("Reboot service request from aiot\n");
    __RECORD_LOG_INFO("Reboot service request from aiot\n");
    anj_sysmng_delay_reboot(1);
    return 0;
}

static GCT_VOID anj_aiot_cloud_packet_cb(const GCT_INT32 nCloudP)
{
    __INFO("Cloud storage=%u\n", nCloudP);
    anj_ser_info *pstSerInfo = getSerInfo();
    if (pstSerInfo->stP2pLoginState.cloudstorage != nCloudP)
    {
        pstSerInfo->stP2pLoginState.cloudstorage = nCloudP;
    }
}

void anj_aiot_callback_init()
{
    // 连接发起回调
    gct_cb_reg_login(anj_aiot_stream_add);
    // 连接退出回调
    gct_cb_reg_logout(anj_aiot_stream_del);
    gct_cb_reg_videoswitch(anj_aiot_stream_switch);

    gct_cb_reg_time_syn(anj_aiot_timesyn_cb);
    gct_cb_reg_svr_ts(anj_aiot_timesyn_cb2);

    // 设备格式信息，比如编码帧率等
    gct_cb_reg_streaminfo(anj_aiot_stream_info_cb);
    // 强插i帧
    gct_cb_reg_iframe(anj_aiot_request_idr_cb);

    gct_cb_reg_ptz_op(anj_aiot_ptzctl_cb);

#if 0	
	//4G的iccid
	gct_cb_reg_4g_iccid(anj_aiot_get_4g_iccid_cb);
	//4G的imei
	gct_cb_reg_4g_imei(anj_aiot_get_4g_imei_cb);
#endif
    // 固件的版本号获取
    gct_cb_reg_firmupdate_getversion(anj_aiot_ota_version_cb);
    gct_cb_reg_firmupdate_req(anj_aiot_ota_req_cb);
    // 固件升级回调
    gct_cb_reg_firmupdate_start(anj_aiot_ota_start_cb);
    // 固件内容回调
    gct_cb_reg_firmupdate_body(anj_aiot_ota_feed_cb);
    // 对讲请求
    gct_cb_reg_talking(anj_aiot_talking_cb);
    // 对讲数据回调
    gct_cb_reg_talking_audio(anj_aiot_talking_feed_cb);
    // 抓取缩略图回调
    gct_cb_reg_screenshots(anj_aiot_snap_jpg_cb);
    // APP传输大数据给设备端，比如文件等
    gct_cb_reg_trans_bigdata_req(anj_aiot_bigfile_req_cb);
    gct_cb_reg_trans_bigdata_ing(anj_aiot_bigfile_feed_cb);
    gct_cb_reg_trans_bigdata_end(anj_aiot_bigfile_stop_cb);
    // 透明通道回调
    gct_cb_reg_trans_channel(anj_aiot_trans_push_cb);
    // 回调设备APP绑定状态
    gct_cb_reg_bind_state(anj_aiot_bind_state_cb);
    // 恢复出厂
    gct_cb_reg_factory_reset(anj_aiot_restore_cb);
    // 搜索wifi
    //	gct_cb_reg_wifi_search(anj_aiot_search_wifi_cb);
    // 配置wifi
    //	gct_cb_reg_wifi_config(anj_aiot_wifi_config_cb);
    // 格式化sd卡
    gct_cb_reg_sdcard_format(anj_aiot_format_storage_cb);
    // 回调处理SD卡重新加载
    //	gct_cb_reg_remount_sdcard_script(anj_aiot_remount_sdcard_cb);
    // 获取设备的wifi状态回调
    gct_cb_reg_dev_network_information(anj_aiot_network_status_cb);
    // 查询sd卡是否正常
    gct_cb_reg_chk_sdcard_normal(anj_aiot_sdcard_status_cb);

    gct_cb_reg_reboot_dev(anj_aiot_reboot_cb);
    // 云存储套餐回调
    gct_cb_reg_cloud_p(anj_aiot_cloud_packet_cb);
}
