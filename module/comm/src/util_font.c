#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <time.h>

#include "anj_comm.h"
#include "anj_mw_comm.h"
#include "anj_config.h"

#include "util_font.h"

#define USER_FONT_PATH "/mnt/nand"

const char g_FontList[][64] =
{
    "unicode_16x16-ch.font",
    "unicode_16x16-en.font",
    "unicode_16x16_jp.font",
    "unicode_16x16-ko.font",
    "unicode_16x16-ru.font",
    "unicode_16x16.font",
};

#define FONT_LIST_NAME_MAX ((int)sizeof(g_FontList[0]))

typedef struct
{
    char str_utf8[256];
    char str_gb2312[256];
    int gb2312_len;
} UTF8_GB2312_MAP_ITEM;


#define UTF8_GB2312_MAP_CNT  10
UTF8_GB2312_MAP_ITEM utf8_gb2312_map[UTF8_GB2312_MAP_CNT];

int find_gb2312_code_bymap(char *pUtf8, char *outGb2312)
{
    int i = 0;
    for (i = 0; i < UTF8_GB2312_MAP_CNT; i++)
    {
        if (utf8_gb2312_map[i].gb2312_len > 0 && strcmp(utf8_gb2312_map[i].str_utf8, pUtf8) == 0)
        {
            memcpy(outGb2312, utf8_gb2312_map[i].str_gb2312, utf8_gb2312_map[i].gb2312_len);
            outGb2312[utf8_gb2312_map[i].gb2312_len] = 0;
            __ERR("find gb2312 in map\n");
            return utf8_gb2312_map[i].gb2312_len;
        }
    }

    __ERR("not find gb2312 in map..\n");
    return 0;
}

int  utf8_2_gb2312_map_add(char *pUtf8, char *pGb2312, int len)
{
    int i = 0;

    if (strlen(pUtf8) > 255 || len > 255)
    {
        __ERR("utf8_2_gb2312_map_add fail, utf8len=%d, gb2312 len=%d\n", strlen(pUtf8),  len);
    }
    
    for (i = 0; i < UTF8_GB2312_MAP_CNT; i++)
    {
        if (utf8_gb2312_map[i].gb2312_len == 0)
        {
            utf8_gb2312_map[i].gb2312_len = len;

            snprintf(utf8_gb2312_map[i].str_utf8, sizeof(utf8_gb2312_map[i].str_utf8), "%s", pUtf8);
            snprintf(utf8_gb2312_map[i].str_gb2312, sizeof(utf8_gb2312_map[i].str_gb2312),"%s", pGb2312);

            __ERR("utf8_2_gb2312_map_add success(index = %d)\n", i);
            return 0;
        }
    }

    __ERR("utf8_2_gb2312_map_add fail(index = %d)\n", i);
    return -1;
}

typedef struct{
    pthread_mutex_t m_mutex;
} CFontMutex;

void CFontMutex_Init(CFontMutex *mutex)
{
    pthread_mutex_init(&mutex->m_mutex, NULL);
}

void CFontMutex_Destroy(CFontMutex *mutex)
{
    pthread_mutex_destroy(&mutex->m_mutex);
}

void CFontMutex_Lock(CFontMutex *mutex)
{
    pthread_mutex_lock(&mutex->m_mutex);
}

void CFontMutex_UnLock(CFontMutex *mutex)
{
    pthread_mutex_unlock(&mutex->m_mutex);
}


const char *week_day_chs[16]=//UTF8
{
    "\xE6\x98\x9F\xE6\x9C\x9F\xE6\x97\xA5",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xB8\x80",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xBA\x8C",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xB8\x89",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE5\x9B\x9B",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xBA\x94",
    "\xE6\x98\x9F\xE6\x9C\x9F\xE5\x85\xAD",
    "",
};

//繁体的星期与上下午文字是一样的

const char *week_day_eng[16]=
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "",
};

const char week_day_russion[8][16]=//UTF8
{
    "\xD0\x92\xD0\xA1",
    "\xD0\x9F\xD0\x9D",
    "\xD0\x92\xD0\xA2",
    "\xD0\xA1\xD0\xA0",
    "\xD0\xA7\xD0\xA2",
    "\xD0\x9F\xD0\xA2",
    "\xD0\xA1\xD0\x91",
    ""
};


const char *am_pm_chs[16]=
{
    "\xE4\xB8\x8A\xE5\x8D\x88",
    "\xE4\xB8\x8B\xE5\x8D\x88",
    "",
};

const char *am_pm_eng[16]=
{
    "AM",
    "PM",
    "",
};
    

static int g_show_weekday = -1;

const char * week_day(int index, int language)
{
    if(g_show_weekday < 0)
    {
        g_show_weekday = 1;
        FILE *fp = fopen("/mnt/nand/noweekday.flag","rb");
        if(fp)
        {
            fclose(fp);
            g_show_weekday = 0;
        }
    }
    
    if(g_show_weekday == 0)
        return week_day_chs[7];
    else
    {   
        if(CONFIG_LANGUAGE_CN == language
            || CONFIG_LANGUAGE_HK == language
            || CONFIG_LANGUAGE_TW == language)
        {
            return week_day_chs[index];
        }
        else if(CONFIG_LANGUAGE_RUSSION == language)
        {
            return week_day_russion[index];
        }
        else
        {
            return week_day_eng[index];
        }
    }
}


const char * am_pm_str(int index, int language)
{
    if(CONFIG_LANGUAGE_CN == language
        || CONFIG_LANGUAGE_HK == language
        || CONFIG_LANGUAGE_TW == language)
    {
        return am_pm_chs[index];
    }
    else
    {
        return am_pm_eng[index];
    }
}


int GetMaskStatus(const unsigned char *pMask, int x, int y) 
{
    int nBytesEachLine = (REGION_ALIGN_VALUE) >> 3;//点阵每行占几个字节

    int bsel = pMask[y*nBytesEachLine+x/8] & (1<<(7-(x%8)));
    return bsel;
}

int GetUtf8Bytes1(unsigned char chHighByte) 
{
    int bits = 1;
    if(( (chHighByte & 0xff) >> 4) == 15 ) //最高位连续4个1
        bits = 4;
    else if(( (chHighByte & 0xff) >> 5) == 7 ) //最高位连续3个1
        bits = 3;
    else if(( (chHighByte & 0xff) >> 6) == 3 )//最高位连续2个1
        bits = 2;

    return bits;
}

int GetUtf8Bytes2(unsigned int utf8) 
{
    int bits = 1;
    unsigned char utf8_bits[4];
    memcpy(utf8_bits, (unsigned char*)(&utf8), 4);
    unsigned char *pHighBit = NULL;
    int k = 0;
    for( k = 3; k >= 0; k--)
    {
        if( utf8_bits[k] != 0 )
        {
            pHighBit = &utf8_bits[k];
            break;
        }
    }

    if( pHighBit != NULL)
        return GetUtf8Bytes1(*pHighBit);

    return bits;
}

unsigned int GetUtf8ValueU32(const unsigned char *ptr)//根据UTF8第一个字符，获取该UTF8 U32值
{
    int bytes = GetUtf8Bytes1(*ptr);
    unsigned int nUtf8 = 0;
    int iIndex = 0;
    for( iIndex = 0; iIndex < bytes; iIndex++)
        nUtf8 = (nUtf8 << 8) | (ptr[iIndex] & 0xff);

    return nUtf8;
}

int GetUtf8ValueU8(unsigned int utf8, unsigned char* pOut)//根据UTF8 U32值，获取该UTF8的每个字节值
{
    int bytes = GetUtf8Bytes2(utf8);

    if( pOut != NULL)
    {
        int iIndex = 0;
        for( iIndex = 0; iIndex < bytes; iIndex++)
        {
            *(pOut+iIndex) = (utf8 >> ((bytes-1-iIndex )*8) ) & 0xff;
        }
    }

    return bytes;
}

unsigned int utf8_to_unicode(unsigned int utf8)
{
    unsigned int unicode = 0;

    unsigned char data[4];

    int bytes = GetUtf8ValueU8(utf8, data);

    if(bytes == 1)
    {
        unicode = utf8;
    }
    else if( bytes == 2)
    {
        if ( (data[1] & 0xC0) != 0x80 )
        {
//          __ERR("data[1]=%#x", data[1]);
            return 0;
        }
        unsigned char b0 = (data[0] << 6) + (data[1] & 0x3F);
        unsigned char b1 = (data[0] >> 2) & 0x07;
        unicode = (b1 << 8 ) | b0;
#if 0
        unsigned int check = unicode_to_utf8(unicode);
        __ERR("utf8=%#x, unicode=%#x, check=%#x", utf8, unicode, check);
#endif      
        
    }
    else if( bytes == 3)
    {
        unicode = ((data[0] & 0x0F) << 4) + ((data[1] >> 2) & 0x0F);  
        unicode <<= 8;
        unicode |= ((data[1] & 0x03) << 6) + (data[2] & 0x3F);    
    }

    return unicode;

}

unsigned int unicode_to_utf8(unsigned int unicode)
{ 
    unsigned int utf8 = 0;
    if(unicode < 0x80)
    {
        utf8 = unicode;
    }
    else if ( unicode >= 0x00000080 && unicode <= 0x000007FF )
    {
        // * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
        char dst[4];
        memset(dst, 0, 4);
        dst[0]     = ((unicode >> 6) & 0x1F) | 0xC0;
        dst[1]  = (unicode & 0x3F) | 0x80;

        utf8 = ((dst[0]&0xff) << 8) | ((dst[1]&0xff) << 0);
    }   
    else if ( unicode >= 0x00000800 && unicode <= 0x0000FFFF )
    {
        char src[4], dst[4];
        memset(src, 0, 4);
        memset(dst, 0, 4);
        src[0] = unicode & 0xff;    
        src[1] = (unicode >> 8) & 0xff;  

        
        dst[0] = (0xE0 | ((src[1] & 0xF0) >> 4));
        dst[1] = (0x80 | ((src[1] & 0x0F) << 2)) + ((src[0] & 0xC0) >> 6);
        dst[2] = (0x80 | (src[0] & 0x3F)); 

        utf8 = ((dst[0]&0xff) << 16) | ((dst[1]&0xff) << 8) | ((dst[2]&0xff) << 0);

#if 0       
        printf("src %x %x, dst %x %x %x, utf8=%#x\n", src[0]&0xff, src[1] &0xff, 
            dst[0] & 0xff, dst[1] & 0xff, dst[2] & 0xff, utf8);
#endif
    }
    

    return utf8;
}

void DrawFontMask(const CHAR_STRU *pCharData) 
{
    int x, y;

    //画对应的点阵
    int bsel;
    for(y=0; y<FONT_H; y++)
    {
        char line[32];
        memset(line, 0, sizeof(line));
        for(x=0; x<FONT_W; x++)
        {
            if( x >= pCharData->font_w || y >= pCharData->font_h)
                break;

            bsel = GetMaskStatus(pCharData->mask, x, y);
            line[x] = bsel > 0 ? '#' : ' ';
        }

        printf("%s\n", line);
    }
}

void DrawFontMaskInLineArray(const CHAR_STRU *pCharData, char szPrintLine[FONT_H][32]) 
{
    memset(szPrintLine, 0, FONT_H*32);

    int x, y;

    //画对应的点阵
    int bsel;
    for(y=0; y<FONT_H; y++)
    {
        for(x=0; x<FONT_W; x++)
        {
            if( x >= pCharData->font_w || y >= pCharData->font_h)
                break;

            bsel = GetMaskStatus(pCharData->mask, x, y);
            szPrintLine[y][x] = bsel > 0 ? '#' : ' ';
        }
    }
}
static CFontMutex g_FontMutex;
static CFontMutex g_CodecMutex;

typedef struct {
    unsigned int key;
    CHAR_STRU *value;
} FontMapItem;

typedef struct {
    unsigned int key;
    CHAR_CODEC_STRU *value;
} CodecMapItem;

static FontMapItem *g_FontMap = NULL;
static int g_FontMapSize = 0;
static FontMapItem *g_UsingMap = NULL;
static int g_UsingMapSize = 0;
static CodecMapItem *g_CodecMap = NULL;
static int g_CodecMapSize = 0;

static int g_CharCount = 0;
static int g_bFontInit = 0;
static int g_bCodecInit = 0;

#if PLATFORM_X86
#define AJ_FONT_PATH "./"
#else
//#define AJ_FONT_PATH AJ_APP_PATH
#define AJ_FONT_PATH "/opt/ch"
#endif

static pthread_mutex_t g_MutexCodecs = PTHREAD_MUTEX_INITIALIZER;

static void LockCodecs(void)
{
//#if defined(LINUX)
#if 1
    pthread_mutex_lock(&g_MutexCodecs);
#endif
//#if defined(WIN32)
#if 0
    WaitForSingleObject(g_MutexCodecs, INFINITE);
#endif
}

void UnLockCodecs(void)
{
//#if defined(LINUX)
#if 1
    pthread_mutex_unlock(&g_MutexCodecs);
#endif
//#if defined(WIN32)
#if 0
    ReleaseMutex(g_MutexCodecs);
#endif
}

int ReadCodec(const char* filename)
{
    int ret = -1;
    if( NULL == filename || filename[0] == 0)
    {   
        printf("filename is NULL.\n");
        return ret;
    }
    
    if(F_OK != access(filename, F_OK))
    {
//      printf("not exist %s.\n", filename);
        return ret;
    }

    FONT_HEAD FontHead;
    CHAR_CODEC_STRU FontData;
    memset(&FontHead, 0, sizeof(FontHead));
    FONT_HEAD *pHeader = (FONT_HEAD*)&FontHead;
    
    unsigned int iIndex = 0;

    int fp = open(filename, O_RDONLY); 
    if(fp < 0)
    {
        printf("%s open fail. \n", filename);
        return ret;
    }
    int filesize = lseek(fp, 0, SEEK_END);
    lseek(fp, 0, SEEK_SET);

    int nReadSize = sizeof(FontHead);
    int readRet = read(fp, &FontHead, nReadSize);
    if( readRet != nReadSize)
    {
        printf("%s read head return %d, failed. \n", filename, readRet);
        goto exit;
    }
    
    if( strcmp("cham.li.codecmap", pHeader->magic) != 0)
    {
        printf("file %s not codec file.\n", filename);
        goto exit;
    }
    
    if( (filesize - sizeof(FONT_HEAD))/ sizeof(CHAR_CODEC_STRU) != pHeader->chars_count)
    {
        __ERR("file %s font size %d error. filesize=%d, sizeof(FONT_HEAD)=%u\n", 
            filename, pHeader->chars_count, filesize, sizeof(FONT_HEAD));
        goto exit;
    }

    __ERR("%s: codec count: %u\n", filename, pHeader->chars_count);

    CFontMutex_Lock(&g_CodecMutex);

    for( iIndex = 0; iIndex < pHeader->chars_count; iIndex++)
    {
        memset(&FontData, 0, sizeof(FontData));
        nReadSize = sizeof(FontData);
        readRet = read(fp, &FontData, nReadSize);
        if( readRet != nReadSize)
        {
            printf("%s read index %d data return %d, failed. \n", filename, iIndex, readRet);
            break;
        }

        int found = 0;
        for (int j = 0; j < g_CodecMapSize; j++)
        {
            if (g_CodecMap[j].key == FontData.unicode)
            {
                found = 1;
                CHAR_CODEC_STRU *pData = g_CodecMap[j].value;
                if (NULL != pData)
                {
                    memcpy(pData, &FontData, sizeof(FontData));
                }
                break;
            }
        }

        if (!found)
        {
            CHAR_CODEC_STRU *pData = (CHAR_CODEC_STRU *)malloc(sizeof(CHAR_CODEC_STRU));
            if (NULL != pData)
            {
                memcpy(pData, &FontData, sizeof(FontData));
                g_CodecMapSize++;
                g_CodecMap = realloc(g_CodecMap, g_CodecMapSize * sizeof(CodecMapItem));
                g_CodecMap[g_CodecMapSize-1].key = FontData.unicode;
                g_CodecMap[g_CodecMapSize-1].value = pData;
            }
        }
    }
    CFontMutex_UnLock(&g_CodecMutex);

    close(fp);

    ret = 0;
exit:
    return ret;
}


int codec_init()//用于只需要CODEC转换，不需要点阵的场合，减少内存使用
{
    unsigned int iIndex = 0;
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s.codecmap", AJ_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadCodec(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s.codecmap", USER_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadCodec(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s.codecmap", "/tmp", FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadCodec(szFileName);
        }
    }

    g_bCodecInit = 1;

    return 0;
}

int codec_deinit() {
    CFontMutex_Lock(&g_CodecMutex);

    for (int i = 0; i < g_CodecMapSize; i++)
    {
        CHAR_CODEC_STRU *pData = g_CodecMap[i].value;
        if (NULL != pData)
        {
            free(pData);
        }
    }
    free(g_CodecMap);
    g_CodecMap = NULL;
    g_CodecMapSize = 0;

    g_bCodecInit = 0;
    CFontMutex_UnLock(&g_CodecMutex);
    return 0;
}


const CHAR_CODEC_STRU *codec_find(unsigned int data)
{
    const CHAR_CODEC_STRU *pFont = NULL;

    for (int i = 0; i < g_CodecMapSize; i++) {
        if (g_CodecMap[i].key == data) {
            pFont = g_CodecMap[i].value;
            break;
        }
    }

    return pFont;
}

//读取文件的时候生成所有字库点阵map
int ReadFont(const char* filename)
{
    int ret = -1;
    if( NULL == filename || filename[0] == 0)
    {   
        printf("filename is NULL.\n");
        return ret;
    }
    
    if(F_OK != access(filename, F_OK))
    {
        printf("not exist %s.\n", filename);
        return ret;
    }

    FONT_HEAD FontHead;
    CHAR_STRU FontData;
    memset(&FontHead, 0, sizeof(FontHead));
    FONT_HEAD *pHeader = (FONT_HEAD*)&FontHead;
    
    unsigned int iIndex = 0;

    int fp = open(filename, O_RDONLY); 
    if(fp < 0)
    {
        printf("%s open fail. \n", filename);
        return ret;
    }
    int filesize = lseek(fp, 0, SEEK_END);
    lseek(fp, 0, SEEK_SET);

    int nReadSize = sizeof(FontHead);
    int readRet = read(fp, &FontHead, nReadSize);
    if( readRet != nReadSize)
    {
        printf("%s read head return %d, failed. \n", filename, readRet);
        goto exit;
    }
    
    if( strcmp("cham.li.font.16x16", pHeader->magic) != 0)
    {
        printf("file %s not font file.\n", filename);
        goto exit;
    }
    
    if( (filesize - sizeof(FONT_HEAD))/ sizeof(CHAR_STRU) != pHeader->chars_count)
    {
        printf("file %s font size %d error. filesize=%d, sizeof(FONT_HEAD)=%u\n", 
            filename, pHeader->chars_count, filesize, sizeof(FONT_HEAD));
        goto exit;
    }

    printf("%s: font count: %u\n", filename, pHeader->chars_count);

    CFontMutex_Lock(&g_FontMutex);

    for( iIndex = 0; iIndex < pHeader->chars_count; iIndex++)
    {
        memset(&FontData, 0, sizeof(FontData));
        nReadSize = sizeof(FontData);
        readRet = read(fp, &FontData, nReadSize);
        if( readRet != nReadSize)
        {
            printf("%s read index %d data return %d, failed. \n", filename, iIndex, readRet);
            break;
        }

        int found = 0;
        for (int j = 0; j < g_FontMapSize; j++)
        {
            if (g_FontMap[j].key == FontData.unicode)
            {
                found = 1;
                CHAR_STRU *pData = g_FontMap[j].value;
                if (NULL != pData)
                {
                    memcpy(pData, &FontData, sizeof(FontData));
                }
                break;
            }
        }

        if (!found)
        {
            CHAR_STRU *pData = (CHAR_STRU *)malloc(sizeof(FontData));
            if (NULL != pData) {
                memcpy(pData, &FontData, sizeof(FontData));
                g_FontMapSize++;
                g_FontMap = realloc(g_FontMap, g_FontMapSize * sizeof(FontMapItem));
                g_FontMap[g_FontMapSize-1].key = FontData.unicode;
                g_FontMap[g_FontMapSize-1].value = pData;
            }
        }
    }
    CFontMutex_UnLock(&g_FontMutex);

    close(fp);

    ret = 0;
exit:
    return ret;
}

//读取文件的时候不先生成所有字库点阵，直接在把点阵写入using map，降低系统内存占用(即使是暂时的)
int ReadFontToUsing(const char* filename)
{
    int ret = -1;
    if( NULL == filename || filename[0] == 0)
    {   
        printf("filename is NULL.\n");
        return ret;
    }
    
    if(F_OK != access(filename, F_OK))
    {
        printf("not exist %s.\n", filename);
        return ret;
    }

    FONT_HEAD FontHead;
    CHAR_STRU FontData;
    memset(&FontHead, 0, sizeof(FontHead));
    FONT_HEAD *pHeader = (FONT_HEAD*)&FontHead;
    
    unsigned int iIndex = 0;

    int fp = open(filename, O_RDONLY); 
    if(fp < 0)
    {
        printf("%s open fail. \n", filename);
        return ret;
    }
    int filesize = lseek(fp, 0, SEEK_END);
    lseek(fp, 0, SEEK_SET);

    int nReadSize = sizeof(FontHead);
    int readRet = read(fp, &FontHead, nReadSize);
    if( readRet != nReadSize)
    {
        printf("%s read head return %d, failed. \n", filename, readRet);
        goto exit;
    }
    
    if( strcmp("cham.li.font.16x16", pHeader->magic) != 0)
    {
        printf("file %s not font file.\n", filename);
        goto exit;
    }
    
    if( (filesize - sizeof(FONT_HEAD))/ sizeof(CHAR_STRU) != pHeader->chars_count)
    {
        printf("file %s font size %d error. filesize=%d, sizeof(FONT_HEAD)=%u\n", 
            filename, pHeader->chars_count, filesize, sizeof(FONT_HEAD));
        goto exit;
    }

    printf("%s: font count: %u\n", filename, pHeader->chars_count);

    CFontMutex_Lock(&g_FontMutex);

    for( iIndex = 0; iIndex < pHeader->chars_count; iIndex++)
    {
        memset(&FontData, 0, sizeof(FontData));
        nReadSize = sizeof(FontData);
        readRet = read(fp, &FontData, nReadSize);
        if( readRet != nReadSize)
        {
            printf("%s read index %d data return %d, failed. \n", filename, iIndex, readRet);
            break;
        }

        for (int j = 0; j < g_UsingMapSize; j++)
        {
            if (g_UsingMap[j].key == FontData.unicode)
            {
                CHAR_STRU **pData = &g_UsingMap[j].value;
                if (NULL == *pData)
                {
                    *pData = (CHAR_STRU *)malloc(sizeof(CHAR_STRU));
                    if (NULL != *pData)
                    {
                        memcpy(*pData, &FontData, sizeof(CHAR_STRU));
                    }
                }
                else 
                {
                    memcpy(*pData, &FontData, sizeof(CHAR_STRU));
                }
                break;
            }
        }
    }
    CFontMutex_UnLock(&g_FontMutex);

    close(fp);

    ret = 0;
exit:
    return ret;
}


int font_init()
{
    unsigned int iIndex = 0;
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", AJ_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFont(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", USER_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFont(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", "/tmp", FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFont(szFileName);
        }
    }

    g_CharCount = g_FontMapSize;
    g_bFontInit = 1;
    g_bCodecInit = 1;
    __ERR("Get char count=%u\n", g_CharCount);

    return 0;
}

int font_deinit()
{
    CFontMutex_Lock(&g_FontMutex);

    for (int i = 0; i < g_FontMapSize; i++)
    {
        CHAR_STRU *pData = g_FontMap[i].value;
        if (NULL != pData)
        {
            free(pData);
        }
    }
    free(g_FontMap);
    g_FontMap = NULL;
    g_FontMapSize = 0;
    
    g_CharCount = 0;
    g_bFontInit = 0;
    CFontMutex_UnLock(&g_FontMutex);

    __ERR("OK\n");

    return 0;
}

int font_deinit_usingmap()
{
    CFontMutex_Lock(&g_FontMutex);
    for (int i = 0; i < g_UsingMapSize; i++)
    {
        CHAR_STRU *pData = g_UsingMap[i].value;
        if (NULL != pData)
        {
            free(pData);
        }
    }
    free(g_UsingMap);
    g_UsingMap = NULL;
    g_UsingMapSize = 0;
    CFontMutex_UnLock(&g_FontMutex);

    return 0;
}

const CHAR_STRU *font_find(unsigned int data)
{
    const CHAR_STRU *pFont = NULL;

    CFontMutex_Lock(&g_FontMutex);
    for (int i = 0; i < g_FontMapSize; i++)
    {
        if (g_FontMap[i].key == data)
        {
            pFont = g_FontMap[i].value;
            break;
        }
    }
    CFontMutex_UnLock(&g_FontMutex);

    return pFont;
}

const CHAR_STRU *font_find_using(unsigned int data)
{
    const CHAR_STRU *pFont = NULL;

    CFontMutex_Lock(&g_FontMutex);
    for (int i = 0; i < g_UsingMapSize; i++)
    {
        if (g_UsingMap[i].key == data)
        {
            pFont = g_UsingMap[i].value;
            break;
        }
    }
    CFontMutex_UnLock(&g_FontMutex);

    return pFont;
}


int font_get_using_data()
{
    unsigned int iIndex;
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", AJ_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFontToUsing(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", USER_FONT_PATH, FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFontToUsing(szFileName);
        }
    }
    for( iIndex = 0; iIndex < sizeof(g_FontList)/sizeof(g_FontList[0]); iIndex++)
    {
        char szFileName[128] = {0};
        snprintf(szFileName, sizeof(szFileName), "%s/%.*s", "/tmp", FONT_LIST_NAME_MAX - 1, g_FontList[iIndex]);
        if( F_OK == access(szFileName, F_OK))
        {
            __ERR("%s\n", szFileName);
            ReadFontToUsing(szFileName);
        }
    }

    return 0;
}

static void *font_get_using_thread(void *arg)
{
    prctl(PR_SET_NAME, __func__);  
    pthread_detach(pthread_self());
    font_get_using_data();
    __ERR("exit main loop\n");
    return 0;   
}

//放在线程中去读取文件，避免读取文件耗时影响当前OSD刷新线程导致卡秒
//启动之前已经更新了g_UsingMap并将没有的文字点阵置成了NULL，所以同一个字符串不会重复启动线程
//不同线程的字符串点阵读取，因为g_UsingMap有锁，所以也不会出问题
int start_font_get_using_thread(void)
{
    pthread_t threadid = 0;
    if(pthread_create(&threadid, NULL, font_get_using_thread, NULL))
    {
        __ERR("Create thread failed, err=%s\n",strerror(errno));
        return -1;
    }
    
    __ERR("start thread OK!!!\n");
    return 0;
}

static void font_check_using_unicode(const char *pString, int *bGetUsingFromGlobalFontMap)
{
    const char *ptr = (const char *)pString;
    const char *pEnd = pString + strlen(pString);    
    while (*ptr != 0 && ptr < pEnd)
    {
        int bytes = GetUtf8Bytes1(*(unsigned char*)ptr);
        
        unsigned int nUtf8 = 0;
        int iIndex = 0;
        for (iIndex = 0; iIndex < bytes; iIndex++)
            nUtf8 = (nUtf8 << 8) | (ptr[iIndex] & 0xff);

        unsigned int unicode = utf8_to_unicode(nUtf8);

        CFontMutex_Lock(&g_FontMutex);
        int found = 0;
        for (int j = 0; j < g_UsingMapSize; j++)
        {
            if (g_UsingMap[j].key == unicode)
            {
                found = 1;
                break;
            }
        }
        
        if (!found)
        {
            g_UsingMapSize++;
            g_UsingMap = realloc(g_UsingMap, g_UsingMapSize * sizeof(FontMapItem));
            g_UsingMap[g_UsingMapSize-1].key = unicode;
            g_UsingMap[g_UsingMapSize-1].value = NULL;
            *bGetUsingFromGlobalFontMap = 1;
        }
        CFontMutex_UnLock(&g_FontMutex);
        
        ptr += bytes;
    }
}

void font_check_using(const char * pString, int bInThread)
{
    int bGetUsingFromGlobalFontMap = 0;//是否需要重新从文件读取g_FontMap，用于构建使用文字的map
    font_check_using_unicode(pString, &bGetUsingFromGlobalFontMap);
    
    int iIndex; 
    //加入星期
    for( iIndex = 0; iIndex < 8; iIndex++)
    {
        font_check_using_unicode(week_day_chs[iIndex], &bGetUsingFromGlobalFontMap);
        font_check_using_unicode(week_day_russion[iIndex], &bGetUsingFromGlobalFontMap);
    }
    //加入上午下午
    for( iIndex = 0; iIndex < 2; iIndex++)
    {
        font_check_using_unicode(am_pm_chs[iIndex], &bGetUsingFromGlobalFontMap);
    }

    //加入ASCII字符，避免重复读取
    for (iIndex = 0x20; iIndex < 128; iIndex++)
    {
        unsigned int unicode = iIndex;
        CFontMutex_Lock(&g_FontMutex);
        int found = 0;
        for (int j = 0; j < g_UsingMapSize; j++)
        {
            if (g_UsingMap[j].key == unicode)
            {
                found = 1;
                break;
            }
        }
        
        if (!found)
        {
            g_UsingMapSize++;
            g_UsingMap = realloc(g_UsingMap, g_UsingMapSize * sizeof(FontMapItem));
            g_UsingMap[g_UsingMapSize-1].key = unicode;
            g_UsingMap[g_UsingMapSize-1].value = NULL;
            bGetUsingFromGlobalFontMap = 1;
        }
        CFontMutex_UnLock(&g_FontMutex);
    }

    if( bGetUsingFromGlobalFontMap > 0 )
    {
        char buffer2[512] = {0};
        
        if( 0 == hexdataTohexStr(pString, strlen(pString), buffer2, 512) )
        {           
            __ERR("utf8: %s\n", buffer2);
        }
        buffer2[0] = 0;
        
        CFontMutex_Lock(&g_FontMutex);
        for (int i = 0; i < g_UsingMapSize; i++)
        {
            const unsigned int unicode = g_UsingMap[i].key;
            if (strlen(buffer2) + 6 > 512)
                break;
            sprintf(buffer2+strlen(buffer2), "%04x ", unicode);
        }
        CFontMutex_UnLock(&g_FontMutex);
        __ERR("alll unicode: %s\n", buffer2);

        if( bInThread == 0 )
            font_get_using_data();
        else
            start_font_get_using_thread();//线程中更新点阵数据，不阻塞当前线程的OSD绘制
    }
}


unsigned int utf8_2_gb2312(const unsigned char *pIn, unsigned char *pOut, int *pOutLen)
{
    const unsigned char* p2 = pIn;  
    unsigned int nUtf82 = (((*(p2)) & 0xff) << 16 )
                    +  (((*(p2+1)) & 0xff) << 8 )
                    +  (((*(p2+2)) & 0xff) << 0 );
    unsigned int unicode = utf8_to_unicode(nUtf82);
    
    CFontMutex_Lock(&g_CodecMutex);
    const CHAR_CODEC_STRU *pdata = codec_find(unicode);
    if( NULL != pdata)
    {
        if( pOut != NULL)
        {
            *pOut = (pdata->gbk >> 8) & 0xff;
            *(pOut+1) = (pdata->gbk >> 0) & 0xff;
        }
        if( pOutLen != NULL)
        {
            *pOutLen = 2;
        }

        __ERR("got gbk: %#x\n", pdata->gbk);

        unsigned int gbk = pdata->gbk;
        CFontMutex_UnLock(&g_CodecMutex);
        return gbk;
    }

    CFontMutex_UnLock(&g_CodecMutex);
    if (pOutLen != NULL) *pOutLen = 0;
    __ERR("not found %#x\n", nUtf82);
    return 0;    
}

unsigned int gb2312_2_utf8(const unsigned char *pIn, unsigned char *pOut, int *pOutLen, int bBig5)
{
    const unsigned char* pgb = pIn; 
    unsigned int nGBK = (((*pgb) & 0xff) << 8 ) 
                        + (((*(pgb+1)) & 0xff) << 0);

    CFontMutex_Lock(&g_CodecMutex);
    for (int i = 0; i < g_CodecMapSize; i++)
    {
        const CHAR_CODEC_STRU *pData = g_CodecMap[i].value;
        if (NULL != pData && pData->gbk == nGBK)
        {
            int bytes = GetUtf8ValueU8(pData->utf8, pOut);
            if (pOutLen != NULL)
            {
                *pOutLen = bytes;
            }

            unsigned int utf8 = pData->utf8;
            CFontMutex_UnLock(&g_CodecMutex);
            return utf8;
        }
    }
    CFontMutex_UnLock(&g_CodecMutex);
    if (pOutLen != NULL) *pOutLen = 0;

    return 0;   
}

int str_utf8_2_gb2312(const char *pIn, char *pOut)
{
    int ret = 0;
    if( pOut == NULL )
    {
        return 0;
    }
    
    if( pIn == NULL )
    {
        pOut[0] = 0;
        return 0;
    }


    ret = find_gb2312_code_bymap((char*)pIn, pOut);
    if (ret > 0)
    {
        __ERR("outlen %d, %s\n", ret, pOut);
        return ret;
    }

    LockCodecs();

    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }

    char title_hex[128];
    memset(title_hex, 0, 128);
    hexdataTohexStr((const char*)pIn, strlen((const char*)pIn), (char*)title_hex, 128);

    int len = strlen((char*)pIn);
    __ERR("inlen %d: %s\n", len ,title_hex);
    int nTransLen = 0;
    int nTitleLen = 0;
    while(nTransLen < len)
    {
        const unsigned char * ptr = (const unsigned char *)pIn + nTransLen;
        if(*ptr>0x80)
        {
            if( nTitleLen + 2 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            int nOutLen = 0;
            utf8_2_gb2312((const unsigned char *)ptr, (unsigned char *)pOut+nTitleLen, &nOutLen);
            nTitleLen += nOutLen;

            nTransLen += 3;         
        }
        else
        {
            if( nTitleLen + 1 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            *(pOut+nTitleLen) = *ptr;
            nTransLen += 1;
            nTitleLen += 1;                 
        }

    }
    *(pOut+nTitleLen) = 0;

    if (nTitleLen > 0)
    {
        utf8_2_gb2312_map_add((char*)pIn, pOut, nTitleLen);
    }

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    UnLockCodecs();
    __ERR("outlen %d, %s", nTitleLen, pOut);
    return nTitleLen;
}


int str_gb2312_2_utf8(const char *pIn, char *pOut, int bBig5)
{
    if( pOut == NULL )
    {
        return 0;
    }
    
    if( pIn == NULL )
    {
        pOut[0] = 0;
        return 0;
    }

    LockCodecs();
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }
    
    int len = strlen((const char*)pIn);
    int nTransLen = 0;
    int nTitleLen = 0;
    while(nTransLen < len)
    {
        const unsigned char * ptr = (const unsigned char *)pIn + nTransLen;
        if(*ptr>0x80)
        {
            if( nTitleLen + 3 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            int nOutLen = 0;
            gb2312_2_utf8((const unsigned char *)ptr, (unsigned char *)pOut+nTitleLen, &nOutLen, bBig5);
            nTitleLen += nOutLen;
            nTransLen += 2;
            
        }
        else
        {
            if( nTitleLen + 1 >= TITLE_MAX_LEN )
            {
                break;
            }

            *(pOut+nTitleLen) = *ptr;
            nTransLen += 1;
            nTitleLen += 1;             
        }
    }

    *(pOut+nTitleLen) = 0;
    
    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }

    UnLockCodecs();
    __ERR("outlen %d, %s\n", nTitleLen, pOut);

    return nTitleLen;
}

unsigned int utf8_2_gbkbig5(const unsigned char *pIn, unsigned char *pOut, int *pOutLen)
{
    const unsigned char* p2 = pIn;  
    unsigned int nUtf82 = (((*(p2)) & 0xff) << 16 )
                    +  (((*(p2+1)) & 0xff) << 8 )
                    +  (((*(p2+2)) & 0xff) << 0 );

    unsigned int unicode = utf8_to_unicode(nUtf82);
    CFontMutex_Lock(&g_CodecMutex);
    const CHAR_CODEC_STRU *pdata = codec_find(unicode);
    if( NULL != pdata)
    {
        *pOut = (pdata->big5 >> 8) & 0xff;
        *(pOut+1) = (pdata->big5 >> 0) & 0xff;
        *pOutLen = 2;

        __ERR("got gbk: %#x\n", pdata->big5);
        unsigned int big5 = pdata->big5;
        CFontMutex_UnLock(&g_CodecMutex);
        
        return big5;
    }
    CFontMutex_UnLock(&g_CodecMutex);

    if( pOutLen != NULL)*pOutLen = 0;
    __ERR("not found %#x\n", nUtf82);
    return 0; 
}


unsigned int gbkbig5_2_utf8(const unsigned char *pIn, unsigned char *pOut, int *pOutLen, int bBig5)
{
    const unsigned char* pgb = pIn; 
    unsigned int nGBK = (((*pgb) & 0xff) << 8 ) 
                        + (((*(pgb+1)) & 0xff) << 0);

    CFontMutex_Lock(&g_CodecMutex);
    for (int i = 0; i < g_CodecMapSize; i++)
    {
        const CHAR_CODEC_STRU *pData = g_CodecMap[i].value;
        if (NULL != pData && pData->big5 == nGBK)
        {
            int bytes = GetUtf8ValueU8(pData->utf8, pOut);
            *pOutLen = bytes;
            unsigned int utf8 = pData->utf8;
            CFontMutex_UnLock(&g_CodecMutex);
            return utf8;
        }
    }
    CFontMutex_UnLock(&g_CodecMutex);
    if (pOutLen != NULL) *pOutLen = 0;
    return 0; 
}

int str_utf8_2_gbkbig5(const char *pIn, char *pOut)
{
    if( pOut == NULL )
    {
        return 0;
    }
    
    if( pIn == NULL )
    {
        pOut[0] = 0;
        return 0;
    }

    LockCodecs();
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }

    int len = strlen((const char*)pIn);
    int nTransLen = 0;
    int nTitleLen = 0;
    while(nTransLen < len)
    {
        const unsigned char * ptr = (const unsigned char *)pIn + nTransLen;
        if(*ptr>0x80)
        {
            if( nTitleLen + 2 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            int nOutLen = 0;
            utf8_2_gbkbig5((const unsigned char *)ptr, (unsigned char *)pOut+nTitleLen, &nOutLen);
            nTitleLen += nOutLen;

            nTransLen += 3;         
        }
        else
        {
            if( nTitleLen + 1 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            *(pOut+nTitleLen) = *ptr;
            nTransLen += 1;
            nTitleLen += 1;                 
        }

    }
    *(pOut+nTitleLen) = 0;

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    UnLockCodecs();
    __ERR("outlen %d, %s\n", nTitleLen, pOut);

    return nTitleLen;
}


int str_gbkbig5_2_utf8(const char *pIn, char *pOut, int bBig5)
{
    if( pOut == NULL )
    {
        return 0;
    }
    
    if( pIn == NULL )
    {
        pOut[0] = 0;
        return 0;
    }
    LockCodecs();
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }
    
    int len = strlen((const char*)pIn);
    int nTransLen = 0;
    int nTitleLen = 0;
    while(nTransLen < len)
    {
        const unsigned char * ptr = (const unsigned char *)pIn + nTransLen;
        if(*ptr>0x80)
        {
            if( nTitleLen + 3 >= TITLE_MAX_LEN )
            {
                break;
            }
            
            int nOutLen = 0;
            gbkbig5_2_utf8((const unsigned char *)ptr, (unsigned char *)pOut+nTitleLen, &nOutLen, bBig5);
            nTitleLen += nOutLen;
            nTransLen += 2;
            
        }
        else
        {
            if( nTitleLen + 1 >= TITLE_MAX_LEN )
            {
                break;
            }

            *(pOut+nTitleLen) = *ptr;
            nTransLen += 1;
            nTitleLen += 1;             
        }
    }

    *(pOut+nTitleLen) = 0;

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    UnLockCodecs();
    __ERR("outlen %d, %s\n", nTitleLen, pOut);

    return nTitleLen;
}

void show_codec_table()
{
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }
    
    CFontMutex_Lock(&g_CodecMutex);
    for (int i = 0; i < g_CodecMapSize; i++)
    {
        const CHAR_CODEC_STRU *pFont = g_CodecMap[i].value;
        printf("utf8=%#8x, unicode=%#8x, gbk=%#x, big5=%#x\n",
            pFont->utf8, pFont->unicode, pFont->gbk, pFont->big5);
    }
    CFontMutex_UnLock(&g_CodecMutex);

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    return; 
}

void show_fonts_all()
{
    int bFontNeedDeInit = 0;
    if( g_bFontInit != 1 )
    {
        font_init();
        bFontNeedDeInit = 1;
    }

    for (int i = 0; i < g_FontMapSize; i++)
    {
        const CHAR_STRU *pFont = g_FontMap[i].value;
        printf("unicode=%#8x, %2uX%2u\n",
            pFont->unicode, pFont->font_w, pFont->font_h);
        
        DrawFontMask(pFont);
    }

    if( bFontNeedDeInit )
    {
        font_deinit();
    }
    return; 
}


void show_gb2312str_inpoint(const char *pszGb2312, int type)
{
    LockCodecs();
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }
    
    int bFontNeedDeInit = 0;
    if( g_bFontInit != 1 )
    {
        font_init();
        bFontNeedDeInit = 1;
    }

    
    char szPrintLine[16][256+4];    //最多显示256列，也就是16个汉字,去掉结束符，设置15个汉字
    memset(szPrintLine, 0, 16*260);

    int len = strlen(pszGb2312);
    if( len > 15*2)
        len = 15*2;


    int pos = 0;
    char *p = (char *)pszGb2312;
    while( p != NULL && *p != 0 && pos < len)
    {
        char szOneHzPrintLine[16][32];  //最多显示256列，也就是16个汉字
        memset(szOneHzPrintLine, 0, 16*32);

        unsigned char data = (*p) & 0xff;
        if( data > 0x80)
        {
            unsigned int utf8 = gb2312_2_utf8((const unsigned char *)p, NULL, NULL, 0);
            unsigned int unicode = utf8_to_unicode(utf8);
            const CHAR_STRU *pdata = font_find(unicode);
            if( pdata != NULL)
            {
                DrawFontMaskInLineArray(pdata, szOneHzPrintLine);
            }
            
            p+=2;
            pos += 2;
        }
        else
        {
            unsigned int unicode = *p;
            const CHAR_STRU *pdata = font_find(unicode);
            if( pdata != NULL)
            {
                DrawFontMaskInLineArray(pdata, szOneHzPrintLine);
            }

            p+=1;
            pos += 1;
        }

        int iIndex = 0;
        for( iIndex = 0; iIndex < 16; iIndex++)
        {
            strcat(szPrintLine[iIndex], szOneHzPrintLine[iIndex]);
        }
    }

    int iIndex = 0;
    for( iIndex = 0; iIndex < 16; iIndex++)
    {
        ShowString("", type, szPrintLine[iIndex]);
    }

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    
    if( bFontNeedDeInit )
    {
        font_deinit();
    }
    UnLockCodecs();
}

void show_utf8str_inpoint(const char *ptr, int type)
{
    LockCodecs();
    int bCodecNeedDeInit = 0;
    if( g_bCodecInit != 1 )
    {
        codec_init();
        bCodecNeedDeInit = 1;
    }
    
    int bFontNeedDeInit = 0;
    if( g_bFontInit != 1 )
    {
        font_init();
        bFontNeedDeInit = 1;
    }
    
    char szPrintLine[16][256+4];    //最多显示256列，也就是16个汉字,去掉结束符，设置15个汉字
    memset(szPrintLine, 0, 16*260);

    int draw_len = 0;
    while(* ptr)
    {
        char szOneHzPrintLine[16][32];  //最多显示256列，也就是16个汉字
        memset(szOneHzPrintLine, 0, 16*32);
        
        int bytes = GetUtf8Bytes1(*(unsigned char*)ptr);
        unsigned int nUtf8 = 0;
        int iIndex = 0;
        for( iIndex = 0; iIndex < bytes; iIndex++)
            nUtf8 = (nUtf8 << 8) | (ptr[iIndex] & 0xff);

        unsigned int unicode = utf8_to_unicode(nUtf8);
        const CHAR_STRU *pdata = font_find(unicode);
        if( pdata != NULL)
        {
            DrawFontMaskInLineArray(pdata, szOneHzPrintLine);
        }
        
        for( iIndex = 0; iIndex < 16; iIndex++)
        {
            strcat(szPrintLine[iIndex], szOneHzPrintLine[iIndex]);
        }

        ptr += bytes;

        draw_len += (bytes> 1? 2:1);
        if( draw_len > 30)//最多画30个字符，否则会超长
            break;
    }

    if( bCodecNeedDeInit )
    {
        codec_deinit();
    }
    
    if( bFontNeedDeInit )
    {
        font_deinit();
    }

    int iIndex = 0;
    for( iIndex = 0; iIndex < 16; iIndex++)
    {
        ShowString("", type, szPrintLine[iIndex]);
    }
    UnLockCodecs();
}

#if 0
extern int osd_get_date_time_str(int fmt, int bWeekday, int time24or12, char *osdbuf);
extern int osd_get_time_str(int fmt, struct tm *ptm, int bWeekday, int bEnglish, int time24or12, const char *space, char *osdbuf);
extern int osd_get_date_str(int fmt, struct tm *ptm, int bWeekday, const char *space, char *osdbuf);

void draw_time_in_yuv(char *pYuvData, unsigned int width, unsigned int height)
{
//在YUV中固定位置绘制时间，用于IVE无标题的NV12 YUV处理后再编码成jpg时加上时间。用于编码进程，确保已经先有进行初始化font using，否则无法绘制时间
    char szDateStr[64] = {0};
#if 1
    char space[4] = {0};
    struct tm ptm; 
    SystemLocalTime(&ptm);
    char szTimeStr[32];
    osd_get_date_str(0, &ptm, 0, space, szDateStr);
    osd_get_time_str(0, &ptm, 0, 1, 0, space, szTimeStr);

    strcat(szDateStr, " ");
    strcat(szDateStr, szTimeStr);
#else   
    osd_get_date_time_str(0, 0, 1,  szDateStr);
#endif

    LockCodecs();
    char szPrintLine[16][256+4];    //最多显示256列，也就是16个汉字,去掉结束符，设置15个汉字
    memset(szPrintLine, 0, 16*260);


    char *ptr = szDateStr;
    int draw_len = 0;
    while(* ptr)
    {
        char szOneHzPrintLine[16][32];  //最多显示256列，也就是16个汉字
        memset(szOneHzPrintLine, 0, 16*32);
        
        int bytes = GetUtf8Bytes1(*(unsigned char*)ptr);
        unsigned int nUtf8 = 0;
        int iIndex = 0;
        for( iIndex = 0; iIndex < bytes; iIndex++)
            nUtf8 = (nUtf8 << 8) | (ptr[iIndex] & 0xff);

        unsigned int unicode = utf8_to_unicode(nUtf8);
        const CHAR_STRU *pdata = font_find_using(unicode);
        if( pdata != NULL)
        {
            DrawFontMaskInLineArray(pdata, szOneHzPrintLine);
        }
        
        for( iIndex = 0; iIndex < 16; iIndex++)
        {
            strcat(szPrintLine[iIndex], szOneHzPrintLine[iIndex]);
        }

        ptr += bytes;

        draw_len += (bytes> 1? 2:1);
        if( draw_len > 30)//最多画30个字符，否则会超长
            break;
    }


    int iIndex = 0;
    int nFromX = 16;
    int nFromY = 16;
    
    for( iIndex = 0; iIndex < 16; iIndex++)
    {
        char *pHead = pYuvData + (width * (iIndex + nFromY)) + nFromX;
        unsigned int jIndex = 0;
        for( jIndex = 0; jIndex < 256; jIndex++)
        {
            if( jIndex + nFromX > width)
                break;
            
            char *pDst = pHead + jIndex;
            char c = szPrintLine[iIndex][jIndex];
            if( c == 0)
                break;
            if( c == '#' )
            {
                *pDst = 255;
            }
        }       
    }
    UnLockCodecs();
}
#endif

