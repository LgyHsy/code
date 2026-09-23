#ifndef __PLATFORM_SN_H__
#define __PLATFORM_SN_H__

#define SN_UUID_OFFSET 512


const char *platform_inner_uuid_get();

int platform_sn_mtd_size_get();

int platform_sn_sect_no_get();
int platform_uboot_env_no_get();


int platform_p2pid_write(char *buf, int len);
int platform_p2pid_read(char *buffer, int buffersize, unsigned int nType);

int platform_phymem_ability_get();

#endif
