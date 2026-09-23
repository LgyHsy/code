#include <stdio.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/ioctl.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_log.h"

#define SYS_DEV_NODE                "/dev/msys"
#define SSN_SYS_LIB_PATH            "/lib/libmi_sys.so"
#define SSN_SYS_FUNC                "MI_SYS_ReadUuid"

#define MSYS_IOCTL_MAGIC            'S'
#define IOCTL_MSYS_GET_UDID         _IO(MSYS_IOCTL_MAGIC, 0x32)

#define UBOOTENV_256K_SIZE          0x00040000
#define SN_MTD_SIZE_256K_ENV        0x40000
#define SN_MTD_SIZE_NORMAL          0x30000

#define UBOOT_ENV_NO_256K_ENV       63
#define UBOOT_ENV_NO_NORMAL         47

#define SN_SECT_NO_256K_ENV         62
#define SN_SECT_NO_NORMAL           46

#define MAGIC_P2PID_1               0x404E4A50
#define MAGIC_P2PID_2               0x32504944


typedef struct 
{ 
    unsigned int VerChk_Version; 
    unsigned long long udid; 
    unsigned int VerChk_Size; 
} __attribute__ ((__packed__)) MSYS_UDID_INFO;


typedef int(*sstar_uuid_func)(unsigned long long *data);
static sstar_uuid_func s_pGetUuidFunc = NULL;
static void *s_pHandle = NULL;


static int uboot_version_get()
{
    static int uboot_256k_version = -1;

    if (uboot_256k_version < 0)
    {
        int size = anj_mw_mtd_size_get(UBOOT_BLOCK_MTD);
        if (size == UBOOTENV_256K_SIZE)
        {
            uboot_256k_version = 1;
        }
        else
        {
            uboot_256k_version = 0;
        }
    }

    return uboot_256k_version;
}

int platform_sn_mtd_size_get()
{
    int mtd_size = 0;
    if (uboot_version_get() == 0)
    {
        mtd_size = SN_MTD_SIZE_NORMAL;
    }
    else
    {
        mtd_size = SN_MTD_SIZE_256K_ENV;
    }

    return mtd_size;
}

int platform_sn_sect_no_get()
{
    int env_no = 0;
    if (uboot_version_get() == 0)
    {
        env_no = SN_SECT_NO_NORMAL;
    }
    else
    {
        env_no = SN_SECT_NO_256K_ENV;
    }

    return env_no;
}

int platform_uboot_env_no_get()
{
    int env_no = 0;
    if (uboot_version_get() == 0)
    {
        env_no = UBOOT_ENV_NO_NORMAL;
    }
    else
    {
        env_no = UBOOT_ENV_NO_256K_ENV;
    }

    return env_no;
}

int platform_p2pid_write(char *buf, int len)
{
    return 0;
}

int platform_p2pid_read(char *buffer, int buffersize, unsigned int nType)
{
    return 0;
}

int platform_phymem_ability_get()
{
    return 1;
}

int sstar_uuid_get_func_init()
{
    int iRet = 0;
    char func_name[32] = {0};
    char path[64] = {0};

    if( NULL != s_pHandle)
    {
        return 0;
    }

	snprintf(path, sizeof(path), "%s", SSN_SYS_LIB_PATH);
    if(access(path, F_OK) != 0)
    {
        return -1;
    }

    char *error = NULL;
    s_pHandle = dlopen(path, RTLD_LAZY);
    dlerror();
	if(s_pHandle == NULL)
	{
        __ERR("dlopen %s failed\n", path);
        iRet = -1;
        goto EXIT;
	}

    snprintf(func_name, sizeof(func_name), SSN_SYS_FUNC);
    sstar_uuid_func pFunc = dlsym(s_pHandle, func_name);
    error = dlerror();
    if (error != NULL)
    {
        __ERR("cannot found %s in %s\n", func_name, path);
        iRet = -1;
        goto EXIT;
    }

    s_pGetUuidFunc = pFunc;
    // printf("found %s=%#x in %s\n", func_name, (unsigned int)pFunc, path);

	return iRet;

EXIT:
    if( NULL != s_pHandle)
    {
        dlclose(s_pHandle);
        s_pHandle = NULL;
    }

    return iRet;
}	

void sstar_uuid_get_func_uninit()
{
    s_pGetUuidFunc = NULL;
    if( NULL != s_pHandle)
    {
        dlclose(s_pHandle);
        s_pHandle = NULL;
    }
}

const char *platform_inner_uuid_get()
{
    int iRet = 0;
    static char sz_uuid[128] = {0};

    sstar_uuid_get_func_init();
    if(s_pGetUuidFunc != NULL)
    {
        unsigned long long uuid = 0;
        iRet = s_pGetUuidFunc(&uuid);
        if (iRet == 0)
        {
            snprintf(sz_uuid, sizeof(sz_uuid), "%llu", uuid);
            // ID_PRINT("%s", sz_uuid);
        }
        else
        {
            // ID_PRINT("read uuid failed %#x\n", ret);
        }

        sstar_uuid_get_func_uninit();
    }
    else
    {
        int sysfd = -1;
        sysfd = open(SYS_DEV_NODE, O_RDWR);
        if (sysfd > 0)
        {
            MSYS_UDID_INFO stInfo = {0};
            stInfo.VerChk_Version = 0x4d530100;
            stInfo.VerChk_Size = sizeof(MSYS_UDID_INFO);

            ioctl(sysfd, IOCTL_MSYS_GET_UDID, &stInfo);
            snprintf(sz_uuid, sizeof(sz_uuid), "%lld", stInfo.udid);
            close(sysfd);
        }
    }

    return sz_uuid; 
}

