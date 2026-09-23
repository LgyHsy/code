#include "anj_mw_media_audio_aec_provider.h"

static const anj_mw_media_audio_aec_ops *s_pstAudioAecOps = 0;

int anj_mw_media_audio_aec_provider_register(const anj_mw_media_audio_aec_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstAudioAecOps == 0 ||
        ops->provider_priority >= s_pstAudioAecOps->provider_priority)
    {
        s_pstAudioAecOps = ops;
    }

    return 0;
}

void anj_mw_media_audio_aec_provider_unregister(const anj_mw_media_audio_aec_ops *ops)
{
    if (s_pstAudioAecOps == ops)
    {
        s_pstAudioAecOps = 0;
    }
}

int anj_mw_media_audio_aec_provider_available(void)
{
    return (s_pstAudioAecOps != 0) ? 1 : 0;
}

int anj_mw_media_audio_aec_provider_init(void *pstAudioAttr)
{
    if (s_pstAudioAecOps && s_pstAudioAecOps->init)
    {
        return s_pstAudioAecOps->init(pstAudioAttr);
    }

    return 0;
}

int anj_mw_media_audio_aec_provider_uninit(void)
{
    if (s_pstAudioAecOps && s_pstAudioAecOps->uninit)
    {
        return s_pstAudioAecOps->uninit();
    }

    return 0;
}

int anj_mw_media_audio_aec_provider_trans(char *data_near, char *data_far, int len, int audio_chn)
{
    if (s_pstAudioAecOps && s_pstAudioAecOps->trans)
    {
        return s_pstAudioAecOps->trans(data_near, data_far, len, audio_chn);
    }

    return 0;
}
