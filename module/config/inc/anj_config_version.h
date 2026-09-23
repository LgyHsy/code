#ifndef _ANJ_CONFIG_VERSION_H_
#define _ANJ_CONFIG_VERSION_H_

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct
{
    char szVersion[64];
} ConfigVersion;


int anj_config_version_get(IXML_Node *pNode, ConfigVersion *pVersionCfg);
int anj_config_version_save(ConfigVersion *pVersionCfg);
int anj_config_version_load(ConfigVersion *pVersionCfg);


#ifdef __cplusplus
}
#endif

#endif
