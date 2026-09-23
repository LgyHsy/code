#ifndef _ANJ_CONFIG_SERVER_H_
#define _ANJ_CONFIG_SERVER_H_

#ifdef __cplusplus
extern "C"
{
#endif


#define FTP_NAME_MAX_LEN 128
#define FTP_PASSWORD_MAX_LEN 128
#define FTP_PATH_MAX_LEN 256

#define SMTP_NAME_MAX_LEN 128
#define SMTP_PASSWORD_MAX_LEN 128
#define SMTP_ACCOUNT_MAX_LEN 256
#define SMTP_SUBJECT_MAX_LEN 256


typedef struct
{
    int index;
    char serverIP[MAX_IP_NAME_LEN];
    int serverPort;
    char userName[FTP_NAME_MAX_LEN];
    char password[FTP_PASSWORD_MAX_LEN];
    char filePath[FTP_PATH_MAX_LEN];
    int fileSize;
} FtpServer;

typedef struct
{
    int index;
    char toMail[SMTP_ACCOUNT_MAX_LEN];
    char ccMail[SMTP_ACCOUNT_MAX_LEN];
    char subject[SMTP_SUBJECT_MAX_LEN];
} SmtpServer;

enum
{
    // FTP_INDEX_FOR_RECORD_UPLOAD,
    FTP_INDEX_FOR_ALARM_UPLOAD,
    FTP_INDEX_FOR_LOG_BACKUP,
    FTP_INDEX_FOR_CONFIG_BACKUP,
    FTP_INDEX_FOR_UPDATE,
    FTP_SERVER_COUNT
};

enum
{
    // SMTP_INDEX_FOR_RECORD_UPLOAD,
    SMTP_INDEX_FOR_ALARM_UPLOAD,
    SMTP_INDEX_FOR_LOG_BACKUP,
    SMTP_INDEX_FOR_CONFIG_BACKUP,
    SMTP_SERVER_COUNT
};

// #define FTP_SERVER_COUNT 5

typedef struct
{
    FtpServer ftpServers[FTP_SERVER_COUNT];
} FtpServerList;

// #define SMTP_SERVER_COUNT 4

typedef struct
{
    char serverIP[MAX_IP_NAME_LEN];
    unsigned int serverPort;
    int auth;
    char userName[SMTP_NAME_MAX_LEN];
    char password[SMTP_PASSWORD_MAX_LEN];
    char fromMail[SMTP_ACCOUNT_MAX_LEN];
    SmtpServer smtpServers[SMTP_SERVER_COUNT];
} SmtpServerList;

typedef struct
{
    FtpServer ftpServers[FTP_SERVER_COUNT];
    SmtpServerList smtpServers;
} ServerConfig;


/* 读取 / 解析 **************************************************************/
int anj_config_server_get(IXML_Node *pNode, ServerConfig *pServerCfg);

int anj_config_server_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf);

int anj_config_server_ftp_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf);

int anj_config_server_smtp_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf);

char *anj_config_server_ftp_conver_xml(FtpServerList *pFtpServerList, int bPwdEntrypt);
char *anj_config_server_smtp_conver_xml(SmtpServerList *pSmtpServerList, int bPwdEntrypt);
char *anj_config_server_conver_xml(ServerConfig* pServerCfg);

FtpServer *anj_config_server_ftp_get_by_id(ServerConfig *pServerCfg, int id);

/* 保存 / 设置 **************************************************************/
int anj_config_server_save(ServerConfig *pServerCfg);

int anj_config_server_set(ServerConfig *pServerCfg);

int anj_config_server_ftp_set(FtpServerList *pstFtpList);

int anj_config_server_smtp_list_set(SmtpServerList *pSmtpListCfg);
    
/* 加载 **********************************************************************/
int anj_config_server_load(ServerConfig *pServerCfg);



#ifdef __cplusplus
}
#endif

#endif
