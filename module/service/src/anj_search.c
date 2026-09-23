/** ===========================================================================
 * @file searchipc.c
 *
 * @path $(IPNCPATH)\sys_adm\system_server
 *
 * @desc
 * .
 * Copyright (c) Anjoy Vision Information Technology Co.,Ltd. 2008
 *
 * Use of this software is controlled by the terms and conditions found
 * in the license agreement under which this software has been supplied
 *
 * =========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

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
#include <netinet/tcp.h>
#include <poll.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <termios.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_crypt.h"
#include "anj_service.h"
#include "anj_factory.h"
#include "anj_sysmng.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_bind.h"
#include "anj_ser.h"
#include "anj_search.h"
#include "record_log.h"

#include "eventhub.h"

static anj_thread_s s_stSearchThread = {0};

#define BROADCASTING_PORT 3001

typedef struct _USER_CONFIG
{
	int allNetEnable;
	int allNetSet;
} USER_CONFIG;

enum
{
	IPC_MESSAGE_SEARCHIPC = 1,
	IPC_MESSAGE_MODIFYIPC = 2,

	IPC_MESSAGE_SEARCHIPC_RESPONSE = 3,
	IPC_MESSAGE_MODIFYIPC_RESPONSE = 4,

	IPC_MESSAGE_RESTORECONFIG = 5,
	IPC_MESSAGE_RESTORECONFIG_RESPONSE = 6,

	IPC_MESSAGE_REBOOT = 7,
	IPC_MESSAGE_REBOOT_RESPONSE = 8,

	IPC_MESSAGE_MODE_FACTORY = 9,
	IPC_MESSAGE_MODE_FACTORY_RESPONSE = 10,

	IPC_MESSAGE_SET_FACTORY_CFG = 11,
	IPC_MESSAGE_SET_FACTORY_CFG_RESPONSE = 12,

	IPC_MESSAGE_CLEAR_FACTORY_CFG = 13,
	IPC_MESSAGE_CLEAR_FACTORY_CFG_RESPONSE = 14,

	IPC_MESSAGE_GET_FACTORY_CFG = 15,
	IPC_MESSAGE_GET_FACTORY_CFG_RESPONSE = 16,

	IPC_MESSAGE_TEST_LED_PTZ_IRCUT = 17,
	IPC_MESSAGE_TEST_LED_PTZ_IRCUT_RESPONSE = 18,

	IPC_MESSAGE_SET_USER_CFG = 19,
	IPC_MESSAGE_SET_USER_CFG_RESPONSE = 20,

	IPC_WIRELESS_NET_PAIR = 211,
	IPC_WIRELESS_NET_PAIR_RESPONSE = 212,
	IPC_WIRELESS_PAIR_INFO_UPDATE = 213,

	IPC_WIRELESS_PAIR_TOPOLOGY_REQ = 215,
	IPC_WIRELESS_PAIR_TOPOLOGY_RSP = 216,

	IPC_MESSAGE_CLEAR_SOFT_SN = 5898,
};

static int anj_search_cmd_get(char *xmlBuf, char *vendorId, int verify_passwd, char *pvendor_id, int *msgflag, char *super_passwd)
{
	int msgcmd = -1;

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

	if (pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MESSAGE_HEADER");
	if (pNodelist != NULL)
	{
		IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
		while (tmp != NULL)
		{
			if (strcmp(tmp->nodeName, "Msg_code") == 0)
			{
				if (tmp->nodeValue != NULL)
				{
					msgcmd = atoi(tmp->nodeValue);
				}
			}
			else if (strcmp(tmp->nodeName, "Msg_flag") == 0)
			{
				if (tmp->nodeValue != NULL)
				{
					*msgflag = atoi(tmp->nodeValue);
				}
			}
			tmp = tmp->nextSibling;
		}

		ixmlNodeList_free(pNodelist);
	}
	else
	{
		ixmlDocument_free(pDocNode);
		return -1;
	}

	int i = 0;
	char username[256] = {0};
	char password[256] = {0};
	char md5Str[64] = {0};
	if (msgcmd != IPC_MESSAGE_SEARCHIPC && msgcmd != IPC_MESSAGE_SEARCHIPC_RESPONSE)
	{
		if (verify_passwd > 0)
		{
			pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "OPERATOR_AUTH");
			if (pNodelist == NULL)
			{
				__ERR("Not found auth node\n");
				ixmlDocument_free(pDocNode);
				return -100;
			}

			IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
			while (tmpAttr != NULL)
			{
				if (0 == strcmp(tmpAttr->nodeName, "Username"))
				{
					if (tmpAttr->nodeValue)
					{
						strncpy(username, tmpAttr->nodeValue, sizeof(username));
						username[sizeof(username) - 1] = '\0';
					}
				}
				else if (0 == strcmp(tmpAttr->nodeName, "Password"))
				{
					if (tmpAttr->nodeValue)
					{
						strncpy(password, tmpAttr->nodeValue, sizeof(password) - 1);
						password[sizeof(password) - 1] = '\0';
					}
				}

				tmpAttr = tmpAttr->nextSibling;
			}

			ixmlNodeList_free(pNodelist);

			if (!strlen(username) || !strlen(password))
			{
				__ERR("Not found auth info\n");
				msgcmd = -100;

				ixmlDocument_free(pDocNode);
				return msgcmd;
			}
			// PASSWORD must be MD5

			if (strcasecmp(password, super_passwd) != 0)
			{
				SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
				UserConfig *pUserCfg = &pstSystemConfig->userCfg;
				for (i = 0; i < pUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
				{
					our_md5_encode(md5Str, (unsigned char *)pUserCfg->accounts[i].password, strlen(pUserCfg->accounts[i].password));
					md5Str[32] = '\0';

					if (((0 == strcmp(pUserCfg->accounts[i].userName, username)) && (0 == strcmp(md5Str, password))) ||
						(0 == strcmp(pUserCfg->accounts[i].password, password)))
						break;
				}

				if (i == pUserCfg->count)
				{
					__ERR("verify user password failed!!!\n");

					msgcmd = -1;

					ixmlDocument_free(pDocNode);
					return -100;
				}
			}
			else
			{
				__ERR("Super password %s checked ok.\n", password);
			}
		}
	}

	// check if we have same Vendor Id, otherwise don't response this message
	char vendor_id[256] = {0};
	int iRet = 0;

	if (strlen(pvendor_id)) // read from save value
	{
		strcpy(vendor_id, pvendor_id);
		strcpy(vendorId, vendor_id);
	}
	else
	{
		if (anj_mw_file_exists("/tmp/vendor_id_md5.dat"))
		{
			iRet = anj_mw_read_file_limit_len("/tmp/vendor_id_md5.dat", vendor_id, sizeof(vendor_id));
			if (iRet > 0)
			{
				vendor_id[iRet] = '\0';
				strcpy(vendorId, vendor_id);
				strcpy(pvendor_id, vendor_id);
			}
		}
	}

	// check vendor id
	pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VENDOR_PARAM");
	if (pNodelist != NULL)
	{
		IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
		while (tmp != NULL)
		{
			//__ERR("nodeName: [%s], nodeValue: 0x%x\n", tmp->nodeName, tmp->nodeValue);
			if (!strcmp(tmp->nodeName, "VendorId") || !strcmp(tmp->nodeName, "VendorID"))
			{
				if (tmp->nodeValue)
				{
					if (!strcmp(tmp->nodeValue, vendor_id))
					{
						//__ERR("vendor_id match, nodeValue: %s\n", tmp->nodeValue);
						break;
					}
					else
					{
						//__ERR("vendor_id not match, nodeValue: %s\n", tmp->nodeValue);
					}
				}
				else
				{
					//__ERR("vendor_id value not found!!!\n");
				}

				msgcmd = -1;
				break;
			}

			tmp = tmp->nextSibling;
		}

		ixmlNodeList_free(pNodelist);
	}
	else
	{
		if (strlen(vendor_id) > 0)
		{
			//__ERR("we have vendorId: %s, but not found VENDOR_PARAM info\n", vendor_id);
			msgcmd = -1;
		}
	}

	ixmlDocument_free(pDocNode);
	return msgcmd;
}

static int anj_search_modify_sn_get(const char *xmlBuf, char *sn)
{
	strcpy(sn, "");

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if (pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	char *value = GetRequestParamValueByName(pDocNode, "IPC_SERIALNUMBER", "SerialNumber");
	ixmlDocument_free(pDocNode);

	if (value)
	{
		strcpy(sn, value);
		anj_mw_free(value);
		return 0;
	}
	else
	{
		return -1;
	}
}

static int anj_search_retain_get(const char *xmlBuf, char *retain)
{
	strcpy(retain, "");

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if (pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	char *value = GetRequestParamValueByName(pDocNode, "retain", "value");
	ixmlDocument_free(pDocNode);

	if (value)
	{
		strcpy(retain, value);
		anj_mw_free(value);
		return 0;
	}
	else
	{
		return -1;
	}
}

static int anj_search_usercfg_get(const char *xmlBuf, USER_CONFIG *pUserCfg)
{
	memset(pUserCfg, 0, sizeof(USER_CONFIG));
	NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
	pUserCfg->allNetEnable = pstNetworkConfig->lanCfg.onvifAllnetEnable;

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if (pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	char *value = GetRequestParamValueByName(pDocNode, "UserCfg", "AllNet");
	ixmlDocument_free(pDocNode);

	if (value)
	{
		pUserCfg->allNetEnable = atoi(value);
		pUserCfg->allNetSet = 1;
		anj_mw_free(value);
		return 0;
	}
	else
	{
		return -1;
	}

	return 0;
}

static int anj_search_send_remote(const int sockfd, char *send_buf, struct sockaddr_in remote)
{
	int iRet = 0;
	// send use unicast first
	iRet = safe_sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr *)&remote, sizeof(remote));

	// and then use broadcast again
	remote.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	iRet = safe_sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr *)&remote, sizeof(remote));

	if (iRet <= 0)
	{
		__INFO("safe_sendto failed, error=%d, errinfo=%s\n", errno, strerror(errno));

		// try to add route for address 255.255.255.255
		__INFO("try to add route for address 255.255.255.255\n");

		if (Check_Link_Status(WIRE_INTERFACE_NAME))
		{
			anj_mw_system("route add -host 255.255.255.255 dev eth0");
		}
		else
		{
			anj_mw_system("route add -host 255.255.255.255 dev wlan0");
		}
	}

	return iRet;
}

static const char *anj_search_xml_name_get()
{
	MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
	if (pstMediaStreamConfig->tstConfig.enable != 0)
		return XML_ROOT_NAME1;

	return ANJ_CUSTOMER_TYPE == CUSTOMER_WTD ? XML_ROOT_NAME3 : XML_ROOT_NAME2;
}

int anj_search_wifi_info_get(const char *xmlBuf, char *ssid, char *pwd)
{
	strcpy(ssid, "");
	strcpy(pwd, "");

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if (pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	char *pSsid = GetRequestParamValueByName(pDocNode, "WIFI_INFO", "ssid");
	char *pPasswd = GetRequestParamValueByName(pDocNode, "WIFI_INFO", "pass");
	ixmlDocument_free(pDocNode);

	if (pSsid)
	{
		strcpy(ssid, pSsid);
		anj_mw_free(pSsid);
		pSsid = NULL;
	}

	if (pPasswd)
	{
		strcpy(pwd, pPasswd);
		anj_mw_free(pPasswd);
		pPasswd = NULL;
	}

	if (strlen(ssid))
		return 0;
	else
		return -1;
}

static int anj_search_cmd_proc(const int sockfd, const char *vendorId, const char *sn_str,
							   char *send_buf, struct sockaddr_in remote, int verify_passwd)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	snprintf(remoteip, sizeof(remoteip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

	char strAFversion[128] = {0};
	// 判断是否为AF版本
	if (anj_mw_file_exists(AJ_APP_PATH "/af.flag") && anj_mw_file_exists("/tmp/version.flag"))
	{
		anj_mw_read_file_limit_len("/tmp/AF_version.txt", strAFversion, sizeof(strAFversion));
	}

	// mcu版本复用af版本名字
	if ((ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV) && anj_mw_file_exists("/tmp/mcu_ver.txt"))
	{
		anj_mw_read_file_limit_len("/tmp/mcu_ver.txt", strAFversion, sizeof(strAFversion));
	}

	char szPlatformID[32] = {0};

	if (anj_mw_file_exists(SERVER_NO_FILE_NAME))
	{
		anj_mw_read_file_limit_len(SERVER_NO_FILE_NAME, szPlatformID, sizeof(szPlatformID));
	}

	NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
	P2pLoginState_t p2pState = {0};
	if (pstNetworkConfig->p2pCfg.enable > 0)
	{
		anj_ser_info *pstSerInfo = getSerInfo();
		p2pState = pstSerInfo->stP2pLoginState;
	}

	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	UserConfig stUsrCfg = pstSystemConfig->userCfg;

	if (verify_passwd)
	{
		memset(&stUsrCfg, 0, sizeof(UserConfig));
		stUsrCfg.count = 1;
	}
	else
	{
		int i = 0;
		for (i = 0; i < stUsrCfg.count; i++)
		{
			strcpy(stUsrCfg.accounts[i].userName, "");
			strcpy(stUsrCfg.accounts[i].password, "");
		}
	}

	char *usercfg = strlen(vendorId)
						? anj_config_system_user_password_conver_xml(&stUsrCfg)
						: anj_config_system_user_conver_xml(&stUsrCfg, 0);

	LANConfig tmpLanCfg = {0};
	struct NET_CONFIG netcfg = {0};
	int bResponse = 1;
	const char *targetInterface = WIRE_INTERFACE_NAME;
	int iRet = 0;

	// 记录接口状态
	int wireLink = Check_Link_Status(targetInterface);
	// __ERR("Check_Link_Status %s:%d, remote %s:%d \n", targetInterface, wireLink, remoteip, ntohs(remote.sin_port));

	if (wireLink)
	{
		// 有线网络处理
		memcpy(&tmpLanCfg, &pstNetworkConfig->lanCfg, sizeof(LANConfig));

		char macBuf[6] = {0};
		net_get_hwaddr(targetInterface, (unsigned char *)macBuf);
		format_mac_addr_from_digit_to_string(macBuf, sizeof(macBuf), (char *)tmpLanCfg.MACAddress, MAC_ADDRESS_LEN);

		// 特殊序列号处理
		if (sn_str[0] == 'E' && sn_str[1] == 'E' && ANJ_CAMERA_MAX_NUMS > 1)
		{
			net_get_info("eth0:0", &netcfg);
			tmpLanCfg.MACAddress[0] = 'E';
		}
		else
		{
			net_get_info(targetInterface, &netcfg);
		}

		// 静态IP配置但未获取到IP时不回复
		if (netcfg.ifaddr == 0 && pstNetworkConfig->lanCfg.dhcpEnable == 0)
		{
			bResponse = 0;
		}
		else
		{
			get_ip_str(netcfg.ifaddr, tmpLanCfg.IPAddress, MAX_IP_NAME_LEN);
			get_ip_str(netcfg.netmask, tmpLanCfg.netMask, MAX_IP_NAME_LEN);
			get_ip_str(netcfg.gateway, tmpLanCfg.gateWay, MAX_IP_NAME_LEN);
			net_get_two_dns(tmpLanCfg.DNS1, MAX_IP_NAME_LEN, tmpLanCfg.DNS2, MAX_IP_NAME_LEN);
		}
	}
	else
	{
		// 无线网络处理
		targetInterface = net_get_wireless_name();
		tmpLanCfg.dhcpEnable = is_dhcp_running((char *)targetInterface);
		tmpLanCfg.onvifAllnetEnable = pstNetworkConfig->lanCfg.onvifAllnetEnable;

		char macBuf[6] = {0};
		net_get_hwaddr(targetInterface, (unsigned char *)macBuf);
		format_mac_addr_from_digit_to_string(macBuf, sizeof(macBuf), (char *)tmpLanCfg.MACAddress, MAC_ADDRESS_LEN);

		// 特殊序列号处理
		if (sn_str[0] == 'E' && sn_str[1] == 'E' &&
			strcmp(targetInterface, net_get_wireless_name()) == 0 &&
			ANJ_CAMERA_MAX_NUMS > 1)
		{
			iRet = net_get_info("wlan0:0", &netcfg);
			tmpLanCfg.MACAddress[0] = 'E';
		}
		else
		{
			iRet = net_get_info(targetInterface, &netcfg);
		}

		if (iRet != 0)
		{
			bResponse = 0;
		}
		else
		{
			get_ip_str(netcfg.ifaddr, tmpLanCfg.IPAddress, MAX_IP_NAME_LEN);
			get_ip_str(netcfg.netmask, tmpLanCfg.netMask, MAX_IP_NAME_LEN);
			get_ip_str(netcfg.gateway, tmpLanCfg.gateWay, MAX_IP_NAME_LEN);
			net_get_two_dns(tmpLanCfg.DNS1, MAX_IP_NAME_LEN, tmpLanCfg.DNS2, MAX_IP_NAME_LEN);
		}
	}

	// 准备XML内容
	char *lancfg = anj_config_network_lan_conver_xml(&tmpLanCfg);
	MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
	char *streamcfg = anj_config_stream_search_conver_xml(pstMediaStreamConfig);

	if (!lancfg || !streamcfg)
	{
		if (usercfg)
			free(usercfg);
		if (lancfg)
			free(lancfg);
		if (streamcfg)
			free(streamcfg);
		return -1;
	}

	// 构建响应XML
	DevInfo *pstDevInfo = getDevInfo();
	MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
	char ascii_title_utf8[128] = {0};
	bin_to_hexstr(ascii_title_utf8, sizeof(ascii_title_utf8),
				  pstMediaConfig->videoConfig[0].overlay.titleOverlay.title_utf8);

	const char *deviceTypePrefix = (ANJ_CAMERA_MAX_NUMS > 1) ? "NVS-IPCAM-2CHN" : "NVS-IPCAM";
	int factory_mode = pstDevInfo->bFactoryMode;
	int runtime = anj_mw_get_cputime_ms(NULL) / 1000;

	// 获取P2P配置码
	IOTBindConfig *pstBindInfo = getBindInfo();

	// 构建XML主体
	strcpy(send_buf, ""); // 清空发送缓冲区
	snprintf(send_buf, 2048,
			 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
			 "<%s>\n"
			 "<MESSAGE_HEADER Msg_type=\"SYSTEM_SEARCHIPC_MESSAGE\" Msg_code=\"%d\" Msg_flag=\"0\" />\n"
			 "<MESSAGE_BODY>\n",
			 anj_search_xml_name_get(), IPC_MESSAGE_SEARCHIPC_RESPONSE);

	if (strlen(vendorId))
	{
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
				 "<VENDOR_PARAM VendorId=\"%s\"/>\n", vendorId);
	}

	// 设备类型部分
	snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
			 "<DEVICE_TYPE DeviceType=\"%s-%s\" DeviceModule=\"%s\" OSD=\"%s\" />\n",
			 deviceTypePrefix, pstDevInfo->search_devicetype, pstDevInfo->devType, ascii_title_utf8);

	// 序列号部分
	snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
			 "<IPC_SERIALNUMBER UUID=\"%s\" SerialNumber=\"%s\" ",
			 pstDevInfo->uuid, sn_str);

	if (strlen(pstDevInfo->oem_sn) > 0)
	{
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf), "OEM_SN=\"%s\" ", pstDevInfo->oem_sn);
	}

	// 平台ID和P2P信息
	if (pstMediaStreamConfig->unvConfig.onvif_expand)
	{
		/* privatetype==0: 宇视白色界面 NVR 识别为 A_ONVIF_CAMERA；否则 Name=NONE */
		const char *unv_name = (pstMediaStreamConfig->unvConfig.privatetype == 0)
								   ? "A_ONVIF_CAMERA"
								   : "NONE";
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
				 "PLATFORMID=\"%s\" P2PTYPE=\"%d\" P2PID=\"%s\" P2PSTATUS=\"%d\" "
				 "P2PNETCFGCODE=\"%s\" runtime=\"%d\" VERSION=\"%s\" Name=\"%s\" "
				 "FACTORYMODE=\"%d\" AF_VERSION=\"%s\" />\n",
				 szPlatformID, p2pState.p2ptype, p2pState.devid, p2pState.logined,
				 pstBindInfo->match_code, runtime, pstDevInfo->stVersionInfo.fsVersion, unv_name,
				 factory_mode, strAFversion);
	}
	else if (ANJ_CAMERA_MAX_NUMS > 1)
	{
		int chn_num = (sn_str[0] == 'E' && sn_str[1] == 'E') ? 1 : 0;
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
				 "PLATFORMID=\"%s\" P2PTYPE=\"%d\" P2PID=\"%s\" P2PSTATUS=\"%d\" "
				 "P2PNETCFGCODE=\"%s\" runtime=\"%d\" VERSION=\"%s\" FACTORYMODE=\"%d\" "
				 "AF_VERSION=\"%s\" ChannelNum=\"%d\" />\n",
				 szPlatformID, p2pState.p2ptype, p2pState.devid, p2pState.logined,
				 pstBindInfo->match_code, runtime, pstDevInfo->stVersionInfo.fsVersion, factory_mode, strAFversion, chn_num);
	}
	else
	{
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
				 "PLATFORMID=\"%s\" P2PTYPE=\"%d\" P2PID=\"%s\" P2PSTATUS=\"%d\" "
				 "P2PNETCFGCODE=\"%s\" runtime=\"%d\" VERSION=\"%s\" FACTORYMODE=\"%d\" "
				 "AF_VERSION=\"%s\" />\n",
				 szPlatformID, p2pState.p2ptype, p2pState.devid, p2pState.logined,
				 pstBindInfo->match_code, runtime, pstDevInfo->stVersionInfo.fsVersion, factory_mode, strAFversion);
	}

	// 添加LAN配置、用户配置和流配置
	snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
			 "%s\n", lancfg);

	if (usercfg)
	{
		snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
				 "%s\n", usercfg);
	}

	snprintf(send_buf + strlen(send_buf), 2048 - strlen(send_buf),
			 "%s\n"
			 "</MESSAGE_BODY>\n"
			 "</%s>\n",
			 streamcfg,
			 anj_search_xml_name_get());

	if (usercfg)
		free(usercfg);
	if (lancfg)
		free(lancfg);
	if (streamcfg)
		free(streamcfg);

	// 发送响应
	if (bResponse && strlen(send_buf) > 0)
	{
		iRet = anj_search_send_remote(sockfd, send_buf, remote);
		if (iRet <= 0)
		{
			return -1;
		}
	}
	else
	{
	    __ERR("search don't reply response!\n");      
	}

	return 0;
}

static int anj_search_modify_proc(const int sockfd, const char *sn_str, char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_MODIFYIPC from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	LANConfig lanCfg;
	MediaStreamConfig mediaStreamCfg;

	char tmp[256] = {0};

	if (anj_search_modify_sn_get(recv_buf, tmp) != 0)
		return 0;

	__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

	if (tmp[1] == 'E')
	{
		if (strcasecmp(tmp + 2, sn_str + 2) != 0)
			return 0;
	}
	else
	{

		if (strcasecmp(tmp, sn_str) != 0)
			return 0;
	}

	if (anj_config_network_lan_get_by_xml(&lanCfg, recv_buf, 0) == 0)
	{
		__ERR("got network cfg ok, msg=%s\n", recv_buf);

		snprintf(send_buf, 2048,
				 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
				 "<%s>\n"
				 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"0\"\n"
				 "/>\n"
				 "<MESSAGE_BODY>\n"
				 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
				 "</MESSAGE_BODY>\n"
				 "</%s>\n",
				 anj_search_xml_name_get(), IPC_MESSAGE_MODIFYIPC_RESPONSE, sn_str, anj_search_xml_name_get());

		__INFO("send_buf:%s\n", send_buf);

		iRet = anj_search_send_remote(sockfd, send_buf, remote);
		if (iRet <= 0)
		{
			return -1;
		}

		__INFO("send modifyipc response ok.\n");

		// verify network config
		NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
		if (Check_Link_Status(WIRE_INTERFACE_NAME) || !is_network_device_exist(net_get_wireless_name()))
		{
			__ERR("For lan cfg, msg=%s\n", recv_buf);

			memcpy(lanCfg.MACAddress, pstNetworkConfig->lanCfg.MACAddress, MAC_ADDRESS_LEN);
			lanCfg.onvifAllnetEnable = pstNetworkConfig->lanCfg.onvifAllnetEnable;
			lanCfg.dhcpOffTime = pstNetworkConfig->lanCfg.dhcpOffTime;
			strncpy(lanCfg.hostname, pstNetworkConfig->lanCfg.hostname, MAX_IP_NAME_LEN - 1);
			lanCfg.hostname[MAX_IP_NAME_LEN - 1] = '\0';

			if ((lanCfg.dhcpEnable == pstNetworkConfig->lanCfg.dhcpEnable) && lanCfg.dhcpEnable > 0)
			{
				int bNeedSh = 1;
				if (is_dhcp_running(WIRE_INTERFACE_NAME)) // 已经是DHCP在运行的情况下，如果IP是OK的，则不重新执行DHCP
				{
					int ip_addr = (int)net_get_ifaddr(WIRE_INTERFACE_NAME);
					if (ip_addr != 0 && ip_addr != 0xffffffff && ip_addr != 0xffffff && ip_addr != 0x100007f)
					{
						__ERR("DHCP already get IP %#x. egnore this DHCP setting.", ip_addr);
						bNeedSh = 0;
					}
				}

				if (bNeedSh > 0)
				{
					__RECORD_LOG_INFO("start DHCP.\n");
					anj_net_dhcp_up(WIRE_INTERFACE_NAME);
				}
			}
			else if (memcmp(&pstNetworkConfig->lanCfg, &lanCfg, sizeof(LANConfig)))
			{
				__INFO("got new network config.\n");
				memcpy(&pstNetworkConfig->lanCfg, &lanCfg, sizeof(LANConfig));
				anj_config_network_save(pstNetworkConfig);
				__ERR("Lan DHCP %d, %s:%s:%s\n",
					  pstNetworkConfig->lanCfg.dhcpEnable,
					  pstNetworkConfig->lanCfg.IPAddress,
					  pstNetworkConfig->lanCfg.netMask,
					  pstNetworkConfig->lanCfg.gateWay);
				__RECORD_LOG_INFO("Lan DHCP %d, %s:%s:%s\n",
									pstNetworkConfig->lanCfg.dhcpEnable,
									pstNetworkConfig->lanCfg.IPAddress,
									pstNetworkConfig->lanCfg.netMask,
									pstNetworkConfig->lanCfg.gateWay);

				anj_net_set();
			}
		}
		else
		{
			int change = 0;

			__INFO("For wifi config.\n");
			WIFIConfig wifiConfig = pstNetworkConfig->wifiCfg;
			wifiConfig.dhcpEnable = lanCfg.dhcpEnable;
			strcpy(wifiConfig.gateWay, lanCfg.gateWay);
			strcpy(wifiConfig.netMask, lanCfg.netMask);
			strcpy(wifiConfig.IPAddress, lanCfg.IPAddress);

			if (memcmp(&pstNetworkConfig->wifiCfg, &wifiConfig, sizeof(WIFIConfig)) != 0)
			{
				change = 1;
				memcpy(&pstNetworkConfig->wifiCfg, &wifiConfig, sizeof(WIFIConfig));
			}

			if (strcmp(pstNetworkConfig->lanCfg.DNS1, lanCfg.DNS1) != 0 || strcmp(pstNetworkConfig->lanCfg.DNS2, lanCfg.DNS2) != 0)
			{
				change = 1;
				memset(pstNetworkConfig->lanCfg.DNS1, 0, MAX_IP_NAME_LEN);
				strcpy(pstNetworkConfig->lanCfg.DNS1, lanCfg.DNS1);
				memset(pstNetworkConfig->lanCfg.DNS2, 0, MAX_IP_NAME_LEN);
				strcpy(pstNetworkConfig->lanCfg.DNS2, lanCfg.DNS2);
			}

			if (change)
			{
				__ERR("got new network config(for wifi).\n");

				anj_config_network_save(pstNetworkConfig);
				__ERR("wifi: DHCP:%d %s:%s:%s\n",
					  pstNetworkConfig->wifiCfg.dhcpEnable,
					  pstNetworkConfig->wifiCfg.IPAddress,
					  pstNetworkConfig->wifiCfg.netMask,
					  pstNetworkConfig->wifiCfg.gateWay);
				__RECORD_LOG_INFO("wifi: DHCP:%d %s:%s:%s\n",
									pstNetworkConfig->wifiCfg.dhcpEnable,
									pstNetworkConfig->wifiCfg.IPAddress,
									pstNetworkConfig->wifiCfg.netMask,
									pstNetworkConfig->wifiCfg.gateWay);
			}
		}
	}

	MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
	memcpy(&mediaStreamCfg, pstMediaStreamConfig, sizeof(mediaStreamCfg));
	if (anj_config_stream_get_by_xml(&mediaStreamCfg, recv_buf, 1) == 0)
	{
		__ERR("got mediaStream cfg ok, msg=%s\n", recv_buf);

		int portOK = 0;
		// validate the stream config, port checking
		if (anj_config_stream_port_check(&mediaStreamCfg))
		{
			__ERR("Invalid port config!!!\n");
			portOK = -1;
		}

		// send response
		snprintf(send_buf, 2048,
				 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
				 "<%s>\n"
				 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
				 "/>\n"
				 "<MESSAGE_BODY>\n"
				 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
				 "</MESSAGE_BODY>\n"
				 "</%s>\n",
				 anj_search_xml_name_get(), IPC_MESSAGE_MODIFYIPC_RESPONSE, portOK, sn_str, anj_search_xml_name_get());

		__INFO("send_buf:%s\n", send_buf);

		iRet = anj_search_send_remote(sockfd, send_buf, remote);
		if (iRet <= 0)
		{
			return -1;
		}

		__INFO("send modifyipc response ok. portOK:%d\n", portOK);

		if (portOK >= 0)
		{
			// backup first
			MediaStreamConfig stOldMediaStreamConfig = *pstMediaStreamConfig;

			// update then
			memcpy(pstMediaStreamConfig, &mediaStreamCfg, sizeof(MediaStreamConfig));

			if (anj_config_stream_save(&mediaStreamCfg) == 0)
			{
				// if rtsp port or auth change, restart rtsp_server
				if ((stOldMediaStreamConfig.rtspConfig.videoPort != pstMediaStreamConfig->rtspConfig.videoPort) ||
					(stOldMediaStreamConfig.rtspConfig.rtsp_auth != pstMediaStreamConfig->rtspConfig.rtsp_auth))
				{
					__ERR("rtsp config change, restart rtsp\n");

					EventResult event_result = {0};
					eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTSP_RESTART, &event_result, NULL);
				}

				// if web port change, restart web_server
				if (stOldMediaStreamConfig.webConfig.webPort != pstMediaStreamConfig->webConfig.webPort)
				{
					__ERR("web config change, restart web\n");
					// todo restart web
					// mysystem("killall web_server");
				}

				// if ptz port change, restart comm_server
				if (stOldMediaStreamConfig.commConfig.ptzPort != pstMediaStreamConfig->commConfig.ptzPort)
				{
					__ERR("ptz port change, restart ptz\n");
					// todo restart ptz
					//  mysystem("killall comm_server");
				}
			}
			else // copy back
			{
				__ERR("SetMediaStreamConfig failed!!!\n");
				memcpy(&pstMediaStreamConfig, &stOldMediaStreamConfig, sizeof(MediaStreamConfig));
			}
		}
	}

	return 0;
}

static int anj_search_restore_proc(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_RESTORECONFIG from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	unsigned int reserved_bits = 0;
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			char pRetain[256] = {0};
			if (anj_search_retain_get(recv_buf, pRetain) == 0)
			{
				if (strstr(pRetain, "network") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_NETWORK);
				if (strstr(pRetain, "language") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_LANGUAGE);
				if (strstr(pRetain, "time") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_TIME);
				if (strstr(pRetain, "usercfg") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_USER);
				if (strstr(pRetain, "mediacode") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_MEDIACODE);
				if (strstr(pRetain, "ptzcfg") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_PTZ);
				if (strstr(pRetain, "streamaccess") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_STREAMACCESS);
				if (strstr(pRetain, "record") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_RECORD);
				if (strstr(pRetain, "gb28181") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_GB28181);
				if (strstr(pRetain, "alarm") != NULL)
					BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_ALARM);

				__ERR("reserved_bits=%u, %s", reserved_bits, pRetain);
			}

			// send response
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"0\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(), IPC_MESSAGE_RESTORECONFIG_RESPONSE, sn_str, anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send restore config response ok.\n");

			__INFO("CMD_RESET_TO_VOICECONFIG_MODE\n");

			remove(P2P_ID_NETCONFIGED_ALI);
			remove(P2P_ID_NETCONFIGED_AIOT);
			remove("/mnt/nand/ap_mode");

			__RECORD_LOG_INFO("restore config reserved_bits=%u from %s: %d.\n", reserved_bits, remoteip, htons(remote.sin_port)); 
			anj_sysmng_config_restore(reserved_bits);
		}
	}
	return 0;
}

static int anj_search_reboot_proc(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("go IPC_MESSAGE_REBOOT from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			// send response
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"0\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(), IPC_MESSAGE_REBOOT_RESPONSE, sn_str, anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__WARN("send reboot response ok.\n");
			__RECORD_LOG_INFO("send reboot response ok. from %s: %d\n", remoteip, htons(remote.sin_port));
			anj_sysmng_delay_reboot(3);
		}
	}
	return 0;
}

// 厂测模式,不保存任何配置
static int anj_search_factory_proc(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote, int msgflag)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_MODE_FACTORY from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	DevInfo *pstDevInfo = getDevInfo();
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			// send response
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"0\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(), IPC_MESSAGE_MODE_FACTORY_RESPONSE, sn_str, anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send IPC_MESSAGE_MODE_FACTORY response ok.\n");

			if (msgflag == 0)
			{
				pstDevInfo->bFactoryMode = 1;
				anj_factory_init();

				remove(CONFIG_PTZ_PATH);
				remove("/mnt/nand/power_save_default");
				remove("/mnt/nand/power_save_disable");

				// 如果产测带wifi信息需要重启配网
				char ssid[128] = {0};
				char passwd[128] = {0};
				iRet = anj_search_wifi_info_get(recv_buf, ssid, passwd);
				if (iRet == 0)
				{
					anj_factory_wifi_connect_set(ssid, passwd);
				}
			}
			else
			{
				pstDevInfo->bFactoryMode = 0;
			}
		}
	}
	return 0;
}

static int anj_search_factory_cfg_get(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_GET_FACTORY_CFG from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();

	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			// send response
			FactoryDefaultCfg defaultCfg = {0};
			defaultCfg.LedMode = pstMediaConfig->videoConfig[0].videoCapture.led_mode;
			defaultCfg.IrCutMode = pstMediaConfig->videoConfig[0].videoCapture.ircut_mode;

			anj_factory_defcfg_load(&defaultCfg);

			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "<FactoryCfg  LedMode=\"%d\"  IrcutMode=\"%d\" />\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(),
					 IPC_MESSAGE_GET_FACTORY_CFG_RESPONSE, iRet, sn_str, defaultCfg.LedMode, defaultCfg.IrCutMode,
					 anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send IPC_MESSAGE_GET_FACTORY_CFG_RESPONSE response ok.\n");
		}
	}
	return 0;
}

static int anj_search_factory_cfg_clear(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_CLEAR_FACTORY_CFG from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			// send response
			remove(FACTORY_DEFAULT_CFG_PATH);
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(), IPC_MESSAGE_CLEAR_FACTORY_CFG_RESPONSE, iRet, sn_str, anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send IPC_MESSAGE_CLEAR_FACTORY_CFG_RESPONSE response ok.\n");
		}
	}
	return 0;
}

static int anj_search_factory_config_proc(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote, int msgflag)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_SET_FACTORY_CFG from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	DevInfo *pstDevInfo = getDevInfo();

	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			FactoryDefaultCfg defaultCfg;
			memset(&defaultCfg, 0, sizeof(FactoryDefaultCfg));
			iRet = anj_factory_defcfg_get(recv_buf, &defaultCfg);
			if (iRet != 0)
			{
				__ERR("anj_factory_defcfg_get fail\n");
			}
			else
			{
				iRet = anj_factory_defcfg_save(&defaultCfg);
				__ERR("ircut mode = %d, led mode = %d\n", defaultCfg.IrCutMode, defaultCfg.LedMode);
				pstDevInfo->bFactoryReload = 1;
			}

			// send response
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "<FactoryCfg  LedMode=\"%d\"  IrcutMode=\"%d\" />\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(),
					 IPC_MESSAGE_SET_FACTORY_CFG_RESPONSE, iRet, sn_str, defaultCfg.LedMode, defaultCfg.IrCutMode,
					 anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send IPC_MESSAGE_SET_FACTORY_CFG_RESPONSE response ok.\n");
		}
	}
	return 0;
}

static int anj_search_usercfg_set(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_SET_USER_CFG from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	int iRet = 0;
	char tmp[256] = {0};
	NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();

	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg sn: %s, my sn: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			USER_CONFIG userConfig = {0};
			iRet = anj_search_usercfg_get(recv_buf, &userConfig);
			if (iRet != 0)
			{
				__ERR("anj_search_usercfg_get fail\n");
			}
			else
			{
				if (userConfig.allNetSet && pstNetworkConfig->lanCfg.onvifAllnetEnable != userConfig.allNetEnable)
				{
					__RECORD_LOG_INFO("ALLNET enable changed to %d\n", userConfig.allNetEnable);
					pstNetworkConfig->lanCfg.onvifAllnetEnable = userConfig.allNetEnable;
					anj_config_network_save(pstNetworkConfig);
					// todo
					//  mysystem("killall web_server");
				}
			}

			// send response
			snprintf(send_buf, 2048,
					 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
					 "<%s>\n"
					 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_SEARCHIPC_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
					 "/>\n"
					 "<MESSAGE_BODY>\n"
					 "<IPC_SERIALNUMBER\nSerialNumber=\"%s\"\n/>\n"
					 "<UserCfg  AllNet=\"%d\" />\n"
					 "</MESSAGE_BODY>\n"
					 "</%s>\n",
					 anj_search_xml_name_get(),
					 IPC_MESSAGE_SET_USER_CFG_RESPONSE, iRet, sn_str, userConfig.allNetEnable,
					 anj_search_xml_name_get());

			__INFO("send_buf:%s\n", send_buf);

			iRet = anj_search_send_remote(sockfd, send_buf, remote);
			if (iRet <= 0)
			{
				return -1;
			}

			__INFO("send IPC_MESSAGE_SET_USER_CFG_RESPONSE response ok.\n");
		}
	}
	return 0;
}

static int anj_search_factory_test_proc(const int sockfd, const char *sn_str, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	__INFO("got IPC_MESSAGE_TEST_LED_PTZ_IRCUT from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	char tmp[256] = {0};
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		__ERR("msg uuid: %s, my uuid: %s\n", tmp, sn_str);

		if (strcasecmp(tmp, sn_str) == 0)
		{
			anj_factory_test(FACTORY_TEST_PTZ_LED_IRCUT);
		}
	}
	return 0;
}

static int anj_search_clear_sn(const int sockfd, const char *recv_buf, char *send_buf, struct sockaddr_in remote)
{
	char remoteip[32] = {0};
	DevInfo *pstDevInfo = getDevInfo();
	unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
	sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

	//	__INFO("got IPC_MESSAGE_CLEAR_SOFT_SN from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);

	char tmp[256] = {0};
	if (anj_search_modify_sn_get(recv_buf, tmp) == 0)
	{
		//		__ERR("msg uuid: %s, my uuid: %s\n", tmp, pstDevInfo->uuid);

		if (strcasecmp(tmp, pstDevInfo->uuid) == 0)
		{
			__INFO("got IPC_MESSAGE_CLEAR_SOFT_SN from %s: %d. msg=%s\n", remoteip, htons(remote.sin_port), recv_buf);
			__ERR("CameraID %s, clear SN", tmp);

			ClearEncriptDataToSoft(-1);
			__WARN("clear soft SN, reboot\n");
			__RECORD_LOG_INFO("clear soft SN, reboot\n");
			anj_sysmng_reboot();
		}
	}
	return 0;
}

static int anj_search_thread(void *ctx, int *bStart)
{
	int sockfd = -1;
	struct sockaddr_in remote;
	int iRet = 0;
	int nReuseAddress = 1;
	unsigned long long tLastCpuTimeMs = 0;

	fd_set read_fds;
	struct timeval wait_time;

	char recv_buf[2048] = {0};
	char send_buf[2048] = {0};
	char vendor_id[256] = {0};
	char super_passwd[32] = {0};
	MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
	DevInfo *pstDevInfo = getDevInfo();

	int verify_passwd = 0;
	if (anj_mw_file_exists("/mnt/nand/security.flag") || anj_mw_file_exists(AJ_APP_PATH "/security.flag"))
	{
		verify_passwd = 1;

		char source[256] = {0};
		char buffer[64] = {0};
		snprintf(source, sizeof(source), "%s%s", pstDevInfo->sn, "www.anjvision.com");
		our_md5_encode(buffer, (unsigned char *)source, strlen(source));
		strcpy(super_passwd, buffer + 6);
		super_passwd[10] = 0;
	}

	__INFO("enter main loop, verify_passwd = %d\n", verify_passwd);

	while (bStart && *bStart)
	{
		sleep(1);
		sockfd = broadcastserver_ex(BROADCASTING_PORT, nReuseAddress);
		if (sockfd < 0)
		{
			__ERR("create socket failed, errno=%d\n", errno);
			continue;
		}

		__INFO("enter send and recv loop...\n");

		while (*bStart)
		{
			if (pstMediaStreamConfig->commConfig.enable == 0)
			{
				usleep(1000 * 100);
				continue;
			}

			wait_time.tv_sec = 1;
			wait_time.tv_usec = 0;

			FD_ZERO(&read_fds);
			FD_SET(sockfd, &read_fds);

			iRet = select(sockfd + 1, &read_fds, NULL, NULL, &wait_time);
			if (iRet < 0)
			{
				__ERR("select failed, error=%d\n", errno);
				net_set_up(WIRE_INTERFACE_NAME);
				break;
			}
			else if (iRet == 0)
			{
				continue;
			}

			if (!FD_ISSET(sockfd, &read_fds))
			{
				continue;
			}

			int remote_len = sizeof(remote);
			iRet = safe_recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&remote, (socklen_t *)&remote_len);
			if (iRet <= 0)
			{
				__ERR("safe_recvfrom failed, error=%d\n", errno);
				break;
			}

			recv_buf[iRet] = '\0';

			int msgflag = 0;
			char vendorId[256] = {0};
			char remoteip[32] = {0};
			int cmd = anj_search_cmd_get(recv_buf, vendorId, verify_passwd, vendor_id, &msgflag, super_passwd);
			unsigned char *ip = (unsigned char *)&remote.sin_addr.s_addr;
			sprintf(remoteip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); // host order

			if (-100 == cmd) // 无权限
			{
				unsigned long long tNowCpuTimeMs = anj_mw_get_cputime_ms(NULL);

				if (tNowCpuTimeMs - tLastCpuTimeMs > 2 * 1000)
				{
					tLastCpuTimeMs = tNowCpuTimeMs;
					char tmpbuf[128] = {0};
					sprintf(tmpbuf, "Unauthorized modification from %s", remoteip);
					// todo
					// NotifyIllegalModify(tmpbuf);
				}
			}

			//						__ERR("cmd = %d\n",cmd);

			if (anj_sysmng_is_limit_ip(remote.sin_addr.s_addr) > 0 && cmd != IPC_MESSAGE_SEARCHIPC)
			{
				//							__ERR("%u now alowed.", remote.sin_addr.s_addr);
				continue;
			}

			//						__INFO("CMD  %d from %s: %d \n", remoteip, htons(remote.sin_port));

			if (cmd == IPC_MESSAGE_SEARCHIPC)
			{
				if (0 > anj_search_cmd_proc(sockfd, vendorId, pstDevInfo->sn, send_buf, remote, verify_passwd))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_MODIFYIPC)
			{
				__INFO("got IPC_MESSAGE_MODIFYIPC: %s\n", recv_buf);
				if (0 > anj_search_modify_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_RESTORECONFIG)
			{
				__INFO("got IPC_MESSAGE_RESTORECONFIG: %s\n", recv_buf);
				if (0 > anj_search_restore_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_REBOOT)
			{
				__INFO("got IPC_MESSAGE_REBOOT: %s\n", recv_buf);
				// topsee cmd
				if (strstr(recv_buf, "randStr"))
				{
					__ERR("discard topsee reboot cmd\n");
					continue;
				}

				if (0 > anj_search_reboot_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_MODE_FACTORY)
			{
				__INFO("got IPC_MESSAGE_MODE_FACTORY: %s\n", recv_buf);
				if (0 > anj_search_factory_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote, msgflag))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_SET_FACTORY_CFG)
			{
				__INFO("got IPC_MESSAGE_SET_FACTORY_CFG: %s\n", recv_buf);
				if (0 > anj_search_factory_config_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote, msgflag))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_CLEAR_FACTORY_CFG)
			{
				__INFO("got IPC_MESSAGE_CLEAR_FACTORY_CFG: %s\n", recv_buf);
				if (0 > anj_search_factory_cfg_clear(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_GET_FACTORY_CFG)
			{
				__INFO("got IPC_MESSAGE_GET_FACTORY_CFG: %s\n", recv_buf);
				if (0 > anj_search_factory_cfg_get(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_TEST_LED_PTZ_IRCUT)
			{
				__INFO("got IPC_MESSAGE_TEST_LED_PTZ_IRCUT: %s\n", recv_buf);
				if (0 > anj_search_factory_test_proc(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_CLEAR_SOFT_SN)
			{
				__INFO("got IPC_MESSAGE_CLEAR_SOFT_SN: %s\n", recv_buf);
				if (0 > anj_search_clear_sn(sockfd, recv_buf, send_buf, remote))
				{
					break;
				}
			}
			else if (cmd == IPC_MESSAGE_SET_USER_CFG)
			{
				__INFO("got IPC_MESSAGE_SET_USER_CFG: %s\n", recv_buf);
				if (0 > anj_search_usercfg_set(sockfd, pstDevInfo->sn, recv_buf, send_buf, remote))
				{
					break;
				}
			}
		}

		__INFO("exit send and recv loop\n");

		if (sockfd)
			close(sockfd);
		sockfd = -1;
	}

	__INFO("exit main loop\n");
	return 0;
}

int anj_search_init(void)
{
	int iRet = 0;
	if (s_stSearchThread.pid == 0)
	{
		s_stSearchThread.bAutoDestroy = 1;
		strncpy(s_stSearchThread.iThreadName, "search_thread", sizeof(s_stSearchThread.iThreadName) - 1);
		s_stSearchThread.iThreadjob.ctx = (void *)&s_stSearchThread;
		s_stSearchThread.iThreadjob.func = anj_search_thread;
		iRet = anj_thread_task_create(&s_stSearchThread);
		__INFO("start anj_search_thread OK!!!\n");
	}

	return iRet;
}

void anj_search_uninit(void)
{
	if (s_stSearchThread.pid)
	{
		anj_thread_task_destroy(&s_stSearchThread, -1);
		__INFO("stop anj_search_thread OK!!!\n");
		memset(&s_stSearchThread, 0, sizeof(s_stSearchThread));
	}
}