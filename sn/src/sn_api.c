#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"

#include "anj_config.h"
#include "anj_sysmng.h"
#include "sn_header.h"
#include "sn_utils.h"

static int clear_config()
{
    DIR *dir = NULL;	
    struct dirent *s_dir = NULL;
    dir = opendir("/mnt/nand"); 
    if(dir == NULL)
    {
        return -1;
    }

    while(1)
    {
        s_dir=readdir(dir);
        if(s_dir==NULL)
        {
            break;
        }

        if( strcmp(s_dir->d_name, "." ) == 0)
            continue;

        if( strcmp(s_dir->d_name, ".." ) == 0 )
            continue;

        if( strcmp(s_dir->d_name, AJ_CUST_PATH_NAME) == 0 )
            continue;

        if( strcmp(s_dir->d_name, "rom_md5.txt") == 0 )
            continue;

        if( strstr(s_dir->d_name, "uboot_flag_") != NULL )
            continue;

        if( strstr(s_dir->d_name, "IPLX_flag_") != NULL )
            continue;

        if( strstr(s_dir->d_name, "IPL_flag_") != NULL )
            continue;

        if( strstr(s_dir->d_name, "IPL_CUST_flag_") != NULL )
            continue;

        mysystem_with_param("rm -fr /mnt/nand/%s", s_dir->d_name);
    }

    closedir(dir);			
    return 0;
}

int soft_enc_clear()
{
    char buf[256] = {0};
    char sn_str[32] = {0};

    if (ReadEncriptDataFromSoft((unsigned char *)buf, sizeof(buf)) != 0)
    {
        __INFO("Read softsn failed!\n");
    }
    else
    {
        int i = 0;
        strcpy(sn_str, "");
        for (i = 0; i < 8; i++)
        {
            sprintf(sn_str + strlen(sn_str), "%02X", (unsigned char)buf[i]);
        }
        __INFO("Read softsn:%s\n", sn_str);
    }

    if(ClearEncriptDataToSoft(-1) == 0)
    {
        __INFO("Clear softsn OK \n");
    }
    else
    {
        __INFO("Clear softsn Failed \n");
    }

    if( 0 == clear_config() )
    {
        __INFO("Clear config OK \n");
    }
    else
    {
        __INFO("Clear config Failed \n");
    }

    return 0;
}

int soft_enc_show()
{
    char buf[256] = {0};
    char sn_str[32] = {0};

    const char *p = get_uuid();

    __INFO("Get uuid:%s\n", p);

    if (ReadEncriptDataFromSoft((unsigned char *)buf, sizeof(buf)) != 0)
    {
        __INFO("Read softsn failed!\n");
    }
    else
    {
        int i = 0;
        strcpy(sn_str, "");
        for (i = 0; i < 8; i++)
        {
            sprintf(sn_str + strlen(sn_str), "%02X", (unsigned char)buf[i]);
        }
        __INFO("Read softsn:%s\n", sn_str);
    }

    char buffer[1024] = {0};
    int iRet = ReadPdLicense(buffer, sizeof(buffer));
    if (iRet > 0)
    {
        __INFO("PD license:%s\n", buffer);
    }

	iRet = ReadPdMadpLicense(buffer, sizeof(buffer));
	if(iRet > 0)
	{
        __INFO("PD madp license:%s\n", buffer);		
        debug_show_data_hex((unsigned char*)buffer, iRet, 0);
	}

	iRet = ReadP2pID(buffer, sizeof(buffer), 0);
	if(iRet > 0)
	{
        __INFO("P2P danale conf:%s\n", buffer);
        debug_show_data_hex((unsigned char*)buffer, iRet, 0);
	}

    iRet = ReadP2pID(buffer, sizeof(buffer), 1);
    if(iRet > 0)
    {
        __INFO("P2P goolink conf:%s\n", buffer);
        debug_show_data_hex((unsigned char*)buffer, iRet, 0);
    }			

    iRet = ReadP2pID(buffer, sizeof(buffer), 2);
    if(iRet > 0)
    {
        __INFO("P2P eyeplus conf:%s\n", buffer);
        debug_show_data_hex((unsigned char*)buffer, iRet, 0);
    }

    iRet = ReadP2pID(buffer, sizeof(buffer), 3);
    if(iRet > 0)
    {
        __INFO("P2P tutk conf:%s\n", buffer);
        debug_show_data_hex((unsigned char*)buffer, iRet, 0);
    }

    return 0;
}

int soft_enc_request_p2p(const char *param)
{
    int len = 0;
    char sn_str[32] = {0};
    char uuid_str[32] = {0};

    char *pbuf = (char *)anj_mw_malloc(4096);

    anj_sysmng_load_enc_sn(sn_str, sizeof(sn_str));

    const char *p = get_uuid();
    if( p != NULL )
    {
        strncpy(uuid_str, p, sizeof(uuid_str) - 1);
    }

    if(access(SN_RANDOM_FLAG, F_OK) == F_OK)
    {
        __ERR("copyright not OK\n");
        anj_mw_free(pbuf);
        return -1;
    }               

    int type = 0;
    if( strcasecmp(param, "--danale") == 0 )
        type = TYPE_ID_TYPE_DANALE;
    else if( strcasecmp(param, "--goolink") == 0 )
        type = TYPE_ID_TYPE_GOOLINK;
    else if( strcasecmp(param, "--eyeplus") == 0 )
        type = TYPE_ID_TYPE_EYEPLUS;
    else if( strcasecmp(param, "--tutk") == 0 )
        type = TYPE_ID_TYPE_TUTK;
    else if( strcasecmp(param, "--tuya") == 0 )
        type = TYPE_ID_TYPE_TUYA;
    else if( strcasecmp(param, "--ac18plus") == 0 )
        type = TYPE_ID_TYPE_AC18PLUS_CONSUME;        
    else if( strcasecmp(param, "--ac18pro") == 0 )
        type = TYPE_ID_TYPE_AC18PRO_CONSUME;     
    else if( strcasecmp(param, "--ac18procmcc") == 0 )
        type = TYPE_ID_TYPE_AC18PRO_CMCC4G;   
    else if( strcasecmp(param, "--ac18plusnvr") == 0 )
        type = TYPE_ID_TYPE_AC18PLUS_NVR;    
    else if( strcasecmp(param, "--ac18pronvr") == 0 )
        type = TYPE_ID_TYPE_AC18PRO_NVR;     
    else if( strcasecmp(param, "--tencentipc") == 0 )
        type = TYPE_ID_TYPE_TENCENT_IOT_IPC;     
    else if( strcasecmp(param, "--aiotipc") == 0 )
        type = TYPE_ID_TYPE_AIOT_IPC; 
    else if( strcasecmp(param, "--aiotnvr") == 0 )
        type = TYPE_ID_TYPE_AIOT_NVR;  
    else if( strcasecmp(param, "--dot") == 0 )
        type = TYPE_ID_TYPE_DOT;            
    else
    {
        __ERR("%s: unknow cmd\n.", param);
        anj_mw_free(pbuf);
        return -1;
    }

RECHECK1:
    len = ReadP2pID(pbuf, 4096, type);
    if( len <= 0 )
    {
        AjOemApply_t param = {0};
        strcpy(param.szSN, sn_str);
        strcpy(param.szUUID, uuid_str);
        param.nType = type;
        start_p2pid_thread(&param);
        wait_p2pid_thread();

        goto RECHECK1;
    }
    else
    {
        debug_show_data_hex((unsigned char*)pbuf, len, 0);
    }

    if (pbuf != NULL)
    {
        anj_mw_free(pbuf);
        pbuf = NULL;
    }

    return 0;
}

int soft_enc_show_sn(int sect_no)
{
    int section_no = -1;
    if (sect_no > 0)
    {
        section_no = sect_no;
    }

    unsigned char *pbuffer = (unsigned char *)anj_mw_malloc(4096);
    if (pbuffer == NULL)
    {
        __ERR("pbuffer malloc failed\n");
        return -1;
    }

    if(ReadSnFlashData(pbuffer, 4096, section_no) == 0)
    {
        debug_show_data_hex(pbuffer, 4096, 0);
    }

    if (pbuffer != NULL)
    {
        anj_mw_free(pbuffer);
        pbuffer = NULL;
    }

    return 0;
}

int soft_enc_erasesn(int sect_no)
{
    int section_no = -1;
    if (sect_no > 0)
    {
        section_no = sect_no;
    }

    ClearEncriptDataToSoft(sect_no);

    unsigned char *pbuffer = (unsigned char *)anj_mw_malloc(4096);
    if (pbuffer == NULL)
    {
        __ERR("pbuffer malloc failed\n");
        return -1;
    }

    if(ReadSnFlashData(pbuffer, 4096, section_no) == 0)
    {
        debug_show_data_hex(pbuffer, 4096, 0);
    }

    if (pbuffer != NULL)
    {
        anj_mw_free(pbuffer);
        pbuffer = NULL;
    }

    return 0;
}

int soft_enc_uboot_anlyargs(const char *name, const char *value)
{
    if( name == NULL || value == NULL )
    {
        __ERR("name or value is NULL\n");
        return -1;
    }

	int ret = check_ubootargs2(name, value, 0);
	if( ret < 0 )
	{
		__ERR("check_ubootargs2 failed\n");
	}
	else if( ret == 0 )
	{
		__INFO("check_ubootargs2 OK\n");
	}
	else
	{
		__INFO("check_ubootargs2 modify OK\n");
	}

    return ret;
}

int soft_enc_uboot_write(char *file)
{
    if (file == NULL)
    {
        __ERR("file is NULL\n");
        return -1;
    }

    return WriteUboot_byFile(file);
}
