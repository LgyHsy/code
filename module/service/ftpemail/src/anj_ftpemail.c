#include "anj_ftpemail.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_service_provider.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "util_font.h"

#define ANJ_FTPEMAIL_PATH_MAX_LEN 256
#define ANJ_FTPEMAIL_NAME_MAX_LEN 256

typedef struct
{
    int max_task_count;
    int async_enable;
} anj_ftpemail_cfg_t;

typedef struct
{
    FtpServer *ftp_param;
    char path[ANJ_FTPEMAIL_PATH_MAX_LEN];
    char filename[ANJ_FTPEMAIL_NAME_MAX_LEN];
    char remotepath[FTP_PATH_MAX_LEN];
    char remotefile[ANJ_FTPEMAIL_NAME_MAX_LEN];
    int async_task;
} anj_ftpemail_ftp_param_t;

typedef struct
{
    SmtpServerList *smtp_param;
    int smtp_account_index;
    char path[ANJ_FTPEMAIL_PATH_MAX_LEN];
    char filename[ANJ_FTPEMAIL_NAME_MAX_LEN];
    char infomation[ANJ_FTPEMAIL_INFO_MAX_LEN];
    int async_task;
} anj_ftpemail_email_param_t;

static anj_ftpemail_cfg_t s_stFtpemailCfg = {
    .max_task_count = 1,
    .async_enable = 1,
};

static pthread_mutex_t s_stFtpemailFtpMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stFtpemailEmailMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_iFtpemailFtpTaskCount = 0;
static int s_iFtpemailEmailTaskCount = 0;
static anj_thread_s s_stFtpemailFtpThread = {0};
static anj_thread_s s_stFtpemailEmailThread = {0};

static void anj_ftpemail_remove_temp_file(const char *path, const char *filename)
{
    if (path == NULL || filename == NULL)
    {
        return;
    }

    if (strcmp(path, "/tmp") == 0)
    {
        anj_mw_system_with_param("rm -f %s/%s", path, filename);
    }
    else if (strcmp(path, "/tmp/record") == 0)
    {
        anj_mw_system_with_param("rm -rf %s/*", path);
    }
}

static int anj_ftpemail_ftp_exec(anj_ftpemail_ftp_param_t *thread_param)
{
    FtpServer *param = NULL;
    char userName[FTP_NAME_MAX_LEN] = {0};
    char password[FTP_PASSWORD_MAX_LEN] = {0};

    if (thread_param == NULL || thread_param->ftp_param == NULL)
    {
        return -1;
    }

    param = thread_param->ftp_param;

    if (strlen(param->userName) == 0)
    {
        snprintf(userName, sizeof(userName), "%s", "anonymous");
        snprintf(password, sizeof(password), "%s", "anonymous");
    }
    else
    {
        snprintf(userName, sizeof(userName), "%s", param->userName);
        snprintf(password, sizeof(password), "%s", param->password);
    }

    char cmd[2048] = {0};

    snprintf(cmd, sizeof(cmd), "/bin/ftp_up.sh %s %s %s %s %s %s %d %s",
             param->serverIP,
             userName,
             password,
             thread_param->remotepath,
             thread_param->path,
             thread_param->filename,
             param->serverPort,
             thread_param->remotefile);

    if (thread_param->async_task == 0)
    {
        strncat(cmd, " &", sizeof(cmd) - strlen(cmd) - 1);
    }

    __WARN("begin: %s\n", cmd);
    anj_mw_system(cmd);
    __WARN("complete: %s\n", cmd);
    return 0;
}

static int anj_ftpemail_ftp_thread(void *ctx, int *bStart)
{
    anj_ftpemail_ftp_param_t *thread_param = (anj_ftpemail_ftp_param_t *)ctx;

    if (thread_param == NULL)
    {
        return -1;
    }

    anj_ftpemail_ftp_exec(thread_param);

    if (thread_param->async_task)
    {
        anj_ftpemail_remove_temp_file(thread_param->path, thread_param->filename);
        pthread_mutex_lock(&s_stFtpemailFtpMutex);
        if (s_iFtpemailFtpTaskCount > 0)
        {
            s_iFtpemailFtpTaskCount--;
        }
        pthread_mutex_unlock(&s_stFtpemailFtpMutex);
    }

    anj_mw_free(thread_param);
    return 0;
}

static int anj_ftpemail_email_exec(anj_ftpemail_email_param_t *thread_param)
{
    SmtpServerList *param = NULL;
    int index = 0;
    char *to = NULL;
    char *cc = NULL;
    char *from = NULL;
    int mode = 0;
    char body[ANJ_FTPEMAIL_INFO_MAX_LEN + 128] = {0};
    char subject[SMTP_SUBJECT_MAX_LEN + ANJ_FTPEMAIL_NAME_MAX_LEN + 16] = {0};
    char cmd[4096] = {0};

    if (thread_param == NULL || thread_param->smtp_param == NULL)
    {
        return -1;
    }

    param = thread_param->smtp_param;
    index = thread_param->smtp_account_index;
    to = param->smtpServers[index].toMail;
    cc = param->smtpServers[index].ccMail;
    from = param->fromMail;
    mode = param->auth - 1;

    if (index == SMTP_INDEX_FOR_ALARM_UPLOAD)
    {
        struct timeval now_tm;
        struct tm *ptm;

        gettimeofday(&now_tm, NULL);
        ptm = localtime(&now_tm.tv_sec);

        if (strlen(param->smtpServers[index].subject) > 0)
        {
            snprintf(subject, sizeof(subject), "%.*s %04d-%02d-%02d %02d:%02d:%02d",
                     SMTP_SUBJECT_MAX_LEN - 1, param->smtpServers[index].subject,
                     (1900 + ptm->tm_year),
                     (1 + ptm->tm_mon),
                     ptm->tm_mday,
                     ptm->tm_hour,
                     ptm->tm_min,
                     ptm->tm_sec);
        }
        else
        {
            snprintf(subject, sizeof(subject), "IP Camera Alarm %04d-%02d-%02d %02d:%02d:%02d",
                     (1900 + ptm->tm_year),
                     (1 + ptm->tm_mon),
                     ptm->tm_mday,
                     ptm->tm_hour,
                     ptm->tm_min,
                     ptm->tm_sec);
        }

        snprintf(body, sizeof(body), "%.*s\r\nEvent type:<IP Camera AI Detection>\r\n",
                 (int)sizeof(body) - 64, thread_param->infomation);
    }
    else
    {
        snprintf(subject, sizeof(subject), "%.*s %.*s",
                 SMTP_SUBJECT_MAX_LEN - 1, param->smtpServers[index].subject,
                 ANJ_FTPEMAIL_NAME_MAX_LEN - 1, thread_param->filename);
        snprintf(body, sizeof(body), "%.*s\r\n%s",
                 (int)sizeof(body) - 128, thread_param->infomation,
                 "Sent by aj ip camera automatically, PLEASE DO NOT REPLY!\r\nSee attached file for detail.\r\n");
    }

    char body_sn[2048] = {0};
    char sn[32] = {0};

    if (anj_sysmng_load_sn(sn, sizeof(sn)) == 0 && strlen(sn) > 0)
    {
        snprintf(body_sn, sizeof(body_sn), "%sDevice SN: <%s>", body, sn);
    }
    else
    {
        snprintf(body_sn, sizeof(body_sn), "%.*s", (int)sizeof(body_sn) - 1, body);
    }

    snprintf(cmd, sizeof(cmd),
             "/bin/mail "
             "server_name=%s server_port=%d mode=%d "
             "authentication=1 user_name=%s user_pwd=%s "
             "mail_subject=\"%s\" mail_content=\"%s\" attachments=%s/%s "
             "mail_from=%s mail_to=%s ",
             param->serverIP,
             param->serverPort,
             mode,
             param->userName,
             param->password,
             subject,
             body_sn,
             thread_param->path,
             thread_param->filename,
             from,
             to);

    if (strlen(cc) > 0)
    {
        strncat(cmd, "mail_cc=", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, cc, sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
    }

    if (thread_param->async_task == 0)
    {
        strncat(cmd, " &", sizeof(cmd) - strlen(cmd) - 1);
    }

    __ERR("begin: %s\n", cmd);
    anj_mw_system(cmd);
    __ERR("complete: %s\n", cmd);
    return 0;
}

static int anj_ftpemail_email_thread(void *ctx, int *bStart)
{
    anj_ftpemail_email_param_t *thread_param = (anj_ftpemail_email_param_t *)ctx;

    if (thread_param == NULL)
    {
        return -1;
    }

    anj_ftpemail_email_exec(thread_param);

    if (thread_param->async_task)
    {
        anj_ftpemail_remove_temp_file(thread_param->path, thread_param->filename);
        pthread_mutex_lock(&s_stFtpemailEmailMutex);
        if (s_iFtpemailEmailTaskCount > 0)
        {
            s_iFtpemailEmailTaskCount--;
        }
        pthread_mutex_unlock(&s_stFtpemailEmailMutex);
    }

    anj_mw_free(thread_param);
    return 0;
}

static int anj_ftpemail_create_test_file(const char *path, const char *filename,
                                         const char *device_ip, const char *title)
{
    FILE *fp = NULL;
    char filepath[512] = {0};

    snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);
    fp = fopen(filepath, "w");
    if (fp == NULL)
    {
        __ERR("create test file %s failed, err=%s\n", filepath, strerror(errno));
        return -1;
    }

    fprintf(fp, "%s\r\n", title);
    if (device_ip != NULL && strlen(device_ip) > 0)
    {
        fprintf(fp, "Device IP: %s\r\n", device_ip);
    }

    fclose(fp);
    return 0;
}

static int anj_ftpemail_impl_ftp_file(FtpServer *param, const char *path, const char *filename,
                          const char *remotepath, const char *remotefile)
{
    anj_ftpemail_ftp_param_t *thread_param = NULL;
    int iRet = 0;

    if (param == NULL || path == NULL || filename == NULL || remotepath == NULL || remotefile == NULL)
    {
        return -1;
    }

    if (strlen(param->serverIP) == 0)
    {
        __ERR("ftp server ip is empty\n");
        return -1;
    }

    if (s_iFtpemailFtpTaskCount >= s_stFtpemailCfg.max_task_count)
    {
        __ERR("too many ftp task\n");
        anj_ftpemail_remove_temp_file(path, filename);
        return -1;
    }

    thread_param = (anj_ftpemail_ftp_param_t *)anj_mw_malloc(sizeof(anj_ftpemail_ftp_param_t));
    if (thread_param == NULL)
    {
        anj_ftpemail_remove_temp_file(path, filename);
        return -1;
    }

    memset(thread_param, 0, sizeof(anj_ftpemail_ftp_param_t));
    thread_param->ftp_param = param;
    thread_param->async_task = s_stFtpemailCfg.async_enable;
    strncpy(thread_param->path, path, sizeof(thread_param->path) - 1);
    strncpy(thread_param->filename, filename, sizeof(thread_param->filename) - 1);
    strncpy(thread_param->remotepath, remotepath, sizeof(thread_param->remotepath) - 1);
    strncpy(thread_param->remotefile, remotefile, sizeof(thread_param->remotefile) - 1);

    if (s_stFtpemailCfg.async_enable == 0)
    {
        anj_ftpemail_ftp_exec(thread_param);
        anj_mw_free(thread_param);
        return 0;
    }

    s_iFtpemailFtpTaskCount++;
    memset(&s_stFtpemailFtpThread, 0, sizeof(s_stFtpemailFtpThread));
    s_stFtpemailFtpThread.bAutoDestroy = 1;
    strncpy(s_stFtpemailFtpThread.iThreadName, "ftpemail_ftp", sizeof(s_stFtpemailFtpThread.iThreadName) - 1);
    s_stFtpemailFtpThread.iThreadjob.ctx = thread_param;
    s_stFtpemailFtpThread.iThreadjob.func = anj_ftpemail_ftp_thread;
    iRet = anj_thread_task_create(&s_stFtpemailFtpThread);
    if (iRet != 0)
    {
        __ERR("ftpemail ftp thread create failed\n");
        if (s_iFtpemailFtpTaskCount > 0)
        {
            s_iFtpemailFtpTaskCount--;
        }
        anj_ftpemail_remove_temp_file(path, filename);
        anj_mw_free(thread_param);
        return -1;
    }

    return 0;
}

static int anj_ftpemail_impl_email_file(SmtpServerList *param, int account_index,
                            const char *path, const char *filename, const char *infomation)
{
    anj_ftpemail_email_param_t *thread_param = NULL;
    int iRet = 0;

    if (param == NULL || path == NULL || filename == NULL)
    {
        return -1;
    }

    if (account_index < 0 || account_index >= SMTP_SERVER_COUNT)
    {
        return -1;
    }

    if (strlen(param->serverIP) == 0 || strlen(param->smtpServers[account_index].toMail) == 0)
    {
        __ERR("smtp server or receiver is empty\n");
        return -1;
    }

    if (s_iFtpemailEmailTaskCount >= s_stFtpemailCfg.max_task_count)
    {
        __ERR("too many email task\n");
        anj_ftpemail_remove_temp_file(path, filename);
        return -1;
    }

    thread_param = (anj_ftpemail_email_param_t *)anj_mw_malloc(sizeof(anj_ftpemail_email_param_t));
    if (thread_param == NULL)
    {
        anj_ftpemail_remove_temp_file(path, filename);
        return -1;
    }

    memset(thread_param, 0, sizeof(anj_ftpemail_email_param_t));
    thread_param->smtp_param = param;
    thread_param->smtp_account_index = account_index;
    thread_param->async_task = s_stFtpemailCfg.async_enable;
    strncpy(thread_param->path, path, sizeof(thread_param->path) - 1);
    strncpy(thread_param->filename, filename, sizeof(thread_param->filename) - 1);
    if (infomation != NULL)
    {
        str_gb2312_2_utf8(infomation, thread_param->infomation, 0);
    }

    if (s_stFtpemailCfg.async_enable == 0)
    {
        anj_ftpemail_email_exec(thread_param);
        anj_mw_free(thread_param);
        return 0;
    }

    s_iFtpemailEmailTaskCount++;
    memset(&s_stFtpemailEmailThread, 0, sizeof(s_stFtpemailEmailThread));
    s_stFtpemailEmailThread.bAutoDestroy = 1;
    strncpy(s_stFtpemailEmailThread.iThreadName, "ftpemail_email", sizeof(s_stFtpemailEmailThread.iThreadName) - 1);
    s_stFtpemailEmailThread.iThreadjob.ctx = thread_param;
    s_stFtpemailEmailThread.iThreadjob.func = anj_ftpemail_email_thread;
    iRet = anj_thread_task_create(&s_stFtpemailEmailThread);
    if (iRet != 0)
    {
        __ERR("ftpemail email thread create failed\n");
        if (s_iFtpemailEmailTaskCount > 0)
        {
            s_iFtpemailEmailTaskCount--;
        }
        anj_ftpemail_remove_temp_file(path, filename);
        anj_mw_free(thread_param);
        return -1;
    }

    return 0;
}

static int anj_ftpemail_impl_ftp_test(FtpServer *param, const char *device_ip)
{
    const char *path = "/tmp";
    const char *filename = "ftp_test.txt";
    const char *remotefile = "ftp_test.txt";

    if (param == NULL)
    {
        return -1;
    }

    if (anj_ftpemail_create_test_file(path, filename, device_ip, "FTP Test from IP Camera") != 0)
    {
        return -1;
    }

    return anj_ftpemail_impl_ftp_file(param, path, filename, param->filePath, remotefile);
}

static int anj_ftpemail_impl_smtp_test(SmtpServerList *param, int account_index, const char *device_ip)
{
    const char *path = "/tmp";
    const char *filename = "smtp_test.txt";
    char infomation[ANJ_FTPEMAIL_INFO_MAX_LEN] = {0};

    if (param == NULL)
    {
        return -1;
    }

    if (anj_ftpemail_create_test_file(path, filename, device_ip, "SMTP Test from IP Camera") != 0)
    {
        return -1;
    }

    snprintf(infomation, sizeof(infomation), "This is a test email from IP Camera.");
    return anj_ftpemail_impl_email_file(param, account_index, path, filename, infomation);
}

static int anj_ftpemail_init(void)
{
    anj_sysctl_capability_add(FUNCTION_FTPEMAIL_STORAGE);
    anj_sysctl_capability_add(FUNCTION_EMAIL_SSL);
    return 0;
}

static int anj_ftpemail_uninit(void)
{
    s_iFtpemailFtpTaskCount = 0;
    s_iFtpemailEmailTaskCount = 0;
    return 0;
}

static const anj_service_provider_ops s_stFtpemailProviderOps = {
    .provider_name = "ftpemail",
    .provider_type = ANJ_SERVICE_PROVIDER_FTP_EMAIL,
    .provider_priority = ANJ_SERVICE_PROVIDER_FTP_EMAIL,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_NONE,
    .init = anj_ftpemail_init,
    .uninit = anj_ftpemail_uninit,
    .alarm_event_notify = NULL,
    .audio_enc_change = NULL,
    .ftp_file = anj_ftpemail_impl_ftp_file,
    .email_file = anj_ftpemail_impl_email_file,
    .ftp_test = anj_ftpemail_impl_ftp_test,
    .smtp_test = anj_ftpemail_impl_smtp_test,
};

ANJ_LINK_KEEP(anj_keep_ftpemail_provider);

__attribute__((constructor)) static void anj_ftpemail_register(void)
{
    anj_service_provider_register(&s_stFtpemailProviderOps);
}

__attribute__((destructor)) static void anj_ftpemail_unregister(void)
{
    anj_service_provider_unregister(&s_stFtpemailProviderOps);
}
