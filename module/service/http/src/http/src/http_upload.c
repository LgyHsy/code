#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_crypt.h"

#include "anj_config.h"
#include "anj_record.h"
#include "anj_service.h"
#include "user_auth.h"

#include "http_def.h"
#include "http_handle.h"
#include "cgi_handle.h"
#include "webpost_handle.h"
#include "http_upload.h"


#define FormFilePathSize    128
static char s_WebCertificateFilePath[FormFilePathSize] = {0};

static int s_FormFileStatus = 0;
static unsigned char s_FormFileType = 0;
static char s_FormFilePath[FormFilePathSize] = {0};

static const char *s_OemPath = NULL;

static const char * s_base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char s_basearray[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="; 


int http_upload_form_status_get()
{
    return s_FormFileStatus;
}

void http_upload_form_status_set(int status)
{
    s_FormFileStatus = status;
}

/*
    src 源字符串的首地址(buf的地址) 
    separator 指定的分割字符
    dest 接收子字符串的数组
    num 分割后子字符串的个数
*/
void split_str(char *src, const char *separator, char *dest[10], int *num)
{
    int maxnum = 10;
    int i = 0;

    char *pNext = NULL;
    int count = 0;
    if (src == NULL || strlen(src) == 0)                //如果传入的地址为空或长度为0，直接终止 
        return;

    if (separator == NULL || strlen(separator) == 0)    //如未指定分割的字符串，直接终止 
        return;

    pNext = (char *)strtok(src, separator);             //必须使用(char *)进行强制类型转换(虽然不写有的编译器中不会出现指针错误)
    while(pNext != NULL)
    {
        dest[i] = pNext;
        ++count;
        if((++i) >= maxnum)
            break;
        pNext = (char *)strtok(NULL,separator);         //必须使用(char *)进行强制类型转换
    }  

    *num = count;
} 

/* 
*   解码
*   const char * base64 码字
*   unsigned char * dedata， 解码恢复的数据
*/
static int base64_decode_s(const char * base64, unsigned char * dedata)
{
    int i = 0, j = 0;
    int trans[4] = {0,0,0,0};

    for (; base64[i] != '\0'; i += 4)
    {
        // 每四个一组，译码成三个字符
        trans[0] = strFindchrIndex(s_base64char, base64[i]);
        trans[1] = strFindchrIndex(s_base64char, base64[i + 1]);
        // 1/3
        dedata[j++] = ((trans[0] << 2) & 0xfc) | ((trans[1] >> 4) & 0x03);

        if (base64[i + 2] == '=')
        {
            continue;
        }
        else
        {
            trans[2] = strFindchrIndex(s_base64char, base64[i + 2]);
        }
        // 2/3
        dedata[j++] = ((trans[1] << 4) & 0xf0) | ((trans[2] >> 2) & 0x0f);

        if (base64[i + 3] == '=')
        {
            continue;
        }
        else
        {
            trans[3] = strFindchrIndex(s_base64char, base64[i + 3]);
        }

        // 3/3
        dedata[j++] = ((trans[2] << 6) & 0xc0) | (trans[3] & 0x3f);
    }

    dedata[j] = '\0';
    return 0;
}


char *http_base64_encode(const char* data, int data_len) 
{
    int i = 0; 
    int prepare = 0; 
    int ret_len = 0; 
    int temp = 0; 

    char *ret = NULL; 
    char *f = NULL; 
    int tmp = 0; 
    char changed[4] = {0}; 

    ret_len = data_len / 3; 
    temp = data_len % 3; 
    if (temp > 0) 
    { 
        ret_len += 1; 
    } 

    ret_len = ret_len * 4 + 1; 
    ret = (char *)anj_mw_malloc(ret_len); 
    if (ret == NULL) 
    { 
        __ERR("No enough memory.\n"); 
        return NULL;
    } 
    memset(ret, 0, ret_len); 

    f = ret; 
    while (tmp < data_len) 
   { 
        temp = 0; 
        prepare = 0; 
        memset(changed, '\0', 4); 

        while (temp < 3) 
        { 
            //printf("tmp = %d\n", tmp); 
            if (tmp >= data_len) 
            { 
                break; 
            } 
            prepare = ((prepare << 8) | (data[tmp] & 0xFF)); 
            tmp++; 
            temp++; 
        } 

        prepare = (prepare << ((3 - temp) * 8)); 
        //printf("before for : temp = %d, prepare = %d\n", temp, prepare); 
        for (i = 0; i < 4 ;i++ ) 
        { 
            if (temp < i) 
            { 
                changed[i] = 0x40; 
            } 
            else 
            { 
                changed[i] = (prepare >> ((3 - i) * 6)) & 0x3F; 
            } 
                *f = (char)s_basearray[(int)changed[i]]; 
                //printf("%.2X", changed[i]); 
                f++; 
        } 
    } 

    *f = '\0'; 
    return ret; 
}


int http_base64_decode( const char * base64, unsigned char * bindata)
{
    int i = 0, j = 0;
    unsigned char k;
    unsigned char temp[4] = {0};

    for (i = 0, j = 0; base64[i] != '\0' ; i += 4)
    {
        memset(temp, 0xFF, sizeof(temp));
        for (k = 0 ; k < 64 ; k ++)
        {
            if (s_base64char[k] == base64[i])
                temp[0] = k;
        }

        for (k = 0 ; k < 64 ; k ++)
        {
            if (s_base64char[k] == base64[i + 1])
                temp[1] = k;
        }

        for (k = 0 ; k < 64 ; k ++)
        {
            if (s_base64char[k] == base64[i + 2])
                temp[2] = k;
        }
        for (k = 0 ; k < 64 ; k ++)
        {
            if (s_base64char[k] == base64[i + 3] )
                temp[3]= k;
        }

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2)) & 0xFC)) |
                        ((unsigned char)((unsigned char)(temp[1] >> 4) & 0x03));
        if (base64[i + 2] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4)) & 0xF0)) |
                        ((unsigned char)((unsigned char)(temp[2] >> 2) & 0x0F));
        if (base64[i+3] == '=')
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6)) & 0xF0)) |
                         ((unsigned char)(temp[3] & 0x3F));
    }

    return j;
}


/* 
add 20170419 by yajie.wang
END
*/
static int http_get_header_value(char *src, int src_len, char *name, char *value, int max_size)
{
    if((src == NULL) || (name == NULL) || (value == NULL) || (max_size <= 0))
        return -1;

    char *pIndex = strstr(src, name);
    if(pIndex == NULL)
        return -1;

    pIndex += strlen(name);
    int pos = 0, is_start = 0;
    for(pos = 0; (pIndex < (src + src_len)) && (pos < max_size); pIndex++)
    {
        if((*pIndex=='\r') || (*pIndex=='\n') || (*pIndex=='\0'))
        {
            break;
        }

        if(!is_start)
        {
            if((*pIndex == ' ') || (*pIndex == ':'))
            {
                continue;
            }
            is_start=1;    
        }

        *(value+pos) = *pIndex;
        pos++;
    }

    *(value+pos) = '\0';
    return 0;
}

int http_formdata_info_check(char *buf,  int buflen,
                                    char *out_username, char *out_password, char *out_file_md5, 
                                    int len_out_username, int len_out_password, int len_out_file_md5)
{
    if (out_username == NULL || out_password == NULL || out_file_md5 == NULL)
    {
        __ERR("out string is null\n");
        return -1;
    }
    
    char * form_username = "name=\"form_username\"";
    char * form_password = "name=\"form_password\"";
    char * form_md5_str  = "name=\"form_md5_str\"";
    char username[34] = {0};
    char password[34] = {0};
    char md5_str[34] = {0};

    if(out_username != 0)
        *out_username = 0;
    if(out_password != 0)
        *out_password = 0;
    if(out_file_md5 != 0)
        *out_file_md5 = 0;

    int iIndex = 0;
    char *pIndex = buf;
    for(iIndex = 0; iIndex < (buflen - strlen(form_username)); iIndex++, pIndex++)
    {
        if(!strncmp(pIndex,form_username,strlen(form_username)))
        {
            pIndex += strlen(form_username);
            char *pData = strstr(pIndex, "\r\n\r\n");
            if(pData)
            {
                pData += strlen("\r\n\r\n");
                while((*pData!='\r') && (*pData!='\n') && (*pData!='\0') && (strlen(username)<sizeof(username)))
                {
                    username[strlen(username)] = *pData;
                    pData++;
                }
            }
            break;
        }
    }

    for(iIndex = 0, pIndex = buf; iIndex < (buflen - strlen(form_password));iIndex++, pIndex++)
    {
        if(!strncmp(pIndex,form_password, strlen(form_password)))
        {
            pIndex += strlen(form_password);
            char *pData = strstr(pIndex, "\r\n\r\n");
            if(pData)
            {
                pData+=strlen("\r\n\r\n");
                while((*pData!='\r') && (*pData!='\n') && (*pData!='\0') && (strlen(password) < sizeof(password)))
                {
                    password[strlen(password)] = *pData;
                    pData++;
                }
            }
            break;
        }
    }

    for(iIndex = 0, pIndex = buf; iIndex < (buflen - strlen(form_md5_str)); iIndex++, pIndex++)
    {
        if(!strncmp(pIndex, form_md5_str, strlen(form_md5_str)))
        {
            pIndex += strlen(form_md5_str);
            char *pData = strstr(pIndex, "\r\n\r\n");
            if(pData)
            {
                pData += strlen("\r\n\r\n");
                while((*pData!='\r') && (*pData!='\n') && (*pData!='\0') && (strlen(md5_str) < sizeof(md5_str)))
                {
                    md5_str[strlen(md5_str)] = *pData;
                    pData++;
                }
            }
            break;
        }
    }

    if(out_username != 0)
        strncpy(out_username, username, len_out_username - 1);

    if(out_password != 0)
        strncpy(out_password, password, len_out_password - 1);

    if(out_file_md5 != 0)
        strncpy(out_file_md5, md5_str, len_out_file_md5 - 1);

    return 0;
}

int http_get_form_filename_check_freespace(char *pBody, int fileLen)
{
    char *strFileName = strstr(pBody, "filename=\"");
    if(strFileName == NULL)
        return -1;

    char m_filename[100] = {0};
    char *p_oem_mp3 = strstr(pBody, "name=\"oem_mp3\"");//这里有问题，判断不到MP3
    char *p_oem_app = strstr(pBody, "name=\"oem_app\"");
    char *p_oem_logo = strstr(pBody, "name=\"oem_logo\"");
    char *p1 = strFileName + strlen("filename=\"");
    char *p2 = strchr(p1, '"');

    if(p2!=NULL)
    {
        int pos = 0;
        for(pos = 0; (pos < 100) && (pos < (p2 - p1)); pos++)
        {
            m_filename[pos] = *(p1 + pos);
        }
        m_filename[pos] = '\0';
    }

    if(strlen(m_filename) > 0)
    {
        if( strstr(m_filename, ".mp3") != NULL)
        {
            p_oem_mp3 = pBody;
        }

        if(strchr(m_filename,'\\')||strchr(m_filename,'/'))
        {
            char tmp_filename[100] = {0};
            int i_src = 0, i_dst = 0;
            for(i_src=0;i_src<strlen(m_filename);i_src++)
            {
                if((m_filename[i_src] == '\\') || (m_filename[i_src] == '/'))
                    i_dst = 0;
                else
                    tmp_filename[i_dst++] = m_filename[i_src];
            }
            tmp_filename[i_dst] = '\0';
            strcpy(m_filename, tmp_filename);
        }
    }

    const char *dev_oem_mp3_path = get_oem_mp3_path();
    const char *dev_oem_app_path = get_oem_app_path();
    const char *dev_oem_logo_path = get_oem_logo_path();

    if(strlen(m_filename) == 0 ||
        (p_oem_mp3 == NULL && p_oem_app == NULL && p_oem_logo == NULL) ||
        (p_oem_mp3 != NULL && dev_oem_mp3_path == NULL)||
        (p_oem_app != NULL && dev_oem_app_path == NULL)||
        (p_oem_logo != NULL && dev_oem_logo_path == NULL) )
    {        
        return -1;
    }

    char m_path[FormFilePathSize] = {0};

    if(p_oem_mp3 != NULL)
    {
        s_FormFileType = WebForm_MP3;
        s_OemPath = dev_oem_mp3_path;

        __DBG("@@@@@@@@@pOEM_MP3_PATH:%s \n", dev_oem_mp3_path);
        if(0 == strcmp(s_OemPath, "/tmp") || 0 == strcmp(s_OemPath, DATA_BLOCK_MOUNT_PATH ))
        {
            snprintf(s_FormFilePath, FormFilePathSize, "%s/%s", s_OemPath, UPLOAD_MP3_FILE_NAME);
            if(0 == strcmp(s_OemPath, DATA_BLOCK_MOUNT_PATH"/mp3"))
            {
                if(anj_mw_file_exists("/mnt/nand/upload.mp3") == 0)
                {    
                    __ERR("File /mnt/nand/upload.mp3 isn't exist, will upload.\n");
                    if(anj_mw_file_exists("/mnt/nand/mp3/upload.mp3"))
                    {
                        mysystem_with_param("rm /mnt/nand/mp3/upload.mp3");
                    }
                    mysystem_with_param("ln -s %s/upload.mp3 %s/upload.mp3", DATA_BLOCK_MOUNT_PATH, DATA_BLOCK_MOUNT_PATH"/mp3");
                }
                else
                {
                    if(anj_mw_file_exists("/mnt/nand/mp3/upload.mp3") == 0)
                    {
                        mysystem_with_param("ln -s %s/upload.mp3 %s/upload.mp3", DATA_BLOCK_MOUNT_PATH, DATA_BLOCK_MOUNT_PATH"/mp3");
                    }
                }
            }
        }
        else
        {
            snprintf(s_FormFilePath, sizeof(s_FormFilePath), "%s/mp3/%s", s_OemPath, m_filename);

            //mp3子目录不存在
            snprintf(m_path, FormFilePathSize, "%s/mp3", s_OemPath);
            if (access(m_path, F_OK) != 0)
            {
                mysystem_with_param("mkdir %s/mp3", s_OemPath);
            }
        }
        //@@@@@@@@@pOEM_PATH:/tmp/oem, wFormFilePath:/tmp/oem/mp3/no_sound_en.mp3, m_filename:no_sound_en.mp3 
        __ERR("@@@@@@@@@pOEM_PATH:%s, wFormFilePath:%s, m_filename:%s\n", s_OemPath, s_FormFilePath, m_filename);
    }
    else if(p_oem_logo != NULL)
    {
        s_FormFileType = WebForm_LOGO;
        s_OemPath = dev_oem_logo_path;
        snprintf(s_FormFilePath, FormFilePathSize, "%s/%s", s_OemPath, m_filename);
    }
    else
    {
        s_FormFileType = WebForm_APP;
        s_OemPath = dev_oem_app_path;

        if(!strcmp(s_OemPath, "/tmp"))
        {
            snprintf(s_FormFilePath, sizeof(s_FormFilePath), "%s/%s", s_OemPath, m_filename);
        }
        else
        {
            snprintf(s_FormFilePath, FormFilePathSize, "%s/app/%s", s_OemPath, m_filename);

            //app子目录不存在
            snprintf(m_path, sizeof(m_path), "%s/mp3", s_OemPath);
            if (access(m_path, F_OK) != 0)
            {
                mysystem_with_param("mkdir %s/app", s_OemPath);
            }
        }
    }

    __ERR("fileLen=%u, filetype=%d, orignal file:%s\n", fileLen, s_FormFileType, m_filename);

    //判断剩余空间是否够 fileLen
    unsigned int freespace = 0;    
    if(s_OemPath != NULL)
    {
        freespace = (unsigned int)get_storage_path_freespace_bytes(s_OemPath);
    }

    if((unsigned int)(fileLen) > freespace)
    {
        __ERR("filelen = %u > %d for WebForm file!!!\n", fileLen, freespace);
        s_FormFileStatus = -1;
        return -1;
    }

    //如果是nand目录，剩余小于80k,或者单个文件大于20k,不能上传
    if((s_FormFileType == WebForm_MP3) && 
        (dev_oem_mp3_path != NULL) &&
        (!strcmp(dev_oem_mp3_path, DATA_BLOCK_MOUNT_PATH)) )
    {
        if(freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
        {
            __ERR("%s: freespace=%u, less than %u\n", DATA_BLOCK_MOUNT_PATH, freespace, UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE);
            s_FormFileStatus = -1;
            return -1;
        }

        if(fileLen > UPLOAD_MP3_TO_CFG_MTD_MAX_FILE_SIZE)
        {
            __ERR("filesize=%u, large than %u\n", fileLen, UPLOAD_MP3_TO_CFG_MTD_MAX_FILE_SIZE);
            s_FormFileStatus = -1;
            return -1;
        }
    }

    return 0;
}

//新加的获取证书和私钥的文件名
int http_get_form_filename_certificate(char *pBody, int fileLen)
{
    char *strFileName = strstr(pBody, "filename=\"");
    if(strFileName==NULL)
        return -1;

    if(strstr(strFileName, ".crt") != NULL)
    {
        strcpy(s_WebCertificateFilePath, "/mnt/nand/https.crt");
    }
    if(strstr(strFileName, ".key") != NULL)
    {
        strcpy(s_WebCertificateFilePath, "/mnt/nand/https.key");
    }
    if(strstr(strFileName, ".pwd") != NULL)
    {
        strcpy(s_WebCertificateFilePath, "/mnt/nand/https.pwd");
    }
    //__ERR("strFileName = %s, m_filename = %s\n",strFileName,m_filename);

    return 0;
}

#if 0
// //首包:
// 0000   50 4f 53 54 20 2f 49 50 43 5f 46 69 72 6d 77 61   POST /IPC_Firmwa
// 0010   72 65 55 70 67 72 61 64 65 20 48 54 54 50 2f 31   reUpgrade HTTP/1
// 0020   2e 31 0d 0a 48 6f 73 74 3a 20 31 39 32 2e 31 36   .1..Host: 192.16
// 0030   38 2e 31 2e 31 31 30 0d 0a 43 6f 6e 6e 65 63 74   8.1.110..Connect
// 0040   69 6f 6e 3a 20 6b 65 65 70 2d 61 6c 69 76 65 0d   ion: keep-alive.
// 0050   0a 43 6f 6e 74 65 6e 74 2d 4c 65 6e 67 74 68 3a   .Content-Length:
// 0060   20 31 38 31 39 34 34 37 37 0d 0a 43 61 63 68 65    18194477..Cache
// 0070   2d 43 6f 6e 74 72 6f 6c 3a 20 6d 61 78 2d 61 67   -Control: max-ag
// 0080   65 3d 30 0d 0a 55 70 67 72 61 64 65 2d 49 6e 73   e=0..Upgrade-Ins
// 0090   65 63 75 72 65 2d 52 65 71 75 65 73 74 73 3a 20   ecure-Requests: 
// 00a0   31 0d 0a 4f 72 69 67 69 6e 3a 20 68 74 74 70 3a   1..Origin: http:
// 00b0   2f 2f 31 39 32 2e 31 36 38 2e 31 2e 31 31 30 0d   //192.168.1.110.
// 00c0   0a 43 6f 6e 74 65 6e 74 2d 54 79 70 65 3a 20 6d   .Content-Type: m
// 00d0   75 6c 74 69 70 61 72 74 2f 66 6f 72 6d 2d 64 61   ultipart/form-da
// 00e0   74 61 3b 20 62 6f 75 6e 64 61 72 79 3d 2d 2d 2d   ta; boundary=---
// 00f0   2d 57 65 62 4b 69 74 46 6f 72 6d 42 6f 75 6e 64   -WebKitFormBound
// 0100   61 72 79 4f 4d 78 46 51 6e 46 41 75 42 77 65 4a   aryOMxFQnFAuBweJ
// 0110   79 46 55 0d 0a 55 73 65 72 2d 41 67 65 6e 74 3a   yFU..User-Agent:
// 0120   20 4d 6f 7a 69 6c 6c 61 2f 35 2e 30 20 28 57 69    Mozilla/5.0 (Wi
// 0130   6e 64 6f 77 73 20 4e 54 20 31 30 2e 30 3b 20 57   ndows NT 10.0; W
// 0140   69 6e 36 34 3b 20 78 36 34 29 20 41 70 70 6c 65   in64; x64) Apple
// 0150   57 65 62 4b 69 74 2f 35 33 37 2e 33 36 20 28 4b   WebKit/537.36 (K
// 0160   48 54 4d 4c 2c 20 6c 69 6b 65 20 47 65 63 6b 6f   HTML, like Gecko
// 0170   29 20 43 68 72 6f 6d 65 2f 31 31 36 2e 30 2e 30   ) Chrome/116.0.0
// 0180   2e 30 20 53 61 66 61 72 69 2f 35 33 37 2e 33 36   .0 Safari/537.36
// 0190   20 45 64 67 2f 31 31 36 2e 30 2e 31 39 33 38 2e    Edg/116.0.1938.
// 01a0   36 39 0d 0a 41 63 63 65 70 74 3a 20 74 65 78 74   69..Accept: text
// 01b0   2f 68 74 6d 6c 2c 61 70 70 6c 69 63 61 74 69 6f   /html,applicatio
// 01c0   6e 2f 78 68 74 6d 6c 2b 78 6d 6c 2c 61 70 70 6c   n/xhtml+xml,appl
// 01d0   69 63 61 74 69 6f 6e 2f 78 6d 6c 3b 71 3d 30 2e   ication/xml;q=0.
// 01e0   39 2c 69 6d 61 67 65 2f 77 65 62 70 2c 69 6d 61   9,image/webp,ima
// 01f0   67 65 2f 61 70 6e 67 2c 2a 2f 2a 3b 71 3d 30 2e   ge/apng,*/*;q=0.
// 0200   38 2c 61 70 70 6c 69 63 61 74 69 6f 6e 2f 73 69   8,application/si
// 0210   67 6e 65 64 2d 65 78 63 68 61 6e 67 65 3b 76 3d   gned-exchange;v=
// 0220   62 33 3b 71 3d 30 2e 37 0d 0a 52 65 66 65 72 65   b3;q=0.7..Refere
// 0230   72 3a 20 68 74 74 70 3a 2f 2f 31 39 32 2e 31 36   r: http://192.16
// 0240   38 2e 31 2e 31 31 30 2f 0d 0a 41 63 63 65 70 74   8.1.110/..Accept
// 0250   2d 45 6e 63 6f 64 69 6e 67 3a 20 67 7a 69 70 2c   -Encoding: gzip,
// 0260   20 64 65 66 6c 61 74 65 0d 0a 41 63 63 65 70 74    deflate..Accept
// 0270   2d 4c 61 6e 67 75 61 67 65 3a 20 7a 68 2d 43 4e   -Language: zh-CN
// 0280   2c 7a 68 3b 71 3d 30 2e 39 2c 65 6e 3b 71 3d 30   ,zh;q=0.9,en;q=0
// 0290   2e 38 2c 65 6e 2d 47 42 3b 71 3d 30 2e 37 2c 65   .8,en-GB;q=0.7,e
// 02a0   6e 2d 55 53 3b 71 3d 30 2e 36 0d 0a 43 6f 6f 6b   n-US;q=0.6..Cook
// 02b0   69 65 3a 20 44 48 4c 61 6e 67 43 6f 6f 6b 69 65   ie: DHLangCookie
// 02c0   33 30 3d 53 69 6d 70 43 68 69 6e 65 73 65 3b 20   30=SimpChinese; 
// 02d0   69 70 63 5f 31 39 32 2e 31 36 38 2e 31 2e 31 31   ipc_192.168.1.11
// 02e0   30 5f 75 73 65 72 6e 61 6d 65 3d 61 64 6d 69 6e   0_username=admin
// 02f0   3b 20 69 70 63 5f 31 39 32 2e 31 36 38 2e 31 2e   ; ipc_192.168.1.
// 0300   31 31 30 5f 70 61 73 73 77 6f 72 64 3d 45 31 30   110_password=E10
// 0310   41 44 43 33 39 34 39 42 41 35 39 41 42 42 45 35   ADC3949BA59ABBE5
// 0320   36 45 30 35 37 46 32 30 46 38 38 33 45 3b 20 69   6E057F20F883E; i
// 0330   70 63 5f 31 39 32 2e 31 36 38 2e 31 2e 31 31 30   pc_192.168.1.110
// 0340   5f 77 65 62 4c 61 6e 67 75 61 67 65 3d 7a 68 5f   _webLanguage=zh_
// 0350   63 6e 3b 20 69 70 63 5f 31 39 32 2e 31 36 38 2e   cn; ipc_192.168.
// 0360   31 2e 31 31 30 5f 4b 65 65 70 53 63 61 6c 65 3d   1.110_KeepScale=
// 0370   30 0d 0a 0d 0a                                    0....
// 
#endif

#if 0
//第二包
// 0000   2d 2d 2d 2d 2d 2d 57 65 62 4b 69 74 46 6f 72 6d   ------WebKitForm
// 0010   42 6f 75 6e 64 61 72 79 4f 4d 78 46 51 6e 46 41   BoundaryOMxFQnFA
// 0020   75 42 77 65 4a 79 46 55 0d 0a 43 6f 6e 74 65 6e   uBweJyFU..Conten
// 0030   74 2d 44 69 73 70 6f 73 69 74 69 6f 6e 3a 20 66   t-Disposition: f
// 0040   6f 72 6d 2d 64 61 74 61 3b 20 6e 61 6d 65 3d 22   orm-data; name="
// 0050   69 70 63 61 6d 65 72 61 75 70 67 72 61 64 65 22   ipcameraupgrade"
// 0060   3b 20 66 69 6c 65 6e 61 6d 65 3d 22 66 69 72 6d   ; filename="firm
// 0070   77 61 72 65 5f 4d 43 38 30 30 53 5f 56 30 5f 41   ware_MC800S_V0_A
// 0080   46 2d 48 35 5f 56 33 2e 32 2e 34 2e 36 5f 32 30   F-H5_V3.2.4.6_20
// 0090   32 33 30 39 31 32 31 32 30 33 2e 62 69 6e 22 0d   2309121203.bin".
// 00a0   0a 43 6f 6e 74 65 6e 74 2d 54 79 70 65 3a 20 61   .Content-Type: a
// 00b0   70 70 6c 69 63 61 74 69 6f 6e 2f 6f 63 74 65 74   pplication/octet
// 00c0   2d 73 74 72 65 61 6d 0d 0a 0d 0a 41 4e 4a 4f 59   -stream....ANJOY
// 00d0   38 38 38 72 f2 38 a1 34 9f 15 01 03 00 00 00 14   888r.8.4........
// 00e0   06 00 00 20 19 25 00 c6 9f 9e c6 4d 53 54 41 52   ... .%.....MSTAR
// 00f0   5f 49 36 45 5f 4c 49 4e 55 58 5f 33 33 38 00 00   _I6E_LINUX_338..
#endif
#if 0
//尾包
#endif

int http_recv_form_file(void *pInst, 
                            const char *post_path,
                            FormDataBoundary *pFormDataBoundary, 
                            char *md5_str,
                            int socket, 
                            char *buf, 
                            unsigned int buflen, 
                            int *datalen)
{
    const char *file_path = NULL;
    int isFirmwareUpgrade = 0;
    int isWebForm = 0;
    int isCertificate = 0;
    int isConfigForm = 0;
    int isGbCertificate = 0;
    unsigned char postFileType = 0;

    //这里我们必须要把发送数据的文件名读取出来
    //__ERR("1111post_path = %s\n",post_path);

    if(!strcmp(post_path, "POST /IPC_FirmwareUpgrade"))
    {
        isFirmwareUpgrade = 1;
        postFileType = PostFileType_FirmwareUpgrade;
        file_path = "/tmp/FirmwareUpgrade.bin";
    }
    else if(!strcmp(post_path, "POST /PostFile_DANALE_ID"))
    {
        postFileType = PostFileType_DANALE_ID;
        file_path = "/tmp/DanaleId";
    }
    else if(!strcmp(post_path, "POST /PostFile_CUSTOM"))
    {
        postFileType = PostFileType_CUSTOM;
        file_path = "/tmp/CustomFile";
    }
    else if(!strcmp(post_path, "POST /WebUploadFormFile"))
    {
        isWebForm = 1;
        postFileType = PostFileType_WebForm;
        file_path = s_FormFilePath;
    }
#if 1
    else if(!strcmp(post_path, "POST /WebUploadConfig"))
    {    
        isConfigForm = 1;
        postFileType = PostFileType_ConfigForm;
        file_path = "/tmp/uploadConfig.xml";
    }
#endif
    else if(!strcmp(post_path, "POST /WebCertificateUploadFile"))
    {    
        isCertificate = 1;
        postFileType = PostFileType_CertificateForm;
        file_path = s_WebCertificateFilePath;
        //__ERR("WebCertificateUploadFile = %s\n",file_path);
    }    

    int isFileEnd = 0;
    int result = 0;

    char username[64] = {0};
    char password[64] = {0};
    char getPassword[64]={0};
    char md5Password[64]={0};
    char startBoundary[512] = {0};
    char nextBoundary[512] = {0};
    char endBoundary[512] = {0};

    if((pFormDataBoundary->socket == 0) && (*datalen > 0) && (strstr(buf, post_path)))
    {
        __DBG("Read %u bytes from socket=%d\n", (unsigned int)*datalen, (int)socket);
        char *pHeader = strstr(buf, "Content-Type");
        if(pHeader)
        {
            /*提取Content-Type: multipart/form-data; boundary= 后面的分隔符*/
            pHeader += strlen("Content-Type");
            int lenHeader = *datalen - (pHeader - buf);
            int iIndex = 0, iNum = 0, hasFormData = 0, hasBoundary = 0, getBoundary = 0;
            char strBoundary[256] = {0};
            for(iIndex = 0; iIndex < lenHeader; iIndex++, pHeader++)
            {
                if((*pHeader == '\r') || (*pHeader == '\n')||(*pHeader == '\0'))
                {
                    break;
                }

                if((hasFormData == 0) &&
                    (!strncmp(pHeader, "multipart/form-data", strlen("multipart/form-data"))))
                {
                    hasFormData = 1;
                    pHeader += (strlen("multipart/form-data") - 1);
                }
                else if((hasFormData==1) && 
                    (hasBoundary==0) && 
                    (!strncmp(pHeader,"boundary", strlen("boundary"))))
                {
                    hasBoundary = 1;
                    pHeader += (strlen("boundary") - 1);
                }
                else if((hasBoundary == 1)&&(*pHeader == '='))
                {
                    getBoundary = 1;
                }
                else if(getBoundary == 1)
                {
                    if((*pHeader != ' ') && (iNum < sizeof(strBoundary)))
                    {
                        strBoundary[iNum] = *pHeader;
                        iNum++;
                    }
                }
            }

            if(iNum > 0)
            {
                __DBG("strBoundary=%s, len=%d\n",strBoundary, iNum);
                strcpy(pFormDataBoundary->strBoundary, strBoundary);
                pFormDataBoundary->socket = (int)socket;
                pFormDataBoundary->postFileType = postFileType;
                pFormDataBoundary->hasStartBoundary = 0;

                //获取文件大小
                char *pLength = strstr(buf, "Content-Length");
                if(pLength)
                {
                    pLength += strlen("Content-Length");
                    int lenLength = *datalen - (pLength - buf);
                    int jIndex = 0, jNum = 0;
                    //char strLength[16]={0};
                    for(jIndex = 0; jIndex < lenLength; jIndex++, pLength++)
                    {
                        if((*pLength == '\r') || (*pLength == '\n') || (*pLength == '\0'))
                        {
                            break;
                        }

                        if((*pLength >= '0') && (*pLength <= '9'))
                        {
                            //strLength[jNum]=*pLength;
                            jNum++;
                        }
                    }

                    if(jNum > 0)
                    { 
                        //DebugLog("strLength=%s,len=%d\n",strLength,jNum);
                        //pFormDataBoundary->fileSize = atoi(strLength);
                        pFormDataBoundary->recvSize = 0;
                    }
                }

                snprintf(startBoundary, sizeof(startBoundary), "--%s\r\n", pFormDataBoundary->strBoundary);
                snprintf(nextBoundary, sizeof(startBoundary), "\r\n--%s", pFormDataBoundary->strBoundary);
                snprintf(endBoundary, sizeof(startBoundary), "\r\n--%s--\r\n", pFormDataBoundary->strBoundary);

                char *pBody = strstr(buf, startBoundary);
                if(pBody)
                {

                    /*如果不是升级文件，需要校验密码*/
                    if(!isFirmwareUpgrade && !isWebForm && !isCertificate && !isConfigForm)
                    {
                        http_formdata_info_check(buf, *datalen, username, password, pFormDataBoundary->md5_str, 
                                sizeof(username), sizeof(password), sizeof(pFormDataBoundary->md5_str));

                        if(strlen(username) <= 0 || strlen(password) <= 0 || strlen(pFormDataBoundary->md5_str) <= 0)
                        {
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));            
                            return -1;    //错误返回-1
                        }

                        //得到username, password, md5_str
                        __DBG("http form check username:%s, password:%s, md5_str:%s\n", username, password, pFormDataBoundary->md5_str);

                        //unsigned char outmd5[16];
                        result = UserAuthGetPassword(username, getPassword);
                        if(result != 0) //没有该userid
                        {
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));    
                            return -1;
                        }

                        getPassword[33] = '\0';
                        our_md5_encode(md5Password, (const unsigned char *)getPassword, strlen(getPassword));
                        //DebugLog("user %s ,cur password=%s,recv password=%s\n",username,md5Password,password);
                        if(strcasecmp(md5Password, password))
                        {
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                            return -1;    //错误返回-1
                        }
                    }

                    char *strFileName = strstr(pBody, "filename=\"");
                    if(strFileName)
                    {
                        char *strFile = strstr(strFileName, "\r\n\r\n");
                        if(strFile)
                        {
                            strFile += 4;
                            //pFormDataBoundary->fileSize -= strFile-pBody+strlen(endBoundary);
                            __DBG("===>>g_FormDataBoundary.fileSize=%d<<===\n", pFormDataBoundary->fileSize);
                            int fileLen = *datalen - (strFile - buf);

                            /* 如果是升级文件，比较大，不逐字节寻找nextBoundary*/
                            if(isFirmwareUpgrade || isWebForm || isCertificate)
                            {
                                //判断是否包含结束分隔符
                                if(fileLen > strlen(endBoundary))
                                {
                                    char *strEnd = strFile + (fileLen - strlen(endBoundary));
                                    if(!strncmp(strEnd, endBoundary, strlen(endBoundary)))
                                    {
                                        fileLen -= strlen(endBoundary);
                                        pFormDataBoundary->socket = 0;
                                        pFormDataBoundary->postFileType = 0;
                                        memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                                        strcpy(md5_str,pFormDataBoundary->md5_str);
                                        isFileEnd = 1;
                                    }
                                }
                            }
                            else
                            {
                                //找nextBoundary ,文件结束
                                int iIndex = 0;
                                char *pIndex = strFile;
                                if(fileLen > strlen(nextBoundary))
                                {
                                    for(iIndex = 0; iIndex < (fileLen - strlen(nextBoundary)); iIndex++, pIndex++)
                                    {
                                        void *voidIndex = (void *)pIndex;
                                        void *voidNextBoundary = (void *)nextBoundary;
                                        if(!memcmp(voidIndex, voidNextBoundary, strlen(nextBoundary)))
                                        {
                                            fileLen = pIndex - strFile;
                                            pFormDataBoundary->fileSize = fileLen;
                                            __DBG("===>>g_FormDataBoundary.fileSize=%d<<===\n", pFormDataBoundary->fileSize);
                                            pFormDataBoundary->socket = 0;
                                            pFormDataBoundary->postFileType = 0;
                                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                                            strcpy(md5_str,pFormDataBoundary->md5_str);
                                            isFileEnd = 1;
                                            break;
                                        }
                                    }
                                }
                            }

                            /*CHAM 20190730:注意，这里获取到的filelen并不是整个文件大小，而是受到当前包后的文件的大小。用于判断MP3文件超大时判断不到*/
                            if(isWebForm)
                            {
                                //获取文件名, 检查剩余空间
                                if(http_get_form_filename_check_freespace(pBody, fileLen)!=0)
                                {
                                    __ERR("get_form_filename_check_freespace failed!\n");
                                    pFormDataBoundary->socket = 0;
                                    pFormDataBoundary->postFileType = 0;
                                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));        
                                    return -1;    //错误返回-1
                                }
                                __DBG("file_path=%s\n", file_path);
                            }

                            if(isCertificate)
                            {
                                if(http_get_form_filename_certificate(pBody,fileLen)!=0)
                                {
                                    __ERR("http_get_form_filename_Certificate failed!\n");
                                    pFormDataBoundary->socket = 0;
                                    pFormDataBoundary->postFileType = 0;
                                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));        
                                    return -1;    //错误返回-1                                    
                                }
                            }

                            //开始把固件文件写到/tmp中
                            if(0 && pFormDataBoundary->fileSize > 5 * 1024 * 1024)
                            {
                                __INFO("try malloc mma, size = %d\n", pFormDataBoundary->fileSize);
                            }
                            else
                            {
                                FILE *fFirm = anj_mw_fopen(file_path, "wb");
                                size_t wLen = anj_mw_fwrite(fFirm, (const void *)strFile, fileLen);
                                if (wLen != fileLen)
                                {
                                    __ERR("fwrite fail, wLen=%d\n", wLen);
                                }
                                anj_mw_fclose(fFirm);
                            }
                            pFormDataBoundary->recvSize += fileLen;
                            pFormDataBoundary->hasStartBoundary = 1;
                            __DBG("fileSize=%d,recvSize=%d\n", pFormDataBoundary->fileSize, pFormDataBoundary->recvSize);

                            if(isFileEnd)
                            {
                                http_response_cb(pInst, "", 200);

                                //DebugLog("recv file(%s) succeed!\n",file_path);
                                if(!isFirmwareUpgrade)
                                {
                                    memset(pFormDataBoundary, 0, sizeof(FormDataBoundary));                                        
                                }
                            }
                        }
                    }
                }

                memset(buf, 0, buflen);
                *datalen = 0;
                return isFileEnd;
            }
        }
    }
    else if((pFormDataBoundary->socket > 0) && 
            (pFormDataBoundary->socket == (int)socket) &&
            (pFormDataBoundary->postFileType > 0) && 
            (pFormDataBoundary->postFileType == postFileType))
   {

        snprintf(startBoundary, sizeof(startBoundary), "--%s\r\n", pFormDataBoundary->strBoundary);
        snprintf(nextBoundary, sizeof(startBoundary), "\r\n--%s", pFormDataBoundary->strBoundary);
        snprintf(endBoundary, sizeof(startBoundary), "\r\n--%s--\r\n", pFormDataBoundary->strBoundary);

        //开始分隔符,开始接收文件内容
        if(!strncmp(buf, startBoundary, strlen(startBoundary)))
        {
            /*如果不是升级文件，需要校验密码*/
            if(!isFirmwareUpgrade && !isWebForm && !isCertificate && !isConfigForm && !isGbCertificate)
            {
                http_formdata_info_check(buf, *datalen, username, password, pFormDataBoundary->md5_str, 
                    sizeof(username), sizeof(password), sizeof(pFormDataBoundary->md5_str));

                if(strlen(username) <= 0 || strlen(password) <= 0 || strlen(pFormDataBoundary->md5_str) <= 0)
                {
                    pFormDataBoundary->socket = 0;
                    pFormDataBoundary->postFileType = 0;
                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));            
                    return -1;    //错误返回-1
                }

                //得到username, password, md5_str
                __DBG("username=%s, password=%s, md5_str=%s\n", username, password, pFormDataBoundary->md5_str);

                result = UserAuthGetPassword(username, getPassword);
                if(result != 0) //没有该userid
                {
                    pFormDataBoundary->socket = 0;
                    pFormDataBoundary->postFileType = 0;
                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));            
                    return -1;
                }

                getPassword[33]='\0';
                our_md5_encode(md5Password, (const unsigned char *)getPassword, strlen(getPassword));
                //DebugLog("user %s ,cur password=%s,recv password=%s\n",username,md5Password,password);
                if(strcasecmp(md5Password,password))    //错误返回-1
                {
                    pFormDataBoundary->socket = 0;
                    pFormDataBoundary->postFileType = 0;
                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));            
                    return -1;
                }

            }

            char *strFileName = strstr(buf, "filename=\"");
            if(strFileName)
            {
                char *strFile = strstr(strFileName, "\r\n\r\n");
                if(strFile)
                {
                    strFile += 4;
                    //pFormDataBoundary->fileSize -= strFile-buf+strlen(endBoundary);
                    //DebugLog("===>>g_FormDataBoundary.fileSize=%d<<===\n",pFormDataBoundary->fileSize);
                    int fileLen = *datalen - (strFile - buf);

                    /* 如果是升级文件，比较大，不逐字节寻找nextBoundary*/
                    if(isFirmwareUpgrade || isWebForm || isCertificate || isConfigForm || isGbCertificate)
                    {
                        //判断是否包含结束分隔符
                        if(fileLen > strlen(endBoundary))
                        {
                            char *strEnd = strFile + (fileLen - strlen(endBoundary));
                            if(!strncmp(strEnd, endBoundary, strlen(endBoundary)))
                            {
                                fileLen -= strlen(endBoundary);
                                pFormDataBoundary->socket = 0;
                                pFormDataBoundary->postFileType = 0;
                                memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                                strcpy(md5_str,pFormDataBoundary->md5_str);
                                isFileEnd = 1;
                            }
                        }
                    }
                    else
                    {
                        //找nextBoundary ,文件结束
                        int iIndex = 0;
                        char *pIndex = strFile;
                        if(fileLen > strlen(nextBoundary))
                        {
                            for(iIndex = 0; iIndex < (fileLen - strlen(nextBoundary)); iIndex++, pIndex++)
                            {
                                void *voidIndex = (void *)pIndex;
                                void *voidNextBoundary = (void *)nextBoundary;
                                if(!memcmp(voidIndex, voidNextBoundary, strlen(nextBoundary)))
                                {
                                    fileLen = pIndex - strFile;
                                    pFormDataBoundary->fileSize = fileLen;
                                    __DBG("===>>g_FormDataBoundary.fileSize=%d<<===\n", pFormDataBoundary->fileSize);
                                    pFormDataBoundary->socket = 0;
                                    pFormDataBoundary->postFileType = 0;
                                    memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                                    strcpy(md5_str, pFormDataBoundary->md5_str);
                                    isFileEnd = 1;
                                    break;
                                }
                            }
                        }
                    }

                    if(isWebForm)
                    {
                        //获取文件名, 检查剩余空间
                        if(http_get_form_filename_check_freespace(buf, fileLen) != 0)
                        {
                            __ERR("http_get_form_filename_check_freespace failed!\n");
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));        
                            return -1;     //错误返回-1
                        }
                        __DBG("file_path=%s\n", file_path);
                    }

                    if(isCertificate)
                    {
                        if(http_get_form_filename_certificate(buf, fileLen) != 0)
                        {
                            __DBG("http_get_form_filename_certificate failed!\n");
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));        
                            return -1;     //错误返回-1                                    
                        }
                    }


                    //开始把固件文件写到/tmp中
                    if(0 && pFormDataBoundary->fileSize > 5* 1024 * 1024)
                    {
                        __INFO("try malloc mma, size = %d\n", pFormDataBoundary->fileSize);
                    }
                    else
                    {
                        FILE *fFirm = anj_mw_fopen(file_path, "wb");
                        size_t wLen = anj_mw_fwrite(fFirm, (const void *)strFile, fileLen);
                        if (wLen != fileLen)
                        {
                            __ERR("fwrite fail, wLen=%d\n", wLen);
                        }
                        anj_mw_fclose(fFirm);
                    }

                    pFormDataBoundary->recvSize += fileLen;
                    pFormDataBoundary->hasStartBoundary = 1;
                    __DBG("fileSize = %d, recvSize = %d\n", pFormDataBoundary->fileSize, pFormDataBoundary->recvSize);
                    if(isFileEnd)
                    {
                        http_response_cb(pInst, "", 200);
                        //DebugLog("recv file(%s) succeed!\n",file_path);
                        if(!isFirmwareUpgrade)
                        {
                            memset(pFormDataBoundary, 0, sizeof(FormDataBoundary));
                        }
                    }
                }
            }
        }
        else
        {
            char *strFile = buf;
            if(strFile)
            {
                int fileLen = *datalen;

                /* 如果是升级文件，比较大，不逐字节寻找nextBoundary*/
                if(isFirmwareUpgrade || isWebForm || isCertificate || isConfigForm || isGbCertificate)
                {
                    //判断是否包含结束分隔符
                    if(fileLen>strlen(endBoundary))
                    {
                        char *strEnd = strFile + (fileLen - strlen(endBoundary));
                        if(!strncmp(strEnd, endBoundary, strlen(endBoundary)))
                        {
                            fileLen -= strlen(endBoundary);
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                            strcpy(md5_str, pFormDataBoundary->md5_str);
                            isFileEnd = 1;
                        }
                    }
                }
                else
                {
                    //找nextBoundary ,文件结束
                    int iIndex = 0;
                    char *pIndex = strFile;
                    if(fileLen > strlen(nextBoundary))
                    {
                        for(iIndex = 0; iIndex < (fileLen - strlen(nextBoundary)); iIndex++, pIndex++)
                        {
                            void *voidIndex = (void *)pIndex;
                            void *voidNextBoundary = (void *)nextBoundary;
                            if(!memcmp(voidIndex, voidNextBoundary, strlen(nextBoundary)))
                            {
                                fileLen = pIndex - strFile;
                                pFormDataBoundary->fileSize = fileLen;
                                __DBG("===>>g_FormDataBoundary.fileSize=%d<<===\n", pFormDataBoundary->fileSize);
                                pFormDataBoundary->socket = 0;
                                pFormDataBoundary->postFileType = 0;
                                memset(pFormDataBoundary->strBoundary, 0, sizeof(pFormDataBoundary->strBoundary));
                                strcpy(md5_str, pFormDataBoundary->md5_str);
                                isFileEnd = 1;
                                break;
                            }
                        }
                    }
                }

                if(pFormDataBoundary->hasStartBoundary > 0)
                {
                    if(isWebForm)
                    {
                        //判断剩余空间是否够 fileLen
                        unsigned long long freespace = get_storage_path_freespace_bytes((char *)s_OemPath);
                        int freespace_mb = (int)(freespace >> 20);
                        if((fileLen / (1024*1024)) > freespace_mb)
                        {
                            __ERR("filelen = %u > %d for WebForm file!!!\n", fileLen, freespace_mb);
                            pFormDataBoundary->socket = 0;
                            pFormDataBoundary->postFileType = 0;
                            memset(pFormDataBoundary->strBoundary,0,sizeof(pFormDataBoundary->strBoundary));        
                            return -1;     //错误返回-1
                        }
                    }

                    //开始把固件文件写到/tmp中
                    if((0) && pFormDataBoundary->fileSize > 5 * 1024 * 1024)
                    {
                        //////////分配一块共享内存，存储升级文件//////////////
                        pFormDataBoundary->recvSize += fileLen;    
                    }
                    else
                    {
                        FILE *fFirm = anj_mw_fopen(file_path, "ab");
                        size_t wLen = anj_mw_fwrite(fFirm, (const void *)strFile, fileLen);
                        anj_mw_fclose(fFirm);
                        pFormDataBoundary->recvSize += wLen;
                    }

                    __DBG("fileSize=%d, recvSize=%d\n",pFormDataBoundary->fileSize, pFormDataBoundary->recvSize);
                }

                if(isFileEnd)
                {
                    http_response_cb(pInst, "", 200);

                    //DebugLog("recv file(%s) succeed!\n",file_path);
                    if(!isFirmwareUpgrade)
                    {
                        memset(pFormDataBoundary, 0, sizeof(FormDataBoundary));                                    
                    }
                }
            }
        }

        memset(buf, 0, buflen);
        *datalen = 0;
        return isFileEnd;
    }

    return -1;        
}


//-----------------------------------------------------//
int http_recv_post_file(void *pInst, 
                    const char *post_path, 
                    HttpPostFileInfo *pPostFileInfo,
                    const char *file_path,
                    int socket, 
                    char *buf, 
                    unsigned int buflen, 
                    int *datalen)
{
/**
    HTTP POST提交文件时如果文件过大，会分成多个小包，多次进入recv_post_file()函数
    第一次进入recv_post_file()函数,需要提取文件信息 
*/

    //DebugLog("pPostFileInfo->socket(%d),*datalen=%d,buf=%s,post_path=%s\n",pPostFileInfo->socket,*datalen,buf,post_path);
    //这里如果开启onvif认证的时候，才会进行身份认证

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    int onvif_auth_enable = pstMediaStreamCfg->webConfig.onvif_auth;

    if(onvif_auth_enable)
    {
        unsigned char Basic_authentication[64] = {0};
        unsigned char dec_Basic_authentication[64] = {0};
        unsigned char username_authentication[40] = {0};
        unsigned char password_authentication[40] = {0};
        char buf[256] = {0};     //用于存储xml的内容
        char *revbuf[10] = {0};

        if((pPostFileInfo->socket == 0) && (*datalen > 0) && (strstr(buf, post_path)))
        {
            //得到基本认证的信息
            http_get_header_value(buf, *datalen, "Basic ", (char *)Basic_authentication, sizeof(Basic_authentication));

            //要对隐藏信息进行解析
            base64_decode_s((char *)Basic_authentication, dec_Basic_authentication);
            __DBG("base64_decode: dec_Basic_authentication:%s\n", dec_Basic_authentication);

            //分割后子字符串的个数
            int num = 0, i = 0;
            split_str((char *)dec_Basic_authentication, ":", revbuf, &num); 

            //把字符串分割成用户名和密码
            if(num < 2)
            {
                username_authentication[0] = 0;
                password_authentication[0] = 0;
            }
            else if(revbuf[0] != NULL && revbuf[1] != NULL)
            {
                snprintf((char *)username_authentication, sizeof(username_authentication), "%s", revbuf[0]);
                snprintf((char *)password_authentication, sizeof(password_authentication), "%s", revbuf[1]);
            }

            SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
            UserConfig *pstUsrCfg = &pstSystemCfg->userCfg;

            for(i = 0; i < pstUsrCfg->count && i < MAX_ACCOUNT_COUNT; i++)
            {
                __DBG("usr name:%s, password = %s\n", pstUsrCfg->accounts[i].userName, pstUsrCfg->accounts[i].password);
                if((strcmp((char *)(pstUsrCfg->accounts[i].userName), (char *)username_authentication) == 0) && 
                    (strcmp((char *)(pstUsrCfg->accounts[i].password), (char *)password_authentication) == 0) )
                {
                    __DBG("authentication success!!!!\n");
                    break;
                }

                //检查完所有的用户和密码都没有一个能够匹配
                if(i == (pstUsrCfg->count - 1))
                {  
                    __DBG("authentication failed!!!!\n");
                    sprintf(buf, XML_CGI_FAULT, "authentication", "Authentication failed");
                    http_response_cb(pInst, buf, 200);
                    return -1;
                }
            }
        }
    }

    if((pPostFileInfo->socket == 0) && (*datalen > 0) && (strstr(buf, post_path)))  // 第一次进入
    {
        //__DBG("[2]Read %u bytes from socket=%d\n", (unsigned int)*datalen, (int)socket);
        char content_type[64] = {0};
        char content_length[16] = {0};
        long file_size = 0;

        /*提取Content-Type: 后面的值，判断是否是application/octetstream */
        http_get_header_value(buf, *datalen, "Content-Type", content_type, sizeof(content_type));

        if((strncmp(content_type, "application/octet-stream", strlen("application/octet-stream")) != 0) && 
            (strncmp(content_type, "application/octetstream", strlen("application/octetstream")) != 0))
        {
            __ERR("Content-Type(%s) is not application/octet-stream\n", content_type);
            return -1;
        }

        /*提取文件大小*/
        http_get_header_value(buf, *datalen, "Content-Length", content_length, sizeof(content_length));
        if(strlen(content_length) > 0)
        {
            file_size = atoi(content_length);
            if(file_size < 0)
            {
                file_size = 0;
            }
            pPostFileInfo->fileSize = (unsigned long)file_size;
            pPostFileInfo->recvSize = 0;
            pPostFileInfo->socket = (int)socket;
            __DBG("Content-Length(%d)\n", file_size);
        }

        //http header和http body的分隔
        char *strFile = strstr(buf, "\r\n\r\n");
        //如果没有分隔符,代表附件内容会在下次进入recv_post_file()时才开始
        if(strFile == NULL)
        {
            return 0;
        }

        /*开始接收文件*/
        strFile += 4;
        int isFileEnd = 0;
        int fileLen = *datalen - (strFile-buf);
        //开始把固件文件写到/tmp中
        FILE *fFirm = anj_mw_fopen(file_path, "wb");
        size_t wLen = anj_mw_fwrite(fFirm, (const void *)strFile, fileLen);
        anj_mw_fclose(fFirm);

        pPostFileInfo->recvSize += wLen;
        if(pPostFileInfo->recvSize >= pPostFileInfo->fileSize)
        {
            isFileEnd = 1;
        }
        //DebugLog("fileSize=%d,recvSize=%d\n",pPostFileInfo->fileSize,pPostFileInfo->recvSize);

        if(isFileEnd)
        {
            http_response_cb(pInst, buf, 200);
            //soap_send_empty_response(soap,200);
            //DebugLog("recv file(%s) succeed!\n",file_path);
            memset(pPostFileInfo, 0, sizeof(HttpPostFileInfo));                        
        }
        memset(buf, 0, buflen);
        *datalen = 0;
        return isFileEnd;
    }
    else if((pPostFileInfo->socket > 0) && (pPostFileInfo->socket == (int)socket))  /* 多次进入recv_post_file()函数,保存附件内容 */
    {
        /*接收文件*/
        int isFileEnd = 0;    
        char *strFile = buf;
        int fileLen = *datalen;

        //开始把固件文件写到/tmp中
        FILE *fFirm = anj_mw_fopen(file_path, "ab");
        size_t wLen = anj_mw_fwrite(fFirm, (const void *)strFile, fileLen);
        anj_mw_fclose(fFirm);

        pPostFileInfo->recvSize += wLen;
        if(pPostFileInfo->recvSize >= pPostFileInfo->fileSize)
        {
            isFileEnd = 1;
        }
        //DebugLog("fileSize=%d,recvSize=%d\n",pPostFileInfo->fileSize,pPostFileInfo->recvSize);

        if(isFileEnd)
        {
            http_response_cb(pInst, buf, 200);
            memset(pPostFileInfo, 0, sizeof(HttpPostFileInfo));
        }
        memset(buf, 0, buflen);
        *datalen = 0;
        return isFileEnd;
    }

    return -1;
}



