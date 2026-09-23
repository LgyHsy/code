#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_config.h"
#include "anj_config_media.h"
#include "audio_receiver.h"
#include "anj_audio.h"
#include "media_util.h"

#define AUDIO_TALK_RECV_BUF_SIZE (1024 * 5)

static int gstAudioRecvStart = 0;
static int s_ra_answer = 0;
static int s_ra_answered = 0;
static pthread_mutex_t s_stAudioRecvMutex = PTHREAD_MUTEX_INITIALIZER;
static media_codec_type_e s_eTalkCodecType = MEDIA_CODEC_AUDIO_PCM;
static int s_iTalkSampleRate = 8000;
static int s_iTalkBitrate = 16000;

static volatile int s_talk_working = 0;
static pthread_t s_talk_thread = 0;
static int s_talk_sockfd = -1;
static audio_talk_start_param_t s_talk_param;
static audio_talk_start_param_t s_talk_thread_arg;

static int audio_talk_is_mcastaddr(unsigned int ip)
{
    return ((ip & 0xF0000000) == 0xE0000000);
}

static int audio_talk_mcast_membership(int socket, int type, unsigned int mcastip)
{
    struct ip_mreq mreq;

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = htonl(mcastip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socket, IPPROTO_IP, type, (const char *)&mreq, sizeof(mreq)) < 0)
    {
        __ERR("setsockopt multicast failed, ip %#x, type %d, err=%d\n", mcastip, type, errno);
        return -1;
    }

    return 0;
}

static int audio_talk_udp_open(const audio_talk_start_param_t *param, int *sockfd)
{
    struct sockaddr_in local_addr;
    int err;
    int reuse = 1;

    if (param->is_multicast)
    {
        *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (*sockfd < 0)
        {
            __ERR("socket creating err in udptalk\n");
            return -1;
        }

        err = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if (err < 0)
        {
            __ERR("setsockopt reuse failed, err=%d\n", errno);
        }

        if (!audio_talk_is_mcastaddr(param->src_ip))
        {
            __ERR("multiaddr %#x is not multicast IP\n", param->src_ip);
            close(*sockfd);
            *sockfd = -1;
            return -1;
        }

        if (audio_talk_mcast_membership(*sockfd, IP_ADD_MEMBERSHIP, param->src_ip) < 0)
        {
            close(*sockfd);
            *sockfd = -1;
            return -1;
        }

        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons((unsigned short)param->src_port);
    }
    else
    {
        *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (*sockfd < 0)
        {
            __ERR("socket creating err in udptalk\n");
            return -1;
        }

        err = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if (err < 0)
        {
            __ERR("setsockopt reuse failed, err=%d\n", errno);
        }

        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons((unsigned short)param->local_port);
    }

    err = bind(*sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0)
    {
        __ERR("bind udp talk socket failed, err=%d\n", errno);
        if (param->is_multicast)
        {
            audio_talk_mcast_membership(*sockfd, IP_DROP_MEMBERSHIP, param->src_ip);
        }
        close(*sockfd);
        *sockfd = -1;
        return -1;
    }

    return 0;
}

static void audio_talk_udp_close(const audio_talk_start_param_t *param, int sockfd)
{
    if (sockfd < 0)
    {
        return;
    }

    if (param->is_multicast && audio_talk_is_mcastaddr(param->src_ip))
    {
        audio_talk_mcast_membership(sockfd, IP_DROP_MEMBERSHIP, param->src_ip);
    }

    close(sockfd);
}

static void audio_talk_ra_prompt_start(void)
{
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();

    s_ra_answer = mediaCfg->audioConfig.audioCapture.ra_answer;
    s_ra_answered = 0;
    if (s_ra_answer > 0)
    {
        anj_audio_prompt_play(ANJ_MP3_DEFAULT_PATH, ANJ_MP3_DI_DI, 1);
    }
}

static void audio_talk_ra_prompt_stop(void)
{
    if (s_ra_answer > 0)
    {
        anj_audio_play_file_stop();
    }
    s_ra_answer = 0;
    s_ra_answered = 0;
}

void audio_talk_answer_set(int answered)
{
    s_ra_answered = answered ? 1 : 0;
}

static void *audio_talk_udp_thread(void *arg)
{
    audio_talk_start_param_t param = *(audio_talk_start_param_t *)arg;
    int sockfd = -1;
    struct pollfd pfd;
    char recv_buf[AUDIO_TALK_RECV_BUF_SIZE];

    __INFO("talk udp thread start, src=%#x:%d local=%d multicast=%d\n",
           param.src_ip, param.src_port, param.local_port, param.is_multicast);

    if (audio_talk_udp_open(&param, &sockfd) < 0)
    {
        goto endFunc;
    }

    s_talk_sockfd = sockfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;

    while (s_talk_working)
    {
        int nRet = poll(&pfd, 1, 100);
        if (nRet < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            __ERR("poll failed, err=%d\n", errno);
            break;
        }
        else if (nRet == 0)
        {
            continue;
        }

        if (pfd.revents & POLLIN)
        {
            nRet = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, NULL, NULL);
            if (nRet <= 0)
            {
                if (nRet < 0 && (errno == EINTR || errno == EAGAIN))
                {
                    continue;
                }
                break;
            }

            audio_talk_feed_packet(recv_buf, nRet);
        }
    }

endFunc:
    audio_talk_udp_close(&param, sockfd);
    s_talk_sockfd = -1;
    __INFO("talk udp thread exit\n");
    return NULL;
}

int audio_talk_feed_audio(char *data, int length, media_codec_type_e codec_type, int samplerate, int bitrate)
{
    int iRet = 0;
    ANJ_CHK((data != NULL) && (length > 0), -1, "input Invalid");
    (void)bitrate;
    anj_audio_play_data(data, length, 1, codec_type, samplerate);

endFunc:
    return iRet;
}

int audio_talk_feed_packet(char *data, int length)
{
    media_codec_type_e codec_type;
    int samplerate;
    int bitrate;
    char *feed_data = data;
    int feed_len = length;

    if (data == NULL || length <= 0)
    {
        return -1;
    }

    if (s_ra_answer > 0 && !s_ra_answered)
    {
        return 0;
    }

    audio_talk_stream_param_get(&codec_type, &samplerate, &bitrate);

    if (length >= (int)sizeof(RaDataHeader))
    {
        RaDataHeader *pHeader = (RaDataHeader *)data;
        if (pHeader->magic == AJ_RA_MAGIC)
        {
            if (pHeader->samplerate > 0)
            {
                samplerate = pHeader->samplerate;
            }
            feed_data = data + sizeof(RaDataHeader);
            feed_len = length - (int)sizeof(RaDataHeader);
        }
    }

    if (feed_len <= 0)
    {
        return 0;
    }

    return audio_talk_feed_audio(feed_data, feed_len, codec_type, samplerate, bitrate);
}

void audio_talk_stream_param_set(media_codec_type_e codec_type, int samplerate, int bitrate)
{
    anj_mutex_lock(&s_stAudioRecvMutex);
    s_eTalkCodecType = codec_type;
    if (samplerate > 0)
    {
        s_iTalkSampleRate = samplerate;
    }
    if (bitrate > 0)
    {
        s_iTalkBitrate = bitrate;
    }
    anj_mutex_unlock(&s_stAudioRecvMutex);
}

void audio_talk_stream_param_get(media_codec_type_e *codec_type, int *samplerate, int *bitrate)
{
    anj_mutex_lock(&s_stAudioRecvMutex);
    if (codec_type)
    {
        *codec_type = s_eTalkCodecType;
    }
    if (samplerate)
    {
        *samplerate = s_iTalkSampleRate;
    }
    if (bitrate)
    {
        *bitrate = s_iTalkBitrate;
    }
    anj_mutex_unlock(&s_stAudioRecvMutex);
}

void audio_talk_status_set(int status)
{
    anj_mutex_lock(&s_stAudioRecvMutex);
    gstAudioRecvStart = status;
    anj_mutex_unlock(&s_stAudioRecvMutex);
}

int audio_talk_status_get()
{
    int status = 0;
    anj_mutex_lock(&s_stAudioRecvMutex);
    status = gstAudioRecvStart;
    anj_mutex_unlock(&s_stAudioRecvMutex);
    return status;
}

int audio_talk_start(const audio_talk_start_param_t *param)
{
    int iRet = 0;

    ANJ_CHK((param != NULL), -1, "input Invalid");

    audio_talk_stop();
    anj_audio_talk_reset();
    audio_talk_stream_param_set(param->codec_type, param->samplerate, param->bitrate);
    audio_talk_ra_prompt_start();

    s_talk_param = *param;
    s_talk_thread_arg = *param;
    audio_talk_status_set(1);

    if (param->same_port)
    {
        __INFO("talk same port mode, skip udp thread\n");
        return 0;
    }

    s_talk_working = 1;
    if (anj_thread_create(&s_talk_thread, 0, "audio_talk_udp", audio_talk_udp_thread, &s_talk_thread_arg, 0) != 0)
    {
        s_talk_working = 0;
        audio_talk_ra_prompt_stop();
        audio_talk_status_set(0);
        __ERR("Create audio talk udp thread failed!\n");
        return -1;
    }

    __INFO("Reverse Audio Started.\n");
    return 0;

endFunc:
    return iRet;
}

void audio_talk_stop(void)
{
    s_talk_working = 0;

    if (s_talk_sockfd >= 0)
    {
        shutdown(s_talk_sockfd, SHUT_RDWR);
    }

    if (s_talk_thread != 0)
    {
        anj_thread_destroy(s_talk_thread);
        s_talk_thread = 0;
    }

    audio_talk_ra_prompt_stop();
    anj_audio_talk_reset();
    audio_talk_status_set(0);
    memset(&s_talk_param, 0, sizeof(s_talk_param));
    __INFO("Reverse Audio Stopped.\n");
}
