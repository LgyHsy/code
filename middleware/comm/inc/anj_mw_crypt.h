#ifndef __ANJ_MW_CRYPT_H__
#define __ANJ_MW_CRYPT_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

#define TYPE_ID_TYPE_MACADDR 0x11ff

#define ANJ_OEM_APPLY_STR_LEN   32
typedef struct
{
	unsigned int nType;
	char szSN[ANJ_OEM_APPLY_STR_LEN];
	char szUUID[ANJ_OEM_APPLY_STR_LEN];
}AjOemApply_t;


enum
{
    P2P_ID_TYPE_DANALE             = 0,
    P2P_ID_TYPE_GOOLINK            = 1,
    P2P_ID_TYPE_EYEPLUS            = 2,
    P2P_ID_TYPE_TUTK               = 3,
    P2P_ID_TYPE_QINIU              = 4,
    P2P_ID_TYPE_TUYA               = 5,
    P2P_ID_TYPE_AC18PLUS_CONSUME   = 6,
    P2P_ID_TYPE_TUYA_NVR           = 7,
    P2P_ID_TYPE_TUYA_NVR_MAINPID   = 8,
    P2P_ID_TYPE_TUYA_NVR_SUBPID    = 9,
    P2P_ID_TYPE_TUYA_NVR_APPURL    = 10,
    P2P_ID_TYPE_AC18PLUS_NVR       = 11,
    P2P_ID_TYPE_AC18PRO_CONSUME    = 12,
    P2P_ID_TYPE_AC18PRO_CMCC4G     = 13,
    P2P_ID_TYPE_AC18PRO_NVR        = 14,	
    P2P_ID_TYPE_TENCENT_IOT_IPC    = 15,	
    P2P_ID_TYPE_AIOT_IPC           = 16,	
    P2P_ID_TYPE_AIOT_NVR           = 17,
    P2P_ID_TYPE_DOT                = 18,	
    P2P_ID_TYPE_MAX,	
};


unsigned int anj_crc32_update(unsigned int start, const unsigned char *pBuffer, unsigned int len);
int anj_crc32_file(char *fileName, unsigned int *crc);

char* out_md5_encode_data(const unsigned char* data, unsigned int len, char* buf);
void our_md5_encode(char *md5Buf, const unsigned char *data, int len);

// softsn.a中的接口
const char *get_uuid();
int WriteP2pid(char *buf, int len, unsigned int nType);
int ReadP2pID(char *buffer, int buffersize, unsigned int nType);

int ReadPdMadpLicense(char *buffer, int buffersize);
int ReadPdLicense(char *buffer, int buffersize);

int ReadSnFlashData(unsigned char *buf, unsigned int buflen, int sectno);

int ReadEncriptDataFromSoft(unsigned char *output_decrypt, int buflen);
int ReadEncriptDataFromSoft_ex(unsigned char *output_decrypt, int buflen);
int WriteEncriptDataToSoft(unsigned char *buf, int len, int version);
int ClearEncriptDataToSoft(int sectno);

int soft_enc_xml_sn_data_parse(const char *xmlBuf, char *cameraid, int buflen1, char *data, int buflen2, char *checksum, int buflen3, int *version);
int SupportBootAutoUpdate();


typedef void (*OEMINFO_GET_OK_CALLBACK)(const char* oemstr);
typedef void (*P2PID_GET_OK_CALLBACK)();
typedef void (*SN_GET_OK_CALLBACK)();
typedef int (*PLATFORM_TYPE_GET_CALLBACK)(char *szPlatformType, int bufLen);

void set_softsn_platform_type_cb(PLATFORM_TYPE_GET_CALLBACK cb);
int softsn_platform_type_get(char *szPlatformType, int bufLen);

void set_softsn_v2_cb(SN_GET_OK_CALLBACK cb);
int stop_softsn_thread_v2(void);
int start_softsn_thread_v2(void);
int wait_softsn_thread_v2(void);

void set_p2pid_cb(P2PID_GET_OK_CALLBACK cb);
int stop_p2pid_thread(void);
int start_p2pid_thread(AjOemApply_t *pParam );
int wait_p2pid_thread(void);

void set_oemapply_cb(OEMINFO_GET_OK_CALLBACK cb);
int stop_oemapply_thread(void);
int start_oemapply_thread(AjOemApply_t *pParam );
int wait_oemapply_thread(void);

/*
    ssn -- clear:清除sn数据和/mnt/mtd/下的配置
*/
int soft_enc_clear();

/*
    ssn -- show:打印uuid、sn、pd license、p2p配置等
*/
int soft_enc_show();

/*
    ssn -- showsn:打印sn
*/
int soft_enc_show_sn(int sect_no);

/*
    ssn -- erasesn:擦除sn
*/
int soft_enc_erasesn(int sect_no);

/*
    ssn -- ubootargs2: 解析uboot环境变量
*/
int soft_enc_uboot_anlyargs(const char *name, const char *value);

/*
    ssn -- uboot: 写入uboot参数
*/
int soft_enc_uboot_write(char *file);

int Base64EncodeLen(const char *pInput);
int Base64Encode(unsigned char *pInput, int srclen,	char * pOutput);
int Base64Decode(char *pInput, int srclen, unsigned char * pOutput);

unsigned int GetCrcValue(char *c, int len);
unsigned int GetCrcValueMode2(unsigned int crc, char *c, int len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
