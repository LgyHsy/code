#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "ixml.h"
#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_module.h"
#include "cmd_def.h"
#include "file_receiver.h"
#include "file_sender.h"
#include "anj_pri_cmd.h"
#include "anj_service.h"
#include "anj_service_upgrade.h"
#include "cmd_def.h"

#include "anj_audio.h"

void anj_service_upgrade_prepare_memory(int MsgSrc)
{
    /*
     * Recovery keeps pri/net/service alive while the PC uploads firmware.
     * Full module teardown runs later in anj_sysmng_app_update apply path.
     */
    (void)MsgSrc;
    return;
}

int anj_service_upgrade_handle_upload_file(IXML_Document *pDoc, int MsgSrc, int lognum, char **msg_body)
{
    int iRet = 0;
    char *request_param = GetRequestParamValue(pDoc, (char *)"FileType");

    if (request_param == NULL)
    {
        return -1;
    }

    long file_type = atol(request_param);
    anj_mw_free(request_param);

    char *upload_file = GetRequestParamValue(pDoc, (char *)"FilePath");
    if (upload_file == NULL)
    {
        return -2;
    }

    if ((file_type != UPLOAD_CONFIG_FILE_TYPE) && (file_type != UPLOAD_FIRMWARE_FILE_TYPE) &&
        (file_type != UPLOAD_OEM_APP_FILE_TYPE) && (file_type != UPLOAD_OEM_MP3_FILE_TYPE) &&
        (file_type != UPLOAD_OEM_LOGO_FILE_TYPE) && (file_type != UPLOAD_CERTIFICATION_FILE_TYPE) &&
        (file_type != UPLOAD_KEY_FILE_TYPE) && (file_type != UPLOAD_OEM_CFG_FILE_TYPE) &&
        (file_type != UPLOAD_CONFIG_XML_FILE_TYPE))
    {
        anj_mw_free(upload_file);
        return -3;
    }

    if (file_type == UPLOAD_FIRMWARE_FILE_TYPE)
    {
        anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_START_UPDATE, 1);
        int totalmen = anj_sysmng_get_totalmem();
        int freemem = anj_sysmng_get_freemem();
        __INFO("freemem=%d kB, totalmen=%d kB, msgSrc:%d.\n", freemem, totalmen, MsgSrc);
    }

    char local_file[256] = {0};
    if (file_type == UPLOAD_FIRMWARE_FILE_TYPE)
    {
        snprintf(local_file, sizeof(local_file), "/tmp/ota_firmware.bin");
    }
    else
    {
        struct timeval tv;
        SystemGetTimeofRun(&tv, NULL);
        sprintf(local_file, "/tmp/upfile_%d_%d.dat", (int)tv.tv_sec, (int)tv.tv_usec);
    }

    char *length_param = GetRequestParamValue(pDoc, (char *)"FileLength");
    if (length_param == NULL)
    {
        unsigned short port = 0;
        if (MsgSrc == MSG_SRC_PRI)
        {
            port = anj_pri_cmd_port_get(0);
        }
        if (port == 0)
        {
            __ERR("find anj_mw_free tcp port failed.\n");
            anj_mw_free(upload_file);
            return -4;
        }

        iRet = file_sender_transport(1, file_type, local_file, upload_file, port, lognum);
        anj_mw_free(upload_file);
        *msg_body = anj_mw_malloc(1024);
        if (*msg_body)
        {
            sprintf(*msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\n/>", port);
        }
        return iRet;
    }

    unsigned long filelen = atoi(length_param);
    anj_mw_free(length_param);
    __INFO("filelen=%lu, local_file=%s, recv on pri port\n", filelen, local_file);

    iRet = 0;

    if (file_type == UPLOAD_CONFIG_FILE_TYPE)
    {
        if (filelen > (1024 * 1024))
        {
            __ERR("filelen = %lu > 1024*1024 for config file!!!\n", filelen);
            iRet = -5;
        }
    }
    else if (file_type == UPLOAD_OEM_MP3_FILE_TYPE || file_type == UPLOAD_OEM_APP_FILE_TYPE)
    {
        char *szFileName = GetFileNameFromFullName(upload_file);
        __ERR("filename %s\n", szFileName);

        if (strlen(szFileName) == 0)
        {
            iRet = -7;
        }
        else
        {
            if (file_type == UPLOAD_OEM_MP3_FILE_TYPE)
            {
                if (filelen > UPLOAD_MP3_TO_CFG_MTD_MAX_FILE_SIZE)
                {
                    __ERR("filelen %ld too big.\n", filelen);
                    iRet = -6;
                }
                else
                {
                    unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
                    if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
                    {
                        __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
                        iRet = -7;
                    }
                    else
                    {
                        char targetName[128] = {0};
                        snprintf(targetName, sizeof(targetName), "%s/mp3/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME);
                        snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME);
                        if (!anj_mw_file_exists(local_file))
                        {
                            __ERR("File %s isn't exist, will upload.\n", local_file);
                            remove(targetName);
                            readlink(targetName, local_file, sizeof(local_file));
                        }
                        else if (!anj_mw_file_exists(targetName))
                        {
                            readlink(targetName, local_file, sizeof(local_file));
                        }
                        __INFO("local_file %s\n", local_file);
                    }
                }
            }
            else
            {
                snprintf(local_file, sizeof(local_file), "/tmp/%s", szFileName);
                if (strstr(szFileName, "ispdaybin_"))
                    snprintf(local_file, sizeof(local_file), "/tmp/isp_day.bin");
                else if (strstr(szFileName, "ispnightbin_"))
                    snprintf(local_file, sizeof(local_file), "/tmp/isp_night.bin");
                else if (strstr(szFileName, "aiispbin_"))
                    snprintf(local_file, sizeof(local_file), "/tmp/aiisp.bin");
                __ERR("local_file %s\n", local_file);
            }

            if (iRet >= 0 && strlen(local_file) > 0 && file_type == UPLOAD_OEM_MP3_FILE_TYPE)
            {
                char *pUnicodeDesc = GetRequestParamValue(pDoc, (char *)"UnicodeDesc");
                if (pUnicodeDesc != NULL)
                {
                    char szDescrptionFileName[256] = {0};
                    snprintf(szDescrptionFileName, sizeof(szDescrptionFileName), "%s", local_file);
                    char *last_dot = strrchr(szDescrptionFileName, '.');
                    if (last_dot != NULL)
                        strcpy(last_dot, ".txt");
                    else
                        strncat(szDescrptionFileName, ".txt", sizeof(szDescrptionFileName) - strlen(szDescrptionFileName) - 1);

                    int hexstrlen = strlen(pUnicodeDesc);
                    if (hexstrlen > 4 &&
                        tolower(pUnicodeDesc[0]) == 'f' &&
                        tolower(pUnicodeDesc[1]) == 'f' &&
                        tolower(pUnicodeDesc[2]) == 'f' &&
                        tolower(pUnicodeDesc[3]) == 'e')
                    {
                        int outbufferlen = (hexstrlen >> 1);
                        char *outbuffer = anj_mw_malloc(outbufferlen);
                        memset(outbuffer, 0, outbufferlen);
                        hexStrToUInt(pUnicodeDesc, hexstrlen, (unsigned char *)outbuffer);
                        anj_mw_write_file(szDescrptionFileName, 0, outbuffer, strlen(outbuffer));
                        anj_mw_free(outbuffer);
                    }
                    anj_mw_free(pUnicodeDesc);
                }
            }
        }
    }
    else if (file_type == UPLOAD_OEM_LOGO_FILE_TYPE)
    {
        char *szFileName = GetFileNameFromFullName(upload_file);
        if (strlen(szFileName) == 0)
        {
            iRet = -7;
        }
        else
        {
            unsigned long long freespace = GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
            __ERR("anj_mw_free space %llu\n", freespace);
            if (filelen > freespace + 100 * 1024)
            {
                __ERR("filelen = %lu, freespace %lld, dangerous, egnore!\n", filelen, freespace);
                iRet = -5;
            }
            else
            {
                snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, szFileName);
                __ERR("local_file %s\n", local_file);
            }
        }
    }
    else if (file_type == UPLOAD_CERTIFICATION_FILE_TYPE)
    {
        unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
        if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
        {
            __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
            iRet = -7;
        }
        else
        {
            snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_CERTIFICATION_FILE_NAME);
            __ERR("local_file %s\n", local_file);
        }
    }
    else if (file_type == UPLOAD_KEY_FILE_TYPE)
    {
        unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
        if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
        {
            __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
            iRet = -7;
        }
        else
        {
            snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_KEY_FILE_NAME);
            __ERR("local_file %s\n", local_file);
        }
    }
    else if (file_type == UPLOAD_FIRMWARE_FILE_TYPE)
    {
        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
        {
            char cmd[100] = {0};
            snprintf(cmd, sizeof(cmd), "echo -e -n \"\\xFC\\x01\\xA3\\x01\\x00\\xA1\" > /dev/ttyS2");
            anj_mw_system(cmd);
        }
    }

    anj_mw_free(upload_file);

    if (iRet == 0)
    {
        iRet = file_recver_init(local_file, filelen, file_type, lognum, MsgSrc);
        __INFO("file_recver_init ret=%d, file=%s, len=%lu\n", iRet, local_file, filelen);
        if (iRet == 0)
        {
            *msg_body = anj_mw_malloc(1024);
            if (*msg_body)
            {
                MediaStreamConfig *pstStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
                int port = (MsgSrc == MSG_SRC_SER) ? 8091 : pstStreamCfg->commConfig.ptzPort;
                if (port <= 0)
                {
                    port = anj_pri_cmd_port_get(0);
                }
                sprintf(*msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\nType=\"1\"\n/>", port);
                __INFO("upload ready, response port=%d\n", port);
            }
        }
    }

    return iRet;
}
