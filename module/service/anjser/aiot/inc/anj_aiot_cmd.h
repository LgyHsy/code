#ifndef __ANJ_AIOT_CMD_H__
#define __ANJ_AIOT_CMD_H__

#define AIOT_TRANSDATA_MAX_LENGTH 204800 //200KB
#define FLAG_MAGIC_AIOT_TRANSDATA 0x61696f7470726f74
typedef enum
{
	COMPRESS_NONE = 0,///无压缩
	COMPRESS_ZLIB = 1,//ZLIB压缩	
}AiotTransCompressMethod;

typedef enum
{
	TRANS_DATA_ACP_XML = 0,	//ACP XML私有协议信令
	TRANS_DATA_ACP_JSON = 1,//ACP JSON新增信令	
}AiotTransDataType;

typedef struct
{
	unsigned long long magic; //FLAG_MAGIC_AIOT_TRANSDATA
	AiotTransCompressMethod nCompressType;
	unsigned int nDecompressedLength;//数据压缩前长度
	unsigned int nCompressedLength;//数据压缩后长度
	AiotTransDataType nDataType;
	char reserve[12];
}AiotTransHeader;

int anj_aiot_cmd_data_init();
void anj_aiot_cmd_location_on_bind(const char *accountName);
void anj_aiot_cmd_location_on_unbind(void);

int anj_aiot_cmd_init();
void anj_aiot_cmd_uninit();
void anj_aiot_cmd_push(char *pBuf, unsigned int nBufLen, unsigned int sid);

void anj_aiot_cmd_sd_format_reponse();
void anj_aiot_cmd_ptz_preset_reponse();
void anj_aiot_cmd_ptz_advance_state_reponse(void *data);

char *anj_aiot_cmd_ptz_dir_get();
char *anj_aiot_cmd_audio_capture_get();
char *anj_aiot_cmd_ircut_get();
char *anj_aiot_cmd_osd_get();

/* 属性写云存：cmd 只提供当前/增量 JSON，上报由 report 侧调用 SDK */
char *anj_aiot_cmd_property_change_json(void);
char *anj_aiot_cmd_property_full_json(void);
void anj_aiot_cmd_property_clear_change(void);
int anj_aiot_cmd_property_has_data(void); /* 已有任意属性快照，无增量时才允许心跳 */

#endif
