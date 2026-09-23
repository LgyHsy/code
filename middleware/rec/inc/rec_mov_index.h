#ifndef _REC_MOV_INDEX_H_
#define _REC_MOV_INDEX_H_

#include <stdio.h>
#include <stdint.h>
#include "rec_mov_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REC_MAX_RECORD_NO (8) // 存储最大录像个数
#define REC_DATA_FILE_INDEX_HEADER_PTR(pstData) ((rec_file_index_header *)(pstData))
#define REC_DATA_FILE_INDEX_RECORD_PTR(pstData, fileNo) ((rec_file_index_record *)(pstData + sizeof(rec_file_index_header) + sizeof(rec_file_index_record) * fileNo))

#define REC_FILE_INDEX_HEADER_SIZE (1024)  // sizeof(rec_file_index_header)
#define REC_FILE_INDEX_RECORD_SIZE (32)    // sizeof(rec_file_index_record)
#define REC_MEDIA_INDEX_SEGMENT_SIZE (64)  // sizeof(rec_media_segment_index)
#define REC_MEDIA_INDEX_MAX_SEGMENT (1024) // 一个视频文件最大片段数

#define REC_FILE_CRC_SIZE(size) (size - 4) // 计算CRC长度大小

// 索引状态
typedef enum
{
    REC_STATUS_NULL = 0,  // 状态无数据
    REC_STATUS_WRITE = 1, // 状态当前写入数据
    REC_STATUS_FINISH  = 2,      //状态当前写入数据
    REC_STATUS_FULL   = 3,      //当前写满数据
}rec_file_status_e;

#pragma pack(1)
/* ts 录像文件 索引头 ,1024*/
typedef struct
{
    unsigned int iStartCode;                            // 索引文件起始码 REC_FILE_STARTCODE
    unsigned int iVersion;                              // 当前版本号
    unsigned int iCreateTime;                           // 最新修改时间
    unsigned int iModifyTime;                           // 最新修改时间
    unsigned int iModifyTimes;                          // 修改次数
    unsigned int iRecordTimes;                          // 录制文件次数
    unsigned int iMediaMaxFiles;                        // 录像最大文件次数
    unsigned int iNextRecMediaFileNo;                   // 下次录像文件序号
    unsigned int iFileRecNo[REC_MAX_RECORD_NO];         // 当前写入文件 最大支持8个camera
    unsigned int iMaxPartition;                         // 最大分区数
    unsigned int iPartitionMaxFiles[REC_MAX_RECORD_NO]; // 当前分区最大序号
    unsigned int  bFull;
    unsigned char bad_block_map[REC_MEDIA_BAD_BLOCK_MAP_SIZE]; // 坏块标志位
    unsigned char iRes[404];                            // 保留字节
    unsigned int iCrc32;                                // 前面数据CRC32值
} rec_file_index_header;

/* ts 录像文件 每个media文件索引数据 32字节*/
typedef struct
{
    unsigned char iMediaFileCh;          // 媒体通道
    unsigned char iMediaFileStatus;      // 文件状态 rec_file_status_e
    unsigned short iMediaFileSegRecNums; // 当前录像片段数
    unsigned int tMediaFileEvent;        // 事件 rec_event_mask_e
    unsigned int tMediaFileBeginTime;    // 录像最早时间
    unsigned int tMediaFileEndTime;      // 录像最晚时间
    unsigned char iRes[12];              // 保留字段
    unsigned int iCrc32;                 // 前面数据CRC32值
} rec_file_index_record;

// 64字节
typedef struct
{
    unsigned char iMediaFileCh;       // 媒体通道
    unsigned char iMediaFileStatus;   // 文件状态 rec_file_status_e
    unsigned char iRes1[2];           // 保留字节
    unsigned int tMediaFileEvent;     // 事件 rec_event_mask_e;
    uint64_t tMediaFileBeginPts;      // 录像最早pts
    unsigned int tMediaFileBeginTime; // 录像最早时间
    unsigned int tMediaFileEndTime;   // 录像最晚时间
    unsigned int iVFrameStartNo;      // 当前视频片段起始帧位置
    unsigned int iVFrameNum;          // 当前视频片段帧数量

    unsigned int iVKeyFrameStartNo; // 当前视频片段起始I帧位置
    unsigned int iVKeyFrameNum;     // 当前视频片段I帧数量

    unsigned int iAFrameStartNo; // 当前音频片段起始音频帧位置
    unsigned int iAFrameNum;     // 当前音频片段音频帧数量

    unsigned char iRes[12]; // 保留字段
    unsigned int iCrc32;    // 校验和
} rec_media_segment_index;

#pragma pack()

typedef struct
{
    unsigned char *ptsDataBuf;
    unsigned int ptsDataBufLen;
    unsigned int iMediaMaxFiles;
} rec_file_index_param, *p_rec_file_index_param;

typedef struct
{
    unsigned int iFileOffset;
    unsigned char *ptsDataBuf;
    unsigned int ptsDataBufLen;
} rec_file_write_param, *p_rec_file_write_param;

rec_file_index_header *rec_mov_index_file_get_header(p_rec_file_index_param pstIndexParam);
void rec_mov_index_file_header_show(rec_file_index_header *pstIndexHeader);
void rec_mov_index_file_record_show(rec_file_index_record *pstIndexRecord, int iFileNo);
void rec_mov_index_file_segment_show(rec_media_segment_index *pstIndexSegment);
rec_file_index_record *rec_mov_index_file_get_record(p_rec_file_index_param pstIndexParam, int iFileNo);
int rec_mov_index_is_bad_file(rec_file_index_header *pstIndexHeader, unsigned int iFileNo);
int rec_mov_index_file_write(p_rec_file_index_param pstIndexParam, const char *pstFileName, int iRecChannel, int err);

int rec_mov_index_file_read_header(rec_file_index_header *pstIndexHeader, const char *pstFileName);

int rec_mov_index_file_read(p_rec_file_index_param pstIndexParam, const char *pstFileName);

int rec_mov_index_file_load(p_rec_file_index_param pstIndexParam, char *filePath);

int rec_mov_index_file_load_init(p_rec_file_index_param pstIndexParam);

int rec_mov_index_file_free(p_rec_file_index_param pstIndexParam);

#ifdef __cplusplus
}
#endif

#endif /* __Z_MOV_UTILITY_H__ */
