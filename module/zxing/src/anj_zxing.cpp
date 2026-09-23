#include <unistd.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>

#include "BarcodeFormat.h"
#include "DecodeHints.h"
#include "ImageView.h"
#include "ReadBarcode.h"

#include "anj_mw_comm.h"
#include "anj_module.h"
#include "eventhub.h"
#include "anj_base64.h"
#include "anj_bind.h"

using namespace ZXing; // 添加zxing名称空间

#define ZXING_RUN_INT_TIME (200 * 1000) // ZXING识别间隔时间

#define ZXING_BIND_DATA_MAX_CHAR (BIND_DATA_MAX_CHAR) // 最大字节数
#define ZXING_BIND_DATA_MAX_COUNT (20)                // 数据最多个数 二维码最大个数

// 二维码数据结构
typedef struct
{
    char nDataBuf[ZXING_BIND_DATA_MAX_COUNT][ZXING_BIND_DATA_MAX_CHAR];
    int bUseBuf[ZXING_BIND_DATA_MAX_COUNT];
    int mTotalNum; // 总数量
} Zxing_data_stu;

// 图片数据结构
typedef struct
{
    unsigned char *pData; // 图片Y800 数据
    int width;            // 图片宽
    int height;           // 图片高度
} Zxing_image_stu;

// 二维码信息状态结构
typedef struct
{
    int bRead;  // 是否能读mImageInfo
    int bWrite; // 是否能写mImageInfo
    Zxing_image_stu mImageInfo;
} Zxing_Info_stu;

static Zxing_Info_stu mZXingInfo; // ZXing信息
static anj_thread_s s_stZXingThread = {0};
static Zxing_data_stu mZXingData; // ZXING 数据
static int s_stZXingRun = 0;
pthread_mutex_t s_read_mutex = PTHREAD_MUTEX_INITIALIZER; // 参数互斥锁

static int anj_zxing_data_proc(const char *zxing_data, char *zxing_out, int out_len)
{
    int iRet = -1;
    if ((zxing_data == NULL) || (zxing_out == NULL) || (out_len <= 0))
    {
        __ERR("input Invalid\n");
        return iRet;
    }
    char index_str[32] = {0};
    char field_data[64] = {0};

    // 解析结果格式: "索引/总段数;数据内容"
    if (sscanf(zxing_data, "%[^;];%s", index_str, field_data) != 2)
    {
        __ERR("Error: Invalid zxing_data format: %s\n", zxing_data);
        return iRet;
    }

    __INFO("Processing zxing_data: %s, index_str: %s, field_data: %s\n",
           zxing_data, index_str, field_data);

    // 解析索引和段数
    int iCurNum = 0;
    int iTotalNum = 0;
    if (sscanf(index_str, "%d/%d", &iCurNum, &iTotalNum) != 2)
    {
        __ERR("Error: Invalid index format: %s\n", index_str);
        return iRet;
    }

    // 总页数变更则替换更新数据
    if (mZXingData.mTotalNum != iTotalNum)
    {
        memset(&mZXingData, 0, sizeof(mZXingData));
        mZXingData.mTotalNum = iTotalNum;
    }

    int zxing_index = iCurNum - 1;
    strncpy(mZXingData.nDataBuf[zxing_index], field_data, sizeof(mZXingData.nDataBuf[zxing_index]) - 1);
    mZXingData.bUseBuf[zxing_index] = 1;
    __INFO("Get BindInfo num %d,%d\n", mZXingData.mTotalNum, zxing_index);

    // 检测数据是否完整
    int bUseAll = 1;
    for (int i = 0; i < mZXingData.mTotalNum; i++)
    {
        if (0 == mZXingData.bUseBuf[i])
        {
            bUseAll = 0;
        }
    }

    if (bUseAll)
    {
        // 数据拼接
        strncpy(zxing_out, mZXingData.nDataBuf[0], out_len - 1);
        for (int i = 1; i < mZXingData.mTotalNum; i++)
        {
            strcat(zxing_out, mZXingData.nDataBuf[i]);
        }
        // 清空数据
        memset(&mZXingData, 0, sizeof(mZXingData));
        iRet = 0;
    }
    return iRet;
}

/*****************************************************************************
 函 数 名  : anj_zxing_run
 功能描述  : 二维码识别
 输入参数  : 无
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static int anj_zxing_run(Zxing_image_stu *pImageInfo)
{
    if ((NULL == pImageInfo) || (NULL == pImageInfo->pData))
    {
        __ERR("Invalid Input\n");
        return 0;
    }

    // 1) 将输入的Y800灰度图封装为 ZXing 的 ImageView，不拷贝数据
    // rowStride 传 width，表示每行紧密排布。
    ImageView imageView((const uint8_t *)pImageInfo->pData,
                        pImageInfo->width,
                        pImageInfo->height,
                        ImageFormat::Lum,
                        pImageInfo->width);

    // 2) 配置解码参数：仅识别二维码，并开启更鲁棒的识别策略。
    // - TryHarder: 提高识别成功率（耗时会增加）
    // - TryRotate: 尝试旋转角度
    // - TryInvert: 尝试反色二维码
    // - MaxNumberOfSymbols: 单帧最多识别数量
    DecodeHints hints;
    hints.setFormats(BarcodeFormat::QRCode)
        .setTryHarder(true)
        .setTryRotate(true)
        .setTryInvert(true)
        .setMaxNumberOfSymbols(ZXING_BIND_DATA_MAX_COUNT);

    // 3) 调用 ZXing 批量识别接口，返回一个结果数组。
    Results results = ReadBarcodes(imageView, hints);

    char zxing_out[ZXING_BIND_DATA_MAX_CHAR * ZXING_BIND_DATA_MAX_COUNT] = {0};
    for (const auto &result : results)
    {
        // 4) 过滤无效结果（如校验失败/噪声命中）。
        if (!result.isValid())
        {
            continue;
        }

        // 5) 取解码文本（UTF-8 字符串），为空则跳过。
        std::string qrText = result.text();
        if (qrText.empty())
        {
            continue;
        }

        __INFO("Get ZXing Data:%s\n", qrText.c_str());
        // 6) 复用现有分包拼接逻辑，拼接完整后做 base64 解码并进入绑定流程。
        if (0 == anj_zxing_data_proc(qrText.c_str(), zxing_out, sizeof(zxing_out)))
        {
            char zxing_decode[ZXING_BIND_DATA_MAX_CHAR * ZXING_BIND_DATA_MAX_COUNT] = {0};
            anj_base64_decode(zxing_out, sizeof(zxing_out), (uint8_t *)zxing_decode, sizeof(zxing_decode));
            anj_bind_data_proc(zxing_decode, sizeof(zxing_decode), BIND_TYPE_ZXING);
            break;
        }
    }

    return 0;
}

static int anj_zxing_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    // 初试数据状态可写，不可读
    mZXingInfo.bRead = 0;
    mZXingInfo.bWrite = 1;
    __INFO("anj_zxing_thread start\n");

    while (bStart && *bStart)
    {
        if (s_stZXingRun)
        {
            if (mZXingInfo.bRead)
            {
                if (!anj_bind_get())
                {
                    // 读数据前限制不能写数据
                    mZXingInfo.bWrite = 0;
                    anj_mutex_lock(&s_read_mutex);

                    anj_zxing_run(&mZXingInfo.mImageInfo);

                    anj_mutex_unlock(&s_read_mutex);

                    // 数据已经被读状态
                    mZXingInfo.bRead = 0;
                }
            }
            else
            {
                // 没有读数据时 可以写数据
                mZXingInfo.bWrite = 1;
            }
        }

        usleep(ZXING_RUN_INT_TIME);
    }
    return iRet;
}

/*****************************************************************************
 函 数 名  : anj_zxing_set_image
 功能描述  : 二维码识别绑定退出
 输入参数  :  unsigned char* pData   图片数据buf，数据Y800
 输出参数  :
 返 回 值  :  0成功，其他失败
*****************************************************************************/
static void anj_zxing_set_image(EventResult *event_result, void *data)
{
    event_yuv_s *pstYuv = (event_yuv_s *)data;
    int y_size = 0;

    if (!s_stZXingRun || !s_stZXingThread.start || !mZXingInfo.bWrite || (pstYuv == NULL) ||
        (pstYuv->data == NULL) || (pstYuv->width <= 0) || (pstYuv->height <= 0) || anj_bind_get())
    {
        return;
    }

    anj_mutex_lock(&s_read_mutex);
    if (s_stZXingThread.start && mZXingInfo.bWrite && !anj_bind_get())
    {
        y_size = pstYuv->width * pstYuv->height;
        if ((mZXingInfo.mImageInfo.pData == NULL) ||
            (mZXingInfo.mImageInfo.width != pstYuv->width) ||
            (mZXingInfo.mImageInfo.height != pstYuv->height))
        {
            if (mZXingInfo.mImageInfo.pData != NULL)
            {
                anj_mw_free(mZXingInfo.mImageInfo.pData);
                mZXingInfo.mImageInfo.pData = NULL;
            }
            mZXingInfo.mImageInfo.pData = (unsigned char *)anj_mw_malloc(y_size);
            if (mZXingInfo.mImageInfo.pData == NULL)
            {
                __FATAL("malloc buf fail\n");
                anj_mutex_unlock(&s_read_mutex);
                return;
            }
            mZXingInfo.mImageInfo.width = pstYuv->width;
            mZXingInfo.mImageInfo.height = pstYuv->height;
        }
        memcpy(mZXingInfo.mImageInfo.pData, pstYuv->data, y_size);
        mZXingInfo.bRead = 1;
    }
    anj_mutex_unlock(&s_read_mutex);
}

static void anj_zxing_status_set(EventResult *event_result, void *data)
{
    if (data)
    {
        s_stZXingRun = *(int *)data;
        __INFO("s_stZXingRun:%d\n", s_stZXingRun);
    }
}

/*****************************************************************************
 函 数 名  : Zxing_pro_Init
 功能描述  : 二维码识别绑定初始化
 输入参数  :
 int nWith        ZXING图像宽度
 int nHeight     ZXING图像高度
 输出参数  :
 返 回 值  :  0成功，其他失败
*****************************************************************************/
static int anj_zxing_init()
{
    memset(&mZXingInfo, 0, sizeof(mZXingInfo));
    memset(&mZXingData, 0, sizeof(mZXingData));

    mZXingInfo.mImageInfo.pData = (unsigned char *)anj_mw_malloc(DEFAULT_SMART_WIDTH * DEFAULT_SMART_HEIGHT);
    if (NULL == mZXingInfo.mImageInfo.pData)
    {
        __FATAL("malloc buf fail\n");
        return -1;
    }
    mZXingInfo.mImageInfo.width = DEFAULT_SMART_WIDTH;
    mZXingInfo.mImageInfo.height = DEFAULT_SMART_HEIGHT;

    s_stZXingThread.bAutoDestroy = 0;
    strncpy(s_stZXingThread.iThreadName, "zxing_thread", sizeof(s_stZXingThread.iThreadName) - 1);
    s_stZXingThread.iThreadjob.ctx = (void *)&s_stZXingThread;
    s_stZXingThread.iThreadjob.func = anj_zxing_thread;
    anj_thread_task_create(&s_stZXingThread);
    __INFO("Creat ZXing thread Ok, WxH:%dx%d\n", mZXingInfo.mImageInfo.width, mZXingInfo.mImageInfo.height);

    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_ZXING_SET_IMAGE, anj_zxing_set_image);
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_ZXING_SET_STATUS, anj_zxing_status_set);

    return 0;
}

/*****************************************************************************
 函 数 名  : anj_zxing_uninit
 功能描述  : 二维码识别绑定退出
 输入参数  :   无
 输出参数  :
 返 回 值  :  0成功，其他失败
*****************************************************************************/
static int anj_zxing_uninit()
{
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_ZXING_SET_IMAGE, anj_zxing_set_image);
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_ZXING_SET_STATUS, anj_zxing_status_set);

    anj_thread_task_destroy(&s_stZXingThread, -1);
    if (mZXingInfo.mImageInfo.pData)
    {
        anj_mw_free(mZXingInfo.mImageInfo.pData);
        mZXingInfo.mImageInfo.pData = NULL;
    }
    return 0;
}

REGISTER_MODULE(anj_zxing, MODULE_PRIORITY_ZXING);
