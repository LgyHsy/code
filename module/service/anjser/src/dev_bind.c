#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_ser.h"
#include "anj_base64.h"

#include "cJSON.h"

#define TCP_SEND_BUF_LEN 1024
#define TCP_RECV_BUF_LEN 1024

const int TCP_HEADER_TOTAL_LEN = 24;
const int TCP_HEADER_MAGIC_LEN = 16;
const char TCP_HEADER_MAGIC[32] = "\x77\x77\x77\x2e\x61\x6e\x6a\x76\x69\x73\x69\x6f\x6e\x2e\x63\x6e";
const char *authAppKey_aj = "70b107ced97948beb72d3f98b3c964da";
const char *authAppSecret_aj = "426676df53594e1fb1494103c4891081";
#if 0
//测试环境
#define SERVER_DEFAULT_ADDR "ac18protest.icamra.com"
#define SERVER_DEFAULT_ADDR_EN "ac18protest.icamra.com"
#else
// 正式环境
#define SERVER_DEFAULT_ADDR "device.icamra.com"
#define SERVER_DEFAULT_ADDR_EN "device_en.icamra.com"
#endif

#define SERVER_DEFAULT_PORT "7000"

#define LOCATION_SERVER_DEFAULT_ADDR "anycam.aiot99.com"
#define LOCATION_SERVER_DEFAULT_PORT "80"
#define LOCATION_RC4_KEY "AnjiaSmartlink2024"

static void dev_bind_rc4_transfer(const char *key, char *inbuf, char *outbuf, int length)
{
    unsigned char j = 0;
    int i = 0;
    int keylen = (int)strlen(key);
    int buflen = length;
    unsigned char key2[256] = {0};
    unsigned char perm[256] = {0};
    unsigned char index1 = 0;
    unsigned char index2 = 0;

    for (i = 0; i < 256; i++)
    {
        key2[i] = (unsigned char)key[i % keylen];
        perm[i] = (unsigned char)i;
    }

    for (j = 0, i = 0; i < 256; i++)
    {
        j = (unsigned char)((j + perm[i] + key2[i]) % 256);
        unsigned char tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    index1 = 0;
    index2 = 0;
    for (i = 0; i < buflen; i++)
    {
        index1 = (unsigned char)((index1 + 1) % 256);
        index2 = (unsigned char)((index2 + perm[index1]) % 256);

        unsigned char tmp = perm[index1];
        perm[index1] = perm[index2];
        perm[index2] = tmp;

        j = (unsigned char)((perm[index1] + perm[index2]) % 256);
        outbuf[i] = inbuf[i] ^ perm[j];
    }
}

static int ConnectWithTimeout(unsigned int nServerIp, int port, struct timeval tv)
{
    struct in_addr addr;
    memcpy(&addr.s_addr, &nServerIp, sizeof(unsigned int));

    if (tv.tv_sec == 0 && tv.tv_usec == 0)
    {
        tv.tv_usec = 500 * 1000;
    }
    else if (tv.tv_sec > 30)
    {
        tv.tv_sec = 30;
    }

    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    memcpy(&address.sin_addr.s_addr, &nServerIp, sizeof(unsigned int));
    if (address.sin_addr.s_addr == INADDR_NONE)
        return -1;

    int fd = -1;
    if (-1 == (fd = socket(PF_INET, SOCK_STREAM, 0)))
    {
        return -1;
    }

    int on = 0;
    // 禁止加强型nagle算法
    on = 0;
    setsockopt(fd, SOL_TCP, TCP_CORK, (char *)&on, sizeof(on));
    // 禁止nagle算法
    on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    // set the socket in non-blocking
    int flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (flags == -1)
    {
        __ERR("fcntl O_NONBLOCK failed with error: %d\n", flags);
    }

    int connected = connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (connected != 0)
    {
        if (errno != EINPROGRESS)
        {
            __ERR("connect error :%s\n", strerror(errno));
            close(fd);
            return -1;
        }
    }

    fd_set Write, Err;
    FD_ZERO(&Write);
    FD_ZERO(&Err);
    FD_SET(fd, &Write);
    FD_SET(fd, &Err);

    // check if the socket is ready
    int res = select(fd + 1, NULL, &Write, &Err, &tv);
    if (res < 0)
    {
        __ERR("connect to server error, select failed: %s\n", strerror(errno));
    }
    else if (res == 0)
    {
        __ERR("connect to server timeout\n");
    }
    else if (1 == res)
    {
        if (FD_ISSET(fd, &Write))
        {
            __ERR("connect to server OK\n");
            // restart the socket mode
            flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
            if (flags == -1)
            {
                __ERR("fcntl ~O_NONBLOCK failed with error: %d\n", flags);
            }

            return fd;
        }
        else
        {
            __ERR("other error when select:%s\n", strerror(errno));
        }
    }

    close(fd);

    return -1;
}

int tcp_recv(int fd, char *pRecvbuffer, uint32_t nInputBuflen)
{
    struct timeval wait_time;
    int maxFd = 0;
    int selectret = 0;
    fd_set readSet;
    maxFd = fd;

    int recvlen = 0;
    int nTimes = 0;
    while (nTimes++ < 10)
    {
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);

        wait_time.tv_sec = 1;
        wait_time.tv_usec = 0;
        do
        {
            selectret = select(maxFd + 1, &readSet, NULL, NULL, &wait_time);
        } while (selectret < 0 && EINTR == errno);

        int bRecvFinished = 0;
        if (selectret == 0)
        {
            __ERR("select time out\n");
            if (recvlen > 0)
            {
                bRecvFinished = 1;
            }
            else
            {
                continue;
            }
        }
        else if (selectret > 0)
        {
            int offset = recvlen;
            int buflen = nInputBuflen - offset;
            if (buflen <= 0)
            {
                bRecvFinished = 1;
            }
            else
            {
                int retsize = safe_recv(fd, pRecvbuffer + offset, buflen, 0);
                __ERR("tcp receive data length %d\n", retsize);
                if (retsize > 0) // 还有数据
                {
                    recvlen += retsize;
                }
                else if (retsize == 0) // 收完
                {
                    bRecvFinished = 1;
                }
                else // 对端关闭了
                {
                    bRecvFinished = 1;
                }
            }
        }
        else
        {
            __ERR("select < 0.\n");
            continue;
        }

        if (bRecvFinished)
        {
            __ERR("receive data total len=%d\n", recvlen);
            break;
        }
    }

    return recvlen;
}

static char *AnjGetAccessToken(int fd)
{
    char buffer[TCP_SEND_BUF_LEN] = {0};
    char *result = NULL;
    char *accessToken = anj_mw_malloc(1024);
    if (accessToken == NULL)
    {
        __ERR("malloc failed!\n");
        return NULL;
    }
    memset(accessToken, 0, 1024);

    memcpy(buffer, TCP_HEADER_MAGIC, TCP_HEADER_MAGIC_LEN);
    uint32_t *pType = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN];
    uint32_t *pLength = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN + 4];
    char *pPacket = &buffer[TCP_HEADER_TOTAL_LEN];
    snprintf(pPacket, sizeof(buffer) - TCP_HEADER_TOTAL_LEN - 1,
             "{\"clientId\":\"%s\", \"clientSecret\":\"%s\" }", authAppKey_aj, authAppSecret_aj);
    *pType = htonl(1);
    *pLength = htonl(strlen(pPacket));
    int datalen = strlen(pPacket) + TCP_HEADER_TOTAL_LEN;
    int sendlen = safe_send(fd, buffer, datalen, 0);
    if (sendlen <= 0)
    {
        __ERR("send datalen %d failed %d.\n", datalen, sendlen);
        return NULL;
    }

    char recvbuffer[TCP_RECV_BUF_LEN] = {0};
    int recvlen = tcp_recv(fd, recvbuffer, TCP_RECV_BUF_LEN);
    if (recvlen > TCP_HEADER_TOTAL_LEN)
    {
        result = &recvbuffer[TCP_HEADER_TOTAL_LEN];
    }
    else
    {
        __ERR("recvlen %d error\n", recvlen);
        return NULL;
    }

    //********************************************************************************************************************************
    __ERR("recv result:%s \n", result);
    cJSON *root = cJSON_Parse(result);
    do
    {
        if (root == NULL)
        {
            __ERR("Parse response fail!%s\n", result);
            return accessToken;
        }
        cJSON *codeObj = cJSON_GetObjectItem(root, "code");
        if (codeObj == NULL)
        {
            __ERR("auth get code fail!\n");
            break;
        }
        int code = codeObj->valueint;
        if (code != 200)
        {
            __ERR("auth get code %d!\n", code);
            break;
        }

        cJSON *dataObj = cJSON_GetObjectItem(root, "data");
        cJSON *accessTokenObj = cJSON_GetObjectItem(dataObj, "accessToken");
        if (accessTokenObj == NULL || accessTokenObj->valuestring == NULL)
        {
            __ERR("auth get accessToken fail!\n");
            break;
        }
        strncpy(accessToken, accessTokenObj->valuestring, 1024);
    } while (0);
    cJSON_Delete(root);

    if (strlen(accessToken) == 0)
    {
        __ERR("accessToken empty!\n");
    }

    return accessToken;
}

int ProcReportResult(const char *result)
{
    int iRet = -1;
    anj_ser_info *pstSetInfo = getSerInfo();
    cJSON *root = cJSON_Parse(result);
    do
    {
        if (root == NULL)
        {
            __ERR("Parse response fail!%s\n", result);
            return iRet;
        }
        cJSON *codeObj = cJSON_GetObjectItem(root, "code");
        if (codeObj == NULL)
        {
            __ERR("get code fail!\n");
            break;
        }
        int code = codeObj->valueint;
        if (code != 200)
        {
            __ERR("bind get code %d!\n", code);
            break;
        }
        iRet = 0;
        snprintf(pstSetInfo->bindRecvBuffer, sizeof(pstSetInfo->bindRecvBuffer), "%s", result);
    } while (0);
    cJSON_Delete(root);

    return iRet;
}

static void FillDevReportBuffer(const char *accessToken, char *buffer, size_t buflen,
                                char *product_key, char *device_name, char *partner,
                                char *sn, char *uuid, char *szIccid, char *szMsisdn)
{
    if (accessToken == NULL || buffer == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }

    char szDeviceType[64] = ANJ_PROJECT_NAME;
    AjOemStruct oemInfo = {0};
    memset(buffer, 0, buflen);
    anj_config_oem_get(&oemInfo);

    if (strlen(oemInfo.szDeviceType) > 0)
    {
        strncpy(szDeviceType, oemInfo.szDeviceType, sizeof(szDeviceType) - 1);
    }

    snprintf(buffer, buflen,
             "{\"accessToken\":\"%s\", \"productKey\":\"%s\", \"deviceName\":\"%s\","
             " \"partnerId\":\"%s\", \"sn\":\"%s\", \"uuid\":\"%s\", \"oemModel\":\"%s\"",
             accessToken, product_key, device_name, partner, sn, uuid, szDeviceType);

    if (szIccid && szMsisdn && strlen(szIccid) && strlen(szMsisdn))
    {
        strncat(buffer, ", \"iccid\":\"", buflen - strlen(buffer) - 1);
        strncat(buffer, szIccid, buflen - strlen(buffer) - 1);
        strncat(buffer, "\", \"msisdn\":\"", buflen - strlen(buffer) - 1);
        strncat(buffer, szMsisdn, buflen - strlen(buffer) - 1);
        strncat(buffer, "\"", buflen - strlen(buffer) - 1);
    }
    strncat(buffer, "}", buflen - strlen(buffer) - 1);
}

int dev_bind_task(const char *product_key, const char *device_name, const char *accountName, const char *client_code,
                  char *svraddr, char *svrport)
{
    int iRet = 0;
    int fd = -1;
    char *result = NULL;
    char *accessToken = NULL;
    ANJ_CHK(((client_code != NULL)), -1, "input invalid!");

    int china = 1;
    if (strncmp(client_code, "A", 1) != 0)
        china = 0;

    char dstAddr[32] = {0};
    char dstPort[32] = {0};
    if (svraddr)
    {
        snprintf(dstAddr, sizeof(dstAddr), "%s", svraddr);
    }
    else
    {
        if (china)
            snprintf(dstAddr, sizeof(dstAddr), "%s", SERVER_DEFAULT_ADDR);
        else
            snprintf(dstAddr, sizeof(dstAddr), "%s", SERVER_DEFAULT_ADDR_EN);
    }
    if (svrport)
    {
        snprintf(dstPort, sizeof(dstPort), "%s", svrport);
    }
    else
    {
        snprintf(dstPort, sizeof(dstPort), "%s", SERVER_DEFAULT_PORT);
    }

    int ncount = 0;
    unsigned int destIp = 0;
    while (ncount++ < 10)
    {
        destIp = WS_getIpFromName(dstAddr);
        if (destIp != 0)
            break;

        ncount++;
        if (ncount % 10 == 0)
        {
            __ERR("Get server ip from %s error.\n", dstAddr);
        }
        usleep(1000 * 100);
        continue;
    }

    if (destIp == 0)
    {
        __ERR("resolve %s failed\n", dstAddr);
        iRet = -1;
        goto endFunc;
    }
    __ERR("Device bind connect svraddr:%s \n", dstAddr);

    unsigned short destPort = atoi(dstPort);

    struct timeval tv_timeout;
    tv_timeout.tv_sec = 5;
    tv_timeout.tv_usec = 0;

    fd = ConnectWithTimeout(destIp, destPort, tv_timeout);
    if (fd < 0)
    {
        __ERR("connect %s:%u failed\n", dstAddr, destPort);
        iRet = -1;
        goto endFunc;
    }

    accessToken = AnjGetAccessToken(fd);
    ANJ_CHK(((accessToken != NULL)), -1, "accessToken get failed!");

    char buffer[TCP_SEND_BUF_LEN];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, TCP_HEADER_MAGIC, TCP_HEADER_MAGIC_LEN);
    uint32_t *pType = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN];
    uint32_t *pLength = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN + 4];
    char *pPacket = &buffer[TCP_HEADER_TOTAL_LEN];

    snprintf(pPacket, sizeof(buffer) - TCP_HEADER_TOTAL_LEN,
             "{\"accessToken\":\"%s\", \"productKey\":\"%s\", \"deviceName\":\"%s\", \"accountName\":\"%s\", \"clientCode\":\"%s\", \"channels\":%d, \"gidType\":%d}",
             accessToken, product_key, device_name, accountName, client_code, ANJ_CAMERA_MAX_NUMS, ((ANJ_CAMERA_MAX_NUMS > 1) ? 2 : 0));

    *pType = htonl(201);
    *pLength = htonl(strlen(pPacket));
    int datalen = strlen(pPacket) + TCP_HEADER_TOTAL_LEN;
    int sendlen = safe_send(fd, buffer, datalen, 0);
    if (sendlen <= 0)
    {
        __ERR("send datalen %d failed %d.\n", datalen, sendlen);
        iRet = -1;
        goto endFunc;
    }
    __ERR("send datalen %d return %d.\n", datalen, sendlen);

    char recvbuffer[TCP_RECV_BUF_LEN];
    memset(recvbuffer, 0, sizeof(recvbuffer));
    int recvlen = tcp_recv(fd, recvbuffer, TCP_RECV_BUF_LEN);
    if (recvlen > TCP_HEADER_TOTAL_LEN)
    {
        result = &recvbuffer[TCP_HEADER_TOTAL_LEN];
    }
    else
    {
        __ERR("recvlen %d error\n", recvlen);
        iRet = -1;
        goto endFunc;
    }
    __ERR("recv result:%s \n", result);
    iRet = ProcReportResult(result);
    if (iRet < 0)
    {
        __INFO("bind failed \n");
    }
    else
    {
        __INFO("bind success\n");
    }

endFunc:
    if (fd > 0)
    {
        close(fd);
    }
    if (accessToken)
    {
        anj_mw_free(accessToken);
    }
    return iRet;
}

// int dev_unbind_task(const char *product_key, const char *device_name)
// {
//     const char *AnjTenantId = "Anjvision";
//     if (product_key && strcmp("a1JhXCOT4L6", product_key) == 0)
//     {
//         /* skyworth special handling (kept logic, converted to C) */
//         char buffer[TCP_SEND_BUF_LEN] = {0};
//         const char *authAppKey = "57634645";
//         const char *authAppSecret = "10508e45e9598d74";
//         const char *devTenantId = "1270904915438469122";

//         snprintf(buffer, sizeof(buffer) - 1, "{\"appKey\":\"%s\", \"appSecret\":\"%s\", \"scope\":\"all\", \"grant_type\":\"password\"}",
//                  authAppKey, authAppSecret);

//         const char *hdr1 = "Content-Type: application/json;charset=UTF-8";
//         const char *hdr2 = "Authorization: Basic dGhpcmRwYXJ0eWFwcGtleTp0aGlyZHBhcnR5YXBwa2V5X3NlY3JldA==";
//         char *result = http_post_plaintext("https://app.skyworthdigitaliot.com/skyworthNorthbound/appKeyManage/auth",
//                                            (const char *[]){hdr1, hdr2}, 2, buffer, 90 * 1000);
//         if (!result)
//             return -1;

//         char *accessToken = NULL;
//         cJSON *root = cJSON_Parse(result);
//         if (root)
//         {
//             cJSON *codeObj = cJSON_GetObjectItem(root, "code");
//             if (codeObj && codeObj->valueint == 200)
//             {
//                 cJSON *dataObj = cJSON_GetObjectItem(root, "data");
//                 if (dataObj)
//                 {
//                     cJSON *accessTokenObj = cJSON_GetObjectItem(dataObj, "access_token");
//                     if (accessTokenObj && accessTokenObj->valuestring)
//                         accessToken = strdup(accessTokenObj->valuestring);
//                 }
//             }
//             cJSON_Delete(root);
//         }
//         anj_mw_free(result);
//         if (!accessToken)
//             return -1;

//         char bladeAuth[512];
//         snprintf(bladeAuth, sizeof(bladeAuth), "Blade-Auth: bearer %s", accessToken);
//         const char *headers_arr2[] = {hdr1, "Authorization: Basic YW5qaWF3ZWlzaGlpcGM6YW5qaWF3ZWlzaGlpcGNfc2VjcmV0", bladeAuth};

//         char unbindUrl[512];
//         snprintf(unbindUrl, sizeof(unbindUrl), "https://app.skyworthdigitaliot.com/skyworthNorthbound/aliyun/forceDeviceUnbind?devTenantId=%s&productKey=%s&deviceNames=%s",
//                  devTenantId, product_key ? product_key : "", device_name ? device_name : "");

//         result = http_post_plaintext(unbindUrl, headers_arr2, 3, "", 4 * 1000);
//         anj_mw_free(accessToken);
//         if (!result)
//             return -1;

//         int success = 0;
//         root = cJSON_Parse(result);
//         if (root)
//         {
//             cJSON *codeObj = cJSON_GetObjectItem(root, "code");
//             if (codeObj && codeObj->valueint == 200)
//                 success = 1;
//             cJSON_Delete(root);
//         }
//         anj_mw_free(result);
//         return success ? 0 : -1;
//     }
//     else
//     {
//         /* ac18pro unbind */
//         char buffer[TCP_SEND_BUF_LEN];
//         char *accessToken = AnjGetAccessToken(1);
//         if (!accessToken)
//         {
//             printf("accessToken get failed.\n");
//             return -1;
//         }

//         char anjTokenHeader[512];
//         char authHeader[512];
//         const char *contentHeader = "Content-Type: application/json;charset=UTF-8";
//         snprintf(authHeader, sizeof(authHeader), "Authorization: %s", authAppKey_aj);
//         snprintf(anjTokenHeader, sizeof(anjTokenHeader), "AnjToken: %s", accessToken);
//         const char *headers_arr[3] = {contentHeader, authHeader, anjTokenHeader};

//         snprintf(buffer, sizeof(buffer) - 1, "{\"AnjTenantId\":\"%s\", \"productKey\":\"%s\", \"deviceNames\":[\"%s\"]}",
//                  AnjTenantId, product_key ? product_key : "", device_name ? device_name : "");

//         char *result = http_post_plaintext("http://ac18pro.icamra.com/api/device/unbind", headers_arr, 3, buffer, 4 * 1000);
//         anj_mw_free(accessToken);
//         if (!result)
//             return -1;

//         int success = 0;
//         cJSON *root = cJSON_Parse(result);
//         if (root)
//         {
//             cJSON *codeObj = cJSON_GetObjectItem(root, "code");
//             if (codeObj && codeObj->valueint == 200)
//                 success = 1;
//             cJSON_Delete(root);
//         }
//         anj_mw_free(result);
//         return success ? 0 : -1;
//     }
// }

int dev_bind_report(char *szPartner, char *product_key, char *device_name, char *szIccid, char *szMsisdn,
                    char *svraddr, char *svrport)
{
    int iRet = 0;
    int fd = -1;
    char *result = NULL;
    char *accessToken = NULL;
    DevInfo *pstDevInfo = getDevInfo();
    ANJ_CHK(((product_key != NULL) && (device_name != NULL) && (strlen(product_key) > 0) && (strlen(device_name) > 0)),
            -1, "p2pid absence.");
    ANJ_CHK(((szPartner != NULL) && (strlen(szPartner) > 0)), -1, "partner absence.");
    ANJ_CHK(((strlen(pstDevInfo->sn) > 0) && (strlen(pstDevInfo->uuid) > 0)), -1, "sn or uuid absence.");

    char dstAddr[32] = {0};
    char dstPort[32] = {0};
    if (svraddr)
    {
        snprintf(dstAddr, sizeof(dstAddr), "%s", svraddr);
    }
    else
    {
        snprintf(dstAddr, sizeof(dstAddr), "%s", SERVER_DEFAULT_ADDR);
    }
    if (svrport)
    {
        snprintf(dstPort, sizeof(dstPort), "%s", svrport);
    }
    else
    {
        snprintf(dstPort, sizeof(dstPort), "%s", SERVER_DEFAULT_PORT);
    }

    int ncount = 0;
    unsigned int destIp = 0;
    while (ncount++ < 10)
    {
        destIp = WS_getIpFromName(dstAddr);
        if (destIp != 0)
            break;

        ncount++;
        if (ncount % 10 == 0)
        {
            __ERR("Get server ip from %s error.\n", dstAddr);
        }
        usleep(1000 * 100);
        continue;
    }

    if (destIp == 0)
    {
        __ERR("resolve %s failed\n", dstAddr);
        iRet = -1;
        goto endFunc;
    }

    unsigned short destPort = atoi(dstPort);
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 5;
    tv_timeout.tv_usec = 0;

    fd = ConnectWithTimeout(destIp, destPort, tv_timeout);
    if (fd < 0)
    {
        __ERR("connect %s:%u failed\n", dstAddr, destPort);
        iRet = -1;
        goto endFunc;
    }

    accessToken = AnjGetAccessToken(fd);
    ANJ_CHK(((accessToken != NULL)), -1, "accessToken get failed!");

    char buffer[TCP_SEND_BUF_LEN];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, TCP_HEADER_MAGIC, TCP_HEADER_MAGIC_LEN);
    uint32_t *pType = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN];
    uint32_t *pLength = (uint32_t *)&buffer[TCP_HEADER_MAGIC_LEN + 4];
    char *pPacket = &buffer[TCP_HEADER_TOTAL_LEN];

    FillDevReportBuffer(accessToken, pPacket, sizeof(buffer) - TCP_HEADER_TOTAL_LEN,
                        product_key, device_name, szPartner, pstDevInfo->sn,
                        pstDevInfo->uuid, szIccid, szMsisdn);

    *pType = htonl(101);
    *pLength = htonl(strlen(pPacket));
    int datalen = strlen(pPacket) + TCP_HEADER_TOTAL_LEN;
    __ERR("send data:%s\n", pPacket);

    int sendlen = safe_send(fd, buffer, datalen, 0);
    if (sendlen <= 0)
    {
        __ERR("send datalen %d failed %d.\n", datalen, sendlen);
        iRet = -1;
        goto endFunc;
    }
    __ERR("send datalen %d return %d.\n", datalen, sendlen);

    char recvbuffer[TCP_RECV_BUF_LEN];
    memset(recvbuffer, 0, sizeof(recvbuffer));
    int recvlen = tcp_recv(fd, recvbuffer, TCP_RECV_BUF_LEN);
    if (recvlen > TCP_HEADER_TOTAL_LEN)
    {
        result = &recvbuffer[TCP_HEADER_TOTAL_LEN];
    }
    else
    {
        __ERR("recvlen %d error\n", recvlen);
        iRet = -1;
        goto endFunc;
    }

    //********************************************************************************************************************************
    __ERR("recv result:%s \n", result);
    iRet = ProcReportResult(result);

    if (iRet < 0)
    {
        __INFO("report failed \n");
    }
    else
    {
        __INFO("report success\n");
    }

endFunc:
    if (fd > 0)
    {
        close(fd);
    }
    if (accessToken)
    {
        anj_mw_free(accessToken);
    }
    return iRet;
}

int dev_bind_location(const char *url, const char *lac, const char *ci, const char *mnc,
                      const char *imei, const char *devid, const char *ua, const char *type,
                      char *svraddr, char *svrport)
{
    int iRet = 0;
    int fd = -1;
    int iLac = 0;
    int iCi = 0;
    char json_buf[512] = {0};
    char rc4_buf[512] = {0};
    char szBody[1024] = {0};
    char buffer[2048] = {0};
    char recvbuffer[TCP_RECV_BUF_LEN] = {0};
    char dstAddr[64] = {0};
    char dstPort[32] = {0};
    const char *mncStr = (mnc != NULL) ? mnc : "";
    const char *imeiStr = (imei != NULL) ? imei : "";

    ANJ_CHK(((url != NULL) && (strlen(url) > 0)), -1, "url absence.");
    ANJ_CHK(((lac != NULL) && (strlen(lac) > 0)), -1, "lac absence.");
    ANJ_CHK(((ci != NULL) && (strlen(ci) > 0)), -1, "ci absence.");
    ANJ_CHK(((devid != NULL) && (strlen(devid) > 0)), -1, "device_name absence.");

    if (svraddr && strlen(svraddr) > 0)
    {
        snprintf(dstAddr, sizeof(dstAddr), "%s", svraddr);
    }
    else
    {
        snprintf(dstAddr, sizeof(dstAddr), "%s", LOCATION_SERVER_DEFAULT_ADDR);
    }
    if (svrport && strlen(svrport) > 0)
    {
        snprintf(dstPort, sizeof(dstPort), "%s", svrport);
    }
    else
    {
        snprintf(dstPort, sizeof(dstPort), "%s", LOCATION_SERVER_DEFAULT_PORT);
    }

    sscanf(lac, "%x", &iLac);
    sscanf(ci, "%x", &iCi);

    if (ua != NULL && strlen(ua) > 0 && type != NULL && strlen(type) > 0)
    {
        snprintf(json_buf, sizeof(json_buf),
                 "{\"CellId\": %d,"
                 "\"LAC\": %d,"
                 "\"MCC\": 460,"
                 "\"MNC\": %s,"
                 "\"IMEI\": \"%s\","
                 "\"Signal\":-60,"
                 "\"Cage\":0,"
                 "\"IsCDMA\":0,"
                 "\"DevNo\": \"%s\","
                 "\"Ua\": \"%s\","
                 "\"Type\": %d,"
                 "\"Network\": \"GSM\"}",
                 iCi, iLac, mncStr, imeiStr, devid, ua, atoi(type));
    }
    else
    {
        snprintf(json_buf, sizeof(json_buf),
                 "{\"CellId\": %d,"
                 "\"LAC\": %d,"
                 "\"MCC\": 460,"
                 "\"MNC\": %s,"
                 "\"IMEI\": \"%s\","
                 "\"Signal\":-60,"
                 "\"Cage\":0,"
                 "\"IsCDMA\":0,"
                 "\"DevNo\": \"%s\","
                 "\"Network\": \"GSM\"}",
                 iCi, iLac, mncStr, imeiStr, devid);
    }

    __ERR("json=(%s)\n", json_buf);

    {
        int rc4len = (int)strlen(json_buf);
        dev_bind_rc4_transfer(LOCATION_RC4_KEY, json_buf, rc4_buf, rc4len);
        if (anj_base64_encode((uint8_t *)rc4_buf, (uint32_t)rc4len, szBody, sizeof(szBody)) != 1)
        {
            __ERR("base64 encode failed\n");
            iRet = -1;
            goto endFunc;
        }
    }

    int ncount = 0;
    unsigned int destIp = 0;
    while (ncount++ < 10)
    {
        destIp = WS_getIpFromName(dstAddr);
        if (destIp != 0)
            break;

        ncount++;
        if (ncount % 10 == 0)
        {
            __ERR("Get server ip from %s error.\n", dstAddr);
        }
        usleep(1000 * 100);
        continue;
    }

    if (destIp == 0)
    {
        __ERR("resolve %s failed\n", dstAddr);
        iRet = -1;
        goto endFunc;
    }

    unsigned short destPort = (unsigned short)atoi(dstPort);
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 5;
    tv_timeout.tv_usec = 0;

    fd = ConnectWithTimeout(destIp, destPort, tv_timeout);
    if (fd < 0)
    {
        __ERR("connect %s:%u failed\n", dstAddr, destPort);
        iRet = -1;
        goto endFunc;
    }

    snprintf(buffer, sizeof(buffer),
             "POST %s HTTP/1.1\r\n"
             "Accept: */*\r\n"
             "Accept-Encoding: identity\r\n"
             "Host: anycam.aiot99.com\r\n"
             "Content-Length: %d\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
             "%s",
             url, (int)strlen(szBody), szBody);

    int datalen = (int)strlen(buffer);
    __ERR("http msg (%s)\n", buffer);

    int sendlen = safe_send(fd, buffer, datalen, 0);
    if (sendlen <= 0)
    {
        __ERR("send datalen %d failed %d.\n", datalen, sendlen);
        iRet = -1;
        goto endFunc;
    }
    __ERR("send datalen %d return %d.\n", datalen, sendlen);

    int recvlen = tcp_recv(fd, recvbuffer, TCP_RECV_BUF_LEN);
    __ERR("recv len=%d, recvbuffer=%s\n", recvlen, recvbuffer);

    char *p = strstr(recvbuffer, "\r\n\r\n");
    if (p == NULL)
    {
        __ERR("upload loc info fail\n");
        iRet = -1;
        goto endFunc;
    }

    {
        char decbuf[1024] = {0};
        uint8_t pDecData[1024] = {0};
        unsigned srclen = (unsigned)strlen(p + 4);
        int nDecSize = anj_base64_decode(p + 4, srclen, pDecData, sizeof(pDecData) - 1);
        if (nDecSize < 0)
        {
            __ERR("base64Decode fail\n");
            iRet = -1;
            goto endFunc;
        }

        dev_bind_rc4_transfer(LOCATION_RC4_KEY, (char *)pDecData, decbuf, nDecSize);
        decbuf[nDecSize < (int)sizeof(decbuf) - 1 ? nDecSize : (int)sizeof(decbuf) - 1] = 0;
        __ERR("dec = %s\n", decbuf);

        if (strstr(decbuf, "200") == NULL)
        {
            __ERR("upload loc info fail\n");
            iRet = -1;
            goto endFunc;
        }
    }

    __ERR("upload loc info success\n");
    iRet = 0;

endFunc:
    if (fd > 0)
    {
        close(fd);
    }
    return iRet;
}

int dev_bind_check_iccid_cs(char *szIccidList)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK(((szIccidList != NULL) && (strlen(szIccidList) > 0)), 0, "input invalid!");

    anj_ser_info *pstSetInfo = getSerInfo();
    root = cJSON_Parse(pstSetInfo->bindRecvBuffer);
    ANJ_CHK((root != NULL), 0, "cJSON_Parse failed!");

    int i = 0;
    char szIccid[IPC_4G_SIMCARD_NUM][G4_STR_LEN_256];
    char *token = strtok(szIccidList, ",");
    while (token && i < IPC_4G_SIMCARD_NUM)
    {
        if (strlen(token) > 0)
        {
            snprintf(szIccid[i], G4_STR_LEN_256, "%s", token);
            i++;
        }
        token = strtok(NULL, ",");
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data)
    {
        cJSON *iccids_array = cJSON_GetObjectItem(data, "iccids");
        if (iccids_array && cJSON_IsArray(iccids_array))
        {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, iccids_array)
            {
                cJSON *iccid = cJSON_GetObjectItem(item, "iccid");
                cJSON *cskind = cJSON_GetObjectItem(item, "csKind");

                if (iccid && cJSON_IsString(iccid) && iccid->valuestring)
                {
                    for (int j = 0; j < i; j++)
                    {
                        if (strcmp(szIccid[j], iccid->valuestring) == 0)
                        {
                            if (cskind && cJSON_IsString(cskind) &&
                                cskind->valuestring && strlen(cskind->valuestring) > 0)
                            {
                                iRet = 1; // 找到了且有csKind
                            }
                            else
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                if (iRet == 0)
                    break;
            }
        }
    }
endFunc:
    if (root)
        cJSON_Delete(root);

    return iRet;
}
