#ifndef __ANJ_SNAP_H__
#define __ANJ_SNAP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int xPos;   /* 0-100 */
    int yPos;   /* 0-100 */
    int width;  /* 0-100 */
    int height; /* 0-100 */
} AreaStruct;

/*
 * cam: 摄像头索引 [0, ANJ_CAMERA_MAX_NUMS)
 * stream: 码流索引 0=主码流 1=子码流 [0, MAX_VENC_CHN)
 */
int anj_snap_jpg(int cam, int stream, int quality, char *filePath, char *fileName, AreaStruct *are);
int anj_snap_wait_complete(const char *filename, int timeout_ms);

/*
 * 软编 YUV 入口。iCameraIdex 表示 YUV 源；
 * 抓拍管道只有一套，仅当队头请求的 cam 匹配时才拷贝。
 */
void anj_snap_on_yuv(int iCameraIdex, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param);

#ifdef __cplusplus
}
#endif

#endif
