#ifndef _ANJ_CONFIG_GAT1400_H_
#define _ANJ_CONFIG_GAT1400_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define GAT1400_ID_MAX_LEN 64
#define GAT1400_NAME_MAX_LEN 64
#define GAT1400_PWD_MAX_LEN 64
#define GAT1400_SERVER_NAME_MAX_LEN 128

typedef struct
{
    int enable;                                    // 启动,0-否，1-是
    int https;                                     // 是否https服务器,0-否，1-是
    int stream_type;                               // 抓图选项; 0:主码流，1:子码流 ;默认子码流
    int keepalive_time;                            // 心跳周期;
    int keepalive_count;                           // 最大心跳超时次数(超过重新注册)
    int alarm_time;                                // 报警间隔时间
    char username[GAT1400_NAME_MAX_LEN];           // 用户名
    char password[GAT1400_PWD_MAX_LEN];            // 密码
    char device_id[GAT1400_ID_MAX_LEN];            // 设备ID; 20位: “44030520205039102815”
    char channel_id[GAT1400_ID_MAX_LEN];           // 通道ID
    char server_addr[GAT1400_SERVER_NAME_MAX_LEN]; // 服务器地址,ip/域名
    int server_port;                               // 服务器端口
} GAT1400Config;


/* 读取 / 解析 **************************************************************/
int anj_config_gat1400_get(IXML_Node *pNode, GAT1400Config *pGat1400Cfg);
int anj_config_gat1400_get_by_xml(GAT1400Config *pGat1400Cfg, char *xmlBuf);

char *anj_config_gat1400_conver_xml(GAT1400Config* pGat1400Cfg);

/* 保存 / 设置 **************************************************************/
int anj_config_gat1400_save(GAT1400Config *pGat1400Cfg);
int anj_config_gat1400_set(GAT1400Config *pGat1400Cfg);

/* 加载 ********************************************************************/
int anj_config_gat1400_load(GAT1400Config *pGat1400Cfg);

#ifdef __cplusplus
}
#endif

#endif
