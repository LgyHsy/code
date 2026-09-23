#include <errno.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/types.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_log.h"
#include "anj_mw_errcode.h"

/* Read NMEMB elements of SIZE bytes into PTR from STREAM.  Returns the
 * number of elements read, and a short count if an eof or non-interrupt
 * error is encountered.  */
static size_t safe_fread(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t ret = 0;

    do
    {
        clearerr(stream);
        ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
        // if (ret != nmemb)
        // {
        //     __ERR("write %d != %d,errno:%d,%d\n", ret, nmemb, ferror(stream), errno == EINTR);
        // }
    } while (ret < nmemb && ferror(stream) && errno == EINTR);

    return ret;
}

/* Write NMEMB elements of SIZE bytes from PTR to STREAM.  Returns the
 * number of elements written, and a short count if an eof or non-interrupt
 * error is encountered.  */
static size_t safe_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t ret = 0;

    do
    {
        clearerr(stream);
        ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
        if (ret != nmemb)
        {
            __ERR("write %d != %d,errno:%d,%d\n", ret, nmemb, ferror(stream), errno == EINTR);
        }
    } while (ret < nmemb && ferror(stream) && errno == EINTR);

    return ret;
}

size_t anj_mw_fread(FILE *fp, void *data, unsigned long long int bytes)
{
    // return anj_mw_fread(data, 1, bytes, fp);
    return safe_fread(data, 1, bytes, fp);
}

size_t anj_mw_fwrite(FILE *fp, const void *data, unsigned long long int bytes)
{
    // return fwrite(data, 1, bytes, fp);
    return safe_fwrite(data, 1, bytes, fp);
}

int anj_mw_fseek(FILE *fp, long long int offset)
{
    return fseek(fp, offset, offset >= 0 ? SEEK_SET : SEEK_END);
}

long long int anj_mw_ftell(FILE *fp)
{
    return ftell(fp);
}

FILE *anj_mw_fopen(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

int anj_mw_fclose(FILE *fp)
{
    return fclose(fp);
}

int anj_mw_fflush(FILE *fp)
{
    return fflush(fp);
}

int anj_mw_file_copy(const char *srcFilePath, const char *destFilePath)
{
    __INFO("%s\n", srcFilePath);
    FILE *dstfp = NULL;
    FILE *srcfp = NULL;
    int iRet = 0;

    unsigned char *iSrcBuf = NULL;
    int iBufLen = 0;
    int iSrcRWlen = 0;

    if (srcFilePath == NULL || destFilePath == NULL)
    {
        return -1;
    }

    iBufLen = 256 * 1024;
    iSrcBuf = (unsigned char *)malloc(iBufLen);
    if (NULL == iSrcBuf)
    {
        __ERR("Malloc failed:%d\n", iBufLen);
        iRet = ANJ_ERR_BUF_MALLOC;
        goto endFunc;
    }

    srcfp = anj_mw_fopen(srcFilePath, "rb");
    if (NULL == srcfp)
    {
        __ERR("%s open failed\n", srcFilePath);
        iRet = ANJ_ERR_FILE_OPEN;
        goto endFunc;
    }

    dstfp = anj_mw_fopen(destFilePath, "wb+");

    if (NULL == dstfp)
    {
        __ERR("%s open failed,del check\n", destFilePath);
        iRet = ANJ_ERR_FILE_OPEN;
        goto endFunc;
    }

    do
    {
        iSrcRWlen = anj_mw_fread(srcfp, iSrcBuf, iBufLen);
        if (iSrcRWlen > 0)
        {
            anj_mw_fwrite(dstfp, iSrcBuf, iSrcRWlen);
        }
    } while (iSrcRWlen > 0);

    iRet = 0;

endFunc:
    if (srcfp)
    {
        anj_mw_fclose(srcfp);
    }

    if (dstfp)
    {
        anj_mw_fclose(dstfp);
    }

    if (iSrcBuf)
    {
        free(iSrcBuf);
        iSrcBuf = NULL;
    }
    return iRet;
}

int anj_mw_read_file(const char *filePath, char *pBuffer, unsigned long long int *pSize)
{
    int iRet = 0;
    FILE *fp = NULL;
    unsigned long long int fileLen = 0;
    ANJ_CHK(((filePath != NULL) && (pSize != NULL)), -1, "input Invalid");

    fp = anj_mw_fopen(filePath, "rb");
    if (fp == NULL)
    {
        __INFO("open file:%s failed\n", filePath);
        iRet = -1;
        goto endFunc;
    }

    // Get the size of the file
    fseek(fp, 0, SEEK_END);
    fileLen = ftell(fp);
    if (fileLen == 0 || fileLen == (unsigned long long int)-1)
    {
        __ERR("Invalid file length: %llu for file: %s\n", fileLen, filePath);
        iRet = -1;
        goto endFunc;
    }

    if (pBuffer == NULL)
    {
        // requested the length - set and early return
        *pSize = fileLen;
        goto endFunc;
    }
    else
    {
        // Validate the buffer size
        ANJ_CHK((fileLen <= *pSize), -1, "Size < readlen");

        // Read the file into memory buffer
        fseek(fp, 0, SEEK_SET);
        ANJ_CHK((anj_mw_fread(fp, pBuffer, fileLen) == fileLen), -1, "read file err");
    }

endFunc:
    if (fp != NULL)
    {
        anj_mw_fclose(fp);
        fp = NULL;
    }

    return iRet;
}

int anj_mw_read_file_len(const char *filePath, unsigned long long int *pLength)
{
    return anj_mw_read_file(filePath, NULL, pLength);
}

char *anj_mw_read_file_buffer(const char *filePath)
{
    if (filePath == NULL)
    {
        __ERR("input invalid!\n");
        return NULL;
    }

    unsigned long long int len = 0;
    int iRet = anj_mw_read_file_len(filePath, &len);
    if (iRet != 0 || len == 0)
    {
        __ERR("read %s failed!\n", filePath);
        return NULL;
    }

    char *buffer = (char *)anj_mw_malloc(len + 1);
    if (buffer == NULL)
    {
        __ERR("malloc invalid!\n");
        return NULL;
    }

    memset(buffer, 0, len + 1);
    iRet = anj_mw_read_file(filePath, buffer, &len);
    if (iRet != 0)
    {
        __ERR("read %s failed!\n", filePath);
        anj_mw_free(buffer);
        return NULL;
    }
    buffer[len] = '\0';
    return buffer;
}

int anj_mw_read_file_limit_len(const char *filePath, char *pBuffer, int ilen)
{
    int iRet = 0;
    FILE *fp = NULL;
    ANJ_CHK((filePath != NULL && pBuffer != NULL && ilen > 0), -1, "input Invalid");

    unsigned long long int tmplen = (unsigned long long int)(ilen);

    fp = fopen(filePath, "r");
    if (fp == NULL)
    {
        __INFO("open %s failed\n", filePath);
        iRet = -1;
        goto endFunc;
    }
    // Read the file into memory buffer
    fseek(fp, 0, SEEK_SET);
    iRet = anj_mw_fread(fp, pBuffer, tmplen);
    if (iRet <= 0)
    {
        __ERR("read %s empty or failed\n", filePath);
        iRet = -1;
    }

endFunc:
    if (fp != NULL)
    {
        fclose(fp);
        fp = NULL;
    }

    return iRet;
}

int anj_mw_file_exists(const char *filePath)
{
    int iRet = 0;
    if (filePath)
    {
        struct stat st;
        bool result = stat(filePath, &st);
        iRet = (result == 0) ? 1 : 0;
    }
    return iRet;
}

int anj_mw_write_file(const char *filePath, bool append, const char *pFileData, int buflen)
{
    int iRet = -1;
    FILE *fp = NULL;

    ANJ_CHK((filePath != NULL && pFileData != NULL), -1, "input null");
    ANJ_CHK((buflen > 0), -1, "buflen err");

    fp = anj_mw_fopen(filePath, append ? "ab+" : "wb+");

    ANJ_CHK(fp != NULL, -1, "input null");

    int writeLen = (int)anj_mw_fwrite(fp, pFileData, (unsigned long long int)buflen);
    if (writeLen != buflen)
    {
        __ERR("write file %s failed, writeLen %d, buflen %d, error %d(%s)\n", 
            filePath, writeLen, buflen, errno, strerror(errno));
        iRet = -1;
        goto endFunc;
    }

    if (anj_mw_fflush(fp) != 0)
    {
        __ERR("flush file %s failed, error %d(%s)\n", 
            filePath, errno, strerror(errno));
        iRet = -1;
        goto endFunc;
    }

    iRet = writeLen;

endFunc:
    if (fp != NULL)
    {
        anj_mw_fclose(fp);
        fp = NULL;
    }

    return iRet;
}

int anj_mw_create_file(const char *filePath, const char *pFileData)
{
    int iRet = -1;
    if (pFileData)
    {
        iRet = anj_mw_write_file(filePath, 0, pFileData, strlen(pFileData));
    }
    else
    {
        iRet = anj_mw_write_file(filePath, 0, "1", 1);
    }

    return iRet;
}

int read_file_to_string(const char *filename, char *content, int len)
{
    if (filename == NULL || content == NULL || len <= 1)
    {
        return -1;
    }

    FILE *pFile = anj_mw_fopen(filename, "r");
    if (pFile)
    {
        int ret = anj_mw_fread(pFile, content, len - 1);
        anj_mw_fclose(pFile);

        //		__ERR("len %d, content %s", ret, content);

        if (ret > 0)
        {
            content[ret] = '\0'; // 20140213

            return ret;
        }
        else
        {
            //	printf("anj_mw_fread file %s faild, errString is %s\n", filename, strerror(errno));
        }
    }
    else
    {
        //	printf("open file %s faild, errString is %s\n", filename, strerror(errno));
    }

    return -1;
}

int read_file_to_buffer(const char *filename, char *buffer, int buflen)
{
    if (filename == NULL || buffer == NULL || buflen <= 0)
    {
        return -1;
    }

    FILE *pFile = anj_mw_fopen(filename, "r");
    if (pFile)
    {
        int ret = anj_mw_fread(pFile, buffer, buflen);
        anj_mw_fclose(pFile);
        if (ret > 0)
        {
            return ret;
        }
    }

    return -1;
}

int write_buffer_to_file(const char *filename, const char *data, int buflen)
{
    int iRet = anj_mw_write_file(filename, 0, data, buflen);
    if (iRet != buflen)
    {
    }
    else
    {
    }
    return iRet;
}

int write_file(const char *filename, unsigned char *pBuffer, unsigned int length)
{
    int iRet = anj_mw_write_file(filename, 0, (const char *)pBuffer, length);
    if (iRet != length)
    {
        printf("write Error, return %d, error %d(%s)\n", iRet, errno, strerror(errno));
        iRet = -1;
    }
    return iRet;
}

size_t safe_read(int fd, void *ptr, size_t size)
{
    size_t ret = 0;

    do
    {
        ret = read(fd, ptr, size);
    } while (ret <= 0 && ((errno == EINTR) || (errno == EAGAIN)));

    return ret;
}

size_t safe_write(int fd, void *ptr, size_t size)
{
    size_t ret = 0;

    do
    {
        ret = write(fd, ptr, size);
    } while (ret <= 0 && ((errno == EINTR) || (errno == EAGAIN)));

    return ret;
}

size_t safe_recv(int fd, void *ptr, size_t size, int flags)
{
    size_t ret = 0;

    do
    {
        ret = recv(fd, ptr, size, flags);
    } while (ret <= 0 && ((errno == EINTR) || (errno == EAGAIN)));

    return ret;
}

size_t safe_send(int fd, void *ptr, size_t size, int flags)
{
    size_t ret = 0;

    do
    {
        ret = send(fd, ptr, size, flags);
    } while (ret <= 0 && ((errno == EINTR) || (errno == EAGAIN)));

    return ret;
}

ssize_t safe_recvfrom(int sockfd, void *buf, size_t len, int flags,
                      struct sockaddr *src_addr, socklen_t *addrlen)
{
    ssize_t ret;
    do
    {
        ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return ret;
}

ssize_t safe_sendto(int sockfd, const void *buf, size_t len, int flags,
                    const struct sockaddr *dest_addr, socklen_t addrlen)
{
    ssize_t ret;
    do
    {
        ret = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return ret;
}

int write_sys_file_str(const char *file, char *string)
{
    int fd = 0;

    if (NULL == file || NULL == string)
    {
        return -1;
    }

    fd = open(file, O_RDWR);
    if (fd < 0)
    {
        __ERR("open file %s failed!\n", file);
        return -1;
    }

    int len = safe_write(fd, (void *)string, strlen(string));
    if (-1 == len)
    {
        __ERR("write file %s value:%s failed!\n", file, string);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_mtd_size_get(const char *mtdname)
{
    if(NULL == mtdname || *mtdname == 0)
    {
        return 0;
    }

    char buf[64] = {0};
    char pResult[64] = {0};
    snprintf(buf, sizeof(buf), "cat /proc/mtd | grep %s", mtdname);

    ExecShellCmd(buf, pResult, sizeof(pResult));
    if (strlen(pResult) <= 0)
    {
        return -1;
    }

    int flen = strlen(pResult);

    unsigned int size = 0;
    char *p = strstr(pResult, mtdname);
    if( p != NULL)
    {
        p = p + strlen(mtdname) + 1;
        while( anj_isspace(*p) && p < pResult + flen)
        {
            p++;
        }

        char *q = p;

        while( (anj_isalpha(*q) || isdigit(*q)) && q < pResult + flen)
        {
            q++;
        }

        if( q < pResult + flen )
        {
            *q = 0;
        }

        if(sscanf(p, "%x", &size) == 1)
        {
            return size;
        }
    }

    return 0;
}

int is_jpeg_complete(const char *filename)
{
    FILE *fp = anj_mw_fopen(filename, "rb");
    if (!fp)
        return 0;

    // 检查JPEG文件尾标记(0xFFD9)
    anj_mw_fseek(fp, -2);
    unsigned char tail[2] = {0};
    if (anj_mw_fread(fp, tail, sizeof(tail)) != 2)
    {
        anj_mw_fclose(fp);
        return 0;
    }

    fclose(fp);
    return (tail[0] == 0xFF && tail[1] == 0xD9);
}

int wait_for_jpeg_complete(const char *filename, int timeout_ms)
{
    int elapsed = 0;
    const int interval = 100; // 检查间隔(ms)

    while (elapsed < timeout_ms)
    {
        if (anj_mw_file_exists(filename) && is_jpeg_complete(filename))
        {
            return 0;
        }
        usleep(interval * 1000);
        elapsed += interval;
    }
    return -1;
}

unsigned long long get_storage_path_freespace_bytes(const char *path)
{
    if (path == NULL)
    {
        return 0;
    }

    struct statfs diskInfo;	
    if(statfs(path, &diskInfo) >= 0)
    {		
        unsigned long long blocksize = diskInfo.f_bsize;
        unsigned long long totalsize = blocksize * diskInfo.f_blocks;
        unsigned long long freeDisk = diskInfo.f_bfree * blocksize;
        unsigned long long availableDisk = diskInfo.f_bavail * blocksize;
        __ERR("blocksize= %llu, Total=%llu(%llu MB), free=%llu(%llu MB), avail=%llu(%llu MB)",   
            blocksize, totalsize, totalsize >> 20, freeDisk, freeDisk >> 20, availableDisk, availableDisk >> 20);  

        return freeDisk;
    }

    return 0;
}

int get_file_size(const char *filepath)
{
    int offset = 0;
    int fd = open(filepath, O_RDONLY);
    if(fd == -1)
    {
        return 0;
    }

    offset = lseek(fd, 0, SEEK_END);

    close(fd);
    return offset;
}

void file_search_result_free(file_entry *result_head)
{
    file_entry *pCurrent = result_head;
    file_entry *pNext = NULL;
    
    while (pCurrent != NULL)
    {
        pNext = pCurrent->next;
        anj_mw_free(pCurrent);
        pCurrent = pNext;
    }
}

int file_search_result_put(file_entry **result_head, const char *rootpath, const char* filename, unsigned long file_length, long file_createtime)
{
    file_entry *p_result_head = *result_head;
    file_entry *pCompareItem = NULL;
    file_entry *pTail = NULL;
    int insert = 0;

    file_entry *pItem = (file_entry *)anj_mw_malloc(sizeof(file_entry));
    if (pItem == NULL)
    {
        __ERR("malloc file_entry failed.\n");
        return -1;
    }
    else
    {
        memset(pItem, 0, sizeof(file_entry));
    }

    snprintf(pItem->storage_base, sizeof(pItem->storage_base), "%s", rootpath);
    snprintf(pItem->storage_file, sizeof(pItem->storage_file), "%s", filename);
    pItem->next = NULL;
    pItem->prev = NULL;    

    pItem->file_length = file_length;
    pItem->file_createtime = file_createtime;

    if (p_result_head == NULL)
    {
        p_result_head = pItem;
    }
    else
    {
        pCompareItem = p_result_head;

        do
        {
            int compare_len = MIN(strlen(pItem->storage_file), strlen(pCompareItem->storage_file));
            if (strncmp(pItem->storage_file, pCompareItem->storage_file, compare_len) > 0)
            {
                pItem->next = pCompareItem;
                pItem->prev = pCompareItem->prev;

                if (pCompareItem->prev == NULL)
                {
                    __INFO("Insert as head: %s\n", pItem->storage_file);
                    p_result_head = pItem;
                }
                else
                {
                    pCompareItem->prev->next = pItem;
                    __INFO("insert one: %s\n", pItem->storage_file);
                }

                pCompareItem->prev = pItem;

                insert = 1;    
                break;
            }

            pTail = pCompareItem;
            pCompareItem = pCompareItem->next;
        } while (pCompareItem != NULL);

        if (!insert)
        {
            __ERR("not inserted, append to tail.\n");

            pTail->next = pItem;    
            pItem->prev = pTail;
        }
    }

    *result_head = p_result_head;
    return 0;
}

file_entry *query_file_in_dir(const char *pDirPath, const char *pFileExtension)
{
    if (pDirPath == NULL)
    {
        return NULL;
    }

    if (pFileExtension)
    {
        if (strlen(pFileExtension) == 0)
        {
            __ERR("query file in dir:%s but file extension error\n", pDirPath);
            return NULL;
        }
        else
        {
            __INFO("query file in dir:%s file with extension:%s!\n", pDirPath, pFileExtension);
        }
    }
    else
    {
        __INFO("query file in dir:%s but don't have extension!\n", pDirPath);
    }

    struct dirent *s_dir1 = NULL;
    DIR *dir1 = opendir(pDirPath); 
    if (dir1 == NULL)
    {
        __ERR("open dir:%s failed.\n", pDirPath);
        return NULL;
    }

    char file_compare[320] = {0};
    file_entry *p_result_head = NULL;
    int nFileCount = 0;
    struct tm file_local_mtime = {0};

    while (1)
    {
        s_dir1 = readdir(dir1);
        if (s_dir1 == NULL)
        {
            break;
        }

        if (s_dir1->d_name[0] == '.')
        {
            if (!s_dir1->d_name[1] || (s_dir1->d_name[1] == '.' && !s_dir1->d_name[2]))
            {
                continue;
            }
        }

        snprintf(file_compare, sizeof(file_compare), "%s/%s", pDirPath, (char *)s_dir1->d_name);

        struct stat mstat = {0};
        if (lstat(file_compare, &mstat) == -1) 
        {
            continue;
        }

        if (S_ISDIR(mstat.st_mode)) 
        {
            // 跳过目录
        }
        else
        {
            memset(&file_local_mtime, 0, sizeof(struct tm));
            file_local_mtime = *localtime(&mstat.st_ctime);
            time_t t = mktime(&file_local_mtime);

            if (pFileExtension != NULL && strstr(s_dir1->d_name, pFileExtension) == NULL)
            {
                continue;
            }

            if (file_search_result_put(&p_result_head, pDirPath, s_dir1->d_name, mstat.st_size, t) < 0)
            {
                continue;
            }

            nFileCount++;
        }

        if (nFileCount >= MAX_QUERY_FILE_COUNT)
        {
            __ERR("query file cnt:%d exceed max files. break\n", nFileCount);
            break;
        }
    }

    closedir(dir1);
    return p_result_head;
}

int query_normal_file_in_dir(file_query_result *pFileQueryResult, int skipCount, int pageSize, char *pDir, char *pExtensionName)
{
    if (pFileQueryResult == NULL || pDir == NULL)
    {
        __ERR("param error\n");
        return 0;
    }

    file_entry *pfile_result = query_file_in_dir(pDir, pExtensionName);
    if (pfile_result == NULL)
    {
        __ERR("query file result error\n");
        return 0;
    }

    int filecount = 0;
    file_entry *pfile_item = pfile_result;
    while (pfile_item != NULL)
    {
        filecount++;
        
        if (skipCount > 0)
        {
            pfile_item = pfile_item->next;
            skipCount--;
            continue;
        }
                                    
        if ((pageSize <= 0 || pFileQueryResult->count < pageSize) && pFileQueryResult->count < MAX_QUERY_FILE_COUNT)
        {
            if (pFileQueryResult->count >= MAX_QUERY_FILE_COUNT)
            {
                break;
            }

            // 以防 pFileQueryResult->count > 0，所以从count个开始赋值
            int count = pFileQueryResult->count;
            snprintf(pFileQueryResult->file_info[count].filepath, MAX_FILE_PATH_LEN, "%s/%s", pfile_item->storage_base, pfile_item->storage_file);
            pFileQueryResult->file_info[count].filesize = pfile_item->file_length;

            pFileQueryResult->count++;
        }
        
        pfile_item = pfile_item->next;
    }    

    file_search_result_free(pfile_result);
    //__INFO("query file in dir:%s count:%d!\n", pDir, filecount);
    return filecount;
}

int anj_mw_query_dev_is_mount(const char *devName)
{
    int iRet = 0;
    FILE *fp = anj_mw_fopen("/proc/mounts", "rb");
    if(fp == NULL)
    {
        __ERR("open /proc/mounts failed. err=%s\n", strerror(errno)); 
        return 0;
    }

	int flen = 512;
    char *buf = (char *)anj_mw_malloc(flen);
    if(buf == NULL)
    {
        anj_mw_fclose(fp);
        return 0;
    }

    unsigned int file_buf_len = 0;
    file_buf_len = anj_mw_fread(fp, buf, flen);
    anj_mw_fclose(fp);

    if(file_buf_len == 0)
    {
        anj_mw_free(buf);
        return 0;
    }

    buf[file_buf_len] = 0; 			
    if(strstr(buf, devName)!=NULL)
    {
        iRet = 1;
    }
    else
    {
        iRet = 0;
    }

    anj_mw_free(buf);
    buf = 0;

    return iRet;
}


