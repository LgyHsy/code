#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "rec_mov_index.h"
#include "rec_mov_def.h"

void rec_mov_index_file_header_show(rec_file_index_header *pstIndexHeader)
{
    __INFO("SC:%x,V:%u,CTime:%u,MTime:%u, MTimes:%u, RTimes:%u, maxF:%u, curF(%u,%u,%u) nextF:%u, maxP:(%u-%u,%u,%u,%u), crc:%u\n", 
                        pstIndexHeader->iStartCode, pstIndexHeader->iVersion, pstIndexHeader->iCreateTime, pstIndexHeader->iModifyTime,
                        pstIndexHeader->iModifyTimes, pstIndexHeader->iRecordTimes, pstIndexHeader->iMediaMaxFiles,
                        pstIndexHeader->iFileRecNo[0], pstIndexHeader->iFileRecNo[1], pstIndexHeader->iFileRecNo[2],
                        pstIndexHeader->iNextRecMediaFileNo, pstIndexHeader->iMaxPartition, pstIndexHeader->iPartitionMaxFiles[0],
                        pstIndexHeader->iPartitionMaxFiles[1], pstIndexHeader->iPartitionMaxFiles[2], pstIndexHeader->iPartitionMaxFiles[3],
                        pstIndexHeader->iCrc32);
}

void rec_mov_index_file_record_show(rec_file_index_record *pstIndexRecord, int iFileNo)
{
    __INFO("rec:%d,ch:%x,sta:%x,nums:%u, Times:%u~%u,evt:(%x), crc:%u\n", 
                        iFileNo, pstIndexRecord->iMediaFileCh, pstIndexRecord->iMediaFileStatus, pstIndexRecord->iMediaFileSegRecNums,
                        pstIndexRecord->tMediaFileBeginTime, pstIndexRecord->tMediaFileEndTime, pstIndexRecord->tMediaFileEvent,
                        pstIndexRecord->iCrc32);
}

void rec_mov_index_file_segment_show(rec_media_segment_index *pstIndexSegment)
{
    __INFO("ch:%d,sta:%u,et:%u,T:%u,%u, Vno:%u,%u, Vkno:%u,%u Ano:%u,%u pts:%llu crc:%x\n", 
                        pstIndexSegment->iMediaFileCh, pstIndexSegment->iMediaFileStatus, pstIndexSegment->tMediaFileEvent,
                        pstIndexSegment->tMediaFileBeginTime, pstIndexSegment->tMediaFileEndTime,
                        pstIndexSegment->iVFrameStartNo, pstIndexSegment->iVFrameNum,
                        pstIndexSegment->iVKeyFrameStartNo, pstIndexSegment->iVKeyFrameNum,
                        pstIndexSegment->iAFrameStartNo, pstIndexSegment->iAFrameNum,
                        pstIndexSegment->tMediaFileBeginPts, pstIndexSegment->iCrc32);
}

rec_file_index_header * rec_mov_index_file_get_header(p_rec_file_index_param pstIndexParam)
{
    return REC_DATA_FILE_INDEX_HEADER_PTR(pstIndexParam->ptsDataBuf);
}

rec_file_index_record * rec_mov_index_file_get_record(p_rec_file_index_param pstIndexParam, int iFileNo)
{
    rec_file_index_record * pstIndexRecord = REC_DATA_FILE_INDEX_RECORD_PTR(pstIndexParam->ptsDataBuf, iFileNo);

    if (pstIndexRecord->iMediaFileSegRecNums > REC_MEDIA_INDEX_MAX_SEGMENT)
    {
        __ERR("Invalid segnum %d, no:%d\n", pstIndexRecord->iMediaFileSegRecNums, iFileNo);
        pstIndexRecord->iMediaFileSegRecNums = REC_MEDIA_INDEX_MAX_SEGMENT;
    }

    return pstIndexRecord;
}

int rec_mov_index_is_bad_file(rec_file_index_header *pstIndexHeader, unsigned int iFileNo)
{
    unsigned int byte_index = 0;
    unsigned int bit_index = 0;

    if (pstIndexHeader == NULL)
    {
        return 0;
    }

    byte_index = iFileNo / 8;
    bit_index = iFileNo % 8;
    return (pstIndexHeader->bad_block_map[byte_index] >> bit_index) & 1;
}

int rec_mov_index_file_write(p_rec_file_index_param pstIndexParam, const char *pstFileName, int iRecChannel, int err)
{
    int iRet = -1;
    FILE *fp = NULL;
    if ((NULL == pstIndexParam) || (NULL == pstIndexParam->ptsDataBuf) || (NULL == pstFileName))
    {
        __ERR("input param invalid\n");
        goto endFunc;
    }

    fp = fopen(pstFileName, "rb+");
    if (fp)
    {
        __INFO("Write index(%s),files:%d,len:%d\n", pstFileName, pstIndexParam->iMediaMaxFiles, pstIndexParam->ptsDataBufLen);
        rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(pstIndexParam);
        
        pstIndexHeader->iModifyTime = time(NULL);
        pstIndexHeader->iModifyTimes++;

        if (pstIndexHeader->iNextRecMediaFileNo >= (pstIndexHeader->iMediaMaxFiles - 1) || access("/tmp/sdfull", F_OK) == 0)
        {
            remove("/tmp/sdfull");
            if (pstIndexHeader->bFull == 0)
            {
                pstIndexHeader->bFull = 1;
            }
        }
        
        // 标记坏块
        if (err)
        {
            unsigned int byte_index = pstIndexHeader->iFileRecNo[iRecChannel] / 8;
            unsigned int bit_index = pstIndexHeader->iFileRecNo[iRecChannel] % 8;
            pstIndexHeader->bad_block_map[byte_index] |= (1 << bit_index);
            __ERR("bad block mp4:%d\n", pstIndexHeader->iFileRecNo[iRecChannel]);
        }
        pstIndexHeader->iCrc32 = anj_crc32_update(0, (unsigned char *)pstIndexHeader, REC_FILE_CRC_SIZE(REC_FILE_INDEX_HEADER_SIZE));
        for (unsigned int i = 0; i < pstIndexParam->iMediaMaxFiles; i++)
        {
            rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(pstIndexParam, i);
            pstIndexRecord->iCrc32 = anj_crc32_update(0, (unsigned char *)pstIndexRecord, REC_FILE_CRC_SIZE(REC_FILE_INDEX_RECORD_SIZE));
        }
        iRet = fwrite(pstIndexParam->ptsDataBuf, 1, pstIndexParam->ptsDataBufLen, fp);
        if (iRet != (int)pstIndexParam->ptsDataBufLen)
        {
            __INFO("Write index(%s),files:%d,len:%d, Ret:%d\n", pstFileName, pstIndexParam->iMediaMaxFiles,
                          pstIndexParam->ptsDataBufLen, iRet);
            iRet = -1;
        }
        else
        {
            iRet = 0;
        }
    }
    else
    {
        __INFO("Write index(%s),files:%d,len:%d, Ret:%d\n", pstFileName, pstIndexParam->iMediaMaxFiles,
                          pstIndexParam->ptsDataBufLen, iRet);
    }
endFunc:
    if (fp)
    {
        fclose(fp);
    }
    return iRet;
}

int rec_mov_index_file_read_header(rec_file_index_header *pstIndexHeader, const char *pstFileName)
{
    int iRet = -1;
    FILE *fp = NULL;
    if ((NULL == pstIndexHeader) || (NULL == pstFileName))
    {
        __ERR("input param invalid\n");
        goto endFunc;
    }

    fp = fopen(pstFileName, "rb+");
    if (fp)
    {
        unsigned int iCrc32 = 0;
        iRet = anj_mw_fread(fp, pstIndexHeader, REC_FILE_INDEX_HEADER_SIZE);
        if (iRet != REC_FILE_INDEX_HEADER_SIZE)
        {
            iRet = -1;
            __ERR("read index(%s), len:%d, Ret:%d\n", pstFileName, REC_FILE_INDEX_HEADER_SIZE, iRet);
            goto endFunc;
        }
        rec_mov_index_file_header_show(pstIndexHeader);

        iCrc32 = anj_crc32_update(0, (unsigned char *)pstIndexHeader, REC_FILE_CRC_SIZE(REC_FILE_INDEX_HEADER_SIZE));
        if ((REC_FILE_STARTCODE == pstIndexHeader->iStartCode) && (iCrc32 == pstIndexHeader->iCrc32) && (pstIndexHeader->iMediaMaxFiles <= REC_MEDIA_MAX_FILE_NUM))
        {
            iRet = 0;
        }
        else
        {
            iRet = -1;
            __ERR("read index(%s),crc:%d != %d,files:%d\n", pstFileName, pstIndexHeader->iCrc32, iCrc32, pstIndexHeader->iMediaMaxFiles);
        }
    }
endFunc:
    if (fp)
    {
        fclose(fp);
    }
    return iRet;
}

int rec_mov_index_file_read(p_rec_file_index_param pstIndexParam, const char *pstFileName)
{
    int iRet = -1;
    FILE *fp = NULL;
    if ((NULL == pstIndexParam) || (NULL == pstFileName) || (NULL == pstIndexParam->ptsDataBuf) ||
        (pstIndexParam->ptsDataBufLen > REC_INDEX_MAIN_FILE_SIZE))
    {
        __ERR("input param invalid\n");
        goto endFunc;
    }

    fp = fopen(pstFileName, "rb+");
    if (fp)
    {
        unsigned int iMediaMaxFiles = pstIndexParam->iMediaMaxFiles;
        unsigned int iCrc32 = 0;
        rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(pstIndexParam);
        iRet = anj_mw_fread(fp, pstIndexParam->ptsDataBuf, pstIndexParam->ptsDataBufLen);
        if (iRet != (int)pstIndexParam->ptsDataBufLen)
        {
            __ERR("read index(%s), len:%d, Ret:%d\n", pstFileName, pstIndexParam->ptsDataBufLen, iRet);
            iRet = -1;
            goto endFunc;
        }
        rec_mov_index_file_header_show(pstIndexHeader);

        iCrc32 = anj_crc32_update(0, (unsigned char *)pstIndexHeader, REC_FILE_CRC_SIZE(REC_FILE_INDEX_HEADER_SIZE));
        if ((REC_FILE_STARTCODE == pstIndexHeader->iStartCode) && (iCrc32 == pstIndexHeader->iCrc32) && (iMediaMaxFiles == pstIndexParam->iMediaMaxFiles))
        {
            int iErrCount = 0;
            for (unsigned int i = 0; i < pstIndexParam->iMediaMaxFiles; i++)
            {
                rec_file_index_record *pstIndexRecord = rec_mov_index_file_get_record(pstIndexParam, i);
                iCrc32 = anj_crc32_update(0, (unsigned char *)pstIndexRecord, REC_FILE_CRC_SIZE(REC_FILE_INDEX_RECORD_SIZE));
                if (pstIndexRecord->iCrc32 != iCrc32)
                {
                    memset(pstIndexRecord, 0, REC_FILE_INDEX_RECORD_SIZE);

                    iErrCount++;
                    __ERR("read index(%s),crc:%d != %d, files:%d,%d,err:%d\n", pstFileName, pstIndexRecord->iCrc32,
                                 iCrc32, i, pstIndexParam->iMediaMaxFiles, iErrCount);
                    if (iErrCount >= 3)
                    {
                        __ERR("read index(%s),crc:%d != %d, files:%d,%d,err:%d break\n", pstFileName, pstIndexRecord->iCrc32,
                                     iCrc32, i, pstIndexParam->iMediaMaxFiles, iErrCount);
                        iRet = -1;
                        goto endFunc;
                    }
                }
            }
            iRet = 0;
        }
        else
        {
            __ERR("read index(%s),crc:%d.%d, files:%d,%d\n", pstFileName, pstIndexHeader->iCrc32,
                         iCrc32, iMediaMaxFiles, pstIndexParam->iMediaMaxFiles);
            iRet = -1;
        }
    }
endFunc:
    if (fp)
    {
        fclose(fp);
    }
    return iRet;
}

int rec_mov_index_file_load(p_rec_file_index_param pstIndexParam, char *filePath)
{
    int iRet = -1;
    int bBakFile = 0;
    int pstIndexDataLen = 0;
    char fileIndexName[128] = {0};
    char fileIndexBakName[128] = {0};
    rec_file_index_header stIndexHeader;

    if ((NULL == pstIndexParam) || (pstIndexParam->ptsDataBuf != NULL))
    {
        __ERR("input param invalid:%p %p\n", pstIndexParam, pstIndexParam->ptsDataBuf);
        goto endFunc;
    }
    if (REC_FILE_INDEX_HEADER_SIZE != sizeof(rec_file_index_header))
    {
        __ERR("Invalid rec_file_index_header size");
        goto endFunc;
    }
    if (REC_FILE_INDEX_RECORD_SIZE != sizeof(rec_file_index_record))
    {
        __ERR("Invalid rec_file_index_record size");
        goto endFunc;
    }
    if (REC_MEDIA_INDEX_SEGMENT_SIZE != sizeof(rec_media_segment_index))
    {
        __ERR("Invalid rec_media_segment_index size");
        goto endFunc;
    }

    memset(&stIndexHeader, 0, sizeof(stIndexHeader));
    snprintf(fileIndexName, sizeof(fileIndexName), "%s" REC_INDEX_MAIN_FILE_NAME, filePath);
    snprintf(fileIndexBakName, sizeof(fileIndexBakName), "%s" REC_INDEX_MAIN_FILE_NAME_BAK, filePath);
    iRet = rec_mov_index_file_read_header(&stIndexHeader, fileIndexName);
    if (0 != iRet)
    {
        bBakFile = 1;
        iRet = rec_mov_index_file_read_header(&stIndexHeader, fileIndexBakName);
        if (0 != iRet)
        {
            __ERR("Read header err %d\n", iRet);
            goto endFunc;
        }
        else
        {
            __WARN("load Bak files:%d, write[%d,%d]\n", stIndexHeader.iMediaMaxFiles, stIndexHeader.iFileRecNo[0], stIndexHeader.iFileRecNo[1]);
        }
    }
    else
    {
        __INFO("load files:%d, write[%d,%d]\n", stIndexHeader.iMediaMaxFiles, stIndexHeader.iFileRecNo[0], stIndexHeader.iFileRecNo[1]);
    }
    pstIndexDataLen = REC_FILE_INDEX_HEADER_SIZE + stIndexHeader.iMediaMaxFiles * REC_FILE_INDEX_RECORD_SIZE;
    pstIndexParam->ptsDataBuf = (unsigned char *)malloc(pstIndexDataLen);
    if (pstIndexParam->ptsDataBuf)
    {
        memset(pstIndexParam->ptsDataBuf, 0, pstIndexDataLen);
        pstIndexParam->ptsDataBufLen = pstIndexDataLen;
        pstIndexParam->iMediaMaxFiles = stIndexHeader.iMediaMaxFiles;

        if (0 == bBakFile)
        {
            iRet = rec_mov_index_file_read(pstIndexParam, fileIndexName);
        }
        else
        {
            iRet = -1;
        }

        if ((0 != iRet) || (1 == bBakFile))
        {
            iRet = rec_mov_index_file_read(pstIndexParam, fileIndexBakName);
            if (0 != iRet)
            {
                __ERR("load bak files:%d err:%d\n", stIndexHeader.iMediaMaxFiles, iRet);
            }
        }
    }
    else
    {
        iRet = -1;
        __ERR("malloc failed\n");
    }

endFunc:
    if (0 != iRet)
    {
        if (pstIndexParam->ptsDataBuf)
        {
            rec_mov_index_file_free(pstIndexParam);
        }
    }
    return iRet;
}

int rec_mov_index_file_load_init(p_rec_file_index_param pstIndexParam)
{
    int iRet = -1;
    int pstIndexDataLen = 0;
    if ((NULL == pstIndexParam) || (pstIndexParam->ptsDataBuf != NULL) || (pstIndexParam->iMediaMaxFiles <= 0))
    {
        __ERR("input param invalid\n");
        goto endFunc;
    }

    pstIndexDataLen = REC_FILE_INDEX_HEADER_SIZE + pstIndexParam->iMediaMaxFiles * REC_FILE_INDEX_RECORD_SIZE;

    __INFO("Load init files(%d),size:(%d)\n", pstIndexParam->iMediaMaxFiles, pstIndexDataLen);
    pstIndexParam->ptsDataBuf = (unsigned char *)malloc(pstIndexDataLen);
    if (pstIndexParam->ptsDataBuf)
    {
        rec_file_index_header *pstIndexHeader = rec_mov_index_file_get_header(pstIndexParam);
        pstIndexParam->ptsDataBufLen = pstIndexDataLen;
        memset(pstIndexParam->ptsDataBuf, 0, pstIndexDataLen);
        pstIndexHeader->iStartCode = REC_FILE_STARTCODE;
        pstIndexHeader->iVersion = REC_INDEX_VERSION_128M;
        pstIndexHeader->iCreateTime = time(NULL);
        pstIndexHeader->iModifyTime = pstIndexHeader->iCreateTime;
        pstIndexHeader->iMediaMaxFiles = pstIndexParam->iMediaMaxFiles;
        iRet = 0;
    }
    else
    {
        iRet = -1;
        __ERR("malloc failed\n");
    }
endFunc:
    return iRet;
}

int rec_mov_index_file_free(p_rec_file_index_param pstIndexParam)
{
    if (pstIndexParam && pstIndexParam->ptsDataBuf)
    {
        free(pstIndexParam->ptsDataBuf);
        pstIndexParam->ptsDataBuf = NULL;
        pstIndexParam->ptsDataBufLen = 0;
    }
    else
    {
        __ERR("input param invalid\n");
    }
    return 0;
}

