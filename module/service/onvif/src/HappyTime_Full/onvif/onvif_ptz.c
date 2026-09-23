/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "onvif_ptz.h"
#include "onvif_utils.h"
#include "onvif_event.h"
#include "eventhub.h"
#include "anj_mw_file.h"
#include "para.h"

#ifdef MEDIA2_SUPPORT
#include "onvif_media2.h"
#endif

#ifdef PTZ_SUPPORT

/***************************************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_IDX g_onvif_idx;

extern int g_product_type;

const float EPSINON = 0.00001;
static time_t g_ptz_last_move_time = 0;

static void onvif_ptz_mark_move(int moving)
{
	if (moving)
	{
		g_ptz_last_move_time = time(NULL);
	}
	else
	{
		g_ptz_last_move_time = 0;
	}
}

extern float g_tptz_value_x;								//1
extern float g_tptz_value_y;								//1
extern float g_tptz_value_z;								//1
extern int g_bHxVersion;
extern ptz_tour_ctx_t g_tour_ctx;

typedef struct
{
	int used;
	int is_home;
	int preset_number;
	char profile_token[64];
	char preset_name[64];
	char preset_token[64];
} onvif_preset_entry_t;

#define ONVIF_PRESET_ENTRY_MAX 256
#define ONVIF_TOUR_FILE "/mnt/nand/onvif_tours.dat"

static onvif_preset_entry_t g_onvif_preset_entries[ONVIF_PRESET_ENTRY_MAX];
static pthread_mutex_t g_onvif_preset_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_onvif_tour_lock = PTHREAD_MUTEX_INITIALIZER;

static int onvif_extract_xml_tag(const char *xml, const char *tag, char *out, size_t out_len)
{
	char open_tag[64] = {0};
	char close_tag[64] = {0};
	const char *start = NULL;
	const char *end = NULL;
	size_t len;

	if (!xml || !tag || !out || out_len == 0) {
		return -1;
	}

	snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
	snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

	start = strstr(xml, open_tag);
	if (!start) {
		return -1;
	}

	start += strlen(open_tag);
	end = strstr(start, close_tag);
	if (!end || end < start) {
		return -1;
	}

	len = (size_t)(end - start);
	if (len >= out_len) {
		len = out_len - 1;
	}

	memcpy(out, start, len);
	out[len] = '\0';
	return 0;
}

static int onvif_parse_int_tag(const char *xml, const char *tag, int *value)
{
	char buf[32] = {0};
	char *endptr = NULL;
	long v;

	if (!value) {
		return -1;
	}
	if (onvif_extract_xml_tag(xml, tag, buf, sizeof(buf)) != 0) {
		return -1;
	}

	v = strtol(buf, &endptr, 10);
	if (endptr == buf) {
		return -1;
	}

	*value = (int)v;
	return 0;
}

static void onvif_map_legacy_ptz_cmd(const char *legacy, char *mapped, size_t mapped_len)
{
	if (!legacy || !mapped || mapped_len == 0) {
		return;
	}

	if (strcmp(legacy, "left_up") == 0 || strcmp(legacy, "left_down") == 0) {
		snprintf(mapped, mapped_len, "%s", "left");
	} else if (strcmp(legacy, "right_up") == 0 || strcmp(legacy, "right_down") == 0) {
		snprintf(mapped, mapped_len, "%s", "right");
	} else if (strcmp(legacy, "ILLOn") == 0) {
		snprintf(mapped, mapped_len, "%s", "LensCoverOn");
	} else if (strcmp(legacy, "ILLOff") == 0) {
		snprintf(mapped, mapped_len, "%s", "LensCoverOff");
	} else {
		snprintf(mapped, mapped_len, "%.*s", mapped_len > 0 ? (int)mapped_len - 1 : 0, legacy);
	}
}

static int AuxMsgPTZCmd(const char *xml_cmd)
{
	EventResult result = {0};
	PtzCmdParse ptz_cmd = {0};
	char cmd[32] = {0};
	int pan_speed = 0;
	int tilt_speed = 0;

	if (!xml_cmd) {
		return -1;
	}

	if (onvif_extract_xml_tag(xml_cmd, "cmd", cmd, sizeof(cmd)) != 0) {
		return -1;
	}

	onvif_map_legacy_ptz_cmd(cmd, ptz_cmd.ptzCmd, sizeof(ptz_cmd.ptzCmd));

	if (onvif_parse_int_tag(xml_cmd, "panspeed", &pan_speed) == 0) {
		ptz_cmd.panSpeed = (unsigned char)((pan_speed < 0) ? 0 : (pan_speed > 255 ? 255 : pan_speed));
	}
	if (onvif_parse_int_tag(xml_cmd, "tiltspeed", &tilt_speed) == 0) {
		ptz_cmd.tiltSpeed = (unsigned char)((tilt_speed < 0) ? 0 : (tilt_speed > 255 ? 255 : tilt_speed));
	}
	onvif_parse_int_tag(xml_cmd, "preset", &ptz_cmd.presetID);
	onvif_parse_int_tag(xml_cmd, "flag", &ptz_cmd.flag);

	eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &result, (void *)&ptz_cmd);
	return result.ret;
}

static int AuxMsgPTZMove(int posx, int posy)
{
	EventResult result = {0};
	PtzCmdParse ptz_cmd = {0};

	strncpy(ptz_cmd.ptzCmd, "move3DPoint", sizeof(ptz_cmd.ptzCmd) - 1);
	ptz_cmd.panSpeed = (unsigned char)((posx < 0) ? 0 : (posx > 255 ? 255 : posx));
	ptz_cmd.tiltSpeed = (unsigned char)((posy < 0) ? 0 : (posy > 255 ? 255 : posy));

	eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &result, (void *)&ptz_cmd);
	return result.ret;
}

static int AuxMsgPTZCmdTransData(PtzTransCmd *pData)
{
	EventResult result = {0};
	PtzCmdParse ptz_cmd = {0};

	if (!pData) {
		return -1;
	}

	strncpy(ptz_cmd.ptzCmd, "TransparentCmd", sizeof(ptz_cmd.ptzCmd) - 1);
	memcpy(&ptz_cmd.trans, pData, sizeof(PtzTransCmd));
	eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &result, (void *)&ptz_cmd);
	return result.ret;
}

static int onvif_parse_preset_number_from_token(const char *token)
{
	const char *p = token;
	long v;

	if (!token) {
		return -1;
	}

	while (*p && !isdigit((unsigned char)*p)) {
		++p;
	}
	if (*p == '\0') {
		return -1;
	}

	v = strtol(p, NULL, 10);
	if (v <= 0 || v > 255) {
		return -1;
	}

	return (int)v;
}

static int onvif_find_preset_by_profile_token(const char *profile, const char *token)
{
	int i;

	if (!profile || !token) {
		return -1;
	}
	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			continue;
		}
		if (strcmp(g_onvif_preset_entries[i].profile_token, profile) == 0 &&
			strcmp(g_onvif_preset_entries[i].preset_token, token) == 0) {
			return i;
		}
	}
	return -1;
}

static int onvif_find_preset_by_profile_number(const char *profile, int preset_number)
{
	int i;

	if (!profile || preset_number <= 0) {
		return -1;
	}
	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			continue;
		}
		if (strcmp(g_onvif_preset_entries[i].profile_token, profile) == 0 &&
			g_onvif_preset_entries[i].preset_number == preset_number) {
			return i;
		}
	}
	return -1;
}

static int onvif_find_preset_home(const char *profile)
{
	int i;

	if (!profile) {
		return -1;
	}
	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			continue;
		}
		if (g_onvif_preset_entries[i].is_home &&
			strcmp(g_onvif_preset_entries[i].profile_token, profile) == 0) {
			return i;
		}
	}
	return -1;
}

static int onvif_alloc_preset_slot(void)
{
	int i;

	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			return i;
		}
	}
	return -1;
}

static int onvif_alloc_preset_number(const char *profile)
{
	int num;

	for (num = 1; num <= 255; ++num) {
		if (onvif_find_preset_by_profile_number(profile, num) < 0) {
			return num;
		}
	}
	return -1;
}

static int onvif_ptz_send_cmd(const char *cmd, int pan, int tilt, int preset, int flag)
{
	EventResult result = {0};
	PtzCmdParse ptz_cmd = {0};

	if (!cmd || cmd[0] == '\0') {
		return -1;
	}

	strncpy(ptz_cmd.ptzCmd, cmd, sizeof(ptz_cmd.ptzCmd) - 1);
	ptz_cmd.panSpeed = (unsigned char)((pan < 0) ? 0 : (pan > 255 ? 255 : pan));
	ptz_cmd.tiltSpeed = (unsigned char)((tilt < 0) ? 0 : (tilt > 255 ? 255 : tilt));
	ptz_cmd.presetID = preset;
	ptz_cmd.flag = flag;

	eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &result, (void *)&ptz_cmd);
	return result.ret;
}

static int onvif_norm_speed(int speed)
{
	if (speed == 11) {
		return 10;
	}
	if (speed < 0 || speed > 10) {
		return 5;
	}
	return speed;
}

static int PtzCmdHandle(int cmd, int panSpeed, int tiltSpeed, int presetId)
{
	panSpeed = onvif_norm_speed(panSpeed);
	tiltSpeed = onvif_norm_speed(tiltSpeed);

	switch (cmd) {
	case LENS_UP:
		return onvif_ptz_send_cmd("up", panSpeed, tiltSpeed, 0, 255);
	case LENS_LEFT:
		return onvif_ptz_send_cmd("left", panSpeed, tiltSpeed, 0, 255);
	case LENS_RIGHT:
		return onvif_ptz_send_cmd("right", panSpeed, tiltSpeed, 0, 255);
	case LENS_DOWN:
		return onvif_ptz_send_cmd("down", panSpeed, tiltSpeed, 0, 255);
	case LENS_LEFT_UP:
	case LENS_LEFT_DOWN:
		return onvif_ptz_send_cmd("left", panSpeed, tiltSpeed, 0, 255);
	case LENS_RIGHT_UP:
	case LENS_RIGHT_DOWN:
		return onvif_ptz_send_cmd("right", panSpeed, tiltSpeed, 0, 255);
	case LENS_NEAR:
		return onvif_ptz_send_cmd("zoomwide", 0, 0, 0, 0);
	case LENS_FAR:
		return onvif_ptz_send_cmd("zoomtele", 0, 0, 0, 0);
	case LENS_FOCUSNEAR:
		return onvif_ptz_send_cmd("FocusNearAutoOff", 0, 0, 0, 0);
	case LENS_FOCUSFAR:
		return onvif_ptz_send_cmd("FocusFarAutoOff", 0, 0, 0, 0);
	case LENS_DIAPHRAGM_LARGE:
		return onvif_ptz_send_cmd("IrisOpenAutoOff", 0, 0, 0, 0);
	case LENS_DIAPHRAGM_SMALL:
		return onvif_ptz_send_cmd("IrisCloseAutoOff", 0, 0, 0, 0);
	case LENS_PRESET_GOTO:
		return onvif_ptz_send_cmd("callpreset", 0, 0, presetId, 0);
	case LENS_PRESET_SET:
		return onvif_ptz_send_cmd("setpreset", 0, 0, presetId, 1);
	case LENS_PRESET_DEL:
		return onvif_ptz_send_cmd("clearpreset", 0, 0, presetId, 1);
	case LENS_LIGHT_ON:
		return onvif_ptz_send_cmd("LensCoverOn", 0, 0, 0, 0);
	case LENS_LIGHT_OFF:
		return onvif_ptz_send_cmd("LensCoverOff", 0, 0, 0, 0);
	case WIPER_PWRON:
	case WIPER_PWROFF:
	case LENS_AUTO:
		return 0;
	case LENS_STOP:
		return onvif_ptz_send_cmd("stop", 0, 0, 0, 0);
	default:
		return 0;
	}
}

static void show_preset_list(void)
{
	int i;

	pthread_mutex_lock(&g_onvif_preset_lock);
	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			continue;
		}
		log_print(HT_LOG_INFO, "preset[%d]: profile=%s token=%s num=%d home=%d\n",
			   i,
			   g_onvif_preset_entries[i].profile_token,
			   g_onvif_preset_entries[i].preset_token,
			   g_onvif_preset_entries[i].preset_number,
			   g_onvif_preset_entries[i].is_home);
	}
	pthread_mutex_unlock(&g_onvif_preset_lock);
}

static int add_preset(char *profileToken, char *presetName_onvif,
					  char *presetToken_onvif, char *responseToken, int isHome)
{
	int idx;
	int idx_by_no;
	int preset_no;
	char token_buf[64] = {0};
	char name_buf[64] = {0};
	const char *token;
	const char *name;

	if (!profileToken || !responseToken) {
		return -1;
	}

	token = (presetToken_onvif && presetToken_onvif[0]) ? presetToken_onvif : NULL;
	name = (presetName_onvif && presetName_onvif[0]) ? presetName_onvif : NULL;

	pthread_mutex_lock(&g_onvif_preset_lock);

	preset_no = token ? onvif_parse_preset_number_from_token(token) : -1;
	if (preset_no < 0) {
		preset_no = onvif_alloc_preset_number(profileToken);
	}
	if (preset_no < 0) {
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return -1;
	}

	if (!token) {
		snprintf(token_buf, sizeof(token_buf), "PresetToken%d", preset_no);
		token = token_buf;
	}
	if (!name) {
		snprintf(name_buf, sizeof(name_buf), "PresetName%d", preset_no);
		name = name_buf;
	}

	idx = onvif_find_preset_by_profile_token(profileToken, token);
	idx_by_no = onvif_find_preset_by_profile_number(profileToken, preset_no);
	if (idx < 0 && idx_by_no >= 0) {
		idx = idx_by_no;
	}
	if (idx < 0) {
		idx = onvif_alloc_preset_slot();
	}
	if (idx < 0) {
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return -1;
	}

	memset(&g_onvif_preset_entries[idx], 0, sizeof(g_onvif_preset_entries[idx]));
	g_onvif_preset_entries[idx].used = 1;
	g_onvif_preset_entries[idx].is_home = isHome ? 1 : 0;
	g_onvif_preset_entries[idx].preset_number = preset_no;
	snprintf(g_onvif_preset_entries[idx].profile_token,
			 sizeof(g_onvif_preset_entries[idx].profile_token),
			 "%.*s",
			 (int)(sizeof(g_onvif_preset_entries[idx].profile_token) - 1),
			 profileToken);
	snprintf(g_onvif_preset_entries[idx].preset_token,
			 sizeof(g_onvif_preset_entries[idx].preset_token),
			 "%.*s",
			 (int)(sizeof(g_onvif_preset_entries[idx].preset_token) - 1),
			 token);
	snprintf(g_onvif_preset_entries[idx].preset_name,
			 sizeof(g_onvif_preset_entries[idx].preset_name),
			 "%.*s",
			 (int)(sizeof(g_onvif_preset_entries[idx].preset_name) - 1),
			 name);

	if (isHome) {
		int i;
		for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
			if (i == idx || !g_onvif_preset_entries[i].used) {
				continue;
			}
			if (strcmp(g_onvif_preset_entries[i].profile_token, profileToken) == 0) {
				g_onvif_preset_entries[i].is_home = 0;
			}
		}
	}

	strncpy(responseToken, g_onvif_preset_entries[idx].preset_token,
			sizeof(g_onvif_preset_entries[idx].preset_token) - 1);
	responseToken[sizeof(g_onvif_preset_entries[idx].preset_token) - 1] = '\0';

	pthread_mutex_unlock(&g_onvif_preset_lock);

	return PtzCmdHandle(LENS_PRESET_SET, 0, 0, preset_no);
}

static int remove_preset(char *profileToken, char *presetToken)
{
	int idx;
	int preset_no;

	pthread_mutex_lock(&g_onvif_preset_lock);
	idx = onvif_find_preset_by_profile_token(profileToken, presetToken);
	if (idx < 0) {
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return -1;
	}

	preset_no = g_onvif_preset_entries[idx].preset_number;
	memset(&g_onvif_preset_entries[idx], 0, sizeof(g_onvif_preset_entries[idx]));
	pthread_mutex_unlock(&g_onvif_preset_lock);

	return PtzCmdHandle(LENS_PRESET_DEL, 0, 0, preset_no);
}

static int goto_preset(char *profileToken, char *presetToken)
{
	int idx;
	int preset_no;

	pthread_mutex_lock(&g_onvif_preset_lock);
	idx = onvif_find_preset_by_profile_token(profileToken, presetToken);
	if (idx < 0) {
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return -1;
	}

	preset_no = g_onvif_preset_entries[idx].preset_number;
	pthread_mutex_unlock(&g_onvif_preset_lock);

	return PtzCmdHandle(LENS_PRESET_GOTO, 0, 0, preset_no);
}

static int set_home_preset(char *profileToken)
{
	int i;
	int found_home = -1;
	char response_token[64] = {0};

	if (!profileToken) {
		return -1;
	}

	pthread_mutex_lock(&g_onvif_preset_lock);
	for (i = 0; i < ONVIF_PRESET_ENTRY_MAX; ++i) {
		if (!g_onvif_preset_entries[i].used) {
			continue;
		}
		if (strcmp(g_onvif_preset_entries[i].profile_token, profileToken) == 0 &&
			g_onvif_preset_entries[i].is_home) {
			found_home = i;
			break;
		}
	}

	if (found_home >= 0) {
		int preset_no = g_onvif_preset_entries[found_home].preset_number;
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return PtzCmdHandle(LENS_PRESET_SET, 0, 0, preset_no);
	}

	pthread_mutex_unlock(&g_onvif_preset_lock);

	return add_preset(profileToken, NULL, NULL, response_token, 1);
}

static int goto_home_preset(char *profileToken)
{
	int idx;
	int preset_no;

	pthread_mutex_lock(&g_onvif_preset_lock);
	idx = onvif_find_preset_home(profileToken);
	if (idx < 0) {
		pthread_mutex_unlock(&g_onvif_preset_lock);
		return -1;
	}
	preset_no = g_onvif_preset_entries[idx].preset_number;
	pthread_mutex_unlock(&g_onvif_preset_lock);

	return PtzCmdHandle(LENS_PRESET_GOTO, 0, 0, preset_no);
}

static void onvif_tour_spots_free_nolock(ptz_tour_t *ptour)
{
	tour_spot_entry_t *cur;
	tour_spot_entry_t *next;

	if (!ptour || !ptour->p_spot_head || !ptour->p_spot_last) {
		if (ptour) {
			ptour->p_spot_head = NULL;
			ptour->p_spot_last = NULL;
			ptour->spot_cnt = 0;
		}
		return;
	}

	cur = ptour->p_spot_head;
	while (cur != ptour->p_spot_last) {
		next = cur->next;
		free(cur);
		cur = next;
	}
	free(cur);

	ptour->p_spot_head = NULL;
	ptour->p_spot_last = NULL;
	ptour->spot_cnt = 0;
}

static int clear_tour_spots(ptz_tour_t *ptour)
{
	pthread_mutex_lock(&g_onvif_tour_lock);
	onvif_tour_spots_free_nolock(ptour);
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return 0;
}

static int add_ptz_spot(int tourIdx, int speed, int stayTime, char *presetToken)
{
	ptz_tour_t *ptour;
	tour_spot_entry_t *entry;

	if (!presetToken || tourIdx < 0 || tourIdx >= MAX_TOUR_CNT) {
		return -1;
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	ptour = &g_tour_ctx.ptz_tours[tourIdx];
	if (ptour->free) {
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return -1;
	}

	entry = (tour_spot_entry_t *)calloc(1, sizeof(tour_spot_entry_t));
	if (!entry) {
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return -1;
	}

	entry->tour_spot.speed = speed;
	entry->tour_spot.stay_time = stayTime;
	strncpy(entry->tour_spot.preset_token, presetToken,
			sizeof(entry->tour_spot.preset_token) - 1);

	if (!ptour->p_spot_head) {
		entry->next = entry;
		ptour->p_spot_head = entry;
		ptour->p_spot_last = entry;
	} else {
		entry->next = ptour->p_spot_head;
		ptour->p_spot_last->next = entry;
		ptour->p_spot_last = entry;
	}

	ptour->spot_cnt++;
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return 0;
}

int get_tour_cnt(void)
{
	int i;
	int cnt = 0;

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (!g_tour_ctx.ptz_tours[i].free) {
			cnt++;
		}
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return cnt;
}

ptz_tour_t *find_ptz_tour(char *tour_token)
{
	int i;

	if (!tour_token) {
		return NULL;
	}

	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (g_tour_ctx.ptz_tours[i].free) {
			continue;
		}
		if (strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) == 0) {
			return &g_tour_ctx.ptz_tours[i];
		}
	}
	return NULL;
}

static char *create_ptz_tour(void)
{
	int i;
	int free_idx = -1;

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (g_tour_ctx.ptz_tours[i].free) {
			free_idx = i;
			break;
		}
	}

	if (free_idx < 0) {
		for (i = 0; i < MAX_TOUR_CNT; ++i) {
			if (!g_tour_ctx.ptz_tours[i].active) {
				onvif_tour_spots_free_nolock(&g_tour_ctx.ptz_tours[i]);
				g_tour_ctx.ptz_tours[i].free = 1;
				free_idx = i;
				break;
			}
		}
	}

	if (free_idx >= 0) {
		g_tour_ctx.ptz_tours[free_idx].free = 0;
		g_tour_ctx.ptz_tours[free_idx].active = 0;
		g_tour_ctx.ptz_tours[free_idx].index = free_idx;
		snprintf(g_tour_ctx.ptz_tours[free_idx].token,
				 sizeof(g_tour_ctx.ptz_tours[free_idx].token),
				 "tour_%d", free_idx);
	}

	pthread_mutex_unlock(&g_onvif_tour_lock);
	return (free_idx >= 0) ? g_tour_ctx.ptz_tours[free_idx].token : NULL;
}

static void *onvif_tour_thread(void *arg)
{
	int tour_idx = (int)(intptr_t)arg;

	g_tour_ctx.b_tour_running = 1;
	while (!g_tour_ctx.b_stop_tour) {
		tour_spot_entry_t *head;
		tour_spot_entry_t *cur;

		pthread_mutex_lock(&g_onvif_tour_lock);
		if (tour_idx < 0 || tour_idx >= MAX_TOUR_CNT ||
			g_tour_ctx.ptz_tours[tour_idx].free) {
			pthread_mutex_unlock(&g_onvif_tour_lock);
			break;
		}
		head = g_tour_ctx.ptz_tours[tour_idx].p_spot_head;
		cur = head;
		pthread_mutex_unlock(&g_onvif_tour_lock);

		if (!head) {
			usleep(200 * 1000);
			continue;
		}

		do {
			int wait_ms;

			if (g_tour_ctx.b_stop_tour) {
				break;
			}

			goto_preset((char *)"MainStream", cur->tour_spot.preset_token);
			wait_ms = (cur->tour_spot.stay_time > 0) ? cur->tour_spot.stay_time : 1000;
			while (wait_ms > 0 && !g_tour_ctx.b_stop_tour) {
				int step = (wait_ms > 200) ? 200 : wait_ms;
				usleep((useconds_t)step * 1000);
				wait_ms -= step;
			}
			cur = cur->next;
		} while (cur && cur != head && !g_tour_ctx.b_stop_tour);
	}

	g_tour_ctx.b_tour_running = 0;
	return NULL;
}

static int stop_ptz_tour(char *tour_token)
{
	int i;
	int idx = -1;
	pthread_t tid = 0;

	if (!tour_token) {
		return -1;
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) == 0) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return -1;
	}
	if (!g_tour_ctx.ptz_tours[idx].active) {
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return 0;
	}

	g_tour_ctx.b_stop_tour = 1;
	tid = g_tour_ctx.tour_thrd_id;
	pthread_mutex_unlock(&g_onvif_tour_lock);

	if (tid) {
		pthread_join(tid, NULL);
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	g_tour_ctx.b_stop_tour = 0;
	g_tour_ctx.b_tour_running = 0;
	g_tour_ctx.tour_thrd_id = 0;
	g_tour_ctx.ptz_tours[idx].active = 0;
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return 0;
}

static int start_ptz_tour(char *tour_token)
{
	int i;
	int idx = -1;
	char stop_token[32] = {0};

	if (!tour_token) {
		return -1;
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (!g_tour_ctx.ptz_tours[i].free &&
			strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) == 0) {
			idx = i;
		}
		if (g_tour_ctx.ptz_tours[i].active &&
			strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) != 0) {
			strncpy(stop_token, g_tour_ctx.ptz_tours[i].token, sizeof(stop_token) - 1);
		}
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);

	if (idx < 0) {
		return -1;
	}

	if (stop_token[0] != '\0') {
		stop_ptz_tour(stop_token);
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	if (g_tour_ctx.ptz_tours[idx].active) {
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return 0;
	}

	g_tour_ctx.ptz_tours[idx].active = 1;
	g_tour_ctx.b_stop_tour = 0;
	if (pthread_create(&g_tour_ctx.tour_thrd_id, NULL, onvif_tour_thread,
					   (void *)(intptr_t)idx) != 0) {
		g_tour_ctx.ptz_tours[idx].active = 0;
		pthread_mutex_unlock(&g_onvif_tour_lock);
		return -1;
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return 0;
}

static int remove_ptz_tour(char *tour_token)
{
	int i;
	int idx = -1;

	if (!tour_token) {
		return -1;
	}

	stop_ptz_tour(tour_token);

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		if (strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) == 0) {
			idx = i;
			break;
		}
	}
	if (idx >= 0) {
		onvif_tour_spots_free_nolock(&g_tour_ctx.ptz_tours[idx]);
		g_tour_ctx.ptz_tours[idx].free = 1;
		g_tour_ctx.ptz_tours[idx].active = 0;
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);

	return (idx >= 0) ? 0 : -1;
}

int ptz_tour_init(void)
{
	int i;

	pthread_mutex_lock(&g_onvif_tour_lock);
	memset(&g_tour_ctx, 0, sizeof(g_tour_ctx));
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		g_tour_ctx.ptz_tours[i].index = i;
		g_tour_ctx.ptz_tours[i].free = 1;
		g_tour_ctx.ptz_tours[i].active = 0;
		snprintf(g_tour_ctx.ptz_tours[i].token,
				 sizeof(g_tour_ctx.ptz_tours[i].token),
				 "tour_%d", i);
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);
	return 0;
}

static int save_ptz_tours(void)
{
	FILE *fp = fopen(ONVIF_TOUR_FILE, "wb");
	int i;

	if (!fp) {
		return -1;
	}

	pthread_mutex_lock(&g_onvif_tour_lock);
	for (i = 0; i < MAX_TOUR_CNT; ++i) {
		ptz_tour_t *pt = &g_tour_ctx.ptz_tours[i];

		fwrite(&pt->free, sizeof(int), 1, fp);
		fwrite(pt->token, sizeof(pt->token), 1, fp);
		fwrite(&pt->spot_cnt, sizeof(int), 1, fp);
		if (!pt->free && pt->spot_cnt > 0 && pt->p_spot_head) {
			tour_spot_entry_t *cur = pt->p_spot_head;
			int written = 0;
			do {
				fwrite(&cur->tour_spot, sizeof(tour_spot_t), 1, fp);
				written++;
				cur = cur->next;
			} while (cur && cur != pt->p_spot_head && written < pt->spot_cnt);
		}
	}
	pthread_mutex_unlock(&g_onvif_tour_lock);
	fclose(fp);
	return 0;
}
/************************************************************************************
 *  	
 * The typical sequence of events is that first a client requests a certain preset. 
 * When the device accepts this request, it will send out an invoked event. 
 * The invoked event has to follow either a reached event or an aborted event. 
 * The former is used when the PTZ unit was able to reach the invoked preset position, 
 * the latter in any other case. A reached event has to follow a left event, 
 * as soon as the PTZ unit moves away from the preset position
 *
*************************************************************************************/
void onvif_PTZPresetsInvokedNotify(const char * token, onvif_PTZPreset * p_preset)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:PTZController/PTZPresets/Invoked", PropertyOperation_Changed, 
        "PTZConfigurationToken", token, NULL, NULL, 
        "PresetToken", p_preset->token, "PresetName", p_preset->Name);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_PTZPresetsReachedNotify(const char * token, onvif_PTZPreset * p_preset)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:PTZController/PTZPresets/Reached", PropertyOperation_Changed, 
        "PTZConfigurationToken", token, NULL, NULL, 
        "PresetToken", p_preset->token, "PresetName", p_preset->Name);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_PTZPresetsAbortedNotify(const char * token, onvif_PTZPreset * p_preset)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:PTZController/PTZPresets/Aborted", PropertyOperation_Changed, 
        "PTZConfigurationToken", token, NULL, NULL, 
        "PresetToken", p_preset->token, "PresetName", p_preset->Name);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_PTZPresetsLeftNotify(const char * token, onvif_PTZPreset * p_preset)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:PTZController/PTZPresets/Left", PropertyOperation_Changed, 
        "PTZConfigurationToken", token, NULL, NULL, 
        "PresetToken", p_preset->token, "PresetName", p_preset->Name);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/***************************************************************************************/

/**
 * @brief
 *  A PTZ-capable device shall be able to report its PTZ status through 
 *  the GetStatus command.
 *
 *  The PTZ status contains the following information:
 *
 *  Position (optional) - Specifies the absolute position of the PTZ unit
 *  together with the space references. The default absolute spaces of the
 *  corresponding PTZ configuration shall be referenced within the position
 *  element. This information shall be present if the device signals support
 *  via the capability StatusPosition.
 *
 *  MoveStatus (optional) - Indicates if the pan/tilt/zoom device unit is 
 *  currently moving, idle or in an unknown state. This information shall be
 *  present if the device signals support via the capability MoveStatus. 
 *  The state Unknown shall not be used during normal operation, but is 
 *  reserved to initialization or error conditions.
 *
 *  Error (optional) - States a current PTZ error condition. This field
 *  shall be present if the MoveStatus signals Unkown.
 *
 *  UTC Time - Specifies the UTC time when this status was generated.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoStatus
 **/
ONVIF_RET onvif_ptz_GetStatus(onvif_PTZStatus * p_ptz_status)
{
	time_t now = time(NULL);
	int moving = (g_ptz_last_move_time > 0 && (now - g_ptz_last_move_time) <= 2);

	if (1)
	{
		p_ptz_status->PositionFlag = 1;
		p_ptz_status->Position.PanTiltFlag = 1;
		p_ptz_status->Position.PanTilt.x = g_tptz_value_x;
		p_ptz_status->Position.PanTilt.y = g_tptz_value_y;
		p_ptz_status->Position.ZoomFlag = 1;
		p_ptz_status->Position.Zoom.x = g_tptz_value_z;
	}
	
	p_ptz_status->MoveStatusFlag = 1;
	p_ptz_status->MoveStatus.PanTiltFlag = 1;
	p_ptz_status->MoveStatus.PanTilt = moving ? MoveStatus_MOVING : MoveStatus_IDLE;
	p_ptz_status->MoveStatus.ZoomFlag = 1;
	p_ptz_status->MoveStatus.Zoom = moving ? MoveStatus_MOVING : MoveStatus_IDLE;

	p_ptz_status->ErrorFlag = 0;
	p_ptz_status->UtcTime = time(NULL);
	
	return ONVIF_OK;
}

ONVIF_RET onvif_ptz_GetStatus_old(ONVIF_PROFILE * p_profile, onvif_PTZStatus * p_ptz_status)
{
	if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
	// todo : add get ptz status code ...
	if (1)
	{
		p_ptz_status->PositionFlag = 1;
		p_ptz_status->Position.PanTiltFlag = 1;
		p_ptz_status->Position.PanTilt.x = g_tptz_value_x;
		p_ptz_status->Position.PanTilt.y = g_tptz_value_y;
		p_ptz_status->Position.ZoomFlag = 1;
		p_ptz_status->Position.Zoom.x = g_tptz_value_z;
	}
	
	p_ptz_status->MoveStatusFlag = 1;
	p_ptz_status->MoveStatus.PanTiltFlag = 1;
	p_ptz_status->MoveStatus.PanTilt = MoveStatus_IDLE;
	p_ptz_status->MoveStatus.ZoomFlag = 1;
	p_ptz_status->MoveStatus.Zoom = MoveStatus_IDLE;

	p_ptz_status->ErrorFlag = 0;
	p_ptz_status->UtcTime = time(NULL);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A PTZ-capable device shall support continuous movements. The velocity 
 *  argument of this command specifies a signed speed value for the pan, 
 *  tilt and zoom. The combined pan/tilt element is optional and the Zoom
 *  element itself is optional. If the pan/tilt element is omitted, the 
 *  current pan/tilt movement shall not be affected by this command. The 
 *  same holds for the zoom element. The spaces referenced within the 
 *  velocity element shall be velocity spaces supported by the PTZ node.
 *  If the space information is omitted for the velocity argument, the 
 *  corresponding default spaces of the PTZ configuration belonging to 
 *  the specified media profile is used. A device may support continuous 
 *  pan/tilt movements and/or continuous zoom movements by providing only
 *  velocity spaces for the supported cases.
 *
 *  An existing timeout argument overrides the DefaultPTZTimeout parameter
 *  of the corresponding PTZ configuration for this Move operation. 
 *  The timeout parameter specifies how long the PTZ node continues to move.
 *
 *  A device shall stop movement in a particular axis (Pan, Tilt, or Zoom) 
 *  when zero is sent as the ContinuousMove parameter for that axis. Stopping
 *  shall have the same effect independent of the velocity space referenced.
 *
 *  If the requested velocity leads to absolute positions which cannot be 
 *  reached, the PTZ node shall move to a reachable position along the border
 *  of its range. A typical application of the continuous move operation is
 *  controlling PTZ via joystick.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidTranslation
 *  ONVIF_ERR_TimeoutNotSupported
 *  ONVIF_ERR_InvalidVelocity
 **/
ONVIF_RET onvif_ptz_ContinuousMove(ptz_ContinuousMove_REQ * p_req)
{
	PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
	if (p_req->Velocity.PanTiltFlag)
	{
		if (p_req->Velocity.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Min < -FPP || 
		 	p_req->Velocity.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidVelocity;
		}

		if (p_req->Velocity.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Min < -FPP || 
			p_req->Velocity.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidVelocity;
		}
	}
	
	if (p_req->Velocity.ZoomFlag && 
		(p_req->Velocity.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Min < -FPP || 
		 p_req->Velocity.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidVelocity;
	}
	int x_speed = 0, y_speed = 0, z_speed = 0;
	if(p_req->Velocity.PanTiltFlag)
	{
		if((p_req->Velocity.PanTilt.x > 0.00000 && p_req->Velocity.PanTilt.x <= 1.00000) || 
			(p_req->Velocity.PanTilt.x < 0.00000 && p_req->Velocity.PanTilt.x >= -1.00000))
		{
			x_speed = (int)(p_req->Velocity.PanTilt.x * 10 + (p_req->Velocity.PanTilt.x < 0.00000 ? -1 : 1));
		}
		else
		{
			x_speed = (int)p_req->Velocity.PanTilt.x;
		}

		if((p_req->Velocity.PanTilt.y > 0.00000 && p_req->Velocity.PanTilt.y <= 1.00000) || 
			(p_req->Velocity.PanTilt.y < 0.00000 && p_req->Velocity.PanTilt.y >= -1.00000))
		{
			y_speed = (int)(p_req->Velocity.PanTilt.y * 10 + (p_req->Velocity.PanTilt.y < 0.00000 ? -1 : 1));
		}
		else
		{
			y_speed = (int)p_req->Velocity.PanTilt.y;
		}
	}
	if(p_req->Velocity.ZoomFlag)
	{
		if((p_req->Velocity.Zoom.x > 0.00000 && p_req->Velocity.Zoom.x <= 1.00000)||
			(p_req->Velocity.Zoom.x < 0.00000 && p_req->Velocity.Zoom.x >= -1.00000))
		{
			z_speed = (int)(p_req->Velocity.Zoom.x * 10 + (p_req->Velocity.Zoom.x < 0.00000 ? -1 : 1));
		}
		else
		{
			z_speed = (int)p_req->Velocity.Zoom.x;
		}
	}
	
	if (g_absolute_move)
	{
		system("rm /tmp/onvif_tptz_absloute_move_data");
	}
	if(x_speed > 0)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_RIGHT_UP, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_RIGHT_DOWN, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			PtzCmdHandle(LENS_RIGHT, abs(x_speed), 0, 0);
		}
	}
	if(x_speed < 0)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_LEFT_UP, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_LEFT_DOWN, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			PtzCmdHandle(LENS_LEFT, abs(x_speed), 0, 0);
		}
	}
	if(x_speed >= -EPSINON && x_speed<= EPSINON)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_UP, 0, abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_DOWN, 0, abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			if(z_speed >= -EPSINON && z_speed <= EPSINON)
			{
				PtzCmdHandle(LENS_STOP, 0, 0, 0);
			}
		}
	}
	if(z_speed > 0)
	{
		PtzCmdHandle(LENS_FAR, 0, 0, 0);
	}
	if(z_speed < 0)
	{
		PtzCmdHandle(LENS_NEAR, 0, 0, 0);
	}

	onvif_ptz_mark_move((x_speed != 0) || (y_speed != 0) || (z_speed != 0));
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A PTZ-capable device shall support the Stop operation. If no stop filter 
 *  arguments are present, this command stops all ongoing pan, tilt and zoom
 *  movements. The Stop operation can be filtered to stop a specific movement
 *  by setting the corresponding stop argument.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 **/
ONVIF_RET onvif_ptz_Stop(ptz_Stop_REQ * p_req)
{
	/*ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}*/

	if( g_bHxVersion )
	{
		if(g_tptz_start == 1)
		{
			stop_ptz_tour(g_tptz_tourtoken);
			g_tptz_start = 0;
			usleep(100 * 1000);
		}
	}
	if (g_absolute_move)
	{
		//---------------------------------------------------
		//再发一次获取球机坐标查询
		PtzTransCmd *Pelcod_D;
		Pelcod_D = malloc(sizeof(PtzTransCmd));
		Pelcod_D->datalen = 7;
		memset(Pelcod_D->buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
		Pelcod_D->buffer[0] = 0xff;
		Pelcod_D->buffer[1] = 0x01;
		Pelcod_D->buffer[2] = 0x9a;
		Pelcod_D->buffer[3] = 0x00;
		Pelcod_D->buffer[4] = 0x00;
		Pelcod_D->buffer[5] = 0x00;
		Pelcod_D->buffer[6] = 0x9b;
		Pelcod_D->buffer[7] = '\0';
		//写入到/tmp/pelcod_cood
		AuxMsgPTZCmdTransData(Pelcod_D);
		log_print(HT_LOG_INFO, "Pelcod_D send ok!\n");
		free(Pelcod_D);
	}
	PtzCmdHandle(LENS_STOP,0,0,0);
	onvif_ptz_mark_move(0);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  If a PTZ node supports absolute pan/tilt or absolute zoom movements, 
 *  it shall support the AbsoluteMove operation. Theposition argument of
 *  this command specifies the absolute position to which the PTZ unit 
 *  moves. It splits into an optional pan/tilt element and an optional 
 *  zoom element. If the pan/tilt position is omitted, the current 
 *  pan/tilt movement shall not be affected by this command. The same 
 *  holds for the zoom position.
 *
 *  The spaces referenced within the position shall be absolute position 
 *  spaces supported by the PTZ node. If the space information is omitted, 
 *  the corresponding default spaces of the PTZ configuration, a part of 
 *  the specified media profile, is used. A device may support absolute 
 *  pan/tilt movements, absolute zoom movements or no absolute movements by
 *  providing only absolute position spaces for the supported cases.
 *
 *  An existing Speed argument overrides DefaultSpeed of the corresponding 
 *  PTZ configuration during movement to the requested position. If spaces 
 *  are referenced within the Speed argument, they shall be speed spaces 
 *  supported by the PTZ node.
 *
 *  The operation shall fail if the requested absolute position is not reachable.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidPosition
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_AbsoluteMove(ptz_AbsoluteMove_REQ * p_req)
{
	PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
	if (p_req->Position.PanTiltFlag)
	{
		if (p_req->Position.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min < -FPP || 
		 	p_req->Position.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidPosition;
		}

		if (p_req->Position.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min < -FPP || 
			p_req->Position.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidPosition;
		}
	}

	if (p_req->Position.ZoomFlag && 
		(p_req->Position.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Min < -FPP || 
		 p_req->Position.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidPosition;
	}
	
	if(g_product_type == PRODUCT_TYPE_MYE10 || g_product_type == PRODUCT_TYPE_MYE10Q || g_product_type == PRODUCT_TYPE_MYE40 || g_product_type == PRODUCT_TYPE_MYE18 || g_product_type == PRODUCT_TYPE_MYD20)
	{
		int x_cood = 0, y_cood = 0;
		x_cood = (int )(((p_req->Position.PanTilt.x + 1) / 2) *100);
		y_cood = (int )(((p_req->Position.PanTilt.y + 1) / 2) *100);
		if (0 == AuxMsgPTZMove(x_cood, y_cood))
		{
			onvif_ptz_mark_move(1);
			log_print(HT_LOG_INFO, "move succ\n");
			return ONVIF_OK;
		}
		else
			return ONVIF_ERR_GENERAL;
	}
	else if(g_absolute_move)
	{
		//特殊定制记录PTZ的Status
		g_tptz_value_x = p_req->Position.PanTilt.x;
		g_tptz_value_y = p_req->Position.PanTilt.y;
		g_tptz_value_z = p_req->Position.Zoom.x;
		
		//特殊定制记录PTZ的Status

		PtzTransCmd *Yuntai_body;
		int i = 0;

		int x_cood = 0, y_cood = 0, z_cood = 0;
		int x_speed = 100,y_speed = 100,z_speed = 100;
		//int Speed = 0;

		x_cood = (((int)((p_req->Position.PanTilt.x)*100)) + 100 )/2;
		y_cood = (((int)((p_req->Position.PanTilt.y)*100)) + 100 )/2;
		z_cood = (((int)((p_req->Position.Zoom.x)*100)) + 100 )/2;
		x_cood = x_cood * 65535 / 100 ;
		y_cood = y_cood * 65535 / 100 ;
		z_cood = z_cood * 65535 / 100 ;
		log_print(HT_LOG_INFO, "x_cood:%d\n", x_cood);
		log_print(HT_LOG_INFO, "y_cood:%d\n", y_cood);
		log_print(HT_LOG_INFO, "z_cood:%d\n", z_cood);
		if (p_req->SpeedFlag)
		{
			if(p_req->Speed.PanTiltFlag)
			{
				x_speed = (((int)((p_req->Speed.PanTilt.x)*100)) + 100 )/2;
				y_speed = (((int)((p_req->Speed.PanTilt.y)*100)) + 100 )/2;
			}
			if (p_req->Speed.ZoomFlag)
			{
				z_speed = (((int)((p_req->Speed.Zoom.x)*100)) + 100 )/2;
			}
		}
		
		x_speed = x_speed * 255 / 100 ;
		y_speed = y_speed * 255 / 100 ;
		z_speed = z_speed * 255 / 100 ;
		
		log_print(HT_LOG_INFO, "x:%d\n", x_speed);
		log_print(HT_LOG_INFO, "y:%d\n", y_speed);
		log_print(HT_LOG_INFO, "z:%d\n", z_speed);
		
		Yuntai_body = malloc(sizeof(PtzTransCmd));
		Yuntai_body->datalen = 13;
		memset(Yuntai_body->buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
		
		Yuntai_body->buffer[0] = 0xff;
		Yuntai_body->buffer[1] = 0x01;
		Yuntai_body->buffer[2] = 0x99;	
		
		Yuntai_body->buffer[3] = x_cood >> 8;
		Yuntai_body->buffer[4] = x_cood;
		
		Yuntai_body->buffer[5] = y_cood >> 8;
		Yuntai_body->buffer[6] = y_cood;
		
		Yuntai_body->buffer[7] = x_speed;
		Yuntai_body->buffer[8] = y_speed;
		
		Yuntai_body->buffer[9] = z_cood >> 8;
		Yuntai_body->buffer[10] = z_cood;
		Yuntai_body->buffer[11] = z_speed;
		Yuntai_body->buffer[12] = 0;	
		Yuntai_body->buffer[13] = '\0';	
		//校准位
		for (i=1; i < 12; i++)
		{
			Yuntai_body->buffer[12]  += Yuntai_body->buffer[i];
		}
		Yuntai_body->buffer[12] = Yuntai_body->buffer[12] & 0xff;
		AuxMsgPTZCmdTransData(Yuntai_body);
		free(Yuntai_body);
		onvif_ptz_mark_move(1);
		//删掉上一次坐标信息文件,用于更新坐标
		system("rm /tmp/onvif_tptz_absloute_move_data");
		usleep(2000*1000);
		//---------------------------------------------------
		//再发一次获取球机坐标查询
		PtzTransCmd *Pelcod_D;
		Pelcod_D = malloc(sizeof(PtzTransCmd));
		Pelcod_D->datalen = 7;
		memset(Pelcod_D->buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
		Pelcod_D->buffer[0] = 0xff;
		Pelcod_D->buffer[1] = 0x01;
		Pelcod_D->buffer[2] = 0x9a;
		Pelcod_D->buffer[3] = 0x00;
		Pelcod_D->buffer[4] = 0x00;
		Pelcod_D->buffer[5] = 0x00;
		Pelcod_D->buffer[6] = 0x9b;
		Pelcod_D->buffer[7] = '\0';
		//写入到/tmp/pelcod_cood
		AuxMsgPTZCmdTransData(Pelcod_D);
		log_print(HT_LOG_INFO, "Pelcod_D send ok!\n");
		free(Pelcod_D);
		//---------------------------------------------------
	}
	else
	{
		return ONVIF_ERR_ServiceNotSupported;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  If a PTZ node supports relative pan/tilt or relative zoom movements, 
 *  then it shall support the RelativeMove operation. The translation 
 *  argument of this operation specifies the difference from the current
 *  position to the position to which the PTZ device is instructed to move. 
 *  The operation is split into an optional pan/tilt element and an optional
 *  zoom element. If the pan/tilt element is omitted, the current pan/tilt 
 *  movement shall NOT be affected by this command. The same holds for the 
 *  zoom element.
 *
 *  The spaces referenced within the translation element shall be translation
 *  spaces supported by the PTZ node. If the space information is omitted
 *  for the translation argument, the corresponding default spaces of the 
 *  PTZ configuration, which is part of the specified media profile, is used.
 *  A device may support relative pan/tilt movements, relative Zoom movements
 *  or no relative movements by providing only translation spaces for the 
 *  supported cases.
 *
 *  An existing speed argument overrides DefaultSpeed of the corresponding 
 *  PTZ configuration during movement by the requested translation. 
 *  If spaces are referenced within the speed argument, they shall be speed 
 *  spaces supported by the PTZ node.
 *  
 *  The command can be used to stop the PTZ unit at its current position by
 *  sending zero values for pan/tilt and zoom. Stopping shall have the very
 *  same effect independent of the relative space referenced.
 *
 *  If the requested translation leads to an absolute position which cannot
 *  be reached, the PTZ node shall move to a reachable position along the 
 *  border of valid positions.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidTranslation
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_RelativeMove(ptz_RelativeMove_REQ * p_req)
{
	PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	if (p_req->Translation.PanTiltFlag)
	{
		if (p_req->Translation.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Min < -FPP || 
			p_req->Translation.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidTranslation;
		}

		if (p_req->Translation.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Min < -FPP || 
			p_req->Translation.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidTranslation;
		}
	}

	if (p_req->Translation.ZoomFlag && 
		(p_req->Translation.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Min < -FPP || 
		 p_req->Translation.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidTranslation;
	}

	// todo : here add handler code ...
	if (g_absolute_move)
	{
		if( (int)p_req->Translation.PanTilt.x > 1 ||
			(int)p_req->Translation.PanTilt.y > 1 ||
			(int)p_req->Translation.Zoom.x > 1 ||
			(int)p_req->Translation.PanTilt.x < -1 ||
			(int)p_req->Translation.PanTilt.y < -1 ||
			(int)p_req->Translation.Zoom.x < -1)
		{
			return ONVIF_ERR_InvalidArgVal;
		}
		/*发送查询请求*/
		PtzTransCmd *Pelcod_D;
		Pelcod_D = malloc(sizeof(PtzTransCmd));
		Pelcod_D->datalen = 7;
		memset(Pelcod_D->buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
		Pelcod_D->buffer[0] = 0xff;
		Pelcod_D->buffer[1] = 0x01;
		Pelcod_D->buffer[2] = 0x9a;
		Pelcod_D->buffer[3] = 0x00;
		Pelcod_D->buffer[4] = 0x00;
		Pelcod_D->buffer[5] = 0x00;
		Pelcod_D->buffer[6] = 0x9b;
		Pelcod_D->buffer[7] = '\0';
		AuxMsgPTZCmdTransData(Pelcod_D);
		usleep(50*1000);
		AuxMsgPTZCmdTransData(Pelcod_D);
		usleep(50*1000);
		AuxMsgPTZCmdTransData(Pelcod_D);
		usleep(100*1000);
		free(Pelcod_D);
		/*发送查询请求*/
		
		int x_speed = 100,y_speed = 100,z_speed = 100;
		int i = 0;
		int cur_x = 0, cur_y = 0, cur_z = 0;//当前位置数据
		int cmd_x = 0, cmd_y = 0, cmd_z = 0;//cmd位置数据
		int last_x = 0, last_y = 0, last_z = 0;//last位置数据
		
		cmd_x = (int)((p_req->Translation.PanTilt.x)*32767.5);
		cmd_y = (int)((p_req->Translation.PanTilt.y)*32767.5);
		cmd_z = (int)((p_req->Translation.Zoom.x)*32767.5);
		log_print(HT_LOG_INFO, "cmd_x:%d, cmd_y:%d, cmd_z:%d\n", cmd_x, cmd_y, cmd_z);
		
		if (p_req->SpeedFlag)
		{
			if(p_req->Speed.PanTiltFlag)
			{
				x_speed = (((int)((p_req->Speed.PanTilt.x)*100)) + 100 )/2;
				y_speed = (((int)((p_req->Speed.PanTilt.y)*100)) + 100 )/2;
			}
			if (p_req->Speed.ZoomFlag)
			{
				z_speed = (((int)((p_req->Speed.Zoom.x)*100)) + 100 )/2;
			}
		}
		x_speed = x_speed * 255 / 100 ;
		y_speed = y_speed * 255 / 100 ;
		z_speed = z_speed * 255 / 100 ;
		
		int Get_times = 0;
		int Check_data = 0;
		while(1)
		{
			if (access("/tmp/onvif_tptz_absloute_move_data", F_OK) == 0 && access("/tmp/of_ab_data_read_over", F_OK) == 0)
			{
				char over_buf[2] = {0};
				read_file_to_buffer("/tmp/of_ab_data_read_over", over_buf, 1);
				if(strcmp(over_buf, "1") == 0)
				{
					unsigned int buffer[11] = {0};
					char *data = malloc(128);
					memset(data, 0, 128);
					read_file_to_buffer("/tmp/onvif_tptz_absloute_move_data", data, 128);
					sscanf(data, "%02x,%02x,%02x,%02x,%02x,%02x,%02x", &buffer[0], &buffer[1], &buffer[2], &buffer[3], &buffer[4],
																	&buffer[5], &buffer[6]);
					if(strlen(data) != 20)//确定大小
					{
						log_print(HT_LOG_ERR, "__tptz__GetStatus strlen(data) != 20\n");
						free(data);
						continue;
					}
					cur_x = (buffer[0]<<8) + buffer[1];
					cur_y = (buffer[2]<<8) + buffer[3];
					cur_z = (buffer[4]<<8) + buffer[5];
					log_print(HT_LOG_INFO, "cur_x:%d, cur_y:%d, cur_z:%d\n", cur_x, cur_y, cur_z);
					Check_data = 1;
					free(data);
					break;
				}
				else
				{
					usleep(50*1000);
					Get_times++;
					if(Get_times > 60)
					{
						log_print(HT_LOG_ERR, "Get_times > 60\n");
						break;
					}
				}
			}
			else
			{
				usleep(50*1000);
				Get_times++;
				if(Get_times > 60)
				{
					log_print(HT_LOG_ERR, "Get_times > 60\n");
					break;
				}
			}
		}
		if (Check_data == 1)
		{
			last_x = cur_x + cmd_x;
			last_y = cur_y + cmd_y;
			last_z = cur_z + cmd_z;
			if (last_x >= 65280)
				last_x = 65280;
			if (last_y >= 65535)
				last_y = 65535;
			if (last_z >= 65535)
				last_z = 65535;
			if (last_x <= 255)
				last_x = 255;
			if (last_y <= 0)
				last_y = 0;
			if (last_z <= 0)
				last_z = 0;
			log_print(HT_LOG_INFO, "last_x:%d, last_y:%d, last_z:%d\n", last_x, last_y, last_z);

			PtzTransCmd *Yuntai_body;
			Yuntai_body = malloc(sizeof(PtzTransCmd));
			Yuntai_body->datalen = 13;
			memset(Yuntai_body->buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
			
			Yuntai_body->buffer[0] = 0xff;
			Yuntai_body->buffer[1] = 0x01;
			Yuntai_body->buffer[2] = 0x99;	
			
			Yuntai_body->buffer[3] = last_x >> 8;
			Yuntai_body->buffer[4] = last_x;
			
			Yuntai_body->buffer[5] = last_y >> 8;
			Yuntai_body->buffer[6] = last_y;
			
			Yuntai_body->buffer[7] = x_speed;
			Yuntai_body->buffer[8] = y_speed;
			
			Yuntai_body->buffer[9] = last_z >> 8;
			Yuntai_body->buffer[10] = last_z;
			Yuntai_body->buffer[11] = z_speed;
			Yuntai_body->buffer[12] = 0;	
			Yuntai_body->buffer[13] = '\0'; 
			//校准位
			for (i=1; i < 12; i++)
			{
				Yuntai_body->buffer[12]  += Yuntai_body->buffer[i];
			}
			Yuntai_body->buffer[12] = Yuntai_body->buffer[12] & 0xff;
			AuxMsgPTZCmdTransData(Yuntai_body);
			usleep(500*1000);
			AuxMsgPTZCmdTransData(Yuntai_body);
			usleep(500*1000);
			AuxMsgPTZCmdTransData(Yuntai_body);
			free(Yuntai_body);
			
			return ONVIF_OK;
		}
		else
		{
			return ONVIF_ERR_InvalidArgVal;
		}
	}
	else
	{
		int x_speed = 0,y_speed = 0,z_speed = 0;

		if(p_req->Translation.PanTiltFlag)
		{
			if((p_req->Translation.PanTilt.x > 0.00000 && p_req->Translation.PanTilt.x <= 1.00000)||
				(p_req->Translation.PanTilt.x < 0.00000 && p_req->Translation.PanTilt.x >= -1.00000))
			{
				x_speed = (int)(p_req->Translation.PanTilt.x * 10 + (p_req->Translation.PanTilt.x < 0.00000 ? -1 : 1));
			}
			else
			{
				x_speed = (int)p_req->Translation.PanTilt.x;
			}

			if((p_req->Translation.PanTilt.y > 0.00000 && p_req->Translation.PanTilt.y <= 1.00000)||
				(p_req->Translation.PanTilt.y < 0.00000 && p_req->Translation.PanTilt.y >= -1.00000))
			{
				y_speed = (int)(p_req->Translation.PanTilt.y * 10 + (p_req->Translation.PanTilt.y < 0.00000 ? -1 : 1));
			}
			else
			{
				y_speed = (int)p_req->Translation.PanTilt.y;
			}
		}
		if(p_req->Translation.ZoomFlag)
		{
			if((p_req->Translation.Zoom.x > 0.00000 && p_req->Translation.Zoom.x <= 1.00000)||
					(p_req->Translation.Zoom.x < 0.00000 && p_req->Translation.Zoom.x >= -1.00000))
			{
				z_speed = (int)(p_req->Translation.Zoom.x * 10 + (p_req->Translation.Zoom.x < 0.00000 ? -1 : 1));
			}
			else
			{
				z_speed = (int)p_req->Translation.Zoom.x;
			}
		}
		
		if(x_speed > 0)
		{
			if(y_speed > 0)
			{
				PtzCmdHandle(LENS_RIGHT_UP, abs(x_speed), abs(y_speed), 0);
			}
			if(y_speed < 0)
			{
				PtzCmdHandle(LENS_RIGHT_DOWN, abs(x_speed), abs(y_speed), 0);
			}
			if(y_speed >= -EPSINON && y_speed<= EPSINON)
			{
				PtzCmdHandle(LENS_RIGHT, abs(x_speed), 0, 0);
			}
		}
		if(x_speed < 0)
		{
			if(y_speed > 0)
			{
				PtzCmdHandle(LENS_LEFT_UP, abs(x_speed), abs(y_speed), 0);
			}
			if(y_speed < 0)
			{
				PtzCmdHandle(LENS_LEFT_DOWN, abs(x_speed), abs(y_speed), 0);
			}
			if(y_speed >= -EPSINON && y_speed<= EPSINON)
			{
				PtzCmdHandle(LENS_LEFT, abs(x_speed), 0, 0);
			}
		}
		if(x_speed >= -EPSINON && x_speed<= EPSINON)
		{
			if(y_speed > 0)
			{
				PtzCmdHandle(LENS_UP, 0, abs(y_speed), 0);
			}
			if(y_speed < 0)
			{
				PtzCmdHandle(LENS_DOWN, 0, abs(y_speed), 0);
			}
			if(y_speed >= -EPSINON && y_speed<= EPSINON)
			{
				if(z_speed >= -EPSINON && z_speed <= EPSINON)
				{
					PtzCmdHandle(LENS_STOP, 0, 0, 0);
				}
			}
		}
		if(z_speed > 0)
		{
			PtzCmdHandle(LENS_FAR, 0, 0, 0);
		}
		if(z_speed < 0)
		{
			PtzCmdHandle(LENS_NEAR, 0, 0, 0);
		}

		onvif_ptz_mark_move((x_speed != 0) || (y_speed != 0) || (z_speed != 0));
	}
	

	return ONVIF_OK;
}

/**
 * @brief
 *  The SetPreset command saves the current device position parameters so 
 *  that the device can move to the saved preset position through the 
 *  GotoPreset operation.
 *
 *  If the PresetToken parameter is absent, the device shall create a new
 *  preset. Otherwise it shall update the stored position and optionally 
 *  the name of the given preset. If creation is successful, the response
 *  contains the PresetToken which uniquely identifies the preset. 
 *  An existing preset can be overwritten by specifying the PresetToken of
 *  the corresponding preset. In both cases (overwriting or creation) an 
 *  optional PresetName can be specified. The operation fails if the PTZ 
 *  device is moving during the SetPreset operation.
 *
 *  The device may internally save additional states such as imaging 
 *  properties in the PTZ preset which then should be recalled
 *  in the GotoPreset operation. A device shall accept a valid 
 *  SetPresetRequest that does not include the optional element
 *  PresetName.
 *
 *  Devices may require unique preset names and reject a request that 
 *  contains an already existing PresetName by responding with the error
 *  message ter:PresetExist.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_PresetExist
 *  ONVIF_ERR_InvalidPresetName
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_MovingPTZ
 *  ONVIF_ERR_TooManyPresets
 **/
ONVIF_RET onvif_ptz_SetPreset(ptz_SetPreset_REQ * p_req)
{
	PTZPresetList * p_preset = NULL;
	ONVIF_PROFILE * p_profile;

	p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	if (p_req->PresetTokenFlag && p_req->PresetToken[0] != '\0')
	{
		p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
		if (NULL == p_preset)
		{
			return ONVIF_ERR_NoToken;
		}
	}
	else
	{
		p_preset = onvif_add_PTZPreset(&p_profile->presets);
		if (NULL == p_preset)
		{
			return ONVIF_ERR_TooManyPresets;
		}
	}

	if (p_req->PresetNameFlag && p_req->PresetName[0] != '\0')
	{
		strcpy(p_preset->PTZPreset.Name, p_req->PresetName);
	}
	else
	{
		strcpy(p_req->PresetName, p_preset->PTZPreset.Name);
	}

	if (p_req->PresetTokenFlag && p_req->PresetToken[0] != '\0')
	{
		strcpy(p_preset->PTZPreset.token, p_req->PresetToken);
	}
	else
	{
		strcpy(p_req->PresetToken, p_preset->PTZPreset.token);
	}

	// todo : get PTZ current position ...
	
	char presetToken[256]="";
	
	show_preset_list();
	
	int ret = add_preset(p_req->ProfileToken, p_preset->PTZPreset.Name, p_preset->PTZPreset.token, presetToken, 0); //not home

	show_preset_list();

	
	p_preset->PTZPreset.PTZPositionFlag = 1;
	p_preset->PTZPreset.PTZPosition.PanTiltFlag = 1;
	p_preset->PTZPreset.PTZPosition.PanTilt.x = 0;
	p_preset->PTZPreset.PTZPosition.PanTilt.y = 0;
	p_preset->PTZPreset.PTZPosition.ZoomFlag = 1;
	p_preset->PTZPreset.PTZPosition.Zoom.x = 0;

	if(ret >= 0)
		return ONVIF_OK;
	else
		return ONVIF_ERR_NoToken;
}

/**
 * @brief
 *  The RemovePreset operation removes a previously set preset.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_RemovePreset(ptz_RemovePreset_REQ * p_req)
{
	ONVIF_PROFILE * p_profile;
	PTZPresetList * p_preset;

	p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
	if (NULL == p_preset)
	{
		return ONVIF_ERR_NoToken;
	}
	// add my code
	
	show_preset_list();
	
	int ret = remove_preset(p_req->ProfileToken, p_req->PresetToken);

	show_preset_list();
	
	onvif_free_PTZPreset(&p_profile->presets, p_preset);

	if(ret >= 0)
		return ONVIF_OK;
	else
		return ONVIF_ERR_NoToken;
}

/**
 * @brief
 *  The GotoPreset operation recalls a previously set preset. If the speed 
 *  parameter is omitted, the default speed of the corresponding PTZ 
 *  configuration shall be used. The speed parameter can only be specified
 *  when speed spaces are available for the PTZ node. The GotoPreset command
 *  is a non-blocking operation and can be interrupted by other move commands.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_GotoPreset(ptz_GotoPreset_REQ * p_req)
{
	ONVIF_PROFILE * p_profile;
	PTZPresetList * p_preset;

	p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
	if (NULL == p_preset)
	{
		return ONVIF_ERR_NoToken;
	}

	// todo : here add handler code ...
	show_preset_list();

	int ret = goto_preset(p_req->ProfileToken, p_req->PresetToken);

	if(ret >= 0)
		return ONVIF_OK;
	else
		return ONVIF_ERR_NoToken;
}

/**
 * @brief
 *  This operation moves the PTZ unit to its home position. If the speed 
 *  parameter is omitted, the default speed of the corresponding PTZ 
 *  configuration shall be used. The speed parameter can only be specified 
 *  when speed spaces are available for the PTZ node.The command is 
 *  non-blocking and can be interrupted by other move commands.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoHomePosition
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_GotoHomePosition(ptz_GotoHomePosition_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	// todo : here add handler code ...
	
	int ret = goto_home_preset(p_req->ProfileToken);

	show_preset_list();
	
	if(ret >= 0)
		return ONVIF_OK;
	else
		return ONVIF_ERR_NoConfig;
}

/**
 * @brief
 *  The SetHome operation saves the current position parameters as the home
 *  position, so that the GotoHome operation can request that the device
 *  move to the home position.
 *
 *  The SetHomePosition command shall return with a failure if the "home" 
 *  position is fixed and cannot be overwritten. If the SetHomePosition is
 *  successful, it shall be possible to recall the home position with the
 *  GotoHomePosition command.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_CannotOverwriteHome
 **/
ONVIF_RET onvif_ptz_SetHomePosition(const char * token)
{
	PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	if (p_node->PTZNode.FixedHomePosition)
	{
		return ONVIF_ERR_CannotOverwriteHome;
	}

	// todo : here add handler code ...
	int ret = set_home_preset((char *)token);

	show_preset_list();

	if(ret >= 0)
		return ONVIF_OK;
	else
		return ONVIF_ERR_NoConfig;
}

/**
 * @brief
 *  A PTZ-capable device shall implement the SetConfiguration operation. 
 *  The ForcePersistence flag indicates if the changes remain after reboot
 *  of the device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_ConfigModify
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_ptz_SetConfiguration(ptz_SetConfiguration_REQ * p_req)
{
	PTZConfigurationList * p_ptz_cfg;
	PTZNodeList * p_ptz_node;

	p_ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_req->PTZConfiguration.token);
	if (NULL == p_ptz_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_ptz_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_req->PTZConfiguration.NodeToken);
	if (NULL == p_ptz_node)
	{
		return ONVIF_ERR_ConfigModify;
	}

	if (p_req->PTZConfiguration.DefaultPTZTimeoutFlag)
	{
		if (p_req->PTZConfiguration.DefaultPTZTimeout < p_ptz_cfg->Options.PTZTimeout.Min ||
			p_req->PTZConfiguration.DefaultPTZTimeout > p_ptz_cfg->Options.PTZTimeout.Max)
		{
			return ONVIF_ERR_ConfigModify;
		}
	}

	// todo : here add handler code ...

	if (p_req->PTZConfiguration.MoveRampFlag)
	{
		p_ptz_cfg->Configuration.MoveRamp = p_req->PTZConfiguration.MoveRamp;
	}

	if (p_req->PTZConfiguration.PresetRampFlag)
	{
		p_ptz_cfg->Configuration.PresetRamp = p_req->PTZConfiguration.PresetRamp;
	}

	if (p_req->PTZConfiguration.PresetTourRampFlag)
	{
		p_ptz_cfg->Configuration.PresetTourRamp = p_req->PTZConfiguration.PresetTourRamp;
	}

	strcpy(p_ptz_cfg->Configuration.Name, p_req->PTZConfiguration.Name);
	
	if (p_req->PTZConfiguration.DefaultPTZSpeedFlag)
	{
		if (p_req->PTZConfiguration.DefaultPTZSpeed.PanTiltFlag)
		{
			p_ptz_cfg->Configuration.DefaultPTZSpeed.PanTilt.x = p_req->PTZConfiguration.DefaultPTZSpeed.PanTilt.x;
			p_ptz_cfg->Configuration.DefaultPTZSpeed.PanTilt.y = p_req->PTZConfiguration.DefaultPTZSpeed.PanTilt.y;
		}

		if (p_req->PTZConfiguration.DefaultPTZSpeed.ZoomFlag)
		{
			p_ptz_cfg->Configuration.DefaultPTZSpeed.Zoom.x = p_req->PTZConfiguration.DefaultPTZSpeed.Zoom.x;
		}
	}

	if (p_req->PTZConfiguration.DefaultPTZTimeoutFlag)
	{
		p_ptz_cfg->Configuration.DefaultPTZTimeout = p_req->PTZConfiguration.DefaultPTZTimeout;
	}

	if (p_req->PTZConfiguration.PanTiltLimitsFlag)
	{
		memcpy(&p_ptz_cfg->Configuration.PanTiltLimits, &p_req->PTZConfiguration.PanTiltLimits, sizeof(onvif_PanTiltLimits));
	}

	if (p_req->PTZConfiguration.ZoomLimitsFlag)
	{
		memcpy(&p_ptz_cfg->Configuration.ZoomLimits, &p_req->PTZConfiguration.ZoomLimits, sizeof(onvif_ZoomLimits));
	}

	if (p_req->PTZConfiguration.ExtensionFlag)
	{
		if (p_req->PTZConfiguration.Extension.PTControlDirectionFlag)
		{
			if (p_req->PTZConfiguration.Extension.PTControlDirection.EFlipFlag)
			{
				p_ptz_cfg->Configuration.Extension.PTControlDirection.EFlip = p_req->PTZConfiguration.Extension.PTControlDirection.EFlip;
			}

			if (p_req->PTZConfiguration.Extension.PTControlDirection.ReverseFlag)
			{
				p_ptz_cfg->Configuration.Extension.PTControlDirection.Reverse = p_req->PTZConfiguration.Extension.PTControlDirection.Reverse;
			}
		}
	}

#ifdef MEDIA2_SUPPORT
	onvif_MediaConfigurationChangedNotify(p_req->PTZConfiguration.token, "PTZ");
#endif

	return ONVIF_OK;
}

/**
 * @brief
 *  A device supporting preset tours shall provide options for how preset 
 *  tours can be configured through GetPresetTourOptions.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_GetPresetTourOptions(ptz_GetPresetTourOptions_REQ * p_req, ptz_GetPresetTourOptions_RES * p_res)
{
	int cnt = 0;
	PTZPresetList * p_preset;
	PresetTourList * p_tour;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	if (p_req->PresetTourTokenFlag)
	{
		p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_req->PresetTourToken);
		if (NULL == p_tour)
		{
			return ONVIF_ERR_NoToken;
		}
	}

	// todo : here add handler code ...
	
	// todo : here add handler code ...

	p_res->Options.AutoStart = FALSE;
	p_res->Options.StartingCondition.RecurringTimeFlag = 1;
	p_res->Options.StartingCondition.RecurringTime.Min = 10;
	p_res->Options.StartingCondition.RecurringTime.Max = 100;

	p_res->Options.StartingCondition.RecurringDurationFlag = 1;
	p_res->Options.StartingCondition.RecurringDuration.Min = 10;
	p_res->Options.StartingCondition.RecurringDuration.Max = 100;

	p_res->Options.StartingCondition.PTZPresetTourDirection_Backward = 1;
	p_res->Options.StartingCondition.PTZPresetTourDirection_Forward = 1;

	p_res->Options.TourSpot.PresetDetail.HomeFlag = 1;
	p_res->Options.TourSpot.PresetDetail.Home = TRUE;

	p_preset = p_profile->presets;
	while (p_preset)
	{
		strcpy(p_res->Options.TourSpot.PresetDetail.PresetToken[cnt], p_preset->PTZPreset.token);

		cnt++;
		if (cnt >= ARRAY_SIZE(p_res->Options.TourSpot.PresetDetail.PresetToken))
		{
			break;
		}

		p_preset = p_preset->next;
	}

	p_res->Options.TourSpot.PresetDetail.sizePresetToken = cnt;

	p_res->Options.TourSpot.StayTime.Min = 0;
	p_res->Options.TourSpot.StayTime.Max = 100;

	return ONVIF_OK;
}

/**
 * @brief
 *  A device supporting Preset Tour feature shall allow creating a new 
 *  Preset Tour through the CreatePresetTour.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_TooManyPresetTours
 **/
ONVIF_RET onvif_ptz_CreatePresetTour(ptz_CreatePresetTour_REQ * p_req, ptz_CreatePresetTour_RES * p_res)
{
	//add my code
	char *pTourToken = create_ptz_tour();
	log_print(HT_LOG_INFO, "pTourToken:%s\n", pTourToken ? pTourToken : "(null)");
	if (pTourToken)
	{
		strcpy(p_res->PresetTourToken, pTourToken);
		return ONVIF_OK;
	}
	else
	{
		return ONVIF_ERR_TooManyPresetTours;
	}
}

/**
 * @brief
 *  A device supporting preset tours shall allow modifying a preset tour 
 *  through ModifyPresetTour.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_InvalidPresetTour
 *  ONVIF_ERR_TooManyPresets
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_SpaceNotSupported
 **/
ONVIF_RET onvif_ptz_ModifyPresetTour(ptz_ModifyPresetTour_REQ * p_req)
{
	ptz_tour_t * ptour = find_ptz_tour(p_req->ProfileToken);
	if (!ptour)
	{
		log_print(HT_LOG_ERR, "can not find tour(token:%s)\n", p_req->ProfileToken);
		return ONVIF_ERR_NoToken;
	}
	log_print(HT_LOG_INFO, "tour(idx :%d, token:%s, free:%d, active:%d, spotcnt:%d)\n", 
			ptour->index, ptour->token, ptour->free, ptour->active, ptour->spot_cnt);
	
	clear_tour_spots(ptour);

	int speed = 5;
	int stayTime;
	char presetToken[32];

	 PTZPresetTourSpotList *head  = p_req->PresetTour.TourSpot;
	if (head  == NULL)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
	PTZPresetTourSpotList *current = head;
	while (current != NULL) 
	{
		onvif_PTZPresetTourSpot *spot = &(current->PTZPresetTourSpot);

		stayTime = (spot->StayTime);
		
		strcpy(presetToken, spot->PresetDetail.PresetToken);
		
		add_ptz_spot(ptour->index, speed, stayTime, presetToken);
		
		current = current->next;
	}
	
	save_ptz_tours();
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device supporting preset tours shall allow starting, stopping, or 
 *  pausing a preset tour through OperatePresetTour.
 *
 *  Preset tour can be operated with the PresetTourOperation parameter
 *  of OperatePresetTour command.
 *  Start: indicates starting the preset tour or re-starting the paused preset tour.
 *  Stop: indicates stopping the preset tour.
 *  Pause:iIndicates pausing the preset tour.
 *
 *  When receiving another OperatePresetTour command of Start operation 
 *  for a preset tour which has already been started, the preset tour 
 *  shall be restarted with the newly requested parameter.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_InvalidPresetTour
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_ActivationFailed
 **/
ONVIF_RET onvif_ptz_OperatePresetTour(ptz_OperatePresetTour_REQ * p_req)
{
	if (access("/opt/ch/onvif_shield_cruise", F_OK) == F_OK)
	{
		return ONVIF_OK;
	}
	log_print(HT_LOG_INFO, "### tour token:%s, oper :%d ###\n", p_req->PresetTourToken, p_req->Operation);


	if (p_req->Operation == PTZPresetTourOperation_Start)
	{
		//memset(g_tptz_tourtoken, 0, sizeof(g_tptz_tourtoken));
		//strcpy(g_tptz_tourtoken, p_req->PresetTourToken);
		//g_tptz_start = 1;
		start_ptz_tour(p_req->PresetTourToken);
	}
	else if (p_req->Operation == PTZPresetTourOperation_Stop)
	{
		stop_ptz_tour(p_req->PresetTourToken);
	}

	save_ptz_tours();

	return ONVIF_OK;
}

/**
 * @brief
 *  A device supporting preset tours shall support removing preset tours
 *  through RemoevPresetTour.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_RemovePresetTour(ptz_RemovePresetTour_REQ * p_req)
{
	log_print(HT_LOG_INFO, "remove tour (token:%s)\n", p_req->PresetTourToken);

	remove_ptz_tour(p_req->PresetTourToken);

	save_ptz_tours();

	return ONVIF_OK;
}

/**
 * @brief
 *  This operation is used to call an auxiliary operation on the device. 
 *  The supported commands can be retrieved via the PTZ node properties.
 *  The auxiliary command should match the supported command listed in the
 *  PTZ node; no other syntax is supported. If the PTZ node lists the 
 *  tt:IRLamp command, then the parameter of AuxiliaryCommand command shall
 *  conform to the syntax specified in Section 8.6 Auxiliary operation of 
 *  ONVIF Core Specification. The SendAuxiliaryCommand shall be implemented
 *  when the PTZ node supports auxiliary commands.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 **/
ONVIF_RET onvif_ptz_SendAuxiliaryCommand(ptz_SendAuxiliaryCommand_REQ * p_req, ptz_SendAuxiliaryCommand_RES * p_res)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	// todo : here add handler code ...
	int ptz_aux_cmd = 0;//J  20140711
	if(p_req->ProfileToken)
		log_print(HT_LOG_INFO, "profileToken=%s\n", p_req->ProfileToken);
	
	if(p_req->AuxiliaryData)
	{
		log_print(HT_LOG_INFO, "profileToken=%s\n",p_req->AuxiliaryData);

		if(strstr(p_req->AuxiliaryData, "Wiper|On"))//J  20140710
		{
			ptz_aux_cmd = WIPER_PWRON;
			char ptzcmd[512] = {0};
			if (1)
			{
				sprintf(ptzcmd,
					"<xml>\n"
					"<cmd>Wiper</cmd>\n"
					"</xml>\n");
				AuxMsgPTZCmd(ptzcmd);
			}
		}
		if(strstr(p_req->AuxiliaryData, "Wiper|Off"))
		{
			ptz_aux_cmd = WIPER_PWROFF;
		}
		if(strstr(p_req->AuxiliaryData, "Lamp|On"))
		{
			ptz_aux_cmd = LENS_LIGHT_ON;
		}
		if(strstr(p_req->AuxiliaryData, "Lamp|OFF"))
		{
			ptz_aux_cmd = LENS_LIGHT_OFF;
		}
	}

	if(!ptz_aux_cmd)
	{
	
		return ONVIF_ERR_NotSupported;
	}
	else
	{
		PtzCmdHandle(ptz_aux_cmd, 0, 0, 0);
	}
	// todo : here add handler code ...
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling GeoMove in one of its PTZ nodes shall support this command.
 *
 *  The optional AreaHeight and AreaWidth parameters can be added to the 
 *  request, so that the PTZ-capable device can internally determine the 
 *  zoom factor. In case both AreaHeight and AreaWidth are not provided, 
 *  the unit will not change the zoom. AreaHeight and AreaWidth are 
 *  expressed in meters.
 *
 *  An existing speed argument overrides the DefaultSpeed of the corresponding
 *  PTZ configuration during movement by the requested translation. If spaces
 *  are referenced within the speed argument, they shall be speed spaces 
 *  supported by the PTZ node.
 *
 *  If the PTZ-capable device does not support automatic retrieval of the 
 *  geolocation, it shall be configured by using SetGeoLocation before it 
 *  can perform geo-referenced commands. If the client requests a GeoMove 
 *  command before the geolocation of the device is configured, the device 
 *  shall return an error.
 *
 *  Depending on the kinematics of the PTZ-capable device, the requested 
 *  position may not be reachable. In this situation the device shall return
 *  an error, signalling that it cannot perform the requested action due 
 *  to physical limitations.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_GeoMoveNotSupported
 *  ONVIF_ERR_UnreachablePosition
 *  ONVIF_ERR_TimeoutNotSupported
 *  ONVIF_ERR_GeoLocationUnknown
 **/
ONVIF_RET onvif_ptz_GeoMove(ptz_GeoMove_REQ * p_req)
{
	PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node || !p_node->PTZNode.GeoMove)
	{
		return ONVIF_ERR_GeoMoveNotSupported;
	}

	// todo : here add handler code ... 


	return ONVIF_OK;
}

#endif // PTZ_SUPPORT


