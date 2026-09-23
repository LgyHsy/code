#ifndef _ANJ_CONFIG_GB28181_H_
#define _ANJ_CONFIG_GB28181_H_

#ifdef __cplusplus
extern "C"
{
#endif


#define GB28181_ID_MAX_LEN 32
#define GB28181_IP_MAX_LEN 32
#define GB28181_NAME_MAX_LEN 64
#define GB28181_PWD_MAX_LEN 32
typedef struct
{
    int enable;
    short tcp;
    short nstreams;  // 可用码流:0 主码流 1子码流 2 双码流
    int nChannelNum; // 通道数量

    char hcId[GB28181_ID_MAX_LEN];
    char hcIp[GB28181_IP_MAX_LEN];
    char hcName[GB28181_NAME_MAX_LEN];
    char hcPwd[GB28181_PWD_MAX_LEN];
    int hcPort;

    char lcId[GB28181_ID_MAX_LEN];
    char lcName[GB28181_NAME_MAX_LEN];
    char lcPwd[GB28181_PWD_MAX_LEN];
    int lcPort;

    char camId[GB28181_ID_MAX_LEN];
    char alarmId[GB28181_ID_MAX_LEN];
    char device_name[GB28181_NAME_MAX_LEN];

    int isOpen35114;
    int protocol; // tcp or udp
} GB28181Config;


/* 读取 / 解析 **************************************************************/
int anj_config_gb28181_get(IXML_Node *pNode, GB28181Config* pGb28181Cfg);
int anj_config_gb28181_get_by_xml(GB28181Config *pGb28181Cfg, char *xmlBuf);

char *anj_config_gb28181_conver_xml(GB28181Config* pGb28181Cfg);

/* 保存 / 设置 **************************************************************/
int anj_config_gb28181_save(GB28181Config *pGb28181Cfg);
int anj_config_gb28181_set(GB28181Config *pGb28181Cfg);

/* 加载 ********************************************************************/
int anj_config_gb28181_load(GB28181Config *pGb28181Cfg);


#ifdef __cplusplus
}
#endif

#endif
