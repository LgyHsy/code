
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h> 
#include <termios.h>
#include <stdint.h> 
#include <stddef.h>

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_time.h"
#include "anj_mw_file.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_module.h"
#include "anj_sysmng.h"

#include "anj_mcu.h"

static int s_stMcuInit = 0;

#define MCU_DATA_MAX_LEN            128
#define MCU_READ_TIMEOUT_MS         20
#define MCU_WRITE_TIMEOUT_MS        10
#define MCU_WAIT_TIMEOUT_MS         15

#define MCU_HEADER_BIT0             0xFC
#define MCU_HEADER_BIT1             0x01

#define MCU_MAX_SEND_TIMES          3

#define MCU_VER_LEN                 17


#define MCU_UPDATE_TIMEOUT_SEC      45
#define MCU_UPDATE_PACKET_SIZE      128

typedef enum {
    STATE_IDLE,
    STATE_SEND_START,
    STATE_WAIT_REQUEST,
    STATE_SEND_DATA,
    STATE_COMPLETE,
    STATE_ERROR
} UpgradeState;

typedef struct {
    int fd;
    UpgradeState state;
    unsigned char *file_data;
    size_t file_size;
    size_t current_addr;
} UpgradeContext;



static AnjMcuCtrlInfo_t sAnjMcuCtrlInfo;
static anj_thread_s s_McuCtrlThread;

static time_t g_LastWriteUartTime = 0;      //记录最后一次发送数据时间
static time_t g_LastHeartBeatTime = 0;

static unsigned long long stTime = 0; //for test

static unsigned char g_rxbuffer[128] = {0};     //升级buf
static int g_rxdata_len = 0;

static pthread_mutex_t sMcuWriteMutex;      // 写数据锁，避免多个地方同时调用
static pthread_mutex_t sMcuInfoMutex;       // 回复状态的锁
static pthread_mutex_t sMcuUpdateMutex;     // 升级状态锁

unsigned short CRC16RTU(unsigned char *pszBuf, unsigned int unLength)
{
    unsigned short CRC = 0XFFFF;
    unsigned int CRC_count;

    for(CRC_count = 0; CRC_count < unLength; CRC_count++)
    {
        int i;

        CRC = CRC ^* (pszBuf+CRC_count);

        for(i = 0; i < 8; i++)
        {
            if(CRC&1)
            {
                CRC >>= 1;
                CRC ^= 0xA001;
            }
            else
            {
                CRC >>= 1;
            }
        }
    }

    return CRC;
}

int unpack_app_data(unsigned char *data, int len, unsigned char *appdata)
{
    int pos = 0;
    char verinfo[32] = {0};

    if (data[pos++] != 0x3A) 
    {
        return -1;
    }

    unsigned char verLength = data[pos++];

    if (pos + verLength > len || verLength > 32) 
    {
        return -1;
    }

    memcpy(verinfo, data + pos, verLength);
    pos += verLength;

    // 读取数据长度
    if (pos + 5 > len) 
    {
        return -2;
    }

    if (data[pos++] != 0x3A)
    {
        return -2;
    }

    unsigned int dataLength = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
    pos += 4;

    // 检查数据完整性
    if (pos + dataLength + 2 > len) 
    {
        return -2;
    }

    // 提取并校验数据CRC
    memcpy(appdata, data + pos, dataLength);
    pos += dataLength;

    unsigned short dataCrc = data[pos] << 8 | data[pos+1];
    unsigned short calcDataCrc = CRC16RTU(appdata, dataLength);

    if (calcDataCrc != dataCrc) 
    {
        return -3;
    }

    return dataLength;
}

unsigned char calculate_checksum(const unsigned char *data, int len) 
{
    int i = 0;
    unsigned char sum = 0;
    for (i = 0; i < len; i++) 
    {
        sum += data[i];
    }

    return sum;
}

static int remove_front_bytes(unsigned char *array, int *length, int n) 
{
    if (n <= 0) 
    {
        return *length;
    }

    if (length == NULL || array == NULL)
    {
        return -1;
    }

    if (n >= *length) 
    {
        *length = 0;
        return 0;
    }

    memmove(array, array + n, *length - n);
    *length -= n;

    return *length;
}


// 发送开始指令
int send_start_cmd(UpgradeContext *ctx) 
{
    unsigned char buffer[16] = {0};
    memcpy(buffer, "START", 5);
    
    // 文件大小（大端）
    buffer[5] = (ctx->file_size >> 24) & 0xFF;
    buffer[6] = (ctx->file_size >> 16) & 0xFF;
    buffer[7] = (ctx->file_size >> 8) & 0xFF;
    buffer[8] = ctx->file_size & 0xFF; 
    buffer[9] = calculate_checksum(buffer, 9);

    printf("======= send_start_cmd ===== file_size = 0x%X =====\r\n", ctx->file_size);

    if(10 != write(ctx->fd, buffer, 10))
    {
        __ERR("write buffer size error\n");
        return 0;
    }
    
    return 1;
}

// 发送数据包
int send_data_packet(UpgradeContext *ctx) 
{
    unsigned char packet[4 + 4 + 1 + MCU_UPDATE_PACKET_SIZE + 1] = {0};
    size_t remaining = ctx->file_size - ctx->current_addr;
    size_t data_size = remaining < MCU_UPDATE_PACKET_SIZE ? remaining : MCU_UPDATE_PACKET_SIZE;
    
    // 包头
    memcpy(packet, "DATA", 4);
    packet[4] = (ctx->current_addr >> 24) & 0xFF;
    packet[5] = (ctx->current_addr >> 16) & 0xFF;
    packet[6] = (ctx->current_addr >> 8) & 0xFF;
    packet[7] = ctx->current_addr & 0xFF;
    // 数据大小
    packet[8] = (unsigned char)data_size;
    // 数据内容
    memcpy(packet + 9, ctx->file_data + ctx->current_addr, data_size);

    if (data_size < MCU_UPDATE_PACKET_SIZE) 
    {
        memset(packet + 9 + data_size, 0, MCU_UPDATE_PACKET_SIZE - data_size);
    }
    packet[9 + MCU_UPDATE_PACKET_SIZE] = calculate_checksum(packet, 9 + MCU_UPDATE_PACKET_SIZE);
    
    printf("===== send_data_packet ==== [0x%X, %d] ====\r\n", ctx->current_addr, data_size);

    if (write(ctx->fd, packet, sizeof(packet)) != sizeof(packet)) 
    {
        return -1;
    }
    
    return 0;
}

// 处理请求数据指令
int handle_data_request(UpgradeContext *ctx) 
{
    int iRet = 0;
    unsigned char buf[32] = {0};

    int bytes = read(ctx->fd, buf, sizeof(buf));

    memcpy(g_rxbuffer + g_rxdata_len, buf, bytes);
    g_rxdata_len += bytes;

    if (g_rxdata_len < 10) 
    {
        if(strstr((char *)g_rxbuffer, "OKOK") != NULL)
        {
            ctx->state = STATE_COMPLETE;
        }
        return -1; // 等待接收完成
    }

    do
    {
        printf("===== Recv_data_request[%d]=: ", g_rxdata_len);
        for (int i = 0; i < g_rxdata_len; i++)
        {
            printf("%02X ", g_rxbuffer[i]);
        }
        printf("\r\n");
               
        if (memcmp(g_rxbuffer, "DATA", 4) != 0)     // 无效指令
        {
            iRet = -1;
            break;
        }  

        unsigned char sum = calculate_checksum(g_rxbuffer, 9);
        if (sum != g_rxbuffer[9]) 
        {
            iRet = -1;
            printf("calculate_checksum error!!!\r\n");
            break;
        }

        ctx->current_addr = g_rxbuffer[4] << 24 | g_rxbuffer[5] << 16 | g_rxbuffer[6] << 8 | g_rxbuffer[7];
    }while(0);

    remove_front_bytes(g_rxbuffer, &g_rxdata_len, g_rxdata_len);

    return iRet;
}



static int write_mcu_ver_to_file(char *pcMcuVer, char *file)
{
    int fd = -1;
    if(NULL == pcMcuVer || NULL == file)
    {
        __ERR("pcMcuVer or file is NULL\n");
        return -1;
    }

    if(access(file, F_OK) != 0)
    {
        if (strcmp(file, MCU_VER_TMP_PATH) == 0)
        {
            anj_mw_system("touch /tmp/mcu_ver.txt");
        }
        else
        {
            __ERR("%s not exist\n", file);
            return -1;
        }
    }

    fd = open(file, O_WRONLY);
    if(fd < 0)
    {
        __ERR("open %s err!\n", file);
        return -1;
    }
    else
    {
        __ERR("open %s succ!\n", file);
    }

    //写入文件
    if(write(fd, pcMcuVer, strlen(pcMcuVer)) < 0)
    {
        close(fd); 
        __ERR("write %s err!\n", file);
        return -1;
    }
    else
    {
        __ERR("write %s succ!\n", file);
    }

    close(fd); 
    return 0;
}

/*
    原架构中的 serial_init和 __uart_init现在共用
*/
static int mcu_uart_init(char *dev)
{
    if(NULL == dev)
    {
        __ERR("dev is NULL\n");
        return -1;
    }

    int fd = -1;
    struct termios oldtio;

    fd = open(dev, O_RDWR);
    if (fd < 0) 
    {
        __ERR("open mcu uart device: %s failed\n", dev);
        return -1;
    }

    __INFO("open mcu uart dev %s OK, fd = %d\n", dev, fd);

    if(tcgetattr(fd, &oldtio) != 0)
    {
        __ERR("SetupSerial 1\n");
        close(fd);
        return -1;
    }

    // 设置波特率
    cfsetospeed(&oldtio, B115200);
    cfsetispeed(&oldtio, B115200);

    // 8 数据位，无校验，1 停止位
    oldtio.c_cflag &= ~CSIZE;
    oldtio.c_cflag |= CS8;
    oldtio.c_cflag &= ~PARENB;
    oldtio.c_cflag &= ~CSTOPB;
    oldtio.c_cflag &= ~CRTSCTS;

    oldtio.c_cflag |= CLOCAL | CREAD;
    oldtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 原始输入输出模式
    oldtio.c_oflag &= ~OPOST;
    oldtio.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL);

    // 设置超时和最小读取字符数
    oldtio.c_cc[VTIME] = 0;
    oldtio.c_cc[VMIN] = 1;

    if(tcsetattr(fd, TCSANOW, &oldtio) != 0)
    {
        __ERR("com set error\n");
        close(fd);
        return -1;
    }
    __INFO("__uart_init success\n");    

    return fd;
}

static int mcu_uart_read(int fd, uint8_t *inbuf, uint32_t len, uint32_t timeout)
{
    if( fd < 0 )
    {
        return -1;
    }

    struct timeval wait_time = {0};     
    int maxFd = 0;      
    int selectret = 0;
    wait_time.tv_sec   = (timeout / 1000);
    wait_time.tv_usec  = (timeout % 1000) * 1000;

    fd_set readSet;
    maxFd = fd;

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);

    selectret = select(maxFd + 1, &readSet, NULL, NULL, &wait_time);
    if(selectret > 0)
    {
        int ret = read(fd, inbuf, len);
        // printf("read len %d\n", ret);
        return ret;
    }

    return 0;
}

static int mcu_uart_write(int fd, uint8_t *inbuf, uint32_t len, uint32_t timeout)
{
    if( fd <= 0 || NULL == inbuf )
    {
        return -1;
    }

    //这里从锁write操作，改为锁select + write
    pthread_mutex_lock(&sMcuWriteMutex);

    struct timeval wait_time = {0};
    int ret = 0;
    int maxFd = 0;      
    int selectret = 0;
    wait_time.tv_sec  = (timeout / 1000);
    wait_time.tv_usec = (timeout % 1000) * 1000;

    fd_set writeSet;
    maxFd = fd;

    FD_ZERO(&writeSet);
    FD_SET(fd, &writeSet);
    selectret = select(maxFd + 1, NULL, &writeSet, NULL, &wait_time);
    if(selectret > 0)
    {
        ret = write(fd, inbuf, len);

        //printf("Write %c = %d\n", *inbuf, ret);
        if(ret == len)
        {
            g_LastWriteUartTime = time(NULL);
        }
    }

    pthread_mutex_unlock(&sMcuWriteMutex);
    return ret;
}

//等待应答函数,把等待的变量传进来，检测。超时返回，等待到返回1
static int mcu_uart_wait_rsp(unsigned char *cFlag, unsigned int checkTimeMs)
{
    int nCnt = checkTimeMs;
    if(NULL == cFlag)
    {
        return -1;
    }

    if(0 == checkTimeMs)
    {
        return -1;
    }

    while(nCnt > 0)
    {
        if(1 == *cFlag)
        {
            return 1;
        }

        nCnt--;
        usleep(1000);
    }

    return 0;
}

//计算给定数据的和校验
static int mcu_get_check_sum_value(unsigned char *pucData, int nDataLen, unsigned char *pucCheckSum)
{
    if(NULL == pucData || NULL == pucCheckSum || 0 == nDataLen)
    {
        return -1;
    }

    *pucCheckSum = calculate_checksum(pucData, nDataLen);

    return 0;
}

/*
Fun: __PatchCmdBuf
函数说明：封装CMD指令发送buf
参数说明:
eCmd：CMD指令
pucSrcData 输入数据，允许为空
cSrcDataLen 输入数据长度，允许为0
pucDstData 输入数据buf
pucDstDataLen 作为输入是分配的buf大小
pucDstDataLen 作为输出是输出的实际数据长度   
*/
static int mcu_patch_cmd_buf(E_AOVCmd eCmd, unsigned char *pucSrcData, unsigned char cSrcDataLen, unsigned char *pucDstData, unsigned char *pucDstDataLen)
{
    unsigned char checksum = 0;

    if(NULL == pucDstData || NULL == pucDstDataLen)
    {
        __ERR("param is not valid\n");
        return -1;
    }

    if(*pucDstDataLen <(cSrcDataLen + 5))
    {
        __ERR("dst buf len is too small\n");
        return -1;
    }

    pucDstData[0] = 0xFC; //包头
    pucDstData[1] = 0x01; //保留字节
    pucDstData[2] = eCmd; //CMD

    //数据
    if(0 == cSrcDataLen)
    {
        cSrcDataLen = 1;
        pucDstData[3] = 0x01; //len
        pucDstData[4] = 0x00; //data    
    }
    else
    {
        pucDstData[3] = cSrcDataLen; //len
        memcpy(pucDstData+4, pucSrcData, cSrcDataLen);
    }

    mcu_get_check_sum_value(pucDstData, cSrcDataLen + 4, &checksum);
    //__ERR("checksum:%02X", checksum);
    pucDstData[cSrcDataLen + 4] = checksum;

    *pucDstDataLen = cSrcDataLen + 4 + 1;
    return 0;
}

static int mcu_aov_get_wakeup_timer_interval(int fd)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    mcu_patch_cmd_buf(E_AOVCmd_GetWakeupTimerInvertal, NULL, 0, data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
    printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write get wakeup timer interval error:%d\n", ret);
    }

    return 0;
}

static int mcu_aov_get_wakeup_src(int fd)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    mcu_patch_cmd_buf(E_AOVCmd_GetWakeupSrc, NULL, 0, data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    __INFO("get wakeup src ret:%d\n", ret);

    return 0;
}

static int mcu_aov_set_wakeup_src(int fd, E_WakeupSrc eWakeupSrc)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucWakeupSrc[1] = {0};

    aucWakeupSrc[0] = eWakeupSrc;
    mcu_patch_cmd_buf(E_AOVCmd_SetWakeupSrc, aucWakeupSrc, 1, data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
    printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write set wakeup src error:%d\n", ret);
    }

    return 0;   
}

static int mcu_aov_set_wakeup_timer_interval(int fd, unsigned int nTimerIntervalMs)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucTimerInterval[2] = {0};

#if 0   
    if(nTimerIntervalMs < 200 || nTimerIntervalMs > 30000)
    {
        __ERR("param is not valid");
        return -1;
    }
#endif  
    aucTimerInterval[0] = (nTimerIntervalMs >> 8) & 0xFF;
    aucTimerInterval[1] = nTimerIntervalMs & 0xFF;

    mcu_patch_cmd_buf(E_AOVCmd_SetWakeupTimerInvertal, aucTimerInterval, sizeof(aucTimerInterval), data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
    printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write set wakeup timer interval error:%d\n", ret);
    }

    return 0;
}

static int mcu_aov_lamp_ctrl(int fd, E_LampType eLampType, unsigned int pwmValue)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucLampParam[3] = {0};

    if(pwmValue < 0 || pwmValue > 5000)
    {
        __ERR("param is not valid\n");
        return -1;
    }

    aucLampParam[0] = eLampType;
    aucLampParam[1] = (pwmValue >> 8) & 0xFF;
    aucLampParam[2] = pwmValue & 0xFF;

    mcu_patch_cmd_buf(E_AOVCmd_LampCtrl, aucLampParam, sizeof(aucLampParam), data, &cDataLen);

    //  for(int i=0;i<cDataLen;i++)
    //      printf("%02X ", data[i]);
    //  printf("\n");

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    __INFO("lamp ctrl wirte ret:%d\n", ret);


    return 0;   
}

static int mcu_aov_reset_notify(int fd)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    mcu_patch_cmd_buf(E_AOVCmd_RstNotify, NULL, 0, data, &cDataLen);
    
    /*
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write reset notify error:%d\n", ret);
    }

    return 0;   
}

int mcu_aov_notify_battery_level(int fd, int iBatLevel)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucBatteryLevel[1] = {0};

    aucBatteryLevel[0] = iBatLevel;
    mcu_patch_cmd_buf(E_AOVCmd_BatteryLevelNotify, aucBatteryLevel, sizeof(aucBatteryLevel), data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write notify battery level error:%d\n", ret);
    }

    return 0;   
}

int mcu_aov_get_power_down_delay(int fd)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;

    mcu_patch_cmd_buf(E_AOVCmd_GetDelayPowerDownTime, NULL, 0, data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write get power down delay error:%d\n", ret);
    }

    return 0;   
}

int mcu_aov_set_power_down_delay(int fd, unsigned int nDelayTimeMs)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucDelayTime[2] = {0};

    __ERR("nDelayTimeMs:%d\n", nDelayTimeMs);
    
    if(nDelayTimeMs < 0 || nDelayTimeMs > 5000)
    {
        __ERR("nDelayTimeMs:%d is not valid\n", nDelayTimeMs);
        return -1;
    }
    
    aucDelayTime[0] = (nDelayTimeMs >> 8) & 0xFF;
    aucDelayTime[1] = nDelayTimeMs & 0xFF;
        
    mcu_patch_cmd_buf(E_AOVCmd_SetDelayPowerDownTime, aucDelayTime, sizeof(aucDelayTime), data, &cDataLen);
    
    /*
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write set power down delay time error:%d\n", ret);
    }

    
    return 0;   
}

static int mcu_aov_ctrl_rb_led(int fd, E_RBLedState eState)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucReLedStatus[1] = {0};

    aucReLedStatus[0] = eState;

    mcu_patch_cmd_buf(E_AOVCmd_RBLedCtrl, aucReLedStatus, sizeof(aucReLedStatus), data, &cDataLen);

    /*  
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */  

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write ctrl rb_red mcu ver error:%d\n", ret);
    }

    return 0;   
}

static int mcu_aov_get_ver(int fd)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    mcu_patch_cmd_buf(E_AOVCmd_GetMcuVer, NULL, 0, data, &cDataLen);

    /*  
    for(int i=0;i<cDataLen;i++)
        printf("%02X ", data[i]);
    printf("\n");
    */  

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write get mcu ver error:%d\n", ret);
    }

    return 0;
}



static int mcu_aov_set_heartbeat_interval(int fd, unsigned int nHbInvervals)
{
    int ret = 0;
    unsigned char data[8] = {0};
    unsigned char cDataLen = 8;
    unsigned char aucTimerInterval[2] = {0};

    if(nHbInvervals < 0 || nHbInvervals > 3600)
    {
        __ERR("nHbInvervals:%d is not valid\n", nHbInvervals);
        return -1;
    }

    aucTimerInterval[0] = (nHbInvervals >> 8) & 0xFF;
    aucTimerInterval[1] = nHbInvervals & 0xFF;

    mcu_patch_cmd_buf(E_AOVCmd_SetHbInterval, aucTimerInterval, sizeof(aucTimerInterval), data, &cDataLen);

    /*
    for(int i=0;i<cDataLen;i++)
    printf("%02X ", data[i]);
    printf("\n");
    */  

    ret = mcu_uart_write(fd, data, (uint32_t)cDataLen, MCU_WRITE_TIMEOUT_MS);
    if (ret <= 0)
    {
        __ERR("write heartbeat_interval error:%d\n", ret);
    }

    return 0;
}


//数据解析
static int mcu_aov_parse_recv_data(E_AOVCmd eCmd, unsigned char *paucBuf, unsigned char nBufLen)
{   
    E_WakeupSrc eWakeupSrc;

    if(nBufLen < 1)
    {
        __ERR("not a full cmd, donothing\n");
        return -1;
    }

    //CMD指令
    if(eCmd <E_AOVCmd_GetMcuVer || eCmd > E_AOVCmd_End)
    {
        __ERR("not a valid cmd\n");
        return -1;
    }

    //对整个 sAnjMcuCtrlInfo处理 加锁
    pthread_mutex_lock(&sMcuInfoMutex);

    //根据指令处理数据
    switch(eCmd)
    {
        case E_AOVCmd_GetMcuVer:
        {
            char acMcuVer[16] = {0};
            memcpy(acMcuVer, paucBuf, nBufLen);
            //__ERR("acMcuVer:%s", acMcuVer);

            snprintf(sAnjMcuCtrlInfo.acMcuVer, sizeof(sAnjMcuCtrlInfo.acMcuVer), "%s", acMcuVer);
            sAnjMcuCtrlInfo.getMcuVerRsp = 1;

            break;
        }

        case E_AOVCmd_GetWakeupSrc:
        case E_AOVCmd_SetWakeupSrc:
        {
            eWakeupSrc = (E_WakeupSrc)paucBuf[0];

            sAnjMcuCtrlInfo.eWakeupSrc = eWakeupSrc;
            if(E_AOVCmd_SetWakeupSrc == eCmd)
            {
                sAnjMcuCtrlInfo.setWakeupSrcRsp = 1;    
            }
            else
            {
                sAnjMcuCtrlInfo.getWakeupSrcRsp = 1;
            }

            break;
        }

        case E_AOVCmd_RstNotify:
        {
            __INFO("mcu will reboot system\n");
            break;
        }

        case E_AOVCmd_GetWakeupTimerInvertal:
        case E_AOVCmd_SetWakeupTimerInvertal:
        {
            __INFO("%02x %02x\n", paucBuf[0], paucBuf[1]);
            int WakeupTimerInvertal = (paucBuf[0] << 8) |  paucBuf[1];
            __ERR("WakeupTimerInvertal:%d\n", WakeupTimerInvertal);

            sAnjMcuCtrlInfo.nWakeupTimerInvervalMs = WakeupTimerInvertal;
            if(E_AOVCmd_SetWakeupTimerInvertal == eCmd)
            {
                sAnjMcuCtrlInfo.setWakeupTimerIntervalRsp = 1;
            }

            break;
        }

        case E_AOVCmd_GetDelayPowerDownTime:
        case E_AOVCmd_SetDelayPowerDownTime:
        {
            int DelayPowerDownTime =  (paucBuf[0] << 8) |  paucBuf[1];
            __ERR("DelayPowerDownTime:%d\n", DelayPowerDownTime);
            sAnjMcuCtrlInfo.nDelayPowerDownTimeMs = DelayPowerDownTime;

            break;
        }

        case E_AOVCmd_GetMcuRunTime:
        {
            uint32_t mcutime = paucBuf[0] << 24 | paucBuf[1] << 16| paucBuf[2] << 8 |  paucBuf[3];
            printf("mcu time:%u\n", mcutime);
            break;
        }

        case E_AOVCmd_SetHbInterval:
        {
            int HbIntervals =  (paucBuf[0] << 8) |  paucBuf[1];
            __ERR("HbIntervals:%d\n", HbIntervals);

            sAnjMcuCtrlInfo.nHbIntervals = HbIntervals;
            sAnjMcuCtrlInfo.setHbIntervalRsp = 1;

            break;
        }

        default:
        {
            //__ERR("do nothing\n");
            break;
        }
    }
    pthread_mutex_unlock(&sMcuInfoMutex);

    return 0;
}


// 升级主流程
int mcu_aov_upgrade_process(UpgradeContext *ctx) 
{
    int iRet = 0;
  
    unsigned long long start_time = anj_mw_get_cputime_ms(NULL);
    unsigned long long end_time = start_time;
   
    printf("========== upgrade_process start! =========\n");

    while (ctx->state != STATE_COMPLETE && ctx->state != STATE_ERROR) 
    {
        switch (ctx->state) 
        {
            case STATE_SEND_START:
                if (!send_start_cmd(ctx)) 
                {
                    ctx->state = STATE_ERROR;
                    break;
                }

                ctx->state = STATE_WAIT_REQUEST;
                break;
                
            case STATE_WAIT_REQUEST:
                {
                    struct timeval tv = {1, 0};
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(ctx->fd, &fds);

                    if (select(ctx->fd + 1, &fds, NULL, NULL, &tv) > 0)
                    {
                        if (handle_data_request(ctx) == 0)
                        {
                            ctx->state = STATE_SEND_DATA;
                        }
                    }
                   
                    end_time = anj_mw_get_cputime_ms(NULL);
                    //__INFO("start_time:%u, end_time:%u\n", start_time, end_time);
                    if (end_time - start_time > MCU_UPDATE_TIMEOUT_SEC * 1000) 
                    {
                        ctx->state = STATE_ERROR;
                    }
                }
                break;
                
            case STATE_SEND_DATA:
                {
                    if (send_data_packet(ctx)) 
                    {
                        ctx->state = STATE_ERROR;
                        break;
                    }
                    
                    if (ctx->current_addr >= ctx->file_size) 
                    {
                        ctx->state = STATE_COMPLETE;
                    } 
                    else 
                    {
                        ctx->state = STATE_WAIT_REQUEST;
                    }
                }
                break;
                
            default:
                __ERR("error update state:%d\n", ctx->state);
                break;
        }

        usleep(10 * 1000);
    }
    
    if (ctx->state == STATE_COMPLETE) 
    {
        iRet = 0;
        printf("Upgrade successful!\n");
    }
    else 
    {
        iRet = ctx->state;
        printf("Upgrade failed! %d \n", ctx->state);
        // 发送异常结束指令
        unsigned char end_cmd[] = {'E', 'N', 'D', 0x00};
        for (int i = 0; i < 3; i++) 
        {
            write(ctx->fd, end_cmd, sizeof(end_cmd));
        }
    }

    return iRet;
}


int mcu_aov_update(int fd, char *filepath)
{
    int iRet = 0;

    if (fd <= 0)
    {
        __ERR("update error fd:%d\n", fd);
        return -1;
    }

    if (NULL == filepath)
    {
        unsigned char cmd_buff[] = {'T', 'O', 'A', 'P', 'P', 0x00};
        write(fd, cmd_buff, sizeof(cmd_buff));
        //close(fd);

        __ERR("mcu app not need to be updated\r\n");
        return 0; 
    }

    // 读取固件文件
    FILE *fp = fopen(filepath, "rb");
    if (!fp) 
    {
        __ERR("Error opening firmware file:%s\r\n", filepath);
        //close(fd);

        return -2;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    unsigned int file_size = ftell(fp);
    rewind(fp);    

    unsigned char *file_data = malloc(file_size);
    unsigned char *app_data = malloc(file_size);
    if(NULL == file_data || NULL == app_data)
    {
        if(file_data)
        {
            free(file_data);
            file_data = NULL;
        }

        if (app_data)
        {
            free(app_data);
            app_data = NULL;
        }

        fclose(fp);
        //close(fd);
        return -3;
    }

    // 读取文件内容
    fread(file_data, 1, file_size, fp);
    fclose(fp);

    int app_size = unpack_app_data(file_data, file_size, app_data);
    printf("===== app_size =%d =====\r\n", app_size);   
    if(app_size < 0)
    {
        free(file_data);
        free(app_data);
        //close(fd);
        return -4;
    }
    free(file_data);

    UpgradeContext ctx = {
        .fd = fd,
        .state = STATE_SEND_START,
        .file_data = app_data,
        .file_size = app_size,
        .current_addr = 0
    };
    
    iRet = mcu_aov_upgrade_process(&ctx);
    
    free(app_data);
    //close(fd);

    return iRet;

}


int anj_mcu_get_upgrade_stat()
{
    int upgrade_flag = 0;
    pthread_mutex_lock(&sMcuUpdateMutex);
    upgrade_flag = sAnjMcuCtrlInfo.nNeedUpdateFlag;
    pthread_mutex_unlock(&sMcuUpdateMutex);

    return upgrade_flag;
}

int anj_mcu_get_init_stat()
{
    int init_flag = 0;
    pthread_mutex_lock(&sMcuUpdateMutex);
    init_flag = sAnjMcuCtrlInfo.nInitFlag;
    pthread_mutex_unlock(&sMcuUpdateMutex);

    return init_flag;
}

E_WakeupSrc anj_mcu_get_wakeup_src()
{
    pthread_mutex_lock(&sMcuInfoMutex);
    E_WakeupSrc eWakeupSrc = sAnjMcuCtrlInfo.eWakeupSrc;
    sAnjMcuCtrlInfo.eWakeupSrc = E_WakeupSrc_Empty;
    pthread_mutex_unlock(&sMcuInfoMutex);

    if(E_WakeupSrc_Empty == eWakeupSrc)
    {
        mcu_aov_get_wakeup_src(sAnjMcuCtrlInfo.fd);
    }

    return eWakeupSrc;
}


int anj_mcu_set_wakeup_src(unsigned int uWakeupSrc)
{
    int nRet = 0;
    unsigned char cRspFlag = 0;

    E_WakeupSrc eWakeupSrc = (E_WakeupSrc)uWakeupSrc;

    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.setWakeupSrcRsp = 0;
    pthread_mutex_unlock(&sMcuInfoMutex);

    for(int i = 0; i < MCU_MAX_SEND_TIMES + 1; i++)
    {
        mcu_aov_set_wakeup_src(sAnjMcuCtrlInfo.fd, eWakeupSrc);

        pthread_mutex_lock(&sMcuInfoMutex);
        cRspFlag = sAnjMcuCtrlInfo.setWakeupSrcRsp;
        pthread_mutex_lock(&sMcuInfoMutex);

        nRet = mcu_uart_wait_rsp(&cRspFlag, MCU_WAIT_TIMEOUT_MS);
        if(1 == nRet)
        {
            //__ERR("wakeupSrc:%d", g_McuUartInfo.eWakeupSrc);
            break;
        }
    }

    if(1 != nRet)
    {
        __ERR("failed to set wakeup src\n");
        return -1;
    }

    return 0;
}


int anj_mcu_set_wakeup_timer_interval(unsigned int nTimerIntervalMs)
{
    int nRet = 0;
    unsigned char cRspFlag = 0;

    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.setWakeupTimerIntervalRsp = 0;
    pthread_mutex_unlock(&sMcuInfoMutex);

    for(int i = 0; i < MCU_MAX_SEND_TIMES; i++)
    {
        mcu_aov_set_wakeup_timer_interval(sAnjMcuCtrlInfo.fd, nTimerIntervalMs);

        pthread_mutex_lock(&sMcuInfoMutex);
        cRspFlag = sAnjMcuCtrlInfo.setWakeupTimerIntervalRsp;
        pthread_mutex_unlock(&sMcuInfoMutex);

        nRet = mcu_uart_wait_rsp(&cRspFlag, MCU_WAIT_TIMEOUT_MS);
        if(1 == nRet)
        {
            break;
        }
    }

    if(1 != nRet)
    {
        __ERR("failed to set wakeup timer interval\n");     
        return -1;
    }

    __ERR("McuUart set wakeup time interval %d success\n", nTimerIntervalMs);

    return 0;
}


int anj_mcu_lamp_ctrl(E_LampType eLampType, unsigned int pwmValue)
{
    pthread_mutex_lock(&sMcuInfoMutex);
    if(sAnjMcuCtrlInfo.eLampType == eLampType && sAnjMcuCtrlInfo.nPwmValue == pwmValue)
    {
        pthread_mutex_unlock(&sMcuInfoMutex);
        return 0;
    }

    __INFO("eLampType:%d, pwmValue:%d change to:%d, %d\n", sAnjMcuCtrlInfo.eLampType, sAnjMcuCtrlInfo.nPwmValue, eLampType, pwmValue);
    sAnjMcuCtrlInfo.eLampType = eLampType;
    sAnjMcuCtrlInfo.nPwmValue = pwmValue;
    pthread_mutex_unlock(&sMcuInfoMutex);

    mcu_aov_lamp_ctrl(sAnjMcuCtrlInfo.fd, eLampType, pwmValue);

    return 0;
}

int anj_mcu_set_heartbeat_interval(unsigned int nHbIntervals)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stMcuInit), iRet, "not init");
    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.setHbIntervalRsp = 0;
    pthread_mutex_unlock(&sMcuInfoMutex);

    int i = 0;
    unsigned char cRspFlag = 0;

    for(i = 0; i < MCU_MAX_SEND_TIMES; i++)
    {
        mcu_aov_set_heartbeat_interval(sAnjMcuCtrlInfo.fd, nHbIntervals);

        pthread_mutex_lock(&sMcuInfoMutex);
        cRspFlag = sAnjMcuCtrlInfo.setHbIntervalRsp;
        pthread_mutex_unlock(&sMcuInfoMutex);

        iRet = mcu_uart_wait_rsp(&cRspFlag, MCU_WAIT_TIMEOUT_MS);
        if(1 == iRet)
        {
            break;
        }
    }

    if(1 != iRet)
    {
        __ERR("failed to set heartbeat interval\n");
        iRet = -1;
        goto endFunc;
    }

    iRet = 0;
endFunc:
    return iRet;
}


int anj_mcu_reset_notify()
{
    return mcu_aov_reset_notify(sAnjMcuCtrlInfo.fd);    
}

int anj_mcu_ctrl_rb_led(E_RBLedState eRbLedState)
{
    __ERR("new eRbLedState:%d\n", eRbLedState);
    mcu_aov_ctrl_rb_led(sAnjMcuCtrlInfo.fd, eRbLedState);

    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.eRBLedState = eRbLedState;
    pthread_mutex_unlock(&sMcuInfoMutex);

    return 0;
}

int anj_mcu_get_ver()
{
    int nRet = 0;
    unsigned char cRspFlag = 0;

    //获取版本号
    int i = 0;

    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.getMcuVerRsp = 0;
    pthread_mutex_unlock(&sMcuInfoMutex);

    for(i = 0; i< MCU_MAX_SEND_TIMES; i++)
    {
        mcu_aov_get_ver(sAnjMcuCtrlInfo.fd);

        pthread_mutex_lock(&sMcuInfoMutex);
        cRspFlag = sAnjMcuCtrlInfo.getMcuVerRsp;
        pthread_mutex_unlock(&sMcuInfoMutex);

        nRet = mcu_uart_wait_rsp(&cRspFlag, MCU_WAIT_TIMEOUT_MS);
        if(1 == nRet)
        {
            __ERR("get mcu ver:%s\n", sAnjMcuCtrlInfo.acMcuVer);
            write_mcu_ver_to_file(sAnjMcuCtrlInfo.acMcuVer, MCU_VER_TMP_PATH);
            break;
        }
    }

    if(1 != nRet)
    {
        __ERR("failed to get mcu version\n");
        return -1;
    }

    return 0;
}

int anj_mcu_set_power_down_delay_ms(unsigned int nDelayTimeMs)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stMcuInit), iRet, "not init");
    iRet = mcu_aov_set_power_down_delay(sAnjMcuCtrlInfo.fd, nDelayTimeMs);
endFunc:
    return iRet;
}


/*********************************/

E_WakeupSrc anj_mcu_check_wakeup()
{
    int nRet = 0;
    E_WakeupSrc eWakeupSrc = E_WakeupSrc_Empty;
    int retryTimes = 6;

    unsigned char cRspFlag = 0;

    pthread_mutex_lock(&sMcuInfoMutex);
    sAnjMcuCtrlInfo.getWakeupSrcRsp = 0;
    pthread_mutex_unlock(&sMcuInfoMutex);

    __ERR("stTime:%llu\n", stTime);

    int i = 0;
    for ( i = 0; i < retryTimes; i++)
    {
        pthread_mutex_lock(&sMcuInfoMutex);
        stTime = anj_mw_get_cputime_ms(NULL);
        cRspFlag = sAnjMcuCtrlInfo.getWakeupSrcRsp;
        pthread_mutex_unlock(&sMcuInfoMutex);

        mcu_aov_get_wakeup_src(sAnjMcuCtrlInfo.fd);
        nRet = mcu_uart_wait_rsp(&cRspFlag, MCU_WAIT_TIMEOUT_MS);
        if(1 == nRet)
        {   
            __ERR("wakeupSrc:%d\n", sAnjMcuCtrlInfo.eWakeupSrc);
            eWakeupSrc = sAnjMcuCtrlInfo.eWakeupSrc;
            break;
        }
    }

    if(eWakeupSrc != E_WakeupSrc_Empty)
    {
        if(E_WakeupSrc_AlwaysOn == eWakeupSrc)
        {
            __ERR("AlaysOn source from MCU\n");
        }
        else if(E_WakeupSrc_Net == eWakeupSrc)
        {
            __ERR("Net wakeup source from MCU\n");
        }
        else
        {
            //   printf("Timer wakeup source from MCU\n");
        }

        return eWakeupSrc;      
    }

    __ERR("Get wakeup type fail\n");
    return E_WakeupSrc_AlwaysOn;   //获取唤醒源失败，切换为长电模式
}


static int anj_mcuctrl_thread(void *ctx, int *bStart)
{
    AnjMcuCtrlInfo_t *pMcuCtrlInfo = (AnjMcuCtrlInfo_t *)ctx;

    int iRet = 0;
    int read_pos  = 0;
    unsigned char buffer[MCU_DATA_MAX_LEN] = {0};
    time_t curTime = 0;

    int mcu_update_complete = 0;
    int mcu_update_flag = 0;

    pMcuCtrlInfo->iRuningFlag = 1;

    while(bStart && 1 == *bStart)
    {
        pthread_mutex_lock(&sMcuUpdateMutex);
        mcu_update_flag = pMcuCtrlInfo->nNeedUpdateFlag;
        pthread_mutex_unlock(&sMcuUpdateMutex);

        while(1 == mcu_update_flag)
        {
            if (access(MCU_AOV_BIN_NEW_PATH, F_OK) == 0)
            {
                iRet = mcu_aov_update(pMcuCtrlInfo->fd, MCU_AOV_BIN_NEW_PATH);

                anj_mw_system_with_param("rm -rf %s", MCU_AOV_BIN_NEW_PATH);
                __INFO("Del update file:%s\n", MCU_AOV_BIN_NEW_PATH);

                if (0 == iRet)
                {
                    __INFO("MCU auto update: %s\n", MCU_AOV_BIN_NEW_PATH);
                    write_buffer_to_file(MCU_VER_PATH, pMcuCtrlInfo->UpdateCurVerInfo, sizeof(pMcuCtrlInfo->UpdateCurVerInfo) - 1); 
                }
                else if (5 == iRet && access(MCU_RESET_FILE, F_OK) != 0)
                {
                    anj_mw_system_with_param("touch %s", MCU_RESET_FILE);
                    __WARN("=== reboot updata mcu app ===\n");
                    __RECORD_LOG_INFO("reboot updata mcu app\n");
                    anj_sysmng_reboot();
                }
            }
            else
            {
                iRet = mcu_aov_update(pMcuCtrlInfo->fd, NULL);
            }

            if (iRet)
            {
                __ERR("mcu aov update iRet:%d\n", iRet);
            }

            mcu_update_complete = 1;
            mcu_update_flag = 0;

            usleep(10 * 1000);
        }

        if (1 == mcu_update_complete)
        {
            mcu_update_complete = 0;
            pthread_mutex_lock(&sMcuUpdateMutex);
            pMcuCtrlInfo->nNeedUpdateFlag = 0;
            pthread_mutex_unlock(&sMcuUpdateMutex);
        }

        uint8_t data = 0;
        uint32_t data_len = 1;
        if(mcu_uart_read(pMcuCtrlInfo->fd, &data, data_len, MCU_READ_TIMEOUT_MS) != data_len)
        {
            if(0 != pMcuCtrlInfo->nHbIntervals)
            {
                curTime = time(NULL);
                time_t diffTime = abs(curTime - g_LastWriteUartTime);
                int diff2 = (int)abs(curTime - g_LastHeartBeatTime);

                //时间有跳变或者到心跳时间都发一次心跳
                if((diffTime >= (pMcuCtrlInfo->nHbIntervals - 5) && g_LastWriteUartTime != 0) || diff2 > 1)
                {
                    //发一次数据作为心跳
                    __INFO("snd heatbeat to mcu\n");
                    mcu_aov_get_wakeup_timer_interval(pMcuCtrlInfo->fd);
                }           

                g_LastHeartBeatTime = curTime;
            }

            continue;
        }
/*
2字节头部:FC 01 A1 01 01 A0
1字节类型
1字节长度:
N字节数据
1字节校验：对上面N字节数据的CHECKSUM
*/      
        if( read_pos == 0 )
        {
            if( data != MCU_HEADER_BIT0)
            {
                continue;
            }
        }
        else if( read_pos == 1)
        {
            if( data != MCU_HEADER_BIT1)
            {
                read_pos = 0;               
                continue;
            }
        }
        else if( read_pos > 2)
        {
            //中途如果出现信令头部，则拷贝到buffer头部重新开始
            if( buffer[read_pos-1] == MCU_HEADER_BIT0 && data == MCU_HEADER_BIT1)
            {
                buffer[0] = MCU_HEADER_BIT0;
                buffer[1] = MCU_HEADER_BIT1;
                read_pos = 2;               
                continue;
            }
        }

        buffer[read_pos] = data;
        read_pos++;
        if( read_pos >= MCU_DATA_MAX_LEN)//错误数据，强制清零
        {
            read_pos = 0;           
            continue;
        }

        if( read_pos > 4)
        {
            uint8_t payloadlen = buffer[3];
            if( 4 + payloadlen + 1 == read_pos)//指定数据长度已经收齐
            {
                unsigned char checksum = buffer[4+payloadlen];
                unsigned char checksum_data = 0;
                E_AOVCmd eCmd = (E_AOVCmd)buffer[2];
                mcu_get_check_sum_value(buffer, payloadlen + 4, &checksum_data);
                //__ERR("Get type:%02x, payload len %d, checksum %d checksum_data:%d",
                //  eCmd, payloadlen, checksum, checksum_data);
                                
                #if 0
                for(int i=0;i<read_pos;i++) 
                {
                    printf("%02X ", buffer[i]);
                }
                printf("\n");
                #endif
                
                //unsigned long long edTime = GetCurTimeStamp();
                //__ERR("recv rsp edTime:%llu diff %d ms", edTime, edTime - stTime);
                
                if( checksum_data != checksum )
                {
                    __ERR("checksum %d, should be %d\n", checksum, checksum_data);
                }
                else
                {
                    mcu_aov_parse_recv_data(eCmd, &buffer[4], payloadlen);
                }
                memset(buffer, 0, MCU_DATA_MAX_LEN); 
                read_pos = 0;
            }
        }
    }

    __ERR("exit mcu ctrl thread!\n");

    pMcuCtrlInfo->iRuningFlag = 0;

    if (pMcuCtrlInfo->fd > 0)
    {
        close(pMcuCtrlInfo->fd);
        pMcuCtrlInfo->fd = -1;
    }
    

    return 0;
}

static void anj_mcu_upgrade_init()
{
    int autoUpdate = 0;

    char old_ver_info[64] = {0};
    char cur_ver_info[64] = {0};

    if(access(MCU_AOV_BIN_ORG_PATH, F_OK) == 0)
    {
        //从老固件中读取版本信息
        read_file_to_buffer(MCU_AOV_BIN_ORG_PATH, cur_ver_info, sizeof(cur_ver_info)-1);

        if(access(MCU_VER_PATH, F_OK) == 0)
        {
            // 从新的版本文件中读取版本信息
            read_file_to_buffer(MCU_VER_PATH, old_ver_info, sizeof(old_ver_info)-1);

            if(strncmp(old_ver_info, cur_ver_info, MCU_VER_LEN) != 0)
            {
                autoUpdate = 1;
                anj_mw_system_with_param("cp -f %s %s", MCU_AOV_BIN_ORG_PATH, MCU_AOV_BIN_NEW_PATH);
            }           
        }
        else
        {
            autoUpdate = 1;
            anj_mw_system_with_param("cp -f %s %s", MCU_AOV_BIN_ORG_PATH, MCU_AOV_BIN_NEW_PATH);
        }

        if(!autoUpdate)
        {
            __INFO("No need update %s\n", MCU_AOV_BIN_ORG_PATH);
        }
    }

    if (1 == autoUpdate)
    {
        __INFO("mcu need auto update!\n");
        sAnjMcuCtrlInfo.nNeedUpdateFlag = autoUpdate;
        snprintf(sAnjMcuCtrlInfo.UpdateCurVerInfo, sizeof(sAnjMcuCtrlInfo.UpdateCurVerInfo), "%s", cur_ver_info);
    }
}


static int anj_mcu_info_init()
{
    memset(&sAnjMcuCtrlInfo, 0, sizeof(sAnjMcuCtrlInfo));

    sAnjMcuCtrlInfo.eWakeupSrc = E_WakeupSrc_Empty;
    sAnjMcuCtrlInfo.nPwmValue = 9999;
    sAnjMcuCtrlInfo.nHbIntervals = 0;

    sAnjMcuCtrlInfo.fd = mcu_uart_init(MCU_UART_DEV);
    if(sAnjMcuCtrlInfo.fd < 0)
    {
        __ERR("mcu_uart_init failed.\n");
        return -1;
    }

    memset(sAnjMcuCtrlInfo.UpdateCurVerInfo, 0, sizeof(sAnjMcuCtrlInfo.UpdateCurVerInfo));
    sAnjMcuCtrlInfo.nNeedUpdateFlag = 0;
    sAnjMcuCtrlInfo.nInitFlag = 1;
    __INFO("mcu fd:%d\n", sAnjMcuCtrlInfo.fd);
    return 0;
}

int anj_mcu_init(void)
{
    int iRet = 0;

    ANJ_CHK((0 == s_stMcuInit), iRet, "had been init");
    pthread_mutex_init(&sMcuWriteMutex, NULL);
    pthread_mutex_init(&sMcuInfoMutex, NULL);
    pthread_mutex_init(&sMcuUpdateMutex, NULL);

    iRet = anj_mcu_info_init(&sAnjMcuCtrlInfo);
    if (iRet)
    {
        return -1;
    }

    anj_mcu_upgrade_init();

    memset(&s_McuCtrlThread, 0, sizeof(anj_thread_s));
    s_McuCtrlThread.bAutoDestroy = 1;
    strncpy(s_McuCtrlThread.iThreadName, "anj_mcuctrl_thread", sizeof(s_McuCtrlThread.iThreadName) - 1);
    s_McuCtrlThread.iThreadjob.ctx = &sAnjMcuCtrlInfo;
    s_McuCtrlThread.iThreadjob.func = anj_mcuctrl_thread;
    iRet = anj_thread_task_create(&s_McuCtrlThread);

    s_stMcuInit = 1;
endFunc:
    return iRet;
}

static void anj_mcu_uart_uninit()
{
    sAnjMcuCtrlInfo.nPwmValue = 9999;

    if (sAnjMcuCtrlInfo.fd)
    {
        close(sAnjMcuCtrlInfo.fd);
        sAnjMcuCtrlInfo.fd = 0;
    }

    sAnjMcuCtrlInfo.nInitFlag = 0;
}


int anj_mcu_uninit(void)
{
    int iRet = 0;

    ANJ_CHK((1 == s_stMcuInit), iRet, "not init");
    anj_thread_task_destroy(&s_McuCtrlThread, -1);
    anj_mcu_uart_uninit();

    pthread_mutex_destroy(&sMcuUpdateMutex);
    pthread_mutex_destroy(&sMcuInfoMutex);
    pthread_mutex_destroy(&sMcuWriteMutex);
    s_stMcuInit = 0;
endFunc:
    return iRet;
}

void anj_mcu_restart()
{
    if (s_stMcuInit == 0)
    {
        return;
    }
    anj_mcu_uninit();

    usleep(200 * 1000);

    anj_mcu_init();
}


REGISTER_MODULE(anj_mcu, 5);
