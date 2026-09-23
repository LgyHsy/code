#ifndef __ANJ_NFS_H__
#define __ANJ_NFS_H__

#ifdef __cplusplus
extern "C"
{
#endif

int anj_nfs_init(void);
int anj_nfs_uninit(void);
int anj_nfs_mounted_get(void);

#ifdef __cplusplus
}
#endif

#endif
