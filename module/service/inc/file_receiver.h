#ifndef __FILE_RECEIVER_H__
#define __FILE_RECEIVER_H__

#define UPLOAD_CONFIG_FILE_TYPE 0
#define UPLOAD_FIRMWARE_FILE_TYPE 1
#define UPLOAD_OEM_APP_FILE_TYPE 97
#define UPLOAD_OEM_CFG_FILE_TYPE 98
#define UPLOAD_OEM_MP3_FILE_TYPE 99
#define UPLOAD_OEM_LOGO_FILE_TYPE 100
#define UPLOAD_CERTIFICATION_FILE_TYPE 101
#define UPLOAD_KEY_FILE_TYPE 102
#define UPLOAD_CONFIG_XML_FILE_TYPE 103
#define UPLOAD_AF_FIRMWARE_FILE_TYPE 104

typedef struct
{
    FILE *fp;
    char filename[256];
    int filelen;
    int filetype;
    int writelen;
    int lognum;
    unsigned int u32PhyAddr;
    char *pMappedAddr;
    unsigned int timeoutsec;
    anj_thread_s stRecverThread;
} file_recver_t;

typedef enum MSG_SRC_INDEX
{
    MSG_SRC_PRI = 0,
    MSG_SRC_SER,
} MSG_SRC_INDEX;

int file_recver_feed_data(const char *pData, int nLength);
int file_recver_init(char *filename, int filelen, int filetype, int lognum, int MsgSrc);
int file_recver_uninit(int rmfile);
int file_recver_big_init(char *filename, int filelen);
int file_recver_stop(int rmfile);
int file_recver_proc(int dataerror, char *payload, int payloadlen, int MsgSrc);

file_recver_t *getFileRecver();

#endif