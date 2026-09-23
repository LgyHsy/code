#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_service.h"
#include "anj_ser.h"
#include "anj_net.h"
#include "anj_ser_stream.h"
#include "anj_module.h"

static anj_ser_info s_stSerInfo = {0};
static const anj_ser_p2p_ops *s_stP2pOps = NULL;
static const anj_cloud_ops *s_stCloudOps = NULL;

int anj_ser_p2p_provider_register(const anj_ser_p2p_ops *ops)
{
    if (!ops)
        return -1;

    s_stP2pOps = ops;
    return 0;
}

void anj_ser_p2p_provider_unregister(const anj_ser_p2p_ops *ops)
{
    if (s_stP2pOps == ops)
        s_stP2pOps = NULL;
}

int anj_cloud_ops_register(const anj_cloud_ops *ops)
{
    if (!ops)
        return -1;

    s_stCloudOps = ops;
    return 0;
}

void anj_cloud_ops_unregister(const anj_cloud_ops *ops)
{
    if (s_stCloudOps == ops)
        s_stCloudOps = NULL;
}

int anj_ser_alarm_handle(int chn, int code, int sub_code, char *pdata)
{
    int iRet = 0;
    if (s_stP2pOps && s_stP2pOps->alarm_handle)
        iRet = s_stP2pOps->alarm_handle(chn, code, sub_code, pdata);
    return iRet;
}

static int anj_ser_init(void)
{
    int iRet = 0;
    NetworkConfigNew *pstNetWorkConfig = getNetWorkConfig();
    if (pstNetWorkConfig->p2pCfg.enable == 0)
    {
        __INFO("p2p disable!\n");
        goto endFunc;
    }

    s_stSerInfo.stP2pLoginState.enable = pstNetWorkConfig->p2pCfg.enable;
    if (s_stP2pOps && s_stP2pOps->init)
    {
        s_stSerInfo.stP2pLoginState.p2ptype = s_stP2pOps->p2p_type;
        iRet = s_stP2pOps->init();
    }
    if (s_stCloudOps && s_stCloudOps->init)
        iRet = s_stCloudOps->init();

    anj_ser_stream_create();

endFunc:
    return iRet;
}

static int anj_ser_uninit(void)
{
    anj_ser_stream_release();
    if (s_stCloudOps && s_stCloudOps->uninit)
        s_stCloudOps->uninit();
    if (s_stP2pOps && s_stP2pOps->uninit)
        s_stP2pOps->uninit();
    return 0;
}

void anj_ser_reponse(int func, void *data)
{
    if (s_stP2pOps && s_stP2pOps->response)
        s_stP2pOps->response(func, data);
}

int anj_ser_bind(char *user_name, char *client_code)
{
    int iRet = 0;
    if (s_stP2pOps && s_stP2pOps->bind)
        iRet = s_stP2pOps->bind(s_stSerInfo.uid, s_stSerInfo.report_dn, user_name, client_code);
    return iRet;
}

void anj_ser_unbind()
{
    if (s_stP2pOps && s_stP2pOps->unbind)
        s_stP2pOps->unbind();
}

void anj_ser_reset_conn(int eNetStatus)
{
    if (s_stCloudOps && s_stCloudOps->check_support)
        s_stCloudOps->check_support((eNetStatus != ANJ_NET_STATUS_4G));
    if (s_stP2pOps && s_stP2pOps->reset_conn)
        s_stP2pOps->reset_conn();
}

void anj_ser_push_video(int camera_type, int streamtype, int iskey,
                        unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs)
{
    if (s_stSerInfo.stP2pLoginState.logined)
    {
        if (s_stP2pOps && s_stP2pOps->push_video)
            s_stP2pOps->push_video(camera_type, streamtype, iskey, frameBuf, frameLen, frameTimeMs);
    }
}

void anj_ser_push_audio(int camera_type, unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs)
{
    if (s_stSerInfo.stP2pLoginState.logined)
    {
        if (s_stP2pOps && s_stP2pOps->push_audio)
            s_stP2pOps->push_audio(camera_type, frameBuf, frameLen, frameTimeMs);
    }
}

void anj_ser_simple_unbind()
{
    if (s_stP2pOps && s_stP2pOps->manual_unbind)
        s_stP2pOps->manual_unbind();
}

void anj_ser_remove_unbind_device()
{
    if (s_stP2pOps && s_stP2pOps->remove_unbind_device)
        s_stP2pOps->remove_unbind_device();
}

int anj_ser_bind_status_get()
{
    if (s_stP2pOps && s_stP2pOps->bind_status_get)
        return s_stP2pOps->bind_status_get();
    return 0;
}

void anj_ser_cloud_binduser_check(char *pstBindUser)
{
    if (s_stCloudOps && s_stCloudOps->binduser_check)
        s_stCloudOps->binduser_check(pstBindUser);
}

void anj_ser_cloud_push_alarm(int channel, int eventype, int buploadcloud)
{
    if (s_stCloudOps && s_stCloudOps->push_alarm)
        s_stCloudOps->push_alarm(channel, eventype, buploadcloud);
}

void anj_ser_cloud_push_video(int camera_type, int streamtype, int iskey, unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudOps && s_stCloudOps->push_video)
        s_stCloudOps->push_video(camera_type, streamtype, iskey, frameBuf, frameLen);
}

void anj_ser_cloud_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudOps && s_stCloudOps->push_audio)
        s_stCloudOps->push_audio(camera_type, frameBuf, frameLen);
}

anj_ser_info *getSerInfo(void)
{
    return (void *)&s_stSerInfo;
}

REGISTER_MODULE(anj_ser, MODULE_PRIORITY_SER);
