#include "sn_utils.h"
#include <stdio.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "ixml.h"
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_mw_str.h"
#include "anj_mw_mem.h"
#include "anj_mw_file.h"
#include "anj_mw_time.h"
#include "anj_mw_crypt.h"
#include "platform_sn.h"
#include "anj_mw_comm.h"
#include "driver_interface.h"
#include "firmware_util.h"

#include "flash_rw.h"
#include "sn_tools.h"

static const char snFileArray[][64] =
{
    "/misc/sn_sn_1.dat",
    "/misc/sn_sn_2.dat",
    "/mnt/nand/sn_sn_1.dat",
    "/mnt/nand/sn_sn_2.dat",
    "/data/sn_sn_1.dat",
    "/data/sn_sn_2.dat",
};

int read_sndata_by_file(char* pBuffer, int nSize)
{
    int iRet = -1;
    int iIndex = 0;
    for( iIndex = 0; iIndex < sizeof(snFileArray)/sizeof(snFileArray[0]); iIndex++)
    {
        int read = read_file_to_buffer(snFileArray[iIndex], (char *)pBuffer, nSize);
        if(read == nSize)
        {
            iRet = 0;
            break;
        }
    }

    return iRet;
}

int write_sndata_by_file(char* pBuffer, int nSize)
{
    int iRet = -1;
    int iIndex = 0;
    for(iIndex = 0; iIndex < sizeof(snFileArray)/sizeof(snFileArray[0]); iIndex++)
    {
        int write = write_buffer_to_file(snFileArray[iIndex], (char *)pBuffer, nSize);
        if(write == nSize)
        {
            iRet = 0;
        }
    }

    return iRet;
}

int soft_enc_xml_cmd_parse(const char *xmlBuf)
{
    int msgcmd = -1;

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if(pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MESSAGE_HEADER");
    if(pNodelist != NULL)
    {	
        IXML_Node* tmp = pNodelist->nodeItem->firstAttr;	
        while(tmp != NULL) 
        {
            if (strcmp(tmp->nodeName, "Msg_type") == 0)
            {
                ;
            }
            else if (strcmp(tmp->nodeName, "Msg_code") == 0)
            {
                if(tmp->nodeValue != NULL)
                {
                    msgcmd = atoi(tmp->nodeValue);
                }
                break;
            } 
            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    ixmlDocument_free(pDocNode);
    return msgcmd;	
}

int soft_enc_xml_serial_data_parse(const char *xmlBuf, char *serialid, int buflen1, char *data, int buflen2, unsigned int *nType)
{
    strcpy(data, "");

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if(pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DEVICE");
    if(pNodelist != NULL)
    {
        IXML_Node* tmp = pNodelist->nodeItem->firstAttr;	
        while(tmp != NULL) 
        {
            if (strcmp(tmp->nodeName, "SERIALID") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(serialid, tmp->nodeValue, buflen1 - 1);
                }
            } 
            else if (strcmp(tmp->nodeName, "SERIALDATA") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(data, tmp->nodeValue, buflen2 - 1);
                }
            } 
            else if (strcmp(tmp->nodeName, "TYPE") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    *nType = atoi(tmp->nodeValue);
                }
            } 
            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);	
    }
    else
    {
        __ERR("not found DEVICE\r\n");
        ixmlDocument_free(pDocNode);
        return -1;
    }

    ixmlDocument_free(pDocNode);

    if(strlen(data) > 0 && strlen(serialid) > 0 )
    {
        __DBG("%d: %s --> %s\n", *nType, serialid, data);
        return 0;
    }
    else
    {
        return -1;	
    }
}

int soft_enc_xml_uuid_data_get(const char *xmlBuf, char *identity, int buflen1, char *data, int buflen2, char *checksum, int buflen3)
{
    strcpy(data,"");

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if(pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DEVICE");
    if(pNodelist != NULL)
    {	
        IXML_Node* tmp = pNodelist->nodeItem->firstAttr;	
        while(tmp != NULL) 
        {
            if (strcmp(tmp->nodeName, "Identity") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(identity, tmp->nodeValue, buflen1-1);
                }
            } 
            else if (strcmp(tmp->nodeName, "SNDATA") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(data, tmp->nodeValue, buflen2-1);
                }
            } 
            else if (strcmp(tmp->nodeName, "CHECKSUM") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(checksum, tmp->nodeValue, buflen3-1);
                }
            }

            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);	
    }
    else
    {
        __ERR("not found DEVICE\r\n");
        ixmlDocument_free(pDocNode);
        return -1;
    }

    ixmlDocument_free(pDocNode);

    if(strlen(data) > 0 && strlen(identity) > 0 )
    {
        __DBG("%s --> %s\n", identity, data);
        return 0;
    }
    else
    {
        __ERR("data: %s, identity: %s\r\n", data, identity);
        return -1;
    }
}

int soft_enc_xml_sn_data_parse(const char *xmlBuf, char *cameraid, int buflen1, char *data, int buflen2, char *checksum, int buflen3, int *version)
{
    strcpy(data,"");

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if(pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DEVICE");
    if(pNodelist != NULL)
    {
        IXML_Node* tmp = pNodelist->nodeItem->firstAttr;	
        while(tmp != NULL) 
        {
            if (strcmp(tmp->nodeName, "CameraID") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(cameraid, tmp->nodeValue, buflen1 - 1);
                }
            } 
            else if (strcmp(tmp->nodeName, "SNDATA") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(data, tmp->nodeValue, buflen2 - 1);
                }
            } 
            else if (strcmp(tmp->nodeName, "CHECKSUM") == 0) 
            {
                if(tmp->nodeValue!=NULL)
                {
                    strncpy(checksum, tmp->nodeValue, buflen3 - 1);
                }
            }
            else if (version != NULL && strcmp(tmp->nodeName, "Version") == 0)
            {
                if(tmp->nodeValue!=NULL)
                {
                    *version = atoi(tmp->nodeValue);
                }
            }

            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);	
    }
    else
    {
        __ERR("not found DEVICE\r\n");
        ixmlDocument_free(pDocNode);			
        return -1;
    }

    ixmlDocument_free(pDocNode);

    if(strlen(data) > 0 && strlen(cameraid) > 0 )
    {
        __DBG("%s --> %s\n", cameraid, data);
        return 0;	
    }
    else
    {
        __ERR("data: %s, cameraid: %s\r\n", data, cameraid);
        return -1;	
    }
}

int broardcast_send_request(int sockfd, int nServerPort, struct sockaddr_in remote, char *send_buf)
{
    int iRet = 0;
    struct timeval wait_time = {0};

    fd_set write_fds;
    fd_set error_fds;

    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);

    FD_SET(sockfd, &write_fds); 
    FD_SET(sockfd, &error_fds); 
    wait_time.tv_sec    = 2;
    wait_time.tv_usec   = 0;

    iRet = select(sockfd + 1, NULL, &write_fds, &error_fds, &wait_time);
    if(iRet < 0)
    {
        __ERR("select write failed, error=%d\n", errno);
        return -1;
    }
    else if(iRet == 0)
    {
        __ERR("select write timeout\n");
        return -1;
    }

    if(FD_ISSET(sockfd, &error_fds))
    {
        __ERR("select write sock find socket error\n");
        return -1;
    }

    if(FD_ISSET(sockfd, &write_fds))
    {
        remote.sin_family       = AF_INET;
        remote.sin_addr.s_addr  = htonl(INADDR_BROADCAST);
        remote.sin_port         = htons(nServerPort);

        memset(&(remote.sin_zero), 0, 8);

        __DBG("%s\n", send_buf);
        iRet = sendto(sockfd, (char *)send_buf, strlen(send_buf), 0, (struct sockaddr*)&remote, sizeof(remote));
        if(iRet <= 0)
        {
            __ERR("sendto failed, error=%d, errinfo=%s\n",errno, strerror(errno));

            //try to add route for address 255.255.255.255
            __ERR("try to add route for address 255.255.255.255\n");

            sn_broadcast_route_add();

            SLEEP_SECOND(1);
            return -1;
        }
        else
        {
            //__INFO("send broadcast msg.\n");
            return 0;
        }
    }

    return -1;
}



/*
 *	transfer the 16 hex string to int
 */
int hexStrToInt(const char* buf, unsigned int len, char* out)
{
    unsigned int i = 0,	j = 0;
    for(i = 0, j = 0; i < len; i += 2, ++j)
    {
        out[j] = (char2int(buf[i]) << 4) + char2int(buf[i + 1]);
    }
    return 0;
}

/*
 *	transfer the 16 hex string to int
 */
int hexStrToUInt(const char* buf, unsigned int len, unsigned char* out)
{
    unsigned int i = 0,	j = 0;
    for( i = 0, j = 0; i < len; i += 2, ++j)
    {
        out[j] = (char2int(buf[i]) << 4) + char2int(buf[i + 1]);
    }
    return 0;
}

int hexdataTohexStr(const char* buf, unsigned int len, char* out, unsigned int outbuflen)
{
    if( outbuflen == 0 )
        return -1;

    if( outbuflen <= 2 )
    {
        out[0] = 0;
    }

    if( len >= (outbuflen - 1) / 2 )
        len = (outbuflen - 1) / 2;

    unsigned int iIndex;
    for(iIndex = 0; iIndex < len; iIndex++)
    {
        sprintf(&out[2*iIndex], "%02x", (unsigned char)buf[iIndex]);
    }

    return 0;
}

int hexdataToDebugStr(const char* buf, unsigned int len, char* out, unsigned int outbuflen)
{
	if( outbuflen == 0 )
		return -1;
	
	if( outbuflen <= 3 )
	{
		out[0] = 0;
	}
	
	if( len >= (outbuflen-1)/3 )
		len = (outbuflen-1)/3;

	unsigned int iIndex;
	for(iIndex = 0; iIndex < len; iIndex++)
	{
		sprintf(&out[3*iIndex], "%02x ", (unsigned char)buf[iIndex]);
	}

	return 0;
}

/**
 * @brief 将UTC毫秒时间戳转换为可读字符串（YYYY-MM-DD HH:MM:SS.sss UTC）
 * @param timestamp_ms 输入：UTC毫秒时间戳（1970-01-01 00:00:00至今）
 * @param out_str      输出：存储格式化字符串的缓冲区（建议长度≥30）
 * @param str_len      输入：out_str的缓冲区长度
 * @return 成功返回0，失败返回-1
 */
int format_utc_timestamp(unsigned long long timestamp_ms, char *out_str, size_t str_len)
{
    if (out_str == NULL || str_len < 30) {  // 30字节足够容纳"YYYY-MM-DD HH:MM:SS.sss UTC"
        return -1;
    }

    // 1. 拆分秒级时间戳和毫秒余数
    time_t sec = (time_t)(timestamp_ms / 1000);  // 秒级时间戳
    int ms_part = (int)(timestamp_ms % 1000);    // 毫秒部分（0~999）

    // 2. 转换秒级时间戳为UTC的tm结构体（线程安全）
    struct tm tm_utc;
    memset(&tm_utc, 0, sizeof(struct tm));  // 初始化tm结构体，避免脏数据

    // Linux：gmtime_r返回tm结构体指针（NULL失败）
    struct tm *tm_ptr = gmtime_r(&sec, &tm_utc);
    if (tm_ptr == NULL) {
        return -1;
    }

    // 3. 格式化字符串（YYYY-MM-DD HH:MM:SS.sss UTC）
    int ret = snprintf(out_str, str_len,
                       "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC",
                       tm_utc.tm_year + 1900,  // tm_year是从1900年开始的偏移量
                       tm_utc.tm_mon + 1,      // tm_mon是0~11（对应1~12月）
                       tm_utc.tm_mday,         // 日（1~31）
                       tm_utc.tm_hour,         // 时（0~23，UTC）
                       tm_utc.tm_min,          // 分（0~59）
                       tm_utc.tm_sec,          // 秒（0~60，考虑闰秒）
                       ms_part);               // 毫秒（0~999）

    // 检查格式化是否成功（返回值<0或≥缓冲区长度均失败）
    if (ret < 0 || (size_t)ret >= str_len) {
        return -1;
    }

    return 0;
}


/*
修改/添加/删除UBOOT任意参数,value不为空时如果没有就添加。value为空时删除
return:
	-1: 修改失败
	0:  相同，不需要修改
	1: 不相同，修改成功
*/
int check_ubootargs2(const char *name, const char* value, int print)
{
#if 0
    if( FlashIsSpiNandFlash() != 0 )
    {
        printf("[%s:%d] spinand not support!!!\n", __func__, __LINE__);
        return -1;
    }
#endif

    if( name == NULL || *name == 0)
        return -1;

    char cmd[512] = {0};
    if( value != NULL && *value != 0)
    {
        snprintf(cmd, sizeof(cmd), "%s=%s", name, value);
    }

    int iRet = 0;	
    int size = anj_mw_mtd_size_get(UBOOT_BLOCK_MTD);
    int sn_mtd_size = platform_sn_mtd_size_get();
    if( size <= 0 || ((size % sn_mtd_size) != 0) )
    {
        if(SUPPORT_NAND_FLASH == 0)
        {
            __ERR("nand mtd %s size %d error.\n", UBOOT_BLOCK_MTD, size);
            return -1;
        }
    }

    /*读出一个SECT的数据*/
    unsigned int size_sect = SECTOR_SIZE;
    const char* mtd_block = UBOOT_MTD_BLOCK;
    unsigned int sect_number = platform_uboot_env_no_get();
    unsigned int size_env = SPI_ENV_SIZE;

    char *pSectBuffer = (char *)anj_mw_malloc(size_sect);
    char *pSectBuffer_modify = (char *)anj_mw_malloc(size_sect);
    if( NULL == pSectBuffer || NULL == pSectBuffer_modify)
    {
        __ERR("malloc %u error.\n", size_sect);
        return -1;
    }

    iRet = flash_sect_rw(0, mtd_block, size_sect, sect_number, (unsigned char*)pSectBuffer);
    if (0 != iRet)
    {
        __ERR("sect %d: flash_read error.\n", sect_number);
        iRet = -1;
        goto __exit;
    }

    int kIndex = 0;
    for(kIndex = 4; kIndex < size_sect; kIndex++)
    {
        if( (unsigned char)pSectBuffer[kIndex] == 0xff)
            pSectBuffer[kIndex] = 0;
    }

    memcpy(pSectBuffer_modify, pSectBuffer, size_sect);

    char szTempFind[64] = {0};
    snprintf(szTempFind, sizeof(szTempFind), "%s=", name);

    const char *findstr = szTempFind;
    int pos_from = -1;
    int pos_end = -1;
    int iIndex = 0;

    iRet = -1;
    for( iIndex = 0; iIndex < size_sect; iIndex++)
    {
        char *p = pSectBuffer_modify + iIndex;
        if(strncmp(p, findstr, strlen(findstr)) == 0)
        {
            if( iIndex > 4)
            {
                if( *(p-1) != 0 )   //参数以0分割，如果是其他字符则是其他参数的一部分
                    continue;
            }

            pos_from = iIndex;
            break;
        }
    }

    if( pos_from > 0 )
    {
        for( iIndex = pos_from; iIndex < size_sect; iIndex++)
        {
            char *p = pSectBuffer_modify + iIndex;
            if( *p == 0)
            {
                pos_end = iIndex;
                break;			
            }
        }
    }

    if( pos_from > 0 && pos_end > 0 )
    {
        char bootargs_old[512] = {0};
        int len = pos_end - pos_from;

        if( len < 512 && len > 0 )
        {
            memcpy(bootargs_old, pSectBuffer_modify + pos_from, len);

            if( print > 0 )
            {
                __DBG("old: %s\n", bootargs_old);
            }

            if( strcmp(bootargs_old, cmd) == 0)
            {
                __INFO("old is same with new\n");
                iRet = 0;			
            }
            else
    		{
                __DBG("new cmd: %s\n", cmd);
                strcpy(pSectBuffer_modify + pos_from, cmd);

                char *pOrignalPos = pSectBuffer + pos_end;
                char *pNewPos = pSectBuffer_modify + pos_from + strlen(cmd);
                int nOrignalLeftlen = size_sect - pos_end;
                int nNewLeftlen = size_sect - pos_from - strlen(cmd);
                int copylen = nOrignalLeftlen < nNewLeftlen ? nOrignalLeftlen: nNewLeftlen ;

                if( strlen(cmd) == 0)//删除环境变量的时候，需要往回退一个，否则会连续出现2个0，导致后面的环境变量被忽略了
                    pNewPos -= 1;

                memcpy(pNewPos, pOrignalPos, copylen);

#if 0
                unsigned int crc = GetCrcValue_ex(pSectBuffer_modify+4, size_sect-4, size_env-size_sect);
#else
                unsigned int crc = GetCrcValue(pSectBuffer_modify + 4, size_sect - 4);
                if( size_env > size_sect)
                {
                    char *pTmpBuffer = (char *)anj_mw_malloc(size_sect);
                    if( NULL == pTmpBuffer)
                    {
                        __ERR("malloc %u error.\n", size_sect);
                        iRet = -1;
                        goto __exit;
                    }

                    int sects = size_env / size_sect;
                    int kIndex = 0;
                    for( kIndex = 1; kIndex < sects; kIndex++)
                    {
                        iRet = flash_sect_rw(0, mtd_block, size_sect, sect_number+ kIndex, (unsigned char*)pTmpBuffer);
                        if (0 != iRet)
                        {
                            anj_mw_free(pTmpBuffer);
                            pTmpBuffer = NULL;
                            __ERR("sect %d: flash_read error.\n", sect_number + kIndex);

                            iRet = -1;
                            goto __exit;

                        }
                        crc = GetCrcValueMode2(crc, pTmpBuffer, size_sect);
                    }

                    anj_mw_free(pTmpBuffer);
                    pTmpBuffer = NULL;
                }
#endif
                unsigned int crc_sect = *(unsigned int*)pSectBuffer;
                __DBG("sect current crc=%#x, change to crc=%#x\n", crc_sect, crc);

                unsigned int *p = (unsigned int*)pSectBuffer_modify;
                *p = crc;

                const char* mtd_block = UBOOT_MTD_BLOCK_ENV;
                iRet =  WriteMtdData((unsigned char*)pSectBuffer_modify, size_sect, mtd_block, sect_number, sect_number + 1, sect_number + 1);
                if (0 == iRet)
                {
                    iRet = 1;
                }
            }
        }
    }
    else
    {
        iRet = -1;
        if( pos_from < 0 && pos_end < 0 )
        {
            for( iIndex = 0; iIndex < size_sect; iIndex++)
            {
                char *p = pSectBuffer_modify + iIndex;
                if( *p == 0 && *(p+1) == 0 )
                {
                    pos_from = iIndex + 1;
                    break;
                }
            }

            if( pos_from > 0 )
            {
                strcpy(pSectBuffer_modify+pos_from, cmd);

#if 0
                unsigned int crc = GetCrcValue_ex(pSectBuffer_modify+4, size_sect-4, size_env-size_sect);
#else
                unsigned int crc = GetCrcValue(pSectBuffer_modify + 4, size_sect - 4);
                if( size_env > size_sect)
                {
                    char *pTmpBuffer = (char *)anj_mw_malloc(size_sect);
                    if( NULL == pTmpBuffer)
                    {
                        __ERR("malloc %u error.\n", size_sect);
                        iRet = -1;
                        goto __exit;
                    }

                    int sects = size_env / size_sect;
                    int kIndex = 0;
                    for( kIndex = 1; kIndex < sects; kIndex++)
                    {
                        iRet = flash_sect_rw(0, mtd_block, size_sect, sect_number + kIndex, (unsigned char*)pTmpBuffer);
                        if (0 != iRet)
                        {
                            anj_mw_free(pTmpBuffer);
                            pTmpBuffer = NULL;

                            __ERR("sect %d: flash_read error.\n", sect_number + kIndex);
                            iRet = -1;
                            goto __exit;
                        }
                        crc = GetCrcValueMode2(crc, pTmpBuffer, size_sect);
                    }

                    anj_mw_free(pTmpBuffer);
                    pTmpBuffer = NULL;
                }
#endif

                unsigned int crc_sect = *(unsigned int*)pSectBuffer;
                __INFO("sect current crc=%#x, change to crc=%#x\n", crc_sect, crc);

                unsigned int *p = (unsigned int*)pSectBuffer_modify;
                *p = crc;

                const char* mtd_block = UBOOT_MTD_BLOCK_ENV;
                iRet =  WriteMtdData((unsigned char*)pSectBuffer_modify, size_sect, mtd_block, sect_number, sect_number + 1, sect_number + 1);
                if (0 == iRet)
                {
                    iRet = 1;
                }
            }
    	}
    }

__exit:
    if( NULL != pSectBuffer_modify )
    {
        anj_mw_free(pSectBuffer_modify);
        pSectBuffer_modify = NULL;
    }

    if (pSectBuffer != NULL)
    {
        anj_mw_free(pSectBuffer);
        pSectBuffer = NULL;
    }

    return iRet;
}

/*
修改uboot的bootargs
return:
	-1: 修改失败
	0:  相同，不需要修改
	1: 不相同，修改成功
*/
int check_ubootargs(const char *cmd, int print)
{
    return check_ubootargs2("bootargs", cmd, print);
}

int WriteMtdData(unsigned char *buf, int len, const char* mtd_block, int from_sect, int end_sect, int total_sects)
{
    unsigned int size_sect = SECTOR_SIZE;
    int max_len = (end_sect - from_sect) * size_sect;
    if( len < 0 || len > max_len )
    {
        __ERR("len %d error, max len %d\n",	len, max_len);
        return -1;
    }

    int iRet = 0;

    unsigned char *pSectBuffer = (unsigned char *)anj_mw_malloc(size_sect);
    if( NULL == pSectBuffer)
    {
        __ERR("malloc %u error.\n", size_sect);
        return -1;
    }

    int offset = 0;
    int iIndex = 0;
    for(iIndex = from_sect; iIndex < end_sect && iIndex < total_sects; iIndex++ )
    {
        unsigned int sect_number = iIndex;
        memset(pSectBuffer, 0, size_sect);

        int copylen = size_sect;
        if(offset + size_sect > len)
        {
            copylen = len - offset;
            if(copylen < 0)
                copylen = 0;
        }
        else
        {
            copylen = size_sect;
        }

        if( copylen > 0 )
        {
            memcpy(pSectBuffer, buf + offset, copylen);		
        }

        /*如果不够指定扇区数量,清空SECT的数据*/
        iRet = flash_sect_rw(1, mtd_block, size_sect, sect_number, (unsigned char*)pSectBuffer);
        if (iRet != 0)
        {
            __ERR("sect %d: flash_read error.\n", sect_number);
            iRet = -1;
            goto __exit;
        }

        //__DBG("sect %d: flash_sect_rw OK, offset = %d.\n", sect_number, offset);

        offset += size_sect;
    }	

    iRet = 0;

__exit:
    if (pSectBuffer != NULL)
    {
        anj_mw_free(pSectBuffer);
        pSectBuffer = NULL;
    }

    return iRet;
}

int WriteUboot(char *buf, int len)
{
	if( FlashIsSpiNandFlash() != 0 )
	{
		printf("[%s:%d] spinand not support!!!\n", __func__, __LINE__);
		return -1;
	}

	const char* mtd_block = GET_UBOOT_MTD_BLOCK();
	if( mtd_block == NULL || strlen(mtd_block) == 0)
		return 0;

	int size = anj_mw_mtd_size_get(UBOOT_BLOCK_MTD);
	if (size <= 0 || (size % SECTOR_SIZE) != 0)
	{
		__ERR("uboot mtd %s size %d error.\n", UBOOT_BLOCK_MTD, size);
		return -1;
	}

	/* layout: [uboot ... | SN 4K | ENV 4K], uboot from sect 0 */
	int total_sects = size / SECTOR_SIZE;
	int from_sect = 0;
	int end_sect = total_sects - 2;

	return WriteMtdData((unsigned char*)buf, len, mtd_block, from_sect, end_sect, total_sects);
}

int WriteUboot_byFile(char *param)
{
	int len = 0;
	if(access(param, F_OK) != F_OK )
	{
		__ERR("file %s not found\n", param);
		return -1;
	}

	if( FlashIsSpiNandFlash() > 0 )
	{
		int ret = 0;
		const char *szBkDev = GET_UBOOT_MTD_DEV();
		if( NULL != szBkDev && *szBkDev != 0 )
		{
			ret = FlashNand(param, szBkDev, 0);
		}
		
		szBkDev = GET_UBOOT_MTD_DEV_BK();
		if( NULL != szBkDev && *szBkDev != 0 )
		{
			FlashNand(param, szBkDev, 0);
		}
		
		if(ret != 0)
		{
			__ERR("flash uboot fail\n");		
			return -1;
		}

		return 0;
	}

	
	int buflen = 1024*256;
	int maxlen = 1024*188;
    char platform[32] = {0};
	if (softsn_platform_type_get(platform, sizeof(platform)) == 0 &&
		strcmp(platform, "TS_LINUX_5326") == 0)
	{
		buflen = MAX_UBOOT_SIZE;
		maxlen = MAX_UBOOT_SIZE;
	}
	char *pbuffer = (char*)malloc(buflen);
	if( NULL == pbuffer)
	{
		__ERR("malloc %d not found\n", buflen);
		return -1;
	}
		
	len = read_file_to_buffer(param, pbuffer, buflen);
	if( len > 0 )
	{
		__INFO("read %s length=%d\n", param, len);
	}
	else
	{
		__ERR("read %s length=%d, error\n", param, len);
		free(pbuffer);
		return -1;
	}

	if( len > maxlen)
	{
		__ERR("length=%d, excceed %d KBytes, error\n", len, maxlen);
		free(pbuffer);
		return -1;
	}

	int ret = WriteUboot(pbuffer, len);
	if( ret != 0 )
	{
		__ERR("WriteUboot failed\n");
		free(pbuffer);
		return -1;
	}
	else
	{
		__INFO("WriteUboot OK\n");
	}

	free(pbuffer);
	return 0;
}
