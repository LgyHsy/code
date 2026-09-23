#include "sn_uuid.h"
#include "sn_utils.h"
#include "sn_header.h"
#include "platform_sn.h"
#include "get_sn.h"

#include <stddef.h>

#include "aes.h"
#include "flash_rw.h"
#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "anj_mw_crypt.h"
#include "anj_mw_comm.h"

int key_uuid[32] =  
{
	0xce,0x10,0x91,0x43,0x62,0x61,0x15,0xd9,
	0x2c,0x2f,0xef,0x3e,0x53,0x70,0x23,0xc9,
	0x49,0x39,0x91,0x22,0x4e,0x85,0x39,0xb0,
	0x5f,0xbc,0x89,0x15,0x36,0x98,0x4f,0xae
};

int save_uuid_to_ubootenv(const char *cmd)
{
    return check_ubootargs2("AJUUID", cmd, 1);
}

int GetSoftUUIDData(const char* buf, int inbuflen, char *output_decrypt, int outbuflen)
{
    unsigned char *key = NULL;
    key = (unsigned char*)key_uuid;

    unsigned char buffer[16 + 4] = {0};
    unsigned char decrypt[16 + 4] = {0};    
    unsigned char decrypt2[16 + 4] = {0};    
    struct crypto_aes_ctx ctx;

    memcpy(buffer, buf, 16);
    memcpy(decrypt , buffer , 16);

    crypto_aes_expand_key(&ctx, (unsigned char *)key, 32);		
    aes_decrypt_(&ctx, decrypt2, decrypt);
    memcpy(output_decrypt, decrypt2, 16);
    return 0;
}

int write_uuid(unsigned char *buf, int len)
{
    if (buf == NULL)
    {
        __ERR("buf error\n");
        return -1;
    }

    if(len < 20 || len > 32)
    {
        __ERR("len %d error\n",  len);
        return -1;
    }

    __INFO("uuid write:%s, length:%d\n", buf, len);
    if (SUPPORT_NAND_FLASH)
    {
        __ERR("device don't support write uuid in spi nand flash!\n");
        return -1;
    }

    int iRet = 0;
	iRet = save_uuid_to_ubootenv((const char *)buf);

    char uuid_encrypt[128] = {0};
    copy_prevention_encrypt_v1((const char *)buf, (void*)uuid_encrypt, sizeof(uuid_encrypt));

    //这里先加密一下 
    //换一个密钥哈，加解密都在这里，避免被破解
    struct crypto_aes_ctx ctx;
    unsigned char encrypt_src[20] = {0};    //加密源
    unsigned char encrypt_dst[20] = {0};    //加密后
    char uuid_encrypt_savedata[64] = {0};   //加密后的UUID数据，每16字节一次加密，再串起来

    int iIndex = 0;
    for(iIndex = 0; iIndex < 2; iIndex++)
    {
        memset(encrypt_src, 0, sizeof(encrypt_src));
        memset(encrypt_dst, 0, sizeof(encrypt_dst));

        int leftlen = 16;
        if(iIndex == 1)
        {
            leftlen = len - 16;
        }

        memcpy(encrypt_src, buf + iIndex * 16, leftlen);

        unsigned char *key = NULL;
        key = (unsigned char*)key_uuid;
        crypto_aes_expand_key(&ctx, (unsigned char *)key, 32);
        aes_encrypt_(&ctx, encrypt_dst, encrypt_src);
        memcpy(uuid_encrypt_savedata+iIndex*16, encrypt_dst, 16);
    }

    len = 32;//强制写入32字节

    /*读出一个SECT的数据*/
    unsigned int size_sect = SECTOR_SIZE;
    unsigned char *pSectBuffer = (unsigned char *)anj_mw_malloc(size_sect);
    if( NULL == pSectBuffer)
    {
        __ERR("malloc %u error.\n", size_sect);
        return -1;
    }

    if(SUPPORT_NAND_FLASH)
    {
        iRet = read_sndata_by_file((char *)pSectBuffer, size_sect);
        if( iRet != 0)
        {
            memset(pSectBuffer, 0, size_sect);
        }
        iRet = 0;
    }
    else
    {
        const char* mtd_block = SN_BLOCK;
        unsigned int sect_number =  platform_sn_sect_no_get();
        iRet = flash_sect_rw(0, mtd_block, size_sect, sect_number, (unsigned char*)pSectBuffer);
        if (iRet != 0)
        {
            __ERR("sect %d: flash_read error.\n", sect_number);
            iRet = -1;
            goto __exit;
        }
    }

	for(iIndex = 0; iIndex < 1; iIndex++ )
	{
        unsigned char *p = pSectBuffer + iIndex * 128 + SN_UUID_OFFSET;
        AjSnHeaderV1 *pHeader = (AjSnHeaderV1*)p;

        if( pHeader->magic1 == MAGIC1_UUID &&
            pHeader->magic2 == MAGIC2_UUID &&
            pHeader->magic3 == MAGIC3_UUID &&
            pHeader->magic4 == MAGIC4_UUID)
        {
            if( pHeader->len == len &&	
                memcmp(pHeader->data, uuid_encrypt_savedata, len ) == 0)
            {
                __ERR("%d: %s: data same, not changed.\n", iIndex, uuid_encrypt_savedata);

                iRet = 0;
                goto __exit;
            }
        }

        pHeader->magic1 = MAGIC1_UUID; 
        pHeader->magic2 = MAGIC2_UUID; 
        pHeader->magic3 = MAGIC3_UUID; 
        pHeader->magic4 = MAGIC4_UUID; 
        pHeader->len = len;

        memcpy(pHeader->data, uuid_encrypt_savedata, pHeader->len);
        pHeader->crc = GetCrcValue((char*)pHeader->data, pHeader->len);
        memcpy(pHeader->uuid_check, uuid_encrypt, 32);
	}	

    if(SUPPORT_NAND_FLASH)
    {
        iRet = write_sndata_by_file((char *)pSectBuffer, size_sect);
        if (0 != iRet)
        {
            __ERR("write_sndata_by_file failed %#x.\n", iRet);

            iRet = -1;
            goto __exit;

        }
    }
    else
    {
        const char* mtd_block = SN_BLOCK;
        unsigned int sect_number = platform_sn_sect_no_get();
        iRet = flash_sect_rw(1, mtd_block, size_sect, sect_number, (unsigned char*)pSectBuffer);
        if (0 != iRet)
        {
            __ERR("sect %d: flash_read error.\n", sect_number);
            iRet = -1;
            goto __exit;
        }
    }

    iRet = 0;

__exit:
    if (pSectBuffer != NULL)
    {
        anj_mw_free(pSectBuffer);
        pSectBuffer = NULL;  
    }

    return iRet;
}
