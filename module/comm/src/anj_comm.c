#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/statfs.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_comm.h"
#include "anj_sysmng.h"

typedef enum
{
	SHOWDATA_PRINTF,
	SHOWDATA_DEBUG,
	SHOWDATA_NONE,
}ShowDataType;

#define HEX_PRINT_PER_LEN 16
#define HEX_STR_EACHLIN_MAXNUM 256

void ShowString(const char *szFunc, int nType, const char *szPrintLine)
{
    if (nType == SHOWDATA_PRINTF)
    {
        if (szFunc == NULL || strlen(szFunc) == 0)
            printf("%s\n", szPrintLine);
        else
            printf("[%s] %s\n", szFunc, szPrintLine);
    }
    else if (nType == SHOWDATA_DEBUG)
    {
        if (szFunc == NULL || strlen(szFunc) == 0)
            __INFO("%s\n", szPrintLine);
        else
            __INFO("[%s] %s\n", szFunc, szPrintLine);
    }
}

void debug_show_data_hex(const unsigned char *data, unsigned short length, int nType)
{
    return debug_show_data_hex_ex(data, (unsigned int)length, nType);
}

void debug_show_data_hex_ex(const unsigned char *data, unsigned int length, int nType)
{
    const unsigned char *pTmp = data;
    unsigned short mIndex = 0;
    unsigned short nIndex = 0;
    unsigned short rowCount = 0;
    int bAllZero = 1;
    int bAllCC = 1;

    char szPrintLine[256];
    sprintf(szPrintLine, "buffer address: %#x, length %d:\n", (unsigned int)data, length);
    ShowString(__func__, nType, szPrintLine);

    if (length % HEX_PRINT_PER_LEN == 0)
        rowCount = (unsigned short)(length / HEX_PRINT_PER_LEN);
    else
        rowCount = (unsigned short)(length / HEX_PRINT_PER_LEN) + 1;

    for (mIndex = 0; mIndex < rowCount; mIndex++)
    {
        // Check if 16 Bytes all are zero or all are 0xcc
        bAllZero = 1;
        bAllCC = 1;

        for (nIndex = 0; nIndex < HEX_PRINT_PER_LEN; nIndex++)
        {
            if (mIndex * HEX_PRINT_PER_LEN + nIndex >= length)
                break;
            else
            {
                if (pTmp[mIndex * HEX_PRINT_PER_LEN + nIndex] != 0)
                {
                    bAllZero = 0;
                    break;
                }
            }
        }
        if (!bAllZero)
        {
            for (nIndex = 0; nIndex < HEX_PRINT_PER_LEN; nIndex++)
            {
                if (mIndex * HEX_PRINT_PER_LEN + nIndex >= length)
                    break;
                else
                {
                    if (pTmp[mIndex * HEX_PRINT_PER_LEN + nIndex] != 0xcc)
                    {
                        bAllCC = 0;
                        break;
                    }
                }
            }

            // if All are 0xcc, do not print this line
            if (bAllCC)
                continue;
        }
        else
            // if All are zero, do not print this line
            continue;

        char buffer[HEX_STR_EACHLIN_MAXNUM];
        memset(buffer, 0, HEX_STR_EACHLIN_MAXNUM);
        // Print the offset of this line
        sprintf(buffer, "%#5x: ", mIndex * HEX_PRINT_PER_LEN);

        // Print the hex of each byte of this line
        unsigned int nLen;
        for (nIndex = 0; nIndex < HEX_PRINT_PER_LEN; nIndex++)
        {
            nLen = strlen(buffer);
            if (nLen > HEX_STR_EACHLIN_MAXNUM - 3 - 1)
            {
                break;
            }

            if (mIndex * HEX_PRINT_PER_LEN + nIndex >= length)
                sprintf(buffer + strlen(buffer), "   ");
            else
                sprintf(buffer + strlen(buffer), "%02X ", pTmp[mIndex * HEX_PRINT_PER_LEN + nIndex]);
        }

        nLen = strlen(buffer);
        if (nLen > HEX_STR_EACHLIN_MAXNUM - strlen("\t") - 1)
        {
            break;
        }

        sprintf(buffer + strlen(buffer), "\t");

        // Print the char of each byte of this line
        for (nIndex = 0; nIndex < HEX_PRINT_PER_LEN; nIndex++)
        {
            if (mIndex * HEX_PRINT_PER_LEN + nIndex >= length)
                break;

            nLen = strlen(buffer);
            if (nLen > HEX_STR_EACHLIN_MAXNUM - 1 - 1)
            {
                break;
            }

            if (isprint(pTmp[mIndex * HEX_PRINT_PER_LEN + nIndex]))
                sprintf(buffer + strlen(buffer), "%c", pTmp[mIndex * HEX_PRINT_PER_LEN + nIndex]);
            else
                sprintf(buffer + strlen(buffer), ".");
        }

        buffer[HEX_STR_EACHLIN_MAXNUM - 1] = 0;

        ShowString(__func__, nType, buffer);
    }
}

char *GetRequestParamValue(IXML_Document *pDoc, char *fieldName)
{
    char *fieldValue = NULL;

    const char *tagName = "REQUEST_PARAM";
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, tagName);
    if (pNodelist == NULL)
    {
        __ERR("REQUEST_PARAM not found from message body.\n");
        return NULL;
    }
    else
    {
        IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
        while (tmp != NULL)
        {
            //__ERR("Node name: %s, value: %s\n", tmp->nodeName, tmp->nodeValue);
            if (strcmp(tmp->nodeName, fieldName) == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    fieldValue = anj_mw_malloc(strlen(tmp->nodeValue) + 1);
                    strcpy(fieldValue, tmp->nodeValue);

                    ixmlNodeList_free(pNodelist);
                    return fieldValue;
                }
            }

            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        return NULL;
    }
}

char *GetRequestParamValueByName(IXML_Document *pDoc, char *tag_name, const char *fieldName)
{
    char *fieldValue = NULL;

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, tag_name);
    if (pNodelist == NULL)
    {
        __ERR("tag:%s and field:%s not found from message body.\n", tag_name, fieldName);
        return NULL;
    }
    else
    {
        IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
        while (tmp != NULL)
        {
            if (strcmp(tmp->nodeName, fieldName) == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    fieldValue = anj_mw_malloc(strlen(tmp->nodeValue) + 1);
                    strcpy(fieldValue, tmp->nodeValue);

                    ixmlNodeList_free(pNodelist);
                    return fieldValue;
                }
            }

            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        return NULL;
    }
}

char *GetFileNameFromFullName(const char *full_file_name)
{
    if (full_file_name == NULL)
    {
        return NULL;
    }

    const char *last_slash = NULL;
    for (const char *p = full_file_name; *p != '\0'; p++)
    {
        if (*p == '\\' || *p == '/')
        {
            last_slash = p;
        }
    }

    if (last_slash != NULL)
    {
        return strdup(last_slash + 1);
    }

    return strdup(full_file_name);
}

unsigned long long GetPathFreeSpace(const char *path)
{
    long long freespace = 0;

    struct statfs diskInfo;
    if (statfs(path, &diskInfo) >= 0)
    {
        unsigned long long blocksize = diskInfo.f_bsize;                  // 每个block里包含的字节数
        unsigned long long totalsize = blocksize * diskInfo.f_blocks;     // 总的字节数，f_blocks为block的数目
        unsigned long long freeDisk = diskInfo.f_bfree * blocksize;       // 剩余空间的大小
        unsigned long long availableDisk = diskInfo.f_bavail * blocksize; // 可用空间大小
        __ERR("blocksize= %llu, Total=%llu(%llu KB), free=%llu(%llu KB), avail=%llu(%llu KB)\n",
              blocksize, totalsize, totalsize >> 10, freeDisk, freeDisk >> 10, availableDisk, availableDisk >> 10);

        freespace = freeDisk; // in Bytes//>>10;//in KBytes
        return freespace;
    }
    else
        return -1;
}

int GetDeviceTypeStr(char *szDeviceType)
{
    return anj_sysmng_dev_str_get(szDeviceType);
}

