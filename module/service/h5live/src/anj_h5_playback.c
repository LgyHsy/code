#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_log.h"
#include "anj_mw_time.h"
#include "anj_record.h"
#include "anj_h5_ws_server.h"
#include "anj_h5_stream.h"
#include "anj_h5_playback.h"

static REC_HANDLE s_pb_handle = NULL;
static unsigned int s_session_id = 0;
static int s_speed = 0;
static int s_fast_or_slow = 0;
static unsigned int s_timeplay_pre_sec = 0;
static unsigned int s_last_play_abs_sec = 0;

static void h5_playback_send_status(unsigned int session_id, int cur_status, int speed)
{
    char retdata[100] = {0};
    StreamBuffer *sb = getStreamBuffer(STREAM_ID_PLAYBACK);

    snprintf(retdata, sizeof(retdata), "<playback_status status=\"%d\" speed=\"%d\" />", cur_status, speed);
    if (sb)
    {
        writeStreamBuffer(sb, cmd_Type, session_id, (unsigned char *)retdata, (unsigned int)strlen(retdata));
        usleep(1000);
        writeStreamBuffer(sb, cmd_Type, session_id, (unsigned char *)retdata, (unsigned int)strlen(retdata));
    }
    __INFO("h5 playback status: %s\n", retdata);
}

static int h5_playback_video_send(rec_pb_poper *pPoper, media_frame_info_t *pFrameInfo, unsigned int session_id)
{
    StreamBuffer *sb = getStreamBuffer(STREAM_ID_PLAYBACK);
    unsigned int len = pFrameInfo->frameParam.frameLen;
    unsigned char *data = pFrameInfo->frameBuf;
    int frame_type = PFatme_Type;

    if (sb == NULL || data == NULL || len == 0)
    {
        return -1;
    }

    if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
    {
        frame_type = IFatme_Type;
    }

    writeStreamBuffer(sb, frame_type, session_id, data, len);

    if (pPoper)
    {
        s_last_play_abs_sec = (unsigned int)(pFrameInfo->frameParam.frameTimeMs / 1000);
        uint32_t timestamp = (pFrameInfo->frameParam.frameTimeMs - (pPoper->tDayStartTime * 1000));
        uint32_t cur_sec = timestamp / 1000;
        if (s_timeplay_pre_sec != cur_sec)
        {
            char retdata[100] = {0};
            s_timeplay_pre_sec = cur_sec;
            snprintf(retdata, sizeof(retdata), "<playback_status timepos=\"%u\" />", cur_sec);
            writeStreamBuffer(sb, cmd_Type, session_id, (unsigned char *)retdata, (unsigned int)strlen(retdata));
        }
    }

    return 0;
}

static int h5_playback_audio_send(media_frame_info_t *pFrameInfo, unsigned int session_id)
{
    StreamBuffer *sb = getStreamBuffer(STREAM_ID_AUDIO_PLAYBACK);
    if (sb == NULL || pFrameInfo->frameBuf == NULL || pFrameInfo->frameParam.frameLen <= 0)
    {
        return -1;
    }

    writeStreamBuffer(sb, AudioData_Type, session_id, pFrameInfo->frameBuf,
                      (unsigned int)pFrameInfo->frameParam.frameLen);
    return 0;
}

/* pb_proc pops frames as fast as possible; the callback must pace playback
 * (same contract as anjpri/rtsp_lite/gb28181 replay callbacks). Without this
 * the 3-slot stream buffer is overwritten thousands of times per second and
 * the web client only receives orphan P-frames => gray screen. */
static void h5_playback_pace(rec_pb_poper *pPoper, media_frame_info_t *pFrameInfo)
{
    if ((pPoper->tLastPts > 0) && (pFrameInfo->frameParam.framePts > pPoper->tLastPts))
    {
        unsigned long long tNowMs = anj_mw_get_cputime_ms(NULL);
        unsigned int iDiffTime = (unsigned int)((pFrameInfo->frameParam.framePts - pPoper->tLastPts) / 90);

        if (pPoper->iSpeed > PB_SPEED_0)
        {
            iDiffTime = iDiffTime / pPoper->iSpeed;
            if (pPoper->iSpeed > PB_SPEED_2)
            {
                iDiffTime = iDiffTime / (pPoper->iSpeed / PB_SPEED_4);
            }
        }

        /* Large PTS jumps are segment gaps, not playback gaps. */
        if (iDiffTime > 1000)
        {
            iDiffTime = 0;
        }

        if ((iDiffTime > 0) && (pPoper->tSendTime > 0) && (tNowMs > pPoper->tSendTime))
        {
            unsigned long long iElapsedTime = tNowMs - pPoper->tSendTime;
            if (iElapsedTime >= iDiffTime)
            {
                iDiffTime = 0;
            }
            else
            {
                iDiffTime -= (unsigned int)iElapsedTime;
            }
        }

        if (iDiffTime == 0)
        {
        }
        else if (iDiffTime < 10)
        {
            usleep(10 * 1000);
        }
        else
        {
            usleep((iDiffTime - 1) * 1000);
        }
    }
    else
    {
        usleep(10 * 1000);
    }

    pPoper->tLastPts = pFrameInfo->frameParam.framePts;
    pPoper->tSendTime = anj_mw_get_cputime_ms(NULL);
}

static int h5_playback_data_cb(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e event_id)
{
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;

    if (pHandle == NULL || pFrameInfo == NULL)
    {
        return -1;
    }
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        return -1;
    }

    if (event_id == PB_CB_NONE)
    {
        return 0;
    }
    if (event_id == PB_CB_FINISH)
    {
        __INFO("h5 playback finished\n");
        h5_playback_send_status(s_session_id, 0, 0);
        return 0;
    }
    if (event_id != PB_CB_START)
    {
        return -1;
    }

    int iSpeed = pPoper->iSpeed;
    if (iSpeed <= PB_SPEED_1)
    {
        iSpeed = PB_SPEED_1;
        if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
        {
            h5_playback_video_send(pPoper, pFrameInfo, s_session_id);
        }
        else
        {
            h5_playback_audio_send(pFrameInfo, s_session_id);
        }
    }
    else
    {
        if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
        {
            h5_playback_video_send(pPoper, pFrameInfo, s_session_id);
        }
        if (iSpeed < PB_SPEED_4 && pFrameInfo->frameParam.frameType == MEDIA_VFRAME_P)
        {
            h5_playback_video_send(pPoper, pFrameInfo, s_session_id);
        }
    }

    if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
    {
        h5_playback_pace(pPoper, pFrameInfo);
    }

    return 0;
}

static void h5_playback_release(void)
{
    if (s_pb_handle)
    {
        anj_record_pb_release(s_pb_handle);
        s_pb_handle = NULL;
    }
    clearStreamBuffer(STREAM_ID_PLAYBACK);
    clearStreamBuffer(STREAM_ID_AUDIO_PLAYBACK);
    s_timeplay_pre_sec = 0;
}

static int h5_playback_create_at_time(unsigned int timepos)
{
    s_pb_handle = anj_record_pb_create(0, timepos, 0, 0, 1, h5_playback_data_cb);
    return (s_pb_handle == NULL) ? -1 : 0;
}

int anj_h5_playback_cmd_ctrl(unsigned int session_id, unsigned int playmode,
                             char *filename, unsigned int timepos)
{
    int cur_status = 0;
    int err_code = 0;

    (void)filename;

    do
    {
        if (s_session_id == 0)
        {
            s_session_id = session_id;
        }
        else if (s_session_id != session_id)
        {
            err_code = -400;
            cur_status = 400;
            break;
        }

        if (playmode == ACTION_PLAY)
        {
            __WARN("h5 playback ACTION_PLAY(filename) not supported\n");
            /* Keep websocket alive: report unsupported command status only. */
            err_code = 0;
            cur_status = 405;
            break;
        }
        else if (playmode == ACTION_SEEK)
        {
            if (timepos == 0)
            {
                err_code = 0;
                cur_status = 400;
                break;
            }

            if (s_pb_handle == NULL)
            {
                err_code = h5_playback_create_at_time(timepos);
            }
            else
            {
                err_code = anj_record_pb_seek(s_pb_handle, timepos);
            }

            if (s_pb_handle == NULL || err_code == -1)
            {
                err_code = -1;
                s_session_id = 0;
            }
            else
            {
                cur_status = 1;
                s_speed = 0;
                s_fast_or_slow = 0;
            }
        }
        else if (playmode == ACTION_TIMEPLAY)
        {
            if (timepos == 0)
            {
                err_code = -1;
                break;
            }

            if (s_pb_handle == NULL)
            {
                err_code = h5_playback_create_at_time(timepos);
            }
            else
            {
                rec_pb_poper *pPoper = (rec_pb_poper *)s_pb_handle;
                if (pPoper->bPause)
                {
                    err_code = anj_record_pb_pause_set(s_pb_handle, 0);
                }
                else
                {
                    err_code = anj_record_pb_seek(s_pb_handle, timepos);
                }
            }

            if (s_pb_handle == NULL || err_code == -1)
            {
                err_code = -1;
                s_session_id = 0;
            }
            else
            {
                cur_status = 1;
                s_speed = 0;
                s_fast_or_slow = 0;
            }
        }
        else if (s_pb_handle != NULL)
        {
            switch (playmode)
            {
            case ACTION_PAUSE:
                anj_record_pb_pause_set(s_pb_handle, 1);
                cur_status = 2;
                break;
            case ACTION_STOP:
                h5_playback_release();
                cur_status = 0;
                s_speed = 0;
                s_fast_or_slow = 0;
                s_session_id = 0;
                break;
            case ACTION_FAST:
                if (s_fast_or_slow == 1)
                {
                    if (s_speed == 0)
                    {
                        s_speed = 2;
                    }
                    else if (s_speed == 32)
                    {
                        s_speed = 0;
                    }
                    else
                    {
                        s_speed = s_speed * 2;
                    }
                }
                else if (s_fast_or_slow == 2)
                {
                    s_speed = 0;
                }
                else
                {
                    s_speed = 2;
                }
                s_fast_or_slow = (s_speed == 0) ? 0 : 1;
                anj_record_pb_speed_set(s_pb_handle, s_speed);
                cur_status = 1;
                break;
            case ACTION_SLOW:
                if (s_fast_or_slow == 1)
                {
                    s_speed = 0;
                }
                else if (s_fast_or_slow == 2)
                {
                    if (s_speed == 0)
                    {
                        s_speed = 2;
                    }
                    else if (s_speed == 32)
                    {
                        s_speed = 0;
                    }
                    else
                    {
                        s_speed = s_speed * 2;
                    }
                }
                else
                {
                    s_speed = 2;
                }
                s_fast_or_slow = (s_speed == 0) ? 0 : 2;
                anj_record_pb_speed_set(s_pb_handle, s_speed);
                cur_status = 1;
                break;
            case ACTION_FRAMESKIP:
                cur_status = 2;
                break;
            case ACTION_RESUME:
                anj_record_pb_pause_set(s_pb_handle, 0);
                cur_status = 1;
                s_speed = 0;
                s_fast_or_slow = 0;
                break;
            default:
                break;
            }
        }
        else
        {
            /* Allow resume-after-stop by re-creating handle at last sent play time. */
            if (playmode == ACTION_RESUME && s_last_play_abs_sec > 0)
            {
                err_code = h5_playback_create_at_time(s_last_play_abs_sec);
                if (err_code == 0)
                {
                    cur_status = 1;
                    s_speed = 0;
                    s_fast_or_slow = 0;
                }
                else
                {
                    cur_status = 0;
                }
            }
            else
            {
                /* Non-fatal control command before TIMEPLAY/SEEK, keep connection alive. */
                cur_status = 0;
                __WARN("h5 playback no handle, playmode=%u ignored status=%d\n", playmode, cur_status);
                err_code = 0;
            }
            break;
        }

        if (cur_status == 1)
        {
            if (s_fast_or_slow == 1)
            {
                cur_status = 3;
            }
            else if (s_fast_or_slow == 2)
            {
                cur_status = 4;
            }
        }
    } while (0);

    if (err_code != -1)
    {
        h5_playback_send_status(session_id, cur_status, s_speed);
    }

    return err_code;
}

int anj_h5_playback_release_session(unsigned int session_id)
{
    if (session_id == 0)
    {
        return -1;
    }

    if (s_session_id != session_id)
    {
        return -1;
    }

    h5_playback_release();
    __INFO("h5 playback release session:%u\n", session_id);
    s_session_id = 0;
    s_speed = 0;
    s_fast_or_slow = 0;
    s_last_play_abs_sec = 0;
    return 0;
}
