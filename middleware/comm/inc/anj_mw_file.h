#ifndef _ANJ_MW_FILE_H_
#define _ANJ_MW_FILE_H_

#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_FILE_PATH_LEN       128
#define MAX_QUERY_FILE_COUNT    20
typedef struct
{
   char  filepath[MAX_FILE_PATH_LEN];
   unsigned long filesize;
}file_item_info;

typedef struct
{
   int count;
   file_item_info file_info[MAX_QUERY_FILE_COUNT];
}file_query_result;

typedef struct tag_file_entey
{
    char storage_base[48];
    char storage_file[48];
    unsigned long file_length;
    int file_createtime;
    struct tag_file_entey *next;
    struct tag_file_entey *prev;
}file_entry;

size_t anj_mw_fread(FILE* fp, void* data, unsigned long long int bytes);

size_t anj_mw_fwrite(FILE* fp, const void* data, unsigned long long int bytes);

int anj_mw_fseek(FILE* fp, long long int offset);

long long int anj_mw_ftell(FILE* fp);

FILE * anj_mw_fopen(const char *pathname, const char *mode);

int anj_mw_fclose(FILE* fp);

int anj_mw_fflush(FILE* fp);

int anj_mw_file_copy(const char *srcFilePath, const char *destFilePath);

int anj_mw_read_file(const char *filePath, char *pBuffer, unsigned long long int *pSize);

int anj_mw_read_file_len(const char *filePath, unsigned long long int *pLength);

char *anj_mw_read_file_buffer(const char *filePath);

int anj_mw_read_file_limit_len(const char *filePath, char *pBuffer, int iLen);

int anj_mw_file_exists(const char *filePath);

int anj_mw_write_file(const char *filePath, bool append, const char *pFileData, int buflen);

int anj_mw_create_file(const char *filePath, const char *pFileData);

int read_file_to_string(const char *filename, char *content, int len);

int read_file_to_buffer(const char *filename, char *buffer, int buflen);

int write_buffer_to_file(const char *filename, const char *data, int buflen);
int write_file(const char* filename, unsigned char *pBuffer, unsigned int length);

size_t safe_read(int fd, void *ptr, size_t size);
size_t safe_write(int fd, void *ptr, size_t size);

size_t safe_recv(int fd, void *ptr, size_t size, int flags);
size_t safe_send(int fd, void *ptr, size_t size, int flags);

ssize_t safe_recvfrom(int sockfd, void *buf, size_t len, int flags,
                      struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t safe_sendto(int sockfd, const void *buf, size_t len, int flags,
                    const struct sockaddr *dest_addr, socklen_t addrlen);


/*
    向系统文件中写入字符串，写入后关闭文件
*/
int write_sys_file_str(const char *file, char *string);

int anj_mw_mtd_size_get(const char *mtdname);

int is_jpeg_complete(const char *filename);

int wait_for_jpeg_complete(const char *filename, int timeout_ms);

int query_normal_file_in_dir(file_query_result *pFileQueryResult, int skipCount, int pageSize, char *pDir, char *pExtensionName);

unsigned long long get_storage_path_freespace_bytes(const char *path);
int get_file_size(const char *filepath);


#ifdef __cplusplus
}
#endif

#endif
