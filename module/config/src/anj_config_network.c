#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_net.h"

#include "eventhub.h"

#define DDNS_NEW_FUNCTION 1

int usb_get_vendor_product(char *vendor_id, char *prod_id)
{
    char vendor_str[32] = "";
    char *buf = (char *)malloc(4 * 1024);
    if (buf == NULL)
    {
        __ERR("malloc buf failed, err=%s\n", strerror(errno));
        return -2;
    }

    FILE *fp = fopen("/proc/bus/usb/devices", "rb");
    if (fp == NULL)
    {
        __ERR("open /proc/bus/usb/devices failed, err=%s\n", strerror(errno));

        free(buf);
        return -1;
    }

    int flen = fread(buf, 1, 4 * 1024, fp);
    fclose(fp);

    if (flen <= 0)
    {
        __ERR("read /proc/bus/usb/devices failed, err=%s\n", strerror(errno));

        free(buf);
        return -3;
    }

    buf[flen] = 0;

    char *valid_start = strstr(buf, "Vendor=1c9e"); // 04d6");
    if (valid_start == NULL)
    {
        valid_start = strstr(buf, "Vendor=19d2");
        if (valid_start == NULL)
        {
            __ERR("read usb controler vendor id failed.\n");

            free(buf);
            return -10;
        }
        else
        {
            strcpy(vendor_str, "Vendor=19d2");
        }
    }
    else
    {
        strcpy(vendor_str, "Vendor=1c9e"); // 04d6");
    }

    char *vendor_start = strstr(valid_start, "Vendor=");
    char *prod_start;
    char *rev_start;

    if (vendor_start)
    {
        prod_start = strstr(vendor_start, "ProdID=");
        if (prod_start)
        {
            rev_start = strstr(prod_start, "Rev=");
            if (rev_start == NULL)
            {
                free(buf);
                return -4;
            }

            memcpy(vendor_id, vendor_start + strlen("Vendor="), prod_start - vendor_start - strlen("Vendor="));
            vendor_id[prod_start - vendor_start - strlen("Vendor=")] = 0;

            memcpy(prod_id, prod_start + strlen("ProdID="), rev_start - prod_start - strlen("ProdID="));
            prod_id[rev_start - prod_start - strlen("ProdID=")] = 0;

            free(buf);
            return 0;
        }
        else
        {
            free(buf);
            return -5;
        }
    }
    else
    {
        free(buf);
        return -6;
    }
}

#if DDNS_NEW_FUNCTION

typedef struct
{
    char type_name[256];
    char def_addr[256];
    int auto_register;
    char manual_reg_url[256];
    int need_update;
} DDNS_INFO_ENTRY;

DDNS_INFO_ENTRY gDdnsTypeList[] =
    {
        //{"ORAY","oray.com", 0,"http://www.oray.com", 0},
        //  {"3322","3322.org", 0,"http://www.3322.org", 1},
        //{"3322","f3322.org", 0,"http://www.f3322.org", 1},
        //  {"DYNDNS","dyndns.org", 0,"http://www.dyndns.org", 1},
        //{"ULOOKME","ulookme.com", 0, "http://www.ulookme.com", 1},
        //{"NVDVR","nvdvr.net", 1,"", 1},
        {"speco", "66.207.40.101", 0, "http://specoddns.net/", 1},
        {"", "", 0, "", 0},
};

int GetDdnsTypeList(char *retBuf, int size)
{
    int index = 0;
    char *pe = retBuf + size - 1;
    char *pb = retBuf;

    //__ERR("GetDdnsTypeList start\n");

    while (strlen(gDdnsTypeList[index].type_name) > 0)
    {
        pb += snprintf(pb, pe - pb, "%s,", gDdnsTypeList[index].type_name);
        pb += snprintf(pb, pe - pb, "%d,", gDdnsTypeList[index].auto_register);
        pb += snprintf(pb, pe - pb, "%s,", gDdnsTypeList[index].manual_reg_url);
        pb += snprintf(pb, pe - pb, "%s;", gDdnsTypeList[index].def_addr);

        //__ERR("GetDdnsTypeList, str= %s\n", retBuf);

        index++;
    }

    //{"ULOOKME","ulookme.com", 0, "http://www.ulookme.com", 1},

    FILE *fp = fopen("/tmp/www/ddns_ulookme.txt", "rb");
    if (fp)
    {
        fclose(fp);

        pb += snprintf(pb, pe - pb, "%s,", "ULOOKME");
        pb += snprintf(pb, pe - pb, "%d,", 0);
        pb += snprintf(pb, pe - pb, "%s,", "http://www.ulookme.com");
        pb += snprintf(pb, pe - pb, "%s;", "ulookme.com");
    }

    // for kaicong, XXX 20130411
    fp = fopen("/tmp/www/ddns_kaicong.txt", "rb");
    if (fp)
    {
        fclose(fp);

        pb += snprintf(pb, pe - pb, "%s,", "KAICONG");
        pb += snprintf(pb, pe - pb, "%d,", 0);
        pb += snprintf(pb, pe - pb, "%s,", "http://www.kaicong.com");
        pb += snprintf(pb, pe - pb, "%s;", "kaicong.com");
    }

    //__ERR("GetDdnsTypeList finished\n");

    return 0;
}
#endif

static int anj_config_network_wifi_pingwatch_get(IXML_Node *pNode, WIFIPingWatchConfig *pPingWatchConfig)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pPingWatchConfig->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PingAddress"))
        {
            memset(pPingWatchConfig->address, '\0', MAX_IP_NAME_LEN);
            StrCpy(pPingWatchConfig->address, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Interval"))
        {
            pPingWatchConfig->interval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MaxFail"))
        {
            pPingWatchConfig->maxFail = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_network_wifi_encrypt_get(IXML_Node *pNode, WirelessEncrypt *pWirelessEncrypt)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pWirelessEncrypt->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptType"))
        {
            memset(pWirelessEncrypt->encryptType, '\0', MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN);
            StrCpy(pWirelessEncrypt->encryptType, MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "WEPEncrypt"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "AuthMode"))
                {
                    memset(pWirelessEncrypt->wepEncrypt.authMode, '\0', MAX_WEPENCRYPT_AUTHMODE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wepEncrypt.authMode, MAX_WEPENCRYPT_AUTHMODE_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "EncryptType"))
                {
                    memset(pWirelessEncrypt->wepEncrypt.encryptType, '\0', MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wepEncrypt.encryptType, MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "KeyIndex"))
                {
                    pWirelessEncrypt->wepEncrypt.keyIndex = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "KeyMode"))
                {
                    memset(pWirelessEncrypt->wepEncrypt.keyMode, '\0', MAX_WEPENCRYPT_KEYMODE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wepEncrypt.keyMode, MAX_WEPENCRYPT_KEYMODE_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "KeyValue"))
                {
                    memset(pWirelessEncrypt->wepEncrypt.keyValue, '\0', MAX_WEPENCRYPT_KEYMODE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wepEncrypt.keyValue, MAX_WEPENCRYPT_KEYMODE_NAME_LEN, tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "WPAEncrypt"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "EncryptType"))
                {
                    memset(pWirelessEncrypt->wpaEncrypt.encryptType, '\0', MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wpaEncrypt.encryptType, MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "AuthMode"))
                {
                    memset(pWirelessEncrypt->wpaEncrypt.authMode, '\0', MAX_WPAENCRYPT_AUTHMODE_NAME_LEN);
                    StrCpy(pWirelessEncrypt->wpaEncrypt.authMode, MAX_WPAENCRYPT_AUTHMODE_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "KeyValue"))
                {
                    memset(pWirelessEncrypt->wpaEncrypt.keyValue, '\0', MAX_WPAENCRYPT_KEYVALUE_LEN);
                    StrCpy(pWirelessEncrypt->wpaEncrypt.keyValue, MAX_WPAENCRYPT_KEYVALUE_LEN, tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

int anj_config_network_lan_get(IXML_Node *pNode, LANConfig *lanCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "MacAddress"))
        {
            memset(lanCfg->MACAddress, '\0', MAC_ADDRESS_LEN);
            StrCpy((char *)lanCfg->MACAddress, MAC_ADDRESS_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DHCP"))
        {
            lanCfg->dhcpEnable = (char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "dhcpOffTime"))
        {
            int dhcpOffTime = Str2Num(tmpAttr->nodeValue);
            lanCfg->dhcpOffTime = (char)dhcpOffTime;
        }
        else if (!strcmp(tmpAttr->nodeName, "ALLNET"))
        {
            lanCfg->onvifAllnetEnable = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IPAddress"))
        {
            memset(lanCfg->IPAddress, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->IPAddress, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Netmask"))
        {
            memset(lanCfg->netMask, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->netMask, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Gateway"))
        {
            memset(lanCfg->gateWay, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->gateWay, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DNS1"))
        {
            memset(lanCfg->DNS1, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->DNS1, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DNS2"))
        {
            memset(lanCfg->DNS2, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->DNS2, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "hostname"))
        {
            memset(lanCfg->hostname, '\0', MAX_IP_NAME_LEN);
            StrCpy(lanCfg->hostname, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MTU"))
        {
            lanCfg->mtu = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (lanCfg->mtu == 0 || lanCfg->mtu > 8192)
        lanCfg->mtu = 1460;

    __INFO("DHCP %d dhcpOffTime %d, ONVIF ALLNET %d, %s:%s:%s, MAC %s, DNS %s %s, hostname %s, mtu %u\n",
          lanCfg->dhcpEnable, (int)lanCfg->dhcpOffTime, lanCfg->onvifAllnetEnable,
          lanCfg->IPAddress, lanCfg->netMask, lanCfg->gateWay,
          lanCfg->MACAddress, lanCfg->DNS1, lanCfg->DNS2, lanCfg->hostname, lanCfg->mtu);

    return 0;
}

static int anj_config_network_wifiap_get(IXML_Node *pNode, WIFIApConfig *wifiCfg)
{
    memset(wifiCfg, 0, sizeof(WIFIApConfig));

    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            wifiCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Version"))
        {
            memset(wifiCfg->version, '\0', MAX_WIFI_VERSION_LEN);
            StrCpy(wifiCfg->version, MAX_WIFI_VERSION_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MacAddress"))
        {
            memset(wifiCfg->MACAddress, '\0', MAC_ADDRESS_LEN);

            StrCpy(wifiCfg->MACAddress, MAC_ADDRESS_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ChannelNumber"))
        {
            wifiCfg->channelNum = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Region"))
        {
            memset(wifiCfg->region, '\0', MAX_WIRELESS_REGION_NAME_LEN);
            StrCpy(wifiCfg->region, MAX_WIRELESS_REGION_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ESSID"))
        {
            memset(wifiCfg->essid, '\0', MAX_WIRELESS_ESSID_NAME_LEN);
            StrCpy(wifiCfg->essid, MAX_WIRELESS_ESSID_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BitRate"))
        {
            memset(wifiCfg->bitRate, '\0', MAX_WIRELESS_BITRATE_NAME_LEN);
            StrCpy(wifiCfg->bitRate, MAX_WIRELESS_BITRATE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MacMode"))
        {
            memset(wifiCfg->macMode, '\0', MAX_WIRELESS_MACMODE_NAME_LEN);
            StrCpy(wifiCfg->macMode, MAX_WIRELESS_MACMODE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IPAddress"))
        {
            memset(wifiCfg->IPAddress, '\0', MAX_IP_NAME_LEN);
            StrCpy(wifiCfg->IPAddress, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Netmask"))
        {
            memset(wifiCfg->netMask, '\0', MAX_IP_NAME_LEN);
            StrCpy(wifiCfg->netMask, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "WirelessEncrypt"))
        {
            anj_config_network_wifi_encrypt_get(tmpChild, &(wifiCfg->wirelessEncrypt));
        }

        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int anj_config_network_alarmserver_get(IXML_Node *pNode, AlarmServerConfig *cfg)
{
    memset(cfg, 0, sizeof(AlarmServerConfig));

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "url"))
        {
            StrCpy(cfg->url, MAX_URL_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "withattachment"))
        {
            cfg->withattachment = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Username"))
        {
            StrCpy(cfg->userName, ACCOUNT_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Password"))
        {
            StrCpy(cfg->password, ACCOUNT_PASSWORD_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptPwd"))
        {
            memset(cfg->password, '\0', ACCOUNT_PASSWORD_MAX_LEN);
            char szEncryptData[64];
            StrCpy(szEncryptData, 64, tmpAttr->nodeValue);
            char dst[32];
            int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
            if (ret != 0)
            {
                __ERR("decrypt %s error.\n", szEncryptData);
            }
            else
            {
                char *tmpDst = restore_with_escape(dst);
                if (tmpDst != NULL)
                {
                    StrCpy(cfg->password, ACCOUNT_PASSWORD_MAX_LEN, tmpDst);
                    free(tmpDst);
                    tmpDst = NULL;
                }
                else
                {
                    cfg->password[0] = '\0';
                }
            }
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_network_wifi_get(IXML_Node *pNode, WIFIConfig *wifiCfg)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    memset(wifiCfg, 0, sizeof(WIFIConfig));
    wifiCfg->enable = 1;
    wifiCfg->dhcpEnable = 0;
    strcpy(wifiCfg->operationMode, WIRELESS_OPERATIONMODE_MANAGED_NAME);

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            wifiCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Version"))
        {
            memset(wifiCfg->version, '\0', MAX_WIFI_VERSION_LEN);
            StrCpy(wifiCfg->version, MAX_WIFI_VERSION_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "OperationMode"))
        {
            memset(wifiCfg->operationMode, '\0', MAX_WIRELESS_OPERATIONMODE_NAME_LEN);
            StrCpy(wifiCfg->operationMode, MAX_WIRELESS_OPERATIONMODE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MacAddress"))
        {
            memset(wifiCfg->MACAddress, '\0', MAC_ADDRESS_LEN);

            StrCpy(wifiCfg->MACAddress, MAC_ADDRESS_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ChannelNumber"))
        {
            wifiCfg->channelNum = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Region"))
        {
            memset(wifiCfg->region, '\0', MAX_WIRELESS_REGION_NAME_LEN);
            StrCpy(wifiCfg->region, MAX_WIRELESS_REGION_NAME_LEN, tmpAttr->nodeValue);
        }

        else if (!strcmp(tmpAttr->nodeName, "ESSID"))
        {
            memset(wifiCfg->essid, '\0', MAX_WIRELESS_ESSID_NAME_LEN);
            gb2312_to_utf8(tmpAttr->nodeValue, wifiCfg->essid);
            // StrCpy(wifiCfg->essid,MAX_WIRELESS_ESSID_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ESSID_UTF8"))
        {
            memset(wifiCfg->essid, '\0', MAX_WIRELESS_ESSID_NAME_LEN);
            if (tmpAttr->nodeValue)
            {
                hex2ascii(tmpAttr->nodeValue, strlen(tmpAttr->nodeValue), wifiCfg->essid, MAX_WIRELESS_ESSID_NAME_LEN);
            }
            //__ERR("333tmpAttr->nodeValue %s;wifiCfg->essid = %s\n",tmpAttr->nodeValue,wifiCfg->essid);
        }

        else if (!strcmp(tmpAttr->nodeName, "BitRate"))
        {
            memset(wifiCfg->bitRate, '\0', MAX_WIRELESS_BITRATE_NAME_LEN);
            StrCpy(wifiCfg->bitRate, MAX_WIRELESS_BITRATE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MacMode"))
        {
            memset(wifiCfg->macMode, '\0', MAX_WIRELESS_MACMODE_NAME_LEN);
            StrCpy(wifiCfg->macMode, MAX_WIRELESS_MACMODE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DHCP"))
        {
            wifiCfg->dhcpEnable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IPAddress"))
        {
            memset(wifiCfg->IPAddress, '\0', MAX_IP_NAME_LEN);
            StrCpy(wifiCfg->IPAddress, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Netmask"))
        {
            memset(wifiCfg->netMask, '\0', MAX_IP_NAME_LEN);
            StrCpy(wifiCfg->netMask, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Gateway"))
        {
            memset(wifiCfg->gateWay, '\0', MAX_IP_NAME_LEN);
            StrCpy(wifiCfg->gateWay, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    wifiCfg->pingWatchCfg.enable = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "WirelessEncrypt"))
        {
            anj_config_network_wifi_encrypt_get(tmpChild, &(wifiCfg->wirelessEncrypt));
        }

        if (!strcmp(tmpChild->nodeName, "PingWatchConfig"))
        {
            anj_config_network_wifi_pingwatch_get(tmpChild, &(wifiCfg->pingWatchCfg));
        }

        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int anj_config_network_adsl_get(IXML_Node *pNode, ADSLConfigNew *adslCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            adslCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Username"))
        {
            memset(adslCfg->userName, '\0', ADSL_NAME_MAX_LEN);
            StrCpy(adslCfg->userName, ADSL_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Password"))
        {
            memset(adslCfg->password, '\0', ADSL_PASSWORD_MAX_LEN);
            StrCpy(adslCfg->password, ADSL_PASSWORD_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptPwd"))
        {
            memset(adslCfg->password, '\0', ADSL_PASSWORD_MAX_LEN);
            char szEncryptData[64];
            StrCpy(szEncryptData, 64, tmpAttr->nodeValue);
            char dst[32];
            int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
            if (ret != 0)
            {
                __ERR("decrypt %s error.\n", szEncryptData);
            }
            else
            {
                char *tmpDst = restore_with_escape(dst);
                if (tmpDst != NULL)
                {
                    StrCpy(adslCfg->password, ADSL_PASSWORD_MAX_LEN, tmpDst);
                    free(tmpDst);
                    tmpDst = NULL;
                }
                else
                {
                    adslCfg->password[0] = '\0';
                }
            }
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_network_g4_get(IXML_Node *pNode, G4Config *g4Cfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Manual"))
        {
            g4Cfg->is_manual = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "CurOperator"))
        {
            g4Cfg->cur_operator = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_network_ddns_get(IXML_Node *pNode, DDNSConfig *ddnsCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            ddnsCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Server"))
        {
            memset(ddnsCfg->server, '\0', MAX_DDNS_SERVER_NAME_LEN);
            StrCpy(ddnsCfg->server, MAX_DDNS_SERVER_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Domain"))
        {
            memset(ddnsCfg->domain, '\0', MAX_DDNS_DOMAIN_NAME_LEN);
            StrCpy(ddnsCfg->domain, MAX_DDNS_DOMAIN_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Username"))
        {
            memset(ddnsCfg->userName, '\0', MAX_DDNS_USERNAME_LEN);
            StrCpy(ddnsCfg->userName, MAX_DDNS_USERNAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Password"))
        {
            memset(ddnsCfg->password, '\0', MAX_DDNS_PASSWORD_LEN);
            StrCpy(ddnsCfg->password, MAX_DDNS_PASSWORD_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptPwd"))
        {
            memset(ddnsCfg->password, '\0', MAX_DDNS_PASSWORD_LEN);
            char szEncryptData[64];
            StrCpy(szEncryptData, 64, tmpAttr->nodeValue);
            char dst[32];
            int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
            if (ret != 0)
            {
                __ERR("decrypt %s error.\n", szEncryptData);
            }
            else
            {
                char *tmpDst = restore_with_escape(dst);
                if (tmpDst != NULL)
                {
                    StrCpy(ddnsCfg->password, MAX_DDNS_PASSWORD_LEN, tmpDst);
                    free(tmpDst);
                    tmpDst = NULL;
                }
                else
                {
                    ddnsCfg->password[0] = '\0';
                }
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "FreshInterval"))
        {
            ddnsCfg->freshInterval = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_network_upnp_get(IXML_Node *pNode, UPNPConfig *upnpCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            upnpCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

static int anj_config_network_p2p_get(IXML_Node *pNode, P2PConfig *pCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Type"))
        {
            pCfg->p2ptype = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Authcode"))
        {
            StrCpy(pCfg->authcode, P2P_AUTH_CODE_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

static int anj_config_network_encrypt_get(IXML_Node *pNode, EncryptionConfig *pPlatformCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Url"))
        {
            StrCpy(pPlatformCfg->url, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MachineId"))
        {
            StrCpy(pPlatformCfg->machineId, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MachineSalt"))
        {
            StrCpy(pPlatformCfg->machineSalt, GAT1400_ID_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ProductID"))
        {
            StrCpy(pPlatformCfg->productID, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ProductSecret"))
        {
            StrCpy(pPlatformCfg->productSecret, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "VkekInterval"))
        {
            pPlatformCfg->vkekInterval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "AuthPasswd"))
        {
            StrCpy(pPlatformCfg->authPasswd, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

static int anj_config_network_telecom_get(IXML_Node *pNode, TelecomDevParamInfo *pPlatformCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "SerialNo"))
        {
            StrCpy(pPlatformCfg->serialNo, sizeof(pPlatformCfg->serialNo), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DevFactory"))
        {
            StrCpy(pPlatformCfg->devFactory, sizeof(pPlatformCfg->devFactory), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DevType"))
        {
            StrCpy(pPlatformCfg->devType, sizeof(pPlatformCfg->devType), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ServerIp"))
        {
            StrCpy(pPlatformCfg->serverIp, sizeof(pPlatformCfg->serverIp), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ServerPort"))
        {
            StrCpy(pPlatformCfg->serverPort, sizeof(pPlatformCfg->serverPort), tmpAttr->nodeValue);
        }
#if 0
        else if(!strcmp(tmpAttr->nodeName, "EthMac"))
        {
            StrCpy(pPlatformCfg->ethMac, sizeof(pPlatformCfg->ethMac), tmpAttr->nodeValue);
        }
        else if(!strcmp(tmpAttr->nodeName, "FirmwareVer"))
        {
            StrCpy(pPlatformCfg->firmwareVer, sizeof(pPlatformCfg->firmwareVer), tmpAttr->nodeValue);
        }
#endif
        else if (!strcmp(tmpAttr->nodeName, "NetAdsl"))
        {
            StrCpy(pPlatformCfg->netAdsl, sizeof(pPlatformCfg->netAdsl), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryKey"))
        {
            StrCpy(pPlatformCfg->encryKey, sizeof(pPlatformCfg->encryKey), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "KeyIV"))
        {
            StrCpy(pPlatformCfg->keyIV, sizeof(pPlatformCfg->keyIV), tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

static int anj_config_network_gb35114_get(IXML_Node *pNode, Gb35114CertConfig *pPlatformCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "DevId"))
        {
            StrCpy(pPlatformCfg->devId, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DevCertIsAuth"))
        {
            pPlatformCfg->devCertIsAuth = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "certMode"))
        {
            pPlatformCfg->certMode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "P10CertType"))
        {
            pPlatformCfg->p10CertType = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DevCertType"))
        {
            pPlatformCfg->devCertType = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "SecretKeyType"))
        {
            pPlatformCfg->secretKeyType = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PlatCerType"))
        {
            pPlatformCfg->platCerType = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "InOrOutType"))
        {
            pPlatformCfg->inOrOutType = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FilePath"))
        {
            StrCpy(pPlatformCfg->filePath, UNICOM_STR_MAX_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

int anj_config_network_sercure_get(IXML_Node *pNode, VSEC_UKEY_INFO *pPlatformCfg)
{
    __ERR("Network_getSecereLoginConfig start\n");
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        __ERR("nodeName ==%s\n", tmpAttr->nodeName);
        if (!strcmp(tmpAttr->nodeName, "ret"))
        {
            pPlatformCfg->ret = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "curStepNum"))
        {
            pPlatformCfg->curStepNum = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ukeyId"))
        {
            StrCpy(pPlatformCfg->first_step.ukeyId, sizeof(pPlatformCfg->first_step.ukeyId), tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ukeyCertData"))
        {
            StrCpy(pPlatformCfg->first_step.ukeyCertData, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "devId"))
        {
            StrCpy(pPlatformCfg->second_step.devId, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "devCertData"))
        {
            StrCpy(pPlatformCfg->second_step.devCertData, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "devR1Num"))
        {
            StrCpy(pPlatformCfg->second_step.devR1Num, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "webR2Num"))
        {
            StrCpy(pPlatformCfg->third_step.webR2Num, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "webSign1"))
        {
            StrCpy(pPlatformCfg->third_step.webSign1, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "devSign2"))
        {
            StrCpy(pPlatformCfg->four_step.devSign2, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "cryptKey"))
        {
            StrCpy(pPlatformCfg->four_step.cryptKey, strlen(tmpAttr->nodeValue) + 1, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    __ERR("Network_getSecereLoginConfig end\n");
    return 0;
}

static int anj_config_network_pptp_get(IXML_Node *pNode, PPTPConfig *pptpCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pptpCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Server"))
        {
            memset(pptpCfg->vpnServerIp, '\0', MAX_VPN_SERVER_NAME_LEN);
            StrCpy(pptpCfg->vpnServerIp, MAX_VPN_SERVER_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Username"))
        {
            memset(pptpCfg->userName, '\0', MAX_VPN_USERNAME_LEN);
            StrCpy(pptpCfg->userName, MAX_VPN_USERNAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Password"))
        {
            memset(pptpCfg->password, '\0', MAX_VPN_PASSWORD_LEN);
            StrCpy(pptpCfg->password, MAX_VPN_PASSWORD_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptPwd"))
        {
            memset(pptpCfg->password, '\0', MAX_VPN_PASSWORD_LEN);
            char szEncryptData[64];
            StrCpy(szEncryptData, 64, tmpAttr->nodeValue);
            char dst[32];
            int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
            if (ret != 0)
            {
                __ERR("decrypt %s error.\n", szEncryptData);
            }
            else
            {
                char *tmpDst = restore_with_escape(dst);
                if (tmpDst != NULL)
                {
                    StrCpy(pptpCfg->password, MAX_VPN_PASSWORD_LEN, tmpDst);
                    free(tmpDst);
                    tmpDst = NULL;
                }
                else
                {
                    pptpCfg->password[0] = '\0';
                }
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "MTU"))
        {
            pptpCfg->mtu = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (pptpCfg->mtu < 1000 || pptpCfg->mtu > 1500)
        pptpCfg->mtu = 1460;

    return 0;
}

char *anj_config_network_lan_conver_xml(LANConfig *lanCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<LANConfig ");
    pb += snprintf(pb, pe - pb, "MacAddress=\"%s\" ", copy_with_escape(escapeBuf, (char *)lanCfg->MACAddress));
    pb += snprintf(pb, pe - pb, "DHCP=\"%d\" ", lanCfg->dhcpEnable);
    pb += snprintf(pb, pe - pb, "dhcpOffTime=\"%d\" ", lanCfg->dhcpOffTime);
    pb += snprintf(pb, pe - pb, "ALLNET=\"%d\" ", lanCfg->onvifAllnetEnable);
    pb += snprintf(pb, pe - pb, "IPAddress=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->IPAddress));
    pb += snprintf(pb, pe - pb, "Netmask=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->netMask));
    pb += snprintf(pb, pe - pb, "Gateway=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->gateWay));
    pb += snprintf(pb, pe - pb, "DNS1=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->DNS1));
    pb += snprintf(pb, pe - pb, "DNS2=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->DNS2));
    pb += snprintf(pb, pe - pb, "hostname=\"%s\" ", copy_with_escape(escapeBuf, lanCfg->hostname));
    pb += snprintf(pb, pe - pb, "MTU=\"%u\" ", lanCfg->mtu);

    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_network_wifi_conver_xml(WIFIConfig *wifiCfg)
{
    int maxSize = 2000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000] = {0};
    char essid[MAX_WIRELESS_ESSID_NAME_LEN] = {0};

    char essid_utf8[256] = {0};

    /* 为了兼容WEB - 先把原始字节转为 hex 放到 ESSID_UTF8 */
    memset(essid_utf8, 0, sizeof(essid_utf8));
    unsigned int i = 0;
    for (i = 0; i < strlen(wifiCfg->essid) && (strlen(essid_utf8) + 2) < sizeof(essid_utf8) - 1; i++)
    {
        char tmp[4];
        sprintf(tmp, "%02x", (unsigned char)wifiCfg->essid[i]);
        strcat(essid_utf8, tmp);
    }

    /* 尝试把 UTF-8 转为 GB2312；失败则使用空字符串（前端可使用 ESSID_UTF8 还原） */
    memset(essid, 0, sizeof(essid));
    if (utf8_to_gb2312(wifiCfg->essid, essid) != 0)
    {
        essid[0] = '\0';
    }

    if (strlen(wifiCfg->essid) > 0 && strlen(essid) == 0)
    {
        StrCpy(essid, sizeof(essid), wifiCfg->essid);
        __INFO("wifi essid:%s maybe use gb format, copy for xml!\n", essid);
    }

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<WIFIConfig\r\n");
    pb += snprintf(pb, pe - pb, "Version=\"2.0\"\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", wifiCfg->enable);
    pb += snprintf(pb, pe - pb, "DHCP=\"%d\"\r\n", wifiCfg->dhcpEnable);
    pb += snprintf(pb, pe - pb, "IPAddress=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->IPAddress));
    pb += snprintf(pb, pe - pb, "Netmask=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->netMask));
    pb += snprintf(pb, pe - pb, "Gateway=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->gateWay));
    pb += snprintf(pb, pe - pb, "OperationMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->operationMode));

    pb += snprintf(pb, pe - pb, "MacAddress=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->MACAddress));

    pb += snprintf(pb, pe - pb, "ChannelNumber=\"%d\"\r\n", wifiCfg->channelNum);
    pb += snprintf(pb, pe - pb, "Region=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->region));
    pb += snprintf(pb, pe - pb, "ESSID=\"%s\"\r\n", copy_with_escape(escapeBuf, essid));

    pb += snprintf(pb, pe - pb, "ESSID_UTF8=\"%s\"\r\n", copy_with_escape(escapeBuf, essid_utf8));

    pb += snprintf(pb, pe - pb, "BitRate=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->bitRate));
    pb += snprintf(pb, pe - pb, "MacMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->macMode));
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<WirelessEncrypt\r\n");
    // pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n", wifiCfg->enable);
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", wifiCfg->wirelessEncrypt.enable);
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<WEPEncrypt\r\n");
    pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.authMode));
    pb += snprintf(pb, pe - pb, "KeyIndex=\"%d\"\r\n", wifiCfg->wirelessEncrypt.wepEncrypt.keyIndex);
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, "KeyMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.keyMode));
    pb += snprintf(pb, pe - pb, "KeyValue=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.keyValue));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "<WPAEncrypt\r\n");
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.authMode));
    pb += snprintf(pb, pe - pb, "KeyValue=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.keyValue));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "</WirelessEncrypt>\r\n");

    // added by XXX 20110701
    pb += snprintf(pb, pe - pb, "<PingWatchConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", wifiCfg->pingWatchCfg.enable);
    pb += snprintf(pb, pe - pb, "PingAddress=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->pingWatchCfg.address));
    pb += snprintf(pb, pe - pb, "Interval=\"%d\"\r\n", wifiCfg->pingWatchCfg.interval);
    pb += snprintf(pb, pe - pb, "MaxFail=\"%d\"\r\n", wifiCfg->pingWatchCfg.maxFail);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "</WIFIConfig>\r\n");

    return buf;
}

char *anj_config_network_wifiap_conver_xml(WIFIApConfig *wifiCfg)
{
    int maxSize = 2000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<WIFIApConfig\r\n");
    pb += snprintf(pb, pe - pb, "Version=\"2.0\"\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", wifiCfg->enable);
    pb += snprintf(pb, pe - pb, "IPAddress=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->IPAddress));
    pb += snprintf(pb, pe - pb, "Netmask=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->netMask));

    pb += snprintf(pb, pe - pb, "MacAddress=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->MACAddress));

    pb += snprintf(pb, pe - pb, "ChannelNumber=\"%d\"\r\n", wifiCfg->channelNum);
    pb += snprintf(pb, pe - pb, "Region=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->region));
    pb += snprintf(pb, pe - pb, "ESSID=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->essid));
    pb += snprintf(pb, pe - pb, "BitRate=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->bitRate));
    pb += snprintf(pb, pe - pb, "MacMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->macMode));
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<WirelessEncrypt\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", wifiCfg->wirelessEncrypt.enable);
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<WEPEncrypt\r\n");
    pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.authMode));
    pb += snprintf(pb, pe - pb, "KeyIndex=\"%d\"\r\n", wifiCfg->wirelessEncrypt.wepEncrypt.keyIndex);
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, "KeyMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.keyMode));
    pb += snprintf(pb, pe - pb, "KeyValue=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wepEncrypt.keyValue));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "<WPAEncrypt\r\n");
    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.encryptType));
    pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.authMode));
    pb += snprintf(pb, pe - pb, "KeyValue=\"%s\"\r\n", copy_with_escape(escapeBuf, wifiCfg->wirelessEncrypt.wpaEncrypt.keyValue));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "</WirelessEncrypt>\r\n");

    pb += snprintf(pb, pe - pb, "</WIFIApConfig>\r\n");

    return buf;
}

char *anj_config_network_alarmserver_conver_xml(AlarmServerConfig *cfg, int bPwdEntrypt)
{
    int maxSize = 2000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<AlarmServerConfig\r\n");
    pb += snprintf(pb, pe - pb, "url=\"%s\"\r\n", copy_with_escape(escapeBuf, cfg->url));
    pb += snprintf(pb, pe - pb, "withattachment=\"%d\"\r\n", cfg->withattachment);
    pb += snprintf(pb, pe - pb, "Username=\"%s\"\r\n", cfg->userName);

    copy_with_escape(escapeBuf, cfg->password);
    char dst[64] = {0};
    int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
    if (0 == bPwdEntrypt || ret != 0)
    {
        if (ret != 0)
            __ERR("Encrypt %s error.", escapeBuf);
        pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", escapeBuf);
    }
    else
    {
        pb += snprintf(pb, pe - pb, "EncryptPwd=\"%s\"\r\n", dst);
    }

    pb += snprintf(pb, pe - pb, " />\r\n");

    return buf;
}

char *anj_config_network_adsl_conver_xml(ADSLConfigNew *adslCfg, int bPwdEntrypt)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<ADSLConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", adslCfg->enable);
    pb += snprintf(pb, pe - pb, "Username=\"%s\"\r\n", copy_with_escape(escapeBuf, adslCfg->userName));
    copy_with_escape(escapeBuf, adslCfg->password);
    char dst[64] = {0};
    int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
    if (0 == bPwdEntrypt || ret != 0)
    {
        if (ret != 0)
            __ERR("Encrypt %s error.", escapeBuf);
        pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", escapeBuf);
    }
    else
    {
        pb += snprintf(pb, pe - pb, "EncryptPwd=\"%s\"\r\n", dst);
    }

    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_network_pptp_conver_xml(PPTPConfig *pptpCfg, int bPwdEntrypt)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<PPTPConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pptpCfg->enable);
    pb += snprintf(pb, pe - pb, "Server=\"%s\"\r\n", pptpCfg->vpnServerIp);
    pb += snprintf(pb, pe - pb, "Username=\"%s\"\r\n", copy_with_escape(escapeBuf, pptpCfg->userName));

    copy_with_escape(escapeBuf, pptpCfg->password);
    char dst[64] = {0};
    int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
    if (0 == bPwdEntrypt || ret != 0)
    {
        if (ret != 0)
            __ERR("Encrypt %s error.", escapeBuf);
        pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", escapeBuf);
    }
    else
    {
        pb += snprintf(pb, pe - pb, "EncryptPwd=\"%s\"\r\n", dst);
    }

    pb += snprintf(pb, pe - pb, "MTU=\"%d\"\r\n", pptpCfg->mtu);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_network_ddns_conver_xml(DDNSConfig *ddnsCfg, int bPwdEntrypt)
{
    int maxSize = 2000;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[1000];

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<DDNSConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", ddnsCfg->enable);
    pb += snprintf(pb, pe - pb, "Server=\"%s\"\r\n", copy_with_escape(escapeBuf, ddnsCfg->server));
    pb += snprintf(pb, pe - pb, "Domain=\"%s\"\r\n", copy_with_escape(escapeBuf, ddnsCfg->domain));
    pb += snprintf(pb, pe - pb, "Username=\"%s\"\r\n", copy_with_escape(escapeBuf, ddnsCfg->userName));

    copy_with_escape(escapeBuf, ddnsCfg->password);
    char dst[64] = {0};
    int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
    if (0 == bPwdEntrypt || ret != 0)
    {
        if (ret != 0)
            __ERR("Encrypt %s error.", escapeBuf);
        pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", escapeBuf);
    }
    else
    {
        pb += snprintf(pb, pe - pb, "EncryptPwd=\"%s\"\r\n", dst);
    }

    pb += snprintf(pb, pe - pb, "FreshInterval=\"%d\"\r\n", ddnsCfg->freshInterval);

#if DDNS_NEW_FUNCTION
    char ddns_type_list[1024];
    GetDdnsTypeList(ddns_type_list, 1024);
    pb += snprintf(pb, pe - pb, "DdnsTypeList=\"%s\"\r\n", copy_with_escape(escapeBuf, ddns_type_list));
#endif

    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}
char *anj_config_network_upnp_conver_xml(UPNPConfig *upnpCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<UPNPConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", upnpCfg->enable);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_network_g4_conver_xml(G4Config *g4Cfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<G4Config\r\n");
    pb += snprintf(pb, pe - pb, "Manual=\"%d\"\r\n", g4Cfg->is_manual);
    pb += snprintf(pb, pe - pb, "CurOperator=\"%d\"\r\n", g4Cfg->cur_operator);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_network_p2p_conver_xml(P2PConfig *pCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<P2PConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "Type=\"%d\"\r\n", pCfg->p2ptype);
    pb += snprintf(pb, pe - pb, "Authcode=\"%s\"\r\n", pCfg->authcode);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_network_encryption_conver_xml(EncryptionConfig *pCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;
    pb += snprintf(pb, pe - pb, "<EncryptionConfig ");
    pb += snprintf(pb, pe - pb, "Url=\"%s\" ", pCfg->url);
    pb += snprintf(pb, pe - pb, "MachineId=\"%s\" ", pCfg->machineId);
    pb += snprintf(pb, pe - pb, "MachineSalt=\"%s\" ", pCfg->machineSalt);
    pb += snprintf(pb, pe - pb, "ProductID=\"%s\" ", pCfg->productID);
    pb += snprintf(pb, pe - pb, "ProductSecret=\"%s\" ", pCfg->productSecret);
    pb += snprintf(pb, pe - pb, "VkekInterval=\"%d\" ", pCfg->vkekInterval);
    pb += snprintf(pb, pe - pb, "AuthPasswd=\"%s\" ", pCfg->authPasswd);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}
char *anj_config_network_telecom_conver_xml(TelecomDevParamInfo *pCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;
    pb += snprintf(pb, pe - pb, "<TelecomDevParamInfo ");
    pb += snprintf(pb, pe - pb, "SerialNo=\"%s\" ", pCfg->serialNo);
    pb += snprintf(pb, pe - pb, "DevFactory=\"%s\" ", pCfg->devFactory);
    pb += snprintf(pb, pe - pb, "DevType=\"%s\" ", pCfg->devType);
    pb += snprintf(pb, pe - pb, "ServerIp=\"%s\" ", pCfg->serverIp);
    pb += snprintf(pb, pe - pb, "ServerPort=\"%s\" ", pCfg->serverPort);
    pb += snprintf(pb, pe - pb, "NetAdsl=\"%s\" ", pCfg->netAdsl);
    pb += snprintf(pb, pe - pb, "EncryKey=\"%s\" ", pCfg->encryKey);
    pb += snprintf(pb, pe - pb, "KeyIV=\"%s\" ", pCfg->keyIV);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}
char *anj_config_network_gb35114_conver_xml(Gb35114CertConfig *pCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;
    pb += snprintf(pb, pe - pb, "<GbCertificate ");
    pb += snprintf(pb, pe - pb, "certMode=\"%d\" ", pCfg->certMode);
    pb += snprintf(pb, pe - pb, "DevId=\"%s\" ", pCfg->devId);
    pb += snprintf(pb, pe - pb, "DevCertIsAuth=\"%d\" ", pCfg->devCertIsAuth);
    if (pCfg->devCertType != 0xff)
    {
        pb += snprintf(pb, pe - pb, "P10CertType=\"%d\" ", pCfg->p10CertType);
        pb += snprintf(pb, pe - pb, "DevCertType=\"%d\" ", pCfg->devCertType);
        pb += snprintf(pb, pe - pb, "SecretKeyType=\"%d\" ", pCfg->secretKeyType);
        pb += snprintf(pb, pe - pb, "PlatCerType=\"%d\" ", pCfg->platCerType);
        pb += snprintf(pb, pe - pb, "InOrOutType=\"%d\" ", pCfg->inOrOutType);
        pb += snprintf(pb, pe - pb, "FilePath=\"%s\" ", pCfg->filePath);
    }
    else
    {
        pCfg->devCertType = 0;
    }
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_network_vsec_conver_xml(VsecDevInfo *pCfg)
{
    int maxSize = 1000;
    char *pe;
    char *pb;
    char *buf;
    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;
    pb += snprintf(pb, pe - pb, "<VsecDevInfo ");
    pb += snprintf(pb, pe - pb, "digestAlg=\"%d\" ", pCfg->digestAlg);
    pb += snprintf(pb, pe - pb, "rand=\"%d\" ", pCfg->rand);
    pb += snprintf(pb, pe - pb, "fileIo=\"%d\" ", pCfg->fileIo);
    pb += snprintf(pb, pe - pb, "asymmetryAlg=\"%d\" ", pCfg->asymmetryAlg);
    pb += snprintf(pb, pe - pb, "symmetryAlg=\"%d\" ", pCfg->symmetryAlg);
    pb += snprintf(pb, pe - pb, "devStatus=\"%d\" ", pCfg->devStatus);
    pb += snprintf(pb, pe - pb, "isCert=\"%d\" ", pCfg->isCert);
    pb += snprintf(pb, pe - pb, "sdkVersion=\"%d\" ", pCfg->sdkVersion);
    pb += snprintf(pb, pe - pb, "sdkSn=\"%s\" ", pCfg->sdkSn);
    pb += snprintf(pb, pe - pb, "chipVersion=\"%s\" ", pCfg->chipVersion);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_network_secure_conver_xml(VSEC_UKEY_INFO *pCfg)
{
    int maxSize = 4096;
    char *pe;
    char *pb;
    char *buf;
    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;
    pb += snprintf(pb, pe - pb, "<SecureLoginInfo ");
    pb += snprintf(pb, pe - pb, "ret=\"%d\" ", pCfg->ret);
    pb += snprintf(pb, pe - pb, "curStepNum=\"%d\" ", pCfg->curStepNum);
    pb += snprintf(pb, pe - pb, "ukeyId=\"%s\" ", pCfg->first_step.ukeyId);
    pb += snprintf(pb, pe - pb, "ukeyCertData=\"%s\" ", pCfg->first_step.ukeyCertData);
    pb += snprintf(pb, pe - pb, "devId=\"%s\" ", pCfg->second_step.devId);
    pb += snprintf(pb, pe - pb, "devCertData=\"%s\" ", pCfg->second_step.devCertData);
    pb += snprintf(pb, pe - pb, "devR1Num=\"%s\" ", pCfg->second_step.devR1Num);
    pb += snprintf(pb, pe - pb, "webR2Num=\"%s\" ", pCfg->third_step.webR2Num);
    pb += snprintf(pb, pe - pb, "webSign1=\"%s\" ", pCfg->third_step.webSign1);
    pb += snprintf(pb, pe - pb, "devSign2=\"%s\" ", pCfg->four_step.devSign2);
    pb += snprintf(pb, pe - pb, "cryptKey=\"%s\" ", pCfg->four_step.cryptKey);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_network_status_4g_conver_xml(G4InfoStruct *pCfg)
{
    int maxSize = 1000;

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Network4GStatus\r\n");
    pb += snprintf(pb, pe - pb, "Manufacturer=\"%s\"\r\n", pCfg->Manufacturer);
    pb += snprintf(pb, pe - pb, "Model=\"%s\"\r\n", pCfg->Model);
    pb += snprintf(pb, pe - pb, "ICCID=\"%s\"\r\n", pCfg->ICCID);
    pb += snprintf(pb, pe - pb, "IMEI=\"%s\"\r\n", pCfg->IMEI);
    pb += snprintf(pb, pe - pb, "IMSI=\"%s\"\r\n", pCfg->IMSI);
    pb += snprintf(pb, pe - pb, "MSISDN=\"%s\"\r\n", pCfg->MSISDN);
    pb += snprintf(pb, pe - pb, "WorkMode=\"%s\"\r\n", pCfg->WorkMode);
    pb += snprintf(pb, pe - pb, "Operator=\"%s\"\r\n", pCfg->Operator);
    pb += snprintf(pb, pe - pb, "nSvrStatus=\"%d\"\r\n", pCfg->nSvrStatus);
    pb += snprintf(pb, pe - pb, "nDialStatus=\"%d\"\r\n", pCfg->nDialStatus);
    pb += snprintf(pb, pe - pb, "nSimStatus=\"%d\"\r\n", pCfg->nSimStatus);
    pb += snprintf(pb, pe - pb, "nSignalLevel=\"%d\"\r\n", pCfg->nSignalLevel);
    if (strlen(pCfg->IccidList) > 0)
    {
        pb += snprintf(pb, pe - pb, "IccidList=\"%s\"\r\n", pCfg->IccidList);
    }
    if (strlen(pCfg->ImsiList) > 0)
    {
        pb += snprintf(pb, pe - pb, "ImsiList=\"%s\"\r\n", pCfg->ImsiList);
    }

    if (strlen(pCfg->Revision) > 0)
    {
        pb += snprintf(pb, pe - pb, "Revision=\"%s\"\r\n", pCfg->Revision);
    }
    pb += snprintf(pb, pe - pb, "Manual=\"%d\"\r\n", pCfg->ManualMode);
    pb += snprintf(pb, pe - pb, "CurOperator=\"%d\"\r\n", pCfg->CurOperator);

    if (strlen(pCfg->P2PID) > 0)
    {
        pb += snprintf(pb, pe - pb, "P2PID=\"%s\"\r\n", pCfg->P2PID);
    }

    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_network_conver_xml(NetworkConfigNew *pNetworkCfg)
{
    int initSize = 1000;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<NetworkConfig>\r\n");

    // lan
    curPos = pb - buf;
    tmp = anj_config_network_lan_conver_xml(&(pNetworkCfg->lanCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // wifi
    curPos = pb - buf;
    tmp = anj_config_network_wifi_conver_xml(&(pNetworkCfg->wifiCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // wifiap
    curPos = pb - buf;
    tmp = anj_config_network_wifiap_conver_xml(&(pNetworkCfg->wifiApCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_network_alarmserver_conver_xml(&(pNetworkCfg->alarmServerCfg), 1);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // adsl
    curPos = pb - buf;
    tmp = anj_config_network_adsl_conver_xml(&(pNetworkCfg->adslCfg), 1);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // ddns
    curPos = pb - buf;
    tmp = anj_config_network_ddns_conver_xml(&(pNetworkCfg->ddnsCfg), 1);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // upnp
    curPos = pb - buf;
    tmp = anj_config_network_upnp_conver_xml(&(pNetworkCfg->upnpCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // g4
    curPos = pb - buf;
    tmp = anj_config_network_g4_conver_xml(&pNetworkCfg->g4Cfg);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // pptp
    curPos = pb - buf;
    tmp = anj_config_network_pptp_conver_xml(&(pNetworkCfg->pptpCfg), 1);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    // p2p
    curPos = pb - buf;
    tmp = anj_config_network_p2p_conver_xml(&(pNetworkCfg->p2pCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_network_encryption_conver_xml(&(pNetworkCfg->encryptionConfig));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);
    curPos = pb - buf;
    tmp = anj_config_network_gb35114_conver_xml(&(pNetworkCfg->gb35114CertConfig));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);
    curPos = pb - buf;
    tmp = anj_config_network_telecom_conver_xml(&(pNetworkCfg->telecomDevParamInfo));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</NetworkConfig>\r\n");

    return buf;
}

int anj_config_network_default(NetworkConfigNew *pNetworkCfg)
{
    int iRet = 0;
    ANJ_CHK(pNetworkCfg != NULL, -1, "input Invalid");
    memset(pNetworkCfg, 0, sizeof(NetworkConfigNew));

    pNetworkCfg->p2pCfg.enable = 1;
    pNetworkCfg->p2pCfg.p2ptype = P2P_TYPE_AC18PRO;

    strcpy(pNetworkCfg->lanCfg.hostname, "");
    pNetworkCfg->lanCfg.dhcpOffTime = -1;
    pNetworkCfg->lanCfg.mtu = 1460;

endFunc:
    return iRet;
}

int anj_config_network_lan_default(LANConfig *lanCfg)
{
    int iRet = 0;
    ANJ_CHK(lanCfg != NULL, -1, "input Invalid");
    memset(lanCfg, 0, sizeof(LANConfig));

    strcpy(lanCfg->hostname, "");
    lanCfg->dhcpOffTime = -1;
    lanCfg->mtu = 1460;

endFunc:
    return iRet;
}

int anj_config_network_get(IXML_Node *pNode, NetworkConfigNew *pNetworkCfg)
{
    IXML_Node *pChildNode = pNode->firstChild;

    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "LANConfig"))
        {
            anj_config_network_lan_get(pChildNode, &(pNetworkCfg->lanCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "WIFIConfig"))
        {
            anj_config_network_wifi_get(pChildNode, &(pNetworkCfg->wifiCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "WIFIApConfig"))
        {
            anj_config_network_wifiap_get(pChildNode, &(pNetworkCfg->wifiApCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "AlarmServerConfig"))
        {
            anj_config_network_alarmserver_get(pChildNode, &(pNetworkCfg->alarmServerCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "ADSLConfig"))
        {
            anj_config_network_adsl_get(pChildNode, &(pNetworkCfg->adslCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "DDNSConfig"))
        {
            anj_config_network_ddns_get(pChildNode, &(pNetworkCfg->ddnsCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "UPNPConfig"))
        {
            anj_config_network_upnp_get(pChildNode, &(pNetworkCfg->upnpCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "P2PConfig"))
        {
            anj_config_network_p2p_get(pChildNode, &(pNetworkCfg->p2pCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "G4Config"))
        {
            anj_config_network_g4_get(pChildNode, &(pNetworkCfg->g4Cfg));
        }
        else if (!strcmp(pChildNode->nodeName, "PPTPConfig"))
        {
            anj_config_network_pptp_get(pChildNode, &(pNetworkCfg->pptpCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "EncryptionConfig"))
        {
            anj_config_network_encrypt_get(pChildNode, &(pNetworkCfg->encryptionConfig));
        }
        else if (!strcmp(pChildNode->nodeName, "GbCertificate"))
        {
            anj_config_network_gb35114_get(pChildNode, &(pNetworkCfg->gb35114CertConfig));
        }
        else if (!strcmp(pChildNode->nodeName, "TelecomDevParamInfo"))
        {
            anj_config_network_telecom_get(pChildNode, &(pNetworkCfg->telecomDevParamInfo));
        }

        pChildNode = pChildNode->nextSibling;
    }

    return 0;
}

int anj_config_network_save(NetworkConfigNew *pNetworkCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_network_conver_xml(pNetworkCfg);
    iRet = anj_config_save_node(pDataXml, "<NetworkConfig>", "</NetworkConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_network_set(NetworkConfigNew *pstNetworkCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    if (memcmp(networkCfg, pstNetworkCfg, sizeof(NetworkConfigNew)))
    {
        // Todo
        // if (anj_audio_ctrl() == 0)
        {
            memcpy(networkCfg, pstNetworkCfg, sizeof(NetworkConfigNew));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

/*
int anj_config_network_IPAcquireConfig(SYSTEM_MSG_BUF *pMsg)
{
    int ret;
    NetworkConfig *networkCfg = &(gp_config->networkCfg);
    char *buf = NULL;

    __INFO("pMsg->flag= %d \n",pMsg->flag);

    if(pMsg->flag == 0)
    {
        buf = (char*)malloc(pMsg->length);
    }
    else if(pMsg->flag == 1)
    {
        buf = (char*)malloc(pMsg->length + 1);
        buf[pMsg->length] = '\0';
    }

    ShmRead(pMsg->offset, buf, pMsg->length);

    if(pMsg->flag == 0)
    {
        memcpy(&(networkCfg->ipAcquireCfg),buf, pMsg->length);
    }
    else if(pMsg->flag == 1)
    {
        ret = Network_getIPAcquireCfgByXml(&(networkCfg->ipAcquireCfg),buf);
        if(ret != 0)
        {
            ERR_RETURN
        }
    }

    ret = 0; SetConfigSaveNeeded(); //save after msg handle finished. if save here, will block msg //anj_config_network_save(networkCfg);
    if(ret != 0)
    {
        ERR_RETURN
    }

    free(buf);
    pMsg->ret = 0;
    pMsg->length = 0;

    return 0;
}

int anj_config_network_CommonConfig(SYSTEM_MSG_BUF *pMsg)
{
    int ret;
    NetworkConfig *networkCfg = &(gp_config->networkCfg);
    char *buf = NULL;

    __INFO("pMsg->flag= %d \n",pMsg->flag);

    if(pMsg->flag == 0)
    {
        buf = (char*)malloc(pMsg->length);
    }
    else if(pMsg->flag == 1)
    {
        buf = (char*)malloc(pMsg->length + 1);
        buf[pMsg->length] = '\0';
    }

    ShmRead(pMsg->offset, buf, pMsg->length);

    if(pMsg->flag == 0)
    {
        memcpy(&(networkCfg->commonCfg),buf, pMsg->length);
    }
    else if(pMsg->flag == 1)
    {
        ret = Network_getCmnCfgByXml(&(networkCfg->commonCfg),buf);
        if(ret != 0)
        {
            ERR_RETURN
        }
    }

    ret = 0; SetConfigSaveNeeded(); //save after msg handle finished. if save here, will block msg //anj_config_network_save(networkCfg);
    if(ret != 0)
    {
        ERR_RETURN
    }

    free(buf);
    pMsg->ret = 0;
    pMsg->length = 0;

    return 0;
}
*/

int anj_config_network_lan_set(LANConfig *pstLanCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();
    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    LANConfig *lanCfg = &networkCfg->lanCfg;
    if (memcmp(lanCfg, pstLanCfg, sizeof(LANConfig)))
    {
        memcpy(lanCfg, pstLanCfg, sizeof(LANConfig));
        anj_config_network_save(networkCfg);
        anj_net_set();
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_wifi_set(WIFIConfig *pstWifiCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    WIFIConfig *wifiCfg = &networkCfg->wifiCfg;
    if (memcmp(wifiCfg, pstWifiCfg, sizeof(WIFIConfig)))
    {
        // Todo
        {
            memcpy(wifiCfg, pstWifiCfg, sizeof(WIFIConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_wifiap_set(WIFIApConfig *pstWifiApCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    WIFIApConfig *wifiApCfg = &networkCfg->wifiApCfg;
    if (memcmp(wifiApCfg, pstWifiApCfg, sizeof(WIFIApConfig)))
    {
        // Todo
        {
            memcpy(wifiApCfg, pstWifiApCfg, sizeof(WIFIApConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_alarmserver_set(AlarmServerConfig *pstAlarmServerCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    AlarmServerConfig *alarmServerCfg = &networkCfg->alarmServerCfg;
    if (memcmp(alarmServerCfg, pstAlarmServerCfg, sizeof(AlarmServerConfig)))
    {
        // Todo
        {
            memcpy(alarmServerCfg, pstAlarmServerCfg, sizeof(AlarmServerConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_adsl_set(ADSLConfigNew *pstAdslCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    ADSLConfigNew *adslCfg = &networkCfg->adslCfg;
    if (memcmp(adslCfg, pstAdslCfg, sizeof(ADSLConfigNew)))
    {
        // Todo
        {
            memcpy(adslCfg, pstAdslCfg, sizeof(ADSLConfigNew));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_g4_set(G4Config *pstG4Cfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    G4Config *g4Cfg = &networkCfg->g4Cfg;
    if (memcmp(g4Cfg, pstG4Cfg, sizeof(G4Config)))
    {
        if (pstG4Cfg->is_manual)
        {
            int simIndex = pstG4Cfg->cur_operator;
            EventResult event_result = {0};
            eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_SIM_SET, &event_result, (void *)&simIndex);
        }
        
        memcpy(g4Cfg, pstG4Cfg, sizeof(G4Config));
        anj_config_network_save(networkCfg);
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_pptp_set(PPTPConfig *pstPptpCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    PPTPConfig *pptpCfg = &networkCfg->pptpCfg;
    if (memcmp(pptpCfg, pstPptpCfg, sizeof(PPTPConfig)))
    {
        // Todo
        {
            memcpy(pptpCfg, pstPptpCfg, sizeof(PPTPConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_ddns_set(DDNSConfig *pstDdnsCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    DDNSConfig *ddnsCfg = &networkCfg->ddnsCfg;
    if (memcmp(ddnsCfg, pstDdnsCfg, sizeof(DDNSConfig)))
    {
        // Todo
        {
            memcpy(ddnsCfg, pstDdnsCfg, sizeof(DDNSConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_upnp_set(UPNPConfig *pstUpnpCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    UPNPConfig *upnpCfg = &networkCfg->upnpCfg;
    if (memcmp(upnpCfg, pstUpnpCfg, sizeof(UPNPConfig)))
    {
        // Todo
        {
            memcpy(upnpCfg, pstUpnpCfg, sizeof(UPNPConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_network_p2p_set(P2PConfig *pstP2pCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    P2PConfig *p2pCfg = &networkCfg->p2pCfg;
    if (memcmp(p2pCfg, pstP2pCfg, sizeof(P2PConfig)))
    {
        // Todo
        {
            memcpy(p2pCfg, pstP2pCfg, sizeof(P2PConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_encryption_set(EncryptionConfig *pstEncryptionConfig)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    EncryptionConfig *encryptionConfig = &networkCfg->encryptionConfig;
    if (memcmp(encryptionConfig, pstEncryptionConfig, sizeof(EncryptionConfig)))
    {
        // Todo
        {
            memcpy(encryptionConfig, pstEncryptionConfig, sizeof(EncryptionConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_gb35114_set(Gb35114CertConfig *pstGb35114CertConfig)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    Gb35114CertConfig *gb35114CertConfig = &networkCfg->gb35114CertConfig;
    if (memcmp(gb35114CertConfig, pstGb35114CertConfig, sizeof(Gb35114CertConfig)))
    {
        // Todo
        {
            memcpy(gb35114CertConfig, pstGb35114CertConfig, sizeof(Gb35114CertConfig));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_telecom_set(TelecomDevParamInfo *pstTelecomDevParamInfo)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    TelecomDevParamInfo *telecomDevParamInfo = &networkCfg->telecomDevParamInfo;
    if (memcmp(telecomDevParamInfo, pstTelecomDevParamInfo, sizeof(TelecomDevParamInfo)))
    {
        // Todo
        {
            memcpy(telecomDevParamInfo, pstTelecomDevParamInfo, sizeof(TelecomDevParamInfo));
            anj_config_network_save(networkCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_network_load(NetworkConfigNew *pNetworkCfg)
{
    return anj_config_load("NetworkConfig", pNetworkCfg, CONFIG_FILE_PATH);
}

int anj_config_network_adsl_get_by_xml(ADSLConfigNew *adslCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "ADSLConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_adsl_get(pNodelist->nodeItem, adslCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_alarmserver_get_by_xml(AlarmServerConfig *cfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    int ret = 0;

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AlarmServerConfig");
    if (pNodelist != NULL)
    {
        ret = anj_config_network_alarmserver_get(pNodelist->nodeItem, cfg);

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);

        return ret;
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_ddns_get_by_xml(DDNSConfig *ddnsCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DDNSConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_ddns_get(pNodelist->nodeItem, ddnsCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_encrypt_get_by_xml(EncryptionConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "EncryptionConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_encrypt_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_g4_get_by_xml(G4Config *g4Cfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "G4Config");
    if (pNodelist != NULL)
    {
        anj_config_network_g4_get(pNodelist->nodeItem, g4Cfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_gb35114_get_by_xml(Gb35114CertConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "GbCertificate");
    if (pNodelist != NULL)
    {
        anj_config_network_gb35114_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_lan_get_by_xml(LANConfig *lanCfg, char *xmlBuf, int bHaveOldCfg)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "LANConfig");
    if (pNodelist != NULL)
    {
        if (0 == bHaveOldCfg)
        {
            anj_config_network_lan_default(lanCfg);
        }

        anj_config_network_lan_get(pNodelist->nodeItem, lanCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_p2p_get_by_xml(P2PConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "P2PConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_p2p_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_pptp_get_by_xml(PPTPConfig *pptpCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "PPTPConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_pptp_get(pNodelist->nodeItem, pptpCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_sercure_get_by_xml(VSEC_UKEY_INFO *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "SecureLoginInfo");
    if (pNodelist != NULL)
    {
        anj_config_network_sercure_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }
    return 0;
}

int anj_config_network_telecom_get_by_xml(TelecomDevParamInfo *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "TelecomDevParamInfo");
    if (pNodelist != NULL)
    {
        anj_config_network_telecom_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_upnp_get_by_xml(UPNPConfig *upnpCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "UPNPConfig");
    if (pNodelist != NULL)
    {
        anj_config_network_upnp_get(pNodelist->nodeItem, upnpCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_wifiap_get_by_xml(WIFIApConfig *wifiCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    int ret = 0;

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "WIFIApConfig");
    if (pNodelist != NULL)
    {
        ret = anj_config_network_wifiap_get(pNodelist->nodeItem, wifiCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
        return ret;
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_wifi_get_by_xml(WIFIConfig *wifiCfg, char *xmlBuf)
{
    int ret = 0;

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "WIFIConfig");
    if (pNodelist != NULL)
    {
        ret = anj_config_network_wifi_get(pNodelist->nodeItem, wifiCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
        return ret;
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_network_get_by_xml(NetworkConfigNew *pNetworkCfg, char *xmlBuf, int bHaveOldCfg)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "NetworkConfig");
    if (pNodelist != NULL)
    {
        if (0 == bHaveOldCfg)
        {
            anj_config_network_default(pNetworkCfg);
        }

        anj_config_network_get(pNodelist->nodeItem, pNetworkCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}
