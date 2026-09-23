#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_snap.h"
#include "audio_receiver.h"
#include "eventhub.h"
#include "project_option.h"

#include "hik_net_alarm.h"
#include "hik_net_cmd.h"
#include "hik_net_ctrl.h"
#include "hik_net_types.h"

#define HIK_VOICE_MAX_FRAME 128
#define HIK_VOICE_FRAME_SIZE 320

static pthread_t s_voice_tid;
static volatile int s_voice_run = 0;
static volatile int s_voice_started = 0;
static int s_voice_fd = -1;
static pthread_mutex_t s_voice_lock = PTHREAD_MUTEX_INITIALIZER;

/* Maps Hik opcodes to EVENTHUB_PTZ_HANDLE (AjPtzControl-equivalent);
 * anj_ptz then drives anj_ptz_provider_operate. */
static void hik_ptz_publish(const char *cmd, int speed_or_preset, int is_preset)
{
    EventResult event_result;
    PtzCmdParse stPtzCmdParse;
    int speed = speed_or_preset;

    memset(&event_result, 0, sizeof(event_result));
    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    strncpy(stPtzCmdParse.ptzCmd, cmd, sizeof(stPtzCmdParse.ptzCmd) - 1);

    if (is_preset)
    {
        stPtzCmdParse.presetID = speed_or_preset;
        stPtzCmdParse.flag = 1;
    }
    else
    {
        if (speed < 0)
        {
            speed = 0;
        }
        if (speed > 7)
        {
            speed = 7;
        }
        stPtzCmdParse.panSpeed = (UINT8)(speed + 1);
        stPtzCmdParse.tiltSpeed = (UINT8)(speed + 1);
    }
    eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
}

static int hik_ptz_apply(int command, int data)
{
    int stop = 0;
    int cmd = command;

    if (command < 0)
    {
        cmd = ~command;
        stop = 1;
    }

    if (stop || cmd == HIK_PTZ_STOP_ALL)
    {
        hik_ptz_publish("stop", 0, 0);
        return 0;
    }

    switch (cmd)
    {
    case HIK_PTZ_TILT_UP:
        hik_ptz_publish("up", data, 0);
        break;
    case HIK_PTZ_TILT_DOWN:
        hik_ptz_publish("down", data, 0);
        break;
    case HIK_PTZ_PAN_LEFT:
        hik_ptz_publish("left", data, 0);
        break;
    case HIK_PTZ_PAN_RIGHT:
        hik_ptz_publish("right", data, 0);
        break;
    case HIK_PTZ_UP_LEFT:
        hik_ptz_publish("left_up", data, 0);
        break;
    case HIK_PTZ_UP_RIGHT:
        hik_ptz_publish("right_up", data, 0);
        break;
    case HIK_PTZ_DOWN_LEFT:
        hik_ptz_publish("left_down", data, 0);
        break;
    case HIK_PTZ_DOWN_RIGHT:
        hik_ptz_publish("right_down", data, 0);
        break;
    case HIK_PTZ_ZOOM_IN:
        hik_ptz_publish("zoomtele", data, 0);
        break;
    case HIK_PTZ_ZOOM_OUT:
        hik_ptz_publish("zoomwide", data, 0);
        break;
    case HIK_PTZ_GOTO_PRESET:
        hik_ptz_publish("callpreset", data, 1);
        break;
    case HIK_PTZ_SET_PRESET:
        hik_ptz_publish("setpreset", data, 1);
        break;
    case HIK_PTZ_CLE_PRESET:
        hik_ptz_publish("clearpreset", data, 1);
        break;
    case HIK_PTZ_FOCUS_IN:
    case HIK_PTZ_FOCUS_OUT:
    case HIK_PTZ_IRIS_ENLARGE:
    case HIK_PTZ_IRIS_SHRINK:
    case HIK_PTZ_LIGHT_PWRON:
    case HIK_PTZ_WIPER_PWRON:
    case HIK_PTZ_AUTO_PAN:
        __DBG("hik ptz cmd=%d stub/ignored\n", cmd);
        break;
    default:
        __DBG("hik ptz unsupported cmd=%d\n", cmd);
        break;
    }
    return 0;
}

int hik_cmd_ptz(int fd, const char *recvbuf, int recvlen)
{
    NET_PTZ_CTRL_DATA req;
    UINT32 chan = 1;
    UINT32 command = 0;
    UINT32 presetNo = 0;

    if (recvlen < (int)sizeof(req))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    chan = ntohl(req.channel);
    command = ntohl(req.command);
    presetNo = ntohl(req.presetNo);

    if (chan < 1 || chan > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }

    __INFO("hik PTZ cmd=%u data=%u\n", command, presetNo);
    (void)hik_ptz_apply((int)command, (int)presetNo);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

int hik_cmd_ptz_with_speed(int fd, const char *recvbuf, int recvlen)
{
    NET_PTZ_CTRL_DATA req;
    UINT32 chan = 1;
    UINT32 command = 0;
    UINT32 speed = 0;

    if (recvlen < (int)sizeof(req))
    {
        return hik_send_retval(fd, NETRET_ERROR_DATA);
    }

    memcpy(&req, recvbuf, sizeof(req));
    chan = ntohl(req.channel);
    command = ntohl(req.command);
    speed = ntohl(req.speed);

    if (chan < 1 || chan > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }

    __INFO("hik PTZ_WITHSPEED cmd=%u speed=%u\n", command, speed);
    (void)hik_ptz_apply((int)command, (int)speed);
    return hik_send_retval(fd, NETRET_QUALIFIED);
}

static int hik_read_file_buf(const char *path, unsigned char **out, int *out_size)
{
    FILE *fp = NULL;
    long sz = 0;
    unsigned char *buf = NULL;
    size_t n = 0;

    *out = NULL;
    *out_size = 0;
    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz <= 0)
    {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return -1;
    }
    buf = (unsigned char *)malloc((size_t)sz);
    if (buf == NULL)
    {
        fclose(fp);
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (n != (size_t)sz)
    {
        free(buf);
        return -1;
    }
    *out = buf;
    *out_size = (int)sz;
    return 0;
}

int hik_cmd_get_jpeg(int fd, const char *recvbuf, int recvlen)
{
    NETCMD_CHAN_HEADER chan_hdr;
    UINT32 chan = 1;
    char path[64] = "/tmp";
    char name[64] = "snap.hik.jpg";
    char full[128];
    unsigned char *jpeg = NULL;
    int jpeg_size = 0;
    NETRET_HEADER header;
    int quality = 60;

    if (recvlen >= (int)sizeof(chan_hdr))
    {
        memcpy(&chan_hdr, recvbuf, sizeof(chan_hdr));
        chan = ntohl(chan_hdr.channel);
    }
    if (chan < 1 || chan > (UINT32)ANJ_CAMERA_MAX_NUMS)
    {
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }

    if (recvlen >= (int)(sizeof(NETCMD_CHAN_HEADER) + sizeof(JPEG_CFG)))
    {
        JPEG_CFG jcfg;
        memcpy(&jcfg, recvbuf + sizeof(NETCMD_CHAN_HEADER), sizeof(jcfg));
        /* quality field is [0,3] in old JPEG_CFG; map coarsely */
        switch (jcfg.quality)
        {
        case 0:
            quality = 40;
            break;
        case 1:
            quality = 60;
            break;
        case 2:
            quality = 80;
            break;
        case 3:
            quality = 90;
            break;
        default:
            quality = 60;
            break;
        }
    }

    snprintf(full, sizeof(full), "%s/%s", path, name);
    unlink(full);

    if (anj_snap_jpg(0, 1, quality, path, name, NULL) != 0)
    {
        __WARN("hik snap jpg start failed\n");
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }
    if (anj_snap_wait_complete(full, 2000) != 0)
    {
        __WARN("hik snap jpg wait failed\n");
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }
    if (hik_read_file_buf(full, &jpeg, &jpeg_size) != 0 || jpeg == NULL || jpeg_size <= 0)
    {
        __WARN("hik read jpg failed\n");
        return hik_send_retval(fd, NETRET_NO_CHANNEL);
    }

    memset(&header, 0, sizeof(header));
    header.length = htonl((UINT32)(sizeof(NETRET_HEADER) + jpeg_size));
    header.retVal = htonl(NETRET_QUALIFIED);
    /* Match old: checksum covers header.retVal..header end only, not jpeg body */
    header.checkSum = htonl(hik_check_byte_sum((char *)&header.retVal, (int)sizeof(NETRET_HEADER) - 8));

    if (hik_writen(fd, &header, sizeof(header)) != 0 || hik_writen(fd, jpeg, (size_t)jpeg_size) != 0)
    {
        free(jpeg);
        return -1;
    }
    __INFO("hik GET_JPEG size=%d\n", jpeg_size);
    free(jpeg);
    unlink(full);
    return 0;
}

static void *hik_voice_talk_task(void *arg)
{
    char frame_buf[HIK_VOICE_MAX_FRAME * HIK_VOICE_FRAME_SIZE];
    UINT32 frame_nums = 0;
    audio_talk_start_param_t talk_param;

    (void)arg;
    prctl(PR_SET_NAME, "hik_voice");
    __INFO("hik voice talk task start fd=%d\n", s_voice_fd);

    memset(&talk_param, 0, sizeof(talk_param));
    talk_param.codec_type = MEDIA_CODEC_AUDIO_G711U;
    talk_param.samplerate = 8000;
    talk_param.bitrate = 64000;
    talk_param.same_port = 1;
    if (audio_talk_start(&talk_param) != 0)
    {
        __WARN("hik voice audio_talk_start failed; drain-only mode\n");
    }

    while (s_voice_run)
    {
        fd_set readset;
        struct timeval timeout;
        int fd = 0;
        int ret = 0;
        int datalen = 0;

        pthread_mutex_lock(&s_voice_lock);
        fd = s_voice_fd;
        pthread_mutex_unlock(&s_voice_lock);
        if (fd <= 0)
        {
            break;
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        FD_ZERO(&readset);
        FD_SET(fd, &readset);
        ret = select(fd + 1, &readset, NULL, NULL, &timeout);
        if (ret == 0)
        {
            continue;
        }
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (!FD_ISSET(fd, &readset))
        {
            continue;
        }

        if (hik_readn(fd, &frame_nums, sizeof(frame_nums)) != 0)
        {
            break;
        }
        frame_nums = ntohl(frame_nums);
        if (frame_nums == 0 || frame_nums > HIK_VOICE_MAX_FRAME)
        {
            __WARN("hik voice bad frameNums=%u\n", frame_nums);
            break;
        }
        datalen = (int)frame_nums * HIK_VOICE_FRAME_SIZE;
        if (hik_readn(fd, frame_buf, (size_t)datalen) != 0)
        {
            break;
        }
        /* Best-effort playback; no uplink PCM/G711 encode back to NVR yet. */
        (void)audio_talk_feed_audio(frame_buf, datalen, MEDIA_CODEC_AUDIO_G711U, 8000, 64000);
    }

    audio_talk_stop();

    pthread_mutex_lock(&s_voice_lock);
    if (s_voice_fd >= 0)
    {
        close(s_voice_fd);
        s_voice_fd = -1;
    }
    s_voice_started = 0;
    s_voice_run = 0;
    pthread_mutex_unlock(&s_voice_lock);
    __INFO("hik voice talk task exit\n");
    return NULL;
}

int hik_net_voice_stop(void)
{
    pthread_mutex_lock(&s_voice_lock);
    s_voice_run = 0;
    if (s_voice_fd >= 0)
    {
        shutdown(s_voice_fd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&s_voice_lock);

    if (s_voice_tid != 0)
    {
        pthread_join(s_voice_tid, NULL);
        s_voice_tid = 0;
    }

    audio_talk_stop();

    pthread_mutex_lock(&s_voice_lock);
    if (s_voice_fd >= 0)
    {
        close(s_voice_fd);
        s_voice_fd = -1;
    }
    s_voice_started = 0;
    pthread_mutex_unlock(&s_voice_lock);
    return 0;
}

int hik_cmd_start_voicecom(int fd, const char *recvbuf, int recvlen)
{
    (void)recvbuf;
    (void)recvlen;

    pthread_mutex_lock(&s_voice_lock);
    if (s_voice_started)
    {
        pthread_mutex_unlock(&s_voice_lock);
        __WARN("hik voice already started\n");
        (void)hik_send_retval(fd, NETRET_DVR_OPER_FAILED);
        return 0;
    }

    if (hik_send_retval(fd, NETRET_QUALIFIED) != 0)
    {
        pthread_mutex_unlock(&s_voice_lock);
        return -1;
    }

    s_voice_fd = fd;
    s_voice_run = 1;
    s_voice_started = 1;
    if (pthread_create(&s_voice_tid, NULL, hik_voice_talk_task, NULL) != 0)
    {
        s_voice_fd = -1;
        s_voice_run = 0;
        s_voice_started = 0;
        pthread_mutex_unlock(&s_voice_lock);
        __ERR("hik create voice task failed\n");
        return -1;
    }
    pthread_mutex_unlock(&s_voice_lock);
    __INFO("hik STARTVOICECOM keep fd=%d\n", fd);
    return 1; /* keep-open */
}

int hik_cmd_alarmchan(int fd, const char *recvbuf, int recvlen)
{
    (void)recvbuf;
    (void)recvlen;

    if (hik_net_alarm_add_fd(fd) != 0)
    {
        (void)hik_send_retval(fd, NETRET_QUALIFIED);
        return -1;
    }
    if (hik_send_retval(fd, NETRET_QUALIFIED) != 0)
    {
        return -1;
    }
    __INFO("hik ALARMCHAN keep fd=%d\n", fd);
    return 1; /* keep-open */
}
