#ifndef __ANJ_MODULE__
#define __ANJ_MODULE__

#if defined(__cplusplus)
extern "C"
{
#endif

enum MODULE_PRIORITY
{
    MODULE_PRIORITY_CONFIG = 0,
    MODULE_PRIORITY_SYSTEM,
    MODULE_PRIORITY_VIDEO,
    MODULE_PRIORITY_AUDIO,
    MODULE_PRIORITY_ZXING,
    MODULE_PRIORITY_WAVE,
    MODULE_PRIORITY_NET,
    MODULE_PRIORITY_SDCARD,
    MODULE_PRIORITY_NFS,
    MODULE_PRIORITY_MCU,
    MODULE_PRIORITY_BATTERY,
    MODULE_PRIORITY_ISP,
    MODULE_PRIORITY_ZOOM,
    MODULE_PRIORITY_OSD,
    MODULE_PRIORITY_SNAP,
    MODULE_PRIORITY_SMART,
    MODULE_PRIORITY_ALARM,
    MODULE_PRIORITY_SERVICE,
    MODULE_PRIORITY_PTZ,
    MODULE_PRIORITY_SER,
    MODULE_PRIORITY_BIND,
    MODULE_PRIORITY_GYRO,
};

typedef struct
{
    char module_name[32];
    int (*init)(void);
    int (*uninit)(void);
    int module_priority;
} ModuleInterface;

typedef struct ModuleEntry
{
    ModuleInterface *info;
    struct ModuleEntry *next;
} ModuleEntry;

#ifdef __cplusplus
    #define ANJ_MODULE_LINK_KEEP(name)                                      \
        extern "C" __attribute__((used)) void anj_module_keep_##name(void); \
        extern "C" __attribute__((used)) void anj_module_keep_##name(void) {}
#else
    #define ANJ_MODULE_LINK_KEEP(name)                          \
        __attribute__((used)) void anj_module_keep_##name(void); \
        __attribute__((used)) void anj_module_keep_##name(void) {}
#endif

// 模块注册宏
#ifdef __cplusplus
    #define REGISTER_MODULE(name, priority)                            \
        ANJ_MODULE_LINK_KEEP(name);                                    \
        static void name##_register(void)                               \
        {                                                              \
            static ModuleInterface module_info = {                     \
                #name,                                                 \
                name##_init,                                           \
                name##_uninit,                                         \
                priority                                               \
            };                                                         \
            register_module(&module_info);                             \
        }                                                              \
        static int name##_register_helper = (name##_register(), 0)
#else
    #define REGISTER_MODULE(name, priority)                            \
        ANJ_MODULE_LINK_KEEP(name);                                    \
        __attribute__((constructor)) static void name##_register(void) \
        {                                                              \
            static ModuleInterface module_info = {                     \
                .module_name = #name,                                  \
                .init = name##_init,                                   \
                .uninit = name##_uninit,                               \
                .module_priority = priority                            \
            };                                                         \
            register_module(&module_info);                             \
        }
#endif

void register_module(ModuleInterface *module);
int modules_init(void);
void modules_uninit(const char *skip_module, ...);
int module_init_single(const char *module_name);
int module_uninit_single(const char *module_name);

#if defined(__cplusplus)
}
#endif

#endif