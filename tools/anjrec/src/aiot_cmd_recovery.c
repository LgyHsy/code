#include <string.h>

#include "zlib.h"
#include "cJSON.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"
#include "anj_sysmng.h"
#include "anj_aiot_cmd.h"
#include "protocol_queue.h"
#include "project_option.h"

#include "gct_apiv4.h"
#include "gct_common.h"

static FRAME_BUFFER_MANAGER s_stAiotCmdMgr;
static anj_thread_s s_stAiotCmdThread;

static char *anjrec_aiot_dev_property_json(void)
{
    DevInfo *pstDevInfo = getDevInfo();
    char szRealVersion[64] = {0};
    cJSON *pRoot = cJSON_CreateObject();

    if (pRoot == NULL || pstDevInfo == NULL)
    {
        return NULL;
    }

    snprintf(szRealVersion, sizeof(szRealVersion), "%s_V%s",
             pstDevInfo->devType, pstDevInfo->productVersion);

    const char *model = pstDevInfo->search_devicetype;
    if (model[0] == '\0')
    {
        model = pstDevInfo->subDevType;
    }

    cJSON_AddStringToObject(pRoot, "SN", pstDevInfo->sn);
    cJSON_AddStringToObject(pRoot, "DeviceType", "IPCamera");
    cJSON_AddNumberToObject(pRoot, "ChannelNumber", ANJ_CAMERA_MAX_NUMS);
    cJSON_AddStringToObject(pRoot, "Model", model);
    cJSON_AddStringToObject(pRoot, "RealModel", szRealVersion);
    cJSON_AddStringToObject(pRoot, "IpcVersion", pstDevInfo->version_name);
    cJSON_AddStringToObject(pRoot, "release_date", pstDevInfo->release_date);

    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anjrec_aiot_cmd_json(const char *pDecBuffer)
{
    char *szRspMsg = NULL;
    int bGetProperty = 0;
    cJSON *pNode = cJSON_Parse(pDecBuffer);

    if (pNode == NULL)
    {
        return NULL;
    }

    for (cJSON *pChild = pNode->child; pChild != NULL; pChild = pChild->next)
    {
        if (strcasecmp("GetProperty", pChild->string) == 0)
        {
            bGetProperty = 1;
            break;
        }
    }
    cJSON_Delete(pNode);

    if (bGetProperty)
    {
        szRspMsg = anjrec_aiot_dev_property_json();
    }

    return szRspMsg;
}

static int anjrec_aiot_cmd_decompress(const char *pSrcBuffer, unsigned int nSrcBuflen, char **pDstBuffer)
{
    const AiotTransHeader *pHeader = (const AiotTransHeader *)pSrcBuffer;

    if (pSrcBuffer == NULL || nSrcBuflen == 0 || pDstBuffer == NULL ||
        FLAG_MAGIC_AIOT_TRANSDATA != pHeader->magic)
    {
        return -1;
    }

    if (pHeader->nCompressedLength + sizeof(AiotTransHeader) > nSrcBuflen)
    {
        return -1;
    }

    unsigned int nDecodeBufLen = pHeader->nDecompressedLength;
    char *pDecodeBuf = (char *)anj_mw_malloc(nDecodeBufLen + 1);
    if (pDecodeBuf == NULL)
    {
        return -1;
    }

    int iRet = -1;
    if (pHeader->nCompressType == COMPRESS_NONE)
    {
        if (pHeader->nCompressedLength != pHeader->nDecompressedLength)
        {
            anj_mw_free(pDecodeBuf);
            return -1;
        }
        memcpy(pDecodeBuf, pSrcBuffer + sizeof(AiotTransHeader), pHeader->nCompressedLength);
        iRet = (int)pHeader->nCompressedLength;
    }
    else if (pHeader->nCompressType == COMPRESS_ZLIB)
    {
        uLongf nOutLen = nDecodeBufLen;
        if (uncompress((Bytef *)pDecodeBuf, &nOutLen,
                       (const Bytef *)(pSrcBuffer + sizeof(AiotTransHeader)),
                       pHeader->nCompressedLength) == Z_OK)
        {
            iRet = (int)nOutLen;
        }
    }

    if (iRet <= 0)
    {
        anj_mw_free(pDecodeBuf);
        return -1;
    }

    pDecodeBuf[iRet] = '\0';
    *pDstBuffer = pDecodeBuf;
    return iRet;
}

static int anjrec_aiot_cmd_compress(const char *pSrcBuffer, unsigned int nSrcBuflen,
                                    char **pDstBuffer, AiotTransDataType nDataType)
{
    unsigned int nDstBuflen = nSrcBuflen + (unsigned int)sizeof(AiotTransHeader);
    if (nDstBuflen < 1024)
    {
        nDstBuflen = 1024;
    }

    char *pTransData = (char *)anj_mw_malloc(nDstBuflen);
    if (pTransData == NULL)
    {
        return -1;
    }

    char *pCompData = pTransData + sizeof(AiotTransHeader);
    AiotTransHeader *pHeader = (AiotTransHeader *)pTransData;

    pHeader->magic = FLAG_MAGIC_AIOT_TRANSDATA;
    pHeader->nCompressType = COMPRESS_ZLIB;
    pHeader->nDecompressedLength = nSrcBuflen;
    pHeader->nDataType = nDataType;

    uLongf nCompOutLen = nDstBuflen - (unsigned int)sizeof(AiotTransHeader);
    if (compress((Bytef *)pCompData, &nCompOutLen, (const Bytef *)pSrcBuffer, nSrcBuflen) != Z_OK)
    {
        memcpy(pCompData, pSrcBuffer, nSrcBuflen);
        pHeader->nCompressType = COMPRESS_NONE;
        pHeader->nCompressedLength = nSrcBuflen;
        *pDstBuffer = pTransData;
        return (int)(nSrcBuflen + sizeof(AiotTransHeader));
    }

    pHeader->nCompressedLength = (unsigned int)nCompOutLen;
    *pDstBuffer = pTransData;
    return (int)(nCompOutLen + sizeof(AiotTransHeader));
}

static int anjrec_aiot_cmd_proc(char *pBuf, int nBufLen, unsigned int sid)
{
    char *pDecBuffer = NULL;
    char *szRspMsg = NULL;
    char *pSendBuffer = NULL;
    int iRet = -1;

    if (pBuf == NULL || nBufLen <= 0)
    {
        return -1;
    }

    int nDecompressLen = anjrec_aiot_cmd_decompress(pBuf, (unsigned int)nBufLen, &pDecBuffer);
    if (nDecompressLen <= 0 || pDecBuffer == NULL)
    {
        goto endFunc;
    }

    const AiotTransHeader *pHeader = (const AiotTransHeader *)pBuf;
    if (pHeader->nDataType == TRANS_DATA_ACP_JSON)
    {
        __INFO("recovery aiot cmd recv:\n%s\n", pDecBuffer);
        szRspMsg = anjrec_aiot_cmd_json(pDecBuffer);
    }

    if (szRspMsg == NULL || strlen(szRspMsg) == 0)
    {
        goto endFunc;
    }

    __INFO("recovery aiot cmd send:\n%s\n", szRspMsg);

    int nCompressLen = anjrec_aiot_cmd_compress(szRspMsg, (unsigned int)strlen(szRspMsg),
                                              &pSendBuffer, pHeader->nDataType);
    if (nCompressLen > 0 && pSendBuffer != NULL)
    {
        gct_apiv4_trans_channel_write(sid, pSendBuffer, (GCT_UINT32)nCompressLen);
        iRet = 0;
    }

endFunc:
    if (szRspMsg)
    {
        anj_mw_free(szRspMsg);
    }
    if (pDecBuffer)
    {
        anj_mw_free(pDecBuffer);
    }
    if (pSendBuffer)
    {
        anj_mw_free(pSendBuffer);
    }
    return iRet;
}

static int anjrec_aiot_cmd_thread(void *ctx, int *bStart)
{
    (void)ctx;
    while (bStart && *bStart)
    {
        FRAME_ENTRY frame = {0};
        if (frame_mgr_pop(&s_stAiotCmdMgr, &frame) > 0)
        {
            anjrec_aiot_cmd_proc(frame.pFrame, frame.nFrameLen, (unsigned int)frame.session);
            if (frame.pFrame)
            {
                anj_mw_free(frame.pFrame);
            }
        }
        usleep(10 * 1000);
    }
    return 0;
}

int anjrec_aiot_cmd_init(void)
{
    frame_mgr_init(&s_stAiotCmdMgr, 20);
    s_stAiotCmdThread.bAutoDestroy = 1;
    strncpy(s_stAiotCmdThread.iThreadName, "aiot_cmd_rec", sizeof(s_stAiotCmdThread.iThreadName) - 1);
    s_stAiotCmdThread.iThreadjob.ctx = &s_stAiotCmdThread;
    s_stAiotCmdThread.iThreadjob.func = anjrec_aiot_cmd_thread;
    return anj_thread_task_create(&s_stAiotCmdThread);
}

void anjrec_aiot_cmd_uninit(void)
{
    anj_thread_task_destroy(&s_stAiotCmdThread, -1);
    frame_mgr_release(&s_stAiotCmdMgr);
}

void anjrec_aiot_cmd_push(char *pBuf, unsigned int nBufLen, unsigned int sid)
{
    FRAME_ENTRY frame = {0};

    if (pBuf == NULL || nBufLen == 0)
    {
        return;
    }

    frame.pFrame = anj_mw_malloc(nBufLen + 1);
    if (frame.pFrame == NULL)
    {
        return;
    }

    memcpy(frame.pFrame, pBuf, nBufLen);
    frame.pFrame[nBufLen] = '\0';
    frame.nFrameLen = nBufLen;
    frame.session = sid;
    frame_mgr_push(&s_stAiotCmdMgr, &frame);
}
