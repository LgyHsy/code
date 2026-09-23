#ifndef __FILE_SENDER_H__
#define __FILE_SENDER_H__

#include "anj_mw_thread.h"
#include <sys/time.h>

typedef struct file_sender {
    char filename[256];
    unsigned long startpos;
    int lognum;
    int framesize;
    int interval;
    int sendfinish;
    anj_thread_s stFileThread;
} file_sender_t;

#define UPLOAD_CONFIG_FILE_TYPE		0
#define UPLOAD_FIRMWARE_FILE_TYPE   1
#define UPLOAD_OEM_APP_FILE_TYPE   97
#define UPLOAD_OEM_CFG_FILE_TYPE   98
#define UPLOAD_OEM_MP3_FILE_TYPE   99
#define UPLOAD_OEM_LOGO_FILE_TYPE   100
#define UPLOAD_CERTIFICATION_FILE_TYPE   101
#define UPLOAD_KEY_FILE_TYPE   102
#define UPLOAD_CONFIG_XML_FILE_TYPE   103

int file_sender_transport(int updown, int file_type, char *local_file, char *remote_file, int port, int lognum);
int file_sender_init(char *filename, int startpos, int lognum);
int file_sender_uninit();
void file_sender_clear();

file_sender_t *getFileSender();

#endif
