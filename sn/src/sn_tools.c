#include <string.h>
#include <errno.h>
#include <net/if.h>      // struct ifreq 的定义在这个文件中
#include <sys/ioctl.h>  // 配合 ioctl() 系统调用使用
#include <sys/socket.h> // 必须引入，因为 ifreq 内部使用了 sockaddr 结构体

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/route.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "sha2.h"

/**
 * @brief	Get an interface flag.
 * @param	"char *ifname" : interface name
 * @retval	ifr.ifr_flags
 * @retval	-1 : fail
 */
/**
 * @brief 获取指定网络接口的标志位
 *
 * 通过创建套接字并调用 ioctl 的 SIOCGIFFLAGS 命令，
 * 读取指定网络接口的标志位信息（如 IFF_UP、IFF_RUNNING 等）。
 *
 * @param[in] ifname 网络接口名称，例如 "eth0"、"wlan0"
 *
 * @return 成功时返回接口的标志位（ifr_flags），非负整数
 * @return 失败时返回 -1（套接字创建失败或 ioctl 调用失败）
 *
 * @throws 套接字创建失败时输出错误日志并返回 -1
 * @throws ioctl SIOCGIFFLAGS 调用失败时输出错误日志并返回 -1
 */
int sn_net_get_flag(const char *ifname)
{
	struct ifreq ifr;
	int skfd;

	if ( (skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		__ERR("%s socket error\n", ifname);
		return -1;
	}
	
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) 
	{
		__ERR("%s ioctl SIOCGIFFLAGS, skfd %d\n", ifname, skfd);
		close(skfd);
		return -1;
	}
	close(skfd);
	return ifr.ifr_flags;
}

/**
 * @brief 检查指定网络接口是否已连接（处于 RUNNING 状态）
 * @param ifname 网络接口名称，例如 "eth0"
 * @return 1 表示接口已连接（IFF_RUNNING 标志已设置），0 表示接口未连接或获取标志失败
 */
int sn_ifname_connected(const char *ifname)
{
	int flag;

	flag = sn_net_get_flag(ifname);

//	__ERR("flag=0x%x  IFF_UP=0x%x, IFF_RUNNING=0x%x\n", flag, IFF_UP, IFF_RUNNING);
	if(flag == -1)
	{
		return 0;
	}

	if((flag & IFF_RUNNING) == 0)
	{
		return 0;
	}

	return 1;
}

/**
 * @brief 获取当前已连接的网络接口名称，优先选择有线接口
 *
 * 依次检测有线接口和无线接口的连接状态，将第一个已连接的接口名称
 * 复制到输出缓冲区中。有线接口优先于无线接口。
 *
 * @param[out] ifname 输出缓冲区，用于存储接口名称。若传入 NULL 则直接返回，
 *                    若无已连接接口则内容被置为空字符串。
 */	
void sn_get_ifname(char *ifname)
{
    if( NULL == ifname)
        return;

    ifname[0] = 0;
	if (sn_ifname_connected(WIRE_INTERFACE_NAME))
	{
		strcpy(ifname, WIRE_INTERFACE_NAME);
	}
	else
	{
		const char *wireless_name = net_get_wireless_name();
        if (sn_ifname_connected(wireless_name))
        {
            strcpy(ifname, wireless_name);
        }
	}
}


// 检查广播路由是否已经存在
// 返回 1 表示存在，0 表示不存在，-1 表示读取失败
/**
 * @brief 检查指定网卡上是否存在广播路由
 *
 * 通过读取 /proc/net/route 文件，查找目的地址为 255.255.255.255（0xFFFFFFFF）
 * 的路由条目，判断指定网卡上是否配置了广播路由。
 *
 * @param interface_name 网卡接口名称，如 "eth0"、"br-lan" 等
 * @return 1 表示存在广播路由，0 表示不存在，-1 表示打开路由文件失败或读取异常
 * @throws 无法打开 /proc/net/route 时通过 perror 输出错误信息并返回 -1
 */
int sn_broadcast_if_route_exist(const char *interface_name) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/net/route");
        return -1;
    }

    char line[256];
    char iface[32];
    unsigned int dest, gateway, flags, mask;
    int found = 0;

    // 跳过表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    // 循环读取每一行路由信息
    while (fgets(line, sizeof(line), fp)) {
        // 解析网卡名、目的地址(Hex)、网关(Hex)、标志位、掩码(Hex)
        if (sscanf(line, "%31s %X %X %X %*d %*d %*d %X", iface, &dest, &gateway, &flags, &mask) == 5) {
            // 255.255.255.255 的十六进制是 0xFFFFFFFF
            // 同时比对网卡名称，确保是绑定在指定网卡上的广播路由
            if (dest == 0xFFFFFFFF && strcmp(iface, interface_name) == 0) {
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found;
}

/**
 * @brief 为指定网卡接口添加全网广播主机路由
 *
 * 先检查该接口上是否已存在广播路由，若已存在则直接返回成功。
 * 否则通过 ioctl(SIOCADDRT) 向内核添加一条目的地址为 255.255.255.255、
 * 子网掩码为 255.255.255.255 的主机路由，绑定到指定网卡接口。
 *
 * @param interface_name 网卡接口名称，例如 "eth0"、"wlan0"
 * @return 0 表示成功（路由已存在或添加成功），-1 表示失败
 * @throws socket 创建失败时返回 -1 并通过 perror 输出错误信息
 * @throws ioctl SIOCADDRT 调用失败时返回 -1 并通过 perror 输出错误信息
 */	
int sn_broadcast_if_route_add(const char *interface_name) {
    int check = sn_broadcast_if_route_exist(interface_name);
    if (check == 1) {
        printf("Broadcast route already exists on %s.\n", interface_name);
        return 0; 
    }


    int sockfd;
    struct rtentry route;
    struct sockaddr_in *addr;

    // 1. 创建 standard UDP socket 用于 ioctl 操作
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&route, 0, sizeof(route));

    // 2. 设置目的地址为全网广播 255.255.255.255
    addr = (struct sockaddr_in *)&route.rt_dst;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr("255.255.255.255");

    // 3. 设置子网掩码为 255.255.255.255 (主机路由)
    addr = (struct sockaddr_in *)&route.rt_genmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr("255.255.255.255");

    // 4. 指定网卡接口名称 (例如 "eth0" 或 "wlan0")
    route.rt_dev = (char *)interface_name;
    route.rt_flags = RTF_UP | RTF_HOST; // 路由有效且为主机路由

    // 5. 调用 ioctl 添加路由到内核
    if (ioctl(sockfd, SIOCADDRT, &route) < 0) {
        perror("SIOCADDRT failed (Maybe route already exists?)");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    printf("Successfully added broadcast route to %s\n", interface_name);
    return 0;
}


/**
 * @brief 添加广播路由
 *
 * 获取当前网络接口名称，若接口名称有效则为其添加广播路由。
 *
 * @return int 固定返回 0
 */
int sn_broadcast_route_add() 
{
    mysystem_with_param("ifconfig %s up", WIRE_INTERFACE_NAME);

    if(Check_Link_Status(WIRE_INTERFACE_NAME))
        net_add_broardcast_route(WIRE_INTERFACE_NAME);
    else if(is_network_interface_up(net_get_wireless_name()))
        net_add_broardcast_route(net_get_wireless_name());


    return 0;

    char ifname[64] = {0};
    sn_get_ifname(ifname);
    if( strlen(ifname) > 0 )
    {
        sn_broadcast_if_route_add(ifname);
    }

    return 0;
}

unsigned long long sn_get_runtime(void)
{
	struct timespec runtime;
	if(0 > clock_gettime(CLOCK_MONOTONIC, &runtime))
	{
		__ERR("clock gettime err: %s\n", strerror(errno));
		return 0;
	}

	unsigned long long part_tv_sec = (unsigned long long)runtime.tv_sec*1000;
	unsigned long long part_tv_nsec = (unsigned long long)runtime.tv_nsec/1000000;
	unsigned long long mSeconds = part_tv_sec + part_tv_nsec;
	return mSeconds;
}

#define FIRMWARE_PEPPER "Anjoy@Secure#2026!_" 

/**
 * @brief  根据序列号和芯片UUID动态生成12位高强度设备密码
 * @param  sn:          输入参数，摄像机序列号字符串（以'\0'结尾）
 * @param  uuid:        输入参数，芯片UUID字符串（以'\0'结尾）
 * @param  out_password:输出参数，保存生成的12位密码（⚠️注意：调用者提供的缓冲区长度至少需要 13 字节）
 */
void sn_password(const char* sn, const char* uuid, char* out_password) {
    char mixed_str[256] = {0};
    unsigned char hash[32];        // SHA-256 输出 32 字节二进制
    char hex_hash[65] = {0}; // 转换为 64 字节十六进制字符串 + '\0'
    
    int i = 0, j = 0, k = 0;
    int sn_len = strlen(sn);
    int uuid_len = strlen(uuid);
    
    // 步骤 1：交叉融合（Interleaving）SN 和 UUID
    while (i < sn_len || j < uuid_len) {
        if (i < sn_len) {
            mixed_str[k++] = sn[i++];
            if (k >= 230) break; // 防止缓冲区溢出（留出空间给 Pepper）
        }
        if (j < uuid_len) {
            mixed_str[k++] = uuid[j++];
            if (k >= 230) break;
        }
    }
    mixed_str[k] = '\0';
    
    // 步骤 2：追加固件静态盐值
    strcat(mixed_str, FIRMWARE_PEPPER);
    
    // 步骤 3：计算 SHA-256 哈希
    sha256_ctx ctx;
    sn_sha256_init(&ctx);
    sn_sha256_update(&ctx, (unsigned char*)mixed_str, strlen(mixed_str));
    sn_sha256_final(&ctx, hash);
    
    // 步骤 4：将二进制哈希转为十六进制文本字符串 (长 64 字节)
    for (int m = 0; m < 32; m++) {
        sprintf(&hex_hash[m * 2], "%02x", hash[m]);
    }
    
    // 步骤 5：动态计算截取偏移量（Offset）
    // hex_hash[63] 取模 16，offset 范围 0~15。
    // 起始点最大为 15，截取 12 位，最大触及第 27 位，距离 64 字节尾部非常安全。
    int offset = hex_hash[63] % 16; 
    
    // 步骤 6：截取 12 位字符作为基础密码 (修改此处：8 -> 12)
    strncpy(out_password, &hex_hash[offset], 12);
    out_password[12] = '\0'; // 确保字符串正确结束
    
    // 步骤 7：动态字母大小写变换 (修改此处：循环上限 8 -> 12)
    // 规则：将偶数索引位置上的小写字母转换为大写字母，进一步打乱规律
    for (int n = 0; n < 12; n++) {
        if ((n % 2 == 0) && (out_password[n] >= 'a' && out_password[n] <= 'z')) {
            out_password[n] -= 32; // ASCII 码转大写
        }
    }
}
