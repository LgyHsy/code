#ifndef __ANJ_SER_PROVIDER_H__
#define __ANJ_SER_PROVIDER_H__

typedef struct
{
    int p2p_type;
    int (*init)(void);
    void (*uninit)(void);
    int (*alarm_handle)(int chn, int code, int sub_code, char *pdata);
    void (*response)(int func, void *data);
    int (*bind)(const char *product_key, const char *device_name, const char *accountName, const char *clientCode);
    void (*unbind)(void);
    void (*reset_conn)(void);
    void (*push_video)(int camera_type, int streamtype, int iskey,
                       unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);
    void (*push_audio)(int camera_type, unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);
    void (*manual_unbind)(void);
    void (*remove_unbind_device)(void);
    int (*bind_status_get)(void);
} anj_ser_p2p_ops;

typedef struct
{
    int (*init)(void);
    int (*uninit)(void);
    void (*check_support)(int status);
    void (*binduser_check)(char *pstBindUser);
    void (*push_alarm)(int channel, int eventype, int buploadcloud);
    void (*push_video)(int camera_type, int streamtype, int iskey,
                       unsigned char *frameBuf, unsigned int frameLen);
    void (*push_audio)(int camera_type, unsigned char *frameBuf, unsigned int frameLen);
} anj_cloud_ops;

typedef struct
{
    int (*init)(void);
    int (*uninit)(void);
    void (*check_support)(int status);
    void (*binduser_check)(char *pstBindUser);
    void (*push_alarm)(int channel, int eventype, int buploadcloud);
    void (*push_video)(int camera_type, int streamtype, int iskey,
                       unsigned char *frameBuf, unsigned int frameLen);
    void (*push_audio)(int camera_type, unsigned char *frameBuf, unsigned int frameLen);
} anj_cloud_feature_ops;

int anj_ser_p2p_provider_register(const anj_ser_p2p_ops *ops);
void anj_ser_p2p_provider_unregister(const anj_ser_p2p_ops *ops);

int anj_cloud_ops_register(const anj_cloud_ops *ops);
void anj_cloud_ops_unregister(const anj_cloud_ops *ops);

int anj_cloud_feature_register(const anj_cloud_feature_ops *ops);
void anj_cloud_feature_unregister(const anj_cloud_feature_ops *ops);

#endif
