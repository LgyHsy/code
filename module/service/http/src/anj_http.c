#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "anj_mw_comm.h"

#include "anj_config.h"

#include "http_handle.h"
#include "http_unv.h"
#include "cgi_handle.h"
#include "http_hapi.h"

#include "anj_http.h"
#include "function_list.h"
#include "anj_sysctl.h"

void anj_http_web_file_init()
{
    int bNewWeb = 0;
    char szCustFile[64] = {0};
    char szSrcWebPath[64] = {0};
    char szDstWebPath[64] = {0};

    anj_mw_system("rm -rf /tmp/www");

    if (!anj_mw_file_exists("/var/www/base"))
    {
        bNewWeb = 0;
        strcpy(szSrcWebPath, "/var/www");
        strcpy(szDstWebPath, "/tmp/www");
    }
    else
    {
        bNewWeb = 1;
        strcpy(szSrcWebPath, "/var/www/base");
        strcpy(szDstWebPath, "/tmp/www/base");
    }

    __INFO("anj http use new web file? -> %s!\n", (bNewWeb > 0) ? "yes" : "no");

    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/www.7z", DATA_BLOCK_MOUNT_PATH,  AJ_CUST_PATH_NAME);	
	if(anj_mw_file_exists(szCustFile))
	{
		mysystem_with_param(AJ_APP_PATH"/7zDec x %s -o/tmp/&& sync", szCustFile);
		anj_mw_system("ln -s  /var/www/WEBConfig.exe /tmp/www/WEBConfig.exe");
		anj_mw_system("ln -s  /var/www/f404.html /tmp/www/f404.html");
		anj_mw_system("ln -s  /var/www/f404.jpg /tmp/www/f404.jpg");
	}
	else
	{
		//不直接将/var/www ln到tmp，是因为要单独处理logo.png
		anj_mw_system("mkdir -p /tmp/www");
		anj_mw_system("ln -s /var/www/* /tmp/www/");
		mysystem_with_param("rm -f %s/images", szDstWebPath);
		mysystem_with_param("mkdir -p %s/images", szDstWebPath);
		mysystem_with_param("ln -s %s/images/* %s/images/", szSrcWebPath, szDstWebPath);
	}

    if( bNewWeb > 0 )
    {
        mysystem_with_param("rm -f /tmp/www/base");
        mysystem_with_param("mkdir /tmp/www/base");
        mysystem_with_param("ln -s /var/www/base/*  /tmp/www/base/");

        mysystem_with_param("rm -f /tmp/www/new");
        mysystem_with_param("mkdir /tmp/www/new");
        mysystem_with_param("ln -s /var/www/new/*  /tmp/www/new/");

        mysystem_with_param("ln -s /tmp/www/f404*  %s/base/", szDstWebPath);
        mysystem_with_param("ln -s /tmp/www/f404*  %s/new/", szDstWebPath);

        mysystem_with_param("ln -s /tmp/www/*.exe  /tmp/www/base/");
        mysystem_with_param("ln -s /tmp/www/*.exe  /tmp/www/new/");
    }

    // LOGO定制处理
    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/logo.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/images/logo.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/images/logo.png", szCustFile, szDstWebPath);
    }

    //新WEB LOGO定制处理
    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/login_logo.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/login_logo.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/login_logo.png", szCustFile, szDstWebPath);
    }

    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/login_logo_big.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/login_logo_big.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/login_logo_big.png", szCustFile, szDstWebPath);
    }

    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/login_title.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/login_title.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/login_title.png", szCustFile, szDstWebPath);
    }

    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/login_input_button.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/login_input_button.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/login_input_button.png", szCustFile, szDstWebPath);
    }

    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/main_logo.png", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/main_logo.png", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/main_logo.png", szCustFile, szDstWebPath);
    }

    //新WEB 风格定制文件处理
    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/web_diy.txt", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {
        mysystem_with_param("rm -f %s/web_diy.txt", szDstWebPath);
        mysystem_with_param("ln -s  %s  %s/web_diy.txt", szCustFile, szDstWebPath);
    }

    mysystem_with_param("rm -f %s/Custom", szDstWebPath);
    mysystem_with_param("cp -R %s/Custom %s", szSrcWebPath, szDstWebPath);

    memset(szCustFile, 0, sizeof(szCustFile));
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/Custom", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);	
    if(anj_mw_file_exists(szCustFile))
    {		
        mysystem_with_param("cp -R %s/* %s/Custom/", szCustFile, szDstWebPath);
    }

    int iRet = 0;
    int is_link_file = 0;
    if(anj_mw_file_exists("/mnt/nand/mp3"))
    {
        if(anj_mw_file_exists("/mnt/nand/mp3/upload.mp3"))
        {
            char *mp3_file = "/mnt/nand/mp3/upload.mp3";
            struct stat filestat;

            iRet = lstat(mp3_file, &filestat);
            if (iRet == 0)
            {
                if ((filestat.st_mode & S_IFMT) == S_IFLNK) 
                {
                    is_link_file = 1;
                }
            }

            if (is_link_file == 0)
            {
                mysystem_with_param("mv /mnt/nand/mp3/upload.mp3 /mnt/nand/upload.mp3");
                mysystem_with_param("ln -s %s/upload.mp3 %s/upload.mp3", DATA_BLOCK_MOUNT_PATH, DATA_BLOCK_MOUNT_PATH"/mp3");
            }
        }
        else
        {
            if(anj_mw_file_exists("/mnt/nand/upload.mp3"))
            {
                mysystem_with_param("ln -s %s/upload.mp3 %s/upload.mp3", DATA_BLOCK_MOUNT_PATH, DATA_BLOCK_MOUNT_PATH"/mp3");
            }
        }

    }
    else
    {
        mysystem_with_param("mkdir /mnt/nand/mp3");
        if(anj_mw_file_exists("/mnt/nand/upload.mp3"))
        {
            mysystem_with_param("ln -s %s/upload.mp3 %s/upload.mp3", DATA_BLOCK_MOUNT_PATH, DATA_BLOCK_MOUNT_PATH"/mp3");
        }
    }

}

void anj_http_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile)
{
    http_cgi_init_session_id();     // CGI协议
    http_unv_init();                // 宇视私有协议
    anj_sysctl_capability_add(FUNCTION_UNV_CONFIG);
    http_hapi_init();               // hapi协议

    http_handle_init(cbResponse, cbGetFile);     // http处理
}

void anj_http_uninit()
{
    http_hapi_uninit();
    http_unv_uninit();
    http_cgi_uninit_session_id();
}
