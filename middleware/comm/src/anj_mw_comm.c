#include "anj_mw_comm.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define CALC_TOTAL_DIFF                                            \
    do                                                             \
    {                                                              \
        total_diff = (unsigned)(p_jif->total - p_prev_jif->total); \
        if (total_diff == 0)                                       \
            total_diff = 1;                                        \
    } while (0)
#define CALC_STAT(xxx) double xxx = 100.0 * (double)(p_jif->xxx - p_prev_jif->xxx) / (double)total_diff
#define FMT "%.1f%5 "

static int get_arbitrary_resolution(const char *name, int *width, int *height)
{
    int ret = sscanf(name, "%dX%d", width, height);
    if (ret < 2)
    {
        ret = sscanf(name, "%dx%d", width, height);
    }

    if (ret == 2)
        return 0;
    else
        return -1;
}

int anj_mw_system(const char *cmd)
{
    pid_t pid;
    int status;
    if (cmd == NULL)
    {
        return 1; /**< if cmdstring is NULL return no zero */
    }
    if ((pid = vfork()) < 0)
    {
        /**< vfork,child pid share resource with parrent,not copy */
        status = -1; /**<vfork fail */
    }
    else if (pid == 0)
    {
        /*关闭所有的文件描述符: */
        int i;
        for (i = 3; i < sysconf(_SC_OPEN_MAX); i++)
        {
            close(i);
        }

        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(127); /**< return 127 only exec fail;the chid procee is not exist normore if exec success fail */
    }
    else
    {
        /** parrent pid */
        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                status = -1; /**< return -1 when interrupted by signal except EINTR */
                break;
            }
        }
    }
    return status; /**< return the state of child progress if waitpid success */
}

int anj_mw_system_with_param(const char *fmt, ...)
{
    char content_buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, 256, fmt, ap);
    va_end(ap);

    return anj_mw_system(content_buf);
}

void anj_mw_system_free_cache()
{
    anj_mw_system("sync;echo 3 > /proc/sys/vm/drop_caches");
}

void ResGetDmt(const char *resName, const char *stdName, int *width, int *height)
{
    if (get_arbitrary_resolution(resName, width, height) == 0)
    {
        //		__ERR("%s %s %d %d\n", resName, stdName, *width, *height);
        return;
    }

    // main stream
    if (strcmp(resName, "5MP") == 0)
    {
        *width = RESOLUTION_5MP_WIDTH;
        *height = RESOLUTION_5MP_HEIGHT;
    }
    else if (strcmp(resName, "Disable") == 0)
    {
        *width = 640;
        *height = 360; // 270
    }
    else if (strcmp(resName, "4K") == 0)
    {
        *width = 3840;
        *height = 2160;
    }
    else if (strcmp(resName, "8MP") == 0)
    {
        *width = 3840;
        *height = 2160;
    }
    else if (strcmp(resName, "6MP") == 0)
    {
        *width = 3072;
        *height = 2048;
    }
    else if (strcmp(resName, "12MP") == 0)
    {
        *width = 4000;
        *height = 3000;
    }
    else if (strcmp(resName, "4MP_H") == 0)
    {
        *width = 2688;
        *height = 1512;
    }
    else if (strcmp(resName, "4MP") == 0)
    {
        *width = 2560;
        *height = 1440;
    }
    else if (strcmp(resName, "3MP") == 0)
    {
        *width = 2304;
        *height = 1296;
    }
    else if (strcmp(resName, "3MP(4X3)") == 0)
    {
        *width = 2048;
        *height = 1536;
    }
    else if (strcmp(resName, "1080P") == 0)
    {
        *width = 1920;
        *height = 1080;
    }
    else if (strcmp(resName, "1200P") == 0)
    {
        *width = 1600;
        *height = 1200;
    }
    else if (strcmp(resName, "1024P") == 0)
    {
        *width = 1280;
        *height = 1024;
    }
    else if (strcmp(resName, "960P") == 0)
    {
        *width = 1280;
        *height = 960;
    }
    else if (strcmp(resName, "768P") == 0)
    {
        *width = 1024;
        *height = 768;
    }
    else if (strcmp(resName, "720P") == 0)
    {
        *width = 1280;
        *height = 720;
    }
    else if (strcmp(resName, "D1") == 0)
    {
        *width = 704;
        *height = 576;
    }
    else if (strcmp(resName, "HD1") == 0)
    {
        *width = 704;
        *height = 288;
    }
    else if (strcmp(resName, "VGA") == 0)
    {
        *width = 640;
        *height = 480;
    }
    else if (strcmp(resName, "WSVGA") == 0)
    {
        *width = 1024;
        *height = 576;
    }
    else if (strcmp(resName, "CIF") == 0)
    {
        *width = 352;
        *height = 288;
    }
    else if (strcmp(resName, "QVGA") == 0)
    {
        *width = 320;
        *height = 240;
    }
    else if (strcmp(resName, "HCIF") == 0)
    {
        *width = 352;
        *height = 144;
    }
    else if (strcmp(resName, "QCIF") == 0)
    {
        *width = 192;
        *height = 160;
    }
    else if (strcmp(resName, "960H") == 0)
    {
        *width = 960;
        *height = 576;
    }
    else if (strcmp(resName, "H960H") == 0)
    {
        *width = 960;
        *height = 288;
    }
    else if (strcmp(resName, "Q960H") == 0)
    {
        *width = 480;
        *height = 288;
    }
    else if (strcmp(resName, "HQ960H") == 0)
    {
        *width = 480;
        *height = 144;
    }
    else if (strcmp(resName, "QQ960H") == 0)
    {
        *width = 240;
        *height = 144;
    }
    else
    {
        *width = 1280;
        *height = 720;
    }
}

void GetVideoSize(const char *resName, int tvsystem, int *width, int *height)
{
    char stdName[16];
    if (tvsystem == 1)
    {
        strcpy(stdName, "PAL");
    }
    else
    {
        strcpy(stdName, "NTSC");
    }

    ResGetDmt(resName, stdName, width, height);
}

ANJ_SIZE_S getPicSize(const char *resolution, int tvsystem, int bRotate, int bTrue)
{
    ANJ_SIZE_S ret;

    if (bRotate > 0)
    {
        GetVideoSize(resolution, tvsystem, (int *)&ret.u32Height, (int *)&ret.u32Width);
    }
    else
    {
        GetVideoSize(resolution, tvsystem, (int *)&ret.u32Width, (int *)&ret.u32Height);
    }

    if (bTrue == 0)
    {
        if (ret.u32Width > RESOLUTION_MAX_WIDTH)
        {
            ret.u32Width = RESOLUTION_MAX_WIDTH;
        }
        if (ret.u32Height > RESOLUTION_MAX_HEIGHT)
        {
            ret.u32Height = RESOLUTION_MAX_HEIGHT;
        }
    }

    __INFO("resolution is %s, picSize=%uX%u\n", resolution, ret.u32Width, ret.u32Height);

    return ret;
}

ANJ_SIZE_S anj_mw_sensor_get_size(void)
{
    ANJ_SIZE_S stSize;

    switch (ANJ_PROJECT_SENSOR)
    {
    case SENSOR_TYPE_OS05A20:
        stSize.u32Width = 2592;
        stSize.u32Height = 1944;
        break;
    case SENSOR_TYPE_SC4336P:
        stSize.u32Width = 2560;
        stSize.u32Height = 1440;
        break;
    default:
        stSize.u32Width = RESOLUTION_MAX_WIDTH;
        stSize.u32Height = RESOLUTION_MAX_HEIGHT;
        break;
    }

    return stSize;
}

int anj_mw_sensor_support_wdr(void)
{
    switch (ANJ_PROJECT_SENSOR)
    {
    case SENSOR_TYPE_SC4336P:
        return 0;
    case SENSOR_TYPE_OS05A20:
    default:
        return 1;
    }
}

int anj_mw_check_value_in_range(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    else if (value > max_value)
    {
        return max_value;
    }

    return value;
}

int anj_mw_check_value_by_default(int value, int min_value, int max_value, int def_value)
{
    if (value < min_value || value > max_value)
    {
        return def_value;
    }

    return value;
}

int ExecShellCmd(const char *pCmd, char *pResult, int iResultLen)
{
    int nRet = 0;
    struct timeval stNow = {0};
    char szfileName[64] = {0};
    char szCommand[256] = {0};
    FILE *fp = NULL;

    if (!pCmd || !strlen(pCmd) || !pResult || iResultLen <= 0)
    {
        return -1;
    }

    gettimeofday(&stNow, NULL);

    memset(szfileName, 0, sizeof(szfileName));
    snprintf(szfileName, sizeof(szfileName), "/tmp/.ExecShellCmd%ld", stNow.tv_usec);

    memset(pResult, 0, iResultLen);
    memset(szCommand, 0, sizeof(szCommand));
    snprintf(szCommand, sizeof(szCommand), "%s > %s", pCmd, szfileName);

    anj_mw_system(szCommand);
    if (NULL == (fp = fopen(szfileName, "r")))
    {
        perror("fopen");
        nRet = -1;
    }
    else
    {
        fread(pResult, sizeof(char), iResultLen - 1, fp);
        if (fclose(fp))
        {
            perror("fclose");
            nRet = -1;
        }

        fp = NULL;
    }

    memset(szCommand, 0, sizeof(szCommand));
    snprintf(szCommand, sizeof(szCommand), "rm -rf %s", szfileName);
    anj_mw_system(szCommand);

    return nRet;
}

int SearchStringInCmd(const char *pCmd, const char *string)
{
    int iRet = 0;
    char *point = NULL;
    FILE *fp = NULL;
    char buf[1024] = {0};
    struct timeval now;
    char fileName[64] = {0};
    char cmd[256] = {0};

    if (NULL == pCmd || NULL == string)
    {
        return -1;
    }

    gettimeofday(&now, NULL);

    snprintf(fileName, sizeof(fileName), "/tmp/.SearchCmd%ld", now.tv_usec);
    snprintf(cmd, sizeof(cmd), "%s > %s", pCmd, fileName);
    anj_mw_system(cmd);

    if ((fp = fopen(fileName, "r")) == NULL)
    {
        fprintf(stderr, "Fail to fopen %s\n", fileName);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), "rm -rf %s", fileName);
        anj_mw_system(cmd);
        return -1;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        point = strcasestr(buf, string);
        if (point == NULL)
        {
            iRet = 0;
            continue;
        }
        else
        {
            iRet = 1;
            break;
        }
    }

    if (fclose(fp) == -1)
    {
        perror("fclose");
        iRet = -1;
    }

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "rm -rf %s", fileName);
    anj_mw_system(cmd);

    return iRet;
}

int mysystem_with_param(const char *fmt, ...)
{
    char content_buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, 256, fmt, ap);
    va_end(ap);

    return anj_mw_system(content_buf);
}

void cpu_info_get(jiffy_counts_t *cpu_info)
{
    static jiffy_counts_t data_stat0 = {0};
    static jiffy_counts_t data_stat1 = {0};
    static const char szCpuFmt[] = "cpu %llu %llu %llu %llu %llu %llu %llu %llu";
    char line_buf[256] = {0};
    FILE *fd = anj_mw_fopen("/proc/stat", "r");
    if (fd != NULL)
    {
        anj_mw_fread(fd, line_buf, sizeof(line_buf));

        jiffy_counts_t *p_jif = &data_stat1;
        jiffy_counts_t *p_prev_jif = &data_stat0;
        unsigned total_diff;

        int bCal = 0;
        if (p_prev_jif->busy == 0 && p_prev_jif->idle == 0 && p_prev_jif->total == 0)
        {
            p_jif = p_prev_jif;
            bCal = 0;
        }
        else
        {
            bCal = 1;
        }

        int ret = sscanf(line_buf, szCpuFmt,
                         &p_jif->usr, &p_jif->nic, &p_jif->sys, &p_jif->idle,
                         &p_jif->iowait, &p_jif->irq, &p_jif->softirq,
                         &p_jif->steal);
        if (ret >= 4)
        {
            p_jif->total = p_jif->usr + p_jif->nic + p_jif->sys + p_jif->idle + p_jif->iowait + p_jif->irq + p_jif->softirq + p_jif->steal;
            /* procps 2.x does not count iowait as busy time */
            p_jif->busy = p_jif->total - p_jif->idle - p_jif->iowait;
        }
        else
        {
            if (bCal > 0)
                bCal = 0;
        }

        fclose(fd);

        if (bCal > 0)
        {
            CALC_TOTAL_DIFF;

            // CALC_STAT(busy);
            // CALC_STAT(usr);
            // CALC_STAT(sys);
            // CALC_STAT(nic);
            // CALC_STAT(idle);
            // CALC_STAT(iowait);
            // CALC_STAT(irq);
            // CALC_STAT(softirq);
            /*CALC_STAT(steal);*/

            memcpy(p_prev_jif, p_jif, sizeof(jiffy_counts_t));
            memcpy(cpu_info, p_prev_jif, sizeof(jiffy_counts_t));
        }
    }
}

void aj_swap_value(int *a, int *b)
{
    int tmp_value = *a;
    *a = *b;
    *b = tmp_value;
}

unsigned char GetHexValue(char *str)
{
    unsigned char val = 0;
    char d1 = *str;
    char d2 = *(str + 1);

    if (d1 >= '0' && d1 <= '9')
    {
        val += (d1 - '0') * 16;
    }
    else if (d1 >= 'a' && d1 <= 'f')
    {
        val += (d1 - 'a' + 10) * 16;
    }
    else if (d1 >= 'A' && d1 <= 'F')
    {
        val += (d1 - 'A' + 10) * 16;
    }

    if (d2 >= '0' && d2 <= '9')
    {
        val += (d2 - '0');
    }
    else if (d2 >= 'a' && d2 <= 'f')
    {
        val += (d2 - 'a' + 10);
    }
    else if (d2 >= 'A' && d2 <= 'F')
    {
        val += (d2 - 'A' + 10);
    }

    return val;
}

unsigned char GetBitValue(char data, const int nPos)
{
    return ((data >> nPos) & 0x1) > 0 ? 1 : 0;
}

void SetBitValue(char *szTemp, const int nPos, const int nValue)
{
    if (nValue == 1)
        *szTemp |= 1UL << nPos; // 将nPos的bit位设置为1，其他位不变
    else if (nValue == 0)
        *szTemp &= ~(1UL << nPos); // 将nPos的bit位设置为0，其他位不变
}

int IsMainStream(int venchn)
{
    int grp  = venchn / ANJ_CAMERA_MAX_NUMS;
    int offset = venchn - grp * ANJ_CAMERA_MAX_NUMS;
    
    return (offset == 0) ? 1 : 0;
}