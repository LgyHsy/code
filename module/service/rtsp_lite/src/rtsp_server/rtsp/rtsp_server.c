#include <stdio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include "anj_mw_mem.h"
#include "anj_config.h"
#include "anj_mbuf.h"
#include "media_util.h"
#include "anj_mw_list.h"
#include "rtp.h"
#include "rtsp_stream.h"
#include "rtsp_error.h"
#include "rtsp_auth_check.h"
#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "anj_mw_log.h"
#include "anj_record.h"
#include "audio_receiver.h"
#include "anj_net.h"
#include "project_option.h"

#define RTSP_DEFAULT_PORT					(554)
#define RTSP_UDP_RTP_DEFAULT_PORT			(55532)
#define RTSP_UDP_RTCP_DEFAULT_PORT			(55533)

#define RTSP_SERVER_PORT			RTSP_DEFAULT_PORT
#define RTSP_SERVER_UDP_RTP_PORT  	RTSP_UDP_RTP_DEFAULT_PORT
#define RTSP_SERVER_UDP_RTCP_PORT 	RTSP_UDP_RTCP_DEFAULT_PORT
#define RTSP_MESSAGE_PRESET_SIZE	(65536)

/* Hik/ONVIF backchannel track control names, keep compatible with hik_server-rtsp */
#define VIDEO_TRACK_ID				"trackID=1"
#define AUDIO_TRACK_ID				"trackID=2"
#define AUDIO_BACKCHANNEL_ID		"trackID=5"
#define RTSP_BACKCHANNEL_RTP_HDR_LEN	(12)
#define ANJ_RTSP_ALLNET_CLOSE_SEC  (1 * 3600) /* RTSP直播持续1小时拉流后关闭全网通 */

#define SOCK_CLOSE(fd, act)  \
	do {                     \
		if ((fd) >= 0)       \
		{                    \
			close(fd);       \
			(fd) = -1;       \
		}                    \
		act;                 \
	} while(0)

/*
	NVR预览2路 + CMS预览1路 + CMS设置移动侦测、人形侦测2路 + CMS录像1路
	CMS设置移动侦测、人形侦测 需要2路的原因：移动侦测切换到人形侦测，界面切换后，新连接建立的时候，旧的连接没有断开
*/
#define RTSP_MAX_CLIENT (6)

#define RTSP_CLIENT_RECV_TIMEOUT_MS	(3000) // 接收3秒超时
#define RTSP_CLIENT_SEND_TIMEOUT_MS	(3000) // 发送3秒超时
#define RTSP_CLIENT_SNDBUF_SIZE		(256 * 1024)
#define RTSP_TCP_KEEPIDLE_SEC		(5)    // 5秒后开始发送心跳包
#define RTSP_TCP_KEEPINTVL_SEC		(2)    // 2秒后发送心跳包
#define RTSP_TCP_KEEPCNT			(3)    // 3次心跳包后关闭连接
#define RTSP_MCAST_TTL				(255)

typedef struct rtsp_cfg
{
    int rtsp_enable;
} rtsp_cfg_s;

typedef struct port_cfg
{
    int rtsp_port;
} port_cfg_s;

typedef struct rtsp_client_config
{
	struct list_head node;
	int rtsp_client_fd;
	unsigned int interleaved;
	int b_auth;
	char rtsp_client_ipv4_addr[16];
	char rtsp_session[33];

	anj_thread_s rtsp_reply_thread;
	int live_play_count;
} rtsp_client_config_s;

typedef struct rtsp_server_config
{
	struct list_head rtsp_client_list;
	int rtsp_server_fd;
	unsigned int rtsp_server_port;

	anj_thread_s rtsp_server_thread;
} rtsp_server_config_s;

rtsp_server_config_s *g_rtsp_server_config = NULL;

static unsigned long long s_live_play_start_ms;
static int s_live_play_cnt;

static int _rtsp_backchannel_udp_recv_once(rtp_send_config_s *rtp_send_config);

static void rtsp_allnet_live_add(rtsp_client_config_s *rtsp_client_config)
{
	if (rtsp_client_config->live_play_count == 0)
	{
		rtsp_client_config->live_play_count = 1;
		s_live_play_cnt++;
	}
	s_live_play_start_ms = GetCurrentTimeStampU64();
}

static void rtsp_allnet_live_del(rtsp_client_config_s *rtsp_client_config)
{
	if (rtsp_client_config->live_play_count == 0)
	{
		return;
	}
	rtsp_client_config->live_play_count = 0;
	if (s_live_play_cnt > 0)
	{
		s_live_play_cnt--;
	}
	if (s_live_play_cnt == 0)
	{
		s_live_play_start_ms = 0;
	}
}

/* Called from rtsp accept/select loop; no dedicated watchdog thread. */
static void rtsp_allnet_check_timeout(void)
{
	if (s_live_play_start_ms == 0)
	{
		return;
	}

	unsigned long long now_ms = GetCurrentTimeStampU64();
	int close_sec = ANJ_RTSP_ALLNET_CLOSE_SEC;
	if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
	{
		close_sec *= 4;
	}
	if ((now_ms - s_live_play_start_ms) > (unsigned long long)close_sec * 1000ULL)
	{
		NetworkConfigNew *pNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
		if (pNetworkCfg != NULL && pNetworkCfg->lanCfg.onvifAllnetEnable != 0)
		{
			pNetworkCfg->lanCfg.onvifAllnetEnable = 0;
			if (anj_config_network_save(pNetworkCfg) != 0)
			{
				__ERR("save onvif allnet close failed\n");
			}
			s_live_play_start_ms = 0;
			__INFO("rtsp live play timeout, close onvif allnet, thresh=%d sec\n", close_sec);
		}
	}
}

static inline int port_is_valid(int port) { return port > 0 && port <= 65535; }
static inline int sk_is_valid(int sk) { return sk >= 0; }

static inline char *strstri(const char *haystack, const char *needle)
{
	if (!haystack || !needle) return NULL;
	while (*haystack) {
		const char *h = haystack;
		const char *n = needle;
		while (*h && *n && tolower(*h) == tolower(*n)) { h++; n++; }
		if (!*n) return (char *)haystack;
		haystack++;
	}
	return NULL;
}

static inline int create_server_socket(int type, int snd_buf, int rcv_buf, int reuse, int port, int backlog, int block, int nodelay, int broadcast, int ttl, int tos, int prio)
{
	int sock = socket(AF_INET, type, 0);
	if (sock < 0) return -1;
	if (reuse) { int opt = 1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)); }
	struct sockaddr_in addr_in;
	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons((unsigned short)port);
	addr_in.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) { close(sock); return -1; }
	if (type == SOCK_STREAM && listen(sock, backlog) < 0) { close(sock); return -1; }
	return sock;
}

static void _log_supported_rtsp_urls(void)
{
	char ipv4_addr[16] = {0};
	const char *server_ip = "127.0.0.1";

	if (g_rtsp_server_config != NULL
		&& g_rtsp_server_config->rtsp_server_fd >= 0)
	{
		struct in_addr in;
		in.s_addr = get_my_ipaddr();
		if (in.s_addr != INADDR_NONE && in.s_addr != 0
			&& inet_ntop(AF_INET, &in, ipv4_addr, sizeof(ipv4_addr)) != NULL
			&& ipv4_addr[0] != '\0')
		{
			server_ip = ipv4_addr;
		}
	}
	unsigned int rtsp_server_port = g_rtsp_server_config->rtsp_server_port;

	__INFO("rtsp_lite started, support pull urls:\n");
	__INFO("  rtsp://%s:%u/stream0 , rtsp://%s:%u/stream1\n", server_ip, rtsp_server_port, server_ip, rtsp_server_port);
	__INFO("  rtsp://%s:%u/mpeg4 , rtsp://%s:%u/mpeg4cif\n", server_ip, rtsp_server_port, server_ip, rtsp_server_port);
	__INFO("  rtsp://%s:%u/h264 , rtsp://%s:%u/h264cif\n", server_ip, rtsp_server_port, server_ip, rtsp_server_port);
	__INFO("  rtsp://%s:%u/Streaming/Channels/101 , rtsp://%s:%u/Streaming/Channels/102\n",
		   server_ip, rtsp_server_port, server_ip, rtsp_server_port);
	__INFO("  rtsp://%s:%u/mcast1 , rtsp://%s:%u/mcast2\n", server_ip, rtsp_server_port, server_ip, rtsp_server_port);
	__INFO("  rtsp://%s:%u/replay/chn<channel>/<start_time>\n", server_ip, rtsp_server_port);
	// ____INFO("  rtsp://%s:%u/ch0/mpeg4 , rtsp://%s:%u/ch0/mpeg4cif", server_ip, rtsp_server_port,server_ip, rtsp_server_port);
	// ____INFO("  rtsp://%s:%u/ch1/mpeg4 , rtsp://%s:%u/ch1/mpeg4cif", server_ip, rtsp_server_port,server_ip, rtsp_server_port);
}

static int ipc_config_get_rtsp_cfg(rtsp_cfg_s *rtsp_cfg)
{
    MediaStreamConfig *media_stream_cfg;

    if (rtsp_cfg == NULL)
    {
        return -1;
    }

    media_stream_cfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (media_stream_cfg == NULL)
    {
        return -1;
    }

    memset(rtsp_cfg, 0, sizeof(*rtsp_cfg));
    rtsp_cfg->rtsp_enable = media_stream_cfg->rtspConfig.enable_rtsp;
    return 0;
}

static int ipc_config_get_port_cfg(port_cfg_s *port_cfg)
{
    MediaStreamConfig *media_stream_cfg;

    if (port_cfg == NULL)
    {
        return -1;
    }

    media_stream_cfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (media_stream_cfg == NULL)
    {
        return -1;
    }

    memset(port_cfg, 0, sizeof(*port_cfg));
    port_cfg->rtsp_port = media_stream_cfg->rtspConfig.videoPort;
    return 0;
}

static int _parse_replay_url(const char *url, unsigned int *channel, unsigned int *start_time)
{
	const char *path = NULL;
	unsigned int ch = 0;
	unsigned int ts = 0;

	_NULL_POINTER_CHECK_(url, -1);
	_NULL_POINTER_CHECK_(channel, -1);
	_NULL_POINTER_CHECK_(start_time, -1);

	path = strstr(url, "://");
	if (path != NULL)
	{
		path = strchr(path + 3, '/');
	}
	else
	{
		path = url;
	}
	_NULL_POINTER_CHECK_(path, -1);

	if (sscanf(path, "/replay/chn%u/%u", &ch, &ts) != 2)
	{
		return -1;
	}
	if (ch >= REC_MAX_CH_NUM || ts == 0)
	{
		return -1;
	}

	*channel = ch;
	*start_time = ts;
	return 0;
}

static int _rtsp_url_wants_mcast(const char *url)
{
	if (url == NULL)
	{
		return 0;
	}
	if (strstr(url, "/mcast1") != NULL || strstr(url, "/mcast2") != NULL)
	{
		return 1;
	}
	if (strstri(url, "transportmode=multicast") != NULL)
	{
		return 1;
	}
	return 0;
}

/* Fill dest IP/ports from multicastConfig. 0=ok, -1=disabled or invalid. */
static int _rtsp_mcast_apply(rtp_send_config_s *cfg, const char *url)
{
	MediaStreamConfig *stream_cfg = NULL;
	MulticastStruct *mc = NULL;
	struct in_addr ia;
	int idx = 0;
	char ip_str[32] = {0};

	_NULL_POINTER_CHECK_(cfg, -1);

	if (url != NULL && strstr(url, "/mcast1") != NULL)
	{
		cfg->stream_index = MAIN_STREAM;
		cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
		cfg->hik_private_url = 1;
	}
	else if (url != NULL && strstr(url, "/mcast2") != NULL)
	{
		cfg->stream_index = SUB_STREAM;
		cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
		cfg->hik_private_url = 1;
	}

	idx = (cfg->stream_index == SUB_STREAM) ? SUB_STREAM : MAIN_STREAM;
	stream_cfg = (MediaStreamConfig *)getMediaStreamConfig();
	if (stream_cfg == NULL)
	{
		return -1;
	}
	mc = &stream_cfg->multicastConfig.StreamMulticast[idx];
	if (mc->enable == 0 || mc->Ip == 0 || mc->Port == 0)
	{
		return -1;
	}
	if ((mc->Ip & 0xf0000000U) != 0xe0000000U)
	{
		return -1;
	}

	ia.s_addr = htonl(mc->Ip);
	if (inet_ntop(AF_INET, &ia, ip_str, sizeof(ip_str)) == NULL)
	{
		return -1;
	}

	cfg->is_multicast = 1;
	snprintf(cfg->rtp_over_udp_ip, sizeof(cfg->rtp_over_udp_ip), "%s", ip_str);
	cfg->rtp_client_video_over_udp_port = mc->Port;
	cfg->rtcp_client_video_over_udp_port = mc->Port + 1;
	cfg->rtp_client_audio_over_udp_port = (mc->Port >= 2) ? (mc->Port - 2) : mc->Port;
	cfg->rtcp_client_audio_over_udp_port = cfg->rtp_client_audio_over_udp_port + 1;
	return 0;
}

/* Local IPv4 for IP_MULTICAST_IF: prefer RTSP accept iface, else anj_net_ip_get. */
static void _rtsp_mcast_local_ip_get(int rtsp_client_fd, char *ip_buf, size_t ip_buf_len)
{
	struct sockaddr_in local_addr;
	socklen_t addr_len = sizeof(local_addr);

	if (ip_buf == NULL || ip_buf_len == 0)
	{
		return;
	}
	ip_buf[0] = '\0';

	memset(&local_addr, 0, sizeof(local_addr));
	if (rtsp_client_fd >= 0 &&
		getsockname(rtsp_client_fd, (struct sockaddr *)&local_addr, &addr_len) == 0 &&
		local_addr.sin_family == AF_INET &&
		local_addr.sin_addr.s_addr != INADDR_ANY &&
		local_addr.sin_addr.s_addr != INADDR_NONE)
	{
		if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_buf, ip_buf_len) != NULL)
		{
			return;
		}
		ip_buf[0] = '\0';
	}

	anj_net_ip_get(ip_buf, (int)ip_buf_len);
}

/*
 * Hik URL channel id rules (do NOT copy legacy ==1/==2 else->main bug):
 *   1 / 101 -> MAIN, 2 / 102 -> SUB
 *   id>=100: cam=id/100, stream_digit=id%10
 * TCL12Q v1: only cam==1
 */
static int _parse_hik_channel_id(int id, int *stream_index)
{
	int cam = 1;
	int stream_digit = 0;

	_NULL_POINTER_CHECK_(stream_index, -1);
	if (id <= 0)
	{
		return -1;
	}

	if (id < 100)
	{
		cam = 1;
		stream_digit = id;
	}
	else
	{
		cam = id / 100;
		stream_digit = id % 10;
	}

	if (cam != 1)
	{
		return -1;
	}
	if (stream_digit == 1)
	{
		*stream_index = MAIN_STREAM;
		return 0;
	}
	if (stream_digit == 2)
	{
		*stream_index = SUB_STREAM;
		return 0;
	}
	return -1;
}

static int _parse_live_url_hik_alias(const char *url, rtp_send_config_s *cfg)
{
	char lower[256] = {0};
	const char *p = NULL;
	int id = 0;
	int stream_index = MAIN_STREAM;
	size_t i = 0;
	size_t n = 0;

	_NULL_POINTER_CHECK_(url, -1);
	_NULL_POINTER_CHECK_(cfg, -1);

	n = strlen(url);
	if (n >= sizeof(lower))
	{
		n = sizeof(lower) - 1;
	}
	for (i = 0; i < n; i++)
	{
		lower[i] = (char)tolower((unsigned char)url[i]);
	}

	p = strstr(lower, "streaming/channels/");
	if (p != NULL && sscanf(p, "streaming/channels/%d", &id) == 1)
	{
		if (_parse_hik_channel_id(id, &stream_index) == 0)
		{
			cfg->stream_index = stream_index;
			cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			cfg->hik_private_url = 1;
			return 0;
		}
		return -1;
	}

	p = strstr(lower, "psia/streaming/channels/");
	if (p != NULL && sscanf(p, "psia/streaming/channels/%d", &id) == 1)
	{
		if (_parse_hik_channel_id(id, &stream_index) == 0)
		{
			cfg->stream_index = stream_index;
			cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			cfg->hik_private_url = 1;
			return 0;
		}
		return -1;
	}

	p = strstr(lower, "media/chn/");
	if (p != NULL && sscanf(p, "media/chn/%d", &id) == 1)
	{
		if (_parse_hik_channel_id(id, &stream_index) == 0)
		{
			cfg->stream_index = stream_index;
			cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			cfg->hik_private_url = 1;
			return 0;
		}
		return -1;
	}

	/* Strict Hik path: /ch1/main[/...] or /ch1/sub[/...] (not loose "ch"+"main") */
	p = strstr(lower, "/ch");
	if (p != NULL)
	{
		int ch = 0;
		char role[32] = {0};

		if (sscanf(p, "/ch%d/%31[^/?]", &ch, role) == 2 && ch == 1)
		{
			if (strcmp(role, "main") == 0)
			{
				cfg->stream_index = MAIN_STREAM;
				cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
				cfg->hik_private_url = 1;
				return 0;
			}
			if (strcmp(role, "sub") == 0)
			{
				cfg->stream_index = SUB_STREAM;
				cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
				cfg->hik_private_url = 1;
				return 0;
			}
		}
	}

	if (strstr(lower, "/h264cif") != NULL)
	{
		cfg->stream_index = SUB_STREAM;
		cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
		cfg->hik_private_url = 1;
		return 0;
	}
	if (strstr(lower, "/h264") != NULL)
	{
		cfg->stream_index = MAIN_STREAM;
		cfg->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
		cfg->hik_private_url = 1;
		return 0;
	}

	return -1;
}

static int _rtsp_server_fd_init(int *rtsp_server_fd, unsigned int port)
{
	_NULL_POINTER_CHECK_(rtsp_server_fd, -1);

	/**
	 * send_buf和recv_buf大小参考_rtsp_reply_process里的recv_buf和rtsp_reply_message，同时注意要比MAIN_STREAM_SOCKET_SENDBUFF_SIZE大，因为tcp模式下这个socket会直接复用到rtp中
	 */
	*rtsp_server_fd = create_server_socket(SOCK_STREAM, RTSP_MESSAGE_PRESET_SIZE * 2, RTSP_MESSAGE_PRESET_SIZE * 2, 1, port, RTSP_MAX_CLIENT, 1, 0, 0, 0, 0, 0);
	return sk_is_valid(*rtsp_server_fd) ? 0 : -1;
}

static int _rtsp_client_count(struct list_head *p_rtsp_client_list)
{
	struct list_head *pos = NULL;
	int count = 0;

	_NULL_POINTER_CHECK_(p_rtsp_client_list, 0);

	list_for_each(pos, p_rtsp_client_list)
	{
		count++;
	}

	return count;
}

static rtsp_client_config_s *_get_rtsp_client_from_list(struct list_head *p_rtsp_client_list, int client_fd)
{
	_NULL_POINTER_CHECK_(p_rtsp_client_list, NULL);
	if (!sk_is_valid(client_fd))
	{
		__ERR("socket is invalid!(client_fd=%d)", client_fd);
		return NULL;
	}

	struct list_head *pos = NULL;
	list_for_each(pos, p_rtsp_client_list)
	{
		rtsp_client_config_s *p_tmp_rtsp_client_config = list_entry(pos, rtsp_client_config_s, node);
		if (p_tmp_rtsp_client_config->rtsp_client_fd == client_fd)
		{
			return p_tmp_rtsp_client_config;
		}
	}

	return NULL;
}

static int _rtsp_client_manage(struct list_head *p_rtsp_client_list, rtsp_client_config_s *p_rtsp_client_config, int b_add)
{
	_NULL_POINTER_CHECK_(p_rtsp_client_list, -1);
	_NULL_POINTER_CHECK_(p_rtsp_client_config, -1);

	if (b_add)
	{
		rtsp_client_config_s *p_new_rtsp_client_config = NULL;

		p_new_rtsp_client_config = (rtsp_client_config_s *)anj_mw_malloc(sizeof(rtsp_client_config_s));
		if (p_new_rtsp_client_config == NULL)
		{
			__ERR("anj_mw_malloc %zu failed", sizeof(rtsp_client_config_s));
			return -1;
		}
		memcpy(p_new_rtsp_client_config, p_rtsp_client_config, sizeof(rtsp_client_config_s));
		INIT_LIST_HEAD(&p_new_rtsp_client_config->node);
		list_add_tail(&p_new_rtsp_client_config->node, p_rtsp_client_list);
		return 0;
	}
	else
	{
		struct list_head *pos = NULL;
		struct list_head *n = NULL;

		list_for_each_safe(pos, n, p_rtsp_client_list)
		{
			rtsp_client_config_s *p_tmp_rtsp_client_config = list_entry(pos, rtsp_client_config_s, node);
			if (p_tmp_rtsp_client_config->rtsp_client_fd == p_rtsp_client_config->rtsp_client_fd)
			{
				if (p_tmp_rtsp_client_config->rtsp_reply_thread.start != 0
					&& anj_thread_task_destroy(&p_tmp_rtsp_client_config->rtsp_reply_thread, -1) < 0)
				{
					__ERR("anj_thread_task_destroy failed!");
				}
				list_del(&p_tmp_rtsp_client_config->node);
				anj_mw_free(p_tmp_rtsp_client_config);
				p_tmp_rtsp_client_config = NULL;
				return 0;
			}
		}
	}

	__ERR("p_rtsp_client_config is not in the list, fd=%d", p_rtsp_client_config->rtsp_client_fd);

	return -1;
}

static rtsp_status_code_e _rtsp_message_method_parse(char *p_rtsp_message, int rtsp_message_length, unsigned int *p_rtsp_message_length,
													 char **method, char **url, char **version)
{
	_NULL_POINTER_CHECK_(p_rtsp_message, RTSP_STATUS_BAD_REQUEST);
	if (rtsp_message_length <= 0)
	{
		__ERR("rtsp_message_length is invalid!(%lu)", (unsigned long)rtsp_message_length);
		return RTSP_STATUS_BAD_REQUEST;
	}
	if (p_rtsp_message_length == NULL || method == NULL || url == NULL || version == NULL)
	{
		__ERR("rtsp message output param is null!");
		return RTSP_STATUS_BAD_REQUEST;
	}

	*p_rtsp_message_length = 0;
	int i = 0;
	while (i < (rtsp_message_length - 1))
	{
		if ((p_rtsp_message[i] == '\r') && (p_rtsp_message[i + 1] == '\n'))
		{
			p_rtsp_message[i] = '\0';
			p_rtsp_message[i + 1] = '\0';
			*p_rtsp_message_length = i + 2;
			break;
		}

		i++;
	}

	if (*p_rtsp_message_length <= 0)
	{
		return RTSP_STATUS_IGNORE;
	}

	char *request_line = p_rtsp_message;

	*method = strsep(&request_line, " ");
	*url = strsep(&request_line, " ");
	*version = strsep(&request_line, " ");

	if (*method == NULL || *url == NULL || *version == NULL)
	{
		__ERR("The request line is not formatted correctly");
		if (*method == NULL)
		{
			__ERR("method is null!");
		}
		if (*url == NULL)
		{
			__ERR("url is null!");
		}
		if (*version == NULL)
		{
			__ERR("version is null!");
		}
		return RTSP_STATUS_IGNORE;
	}

	return RTSP_STATUS_OK;
}

static int _get_rtsp_value(char *p_rtsp_message, char *name, char *value, unsigned int value_length)
{
	return get_http_hdr_value(p_rtsp_message, name, value, value_length);
}

static int _rtsp_message_cseq_parse(char *p_rtsp_message, unsigned int rtsp_message_length, int *cseq)
{
	_NULL_POINTER_CHECK_(p_rtsp_message, -1);
	if (rtsp_message_length == 0)
	{
		__ERR("rtsp_message_length is invalid!(%lu)", (unsigned long)rtsp_message_length);
		return -1;
	}
	_NULL_POINTER_CHECK_(cseq, -1);

	char message_cseq[32];
	memset(message_cseq, 0, sizeof(message_cseq));

	if (_get_rtsp_value(p_rtsp_message, "CSeq: ", message_cseq, sizeof(message_cseq)) != 0)
	{
		__ERR("_get_rtsp_value failed!");
		return -1;
	}
	*cseq = atoi(message_cseq);

	return 0;
}

static int _rtsp_message_transport_parse(char *p_rtsp_message, unsigned int rtsp_message_length,
										 rtp_send_type_e *rtp_send_type, int *rtp_over_udp_port, int *rtcp_over_udp_port,
										 int allow_no_client_port)
{
	_NULL_POINTER_CHECK_(p_rtsp_message, -1);
	if (rtsp_message_length == 0)
	{
		__ERR("rtsp_message_length is invalid!(%lu)", (unsigned long)rtsp_message_length);
		return -1;
	}
	if (rtp_send_type == NULL || rtp_over_udp_port == NULL || rtcp_over_udp_port == NULL)
	{
		__ERR("transport parse output param is null!");
		return -1;
	}

	char message_transport[128];
	memset(message_transport, 0, sizeof(message_transport));

	if (_get_rtsp_value(p_rtsp_message, "Transport: ", message_transport, sizeof(message_transport)) != 0)
	{
		__ERR("_get_rtsp_value failed! Can't find Transport");
		return -1;
	}

	char *transport = message_transport;

	if (strstr(transport, "RTP/AVP/TCP"))
	{
		*rtp_send_type = RTP_OVER_TCP;
	}
	else if (strstr(transport, "RTP/AVP"))
	{
		*rtp_send_type = RTP_OVER_UDP;

		char *filed_name = NULL;
		filed_name = strsep(&transport, ";");
		while (filed_name != NULL)
		{
			if (memcmp(filed_name, "client_port=", strlen("client_port=")) == 0)
			{
				if (sscanf(filed_name + strlen("client_port="), "%d-%d", rtp_over_udp_port, rtcp_over_udp_port) != 2)
				{
					__ERR("sscanf failed, filed_name %s", filed_name + strlen("client_port="));
					return -1;
				}

				break;
			}
			filed_name = strsep(&transport, ";");
		}

		if (filed_name == NULL && !allow_no_client_port)
		{
			__ERR("UDP port get failed, %s", transport);
			return -1;
		}
	}
	else
	{
		__ERR("parse failed, %s", p_rtsp_message);
		return -1;
	}

	return 0;
}

static int _check_session_valid(char *p_rtsp_message, char *session)
{
	_NULL_POINTER_CHECK_(p_rtsp_message, -1);
	_NULL_POINTER_CHECK_(session, -1);

	char message_session[128];
	memset(message_session, 0, sizeof(message_session));

	if (_get_rtsp_value(p_rtsp_message, "Session: ", message_session, sizeof(message_session)) != 0)
	{
		__ERR("_get_rtsp_value failed! Can't find Session");
		return -1;
	}
	if (strcmp(session, message_session) == 0)
	{
		return 0;
	}
	else
	{
		__ERR("session no match message=[%s], session=[%s]", message_session, session);
	}
	return -1;
}

static int _rtsp_url_is_backchannel(const char *url)
{
	if (url == NULL)
	{
		return 0;
	}
	return (strstri(url, AUDIO_BACKCHANNEL_ID) != NULL) || (strstri(url, "backchannel") != NULL);
}

static int _rtsp_url_is_video_track(const char *url)
{
	if (url == NULL)
	{
		return 0;
	}
	return (strstr(url, "video_stream") != NULL) || (strstri(url, VIDEO_TRACK_ID) != NULL);
}

static int _rtsp_url_is_audio_track(const char *url)
{
	if (url == NULL || _rtsp_url_is_backchannel(url))
	{
		return 0;
	}
	return (strstr(url, "audio_stream") != NULL) || (strstri(url, AUDIO_TRACK_ID) != NULL);
}

static int _rtsp_message_has_backchannel_require(char *p_rtsp_message)
{
	char require[128] = {0};

	if (p_rtsp_message == NULL)
	{
		return 0;
	}
	if (_get_rtsp_value(p_rtsp_message, "Require: ", require, sizeof(require)) != 0)
	{
		return 0;
	}
	return (strstri(require, "backchannel") != NULL) ? 1 : 0;
}

static int _rtsp_backchannel_talk_start(rtp_send_config_s *rtp_send_config)
{
	audio_talk_start_param_t talk_param;
	int sample_rate;

	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	if (rtp_send_config->backchannel_talk_started)
	{
		return 0;
	}

	sample_rate = rtp_send_config->av_param.audio_sample_rate > 0 ?
				  rtp_send_config->av_param.audio_sample_rate : 8000;

	memset(&talk_param, 0, sizeof(talk_param));
	talk_param.codec_type = MEDIA_CODEC_AUDIO_G711A;
	talk_param.samplerate = sample_rate;
	talk_param.bitrate = 64000;
	/* TCP interleaved and UDP-over-existing rtp_fd both feed via audio_talk_feed_audio */
	talk_param.same_port = 1;

	if (audio_talk_start(&talk_param) != 0)
	{
		__ERR("audio_talk_start failed for rtsp backchannel");
		return -1;
	}

	rtp_send_config->backchannel_talk_started = 1;
	__INFO("rtsp backchannel talk started, sample_rate=%d", sample_rate);
	return 0;
}

static void _rtsp_backchannel_talk_stop(rtp_send_config_s *rtp_send_config)
{
	if (rtp_send_config == NULL || !rtp_send_config->backchannel_talk_started)
	{
		return;
	}
	audio_talk_stop();
	rtp_send_config->backchannel_talk_started = 0;
	rtp_send_config->backchannel_enable = 0;
	__INFO("rtsp backchannel talk stopped");
}

static int _rtsp_backchannel_feed_rtp_payload(const char *payload, int payload_len, unsigned char pt, int sample_rate)
{
	media_codec_type_e codec_type;

	if (payload == NULL || payload_len <= 0)
	{
		return -1;
	}

	/* CN/DTMF */
	if (pt == 13 || pt == 101)
	{
		return 0;
	}

	if (pt == RTP_PAYLOAD_TYPE_G711A)
	{
		codec_type = MEDIA_CODEC_AUDIO_G711A;
	}
	else if (pt == RTP_PAYLOAD_TYPE_G711U)
	{
		codec_type = MEDIA_CODEC_AUDIO_G711U;
	}
	else
	{
		__DBG("ignore backchannel payload type %u", pt);
		return 0;
	}

	if (sample_rate <= 0)
	{
		sample_rate = 8000;
	}

	return audio_talk_feed_audio((char *)payload, payload_len, codec_type, sample_rate, 0);
}

/* Parse TCP interleaved ($ + ch + len + RTP) and optional bare RTP (0x80..) for talkback */
static int _rtsp_backchannel_parse_talk_pack(rtp_send_config_s *rtp_send_config, char *buf, int buflen)
{
	int read_pos = 0;
	int remain_len;
	int sample_rate;

	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(buf, -1);
	if (buflen <= 0)
	{
		return -1;
	}

	sample_rate = rtp_send_config->av_param.audio_sample_rate > 0 ?
				  rtp_send_config->av_param.audio_sample_rate : 8000;
	remain_len = buflen;

	while (read_pos < buflen && remain_len > RTSP_BACKCHANNEL_RTP_HDR_LEN)
	{
		if (buf[read_pos] == '$')
		{
			unsigned char channel;
			unsigned short rtp_len;
			char *rtp_hdr;
			char *payload;
			int payload_len;
			unsigned char pt;

			if (remain_len < 4 + RTSP_BACKCHANNEL_RTP_HDR_LEN)
			{
				break;
			}

			channel = (unsigned char)buf[read_pos + 1];
			memcpy(&rtp_len, buf + read_pos + 2, sizeof(rtp_len));
			rtp_len = ntohs(rtp_len);

			if ((int)(rtp_len + 4) > remain_len)
			{
				__DBG("backchannel interleaved incomplete, rtp_len=%u remain=%d", rtp_len, remain_len);
				break;
			}

			/* Only feed audio RTP channel; ignore RTCP (odd channel) and other tracks */
			if (rtp_send_config->backchannel_enable &&
				channel == (unsigned char)rtp_send_config->rtp_backchannel_channel &&
				rtp_len > RTSP_BACKCHANNEL_RTP_HDR_LEN)
			{
				rtp_hdr = buf + read_pos + 4;
				payload = rtp_hdr + RTSP_BACKCHANNEL_RTP_HDR_LEN;
				payload_len = rtp_len - RTSP_BACKCHANNEL_RTP_HDR_LEN;
				pt = ((unsigned char)rtp_hdr[1]) & 0x7f;
				_rtsp_backchannel_feed_rtp_payload(payload, payload_len, pt, sample_rate);
			}

			read_pos += 4 + rtp_len;
			remain_len = buflen - read_pos;
		}
		else if (((unsigned char)buf[read_pos] & 0xc0) == 0x80)
		{
			/* Bare RTP without interleaved header (some NVR clients) */
			if (remain_len <= RTSP_BACKCHANNEL_RTP_HDR_LEN)
			{
				break;
			}
			if (rtp_send_config->backchannel_enable)
			{
				unsigned char pt = ((unsigned char)buf[read_pos + 1]) & 0x7f;
				_rtsp_backchannel_feed_rtp_payload(buf + read_pos + RTSP_BACKCHANNEL_RTP_HDR_LEN,
												  remain_len - RTSP_BACKCHANNEL_RTP_HDR_LEN,
												  pt, sample_rate);
			}
			break;
		}
		else
		{
			break;
		}
	}

	return read_pos;
}

static int _options_reply_message_get(char *options_reply_message, unsigned int options_reply_message_length, int cseq)
{
	_NULL_POINTER_CHECK_(options_reply_message, -1);
	if (options_reply_message_length == 0)
	{
		__ERR("options_reply_message_length is invalid!(%lu)", (unsigned long)options_reply_message_length);
		return -1;
	}

	snprintf(options_reply_message, options_reply_message_length, "RTSP/1.0 %s\r\n"
		"CSeq: %d\r\n"
		"Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER\r\n"
		"\r\n",
		rtsp_get_status_msg(RTSP_STATUS_OK),
		cseq);
	return 0;
}

static int _describe_reply_message_get(char *describe_reply_message, unsigned int describe_reply_message_length,
									   int cseq, rtp_send_config_s *rtp_send_config, const char *url)
{
	_NULL_POINTER_CHECK_(describe_reply_message, -1);
	if (describe_reply_message_length == 0)
	{
		__ERR("describe_reply_message_length is invalid!(%lu)", (unsigned long)describe_reply_message_length);
		return -1;
	}
	_NULL_POINTER_CHECK_(rtp_send_config, -1);

	char ipv4_addr[16] = {0};
	struct in_addr in;
	in.s_addr = get_my_ipaddr();
	if (in.s_addr == INADDR_NONE || in.s_addr == 0)
	{
		__ERR("get_my_ipaddr failed!");
		return -1;
	}
	if (inet_ntop(AF_INET, &in, ipv4_addr, sizeof(ipv4_addr)) == NULL)
	{
		__ERR("inet_ntop failed!");
		return -1;
	}

	char c_line[64] = {0};
	if (rtp_send_config->is_multicast && rtp_send_config->rtp_over_udp_ip[0] != '\0')
	{
		snprintf(c_line, sizeof(c_line), "c=IN IP4 %s/255\r\n", rtp_send_config->rtp_over_udp_ip);
	}
	else
	{
		snprintf(c_line, sizeof(c_line), "c=IN IP4 0.0.0.0\r\n");
	}

	/* Extract stream path from URL for Content-Base (e.g. "/stream0" from "rtsp://host:port/stream0?...") */
	char content_base_path[128] = {0};
	if (url)
	{
		const char *path_start = NULL;
		const char *schema = strstr(url, "://");
		if (schema)
		{
			path_start = strchr(schema + 3, '/');
		}
		else
		{
			path_start = url;
		}
		if (path_start)
		{
			const char *path_end = strchr(path_start, '?');
			int path_len = path_end ? (int)(path_end - path_start) : (int)strlen(path_start);
			if (path_len > 0 && path_len < (int)sizeof(content_base_path))
			{
				strncpy(content_base_path, path_start, path_len);
			}
		}
	}

	char video_sdp_buf[768] = {0};
	char audio_sdp_buf[512] = {0};
	char backchannel_sdp_buf[256] = {0};
	if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264 ||
		rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
	{
		if (rtsp_stream_key_frame_parse(rtp_send_config) != 0)
		{
			__ERR("rtsp_stream_key_frame_parse failed! \n");
		}
	}

	/* Live / Hik / talkback: trackID=1/2 + Media_header (matches hik_server-rtsp SDP).
	 * Only keep video_stream/audio_stream for non-live paths that never set hik_private_url. */
	const char *play_dir = rtp_send_config->backchannel_requested ? "a=recvonly\r\n" : "";
	int use_track_id = (rtp_send_config->backchannel_requested || rtp_send_config->hik_private_url);
	const char *video_ctrl = use_track_id ? VIDEO_TRACK_ID : "video_stream";
	const char *audio_ctrl = use_track_id ? AUDIO_TRACK_ID : "audio_stream";
	/* MEDIAINFO from old hik_server-rtsp; append after audio track, not before m=video */
	const char *hik_extra = rtp_send_config->hik_private_url
		? "a=Media_header:MEDIAINFO=494D4B48010100000400050010710110401F000000FA000000000000000000000000000000000000;\r\n"
		  "a=appversion:1.0\r\n"
		: "";

	if (rtp_send_config->rtp_payload_type == SINGLE_COMPOSITE_PS_STREAM)
	{
		snprintf(video_sdp_buf, sizeof(video_sdp_buf),
				 "v=0\r\n"
				 "o=- 9%ld 1 IN IP4 %s\r\n"
				 "s=RTSP/RTP stream\r\n"
				 "t=0 0\r\n"
				 "a=control:*\r\n"
				 "a=range:npt=0-\r\n"
				 "m=video 0 RTP/AVP %d\r\n"
				 "%s"
				 "a=rtpmap:%d MP2P/90000\r\n"
				 "a=control:program_stream\r\n"
				 "%s",
				 time(NULL), ipv4_addr, RTP_PAYLOAD_TYPE_PS, c_line, RTP_PAYLOAD_TYPE_PS, play_dir);
	}
	else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
	{
		char fmtp_buf[256] = {0};
		if (rtp_send_config->sprop_sps_[0] != '\0' && rtp_send_config->sprop_pps_[0] != '\0')
		{
			snprintf(fmtp_buf, sizeof(fmtp_buf),
					 "a=fmtp:%d packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s,%s\r\n",
					 RTP_PAYLOAD_TYPE_H264, rtp_send_config->profileid_, rtp_send_config->sprop_sps_, rtp_send_config->sprop_pps_);
		}
		else
		{
			snprintf(fmtp_buf, sizeof(fmtp_buf),
					 "a=fmtp:%d packetization-mode=1;profile-level-id=%s\r\n",
					 RTP_PAYLOAD_TYPE_H264, rtp_send_config->profileid_);
		}
		snprintf(video_sdp_buf, sizeof(video_sdp_buf),
				 "v=0\r\n"
				 "o=- 9%ld 1 IN IP4 %s\r\n"
				 "s=RTSP/RTP stream\r\n"
				 "t=0 0\r\n"
				 "a=control:*\r\n"
				 "a=range:npt=0-\r\n"
				 "m=video 0 RTP/AVP %d\r\n"
				 "%s"
				 "a=rtpmap:%d H264/90000\r\n"
				 "%s"
				 "a=x-dimensions: %d, %d\r\n"
				 "a=x-framerate: %d\r\n"
				 "a=control:%s\r\n"
				 "%s",
				 time(NULL), ipv4_addr, RTP_PAYLOAD_TYPE_H264, c_line, RTP_PAYLOAD_TYPE_H264,
				 fmtp_buf,
				 rtp_send_config->av_param.resolution_width, rtp_send_config->av_param.resolution_height, rtp_send_config->av_param.frame_rate,
				 video_ctrl, play_dir);
	}
	else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
	{
		char fmtp_buf[256] = {0};
		if (rtp_send_config->sprop_vps_[0] != '\0' && rtp_send_config->sprop_sps_[0] != '\0' && rtp_send_config->sprop_pps_[0] != '\0')
		{
			snprintf(fmtp_buf, sizeof(fmtp_buf),
					 "a=fmtp:%d sprop-vps=%s;sprop-sps=%s;sprop-pps=%s\r\n",
					 RTP_PAYLOAD_TYPE_H265, rtp_send_config->sprop_vps_, rtp_send_config->sprop_sps_, rtp_send_config->sprop_pps_);
		}
		else
		{
			fmtp_buf[0] = '\0';
		}
		snprintf(video_sdp_buf, sizeof(video_sdp_buf),
				 "v=0\r\n"
				 "o=- 9%ld 1 IN IP4 %s\r\n"
				 "s=RTSP/RTP stream\r\n"
				 "t=0 0\r\n"
				 "a=control:*\r\n"
				 "a=range:npt=0-\r\n"
				 "m=video 0 RTP/AVP %d\r\n"
				 "%s"
				 "a=rtpmap:%d H265/90000\r\n"
				 "%s"
				 "a=x-dimensions: %d, %d\r\n"
				 "a=x-framerate: %d\r\n"
				 "a=control:%s\r\n"
				 "%s",
				 time(NULL), ipv4_addr, RTP_PAYLOAD_TYPE_H265, c_line, RTP_PAYLOAD_TYPE_H265,
				 fmtp_buf,
				 rtp_send_config->av_param.resolution_width, rtp_send_config->av_param.resolution_height, rtp_send_config->av_param.frame_rate,
				 video_ctrl, play_dir);
	}
	else if (rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
	{
		snprintf(video_sdp_buf, sizeof(video_sdp_buf),
				 "v=0\r\n"
				 "o=- 9%ld 1 IN IP4 %s\r\n"
				 "s=RTSP/RTP stream\r\n"
				 "t=0 0\r\n"
				 "a=control:*\r\n"
				 "a=range:npt=0-\r\n"
				 "m=video 0 RTP/AVP %d\r\n"
				 "%s"
				 "a=rtpmap:%d JPEG/90000\r\n"
				 "a=framesize:%d %d-%d\r\n"
				 "a=x-dimensions: %d, %d\r\n"
				 "a=x-framerate: %d\r\n"
				 "a=control:%s\r\n"
				 "%s",
				 time(NULL), ipv4_addr, RTP_PAYLOAD_TYPE_MJPG, c_line, RTP_PAYLOAD_TYPE_MJPG,
				 RTP_PAYLOAD_TYPE_MJPG,
				 rtp_send_config->av_param.resolution_width, rtp_send_config->av_param.resolution_height,
				 rtp_send_config->av_param.resolution_width, rtp_send_config->av_param.resolution_height,
				 rtp_send_config->av_param.frame_rate,
				 video_ctrl, play_dir);
	}
	else
	{
		__ERR("rtp_send_config->av_param.video_stream_type is invalid!(%lu)", (unsigned long)(rtp_send_config->av_param.video_stream_type));
		return -1;
	}
	__INFO("video_sdp_buf=(%s)", video_sdp_buf);

	if (rtp_send_config->rtp_payload_type != SINGLE_COMPOSITE_PS_STREAM)
	{
		if (rtp_send_config->av_param.audio_stream_type == e_stream_type_AAC)
		{
			int profile = 0x01;																				// aac的profile, 通常情况是1, 或者2
			int channelConfiguration = 0x01;																// 单通道
			int samplingFrequencyIndex = rtp_send_config->av_param.audio_sample_rate == 8000 ? 0x0b : 0x08; // 8kHz or 16kHz，数值为aac的采样频率的索引

			char audio_cfg[2] = {0};
			char audio_cfg_tmp[8] = {0};
			char const audioObjectType = profile + 1; // 从aac adts header读取出来的profile是被减1的
			audio_cfg[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
			audio_cfg[1] = (samplingFrequencyIndex << 7) | (channelConfiguration << 3);
			snprintf(audio_cfg_tmp, sizeof(audio_cfg_tmp), "%02x%02x", audio_cfg[0], audio_cfg[1]);
			snprintf(audio_sdp_buf, sizeof(audio_sdp_buf), "m=audio 0 RTP/AVP %d\r\n"
				"%s"
				"a=rtpmap:%d mpeg4-generic/%d/1\r\n"
				"a=fmtp:%d profile-level-id=1;streamtype=5;mode=AAC-hbr;config=%s;SizeLength=13;IndexLength=3;IndexDeltaLength=3;Profile=%d;\r\n"
				"a=control:%s\r\n"
				"%s",
				RTP_PAYLOAD_TYPE_AAC, c_line, RTP_PAYLOAD_TYPE_AAC, rtp_send_config->av_param.audio_sample_rate, RTP_PAYLOAD_TYPE_AAC, audio_cfg_tmp, profile,
				audio_ctrl, play_dir);
		}
		else if (rtp_send_config->av_param.audio_stream_type == e_stream_type_G711A)
		{
			snprintf(audio_sdp_buf, sizeof(audio_sdp_buf), "m=audio 0 RTP/AVP %d\r\n"
				"%s"
				"a=rtpmap:%d PCMA/%d\r\n"
				"a=control:%s\r\n"
				"%s",
				RTP_PAYLOAD_TYPE_G711A, c_line, RTP_PAYLOAD_TYPE_G711A, rtp_send_config->av_param.audio_sample_rate,
				audio_ctrl, play_dir);
		}
		else if (rtp_send_config->av_param.audio_stream_type == e_stream_type_G711U)
		{
			snprintf(audio_sdp_buf, sizeof(audio_sdp_buf), "m=audio 0 RTP/AVP %d\r\n"
				"%s"
				"a=rtpmap:%d PCMU/%d\r\n"
				"a=control:%s\r\n"
				"%s",
				RTP_PAYLOAD_TYPE_G711U, c_line, RTP_PAYLOAD_TYPE_G711U, rtp_send_config->av_param.audio_sample_rate,
				audio_ctrl, play_dir);
		}
	}

	/* Old hik_server-rtsp: Media_header + appversion after outbound audio (trackID=2) */
	if (hik_extra[0] != '\0')
	{
		if (audio_sdp_buf[0] != '\0')
		{
			size_t used = strlen(audio_sdp_buf);
			snprintf(audio_sdp_buf + used, sizeof(audio_sdp_buf) - used, "%s", hik_extra);
		}
		else
		{
			size_t used = strlen(video_sdp_buf);
			snprintf(video_sdp_buf + used, sizeof(video_sdp_buf) - used, "%s", hik_extra);
		}
	}
	__INFO("audio_sdp_buf=(%s)", audio_sdp_buf);

	/* ONVIF/Hik backchannel: client -> device speaker, G.711 A/U */
	if (rtp_send_config->backchannel_requested &&
		rtp_send_config->rtp_payload_type != SINGLE_COMPOSITE_PS_STREAM)
	{
		int sample_rate = rtp_send_config->av_param.audio_sample_rate > 0 ?
						  rtp_send_config->av_param.audio_sample_rate : 8000;
		snprintf(backchannel_sdp_buf, sizeof(backchannel_sdp_buf),
				 "m=audio 0 RTP/AVP %d %d\r\n"
				 "c=IN IP4 0.0.0.0\r\n"
				 "a=control:%s\r\n"
				 "a=rtpmap:%d PCMA/%d\r\n"
				 "a=rtpmap:%d PCMU/%d\r\n"
				 "a=sendonly\r\n",
				 RTP_PAYLOAD_TYPE_G711A, RTP_PAYLOAD_TYPE_G711U,
				 AUDIO_BACKCHANNEL_ID,
				 RTP_PAYLOAD_TYPE_G711A, sample_rate,
				 RTP_PAYLOAD_TYPE_G711U, sample_rate);
		__INFO("backchannel_sdp_buf=(%s)", backchannel_sdp_buf);
	}

	int sdp_buf_length = strlen(video_sdp_buf) + strlen(audio_sdp_buf) + strlen(backchannel_sdp_buf);

	char content_base[256] = {0};
	if (g_rtsp_server_config->rtsp_server_port == 554)
	{
		snprintf(content_base, sizeof(content_base), "rtsp://%s%s/", ipv4_addr, content_base_path);
	}
	else
	{
		snprintf(content_base, sizeof(content_base), "rtsp://%s:%d%s/", ipv4_addr, g_rtsp_server_config->rtsp_server_port, content_base_path);
	}

	if (rtp_send_config->backchannel_requested)
	{
		snprintf(describe_reply_message, describe_reply_message_length, "RTSP/1.0 %s\r\n"
			"CSeq: %d\r\n"
			"Content-Base: %s\r\n"
			"Content-type: application/sdp\r\n"
			"Supported: www.onvif.org/ver20/backchannel\r\n"
			"Content-length: %d\r\n\r\n"
			"%s%s%s",
			rtsp_get_status_msg(RTSP_STATUS_OK),
			cseq,
			content_base,
			sdp_buf_length,
			video_sdp_buf,
			audio_sdp_buf,
			backchannel_sdp_buf);
	}
	else
	{
		snprintf(describe_reply_message, describe_reply_message_length, "RTSP/1.0 %s\r\n"
			"CSeq: %d\r\n"
			"Content-Base: %s\r\n"
			"Content-type: application/sdp\r\n"
			"Content-length: %d\r\n\r\n"
			"%s%s%s",
			rtsp_get_status_msg(RTSP_STATUS_OK),
			cseq,
			content_base,
			sdp_buf_length,
			video_sdp_buf,
			audio_sdp_buf,
			backchannel_sdp_buf);
	}
	return 0;
}

static int _setup_reply_message_get(char *setup_reply_message, unsigned int setup_reply_message_length, int cseq, char *session,
									unsigned int setup_count, rtp_send_config_s *rtp_send_config,
									rtp_send_type_e rtp_send_type, int rtp_over_udp_port, int rtcp_over_udp_port)
{
	_NULL_POINTER_CHECK_(setup_reply_message, -1);
	_NULL_POINTER_CHECK_(session, -1);
	if (setup_reply_message_length == 0)
	{
		__ERR("setup_reply_message_length is invalid!(%lu)", (unsigned long)setup_reply_message_length);
		return -1;
	}

	switch (rtp_send_type)
	{
		case RTP_OVER_TCP:
			snprintf(setup_reply_message, setup_reply_message_length, 
				"RTSP/1.0 %s\r\n"
				"Cseq: %d\r\n"
				"Session: %s\r\n"
				"Transport: RTP/AVP/TCP;unicast;interleaved=%u-%u;mode=play\r\n"
				"\r\n",
				rtsp_get_status_msg(RTSP_STATUS_OK),
				cseq,
				session,
				setup_count,
				setup_count + 1);
			break;

		case RTP_OVER_UDP:
			if (!port_is_valid(rtp_over_udp_port))
			{
				__ERR("port_is_valid failed!");
				return -1;
			}
			if (!port_is_valid(rtcp_over_udp_port))
			{
				__ERR("port_is_valid failed!");
				return -1;
			}

			if (rtp_send_config != NULL && rtp_send_config->is_multicast)
			{
				char source_ip[16] = {0};
				struct in_addr in;
				in.s_addr = get_my_ipaddr();
				if (in.s_addr == INADDR_NONE || in.s_addr == 0 ||
					inet_ntop(AF_INET, &in, source_ip, sizeof(source_ip)) == NULL)
				{
					snprintf(source_ip, sizeof(source_ip), "0.0.0.0");
				}
				snprintf(setup_reply_message, setup_reply_message_length, "RTSP/1.0 %s\r\n"
					"CSeq: %d\r\n"
					"Transport: RTP/AVP;multicast;destination=%s;source=%s;port=%d-%d;ttl=%d\r\n"
					"Session: %s\r\n"
					"\r\n",
					rtsp_get_status_msg(RTSP_STATUS_OK),
					cseq,
					rtp_send_config->rtp_over_udp_ip,
					source_ip,
					rtp_over_udp_port,
					rtcp_over_udp_port,
					RTSP_MCAST_TTL,
					session);
			}
			else
			{
				snprintf(setup_reply_message, setup_reply_message_length, "RTSP/1.0 %s\r\n"
					"CSeq: %d\r\n"
					"Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
					"Session: %s\r\n"
					"\r\n",
					rtsp_get_status_msg(RTSP_STATUS_OK),
					cseq,
					rtp_over_udp_port,
					rtcp_over_udp_port,
					RTSP_SERVER_UDP_RTP_PORT,
					RTSP_SERVER_UDP_RTCP_PORT,
					session);
			}
			break;

		default:
			__ERR("rtp_send_type is invalid!(%lu)", (unsigned long)(rtp_send_type));
			return -1;
	}
	__INFO("setup_reply_message=(%s)", setup_reply_message);
	return 0;
}

static int _play_reply_message_get(char *play_reply_message, unsigned int play_reply_message_length, int cseq, char *session,
								   rtp_send_config_s *rtp_send_config)
{
	_NULL_POINTER_CHECK_(play_reply_message, -1);
	_NULL_POINTER_CHECK_(session, -1);
	if (play_reply_message_length == 0)
	{
		__ERR("play_reply_message_length is invalid!(%lu)", (unsigned long)play_reply_message_length);
		return -1;
	}

	/* Hik NVR needs RTP-Info with both outbound audio and backchannel, or talk may drop ~20s later */
	if (rtp_send_config != NULL && rtp_send_config->backchannel_enable)
	{
		snprintf(play_reply_message, play_reply_message_length, "RTSP/1.0 %s\r\n"
			"CSeq: %d\r\n"
			"Range: npt=0.000-\r\n"
			"Session: %s; timeout=60\r\n"
			"RTP-Info: url=%s;seq=%u;rtptime=%u,url=%s;seq=0;rtptime=0\r\n"
			"\r\n",
			rtsp_get_status_msg(RTSP_STATUS_OK),
			cseq, session,
			AUDIO_TRACK_ID,
			rtp_send_config->rtp_seq,
			rtp_send_config->rtp_timestamp,
			AUDIO_BACKCHANNEL_ID);
	}
	else
	{
		snprintf(play_reply_message, play_reply_message_length, "RTSP/1.0 %s\r\n"
			"CSeq: %d\r\n"
			"Range: npt=0.000-\r\n"
			"Session: %s; timeout=60\r\n\r\n",
			rtsp_get_status_msg(RTSP_STATUS_OK),
			cseq, session);
	}
	return 0;
}

static int _teardown_reply_message_get(char *teardown_reply_message, unsigned int teardown_reply_message_length, int cseq, char *session)
{
	_NULL_POINTER_CHECK_(teardown_reply_message, -1);
	_NULL_POINTER_CHECK_(session, -1);
	if (teardown_reply_message_length == 0)
	{
		__ERR("teardown_reply_message_length is invalid!(%lu)", (unsigned long)teardown_reply_message_length);
		return -1;
	}

	snprintf(teardown_reply_message, teardown_reply_message_length, "RTSP/1.0 %s\r\n"
		"CSeq: %d\r\n"
		"Range: npt=0.000-\r\n"
		"Session: %s; timeout=60\r\n\r\n"
		"Connection: Close\r\n",
		rtsp_get_status_msg(RTSP_STATUS_OK),
		cseq, session);
	return 0;
}

/* Align with hik_server-rtsp: User-Agent / User-agent header spellings */
static int _rtsp_get_user_agent(char *hdr, char *ua, unsigned int ua_len)
{
	if (get_http_hdr_value(hdr, "User-Agent: ", ua, ua_len) == 0)
	{
		return 0;
	}
	if (get_http_hdr_value(hdr, "User-agent: ", ua, ua_len) == 0)
	{
		return 0;
	}
	return -1;
}

static int _rtsp_ua_is_hik_client(const char *ua)
{
	if (ua == NULL || ua[0] == '\0')
	{
		return 0;
	}
	if ((strstri(ua, "Hikplayer") != NULL) ||
		(strstri(ua, "Hikvision") != NULL) ||
		(strstri(ua, "NKPlayer") != NULL))
	{
		return 1;
	}
	return 0;
}

static rtsp_status_code_e _rtsp_method_handle(char *rtsp_message, unsigned int rtsp_message_length, int *cseq,
											  rtsp_client_config_s *rtsp_client_config, rtp_send_config_s *rtp_send_config,
											  char *rtsp_reply_message, unsigned int rtsp_reply_message_length)
{
	_NULL_POINTER_CHECK_(rtsp_message, RTSP_STATUS_BAD_REQUEST);
	_NULL_POINTER_CHECK_(rtsp_client_config, RTSP_STATUS_BAD_REQUEST);
	_NULL_POINTER_CHECK_(rtp_send_config, RTSP_STATUS_BAD_REQUEST);
	_NULL_POINTER_CHECK_(rtsp_reply_message, RTSP_STATUS_BAD_REQUEST);
	if (rtsp_message_length == 0 || rtsp_reply_message_length == 0)
	{
		__ERR("rtsp method length is invalid!");
		return RTSP_STATUS_BAD_REQUEST;
	}

	unsigned int rtsp_message_method_length = 0;
	char *method = NULL;
	char *url = NULL;
	char *version = NULL;
	rtsp_status_code_e rtsp_status_code = RTSP_STATUS_OK;
	rtsp_status_code = _rtsp_message_method_parse(rtsp_message, rtsp_message_length, &rtsp_message_method_length, &method, &url, &version);
	if (rtsp_status_code != RTSP_STATUS_OK)
	{
		return rtsp_status_code;
	}

	if (_rtsp_message_cseq_parse(rtsp_message + rtsp_message_method_length, rtsp_message_length - rtsp_message_method_length, cseq) != 0)
	{
		__ERR("_rtsp_message_cseq_parse failed! (recv_buf=[%s])", rtsp_message + rtsp_message_method_length);
		return RTSP_STATUS_BAD_REQUEST;
	}

	__INFO("method=(%s), url=(%s), version=(%s) \n", method, url, version);

	MediaConfig *pMediaConfig = (MediaConfig *)getMediaConfig();

	if (strcmp(method, "OPTIONS") == 0)
	{
		if (_options_reply_message_get(rtsp_reply_message, rtsp_reply_message_length, *cseq) != 0)
		{
			__ERR("_options_reply_message_get failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}
	}
	else if (strcmp(method, "DESCRIBE") == 0)
	{
		unsigned int replay_channel = 0;
		unsigned int replay_start_time = 0;
		MediaStreamConfig *pMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
		char user_agent[256] = {0};
		char *hdr = rtsp_message + rtsp_message_method_length;
		unsigned int hdr_len = rtsp_message_length - rtsp_message_method_length;
		int is_hik = 0;
		int need_auth = 1;

		/* Hik UA → hikConfig.auth; others → rtsp_auth (hik_server-rtsp proc_describe) */
		if (_rtsp_get_user_agent(hdr, user_agent, sizeof(user_agent)) == 0)
		{
			is_hik = _rtsp_ua_is_hik_client(user_agent);
		}

		if (pMediaStreamCfg != NULL)
		{
			if (is_hik)
			{
				need_auth = (pMediaStreamCfg->hikConfig.auth != 0);
			}
			else
			{
				need_auth = (pMediaStreamCfg->rtspConfig.rtsp_auth != 0);
			}
		}

		if (need_auth == 0)
		{
			rtsp_client_config->b_auth = 1;
			if (is_hik)
			{
				__INFO("hik UA auth soft-skip (hik_auth=0) ua=%s\n", user_agent);
			}
		}
		else if (rtsp_auth_verify(hdr, hdr_len, url) == 0)
		{
			rtsp_client_config->b_auth = 1; // 鉴权成功
		}
		else
		{
			rtsp_client_config->b_auth = 0;
			return RTSP_STATUS_UNAUTHORIZED; // 鉴权失败
		}

		rtp_send_config->is_replay = 0;
		if (_parse_replay_url(url, &replay_channel, &replay_start_time) == 0)
		{
			rtp_send_config->is_replay = 1;
			rtp_send_config->replay_channel = replay_channel;
			rtp_send_config->replay_start_time = replay_start_time;
			rtp_send_config->stream_index = replay_channel;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
		}
		else if (strstr(url, "PSMainStream"))
		{
			rtp_send_config->stream_index = MAIN_STREAM;
			rtp_send_config->rtp_payload_type = SINGLE_COMPOSITE_PS_STREAM;
		}
		else if (strstr(url, "PSSubStream"))
		{
			rtp_send_config->stream_index = SUB_STREAM;
			rtp_send_config->rtp_payload_type = SINGLE_COMPOSITE_PS_STREAM;
		}
		else if (_parse_live_url_hik_alias(url, rtp_send_config) == 0)
		{
			/* Hik Channels / h264 aliases filled stream_index + hik_private_url */
		}
		else if (strstri(url, "MainStream") || strstr(url, "stream0"))
		{
			/* Align with old hik_server-rtsp: stream0 also uses trackID + Media_header */
			rtp_send_config->stream_index = MAIN_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else if (strstri(url, "SubStream") || strstr(url, "stream1"))
		{
			rtp_send_config->stream_index = SUB_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else if (strstri(url, "mpeg4cif"))
		{
			rtp_send_config->stream_index = SUB_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else if (strstri(url, "mpeg4"))
		{
			rtp_send_config->stream_index = MAIN_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else if (strstr(url, "/mcast1"))
		{
			rtp_send_config->stream_index = MAIN_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else if (strstr(url, "/mcast2"))
		{
			rtp_send_config->stream_index = SUB_STREAM;
			rtp_send_config->rtp_payload_type = MULTIPLE_MEDIA_STREAMS;
			rtp_send_config->hik_private_url = 1;
		}
		else
		{
			__ERR("url is invalid!(%s)", (url));
			return RTSP_STATUS_NOT_FOUND;
		}

		if (_rtsp_url_wants_mcast(url))
		{
			if (_rtsp_mcast_apply(rtp_send_config, url) != 0)
			{
				__ERR("multicast not enabled or invalid, url=%s", url);
				return RTSP_STATUS_NOT_FOUND;
			}
		}

		if (rtsp_stream_init_av_param_from_cfg(&rtp_send_config->av_param, rtp_send_config->stream_index) != 0)
		{
			__ERR("rtsp_stream_init_av_param_from_cfg failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}
		rtp_send_config->av_param.video_enable = 0;
		rtp_send_config->av_param.audio_enable = 0;
		rtp_send_config->backchannel_requested =
			_rtsp_message_has_backchannel_require(rtsp_message + rtsp_message_method_length);
		if (rtp_send_config->backchannel_requested)
		{
			__INFO("DESCRIBE Require backchannel detected");
		}
		if (_describe_reply_message_get(rtsp_reply_message, rtsp_reply_message_length, *cseq, rtp_send_config, url) != 0)
		{
			__ERR("_describe_reply_message_get failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}
	}
	else if (strcmp(method, "SETUP") == 0)
	{
		int rtp_over_udp_client_port = -1;
		int rtcp_over_udp_client_port = -1;
		int is_backchannel = _rtsp_url_is_backchannel(url);
		if (rtsp_client_config->b_auth != 1)
		{
			__DBG("SETUP RTSP_STATUS_UNAUTHORIZED");
			return RTSP_STATUS_UNAUTHORIZED;
		}

		if (_rtsp_url_wants_mcast(url) || rtp_send_config->is_multicast ||
			strstri(rtsp_message, "multicast") != NULL)
		{
			if (_rtsp_mcast_apply(rtp_send_config, url) != 0)
			{
				__ERR("multicast not enabled or invalid, url=%s", url);
				return RTSP_STATUS_NOT_FOUND;
			}
		}

		if (_rtsp_message_transport_parse(rtsp_message + rtsp_message_method_length, rtsp_message_length - rtsp_message_method_length,
										  &rtp_send_config->rtp_send_type, &rtp_over_udp_client_port, &rtcp_over_udp_client_port,
										  rtp_send_config->is_multicast) != 0)
		{
			__ERR("_rtsp_message_transport_parse failed!");
			return RTSP_STATUS_PARAM_NOT_UNDERSTOOD;
		}

		if (rtp_send_config->is_multicast)
		{
			if (rtp_send_config->rtp_send_type == RTP_OVER_TCP)
			{
				__ERR("multicast cannot use TCP transport");
				return RTSP_STATUS_TRANSPORT;
			}
			rtp_send_config->rtp_send_type = RTP_OVER_UDP;
		}

		if (rtp_send_config->rtp_send_type == RTP_OVER_TCP)
		{
			rtp_send_config->rtp_fd = rtsp_client_config->rtsp_client_fd;
			rtp_send_config->rtcp_fd = rtsp_client_config->rtsp_client_fd;

			if (_rtsp_url_is_video_track(url))
			{
				rtp_send_config->av_param.video_enable = 1;
				rtp_send_config->rtp_video_channel = rtsp_client_config->interleaved;
				rtp_send_config->rtcp_video_channel = rtsp_client_config->interleaved + 1;
			}
			else if (_rtsp_url_is_audio_track(url))
			{
				if (pMediaConfig->audioConfig.audioEncode.enable)
				{
					rtp_send_config->av_param.audio_enable = 1;
				}
				else
				{
					rtp_send_config->av_param.audio_enable = 0;
				}
				rtp_send_config->rtp_audio_channel = rtsp_client_config->interleaved;
				rtp_send_config->rtcp_audio_channel = rtsp_client_config->interleaved + 1;
			}
			else if (is_backchannel)
			{
				/* Prefer Hik fixed interleaved 6-7 when free; otherwise use next free channel */
				unsigned int bc_ch = (rtsp_client_config->interleaved <= 6) ? 6 : rtsp_client_config->interleaved;
				rtp_send_config->backchannel_enable = 1;
				rtp_send_config->backchannel_requested = 1;
				rtp_send_config->rtp_backchannel_channel = bc_ch;
				rtp_send_config->rtcp_backchannel_channel = bc_ch + 1;
				rtsp_client_config->interleaved = bc_ch;
				__INFO("SETUP backchannel TCP interleaved=%u-%u", bc_ch, bc_ch + 1);
			}
			else if (strstr(url, "program_stream"))
			{
				rtp_send_config->av_param.video_enable = 1;
				if (pMediaConfig->audioConfig.audioEncode.enable)
				{
					rtp_send_config->av_param.audio_enable = 1;
				}
				else
				{
					rtp_send_config->av_param.audio_enable = 0;
				}
				rtp_send_config->rtp_video_channel = rtsp_client_config->interleaved;
				rtp_send_config->rtcp_video_channel = rtsp_client_config->interleaved + 1;
			}
			else
			{
				__ERR("url is invalid!(%s)", (url));
				return RTSP_STATUS_NOT_FOUND;
			}
		}
		else if (rtp_send_config->rtp_send_type == RTP_OVER_UDP)
		{
			if (!sk_is_valid(rtp_send_config->rtp_fd))
			{
				if (rtp_rtcp_udp_fd_init(&rtp_send_config->rtp_fd, RTSP_SERVER_UDP_RTP_PORT, rtp_send_config->stream_index) != 0)
				{
					__ERR("rtp_rtcp_udp_fd_init failed!");
					return RTSP_STATUS_BAD_REQUEST;
				}
			}

			if (!sk_is_valid(rtp_send_config->rtcp_fd))
			{
				if (rtp_rtcp_udp_fd_init(&rtp_send_config->rtcp_fd, RTSP_SERVER_UDP_RTCP_PORT, rtp_send_config->stream_index) != 0)
				{
					__ERR("rtp_rtcp_udp_fd_init failed!");
					SOCK_CLOSE(rtp_send_config->rtp_fd, (void)0);
					return RTSP_STATUS_BAD_REQUEST;
				}
			}

			if (rtp_send_config->is_multicast)
			{
				char mcast_if_ip[32] = {0};

				_rtsp_mcast_local_ip_get(rtsp_client_config->rtsp_client_fd, mcast_if_ip, sizeof(mcast_if_ip));
				rtp_udp_mcast_sockopt(rtp_send_config->rtp_fd, mcast_if_ip);
				rtp_udp_mcast_sockopt(rtp_send_config->rtcp_fd, mcast_if_ip);
			}

			if (_rtsp_url_is_video_track(url))
			{
				rtp_send_config->av_param.video_enable = 1;
				if (!rtp_send_config->is_multicast)
				{
					rtp_send_config->rtp_client_video_over_udp_port = rtp_over_udp_client_port;
					rtp_send_config->rtcp_client_video_over_udp_port = rtcp_over_udp_client_port;
				}
				rtp_over_udp_client_port = rtp_send_config->rtp_client_video_over_udp_port;
				rtcp_over_udp_client_port = rtp_send_config->rtcp_client_video_over_udp_port;
			}
			else if (_rtsp_url_is_audio_track(url))
			{
				if (pMediaConfig->audioConfig.audioEncode.enable)
				{
					rtp_send_config->av_param.audio_enable = 1;
				}
				else
				{
					rtp_send_config->av_param.audio_enable = 0;
				}
				if (!rtp_send_config->is_multicast)
				{
					rtp_send_config->rtp_client_audio_over_udp_port = rtp_over_udp_client_port;
					rtp_send_config->rtcp_client_audio_over_udp_port = rtcp_over_udp_client_port;
				}
				rtp_over_udp_client_port = rtp_send_config->rtp_client_audio_over_udp_port;
				rtcp_over_udp_client_port = rtp_send_config->rtcp_client_audio_over_udp_port;
			}
			else if (is_backchannel)
			{
				rtp_send_config->backchannel_enable = 1;
				rtp_send_config->backchannel_requested = 1;
				rtp_send_config->rtp_client_backchannel_over_udp_port = rtp_over_udp_client_port;
				rtp_send_config->rtcp_client_backchannel_over_udp_port = rtcp_over_udp_client_port;
				__INFO("SETUP backchannel UDP client_port=%d-%d", rtp_over_udp_client_port, rtcp_over_udp_client_port);
			}
			else if (strstr(url, "program_stream"))
			{
				rtp_send_config->av_param.video_enable = 1;
				if (pMediaConfig->audioConfig.audioEncode.enable)
				{
					rtp_send_config->av_param.audio_enable = 1;
				}
				else
				{
					rtp_send_config->av_param.audio_enable = 0;
				}
				if (!rtp_send_config->is_multicast)
				{
					rtp_send_config->rtp_client_video_over_udp_port = rtp_over_udp_client_port;
					rtp_send_config->rtcp_client_video_over_udp_port = rtcp_over_udp_client_port;
				}
				rtp_over_udp_client_port = rtp_send_config->rtp_client_video_over_udp_port;
				rtcp_over_udp_client_port = rtp_send_config->rtcp_client_video_over_udp_port;
			}
			else
			{
				__ERR("url is invalid!(%s)", (url));
				return RTSP_STATUS_NOT_FOUND;
			}
		}

		if (_setup_reply_message_get(rtsp_reply_message, rtsp_reply_message_length, *cseq, rtsp_client_config->rtsp_session, rtsp_client_config->interleaved,
									 rtp_send_config, rtp_send_config->rtp_send_type, rtp_over_udp_client_port, rtcp_over_udp_client_port) != 0)
		{
			__ERR("_setup_reply_message_get failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}

		rtsp_client_config->interleaved += 2;
	}
	else if (strcmp(method, "PLAY") == 0)
	{
		if (_check_session_valid(rtsp_message + rtsp_message_method_length, rtsp_client_config->rtsp_session) != 0)
		{
			__ERR("_check_session_valid failed!");
			return RTSP_STATUS_SESSION;
		}

		if (_play_reply_message_get(rtsp_reply_message, rtsp_reply_message_length, *cseq, rtsp_client_config->rtsp_session, rtp_send_config) != 0)
		{
			__ERR("_play_reply_message_get failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}

		if (rtp_send_config->backchannel_enable)
		{
			if (_rtsp_backchannel_talk_start(rtp_send_config) != 0)
			{
				__ERR("_rtsp_backchannel_talk_start failed!");
				return RTSP_STATUS_BAD_REQUEST;
			}
		}
		else if (rtp_send_config->backchannel_requested)
		{
			__ERR("PLAY with backchannel requested but trackID=5 was not SETUP, talk will be silent");
		}

		/* Backchannel-only session may have no outbound tracks; still allow PLAY */
		if (rtp_send_config->av_param.video_enable || rtp_send_config->av_param.audio_enable)
		{
			int already_sending = rtp_send_config->rtp_send_thread.start;
			if (rtp_send_start(rtp_send_config) != 0)
			{
				__ERR("rtp_send_start failed!");
				_rtsp_backchannel_talk_stop(rtp_send_config);
				return RTSP_STATUS_BAD_REQUEST;
			}
			/* live only; same session PLAY does not refresh; reconnect restarts */
			if (!rtp_send_config->is_replay && already_sending == 0)
			{
				rtsp_allnet_live_add(rtsp_client_config);
			}
		}
	}
	else if (strcmp(method, "TEARDOWN") == 0)
	{
		if (_check_session_valid(rtsp_message + rtsp_message_method_length, rtsp_client_config->rtsp_session) != 0)
		{
			__ERR("_check_session_valid failed!");
			return RTSP_STATUS_SESSION;
		}

		if (_teardown_reply_message_get(rtsp_reply_message, rtsp_reply_message_length, *cseq, rtsp_client_config->rtsp_session) != 0)
		{
			__ERR("_teardown_reply_message_get failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}

		_rtsp_backchannel_talk_stop(rtp_send_config);

		if (rtp_send_stop(rtp_send_config) != 0)
		{
			__ERR("rtp_send_stop failed!");
			return RTSP_STATUS_BAD_REQUEST;
		}
		rtsp_allnet_live_del(rtsp_client_config);
	}
	else if (strcmp(method, "GET_PARAMETER") == 0)
	{
		/* GET_PARAMETER is used as RTSP session keepalive by ONVIF clients */
		snprintf(rtsp_reply_message, rtsp_reply_message_length, "RTSP/1.0 %s\r\n"
			"CSeq: %d\r\n"
			"Session: %s\r\n"
			"\r\n",
			rtsp_get_status_msg(RTSP_STATUS_OK),
			*cseq,
			rtsp_client_config->rtsp_session);
	}
	else
	{
		return RTSP_STATUS_NOT_IMPLEMENTED;
	}

	return RTSP_STATUS_OK;
}

static int rtsp_pkt_find_end(char *p_buf)
{
	int end_off = 0;
	int rtsp_pkt_finish = 0;
	while (p_buf[end_off] != '\0')
	{
        if ((p_buf[end_off] == '\r' && p_buf[end_off + 1] == '\n')
        && (p_buf[end_off + 2] == '\r' && p_buf[end_off + 3] == '\n'))
		{
			rtsp_pkt_finish = 1;
			break;
		}

		end_off++;
	}

	if (rtsp_pkt_finish)
	{
		return (end_off + 4);
	}

	return 0;
}

static int _rtsp_client_peer_gone(int fd)
{
	struct pollfd pfd;
	char peek;
	int ret;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN | POLLERR | POLLHUP;
	ret = poll(&pfd, 1, 0);
	if (ret < 0)
	{
		return (errno == EINTR) ? 0 : 1;
	}
	if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		return 1;
	}
	if ((pfd.revents & POLLIN) && recv(fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
	{
		return 1;
	}
	return 0;
}

static int _rtsp_request_recv_poll(rtsp_client_config_s *rtsp_client_config, rtp_send_config_s *rtp_send_config,
								   char *recv_buf, int *recv_len)
{
	int client_fd = rtsp_client_config->rtsp_client_fd;
	int ret;
	int len = 0;
	struct pollfd pfds[2];
	int nfds;

	while (rtsp_client_config->rtsp_reply_thread.start != 0)
	{
		/* When UDP backchannel is active, poll both sockets so talk audio is not blocked by RTSP recv */
		if (rtp_send_config != NULL &&
			rtp_send_config->backchannel_enable &&
			rtp_send_config->rtp_send_type == RTP_OVER_UDP &&
			sk_is_valid(rtp_send_config->rtp_fd))
		{
			memset(pfds, 0, sizeof(pfds));
			pfds[0].fd = client_fd;
			pfds[0].events = POLLIN | POLLERR | POLLHUP;
			pfds[1].fd = rtp_send_config->rtp_fd;
			pfds[1].events = POLLIN;
			nfds = 2;
			ret = poll(pfds, nfds, 100);
			if (ret < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				__ERR("poll failed(sk=%d, errno=%d, %s)", client_fd, errno, strerror(errno));
				return -1;
			}
			if (ret == 0)
			{
				continue;
			}
			if (pfds[1].revents & POLLIN)
			{
				while (_rtsp_backchannel_udp_recv_once(rtp_send_config) > 0)
				{
				}
			}
			if (!(pfds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)))
			{
				continue;
			}
			if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				__ERR("peer closed(sk=%d) \n", client_fd);
				return -1;
			}
		}

		ret = recv(client_fd, recv_buf + len, RTSP_MESSAGE_PRESET_SIZE - len, 0);
		if (ret > 0)
		{
			len += ret;
			/* Interleaved RTP/RTCP: wait until at least one complete frame */
			if (recv_buf[0] == '$')
			{
				if (len >= 4)
				{
					unsigned short rtp_len = 0;
					memcpy(&rtp_len, recv_buf + 2, sizeof(rtp_len));
					rtp_len = ntohs(rtp_len);
					if (len >= (int)(4 + rtp_len))
					{
						*recv_len = len;
						return 0;
					}
				}
				continue;
			}
			/* Bare RTP (some talkback clients) */
			if ((((unsigned char)recv_buf[0] & 0xc0) == 0x80) && len >= RTSP_BACKCHANNEL_RTP_HDR_LEN)
			{
				*recv_len = len;
				return 0;
			}
			/* 未收齐 RTSP 头尾，继续 recv */
			if (rtsp_pkt_find_end(recv_buf) != 0)
			{
				*recv_len = len;
				return 0;
			}
			continue;
		}
		if (ret == 0)
		{
			__ERR("peer closed(sk=%d) \n", client_fd);
			return -1;
		}

		if (errno == EINTR)
		{
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			if (_rtsp_client_peer_gone(client_fd))
			{
				__ERR("peer closed(sk=%d) \n", client_fd);
				return -1;
			}
			continue;
		}
		__ERR("recv failed(sk=%d, ret=%d, errno=%d, %s) \n", client_fd, ret, errno, strerror(errno));
		return -1;
	}

	return -1;
}

static int _rtsp_backchannel_udp_recv_once(rtp_send_config_s *rtp_send_config)
{
	char udp_buf[2048];
	int n;
	int sample_rate;
	unsigned char pt;
	int hdr_len = RTSP_BACKCHANNEL_RTP_HDR_LEN;

	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	if (!rtp_send_config->backchannel_enable ||
		rtp_send_config->rtp_send_type != RTP_OVER_UDP ||
		!sk_is_valid(rtp_send_config->rtp_fd))
	{
		return 0;
	}

	n = recvfrom(rtp_send_config->rtp_fd, udp_buf, sizeof(udp_buf), MSG_DONTWAIT, NULL, NULL);
	if (n <= 0)
	{
		return 0;
	}
	if (n <= hdr_len)
	{
		return 0;
	}
	if (((unsigned char)udp_buf[0] & 0xc0) != 0x80)
	{
		return 0;
	}

	/* Skip CSRC / extension roughly like a minimal RTP parse */
	{
		unsigned char cc = ((unsigned char)udp_buf[0]) & 0x0f;
		unsigned char x = (((unsigned char)udp_buf[0]) >> 4) & 0x01;
		hdr_len = RTSP_BACKCHANNEL_RTP_HDR_LEN + cc * 4;
		if (x && n >= hdr_len + 4)
		{
			unsigned short ext_len;
			memcpy(&ext_len, udp_buf + hdr_len + 2, sizeof(ext_len));
			hdr_len += 4 + ntohs(ext_len) * 4;
		}
	}
	if (n <= hdr_len)
	{
		return 0;
	}

	pt = ((unsigned char)udp_buf[1]) & 0x7f;
	sample_rate = rtp_send_config->av_param.audio_sample_rate > 0 ?
				  rtp_send_config->av_param.audio_sample_rate : 8000;
	_rtsp_backchannel_feed_rtp_payload(udp_buf + hdr_len, n - hdr_len, pt, sample_rate);
	return 1;
}

static int _rtsp_reply_process(void *argv, int *bStart)
{
	__LOG_ENTER();
	_NULL_POINTER_CHECK_(argv, -1);
	_NULL_POINTER_CHECK_(bStart, -1);

	char *recv_buf = (char *)anj_mw_malloc(RTSP_MESSAGE_PRESET_SIZE);
	rtp_send_config_s rtp_send_config;
	rtsp_client_config_s *rtsp_client_config = (rtsp_client_config_s *)argv;

	memset(&rtp_send_config, 0, sizeof(rtp_send_config_s));
	rtp_send_config.rtp_fd = -1;
	rtp_send_config.rtcp_fd = -1;
	snprintf(rtp_send_config.rtp_over_udp_ip, sizeof(rtp_send_config.rtp_over_udp_ip), "%s", rtsp_client_config->rtsp_client_ipv4_addr);
	rtp_send_config.ssrc = htonl(10);

	__INFO("rtsp reply process start rtsp_client_fd %d rtp_fd %d rtcp_fd %d \n", rtsp_client_config->rtsp_client_fd, rtp_send_config.rtp_fd, rtp_send_config.rtcp_fd);
	while (*bStart != 0)
	{
		memset(recv_buf, 0, RTSP_MESSAGE_PRESET_SIZE);
		int len;
		if (_rtsp_request_recv_poll(rtsp_client_config, &rtp_send_config, recv_buf, &len) == -1)
			break;

		int cseq = 0;
		rtsp_status_code_e rtsp_status_code = RTSP_STATUS_OK;
		char rtsp_reply_message[RTSP_MESSAGE_PRESET_SIZE];
		char trail_buf[2048];
		int trail_len = 0;
		int rtsp_len = len;
		int i;

		/* TCP interleaved / bare RTP talkback */
		if (recv_buf[0] == '$' ||
			(rtp_send_config.backchannel_enable && (((unsigned char)recv_buf[0] & 0xc0) == 0x80)))
		{
			_rtsp_backchannel_parse_talk_pack(&rtp_send_config, recv_buf, len);
			continue;
		}

		/* RTSP text may be followed by interleaved RTP in the same TCP read */
		{
			int rtsp_end = rtsp_pkt_find_end(recv_buf);
			if (rtsp_end > 0 && rtsp_end < len)
			{
				for (i = rtsp_end; i < len; i++)
				{
					if (recv_buf[i] == '$' || (((unsigned char)recv_buf[i] & 0xc0) == 0x80))
					{
						trail_len = len - i;
						if (trail_len > (int)sizeof(trail_buf))
						{
							trail_len = (int)sizeof(trail_buf);
						}
						memcpy(trail_buf, recv_buf + i, trail_len);
						rtsp_len = rtsp_end;
						recv_buf[rtsp_end] = '\0';
						break;
					}
				}
			}
		}

		memset(rtsp_reply_message, 0, sizeof(rtsp_reply_message));
		rtsp_status_code = _rtsp_method_handle(recv_buf, rtsp_len, &cseq, rtsp_client_config,
											   &rtp_send_config, rtsp_reply_message, sizeof(rtsp_reply_message));

		if (trail_len > 0)
		{
			_rtsp_backchannel_parse_talk_pack(&rtp_send_config, trail_buf, trail_len);
		}

		if ((rtsp_status_code != RTSP_STATUS_OK) && (rtsp_status_code != RTSP_STATUS_IGNORE))
		{
			if (rtsp_status_code == RTSP_STATUS_UNAUTHORIZED)
			{
				__INFO("_rtsp_method_handle unauthorized, rtsp_status_code %d\n", rtsp_status_code);
			}
			else
			{
				__ERR("_rtsp_method_handle failed, rtsp_status_code %d\n", rtsp_status_code);
			}
		}

		if (rtsp_status_code == RTSP_STATUS_IGNORE)
		{
			continue;
		}
		else if (rtsp_status_code != RTSP_STATUS_OK)
		{
			memset(rtsp_reply_message, 0, sizeof(rtsp_reply_message));
			if (rtsp_get_status_reply_message(rtsp_status_code, cseq, rtsp_reply_message, sizeof(rtsp_reply_message)) != 0)
			{
				__ERR("rtsp_get_status_reply_message failed!\n");
			}
		}

		int rtsp_reply_message_length = strlen(rtsp_reply_message);
		int size;
		size = send(rtsp_client_config->rtsp_client_fd, rtsp_reply_message, rtsp_reply_message_length, MSG_NOSIGNAL);
		if (size != rtsp_reply_message_length)
		{
			__ERR("send failed!(sk=%d, size=%lu, ret=%d)(errno=%d, errmsg=%s) \n", rtsp_client_config->rtsp_client_fd,
				  (unsigned long)rtsp_reply_message_length, size, errno, strerror(errno));
			break;
		}
	}
	_rtsp_backchannel_talk_stop(&rtp_send_config);
	if (rtp_send_stop(&rtp_send_config) != 0)
	{
		__ERR("rtp_send_stop failed!\n");
	}
	rtsp_allnet_live_del(rtsp_client_config);

	__INFO("rtsp reply process exit rtsp_client_fd %d rtp_fd %d rtcp_fd %d", rtsp_client_config->rtsp_client_fd, rtp_send_config.rtp_fd, rtp_send_config.rtcp_fd);

	SOCK_CLOSE(rtsp_client_config->rtsp_client_fd, (void)0);
	if (rtp_send_config.rtp_fd != rtsp_client_config->rtsp_client_fd)
	{
		SOCK_CLOSE(rtp_send_config.rtp_fd, (void)0);
	}
	if (rtp_send_config.rtcp_fd != rtsp_client_config->rtsp_client_fd)
	{
		SOCK_CLOSE(rtp_send_config.rtcp_fd, (void)0);
	}

	anj_mw_free(recv_buf);

	__LOG_LEAVE();
	return 0;
}

static int _rtsp_server_process(void *argv, int *bStart)
{
	__LOG_ENTER();
	_NULL_POINTER_CHECK_(bStart, -1);
	int opt = 0;
	fd_set fdr;
	rtsp_client_config_s *p_tmp_rtsp_client_config = NULL;

	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	while (*bStart != 0)
	{
		FD_ZERO(&fdr);
		FD_SET(g_rtsp_server_config->rtsp_server_fd, &fdr);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		int ret;
		ret = select(g_rtsp_server_config->rtsp_server_fd + 1, &fdr, NULL, NULL, &tv);
		if (ret < 0)
		{
			__ERR("select failed by %s", strerror(errno));
			continue;
		}

		rtsp_allnet_check_timeout();

		struct list_head *pos = NULL;
		struct list_head *n = NULL;

		list_for_each_safe(pos, n, &g_rtsp_server_config->rtsp_client_list)
		{
			rtsp_client_config_s *p_tmp_rtsp_client_config = list_entry(pos, rtsp_client_config_s, node);
			if (p_tmp_rtsp_client_config->rtsp_reply_thread.end != 0)
			{
				if (anj_thread_task_destroy(&p_tmp_rtsp_client_config->rtsp_reply_thread, -1) < 0)
				{
					__ERR("anj_thread_task_destroy failed!");
				}
				list_del(&p_tmp_rtsp_client_config->node);
				anj_mw_free(p_tmp_rtsp_client_config);
				p_tmp_rtsp_client_config = NULL;
			}
		}
		if (ret == 0)
		{
			continue;
		}

		if (FD_ISSET(g_rtsp_server_config->rtsp_server_fd, &fdr) == 0)
		{
			__ERR("FD_ISSET error");
			continue;
		}

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(struct sockaddr_in));
		socklen_t address_len = sizeof(struct sockaddr_in);
		int client_fd;
		client_fd = accept(g_rtsp_server_config->rtsp_server_fd, (struct sockaddr *)&addr, &address_len);
		if (client_fd < 0)
		{
			__ERR("accept failed!(sk=%d, ret=%d)(errno=%d, errmsg=%s)", g_rtsp_server_config->rtsp_server_fd, client_fd, errno, strerror(errno));
			continue;
		}

		setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt));
		opt = 1;
		setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt));
		opt = RTSP_CLIENT_SNDBUF_SIZE;
		setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(opt));
#ifdef TCP_KEEPIDLE
		opt = RTSP_TCP_KEEPIDLE_SEC;
		setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&opt, sizeof(opt));
		opt = RTSP_TCP_KEEPINTVL_SEC;
		setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&opt, sizeof(opt));
		opt = RTSP_TCP_KEEPCNT;
		setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, (char *)&opt, sizeof(opt));
#endif
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = RTSP_CLIENT_RECV_TIMEOUT_MS / 1000;
		tv.tv_usec = (RTSP_CLIENT_RECV_TIMEOUT_MS % 1000) * 1000;
		setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
	
		tv.tv_sec = RTSP_CLIENT_SEND_TIMEOUT_MS / 1000;
		tv.tv_usec = (RTSP_CLIENT_SEND_TIMEOUT_MS % 1000) * 1000;
		setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
	
		if (_rtsp_client_count(&g_rtsp_server_config->rtsp_client_list) >= RTSP_MAX_CLIENT)
		{
			SOCK_CLOSE(client_fd, (void)0);
			__ERR("too many socket connect to server");

			usleep(100 * 1000);
			continue;
		}

		rtsp_client_config_s tmp_rtsp_client_config;
		memset(&tmp_rtsp_client_config, 0, sizeof(rtsp_client_config_s));
		snprintf(tmp_rtsp_client_config.rtsp_client_ipv4_addr, sizeof(tmp_rtsp_client_config.rtsp_client_ipv4_addr), "%s", inet_ntoa(addr.sin_addr));
		tmp_rtsp_client_config.rtsp_client_fd = client_fd;

		if (_rtsp_client_manage(&g_rtsp_server_config->rtsp_client_list, &tmp_rtsp_client_config, 1) != 0)
		{
			__ERR("_rtsp_client_manage failed!");
			SOCK_CLOSE(tmp_rtsp_client_config.rtsp_client_fd, (void)0);
			continue;
		}

		p_tmp_rtsp_client_config = _get_rtsp_client_from_list(&g_rtsp_server_config->rtsp_client_list, tmp_rtsp_client_config.rtsp_client_fd);
		if (p_tmp_rtsp_client_config == NULL)
		{
			__ERR("_get_rtsp_client_from_list failed");
			_rtsp_client_manage(&g_rtsp_server_config->rtsp_client_list, &tmp_rtsp_client_config, 0);
			SOCK_CLOSE(tmp_rtsp_client_config.rtsp_client_fd, (void)0);
			continue;
		}

		int i = 0;
		for (i = 0; i < 16; i++)
		{
			sprintf(p_tmp_rtsp_client_config->rtsp_session + strlen(p_tmp_rtsp_client_config->rtsp_session), "%d", rand() % 10);
		}

		memset(&p_tmp_rtsp_client_config->rtsp_reply_thread, 0, sizeof(p_tmp_rtsp_client_config->rtsp_reply_thread));
		p_tmp_rtsp_client_config->rtsp_reply_thread.bAutoDestroy = 0;
		strncpy(p_tmp_rtsp_client_config->rtsp_reply_thread.iThreadName, "rtsp_reply", sizeof(p_tmp_rtsp_client_config->rtsp_reply_thread.iThreadName) - 1);
		p_tmp_rtsp_client_config->rtsp_reply_thread.iThreadjob.ctx = p_tmp_rtsp_client_config;
		p_tmp_rtsp_client_config->rtsp_reply_thread.iThreadjob.func = _rtsp_reply_process;
		if (anj_thread_task_create(&p_tmp_rtsp_client_config->rtsp_reply_thread) != 0)
		{
			_rtsp_client_manage(&g_rtsp_server_config->rtsp_client_list, &tmp_rtsp_client_config, 0);
			SOCK_CLOSE(tmp_rtsp_client_config.rtsp_client_fd, (void)0);
			continue;
		}
		__DBG("rtsp_reply_thread_id=%lu", p_tmp_rtsp_client_config->rtsp_reply_thread.pid);
	}

	__LOG_LEAVE();
	return 0;
}

int rtsp_server_init(void)
{
	int iRet = -1;

	if (g_rtsp_server_config != NULL)
	{
		__ERR("rtsp has already been initialized");
		return 0;
	}

	rtsp_cfg_s rtsp_cfg;
	if (ipc_config_get_rtsp_cfg(&rtsp_cfg) != 0)
	{
		__ERR("ipc_config_get_rtsp_cfg failed!");
		return -1;
	}
	if (!rtsp_cfg.rtsp_enable)
	{
		__ERR("rtsp is not enabled");
		return 0;
	}

	port_cfg_s port_cfg;
	if (ipc_config_get_port_cfg(&port_cfg) != 0)
	{
		port_cfg.rtsp_port = RTSP_SERVER_PORT;
	}

	g_rtsp_server_config = (rtsp_server_config_s *)anj_mw_malloc(sizeof(rtsp_server_config_s));
	if (g_rtsp_server_config == NULL)
	{
		__ERR("anj_mw_malloc %zu failed", sizeof(rtsp_server_config_s));
		return -1;
	}
	memset(g_rtsp_server_config, 0, sizeof(rtsp_server_config_s));

	g_rtsp_server_config->rtsp_server_port = port_cfg.rtsp_port;
	g_rtsp_server_config->rtsp_server_fd = -1;
	ANJ_CHK((_rtsp_server_fd_init(&g_rtsp_server_config->rtsp_server_fd, g_rtsp_server_config->rtsp_server_port) == 0), -1, "_rtsp_server_fd_init failed!");

	INIT_LIST_HEAD(&g_rtsp_server_config->rtsp_client_list);

	memset(&g_rtsp_server_config->rtsp_server_thread, 0, sizeof(g_rtsp_server_config->rtsp_server_thread));
	g_rtsp_server_config->rtsp_server_thread.bAutoDestroy = 0;
	strncpy(g_rtsp_server_config->rtsp_server_thread.iThreadName, "rtsp_server", sizeof(g_rtsp_server_config->rtsp_server_thread.iThreadName) - 1);
	g_rtsp_server_config->rtsp_server_thread.iThreadjob.ctx = g_rtsp_server_config;
	g_rtsp_server_config->rtsp_server_thread.iThreadjob.func = _rtsp_server_process;
	ANJ_CHK_FUNC(anj_thread_task_create(&g_rtsp_server_config->rtsp_server_thread), 0, "anj_thread_task_create failed");

	_log_supported_rtsp_urls();

	iRet = 0;
endFunc:
	if (iRet != 0 && g_rtsp_server_config != NULL)
	{
		SOCK_CLOSE(g_rtsp_server_config->rtsp_server_fd, (void)0);
		anj_mw_free(g_rtsp_server_config);
		g_rtsp_server_config = NULL;
	}
	return iRet;
}

static int mod_exit_cb_impl(void *ptr)
{
	__DBG("RTSP");
	rtsp_server_config_s *p_rtsp_server_config = (rtsp_server_config_s *)ptr;
	int return_value = 0;

	rtsp_client_config_s *p_tmp_rtsp_client_config = NULL;
	while (!list_empty(&p_rtsp_server_config->rtsp_client_list))
	{
		p_tmp_rtsp_client_config = list_first_entry(&p_rtsp_server_config->rtsp_client_list, rtsp_client_config_s, node);

		if (p_tmp_rtsp_client_config->rtsp_reply_thread.start != 0)
		{
			if (shutdown(p_tmp_rtsp_client_config->rtsp_client_fd, SHUT_RDWR) != 0)
			{
				__ERR("shutdown failed!(errno=%d, errmsg=%s)", errno, strerror(errno));
				return_value = -1;
			}
			
			if (anj_thread_task_destroy(&p_tmp_rtsp_client_config->rtsp_reply_thread, -1) < 0)
			{
				__ERR("anj_thread_task_destroy failed!");
				return_value = -1;
			}
		}

		list_del(&p_tmp_rtsp_client_config->node);
		anj_mw_free(p_tmp_rtsp_client_config);
		p_tmp_rtsp_client_config = NULL;
	}

	if (shutdown(p_rtsp_server_config->rtsp_server_fd, SHUT_RDWR) != 0)
	{
		__ERR("shutdown failed!(errno=%d, errmsg=%s)", errno, strerror(errno));
		return_value = -1;
	}

	SOCK_CLOSE(p_rtsp_server_config->rtsp_server_fd, return_value = -1);
	return return_value;
}
int rtsp_server_uninit(void)
{
	if (g_rtsp_server_config == NULL)
	{
		__ERR("rtsp has not been initialized yet\n");
		return 0;
	}

	rtsp_cfg_s rtsp_cfg;
	memset(&rtsp_cfg, 0, sizeof(rtsp_cfg_s));
	if (ipc_config_get_rtsp_cfg(&rtsp_cfg) != 0)
	{
		__ERR("ipc_config_get_rtsp_cfg failed!\n");
		return -1;
	}
	if (!rtsp_cfg.rtsp_enable)
	{
		__ERR("rtsp is not enabled\n");
		return 0;
	}

	int return_value = 0;
	if (anj_thread_task_destroy(&g_rtsp_server_config->rtsp_server_thread, -1) < 0)
	{
		__ERR("anj_thread_task_destroy failed!\n");
		return_value = -1;
	}
	if (mod_exit_cb_impl(g_rtsp_server_config) != 0)
	{
		return_value = -1;
	}
	anj_mw_free(g_rtsp_server_config);
	g_rtsp_server_config = NULL;
	return return_value;
}

int rtsp_server_restart(void)
{
	if (g_rtsp_server_config == NULL)
	{
		__ERR("rtsp has not been initialized yet");
		return -1;
	}

	int return_value = 0;
	rtsp_client_config_s *p_tmp_rtsp_client_config = NULL;
	while (!list_empty(&g_rtsp_server_config->rtsp_client_list))
	{
		p_tmp_rtsp_client_config = list_first_entry(&g_rtsp_server_config->rtsp_client_list, rtsp_client_config_s, node);

		if (p_tmp_rtsp_client_config->rtsp_reply_thread.start != 0)
		{
			if (shutdown(p_tmp_rtsp_client_config->rtsp_client_fd, SHUT_RDWR) != 0)
			{
				__ERR("shutdown failed!(errno=%d, errmsg=%s)", errno, strerror(errno));
				return_value = -1;
			}
			
			if (anj_thread_task_destroy(&p_tmp_rtsp_client_config->rtsp_reply_thread, -1) < 0)
			{
				__ERR("anj_thread_task_destroy failed!");
				return_value = -1;
			}
		}

		list_del(&p_tmp_rtsp_client_config->node);
		anj_mw_free(p_tmp_rtsp_client_config);
		p_tmp_rtsp_client_config = NULL;
	}

	return return_value;
}
