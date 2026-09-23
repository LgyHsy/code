#ifndef __ANJ_FACTORY_H__
#define __ANJ_FACTORY_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define FACTORY_DEFAULT_CFG_PATH    "/mnt/nand/factory.default.cfg"
#define FACTORY_IMAGE_FILP_CFG      "/mnt/nand/imageflip"

typedef struct
{
    int LedMode;
    int IrCutMode;
} FactoryDefaultCfg;

typedef enum
{
    FACTORY_TEST_PTZ_LED_IRCUT = 0,
    FACTORY_TEST_NONE,
} factory_test_mode_e;

int anj_factory_defcfg_get(const char *xmlBuf, FactoryDefaultCfg *pDefaultCfg);
int anj_factory_defcfg_save(FactoryDefaultCfg *pDefaultCfg);
int anj_factory_defcfg_load(FactoryDefaultCfg *pDefaultCfg);
int anj_factory_test(factory_test_mode_e mode);
int anj_factory_image_flip_cfg_load(int *flip);
int anj_factory_image_flip_cfg_save(int flip);

void anj_factory_bitrate_check_set();
int  anj_factory_wifi_connect_set(char *ssid, char *passwd);
void anj_factory_ircut_light_test_set();
int  anj_factory_init();

#ifdef __cplusplus
}
#endif

#endif