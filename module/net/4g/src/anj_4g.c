#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <linux/sockios.h>

#include <netinet/in.h>

#include "anj_mw_comm.h"
#include <netinet/tcp.h>
#include <poll.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <termios.h>
#include <dirent.h>
#include <ctype.h>

#include "anj_config.h"
#include "eventhub.h"
#include "anj_base64.h"
#include "anj_comm.h"
#include "anj_mw_comm.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_net.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_audio.h"
#include "anj_osd.h"
#include "anj_4g.h"
#include "anj_net.h"
#include "anj_net_provider.h"
#include "anj_ser.h"
#include "function_list.h"
#include "cJSON.h"

#define TTY_DEVICE_0_4G "/dev/ttyUSB0"
#define TTY_DEVICE_1_4G "/dev/ttyUSB1"
#define TTY_DEVICE_2_4G "/dev/ttyUSB2"
#define TTY_DEVICE_3_4G "/dev/ttyUSB3"

#define QUECTIL_600N_PID "6002"
#define NOTION_VID "1286:4e3d"
#define UM_VID "1286:4e3c"
#define QUECTIL_VID "2c7c"
#define QUECTIL_6026_VID "2c7c:6026"
#define QUECTIL_6002_VID "2c7c:6002"
#define QUECTIL_0903_VID "2c7c:0903"
#define CIS_VID "2ecc:3010"
#define SIMCOM_VID "1e0e:9011"
#define HZ_VID "19d1:0001"

#define THREAD_DELAY_US ((50) * (1000))     /* 线程延时间隔/微秒 */
#define HEART_TIME_MS ((1) * (60) * (1000)) /* 心跳间隔/毫秒 */
#define HEART_BEAT_CNT (HEART_TIME_MS / (THREAD_DELAY_US / 1000))
#define UPDATE_INFO_MS ((60) * (1000))   /* 更新4G相关信息时间/毫秒 */
#define LOCATION_FILE DATA_BLOCK_MOUNT_PATH "/location.json"
#define MOBILE4G_ONE_AUDIO_INTERVAL (50) /* 同一音频播放间隔/秒 */
#define MOBILE4G_ALL_AUDIO_INTERVAL (5)  /* 所有音频播放间隔/秒 */

#define AT_MODULE_UART_FAILED_CNT (60)     /* AT模块串口失败重试次数 */
#define AT_MODULE_FAILED_CNT (5)           /* AT模块失败重试次数 */
#define AT_MODULE_SIM_FAILED_CNT (60)      /* AT模块SIM卡异常失败重试次数 */
#define AT_MODULE_SIM_INFO_FAILED_CNT (60) /* SIM卡信息获取失败次数 */

#define NCM_DIAL_FAILED_CNT (5)   /* NCM拨号失败重试次数 */
#define USBNET_FAILED_CNT (5)     /* 拨号后网络一直不通次数，重启at模块 */
#define HEART_BEAT_FAILED_CNT (3) /* 心跳失败重启次数 */

#define AT_INFO_SIZE (128) /* AT命令发送接收 每组大小 */

#define MOBILE4G_NO_4G_MAX_CNT (5)
#define MOBILE4G_NO_4G_INTERVAL_CNT (5)
#define MOBILE4G_NO_4G_START_CNT (25)
#define MOBILE4G_CONNECT_NET_FAIL_MAX_CNT (6)
#define MOBILE4G_CONNECT_NET_FAIL_INTERVAL_CNT ((5 * 1000) / (THREAD_DELAY_US / 1000))
#define MOBILE4G_SCAN_QRCODE_MAX_CNT (2)
#define MOBILE4G_SCAN_QRCODE_INTERVAL_CNT ((5 * 1000) / (THREAD_DELAY_US / 1000))
#define MOBILE4G_CONNECTING_NET_MAX_CNT (2)
#define MOBILE4G_CONNECTING_NET_INTERVAL_CNT ((25 * 1000) / (THREAD_DELAY_US / 1000))

static const anj_net_4g_ops s_st4gOps = {
    .init = anj_4g_init,
    .uninit = anj_4g_uninit,
    .set_pause = anj_4g_set_pause,
    .clear_pause = anj_4g_clear_pause,
};

ANJ_LINK_KEEP(anj_keep_net_4g_provider);

__attribute__((constructor)) static void anj_4g_provider_register(void)
{
    anj_net_4g_provider_register(&s_st4gOps);
}

__attribute__((destructor)) static void anj_4g_provider_unregister(void)
{
    anj_net_4g_provider_unregister(&s_st4gOps);
}

typedef enum
{
    AT_SEQ_AT_QUERY = 0,   /* 判断 AT 通讯是否正常 */
    AT_SEQ_ATE0_SET,       /* 关闭回显 */
    AT_SEQ_CFUN_QUERY,     /* 查询模块工作状态：1代表全功能 */
    AT_SEQ_CFUN_SET_ON,    /* 设置模块工作状态1 */
    AT_SEQ_CFUN_SET_OFF,   /* 设置模块工作状态0 */
    AT_SEQ_CPIN_QUERY,     /* 查询 SIM 卡状态 */
    AT_SEQ_HCSQ_QUERY,     /* 查询信号 */
    AT_SEQ_COPS_QUERY,     /* 查询注册上的运营商信息 */
    AT_SEQ_CREG_QUERY,     /* 查询 SIM卡注册状态 */
    AT_SEQ_CREG_SET,       /* 设置注册上报模式，带 lac/ci */
    AT_SEQ_QDSICCID_QUERY, /* 查询卡的 ICCID */
    AT_SEQ_ICCID_QUERY,    /* 查询卡的 ICCID */

    AT_SEQ_NDISDUP_SET_ON,  /* 拨号操作 */
    AT_SEQ_NDISDUP_SET_OFF, /* 断开拨号 */
    AT_SEQ_NDISDUP_SET_NET, /* 拨号操作网卡 SIMCOM使用*/
    AT_SEQ_NDISDUP_QUERY,   /* 查询拨号状态 */

    AT_SEQ_RESET_SET,          /* 模块重启 */
    AT_SEQ_CGDCONT_CHN_CT,     /*电信*/
    AT_SEQ_CGDCONT_CHN_UNICOM, /*联通*/
    AT_SEQ_CGDCONT_CHN_MOBILE, /*移动*/
    AT_SEQ_GET_QDSIMSI,        /* 获取IMSI*/
    AT_SEQ_GET_IMSI,           /* 获取IMSI*/
    AT_SEQ_CGACT_SET_ON,       /* 激活 PDP 上下文*/
    AT_SEQ_CGACT_QUERY,        /* 获取PDP 上下文*/
    AT_SEQ_QUERY_MODE,         /* 查询模式*/
    AT_SEQ_CHANGE_MODE,        /* 切换模式*/
    AT_SEQ_QUERY_USB_NUM,      /* 查询USB NUM*/
    AT_SEQ_QUERY_MODULE_INFO,  /* 查询模块信息*/
    AT_SEQ_QUERY_MODULE_INFO1, /* 查询模块信息 UM/HZ 使用*/
    AT_SEQ_QUERY_MODULE_INFO2, /* 查询模块信息 HZ使用*/
    AT_SEQ_START_HOTPLUG,      /* 热插拔设置 YUGE/QUECTEL使用*/
    AT_SEQ_STOP_HOTPLUG,       /* 热插拔设置 QUECTEL使用*/
    AT_SEQ_QUERY_DSPIN,        /* 查询 QUECTEL 双卡使用*/
    AT_SEQ_QUERY_DSTYPE,       /* 查询双卡模式*/
    AT_SEQ_SET_DSTYPE,         /* 设置双卡模式*/
    AT_SEQ_QUERY_CUR_SIM,      /* 查询当前使用的SIM卡index*/
    AT_SEQ_SEL_SIM0,           /* 切换SIM卡*/
    AT_SEQ_SEL_SIM1,           /* 切换SIM卡*/
    AT_SEQ_QUERY_MSISDN,       /* 查询MSISDN YUGE使用*/
    AT_SEQ_QUERY_SYSINFO,      /* 查询系统信息 YUGE使用*/
    AT_SEQ_GET_CGPADDR,        /* 获取gb28181 ipaddr*/
    AT_CMD_MAX_NUMS,
} AT_SEQ;

typedef enum
{
    SIGNAL_LEVEL0 = 0, /* 信号较差 */
    SIGNAL_LEVEL1,     /* 信号一般 */
    SIGNAL_LEVEL2,     /* 信号较好 */
    SIGNAL_LEVEL3,     /* 信号最优 */
} SIGNAL_LEVEL;

typedef struct
{
    char cmd[256];                  /* 命令内容 */
    int (*recv_fun)(char *recvBuf); /* 命令接收处理 */
} AT_CMD;

typedef enum
{
    ANJ_4G_MANUFACTURER_NONE,
    ANJ_4G_MANUFACTURER_NOTION,  // 诺行
    ANJ_4G_MANUFACTURER_YUGE,    // 域格
    ANJ_4G_MANUFACTURER_QUECTEL, // 移远
    ANJ_4G_MANUFACTURER_CIS,     // CIS
    ANJ_4G_MANUFACTURER_HZ,      // 合宙
    ANJ_4G_MANUFACTURER_UM,      // 优米亚
    ANJ_4G_MANUFACTURER_SIMCOM   // 芯讯通
} ANJ_4G_MANUFACTURER;

static int anj_4g_check_connect(char *recvBuf);
static int anj_4g_close_echo(char *recvBuf);
static int anj_4g_query_module_status(char *recvBuf);
static int anj_4g_set_module_status(char *recvBuf);
static int anj_4g_clear_module_status(char *recvBuf);
static int anj_4g_query_sim_status(char *recvBuf);
static int anj_4g_query_signal(char *recvBuf);
static int anj_4g_query_operator(char *recvBuf);
static int anj_4g_query_creg(char *recvBuf);
static void anj_4g_location_load(void);
static void anj_4g_location_set(EventResult *event_result, void *data);
static int anj_4g_query_iccid_list(char *recvBuf);
static int anj_4g_query_iccid(char *recvBuf);
static int anj_4g_query_usb_num(char *recvBuf);
static int anj_4g_query_module_info(char *recvBuf);
static int anj_4g_query_module_info1(char *recvBuf);
static int anj_4g_query_module_info2(char *recvBuf);
static int anj_4g_ncm_dial_on(char *recvBuf);
static int anj_4g_ncm_dial_off(char *recvBuf);
static int anj_4g_ncm_dial_query(char *recvBuf);
static int anj_4g_set_cgdcont(char *recvBuf);
static int anj_4g_set_hotplug(char *recvBuf);
static int anj_4g_clear_hotplug(char *recvBuf);
static int anj_4g_query_dspin(char *recvBuf);
static int anj_4g_query_dstype(char *recvBuf);
static int anj_4g_query_imsi_list(char *recvBuf);
static int anj_4g_query_imsi(char *recvBuf);
static int anj_4g_query_sim_index(char *recvBuf);
// static int anj_4g_query_sysinfo(char *recvBuf);
// static int anj_4g_query_msidn(char *recvBuf);
static int anj_4g_get_cgpaddr(char *recvBuf);

static AT_CMD stQuectelAtCmd[AT_CMD_MAX_NUMS] =
    {
        [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
        [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
        [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
        [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
        [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
        [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
        [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
        [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
        [AT_SEQ_CREG_QUERY] = {"AT+CREG?", anj_4g_query_creg},
        [AT_SEQ_CREG_SET] = {"AT+CREG=2", anj_4g_check_connect},
        [AT_SEQ_QDSICCID_QUERY] = {"AT+QDSCCID?", anj_4g_query_iccid_list},
        [AT_SEQ_ICCID_QUERY] = {"AT+ICCID", anj_4g_query_iccid},

        [AT_SEQ_NDISDUP_SET_ON] = {"AT+QNETDEVCTL=3,1,1", anj_4g_ncm_dial_on},
        [AT_SEQ_NDISDUP_SET_OFF] = {"AT+QNETDEVCTL=0,1,1", anj_4g_ncm_dial_off},
        [AT_SEQ_NDISDUP_QUERY] = {"AT+QNETDEVCTL?", anj_4g_ncm_dial_query},

        [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
        [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
        [AT_SEQ_GET_QDSIMSI] = {"AT+QDSIMI?", anj_4g_query_imsi_list},
        [AT_SEQ_GET_IMSI] = {"AT+QDSIMI?", anj_4g_query_imsi},
        [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
        [AT_SEQ_START_HOTPLUG] = {"AT+QSIMDET=1,1", anj_4g_set_hotplug},
        [AT_SEQ_STOP_HOTPLUG] = {"AT+QSIMDET=0,0", anj_4g_clear_hotplug},
        [AT_SEQ_QUERY_DSPIN] = {"AT+QDSPIN?", anj_4g_query_dspin},
        [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
        [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
        [AT_SEQ_QUERY_CUR_SIM] = {"AT+QDSIM?", anj_4g_query_sim_index},
        [AT_SEQ_SEL_SIM0] = {"AT+QDSIM=0", NULL},
        [AT_SEQ_SEL_SIM1] = {"AT+QDSIM=1", NULL},
        [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
};

static AT_CMD stUmAtCmd[AT_CMD_MAX_NUMS] =
    {
        [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
        [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
        [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
        [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
        [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
        [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
        [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
        [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
        [AT_SEQ_CREG_QUERY] = {"AT+CEREG?", anj_4g_query_creg},
        [AT_SEQ_CREG_SET] = {"AT+CEREG=2", anj_4g_check_connect},
        [AT_SEQ_ICCID_QUERY] = {"AT*ICCID?", anj_4g_query_iccid_list},

        [AT_SEQ_NDISDUP_SET_ON] = {"AT*DIALMODE=0", anj_4g_ncm_dial_on},
        [AT_SEQ_NDISDUP_SET_OFF] = {"AT*DIALMODE=1", anj_4g_ncm_dial_off},
        [AT_SEQ_NDISDUP_QUERY] = {"AT*DIALMODE?", anj_4g_ncm_dial_query},

        [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
        [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
        [AT_SEQ_GET_IMSI] = {"AT+CIMI", anj_4g_query_imsi_list},
        [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
        [AT_SEQ_QUERY_MODULE_INFO1] = {"AT+CGSN", anj_4g_query_module_info1},
        // [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
        [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
        [AT_SEQ_QUERY_CUR_SIM] = {"AT+SINGLESIM?", anj_4g_query_sim_index},
        [AT_SEQ_SEL_SIM0] = {"AT+SINGLESIM=0", NULL},
        [AT_SEQ_SEL_SIM1] = {"AT+SINGLESIM=1", NULL},
        [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
};

// static AT_CMD stYugeAtCmd[AT_CMD_MAX_NUMS] =
//     {
//         [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
//         [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
//         [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
//         [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
//         [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
//         [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
//         [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
//         [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
//         [AT_SEQ_CREG_QUERY] = {"AT+CREG?", anj_4g_query_creg},
//         [AT_SEQ_QDSICCID_QUERY] = {"AT+QDSCCID?", anj_4g_query_iccid_list},
//         [AT_SEQ_ICCID_QUERY] = {"AT+ICCID?", anj_4g_query_iccid},

//         [AT_SEQ_NDISDUP_SET_ON] = {"AT+RNDISCALL=1", anj_4g_ncm_dial_on},
//         [AT_SEQ_NDISDUP_SET_OFF] = {"AT+RNDISCALL=0", anj_4g_ncm_dial_off},
//         [AT_SEQ_NDISDUP_QUERY] = {"AT+RNDISCALL?", anj_4g_ncm_dial_query},

//         [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
//         [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
//         [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
//         [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
//         [AT_SEQ_GET_QDSIMSI] = {"AT+QDSIMI?", anj_4g_query_imsi_list},
//         [AT_SEQ_GET_IMSI] = {"AT+CIMI", anj_4g_query_imsi},
//         [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
//         [AT_SEQ_START_HOTPLUG] = {"AT+HOSCFG=1,1", anj_4g_set_hotplug},
//         [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
//         [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
//         [AT_SEQ_QUERY_CUR_SIM] = {"AT+QDSIM?", anj_4g_query_sim_index},
//         [AT_SEQ_SEL_SIM0] = {"AT+QDSIM=0", NULL},
//         [AT_SEQ_SEL_SIM1] = {"AT+QDSIM=1", NULL},
//         [AT_SEQ_QUERY_MSISDN] = {"AT+CNUM", anj_4g_query_msidn},
//         [AT_SEQ_QUERY_SYSINFO] = {"AT^SYSINFO", anj_4g_query_sysinfo},
//         [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
// };

static AT_CMD stSimcomAtCmd[AT_CMD_MAX_NUMS] =
    {
        [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
        [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
        [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
        [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
        [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
        [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
        [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
        [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
        [AT_SEQ_CREG_QUERY] = {"AT+CGREG?", anj_4g_query_creg},
        [AT_SEQ_CREG_SET] = {"AT+CGREG=2", anj_4g_check_connect},
        [AT_SEQ_QDSICCID_QUERY] = {"AT+QDSCCID?", anj_4g_query_iccid_list},
        [AT_SEQ_ICCID_QUERY] = {"AT+CICCID?", anj_4g_query_iccid},

        [AT_SEQ_NDISDUP_SET_ON] = {"AT+DIALMODE=0", anj_4g_ncm_dial_on},
        [AT_SEQ_NDISDUP_SET_OFF] = {"AT+DIALMODE=1", anj_4g_ncm_dial_off},
        [AT_SEQ_NDISDUP_QUERY] = {"AT+DIALMODE?", anj_4g_ncm_dial_query},

        [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
        [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
        [AT_SEQ_GET_QDSIMSI] = {"AT+QDSIMI?", anj_4g_query_imsi_list},
        [AT_SEQ_GET_IMSI] = {"AT+CIMI", anj_4g_query_imsi},
        [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
        [AT_SEQ_START_HOTPLUG] = {"AT+QSIMDET=1,1", anj_4g_set_hotplug},
        [AT_SEQ_STOP_HOTPLUG] = {"AT+QSIMDET=0,0", anj_4g_clear_hotplug},
        [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
        [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
        [AT_SEQ_QUERY_CUR_SIM] = {"AT+QDSIM?", anj_4g_query_sim_index},
        [AT_SEQ_SEL_SIM0] = {"AT+QDSIM=0", NULL},
        [AT_SEQ_SEL_SIM1] = {"AT+QDSIM=1", NULL},
        [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
};

static AT_CMD stCisAtCmd[AT_CMD_MAX_NUMS] =
    {
        [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
        [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
        [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
        [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
        [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
        [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
        [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
        [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
        [AT_SEQ_CREG_QUERY] = {"AT+CREG?", anj_4g_query_creg},
        [AT_SEQ_CREG_SET] = {"AT+CREG=2", anj_4g_check_connect},
        [AT_SEQ_QDSICCID_QUERY] = {"AT+QDSCCID?", anj_4g_query_iccid_list},
        [AT_SEQ_ICCID_QUERY] = {"AT+QCCID", anj_4g_query_iccid},

        [AT_SEQ_NDISDUP_SET_ON] = {"AT+QIACT=1", anj_4g_ncm_dial_on},
        [AT_SEQ_NDISDUP_SET_OFF] = {"AT+QIACT=0", anj_4g_ncm_dial_off},
        [AT_SEQ_NDISDUP_QUERY] = {"AT+QIACT?", anj_4g_ncm_dial_query},

        [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
        [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
        [AT_SEQ_GET_QDSIMSI] = {"AT+QDSIMI?", anj_4g_query_imsi_list},
        [AT_SEQ_GET_IMSI] = {"AT+CIMI", anj_4g_query_imsi},
        [AT_SEQ_QUERY_USB_NUM] = {"AT+FCISCFG=usbEnumOrder,1", anj_4g_query_usb_num},
        [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
        [AT_SEQ_START_HOTPLUG] = {"AT+QSIMDET=1,1", anj_4g_set_hotplug},
        [AT_SEQ_STOP_HOTPLUG] = {"AT+QSIMDET=0,0", anj_4g_clear_hotplug},
        [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
        [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
        [AT_SEQ_QUERY_CUR_SIM] = {"AT+QDSIM?", anj_4g_query_sim_index},
        [AT_SEQ_SEL_SIM0] = {"AT+QDSIM=0", NULL},
        [AT_SEQ_SEL_SIM1] = {"AT+QDSIM=1", NULL},
        [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
};

static AT_CMD stHzAtCmd[AT_CMD_MAX_NUMS] =
    {
        [AT_SEQ_AT_QUERY] = {"AT", anj_4g_check_connect},
        [AT_SEQ_ATE0_SET] = {"ATE0", anj_4g_close_echo},
        [AT_SEQ_CFUN_QUERY] = {"AT+CFUN?", anj_4g_query_module_status},
        [AT_SEQ_CFUN_SET_ON] = {"AT+CFUN=1", anj_4g_set_module_status},
        [AT_SEQ_CFUN_SET_OFF] = {"AT+CFUN=0", anj_4g_clear_module_status},
        [AT_SEQ_CPIN_QUERY] = {"AT+CPIN?", anj_4g_query_sim_status},
        [AT_SEQ_HCSQ_QUERY] = {"AT+CSQ", anj_4g_query_signal},
        [AT_SEQ_COPS_QUERY] = {"AT+COPS?", anj_4g_query_operator},
        [AT_SEQ_CREG_QUERY] = {"AT+CREG?", anj_4g_query_creg},
        [AT_SEQ_CREG_SET] = {"AT+CREG=2", anj_4g_check_connect},
        [AT_SEQ_QDSICCID_QUERY] = {"AT+QDSCCID?", anj_4g_query_iccid_list},
        [AT_SEQ_ICCID_QUERY] = {"AT+ICCID", anj_4g_query_iccid},

        [AT_SEQ_NDISDUP_SET_ON] = {"AT+QNETDEVCTL=3,1,1", anj_4g_ncm_dial_on},
        [AT_SEQ_NDISDUP_SET_OFF] = {"AT+QNETDEVCTL=0,1,1", anj_4g_ncm_dial_off},
        [AT_SEQ_NDISDUP_QUERY] = {"AT+QNETDEVCTL?", anj_4g_ncm_dial_query},

        [AT_SEQ_RESET_SET] = {"AT+CFUN=1,1", NULL},
        [AT_SEQ_CGDCONT_CHN_CT] = {"AT+CGDCONT=1,\"IP\",\"CTNET\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_UNICOM] = {"AT+CGDCONT=1,\"IP\",\"GZMZM\"", anj_4g_set_cgdcont},
        [AT_SEQ_CGDCONT_CHN_MOBILE] = {"AT+CGDCONT=1,\"IP\",\"CMIOT\"", anj_4g_set_cgdcont},
        [AT_SEQ_GET_QDSIMSI] = {"AT+QDSIMI?", anj_4g_query_imsi_list},
        [AT_SEQ_GET_IMSI] = {"AT+QDSIMI?", anj_4g_query_imsi},
        [AT_SEQ_QUERY_MODULE_INFO] = {"ATI", anj_4g_query_module_info},
        [AT_SEQ_QUERY_MODULE_INFO1] = {"AT+CGSN", anj_4g_query_module_info1},
        [AT_SEQ_QUERY_MODULE_INFO2] = {"AT+CGMM", anj_4g_query_module_info2},
        [AT_SEQ_QUERY_DSTYPE] = {"AT+QDSTYPE?", anj_4g_query_dstype},
        [AT_SEQ_SET_DSTYPE] = {"AT+QDSTYPE=0", NULL},
        [AT_SEQ_QUERY_CUR_SIM] = {"AT+SIMCROSS?", anj_4g_query_sim_index},
        [AT_SEQ_SEL_SIM0] = {"AT+SIMCROSS=0", NULL},
        [AT_SEQ_SEL_SIM1] = {"AT+SIMCROSS=1", NULL},
        [AT_SEQ_GET_CGPADDR] = {"AT+CGPADDR", anj_4g_get_cgpaddr},
};

typedef struct
{
    int is_connect_internet; /* 0 没连接外网，1 连接外网 */
    int update_flag[AT_CMD_MAX_NUMS];
} AT_INFO;

typedef enum
{
    MOBILE4G_STATUS_AT_CORRECT = 0, /* AT模块正常 */
    MOBILE4G_STATUS_AT_ERROR,       /* AT模块异常 */
    MOBILE4G_STATUS_UART_ERROR,     /* 串口异常 */
    MOBILE4G_STATUS_SIM_ERROR,      /* SIM卡异常（热插拔） */
    MOBILE4G_STATUS_INFO_ERROR,     /* SIM卡信息异常 */
    MOBILE4G_NO_4G,                 /* 没有4G模块 */

    MOBILE4G_STATUS_NET_ERROR,   /* 网络异常 */
    MOBILE4G_STATUS_NET_CORRECT, /* 网络正常 */

    MOBILE4G_STATUS_COMM_ERROR,  /* AT通信异常 */
    MOBILE4G_STATUS_CHANGE_MODE, /* 模式切换 */
    MOBILE4G_STATUS_MAX,
} MOBILE4G_STATUS;

typedef enum
{
    MOBILE4G_ERROR_NONE,
    MOBILE4G_ERROR_UART_1,
    MOBILE4G_ERROR_AT_1,
    MOBILE4G_ERROR_AT_2,
    MOBILE4G_ERROR_SIM,
    MOBILE4G_ERROR_INFO_1,
    MOBILE4G_ERROR_INFO_2,
    MOBILE4G_ERROR_INFO_3,
    MOBILE4G_ERROR_INFO_4,
    MOBILE4G_ERROR_INFO_5,
    MOBILE4G_ERROR_INFO_6,
    MOBILE4G_ERROR_NET,
    MOBILE4G_ERROR_AT_3,
    MOBILE4G_ERROR_UART_2,
} MOBILE4G_ERROR;

typedef struct
{
    int uart_fd;                                   /* at模块串口fd */
    int debug_flag;                                /* 调试标志 */
    int osd_flag;                                  /* OSD显示标志 */
    int anj_4g_at_init_done_flag;                  /* at通信初始化完成标志 */
    int anj_4g_at_usbnet_done_flag;                /* usb网络始化完成标志 */
    pthread_mutex_t *anj_4g_at_mutex;              /* at模块对外接口互斥锁 */
    pthread_mutex_t *anj_4g_at_comm_mutex;         /* at模块读写互斥锁 */
    anj_thread_s stAtThread;                       /* at模块线程状态 */
    THREAD_RUN_STATUS thread_at_pause_flag;        /* at模块线程暂停标志 */
    AT_INFO stAtInfo;                              /* at模块信息 */
    int anj_4g_at_failed_cnt[MOBILE4G_STATUS_MAX]; /* at模块连续通信出错次数 */
    anj_thread_s stHeartBeatThread;                /* 心跳模块线程状态 */
    THREAD_RUN_STATUS heartbeat_pause_flag;        /* 心跳模块线程暂停标志 */
    int bind_flag;                                 /* 绑定标志 */
    int first_ping_flag;                           /* 强制ping */
    int play_net_connectd_flag;                    /* 播放网络连接成功标志 */
    int cur_dididi_cnt;                            /* 播放嘀嘀嘀计数 */
    int update_iccid_flag;                         /* 更新iccid标志 */
    char lsanj_4g_at_iccid[AT_INFO_SIZE];          /* 上一次iccid */
    int simChange;
    int simInsert[IPC_4G_SIMCARD_NUM];
    int need_module_restart; /* 异步重启标志，供事件回调设置，线程执行重启 */
    int bInit;
} MOBILE4G_PARAM;
static MOBILE4G_PARAM stMobile4gParam;

typedef struct
{
    int play_no_4g_cnt;
    int play_connect_net_fail_cnt;
    int play_connecting_net_cnt;
    int play_scan_qr_code_cnt;
} MOBILE4G_AUDIO_PARAM;
static MOBILE4G_AUDIO_PARAM stMobile4gAudioParam = {0};

static void anj_4g_audio_param_reset(void)
{
    memset(&stMobile4gAudioParam, 0, sizeof(stMobile4gAudioParam));
    stMobile4gAudioParam.play_no_4g_cnt = MOBILE4G_NO_4G_MAX_CNT;
    stMobile4gAudioParam.play_connect_net_fail_cnt = MOBILE4G_CONNECT_NET_FAIL_MAX_CNT;
    stMobile4gAudioParam.play_connecting_net_cnt = MOBILE4G_CONNECTING_NET_MAX_CNT;
    stMobile4gAudioParam.play_scan_qr_code_cnt = MOBILE4G_SCAN_QRCODE_MAX_CNT;
}

static MOBILE4G_ERROR s_mErrorIndex = MOBILE4G_ERROR_NONE;
static AT_CMD *s_pstAtCmd = NULL;
static ANJ_4G_MANUFACTURER s_iManufacturer = ANJ_4G_MANUFACTURER_NONE;
static G4InfoStruct g_g4Status = {0};

static int anj_4g_osd_show(int flag)
{
    SystemConfig *pSystemConfig = getSystemConfig();
    osd_custom_content_s osd4gCustom = {0};
    osd4gCustom.custom_show = 1;
    osd4gCustom.custom_x = 0;
    osd4gCustom.custom_y = 1;
    osd4gCustom.custom_location = POSITION_TYPE_BY_FOUR_CORNER;
    char text_utf8[64] = {0};
    // static char text_utf8_sim1_error[32] = {0};
    // static char text_utf8_sim2_error[32] = {0};

    if (flag)
    {
        if (flag == 2)
        {
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr),
                    "%s %s", g_g4Status.Manufacturer, g_g4Status.Model);
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "^");
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr),
                    "%s %s %s", g_g4Status.IMEI, g_g4Status.IMSI, g_g4Status.MSISDN);
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "^");
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr),
                    "%s | Dial Status: %d  ", g_g4Status.WorkMode, g_g4Status.nDialStatus);
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "^");
        }

        if (g_g4Status.nSimStatus == 0)
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "SIM%s", "卡不可用");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "SIM%s", "卡不可用");
            }
            else
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "SIM unvailable");
            }
        }
        else
        {
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), "%3d%%", g_g4Status.nSignalLevel);
        }

        if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
        {
            memset(text_utf8, 0, sizeof(text_utf8));
            if (strcmp(g_g4Status.Operator, "CHN-UNICOM") == 0 || strcmp(g_g4Status.Operator, "China Unicom") == 0)
            {
                strcpy(text_utf8, "中国联通");
            }
            else if (strcmp(g_g4Status.Operator, "CHINA MOBILE") == 0 || strcmp(g_g4Status.Operator, "China Mobile") == 0)
            {
                strcpy(text_utf8, "中国移动");
            }
            else if (strcmp(g_g4Status.Operator, "CHN-CT") == 0 || strcmp(g_g4Status.Operator, "China Telecom") == 0 || strcmp(g_g4Status.Operator, "chn-ct") == 0)
            {
                strcpy(text_utf8, "中国电信");
            }

            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", text_utf8);
        }
        else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
        {
            memset(text_utf8, 0, sizeof(text_utf8));
            if (strcmp(g_g4Status.Operator, "CHN-UNICOM") == 0 || strcmp(g_g4Status.Operator, "China Unicom") == 0)
            {
                strcpy(text_utf8, "中國聯通");
            }
            else if (strcmp(g_g4Status.Operator, "CHINA MOBILE") == 0 || strcmp(g_g4Status.Operator, "China Mobile") == 0)
            {
                strcpy(text_utf8, "中國移動");
            }
            else if (strcmp(g_g4Status.Operator, "CHN-CT") == 0 || strcmp(g_g4Status.Operator, "China Telecom") == 0 || strcmp(g_g4Status.Operator, "chn-ct") == 0)
            {
                strcpy(text_utf8, "中國電信");
            }

            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", text_utf8);
        }
        else
        {
            sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", g_g4Status.Operator);
        }

        switch (g_g4Status.nSvrStatus)
        {
        case G4_SERVICE_NO:
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "无服务");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "無服務");
            }
            else
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "No service");
            }
        }
        break;
        case G4_SERVICE_LIMITED:
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "服务限制");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "服務限制");
            }
            else
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "Limited service");
            }
        }
        break;
        case G4_SERVICE_AVAILABLE:
            break;
        case G4_SERVICE_LIMITED_REGIONAL:
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "区域服务限制");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "區域服務限制");
            }
            else
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "Limited regional service");
            }
        }
        break;
        case G4_SERVICE_POWER_SAVE:
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "省电模式");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "省電模式");
            }
            else
            {
                sprintf(osd4gCustom.overlayStr + strlen(osd4gCustom.overlayStr), " | %s", "Power save");
            }
        }

        break;
        }

        // if (g_dual_sim_dual_standby)
        // {
        //     if (g_sim0_status == 0)
        //     {
        //         if (strlen(text_utf8_sim1_error) == 0)
        //         {
        //             gb2312_to_utf8(" | SIM卡1异常", text_utf8_sim1_error);
        //         }

        //         strcat(osd4gCustom.overlayStr, text_utf8_sim1_error);
        //     }

        //     if (g_sim1_status == 0)
        //     {

        //         if (strlen(text_utf8_sim2_error) == 0)
        //         {
        //             gb2312_to_utf8(" | SIM卡2异常", text_utf8_sim2_error);
        //         }

        //         strcat(osd4gCustom.overlayStr, text_utf8_sim2_error);
        //     }
        // }
    }
    else
    {
        memset(osd4gCustom.overlayStr, 0, sizeof(osd4gCustom.overlayStr));
    }
    anj_osd_4g_set(&osd4gCustom);

    return 0;
}

static void anj_4g_osd_set(EventResult *event_result, void *data)
{
    if (data)
    {
        int flag = *((int *)data);
        stMobile4gParam.osd_flag = flag;
    }
}

/*****************************************************************************
 函 数 名  : anj_4g_get_manufacturer
 功能描述  : 获取4g模块厂家
 输入参数  : NULL
 输出参数  : -1: 不支持的厂家 0: 美格 1:移远
 返 回 值  : 模块厂家
*****************************************************************************/
static ANJ_4G_MANUFACTURER anj_4g_get_manufacturer()
{
    FILE *fp = NULL;
    char buf[256] = {0};
    memset(buf, 0, sizeof(buf));
    ANJ_4G_MANUFACTURER iManufacturer = ANJ_4G_MANUFACTURER_NONE;
    if ((fp = popen("lsusb", "r")) == NULL)
    {
        __ERR("Fail to popen lsusb\n");
        return -1;
    }
    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        if (strstr(buf, QUECTIL_VID))
        {
            iManufacturer = ANJ_4G_MANUFACTURER_QUECTEL;
            s_pstAtCmd = stQuectelAtCmd;
        }
        else if (strstr(buf, UM_VID))
        {
            // s_pstAtCmd = stYugeAtCmd;
            // iManufacturer = ANJ_4G_MANUFACTURER_YUGE;
            // if (anj_mw_file_exists("/opt/ch/flag.um.4g"))
            {
                s_pstAtCmd = stUmAtCmd;
                iManufacturer = ANJ_4G_MANUFACTURER_UM;
            }
        }
        else if (strstr(buf, CIS_VID))
        {
            s_pstAtCmd = stCisAtCmd;
            iManufacturer = ANJ_4G_MANUFACTURER_CIS;
        }
        else if (strstr(buf, SIMCOM_VID))
        {
            s_pstAtCmd = stSimcomAtCmd;
            iManufacturer = ANJ_4G_MANUFACTURER_SIMCOM;
        }
        else if (strstr(buf, HZ_VID))
        {
            s_pstAtCmd = stHzAtCmd;
            iManufacturer = ANJ_4G_MANUFACTURER_HZ;
        }
        // else if (strstr(buf, NOTION_VID))
        // {
        //     iManufacturer = ANJ_4G_MANUFACTURER_NONE;
        // }
    }
    pclose(fp);
    if (iManufacturer == ANJ_4G_MANUFACTURER_NOTION)
    {
        anj_mw_system("insmod /lib/modules/usb-common.ko");
        anj_mw_system("insmod /lib/modules/usbcore.ko");
        anj_mw_system("insmod /lib/modules/usbnet.ko");
        anj_mw_system("insmod /lib/modules/ehci-hcd.ko");
        anj_mw_system("insmod /lib/modules/cdc_ether.ko");
        anj_mw_system("insmod /lib/modules/rndis_host.ko");
        anj_mw_system("insmod /lib/modules/cdc-acm.ko");
    }
    else if (iManufacturer == ANJ_4G_MANUFACTURER_YUGE)
    {
        anj_mw_system("insmod /lib/modules/usbserial.ko");
        anj_mw_system("insmod /lib/modules/usb_wwan.ko");
        anj_mw_system("insmod /lib/modules/option.ko");
        anj_mw_system("insmod /lib/modules/usbnet.ko");
        anj_mw_system("insmod /lib/modules/cdc_ether.ko");
        anj_mw_system("insmod /lib/modules/cdc_ncm.ko");
        anj_mw_system("insmod /lib/modules/cdc_subset.ko");
        anj_mw_system("insmod /lib/modules/libphy.ko");
        anj_mw_system("insmod /lib/modules/net1080.ko");
        anj_mw_system("insmod /lib/modules/of_mdio.ko");
        anj_mw_system("insmod /lib/modules/rndis_host.ko");
        anj_mw_system("insmod /lib/modules/zaurus.ko");
        anj_mw_system("insmod /lib/modules/asix.ko");
    }
    else if ((iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL) ||
             (iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM) ||
             (iManufacturer == ANJ_4G_MANUFACTURER_CIS) ||
             (iManufacturer == ANJ_4G_MANUFACTURER_UM) ||
             (iManufacturer == ANJ_4G_MANUFACTURER_HZ))
    {
        anj_mw_system("insmod /lib/modules/usbserial.ko");
        anj_mw_system("insmod /lib/modules/usb_wwan.ko");
        anj_mw_system("insmod /lib/modules/usbnet.ko");
        anj_mw_system("insmod /lib/modules/cdc_ether.ko");
        anj_mw_system("insmod /lib/modules/cdc_ncm.ko");
        anj_mw_system("insmod /lib/modules/cdc_subset.ko");
        anj_mw_system("insmod /lib/modules/libphy.ko");
        anj_mw_system("insmod /lib/modules/net1080.ko");
        anj_mw_system("insmod /lib/modules/of_mdio.ko");
        anj_mw_system("insmod /lib/modules/rndis_host.ko");
        anj_mw_system("insmod /lib/modules/zaurus.ko");
        anj_mw_system("insmod /lib/modules/asix.ko");
        anj_mw_system("insmod /lib/modules/option.ko");
    }

    return iManufacturer;
}

static int anj_4g_add_capability(ANJ_4G_MANUFACTURER mobile4gDevType)
{
    if (mobile4gDevType == ANJ_4G_MANUFACTURER_YUGE)
    {
        anj_sysctl_capability_add(FUNCTION_MOBILE_NET);
        anj_sysctl_capability_add(FUNCTION_SMS);
    }
    if ((mobile4gDevType == ANJ_4G_MANUFACTURER_QUECTEL) ||
        (mobile4gDevType == ANJ_4G_MANUFACTURER_SIMCOM) ||
        (mobile4gDevType == ANJ_4G_MANUFACTURER_CIS) ||
        (mobile4gDevType == ANJ_4G_MANUFACTURER_UM) ||
        (mobile4gDevType == ANJ_4G_MANUFACTURER_HZ))
    {
        anj_sysctl_capability_add(FUNCTION_MOBILE_NET);

        if (IPC_4G_SIMCARD_NUM > 1)
        {
            anj_sysctl_capability_add(FUNCTION_4G_SWITCH_2CARD);
        }
    }

    anj_sysctl_capability_add(FUNCTION_LOCATION);
    anj_ser_reponse(SER_RESPONSE_ABILITY, NULL);
    return 0;
}

/*****************************************************************************
 函 数 名  : anj_4g_comm_uninit
 功能描述  : 4g模块通信端口去初始化
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static void anj_4g_comm_uninit()
{
    if (stMobile4gParam.uart_fd > 0)
    {
        close(stMobile4gParam.uart_fd);
        stMobile4gParam.uart_fd = -1;
    }
}

/*****************************************************************************
 函 数 名  : anj_4g_comm_init
 功能描述  : 4g模块通信端口初始化
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : 成功fd，失败-1
*****************************************************************************/
static int anj_4g_comm_init(int mobile4gDevType)
{
    const char *tty_device = TTY_DEVICE_2_4G;
    if (mobile4gDevType == ANJ_4G_MANUFACTURER_HZ)
    {
        if (access(TTY_DEVICE_1_4G, F_OK) == 0)
        {
            tty_device = TTY_DEVICE_1_4G;
        }
        if (access(TTY_DEVICE_2_4G, F_OK) == 0)
        {
            tty_device = TTY_DEVICE_2_4G;
        }
    }
    else if (mobile4gDevType == ANJ_4G_MANUFACTURER_UM)
    {
        if (access(TTY_DEVICE_3_4G, F_OK) == 0)
        {
            tty_device = TTY_DEVICE_3_4G;
        }
    }
    else if (IPC_4G_USE_USB0)
    {
        if (access(TTY_DEVICE_0_4G, F_OK) == 0)
        {
            tty_device = TTY_DEVICE_0_4G;
        }
        else
        {
            return -1;
        }
    }

    anj_4g_comm_uninit();
    stMobile4gParam.uart_fd = open(tty_device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (stMobile4gParam.uart_fd <= 0)
    {
        __ERR("open %s fail(%s)\n", tty_device, strerror(errno));
        return -1;
    }

    struct termios newtio;
    if (tcgetattr(stMobile4gParam.uart_fd, &newtio) != 0)
    {
        memset(&newtio, 0, sizeof(newtio));
    }
    cfsetispeed(&newtio, B115200);
    cfsetspeed(&newtio, B115200);
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~PARENB;

    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // input
    newtio.c_oflag &= ~OPOST;                          // output

    /* 非阻塞读取：VMIN=0, VTIME>0（0.2s）便于 select+read 协作 */
    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 2; /* 200ms */
    if (tcsetattr(stMobile4gParam.uart_fd, TCSANOW, &newtio) != 0)
    {
        close(stMobile4gParam.uart_fd);
        stMobile4gParam.uart_fd = -1;

        __ERR("com set error\n");
    }
    else
    {
        tcflush(stMobile4gParam.uart_fd, TCIFLUSH);
    }
    return stMobile4gParam.uart_fd;
}

static int anj_4g_comm_write(char *buffer, unsigned int len, int timeout_ms)
{
    int ret = -1;
    if (stMobile4gParam.uart_fd > 0 && buffer != NULL)
    {
        int bWriteOK = 0;
        int tv_sec = timeout_ms / 1000;
        int tv_usec = timeout_ms % 1000 * 1000;

        fd_set writefd;
        struct timeval wait_time;
        wait_time.tv_sec = tv_sec;
        wait_time.tv_usec = tv_usec;
        while (1)
        {
            FD_ZERO(&writefd);
            FD_SET(stMobile4gParam.uart_fd, &writefd);

            wait_time.tv_sec = tv_sec;
            wait_time.tv_usec = tv_usec;

            ret = select(stMobile4gParam.uart_fd + 1, NULL, &writefd, NULL, &wait_time);
            if (ret < 0)
            {
                if (errno != EINTR)
                {
                    __ERR("select failed, err=%s\n", strerror(errno));
                    break;
                }
                else
                {
                    __ERR("select failed, err=%s\n", strerror(errno));
                    continue;
                }
            }
            else if (ret == 0)
            {
                __ERR("select timeout(cmd=%s)\n", buffer);
                break;
            }

            if (FD_ISSET(stMobile4gParam.uart_fd, &writefd))
            {
                __INFO("BUFFER:%s\n", buffer);
                ret = safe_write(stMobile4gParam.uart_fd, buffer, len);
                if (ret == len)
                {
                    bWriteOK = 1;
                }
                else
                {
                    break;
                }

                // 发送回车换行
                ret = safe_write(stMobile4gParam.uart_fd, "\r\n", 2);
                if (ret == 2)
                    bWriteOK = 1;

                break;
            }
        }

        if (!bWriteOK)
        {
            __ERR("return failed\n");
            return -1;
        }
        else
        {
            return 0;
        }
    }
    return ret;
}

static int anj_4g_comm_read(char *buffer, unsigned int bufLen, int timeout_ms)
{
    int ret = -1;
    if (stMobile4gParam.uart_fd > 0 && buffer != NULL)
    {
        int bReadOK = 0;
        // 200ms select一次, 直到没数据可读
        int nTotalTimes = (timeout_ms == 0) ? 2 : (timeout_ms / 200);
        int times = 0;
        fd_set readfd;
        struct timeval wait_time;

        while (times++ < nTotalTimes)
        {
            int len = strlen(buffer);
            if (len >= bufLen)
                break;

            FD_ZERO(&readfd);
            FD_SET(stMobile4gParam.uart_fd, &readfd);

            wait_time.tv_sec = 0;
            wait_time.tv_usec = 200 * 1000;

            ret = select(stMobile4gParam.uart_fd + 1, &readfd, NULL, NULL, &wait_time);
            if (ret < 0)
            {
                if (errno != EINTR)
                {
                    __ERR("select failed, err=%s\n", strerror(errno));
                    break;
                }
                else
                {
                    __ERR("select failed, err=%s\n", strerror(errno));
                    if (bReadOK)
                        break;
                    else
                    {
                        continue;
                    }
                }
            }
            else if (ret == 0)
            {
                if (bReadOK)
                {
                    if (timeout_ms == 0) // 未设置timeout，读到数据后如果再次select超时了就返回
                        break;
                    else // 设置了timeout，就一定等这么久
                        continue;
                }
                else
                {
                    continue;
                }
            }

            if (FD_ISSET(stMobile4gParam.uart_fd, &readfd))
            {
                int ret = 0;
                ret = safe_read(stMobile4gParam.uart_fd, buffer + len, bufLen - len);
                if (ret > 0)
                {
                    int rsplen = strlen(buffer);
                    bReadOK = 1;

                    if (strstr(buffer, "OK") != NULL && (buffer[rsplen - 1] == '\n'))
                    {
                        break;
                    }
                }
            }
        }

        if (!bReadOK)
        {
            __ERR("read select timeout\n");
            return -1;
        }
    }
    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_send_and_recv
 功能描述  : 和4g模块通信
 输入参数  : anj_4g_at_seq_type 命令类型，timeout_ms 超时时间
 输出参数  : NULL
 返 回 值  : 成功0，失败-1
*****************************************************************************/
static int anj_4g_send_and_recv(AT_CMD *pstAtCmd, AT_SEQ anj_4g_at_seq_type, int timeout_ms)
{
    int ret = -1;
    char recv_buf[256] = {0};

    if (stMobile4gParam.uart_fd < 0 || anj_4g_at_seq_type >= AT_CMD_MAX_NUMS)
    {
        return ret;
    }

    if (strlen(pstAtCmd[anj_4g_at_seq_type].cmd) == 0)
    {
        return 0;
    }

    if (stMobile4gParam.debug_flag)
    {
        __DBG("send_buf = %s", pstAtCmd[anj_4g_at_seq_type].cmd);
    }

    /* 串口写内部有锁 */
    ret = anj_4g_comm_write(pstAtCmd[anj_4g_at_seq_type].cmd, strlen(pstAtCmd[anj_4g_at_seq_type].cmd), timeout_ms);
    if (ret < 0)
    {
        return -1;
    }

    /* AT读加锁 */
    anj_mutex_lock(stMobile4gParam.anj_4g_at_comm_mutex);

    /* 重启at命令后不需要读 */
    if (anj_4g_at_seq_type == AT_SEQ_RESET_SET)
    {
        anj_mutex_unlock(stMobile4gParam.anj_4g_at_comm_mutex);
        return 0;
    }

    ret = anj_4g_comm_read(recv_buf, sizeof(recv_buf), timeout_ms);
    if (ret < 0)
    {
        __ERR("anj_4g_comm_read error\n");
        anj_mutex_unlock(stMobile4gParam.anj_4g_at_comm_mutex);
        return -1;
    }

    if (pstAtCmd[anj_4g_at_seq_type].recv_fun)
    {
        ret = pstAtCmd[anj_4g_at_seq_type].recv_fun(recv_buf);
    }
    else
    {
        ret = 0;
    }

    anj_mutex_unlock(stMobile4gParam.anj_4g_at_comm_mutex);

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_try_send
 功能描述  : 尝试和4g模块多次通信
 输入参数  : anj_4g_at_seq_type 命令类型，timeout_ms 超时时间，nums 尝试次数，error_fun 错误处理
 输出参数  : NULL
 返 回 值  : 成功0，失败-1
*****************************************************************************/
static int anj_4g_try_send(AT_CMD *pstAtCmd, AT_SEQ cmd, int timeout_ms, int nums, void (*error_fun)())
{
    int ret = -1;
    for (int i = 0; i < nums; i++)
    {
        ret = anj_4g_send_and_recv(pstAtCmd, cmd, timeout_ms);
        if (ret == 0)
        {
            break;
        }
        if (error_fun)
        {
            error_fun();
        }
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_check_connect
 功能描述  : at连通性判断，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_at_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_check_connect(char *recvBuf)
{
    int ret = -1;

    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_close_echo
 功能描述  : 关闭回显，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_at_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_close_echo(char *recvBuf)
{
    int ret = -1;

    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_query_module_status
 功能描述  : 查询模块工作状态，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_at_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_query_module_status(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_set_module_status
 功能描述  : 设置模块工作状态1，返回数据处理
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_set_module_status(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_clear_module_status
 功能描述  : 设置模块工作状态0，返回数据处理
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_clear_module_status(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

static int anj_4g_query_usb_num(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

static int anj_4g_query_module_info(char *recvBuf)
{
    int ret = -1;
    int iIndex = 0;
    const char s[4] = "\r\n";
    char *p = NULL;
    char *token = NULL;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    if ((s_iManufacturer == ANJ_4G_MANUFACTURER_CIS) ||
        (s_iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM) ||
        (s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL) ||
        (s_iManufacturer == ANJ_4G_MANUFACTURER_UM))
    {
        char *pB = strstr(recvBuf, "ATI");
        if (pB == NULL)
        {
            pB = recvBuf;
        }
        else
        {
            pB = pB + strlen("ATI");
        }

        token = strtok_r(pB, s, &p);
        while (token != NULL)
        {
            if (iIndex == 0)
                strncpy(g_g4Status.Manufacturer, token, G4_STR_LEN_32 - 1);
            else if (iIndex == 1)
                strncpy(g_g4Status.Model, token, G4_STR_LEN_32 - 1);
            else if ((s_iManufacturer != ANJ_4G_MANUFACTURER_UM) && (iIndex == 2))
            {
                char szPrefix[32];
                sprintf(szPrefix, "Revision:");
                p = strcasestr(token, szPrefix);
                if (p != NULL)
                {
                    p += strlen(szPrefix);
                    while (isspace(*p) && p < token + strlen(token))
                        p++;
                    strncpy(g_g4Status.Revision, p, G4_STR_LEN_256 - 1);
                }
                else
                {
                    strncpy(g_g4Status.Revision, token, G4_STR_LEN_256 - 1);
                }
            }
            else
            {
                break;
            }

            iIndex++;
            token = strtok_r(NULL, s, &p);
        }
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_YUGE)
    {
        /* 获取第一个子字符串 */
        token = strtok(recvBuf, s);
        /* 继续获取其他的子字符串 */
        while (token != NULL)
        {
            char szPrefix[32];
            sprintf(szPrefix, "Manufacturer:");
            p = strcasestr(token, szPrefix);
            if (p != NULL)
            {
                p += strlen(szPrefix);
                while (isspace(*p) && p < token + strlen(token))
                    p++;

                strncpy(g_g4Status.Manufacturer, p, G4_STR_LEN_32 - 1);
            }
            else
            {
                sprintf(szPrefix, "Model:");
                p = strcasestr(token, szPrefix);
                if (p != NULL)
                {
                    p += strlen(szPrefix);
                    while (isspace(*p) && p < token + strlen(token))
                        p++;
                    strncpy(g_g4Status.Model, p, G4_STR_LEN_32 - 1);
                }
                else
                {
                    sprintf(szPrefix, "IMEI:");
                    p = strcasestr(token, szPrefix);
                    if (p != NULL)
                    {
                        p += strlen(szPrefix);
                        while (isspace(*p) && p < token + strlen(token))
                            p++;
                        strncpy(g_g4Status.IMEI, p, G4_STR_LEN_32 - 1);
                    }
                }
            }

            token = strtok(NULL, s);
        }
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_HZ)
    {
        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
        {
            char *head_iccid = "+CGMI:";
            char *p = strcasestr(recvBuf, head_iccid);
            if (p != NULL)
            {
                p += strlen(head_iccid);

                char *pOK = strcasestr(p, "OK");
                if (NULL != pOK)
                    *pOK = 0;
                string_trim_head(p);
                string_trim_tail(p);

                strncpy(g_g4Status.Manufacturer, p, G4_STR_LEN_32 - 1);
                string_remove(g_g4Status.Manufacturer, '"');
            }
        }
        else
        {
            char *pB = strstr(recvBuf, "ATI");
            if (pB == NULL || strstr(recvBuf, "OK") == NULL)
            {
                return 0;
            }
            else
            {
                pB = pB + strlen("ATI");
            }

            int iIndex = 0;
            token = strtok_r(pB, s, &p);
            while (token != NULL)
            {
                if (iIndex == 0)
                {
                    char *p1 = strchr(token, ',');
                    if (p1)
                    {
                        *p1 = '\0';
                        strncpy(g_g4Status.Manufacturer, token, G4_STR_LEN_32 - 1);

                        string_trim_head(p1 + 1);
                        string_trim_tail(p1 + 1);
                        strncpy(g_g4Status.Model, p1 + 1, G4_STR_LEN_32 - 1);
                        break;
                    }
                    else
                    {
                        strncpy(g_g4Status.Manufacturer, token, G4_STR_LEN_32 - 1);
                    }
                }
                else if (iIndex == 1)
                {
                    char *p1 = strstr(token, "Board:");
                    if (p1)
                    {
                        p1 = p1 + strlen("Board:");
                        strncpy(g_g4Status.Model, p1, G4_STR_LEN_32 - 1);
                    }
                    else
                    {
                        strncpy(g_g4Status.Model, token, G4_STR_LEN_32 - 1);
                    }
                    break;
                }
                else
                    break;

                iIndex++;
                token = strtok_r(NULL, s, &p);
            }
        }
    }

    __INFO("Manufacturer:%s, Model:%s, IMEI:%s\n", g_g4Status.Manufacturer, g_g4Status.Model, g_g4Status.IMEI);

    return ret;
}

static int anj_4g_query_module_info1(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    char *p = strstr(recvBuf, "AT+CGSN");
    if (p == NULL)
    {
        p = recvBuf;
    }
    else
    {
        p = p + strlen("AT+CGSN");
    }

    char *pOK = strcasestr(p, "OK");
    if (NULL != pOK)
        *pOK = 0;

    string_trim_head(p);
    string_trim_tail(p);

    strncpy(g_g4Status.IMEI, p, G4_STR_LEN_32 - 1);
    string_remove(g_g4Status.IMEI, '"');

    __INFO("IMEI:%s\n", g_g4Status.IMEI);

    return ret;
}

static int anj_4g_query_module_info2(char *recvBuf)
{
    int ret = -1;
    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    char *head_iccid = "+CGMM:";
    char *p = strcasestr(recvBuf, head_iccid);
    if (p != NULL)
    {
        p += strlen(head_iccid);

        string_trim_head(p);
        string_trim_tail(p);

        strncpy(g_g4Status.Model, p, G4_STR_LEN_32 - 1);
        string_remove(g_g4Status.Model, '"');
    }

    __INFO("Model:%s\n", g_g4Status.Model);

    return ret;
}

static int anj_4g_set_hotplug(char *recvBuf)
{
    int ret = -1;

    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

static int anj_4g_clear_hotplug(char *recvBuf)
{
    int ret = -1;

    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

static int anj_4g_query_dspin(char *recvBuf)
{
    int ret = -1;

    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
    {
        char tmpBuf[64] = {0};
        snprintf(tmpBuf, sizeof(tmpBuf), "+QDSPIN: %d,", i);
        char *p = strstr(recvBuf, tmpBuf);
        if (p)
        {

            p = p + strlen("+QDSPIN: 1,");
            char *p1 = strchr(p, '\n');
            if (p1)
            {
                *p1 = '\0';
            }
            p1 = strchr(p, '\r');
            if (p1)
            {
                *p1 = '\0';
            }

            if (strstr(p, "NOT") != NULL)
            {
                stMobile4gParam.simInsert[i] = 0;
            }
            else
            {
                stMobile4gParam.simInsert[i] = 1;
            }
        }
    }

    return ret;
}

static int anj_4g_query_dstype(char *recvBuf)
{
    int ret = -1;

    __INFO("%s\n", recvBuf);
    char *p = strcasestr(recvBuf, "+QDSTYPE: 0");
    if (p)
    {
        ret = 0;
    }

    return ret;
}

static int anj_4g_query_sim_index(char *recvBuf)
{
    int ret = -1;

    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
    {
        char tmpBuf[64] = {0};
        if (s_iManufacturer == ANJ_4G_MANUFACTURER_UM)
        {
            snprintf(tmpBuf, sizeof(tmpBuf), "+SINGLESIM: %d,", i);
        }
        else if (s_iManufacturer == ANJ_4G_MANUFACTURER_HZ)
        {
            snprintf(tmpBuf, sizeof(tmpBuf), "+SIMCROSS: %d,", i);
        }
        else
        {
            snprintf(tmpBuf, sizeof(tmpBuf), "+QDSIM: %d,", i);
        }
        if (strcasestr(recvBuf, tmpBuf))
        {
            g_g4Status.CurOperator = i;
        }
    }

    return ret;
}

// static int anj_4g_query_msidn(char *recvBuf)
// {
//     int ret = -1;
//     __INFO("%s\n", recvBuf);
//     if (strcasestr(recvBuf, "OK"))
//     {
//         __INFO("success\n");
//         ret = 0;
//     }
//     else
//     {
//         __ERR("failed\n");
//         return ret;
//     }

//     char *p = strcasestr(recvBuf, "+CNUM:");
//     if (p != NULL)
//     {
//         p += strlen("+CNUM:");

//         string_trim_head(p);
//         string_trim_tail(p);

//         const char s[4] = ",";
//         char *token = NULL;
//         char *pTmp = NULL;
//         char *pB = p;

//         int iIndex = 0;
//         token = strtok_r(pB, s, &pTmp);
//         while (token != NULL)
//         {
//             if (iIndex == 1)
//             {
//                 strncpy(g_g4Status.MSISDN, token, G4_STR_LEN_32 - 1);
//                 string_remove(g_g4Status.MSISDN, '"');
//                 break;
//             }

//             iIndex++;
//             token = strtok_r(NULL, s, &pTmp);
//         }
//     }
//     return ret;
// }

// static int anj_4g_query_sysinfo(char *recvBuf)
// {
//     int ret = -1;
//     __INFO("%s\n", recvBuf);
//     if (strcasestr(recvBuf, "OK"))
//     {
//         __INFO("success\n");
//         ret = 0;
//     }
//     else
//     {
//         __ERR("failed\n");
//         return ret;
//     }
//     int srv_status = 0, srv_domain = 0, roam_status = 0, sys_mode = 0, sim_state = 0;
//     char WorkMode[G4_STR_LEN_32] = {0}; // 当前工作网络模式, LAN/NO_SRV/WCDMA/LTE

//     char *p = strcasestr(recvBuf, "SYSINFO:");
//     if (p != NULL)
//     {
//         p = p + strlen("SYSINFO:");
//         //^SYSINFO: <srv_status>,<srv_domain>,<roam_status>,<sys_mode>,<sim_state>[,<reg_mode>]
//         if (sscanf(p, "%d,%d,%d,%d,%d",
//                    &srv_status, &srv_domain, &roam_status, &sys_mode, &sim_state) != 5)
//         {
//             __ERR("error: %s\n", recvBuf);
//         }
//         else
//         {
//             __INFO("%d, %d, %d, %d, %d\n", srv_status, srv_domain, sys_mode, roam_status, sim_state);
//             g_g4Status.nSimStatus = sim_state;
//             g_g4Status.nSvrStatus = (G4_srv_status)srv_status;
//             switch (sys_mode)
//             {
//             case 0:
//                 strcpy(WorkMode, "No_SRV");
//                 break;
//             case 5:
//                 strcpy(WorkMode, "WCDMA");
//                 break;
//             case 9:
//                 strcpy(WorkMode, "LTE");
//                 break;
//             default:
//                 sprintf(WorkMode, "%d", sys_mode);
//             }
//             if (is_network_connect(WIRE_INTERFACE_NAME))
//             {
//                 strcpy(g_g4Status.WorkMode, "LAN");
//             }
//             else
//             {
//                 strcpy(g_g4Status.WorkMode, WorkMode);
//             }
//         }
//     }
//     return ret;
// }

static int anj_4g_get_cgpaddr(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    if (strstr(recvBuf, "+CGPADDR: 1,\"") != NULL)
    {
        char sup_gb28181_ip[64];
        memset(sup_gb28181_ip, 0, 64);
        sscanf(recvBuf, "%*[^\"]\"%[^\"]", sup_gb28181_ip);
        __INFO("CGPADDR-sup_gb28181_ip:%s\n", sup_gb28181_ip);
    }
    return ret;
}

static int anj_4g_query_sim_status(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }
    char *p = strcasestr(recvBuf, "READY");
    if (p)
    {
        g_g4Status.nSimStatus = 1; // 先设置一下，移远模组下面通过SYSINFO读不到
    }
    else
    {
        g_g4Status.nSimStatus = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_query_signal
 功能描述  : 查询信号强度，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_at_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_query_signal(char *recvBuf)
{
    int ret = -1;

    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
        int rssi, ber;
        char *p = strcasestr(recvBuf, "+CSQ:");
        if (p != NULL)
        {

            if (sscanf(p + strlen("+CSQ:"), "%d,%d", &rssi, &ber) != 2)
            {
                __ERR("error: %s\n", recvBuf);
            }
            else
            {
                if (rssi < 0 || rssi > 31)
                    g_g4Status.nSignalLevel = 0;
                else // 0-31
                {
                    g_g4Status.nSignalLevel = rssi * 100 / 31;
                }

                __ERR("%d, %d: signal level %d\n", rssi, ber, g_g4Status.nSignalLevel);
            }
        }
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_query_operator
 功能
 功能描述  : 查询运营商，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_at_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_query_operator(char *recvBuf)
{
    int ret = -1;
    int format = 0, sys = 0;
    char oper[64] = {0};
    __INFO("%s", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    char *p = strcasestr(recvBuf, "+COPS:");
    if (p != NULL)
    {
        p = p + strlen("+COPS:");
        //+COPS: <mode>[,<format>,<oper>,<sys>]

        int iIndex = 0;
        const char s[4] = ",";
        char *token = NULL;
        token = strtok(recvBuf, s);
        while (token != NULL)
        {
            if (iIndex == 1)
                format = atoi(token);
            else if (iIndex == 2)
            {
                // 去掉双引号
                if (token[0] == '"')
                    token++;

                int len = strlen(token);
                if (len > 0 && token[len - 1] == '"')
                    token[len - 1] = 0;

                strcpy(oper, token);
            }
            else if (iIndex == 3)
                sys = atoi(token);

            iIndex++;
            token = strtok(NULL, s);
        }

        if (strlen(oper) == 0)
        {
            __ERR("error: %s\n", recvBuf);
            return -1;
        }

        switch (format)
        {
        case 2:
        {
            if (strcasestr(oper, "46011"))
            {
                strcpy(g_g4Status.Operator, "China Telecom");
            }
            else if (strcasestr(oper, "46001") || strcasestr(oper, "46006"))
            {
                strcpy(g_g4Status.Operator, "China Unicom");
            }
            else if (strcasestr(oper, "46000") ||
                     strcasestr(oper, "46002") ||
                     strcasestr(oper, "46007") ||
                     strcasestr(oper, "46004"))
            {
                strcpy(g_g4Status.Operator, "China Mobile");
            }
            else
            {
                strncpy(g_g4Status.Operator, oper, G4_STR_LEN_32 - 1);
                g_g4Status.Operator[G4_STR_LEN_32 - 1] = '\0';
            }
        }
        break;
        default:
            strncpy(g_g4Status.Operator, oper, G4_STR_LEN_32 - 1);
            g_g4Status.Operator[G4_STR_LEN_32 - 1] = '\0';
            break;
        }
    }

    if ((s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL) ||
        (s_iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM))
    {
        char WorkMode[G4_STR_LEN_32] = {0}; // 当前工作网络模式, LAN/NO_SRV/WCDMA/LTE
        /*
            0 GSM
            2 UTRAN
            3 GSM W/EGPRS
            4 UTRAN W/HSDPA
            5 UTRAN W/HSUPA
            6 UTRAN W/HSDPA an
            7 E-UTRAN
            8 UTRAN HSPA+
        */
        switch (sys)
        {
        case 0:
            strcpy(WorkMode, "GSM");
            break;
        case 2:
            strcpy(WorkMode, "UTRAN");
            break;
        case 3:
            strcpy(WorkMode, "W/EGPRS");
            break;
        case 4:
            strcpy(WorkMode, "W/HSDPA");
            break;
        case 5:
            strcpy(WorkMode, "W/HSUPA");
            break;
        case 6:
            strcpy(WorkMode, "W/HSD-UPA");
            break;
        case 7:
            strcpy(WorkMode, "E-UTRAN");
            break;
        case 8:
            strcpy(WorkMode, "HSPA+");
            break;
        default:
            sprintf(WorkMode, "%d", sys);
        }
        if (is_network_connect(WIRE_INTERFACE_NAME) > 0)
        {
            strcpy(g_g4Status.WorkMode, "LAN");
        }
        else
        {
            strcpy(g_g4Status.WorkMode, WorkMode);
        }
    }

    return ret;
}

static void anj_4g_location_save(void)
{
    cJSON *pRoot = NULL;
    char *szData = NULL;
    char lac[32] = {0};
    char ci[32] = {0};

    if (g_g4Status.lastCellId == 0 || g_g4Status.lastLAC == 0)
    {
        return;
    }
    snprintf(lac, sizeof(lac), "%x", g_g4Status.lastLAC);
    snprintf(ci, sizeof(ci), "%x", g_g4Status.lastCellId);
    pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return;
    }
    cJSON_AddStringToObject(pRoot, "lac", lac);
    cJSON_AddStringToObject(pRoot, "ci", ci);
    cJSON_AddStringToObject(pRoot, "oper", g_g4Status.lastOper);
    if (g_g4Status.locationHasReset && g_g4Status.locationResetUser[0] != '\0')
    {
        cJSON_AddStringToObject(pRoot, "user", g_g4Status.locationResetUser);
    }
    szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    if (szData == NULL)
    {
        return;
    }
    anj_mw_write_file(LOCATION_FILE, 0, szData, (int)strlen(szData));
    anj_mw_free(szData);
}

static void anj_4g_location_load(void)
{
    char buf[1024] = {0};
    cJSON *pRoot = NULL;
    cJSON *pLac = NULL;
    cJSON *pCi = NULL;
    cJSON *pOper = NULL;
    cJSON *pUser = NULL;

    if (!anj_mw_file_exists(LOCATION_FILE))
    {
        return;
    }
    if (anj_mw_read_file_limit_len(LOCATION_FILE, buf, sizeof(buf) - 1) != 0)
    {
        return;
    }
    pRoot = cJSON_Parse(buf);
    if (pRoot == NULL)
    {
        __ERR("parse %s failed\n", LOCATION_FILE);
        return;
    }
    pLac = cJSON_GetObjectItem(pRoot, "lac");
    pCi = cJSON_GetObjectItem(pRoot, "ci");
    pOper = cJSON_GetObjectItem(pRoot, "oper");
    pUser = cJSON_GetObjectItem(pRoot, "user");
    if (pLac != NULL && cJSON_IsString(pLac) && pLac->valuestring != NULL
        && pCi != NULL && cJSON_IsString(pCi) && pCi->valuestring != NULL)
    {
        sscanf(pCi->valuestring, "%x", &g_g4Status.lastCellId);
        sscanf(pLac->valuestring, "%x", &g_g4Status.lastLAC);
        if (pOper != NULL && cJSON_IsString(pOper) && pOper->valuestring != NULL)
        {
            strncpy(g_g4Status.lastOper, pOper->valuestring, sizeof(g_g4Status.lastOper) - 1);
        }
        if (pUser != NULL && cJSON_IsString(pUser) && pUser->valuestring != NULL && pUser->valuestring[0] != '\0')
        {
            g_g4Status.locationHasReset = 1;
            strncpy(g_g4Status.locationResetUser, pUser->valuestring, sizeof(g_g4Status.locationResetUser) - 1);
        }
    }
    cJSON_Delete(pRoot);
}

static void anj_4g_location_set(EventResult *event_result, void *data)
{
    G4LocationSet *pstSet = (G4LocationSet *)data;

    (void)event_result;
    if (pstSet == NULL || stMobile4gParam.anj_4g_at_mutex == NULL)
    {
        return;
    }
    anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
    if (pstSet->action == G4_LOCATION_ACT_COMMIT)
    {
        if (g_g4Status.cellId != 0 && g_g4Status.LAC != 0)
        {
            g_g4Status.lastCellId = g_g4Status.cellId;
            g_g4Status.lastLAC = g_g4Status.LAC;
            strncpy(g_g4Status.lastOper, g_g4Status.Operator, sizeof(g_g4Status.lastOper) - 1);
            g_g4Status.locationNeedUpload = 0;
            anj_4g_location_save();
        }
    }
    else if (pstSet->action == G4_LOCATION_ACT_SAVE_RESET)
    {
        if (g_g4Status.lastCellId != 0 && g_g4Status.lastLAC != 0
            && pstSet->user[0] != '\0')
        {
            g_g4Status.locationHasReset = 1;
            strncpy(g_g4Status.locationResetUser, pstSet->user, sizeof(g_g4Status.locationResetUser) - 1);
            anj_4g_location_save();
        }
    }
    else if (pstSet->action == G4_LOCATION_ACT_CLEAR_RESET)
    {
        g_g4Status.locationHasReset = 0;
        g_g4Status.locationResetUser[0] = '\0';
        anj_4g_location_save();
    }
    anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
}

static int anj_4g_query_creg(char *recvBuf)
{
    int ret = -1;
    int n = 0, stat = 0;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    char tmpBuf[64] = {0};
    if (s_iManufacturer == ANJ_4G_MANUFACTURER_UM)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+CEREG:");
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+CGREG:");
    }
    else
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+CREG:");
    }
    char *p = strcasestr(recvBuf, tmpBuf);
    if (p != NULL)
    {
        p = p + strlen(tmpBuf);
        //+CREG: <n>,<stat>[,<lac>,<ci>[,<Act>]]
        if (sscanf(p, "%d,%d", &n, &stat) != 2)
        {
            __ERR("error: %s\n", recvBuf);
        }
        else
        {
            __INFO("%d, %d\n", n, stat);

            /*
            <stat>
            0 Not registered. ME is not currently searching a new operator to register to
            1 Registered, home network
            2 Not registered, but ME is currently searching a new operator to register to
            3 Registration denied
            4 Unknown
            5 Registered, roaming
            */
            switch (stat)
            {
            case 0:
            case 2:
                g_g4Status.nSvrStatus = G4_SERVICE_NO;
                break;

            case 3:
            case 4:
                g_g4Status.nSvrStatus = G4_SERVICE_LIMITED;
                break;

            case 1:
            case 5:
                g_g4Status.nSvrStatus = G4_SERVICE_AVAILABLE;
                break;
            default:
                g_g4Status.nSvrStatus = G4_SERVICE_NO;
                break;
            }

            if (stat == 1 || stat == 5)
            {
                char parseBuf[256] = {0};
                char lacHex[G4_STR_LEN_32] = {0};
                char ciHex[G4_STR_LEN_32] = {0};
                strncpy(parseBuf, p, sizeof(parseBuf) - 1);
                int iIndex = 0;
                char *token = strtok(parseBuf, ",");
                while (token != NULL)
                {
                    if (iIndex == 2 || iIndex == 3)
                    {
                        char cell[G4_STR_LEN_32] = {0};
                        int i = 0;
                        while (*token == ' ' || *token == '"')
                        {
                            token++;
                        }
                        while (token[i] != '\0' && isxdigit((unsigned char)token[i]) && i < (int)sizeof(cell) - 1)
                        {
                            cell[i] = token[i];
                            i++;
                        }
                        cell[i] = '\0';
                        if (iIndex == 2)
                        {
                            strncpy(lacHex, cell, sizeof(lacHex) - 1);
                        }
                        else
                        {
                            strncpy(ciHex, cell, sizeof(ciHex) - 1);
                        }
                    }
                    iIndex++;
                    token = strtok(NULL, ",");
                }

                g_g4Status.cellId = 0;
                g_g4Status.LAC = 0;
                g_g4Status.MCC = 0;
                g_g4Status.MNC = 0;
                if (strlen(lacHex) > 0 && strlen(ciHex) > 0)
                {
                    sscanf(lacHex, "%x", &g_g4Status.LAC);
                    sscanf(ciHex, "%x", &g_g4Status.cellId);
                    if (g_g4Status.LAC != 0 && g_g4Status.cellId != 0)
                    {
                        g_g4Status.MCC = 460;
                        g_g4Status.MNC = 1;
                        if (strcasestr(g_g4Status.Operator, "MOBILE") != NULL)
                        {
                            g_g4Status.MNC = 0;
                        }
                    }
                    else
                    {
                        g_g4Status.cellId = 0;
                        g_g4Status.LAC = 0;
                    }
                }
                __INFO("cellId=%d, LAC=%d, MCC=%d, MNC=%d\n",
                       g_g4Status.cellId, g_g4Status.LAC, g_g4Status.MCC, g_g4Status.MNC);
            }
            else
            {
                g_g4Status.cellId = 0;
                g_g4Status.LAC = 0;
                g_g4Status.MCC = 0;
                g_g4Status.MNC = 0;
            }
        }
    }
    if (g_g4Status.cellId != 0 && g_g4Status.LAC != 0)
    {
        g_g4Status.locationNeedUpload =
            (g_g4Status.cellId != g_g4Status.lastCellId || g_g4Status.LAC != g_g4Status.lastLAC) ? 1 : 0;
    }
    if (g_g4Status.nSvrStatus != G4_SERVICE_NO)
    {
        ret = 0;
    }
    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_query_iccid
 功能描述  : 查询iccid，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_query_iccid_list(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    if (s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL)
    {
        char iccid[IPC_4G_SIMCARD_NUM][G4_STR_LEN_32];
        memset(iccid, 0, sizeof(iccid));

        for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
        {
            char tmpBuf[64] = {0};
            snprintf(tmpBuf, sizeof(tmpBuf), "+QDSCCID: %d,", i);
            char *p = strstr(recvBuf, tmpBuf);
            if (p)
            {
                p = p + strlen(tmpBuf);
                char *p1 = strchr(p, '\n');
                if (p1)
                {
                    *p1 = '\0';
                }
                p1 = strchr(p, '\r');
                if (p1)
                {
                    *p1 = '\0';
                }

                if (stMobile4gParam.simInsert[i])
                {
                    strcpy(iccid[i], p);
                    if (i == 0)
                    {
                        strncpy(g_g4Status.IccidList, iccid[i], sizeof(g_g4Status.IccidList));
                        g_g4Status.IccidList[sizeof(g_g4Status.IccidList) - 1] = '\0';
                    }
                    else
                    {
                        strncat(g_g4Status.IccidList, ",", sizeof(g_g4Status.IccidList) - strlen(g_g4Status.IccidList) - 1);
                        strncat(g_g4Status.IccidList, iccid[i], sizeof(g_g4Status.IccidList) - strlen(g_g4Status.IccidList) - 1);
                    }
                }
            }
        }

        if (strlen(g_g4Status.ICCID) == 0)
        {
            for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
            {
                if (strlen(iccid[i]) > 0)
                {
                    strcpy(g_g4Status.ICCID, iccid[i]);
                    break;
                }
            }
        }
    }
    else
    {
        char iccid[64] = {0};
        char *head_iccid = "+ICCID:";
        if (s_iManufacturer == ANJ_4G_MANUFACTURER_CIS)
        {
            head_iccid = "+QCCID:";
        }
        else if ((s_iManufacturer == ANJ_4G_MANUFACTURER_UM))
        {
            head_iccid = "*ICCID:";
        }
        else if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
        {
            head_iccid = "+ECICCID:";
        }
        char *p = strcasestr(recvBuf, head_iccid);
        if (p != NULL)
        {
            p += strlen(head_iccid);

            char *pOK = strcasestr(p, "OK");
            if (NULL != pOK)
                *pOK = 0;

            string_trim_head(p);
            string_trim_tail(p);
            strncpy(iccid, p, G4_STR_LEN_32 - 1);

            string_remove(iccid, '"');

            strcpy(g_g4Status.ICCID, iccid);
            if (strlen(g_g4Status.IccidList) == 0)
            {
                strcpy(g_g4Status.IccidList, iccid);
                strcat(g_g4Status.IccidList, ",");
            }
            else
            {
                if (strstr(g_g4Status.IccidList, iccid) == NULL)
                {
                    strcat(g_g4Status.IccidList, iccid);
                }
            }
        }
    }

    return ret;
}

static int anj_4g_query_iccid(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    char *head_iccid = "+ICCID:";
    char iccid[64] = {0};
    char *p = strcasestr(recvBuf, head_iccid);
    if (p != NULL)
    {
        p += strlen(head_iccid);

        string_trim_head(p);
        string_trim_tail(p);
        strncpy(iccid, p, G4_STR_LEN_32 - 1);

        string_remove(iccid, '"');
        strcpy(g_g4Status.ICCID, iccid);
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_ncm_dial_on
 功能描述  : NCM拨号，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_ncm_dial_on(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);

    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_ncm_dial_off
 功能描述  : 断开NCM拨号，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_ncm_dial_off(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);

    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_ncm_dial_query
 功能描述  : 查询NCM拨号，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_ncm_dial_query(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    char tmpBuf[64] = {0};
    int nDialStatus = 0;
    if (s_iManufacturer == ANJ_4G_MANUFACTURER_YUGE)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+RNDISCALL:");
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+QNETDEVCTL:");
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+DIALMODE:");
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_UM)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "*DIALMODE:");
    }
    else if (s_iManufacturer == ANJ_4G_MANUFACTURER_CIS)
    {
        snprintf(tmpBuf, sizeof(tmpBuf), "+QIACT:");
    }
    char *p = strcasestr(recvBuf, tmpBuf);
    if (p != NULL)
    {
        p = p + strlen(tmpBuf);
        if (p)
        {
            if (s_iManufacturer == ANJ_4G_MANUFACTURER_YUGE)
            {
                if (sscanf(p, "%d", &nDialStatus) != 1)
                {
                    __ERR("error: %s\n", recvBuf);
                }
                else
                {
                    g_g4Status.nDialStatus = nDialStatus;
                }
            }
            else if (s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL)
            {
                int op, cid, urc_en, state;
                if (sscanf(p, "%d,%d,%d,%d", &op, &cid, &urc_en, &state) == 4)
                {
                    __ERR("%d,%d,%d,%d\n", op, cid, urc_en, state);
                    nDialStatus = state;
                    g_g4Status.nDialStatus = nDialStatus;
                }
            }
            else if ((s_iManufacturer == ANJ_4G_MANUFACTURER_SIMCOM) ||
                     (s_iManufacturer == ANJ_4G_MANUFACTURER_UM))
            {
                if (sscanf(p, "%d", &nDialStatus) != 1)
                {
                    __ERR("error: %s\n", recvBuf);
                }
                else
                {
                    g_g4Status.nDialStatus = !nDialStatus;
                }
            }
            else if (s_iManufacturer == ANJ_4G_MANUFACTURER_CIS)
            {
                int iIndex = 0;
                int cstatus = 0;
                const char s[4] = ",";
                char *token = NULL;
                token = strtok(p, s);
                while (token != NULL)
                {
                    if (iIndex == 1)
                    {
                        cstatus = atoi(token);
                        break;
                    }

                    iIndex++;
                    token = strtok(NULL, s);
                }

                if (cstatus == 1)
                {
                    g_g4Status.nDialStatus = 1;
                }
                else
                {
                    g_g4Status.nDialStatus = 0;
                }
            }
        }
    }
    __INFO("nDialStatus:%d\n", g_g4Status.nDialStatus);
    return ret;
}

static int anj_4g_set_cgdcont(char *recvBuf)
{
    return 0;
    int ret = -1;

    __INFO("recvBuf = %s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        ret = 0;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_query_signal
 功能描述  : 查询信号强度，返回数据处理
 输入参数  : recvBuf 分割后的命令，group 分割组数，anj_4g_seq_type 命令类型
 输出参数  : NULL
 返 回 值  : 成功 0，失败 -1
*****************************************************************************/
static int anj_4g_query_imsi_list(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    if (s_iManufacturer == ANJ_4G_MANUFACTURER_QUECTEL)
    {
        char imi[IPC_4G_SIMCARD_NUM][G4_STR_LEN_32];
        memset(imi, 0, sizeof(imi));

        for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
        {
            char tmpBuf[64] = {0};
            snprintf(tmpBuf, sizeof(tmpBuf), "+QDSIMI: %d,", i);
            char *p = strstr(recvBuf, tmpBuf);
            if (p)
            {
                p = p + strlen(tmpBuf);
                char *p1 = strchr(p, '\n');
                if (p1)
                {
                    *p1 = '\0';
                }
                p1 = strchr(p, '\r');
                if (p1)
                {
                    *p1 = '\0';
                }

                if (stMobile4gParam.simInsert[i])
                {
                    strcpy(imi[i], p);
                    if (i == 0)
                    {
                        strncpy(g_g4Status.ImsiList, imi[i], sizeof(g_g4Status.ImsiList));
                        g_g4Status.ImsiList[sizeof(g_g4Status.ImsiList) - 1] = '\0';
                    }
                    else
                    {
                        strncat(g_g4Status.ImsiList, ",", sizeof(g_g4Status.ImsiList) - strlen(g_g4Status.ImsiList) - 1);
                        strncat(g_g4Status.ImsiList, imi[i], sizeof(g_g4Status.ImsiList) - strlen(g_g4Status.ImsiList) - 1);
                    }
                }
            }
        }

        if (strlen(g_g4Status.IMSI) == 0)
        {
            for (int i = 0; i < IPC_4G_SIMCARD_NUM; i++)
            {
                if (strlen(imi[i]) > 0)
                {
                    strcpy(g_g4Status.IMSI, imi[i]);
                    break;
                }
            }
        }
    }
    else
    {
        char imsi[64] = {0};
        char *head_imsi = "";
        char *p = recvBuf;
        if ((s_iManufacturer != ANJ_4G_MANUFACTURER_CIS) && (s_iManufacturer != ANJ_4G_MANUFACTURER_UM))
        {
            head_imsi = "AT+CIMI";
        }
        if (p)
        {
            p += strlen(head_imsi);
            char *pOK = strcasestr(p, "OK");
            if (NULL != pOK)
                *pOK = 0;

            string_trim_head(p);
            string_trim_tail(p);
            strncpy(imsi, p, G4_STR_LEN_32 - 1);
            string_remove(imsi, '"');

            strcpy(g_g4Status.IMSI, imsi);
            if (strlen(g_g4Status.ImsiList) == 0)
            {
                strcpy(g_g4Status.ImsiList, imsi);
                strcat(g_g4Status.ImsiList, ",");
            }
            else
            {
                if (strstr(g_g4Status.ImsiList, imsi) == NULL)
                {
                    strcat(g_g4Status.ImsiList, imsi);
                }
            }
        }
    }

    return ret;
}

static int anj_4g_query_imsi(char *recvBuf)
{
    int ret = -1;
    __INFO("%s\n", recvBuf);
    if (strcasestr(recvBuf, "OK"))
    {
        __INFO("success\n");
        ret = 0;
    }
    else
    {
        __ERR("failed\n");
        return ret;
    }

    char *p = recvBuf;
    if (p != NULL)
    {

        char *pOK = strcasestr(p, "OK");
        if (NULL != pOK)
        {
            *pOK = 0;
            string_trim_head(p);
            string_trim_tail(p);
            strncpy(g_g4Status.IMSI, p, G4_STR_LEN_32 - 1);

            if (strlen(g_g4Status.ImsiList) == 0)
            {
                strcpy(g_g4Status.ImsiList, g_g4Status.IMSI);
                g_g4Status.ImsiList[sizeof(g_g4Status.ImsiList) - 1] = '\0';
                strncat(g_g4Status.ImsiList, ",", sizeof(g_g4Status.ImsiList) - strlen(g_g4Status.ImsiList) - 1);
            }
            else
            {
                strncat(g_g4Status.ImsiList, p, sizeof(g_g4Status.ImsiList) - strlen(g_g4Status.ImsiList) - 1);
            }
        }
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_4g_airmode_switch
 功能描述  : at模块飞行模式关开
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static void anj_4g_airmode_switch()
{
    __INFO("anj_4g_airmode_switch\n");
    anj_4g_send_and_recv(s_pstAtCmd, AT_SEQ_CFUN_SET_OFF, 1000);
    anj_4g_send_and_recv(s_pstAtCmd, AT_SEQ_CFUN_SET_ON, 1000);
}

static void anj_4g_dstype_set()
{
    anj_4g_send_and_recv(s_pstAtCmd, AT_SEQ_SET_DSTYPE, 1000);
}

/*****************************************************************************
 函 数 名  : anj_4g_module_check
 功能描述  : 4g模块通信前检查
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : AT模块正常、AT模块异常、串口异常
*****************************************************************************/
static MOBILE4G_STATUS anj_4g_module_check()
{
    int ret = -1;

    s_iManufacturer = anj_4g_get_manufacturer();
    if (s_iManufacturer <= 0)
    {
        s_mErrorIndex = MOBILE4G_ERROR_UART_1;
        __ERR("Unsupported 4g module!\n");
        return MOBILE4G_NO_4G;
    }

    anj_4g_add_capability(s_iManufacturer);

    ret = anj_4g_comm_init(s_iManufacturer);
    if (ret < 0)
    {
        s_mErrorIndex = MOBILE4G_ERROR_UART_1;
        return MOBILE4G_STATUS_UART_ERROR;
    }

    /* 4G模块重启后10秒左右暂时无法通信，需要多次尝试 */
    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_ATE0_SET, 1000, 60, NULL);
    if (ret < 0)
    {
        __ERR("AT_CMD_CLOSE_ECHO try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_1;
        return MOBILE4G_STATUS_AT_ERROR;
    }

    return MOBILE4G_STATUS_AT_CORRECT;
}

/*****************************************************************************
 函 数 名  : anj_4g_module_hardware_restart
 功能描述  : 4g模块硬件断电重启
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static void anj_4g_module_hardware_restart()
{
    anj_mw_hwctrl_usb_status(!IPC_USB_GPIO_DEFAULT_VALUE);
    usleep(100 * 1000);
    anj_mw_hwctrl_usb_status(IPC_USB_GPIO_DEFAULT_VALUE);
}

static void anj_4g_sim_error_change()
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    G4Config *pst4gConfig = &pstNetworkConfig->g4Cfg;

    // 自动模式 A卡不通切B卡
    anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
    g_g4Status.CurOperator = !g_g4Status.CurOperator;
    stMobile4gParam.simChange = 1;
    if (pst4gConfig->is_manual == 0)
    {
        anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_CONFIG_INVALID, 2);
    }
    else
    {
        // 手动模式网络不通 切自动模式
        if (pst4gConfig->cur_operator == 0)
        {
            anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_TELECOM_ERROR, 2);
        }
        else
        {
            anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_MOBILE_ERROR, 2);
        }
        pst4gConfig->is_manual = 0;
        pst4gConfig->cur_operator = 0;
        anj_config_network_save(pstNetworkConfig);
    }

    anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);

    __INFO("switch to other sim\n");
}

/*****************************************************************************
 函 数 名  : anj_4g_module_restart
 功能描述  : 4g模块重启
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static void anj_4g_module_restart(MOBILE4G_STATUS status)
{
    /* 断开拨号 */
    anj_4g_try_send(s_pstAtCmd, AT_SEQ_NDISDUP_SET_OFF, 5000, 1, NULL);

    /* 关闭网卡 */
    if (is_network_connect(WIRE_INTERFACE_NAME1) == 1)
    {
        net_del_ip(WIRE_INTERFACE_NAME1);
        net_set_down(WIRE_INTERFACE_NAME1);
        usleep(50 * 1000);
    }

    /* AT模块复位 */
    anj_4g_module_hardware_restart();
    anj_4g_try_send(s_pstAtCmd, AT_SEQ_RESET_SET, 1000, 1, NULL);
    anj_4g_comm_uninit();

    usleep(500 * 1000);
    __INFO("anj_4g_module_restart done (%d) (%d) iccid:%s imei:%s connect:%d oper:%s signal:%d dia:%d msisdn:%s sim:%d\n",
           status, s_mErrorIndex,
           g_g4Status.ICCID, g_g4Status.IMEI, stMobile4gParam.stAtInfo.is_connect_internet,
           g_g4Status.Operator, g_g4Status.nSignalLevel, g_g4Status.nDialStatus,
           g_g4Status.MSISDN, g_g4Status.CurOperator);

    /* 清除信息 */
    memset(&stMobile4gParam.stAtInfo, 0, sizeof(stMobile4gParam.stAtInfo));
    stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_PAUSE;
    stMobile4gParam.anj_4g_at_usbnet_done_flag = 0;
    stMobile4gParam.anj_4g_at_init_done_flag = 0;
    stMobile4gParam.first_ping_flag = 0;
    memset(stMobile4gParam.anj_4g_at_failed_cnt, 0, sizeof(stMobile4gParam.anj_4g_at_failed_cnt));
}

/*****************************************************************************
 函 数 名  : anj_4g_usbnet_pause
 功能描述  : 关闭usb网卡
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static void anj_4g_usbnet_pause()
{
    /* 下次继续运行时重新拨号 */
    stMobile4gParam.anj_4g_at_usbnet_done_flag = 0;
    stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_PAUSE;

    net_del_ip(WIRE_INTERFACE_NAME1);
    net_set_down(WIRE_INTERFACE_NAME1);

    usleep(50 * 1000);
}

/*****************************************************************************
 函 数 名  : anj_4g_module_error_handle
 功能描述  : AT模块错误处理
 输入参数  : status 4G模块状态，err_cnt 错误重启次数
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static int anj_4g_module_error_handle(MOBILE4G_STATUS status, int err_cnt, int *CurOperator)
{
    if (stMobile4gParam.debug_flag)
    {
        __INFO("at module error (%d)\n", status);
    }

    if (stMobile4gParam.anj_4g_at_failed_cnt[status] > err_cnt && err_cnt >= 0)
    {
        if (status == MOBILE4G_STATUS_SIM_ERROR || status == MOBILE4G_STATUS_INFO_ERROR)
        {
            // SIM卡错误
            if (*CurOperator == 0)
            {
                anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_SIM1_ERROR, 2);
            }
            else
            {
                anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_SIM2_ERROR, 2);
            }
        }

        anj_4g_osd_show(0);
        anj_4g_module_restart(status);
        *CurOperator = !*CurOperator;
        return 0;
    }
    // 25s后 每5s播一次 最多播5次
    if ((stMobile4gParam.anj_4g_at_failed_cnt[MOBILE4G_NO_4G] > MOBILE4G_NO_4G_START_CNT) &&
        ((stMobile4gParam.anj_4g_at_failed_cnt[MOBILE4G_NO_4G] % MOBILE4G_NO_4G_INTERVAL_CNT) == 0) &&
        (stMobile4gAudioParam.play_no_4g_cnt > 0))
    {
        anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_NO_4G, 1);
        stMobile4gAudioParam.play_no_4g_cnt--;
    }
    stMobile4gParam.anj_4g_at_failed_cnt[status]++;
    if (stMobile4gParam.debug_flag)
    {
        __INFO("anj_4g_at_failed_cnt (%d)\n", stMobile4gParam.anj_4g_at_failed_cnt[status]);
    }

    return 0;
}

/*****************************************************************************
 函 数 名  : anj_4g_usbnet_error_handle
 功能描述  : 拨号错误处理
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
static int anj_4g_usbnet_error_handle(MOBILE4G_STATUS status, int err_cnt, int *CurOperator)
{
    if (stMobile4gParam.debug_flag)
    {
        __INFO("at usbnet error (%d)\n", status);
    }

    stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_PAUSE;
    if (stMobile4gParam.anj_4g_at_failed_cnt[status] > err_cnt)
    {
        anj_4g_module_restart(status);
        anj_4g_sim_error_change();
        return 0;
    }
    anj_4g_airmode_switch();
    stMobile4gParam.anj_4g_at_init_done_flag = 0;
    stMobile4gParam.anj_4g_at_failed_cnt[status]++;

    return 0;
}

/*****************************************************************************
 函 数 名  : anj_4g_module_start
 功能描述  : 查询4g模块工作状态，确认注册信息
 输入参数  : pause_flag 暂停标志
 输出参数  : NULL
 返 回 值  : AT模块正常、AT模块异常、SIM卡异常、SIM卡信息异常
*****************************************************************************/
static MOBILE4G_STATUS anj_4g_module_start(int CurOperator)
{
    int ret = -1;
    MOBILE4G_STATUS status = MOBILE4G_STATUS_AT_CORRECT;
    AT_SEQ stSetApn = AT_CMD_MAX_NUMS;
    status = anj_4g_module_check();
    if (status != MOBILE4G_STATUS_AT_CORRECT)
    {
        return status;
    }

    if (IPC_4G_USE_USB0)
    {
        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_USB_NUM, 1000, 2, NULL);
        if (ret == 0)
        {
            __WARN("usbEnumOrder!\n");
        }
    }

    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_MODULE_INFO, 1000, 2, NULL);
    if (ret != 0)
    {
        __ERR("AT_SEQ_QUERY_MODULE_INFO try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_2;
        return MOBILE4G_STATUS_AT_ERROR;
    }
    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_MODULE_INFO1, 1000, 2, NULL);
    if (ret != 0)
    {
        __ERR("AT_SEQ_QUERY_MODULE_INFO1 try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_2;
        return MOBILE4G_STATUS_AT_ERROR;
    }
    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
    {
        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_MODULE_INFO2, 1000, 2, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_QUERY_MODULE_INFO2 try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_AT_2;
            return MOBILE4G_STATUS_AT_ERROR;
        }
    }

    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_START_HOTPLUG, 1000, 2, NULL);
    if (ret != 0)
    {
        __ERR("AT_SEQ_START_HOTPLUG try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_2;
        return MOBILE4G_STATUS_AT_ERROR;
    }

    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_STOP_HOTPLUG, 1000, 2, NULL);
    if (ret != 0)
    {
        __ERR("AT_SEQ_STOP_HOTPLUG try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_2;
        return MOBILE4G_STATUS_AT_ERROR;
    }

    ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_CFUN_QUERY, 1000, 2, anj_4g_airmode_switch);
    if (ret != 0)
    {
        __ERR("AT_SEQ_CFUN_QUERY try out\n");
        s_mErrorIndex = MOBILE4G_ERROR_AT_2;
        return MOBILE4G_STATUS_AT_ERROR;
    }

    for (int i = 0; i < AT_MODULE_FAILED_CNT; i++)
    {
        usleep(500 * 1000);
        if (IPC_4G_SIMCARD_NUM > 1)
        {
            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_DSPIN, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_QUERY_DSPIN try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }

            if (CurOperator == 0)
            {
                stSetApn = AT_SEQ_SEL_SIM0;
            }
            else
            {
                stSetApn = AT_SEQ_SEL_SIM1;
            }
            ret = anj_4g_try_send(s_pstAtCmd, stSetApn, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_SEL_SIM%d try out\n", CurOperator);
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }

            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_CUR_SIM, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_QUERY_SIM try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }

            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_DSTYPE, 1000, 1, anj_4g_dstype_set);
            if (ret != 0)
            {
                __ERR("AT_SEQ_QUERY_DSTYPE try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }

            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QDSICCID_QUERY, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_QDSICCID_QUERY try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }

            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_GET_QDSIMSI, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_GET_QDSIMSI try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_SIM;
                status = MOBILE4G_STATUS_SIM_ERROR;
                continue;
            }
            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_COPS_QUERY, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SEQ_COPS_QUERY try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_INFO_2;
                status = MOBILE4G_STATUS_INFO_ERROR;
                continue;
            }
        }

        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_CPIN_QUERY, 1000, 60, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_CPIN_QUERY try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_SIM;
            status = MOBILE4G_STATUS_SIM_ERROR;
            break;
        }

        anj_4g_try_send(s_pstAtCmd, AT_SEQ_CREG_SET, 1000, 1, NULL);
        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_CREG_QUERY, 1000, 1, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_CREG_QUERY try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_1;
            status = MOBILE4G_STATUS_INFO_ERROR;
            continue;
        }

        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_HCSQ_QUERY, 1000, 60, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_HCSQ_QUERY try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_3;
            status = MOBILE4G_STATUS_INFO_ERROR;
            break;
        }

        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_ICCID_QUERY, 1000, 1, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_QDSICCID_QUERY try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_4;
            status = MOBILE4G_STATUS_INFO_ERROR;
            continue;
        }

        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_GET_IMSI, 1000, 1, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_GET_QDSIMSI try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_5;
            status = MOBILE4G_STATUS_INFO_ERROR;
            continue;
        }

        stSetApn = AT_CMD_MAX_NUMS;
        if (strcasestr(g_g4Status.IMSI, "46011"))
        {
            stSetApn = AT_SEQ_CGDCONT_CHN_CT;
        }
        else if (strcasestr(g_g4Status.IMSI, "46001") || strcasestr(g_g4Status.IMSI, "46006"))
        {
            stSetApn = AT_SEQ_CGDCONT_CHN_UNICOM;
        }
        else if (strcasestr(g_g4Status.IMSI, "46000") ||
                 strcasestr(g_g4Status.IMSI, "46002") ||
                 strcasestr(g_g4Status.IMSI, "46007") ||
                 strcasestr(g_g4Status.IMSI, "46004") ||
                 strcasestr(g_g4Status.IMSI, "46008") ||
                 strcasestr(g_g4Status.IMSI, "46009") ||
                 strcasestr(g_g4Status.IMSI, "46024") ||
                 strcasestr(g_g4Status.IMSI, "86827") ||
                 strcasestr(g_g4Status.IMSI, "46010"))
        {
            stSetApn = AT_SEQ_CGDCONT_CHN_MOBILE;
        }
        else
        {
            __ERR("AT_SEQ_GET_QDSIMSI try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_5;
            status = MOBILE4G_STATUS_INFO_ERROR;
            continue;
        }

        if (stSetApn != AT_CMD_MAX_NUMS)
        {
            ret = anj_4g_try_send(s_pstAtCmd, stSetApn, 1000, 1, NULL);
            if (ret != 0)
            {
                __ERR("AT_SET_CGDCONT_APN try out\n");
                s_mErrorIndex = MOBILE4G_ERROR_INFO_5;
                status = MOBILE4G_STATUS_INFO_ERROR;
                continue;
            }
        }

        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_QUERY_SYSINFO, 1000, 1, NULL);
        if (ret != 0)
        {
            __ERR("AT_SEQ_QUERY_SYSINFO try out\n");
            s_mErrorIndex = MOBILE4G_ERROR_INFO_6;
            status = MOBILE4G_STATUS_INFO_ERROR;
            continue;
        }

        if (ret == 0)
        {
            status = MOBILE4G_STATUS_AT_CORRECT;
            break;
        }
    }

    return status;
}

/*****************************************************************************
 函 数 名  : anj_4g_usbnet_start
 功能描述  : 4g模块开始拨号，测试网络连接
 输入参数  : pause_flag 暂停标志
 输出参数  : NULL
 返 回 值  : AT模块异常、网络异常、网络正常
*****************************************************************************/
static MOBILE4G_STATUS anj_4g_usbnet_start(THREAD_RUN_STATUS *pause_flag)
{
    int ret = -1;
    if (pause_flag == NULL)
    {
        return ret;
    }

    MOBILE4G_STATUS status = MOBILE4G_STATUS_NET_ERROR;

    status = anj_4g_module_check();
    if (status != MOBILE4G_STATUS_AT_CORRECT)
    {
        return status;
    }

    net_set_down(WIRE_INTERFACE_NAME1);
    usleep(50 * 1000);

    net_del_ip(WIRE_INTERFACE_NAME1);
    usleep(50 * 1000);

    for (int i = 0; i < NCM_DIAL_FAILED_CNT; i++)
    {
        if (*pause_flag != THREAD_STATUS_RUNNING)
        {
            return -1;
        }
        /* 若已拨号，再次发送拨号会超时 */
        ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_NDISDUP_SET_ON, 5000, 1, NULL);
        if (ret == 0)
        {
            anj_net_dhcp_up(WIRE_INTERFACE_NAME1);
            usleep(50 * 1000);

            net_set_mtu(WIRE_INTERFACE_NAME1, 1400);
            usleep(50 * 1000);

            // 查询数据连接
            ret = anj_4g_try_send(s_pstAtCmd, AT_SEQ_NDISDUP_QUERY, 1000, 1, NULL);
            if (ret == 0)
            {
                __INFO("AT_SEQ_NDISDUP_QUERY success\n");
                status = MOBILE4G_STATUS_NET_CORRECT;
                break;
            }
        }

        if (ret < 0)
        {
            anj_4g_try_send(s_pstAtCmd, AT_SEQ_NDISDUP_SET_OFF, 5000, 1, NULL);
            continue;
        }
    }

    if (ret < 0)
    {
        net_set_down(WIRE_INTERFACE_NAME1);
        usleep(50 * 1000);
    }

    return status;
}

static int anj_4g_thread(void *ctx, int *bStart)
{
    int ret = -1;
    MOBILE4G_STATUS status = MOBILE4G_STATUS_AT_ERROR;
    int CurOperator = !g_g4Status.CurOperator;
    const int update_info_cnt = UPDATE_INFO_MS / (THREAD_DELAY_US / 1000);
    int update_info_cur = 0;

    __INFO("anj_4g_thread enter\n");

    sleep(5);

    while (bStart && *bStart)
    {
        if (anj_mw_file_exists(AJ_APP_PATH "/flag.usb.4g")) // 退出线程
        {
            return 0;
        }

        /* 异步重启请求由事件线程设置，在线程上下文执行实际重启，避免事件回调阻塞 */
        if (stMobile4gParam.need_module_restart)
        {
            anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
            stMobile4gParam.need_module_restart = 0;
            anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);

            __INFO("anj_4g_thread: performing async module restart requested\n");
            anj_ser_reset_conn(ANJ_NET_STATUS_4G);
            anj_4g_module_restart(MOBILE4G_STATUS_MAX);

            /* 确保后续循环会重新初始化 AT 子系统 */
            stMobile4gParam.anj_4g_at_init_done_flag = 0;
            stMobile4gParam.anj_4g_at_usbnet_done_flag = 0;

            /* 继续下一次循环，模块会在初始化分支被重新启动 */
            continue;
        }

        /* AT模块初始化 */
        if (stMobile4gParam.anj_4g_at_init_done_flag == 0)
        {
            anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
            if (stMobile4gParam.simChange)
            {
                stMobile4gParam.simChange = 0;
                CurOperator = g_g4Status.CurOperator;
            }
            anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
            status = anj_4g_module_start(CurOperator);
            if (status == MOBILE4G_NO_4G)
            {
                anj_4g_module_error_handle(status, AT_MODULE_UART_FAILED_CNT, &CurOperator);
                sleep(1);
                continue;
            }
            else if (status == MOBILE4G_STATUS_UART_ERROR)
            {
                // 串口错误
                anj_4g_module_error_handle(status, AT_MODULE_UART_FAILED_CNT, &CurOperator);
                sleep(1);
                continue;
            }
            else if (status == MOBILE4G_STATUS_AT_ERROR)
            {
                // AT模块错误
                anj_4g_module_error_handle(status, AT_MODULE_FAILED_CNT, &CurOperator);
                sleep(1);
                continue;
            }
            else if (status == MOBILE4G_STATUS_SIM_ERROR)
            {
                anj_4g_module_error_handle(status, AT_MODULE_SIM_FAILED_CNT, &CurOperator);
                sleep(1);
                continue;
            }
            else if (status == MOBILE4G_STATUS_INFO_ERROR)
            {
                anj_4g_module_error_handle(status, AT_MODULE_SIM_INFO_FAILED_CNT, &CurOperator);
                sleep(1);
                continue;
            }
            else if (status == MOBILE4G_STATUS_AT_CORRECT)
            {
                // 所有sim卡的信息都要先获取一遍,自动模式优先使用sim0
                if (CurOperator != g_g4Status.CurOperator)
                {
                    anj_4g_module_restart(MOBILE4G_STATUS_MAX);
                    CurOperator = g_g4Status.CurOperator;
                    continue;
                }
                else
                {
                    stMobile4gParam.anj_4g_at_init_done_flag = 1;
                    stMobile4gParam.anj_4g_at_usbnet_done_flag = 0;
                    memset(stMobile4gParam.anj_4g_at_failed_cnt, 0, sizeof(stMobile4gParam.anj_4g_at_failed_cnt));
                    // 上报服务器4G信息
                    anj_ser_reponse(SER_RESPONSE_4G_INIT_DONE, NULL);
                    __INFO("AT module init done\n");
                }
            }
        }

        anj_4g_osd_show(stMobile4gParam.osd_flag);
        /* 更新4G相关信息 */
        /* 减少锁持有时间：先复制标志位到本地副本 */
        int flags[AT_CMD_MAX_NUMS];
        if (stMobile4gParam.anj_4g_at_mutex)
        {
            anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
            memcpy(flags, stMobile4gParam.stAtInfo.update_flag, sizeof(flags));
            anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
        }
        else
        {
            memset(flags, 0, sizeof(flags));
        }
        if (update_info_cur % update_info_cnt == 0 && stMobile4gParam.anj_4g_at_init_done_flag)
        {
            update_info_cur = 0;
            for (AT_SEQ i = 0; i < AT_CMD_MAX_NUMS; i++)
            {
                /* ICCID/CREG每1分钟获取一次，其它信息需要时再获取 */
                if (flags[i] || i == AT_SEQ_ICCID_QUERY || i == AT_SEQ_CREG_QUERY)
                {
                    __INFO("cmd %d update\n", i);
                    ret = anj_4g_try_send(s_pstAtCmd, i, 1000, 1, NULL);
                    anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
                    if (ret == 0)
                    {
                        stMobile4gParam.stAtInfo.update_flag[i] = 0;
                        stMobile4gParam.anj_4g_at_failed_cnt[MOBILE4G_STATUS_COMM_ERROR] = 0;
                    }
                    else
                    {
                        stMobile4gParam.anj_4g_at_failed_cnt[MOBILE4G_STATUS_COMM_ERROR]++;
                        __INFO("cmd %d update failed\n", i);
                    }
                    anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
                }
            }
            /* AT模块连续通信失败后，重启AT模块 */
            if (stMobile4gParam.anj_4g_at_failed_cnt[MOBILE4G_STATUS_COMM_ERROR] > AT_MODULE_FAILED_CNT)
            {
                anj_4g_module_restart(MOBILE4G_STATUS_COMM_ERROR);
                CurOperator = !CurOperator;
            }
        }
        update_info_cur++;

        if (access("/tmp/atrun", F_OK) == 0)
        {
            remove("/tmp/atrun");
            stMobile4gParam.thread_at_pause_flag = THREAD_STATUS_RUNNING;
        }
        /* AT线程暂停代表接入网线，断开usb网卡，但保持AT通信 */
        if (stMobile4gParam.thread_at_pause_flag == THREAD_STATUS_WAIT)
        {
            __INFO("at thread start pause\n");
            anj_4g_usbnet_pause();
            stMobile4gParam.first_ping_flag = 0; // 切换网卡时强制ping一次外网
            stMobile4gParam.play_net_connectd_flag = 0;
            anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
            stMobile4gParam.thread_at_pause_flag = THREAD_STATUS_PAUSE;
            anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
        }

        if (stMobile4gParam.thread_at_pause_flag == THREAD_STATUS_PAUSE)
        {
            usleep(THREAD_DELAY_US);
            continue;
        }

        // 拨号，成功后开始心跳
        if (stMobile4gParam.anj_4g_at_usbnet_done_flag == 0 && stMobile4gParam.anj_4g_at_init_done_flag)
        {
            status = anj_4g_usbnet_start(&stMobile4gParam.thread_at_pause_flag);
            if (status == MOBILE4G_STATUS_NET_ERROR)
            {
                // 拨号或网络错误
                s_mErrorIndex = MOBILE4G_ERROR_NET;
                anj_4g_usbnet_error_handle(status, USBNET_FAILED_CNT, &CurOperator);
                continue;
            }
            else if (status == MOBILE4G_STATUS_AT_ERROR)
            {
                s_mErrorIndex = MOBILE4G_ERROR_AT_3;
                anj_4g_usbnet_error_handle(status, AT_MODULE_FAILED_CNT, &CurOperator);
                continue;
            }
            else if (status == MOBILE4G_STATUS_UART_ERROR)
            {
                s_mErrorIndex = MOBILE4G_ERROR_UART_2;
                anj_4g_usbnet_error_handle(status, AT_MODULE_UART_FAILED_CNT, &CurOperator);
                continue;
            }
            else if (status == MOBILE4G_STATUS_NET_CORRECT)
            {
                stMobile4gParam.anj_4g_at_usbnet_done_flag = 1;
                memset(stMobile4gParam.anj_4g_at_failed_cnt, 0, sizeof(stMobile4gParam.anj_4g_at_failed_cnt));
                stMobile4gParam.play_net_connectd_flag = 0;
                stMobile4gParam.first_ping_flag = 0; // 拨号成功强制ping一次外网
                stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_RUNNING;

                /*双网卡切换，需要对默认网关进行一次切换 否则p2p无法出图*/
                net_set_down(WIRE_INTERFACE_NAME);
                net_set_up(WIRE_INTERFACE_NAME);

                anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_CONNECTING_NET, 1);
                stMobile4gAudioParam.play_connecting_net_cnt--;
                __INFO("usbnet init done\n");
            }
        }

        usleep(THREAD_DELAY_US);
    }

    anj_4g_module_restart(MOBILE4G_STATUS_MAX);
    if (stMobile4gParam.anj_4g_at_comm_mutex != NULL)
    {
        anj_mutex_destroy(stMobile4gParam.anj_4g_at_comm_mutex);
        free(stMobile4gParam.anj_4g_at_comm_mutex);

        stMobile4gParam.anj_4g_at_comm_mutex = NULL;
    }

    if (stMobile4gParam.anj_4g_at_mutex != NULL)
    {
        anj_mutex_destroy(stMobile4gParam.anj_4g_at_mutex);
        free(stMobile4gParam.anj_4g_at_mutex);
        stMobile4gParam.anj_4g_at_mutex = NULL;
    }

    __INFO("anj_4g_thread exit\n");
    return 0;
}

static int anj_4g_heartbeat_thread(void *ctx, int *bStart)
{
    int ret = -1;
    const int ping_cnt = 3;          /* ping尝试次数 */
    const int ping_timeout_ms = 500; /* 每次ping超时时间/ms */
    int heart_beanj_4g_at_cnt = HEART_BEAT_CNT;
    int heart_beanj_4g_at_cur = 0;
    int internet_failed_cnt = 0;
    anj_ser_info *pstSerInfo = getSerInfo();

    __INFO("anj_4g_heartbeat_thread entry\n");

    while (bStart && *bStart)
    {
        if (stMobile4gParam.heartbeat_pause_flag != THREAD_STATUS_RUNNING)
        {
            heart_beanj_4g_at_cur = 0;
            internet_failed_cnt = 0;
            usleep(THREAD_DELAY_US);
            continue;
        }

        if (heart_beanj_4g_at_cur % heart_beanj_4g_at_cnt == 0)
        {
            heart_beanj_4g_at_cur = 0;
            if (pstSerInfo->stP2pLoginState.logined == 0 || stMobile4gParam.first_ping_flag == 0)
            {
                ret = try_ping("8.8.8.8", ping_timeout_ms, ping_cnt, WIRE_INTERFACE_NAME1, bStart);
                if (ret < 0)
                {
                    ret = try_ping("114.114.114.114", ping_timeout_ms, ping_cnt, WIRE_INTERFACE_NAME1, bStart);
                }

                if (ret < 0)
                {
                    internet_failed_cnt++;
                    stMobile4gParam.stAtInfo.is_connect_internet = 0;
                    heart_beanj_4g_at_cnt = HEART_BEAT_CNT / 6;
                    __ERR("ping internet failed %d\n", internet_failed_cnt);
                }
                else
                {
                    internet_failed_cnt = 0;
                    stMobile4gParam.first_ping_flag = 1;
                    stMobile4gParam.stAtInfo.is_connect_internet = 1;
                    heart_beanj_4g_at_cnt = HEART_BEAT_CNT;
                    __INFO("###4G network heart beat success\n");
                }
            }
            else if (pstSerInfo->stP2pLoginState.logined)
            {
                internet_failed_cnt = 0;
                stMobile4gParam.stAtInfo.is_connect_internet = 1;
            }

            /* 音频：网络连接正常 */
            if (stMobile4gParam.play_net_connectd_flag == 0 && stMobile4gParam.stAtInfo.is_connect_internet)
            {
                anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONNECT_NET_SUCCESS, 1);
                sleep(1);
                stMobile4gParam.play_net_connectd_flag = 1;
                if (g_g4Status.nSignalLevel < 50)
                {
                    anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_SIGNAL_WEAK, 1);
                }
                else
                {
                    char filename[64] = {0};
                    anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_SIGNAL_STRENGTH, 1);
                    snprintf(filename, sizeof(filename), ANJ_MP3_PERCENT, ANJ_ALIGN_DOWN(g_g4Status.nSignalLevel, 10));
                    anj_audio_prompt_play(ANJ_MP3_4G_PATH, filename, 1);
                }
                sleep(1);
            }

            /* 网络不通，AT重新初始化 */
            if (internet_failed_cnt > HEART_BEAT_FAILED_CNT && stMobile4gParam.heartbeat_pause_flag == THREAD_STATUS_RUNNING)
            {
                stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_PAUSE;
                stMobile4gParam.anj_4g_at_init_done_flag = 0;
                stMobile4gParam.first_ping_flag = 0;
                anj_4g_sim_error_change();
                __INFO("restart usb net (%d)  iccid:%s imei:%s connect:%d oper:%s signal:%d dia:%d msisdn:%s sim:%d\n",
                       internet_failed_cnt,
                       g_g4Status.ICCID, g_g4Status.IMEI, stMobile4gParam.stAtInfo.is_connect_internet,
                       g_g4Status.Operator, g_g4Status.nSignalLevel, g_g4Status.nDialStatus,
                       g_g4Status.MSISDN, g_g4Status.CurOperator);
                continue;
            }
        }

        if (stMobile4gParam.stAtInfo.is_connect_internet == 0)
        {
            // 每隔25s播放一次 网络连接中 一共2次
            if (stMobile4gAudioParam.play_connecting_net_cnt && (heart_beanj_4g_at_cur % MOBILE4G_CONNECTING_NET_INTERVAL_CNT == 0))
            {
                stMobile4gAudioParam.play_connecting_net_cnt--;
                anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_CONNECTING_NET, 1);
            }
            // 2分钟超时 每隔5s播放一次 网络连接失败 一共6次
            if (internet_failed_cnt > 1 &&
                stMobile4gAudioParam.play_connect_net_fail_cnt && (heart_beanj_4g_at_cur % MOBILE4G_CONNECT_NET_FAIL_INTERVAL_CNT == 0))
            {
                stMobile4gAudioParam.play_connect_net_fail_cnt--;
                anj_audio_prompt_play(ANJ_MP3_4G_PATH, ANJ_MP3_CONNECT_FAIL, 1);
            }
        }
        else
        {
            // 2每隔5s播放一次 扫描二维码 一共2次
            if (stMobile4gAudioParam.play_scan_qr_code_cnt && (heart_beanj_4g_at_cur % MOBILE4G_SCAN_QRCODE_INTERVAL_CNT == 0))
            {
                stMobile4gAudioParam.play_scan_qr_code_cnt--;
                anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_SCAN_QRCODE, 1);
            }
        }

        heart_beanj_4g_at_cur++;
        usleep(THREAD_DELAY_US);
    }

    __INFO("anj_4g_heartbeat_thread exit\n");
    return 0;
}

static void anj_4g_sim_set(EventResult *event_result, void *data)
{
    if (data && stMobile4gParam.anj_4g_at_mutex)
    {
        int CurOperator = *(int *)data;
        anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
        if (CurOperator != g_g4Status.CurOperator)
        {
            g_g4Status.CurOperator = CurOperator;
            /* 标记需要切卡并让线程负责重启，避免在回调线程阻塞 */
            stMobile4gParam.simChange = 1;
            stMobile4gParam.need_module_restart = 1;
        }
        anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
    }
}

static void anj_4g_status_get(EventResult *event_result, void *data)
{
    if (stMobile4gParam.anj_4g_at_mutex)
    {
        anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_HCSQ_QUERY] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_ICCID_QUERY] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_CREG_QUERY] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_QDSICCID_QUERY] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_GET_IMSI] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_GET_QDSIMSI] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_QUERY_MODULE_INFO] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_QUERY_MSISDN] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_QUERY_SYSINFO] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_NDISDUP_QUERY] = 1;
        stMobile4gParam.stAtInfo.update_flag[AT_SEQ_CPIN_QUERY] = 1;
        anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
        event_result->result = (void *)&g_g4Status;
    }
}

/*****************************************************************************
 函 数 名  : anj_4g_set_pause
 功能描述  : 4g模块设置暂停状态
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
void anj_4g_set_pause()
{
    if (stMobile4gParam.bInit == 0 || 
	    stMobile4gParam.anj_4g_at_mutex == NULL ||
		stMobile4gParam.anj_4g_at_comm_mutex == NULL)
    {
        __INFO("anj_4g_set_pause error\n");
        return;
    }

    anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);

    if (stMobile4gParam.thread_at_pause_flag == THREAD_STATUS_RUNNING)
    {
        __INFO("anj_4g_pause_flag = THREAD_STATUS_WAIT\n");
        stMobile4gParam.thread_at_pause_flag = THREAD_STATUS_WAIT;
        anj_4g_audio_param_reset();
    }

    anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
}

/*****************************************************************************
 函 数 名  : anj_4g_clear_pause
 功能描述  : 4g模块清除暂停状态
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
void anj_4g_clear_pause()
{
    if (stMobile4gParam.bInit == 0 || 
	    stMobile4gParam.anj_4g_at_mutex == NULL ||
		stMobile4gParam.anj_4g_at_comm_mutex == NULL)
    {
        __INFO("anj_4g_clear_pause error\n");
        return;
    }

    anj_mutex_lock(stMobile4gParam.anj_4g_at_mutex);

    if (stMobile4gParam.thread_at_pause_flag == THREAD_STATUS_PAUSE)
    {
        __INFO("anj_4g_pause_flag = THREAD_STATUS_RUNNING\n");
        stMobile4gParam.thread_at_pause_flag = THREAD_STATUS_RUNNING;
        anj_4g_audio_param_reset();
    }

    anj_mutex_unlock(stMobile4gParam.anj_4g_at_mutex);
}

/*****************************************************************************
 函 数 名  : anj_4g_query_module_exist
 功能描述  : 查询是否存在4G模块
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : 存在-1，不存在-0
*****************************************************************************/
int anj_4g_query_module_exist(void)
{
    int iRet = 0;
    if (stMobile4gParam.bInit == 0)
	    return iRet;
    if (s_iManufacturer != ANJ_4G_MANUFACTURER_NONE)
    {
        iRet = 1;
    }

    return iRet;
}

/*****************************************************************************
 函 数 名  : anj_4g_init
 功能描述  : 4g模块初始化
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
int anj_4g_init()
{
    if (stMobile4gParam.bInit)
    {
        __ERR("had been init\n");
        return -1;
    }

    anj_4g_module_hardware_restart();

    memset(&stMobile4gParam, 0, sizeof(stMobile4gParam));
    stMobile4gParam.osd_flag = 1;
    stMobile4gParam.uart_fd = -1;
    stMobile4gParam.thread_at_pause_flag = THREAD_STATUS_PAUSE;
    stMobile4gParam.heartbeat_pause_flag = THREAD_STATUS_PAUSE;
    anj_4g_audio_param_reset();
    stMobile4gParam.anj_4g_at_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (stMobile4gParam.anj_4g_at_mutex == NULL)
    {
        __ERR("Error:malloc anj_4g_at_mutex failed\n");
        return -1;
    }
    stMobile4gParam.anj_4g_at_comm_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (stMobile4gParam.anj_4g_at_comm_mutex == NULL)
    {
        __ERR("Error:malloc anj_4g_at_comm_mutex failed\n");
        anj_mw_free(stMobile4gParam.anj_4g_at_mutex);
        stMobile4gParam.anj_4g_at_mutex = NULL;
        return -1;
    }
    anj_mutex_create(stMobile4gParam.anj_4g_at_mutex, 0);
    anj_mutex_create(stMobile4gParam.anj_4g_at_comm_mutex, 0);

    if (access("/tmp/debug4g.flag", F_OK) == 0)
    {
        stMobile4gParam.debug_flag = 1;
    }
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    G4Config *pst4gConfig = &pstNetworkConfig->g4Cfg;
    g_g4Status.CurOperator = 0;
    g_g4Status.ManualMode = pst4gConfig->is_manual;
    if (pst4gConfig->is_manual)
    {
        if (pst4gConfig->cur_operator)
        {
            g_g4Status.CurOperator = 1;
        }
    }

    stMobile4gParam.stAtThread.bAutoDestroy = 1;
    strncpy(stMobile4gParam.stAtThread.iThreadName, "4g_thread", sizeof(stMobile4gParam.stAtThread.iThreadName) - 1);
    stMobile4gParam.stAtThread.iThreadjob.ctx = &stMobile4gParam.stAtThread;
    stMobile4gParam.stAtThread.iThreadjob.func = anj_4g_thread;
    if (anj_thread_task_create(&stMobile4gParam.stAtThread) != 0)
    {
        __ERR("Error:anj_4g_thread thread creation failed\n");
        return -1;
    }

    stMobile4gParam.stHeartBeatThread.bAutoDestroy = 1;
    strncpy(stMobile4gParam.stHeartBeatThread.iThreadName, "4g_heartbeat", sizeof(stMobile4gParam.stHeartBeatThread.iThreadName) - 1);
    stMobile4gParam.stHeartBeatThread.iThreadjob.ctx = &stMobile4gParam.stHeartBeatThread;
    stMobile4gParam.stHeartBeatThread.iThreadjob.func = anj_4g_heartbeat_thread;
    if (anj_thread_task_create(&stMobile4gParam.stHeartBeatThread) != 0)
    {
        __ERR("Error:anj_4g_heartbeat_thread thread creation failed\n");
        return -1;
    }

    if (IPC_4G_CONFIG_SET_ENABLE)
    {
        anj_sysctl_capability_add(FUNCTION_4G_CONFIG);
    }
    if (ANJ_GPIO_PORT_SWITCH_SIMCARD)
    {
        anj_mw_hwctrl_switch_simcard_status(0);
    }

    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_OSD_SET, anj_4g_osd_set);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_SIM_SET, anj_4g_sim_set);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, anj_4g_status_get);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_LOCATION_SET, anj_4g_location_set);
    anj_4g_location_load();
    stMobile4gParam.bInit = 1;
    return 0;
}

/*****************************************************************************
 函 数 名  : anj_4g_uninit
 功能描述  : 4g模块去初始化
 输入参数  : NULL
 输出参数  : NULL
 返 回 值  : NULL
*****************************************************************************/
int anj_4g_uninit()
{
    if (stMobile4gParam.bInit == 0)
    {
        __ERR("not init\n");
        return 0;
    }
    anj_thread_task_destroy(&stMobile4gParam.stHeartBeatThread, -1);
    anj_thread_task_destroy(&stMobile4gParam.stAtThread, -1);
    stMobile4gParam.bInit = 0;
    return 0;
}
