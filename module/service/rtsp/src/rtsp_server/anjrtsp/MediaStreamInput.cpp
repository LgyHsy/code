/*
 * Copyright (C) 2005-2006 WIS Technologies International Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and the associated README documentation file (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
// An interface to the WIS GO7007 capture device.
// Implementation

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include "anj_mw_comm.h"

#include "anj_config.h"
#include "anj_mbuf.h"
#include "anj_comm.h"
#include "anj_video.h"
#include "anj_sysmng.h"
#include "record_log.h"
#include "sdk_option.h"

#include "Err.hh"
#include "MediaStreamInput.hh"

static char mpeg4_header[] = {0x00, 0x00, 0x01, 0xb0, 0x01, 0x00, 0x00, 0x01, 0xb5, 0x09};

void printErr(UsageEnvironment &env, char const *str = NULL)
{
    if (str != NULL)
        err(env) << str;
    env << ": " << strerror(env.getErrno()) << "\n";
}

void TimeStampGetTimeVal(struct timeval *time, double timestamp_ms)
{
    time->tv_sec = (long)(timestamp_ms / 1000);
    time->tv_usec = (long)((long long)(timestamp_ms) % 1000) * 1000;
}

////////// MediaStreamInput implementation //////////

MediaStreamInput *MediaStreamInput::createNew(UsageEnvironment &env, int vType)
{
    return new MediaStreamInput(env, vType);
}

MediaStreamInput *MediaStreamInput::createNew(UsageEnvironment &env, int vType, int vchn, int iCameraId)
{
    return new MediaStreamInput(env, vType, vchn, iCameraId);
}

FramedSource *MediaStreamInput::videoSource()
{
    if (fOurVideoSource == NULL)
    {
        fOurVideoSource = new VideoOpenFileSource(envir(), *this, STREAM_READER_DELAY_MICROSECOND);
    }
    return fOurVideoSource;
}

FramedSource *MediaStreamInput::audioSource()
{
    if (fOurAudioSource == NULL)
    {
        fOurAudioSource = new AudioOpenFileSource(envir(), *this, STREAM_READER_DELAY_MICROSECOND);
    }
    return fOurAudioSource;
}

MediaStreamInput::MediaStreamInput(UsageEnvironment &env, int vType)
    : Medium(env), videoType(vType), fOurVideoSource(NULL), fOurAudioSource(NULL)
{

    fVideoReaderHandler = NULL;
    fAudioReaderMgr = NULL;

    fvideo_config_len = 0;
}

MediaStreamInput::MediaStreamInput(UsageEnvironment &env, int vType, int vchn, int iCameraId)
    : Medium(env), videoType(vType), fOurVideoSource(NULL), fOurAudioSource(NULL)
{
    fVideoReaderHandler = NULL;
    fAudioReaderMgr = NULL;

    fvideo_config_len = 0;
    fCameraId = iCameraId;

    __INFO("rtsp mediastream input create video chn:%d, camera_id:%d\n", vchn, iCameraId);
}

MediaStreamInput::~MediaStreamInput()
{
    __INFO("rtsp mediastream input closed.\n");

    if (fVideoReaderHandler)
    {
        anj_mbuf_destory_reader(fVideoReaderHandler);
        fVideoReaderHandler = NULL;
    }

    if (fAudioReaderMgr)
    {
        frame_mgr_release(fAudioReaderMgr);
        anj_mw_free(fAudioReaderMgr);
        fAudioReaderMgr = NULL;
    }

    if (fOurVideoSource)
        Medium::close(fOurVideoSource);

    if (fOurAudioSource)
        Medium::close(fOurAudioSource);
}

////////// OpenFileSource implementation //////////

OpenFileSource ::OpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay)
    : FramedSource(env),
      fInput(input),
      fusDelay(usDelay),
      fusCurDelay(usDelay)
{
}

OpenFileSource::~OpenFileSource()
{
}

void OpenFileSource::doGetNextFrame()
{
    incomingDataHandler(this);
}

void OpenFileSource ::incomingDataHandler(OpenFileSource *source)
{
    source->incomingDataHandler1();
}

void OpenFileSource::incomingDataHandler1()
{
    int ret;

    if (!isCurrentlyAwaitingData())
    {
        __ERR("rtsp waiting data...\n");
        return; // we're not ready for the data yet
    }

    ret = readFromFile();
    if (ret < 0)
    {
        handleClosure(this);
        fprintf(stderr, "In Grab Image, the source stops being readable!!!!\n");
    }
    else if (ret == 0)
    {
        int uSecsToDelay = fusCurDelay; // Video:20ms/Audio 20ms
        fusCurDelay = fusCurDelay / 2;

        if (fusCurDelay < STREAM_READER_DELAY_MIN_MICROSECOND)
        {
            fusCurDelay = STREAM_READER_DELAY_MIN_MICROSECOND;
        }

        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                 (TaskFunc *)incomingDataHandler, this);
    }
    else
    {
        fusCurDelay = fusDelay;
        nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc *)afterGetting, this);
    }
}

////////// VideoOpenFileSource implementation //////////

VideoOpenFileSource ::VideoOpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay)
    : OpenFileSource(env, input, usDelay), StreamFlag(STREAM_GET_VOL), ContineReadZeroNum(0), NoStreamCnt(0), HaveStreamCnt(0)
{

    int offset = 0;

    fInput.fvideo_config_len = 0;
    fInput.fVideoReaderHandler = NULL;
    if (fInput.videoType == VIDEO_TYPE_MPEG4 || fInput.videoType == VIDEO_TYPE_MPEG4_CIF)
    {
        memcpy(fInput.fvideo_config, mpeg4_header, sizeof(mpeg4_header));
        offset = sizeof(mpeg4_header);
        __INFO("mpeg offset:%d\n", offset);
    }
    int stream_index = getStreamIndex();
    if ((stream_index % MAX_VENC_CHN) == 0)
    {
        iframe_buf_size = ANJ_CAMERA_VIDEO_MAX_SIZE;
    }
    else
    {
        iframe_buf_size = ANJ_CAMERA_VIDEO_SUB_MAX_SIZE;
    }

    iframe_size = 0;
    iframe_buf.resize(iframe_buf_size);
    memset(iframe_buf.data(), 0, iframe_buf_size);
    __INFO("rtsp init video open file source! stream_index:%d malloc buf size %d\n", stream_index, iframe_buf_size);

    fsps_len = 0;
    fpps_len = 0;
    fsei_len = 0;
}

VideoOpenFileSource::~VideoOpenFileSource()
{
    __INFO("rtsp uninit video type:%d cameraid:%d reader handle!\n", fInput.videoType, fInput.fCameraId);
    releaseMediaResources();
}

void VideoOpenFileSource::releaseMediaResources()
{
    if (fInput.fVideoReaderHandler)
    {
        __INFO("rtsp destory video reader:%p\n", fInput.fVideoReaderHandler);
        anj_mbuf_destory_reader(fInput.fVideoReaderHandler);
        fInput.fVideoReaderHandler = NULL;
    }

    int stream_index = getStreamIndex();
    if (!iframe_buf.empty())
    {
        __ERR("clear iframe_buf stream_index:%d\n", stream_index);
        iframe_buf.clear();
        iframe_size = 0;
    }

    memset(fInput.fvideo_config, 0, sizeof(fInput.fvideo_config));
    fInput.fvideo_config_len = 0;
    fInput.fOurVideoSource = NULL;
}

int VideoOpenFileSource::getStreamIndex()
{
    int stream_no = 0;
    int stream_index = 0;
    if ((fInput.videoType == VIDEO_TYPE_H264) ||
        (fInput.videoType == VIDEO_TYPE_H265) ||
        (fInput.videoType == VIDEO_TYPE_MPEG4))
    {
        stream_no = 0;
    }
    else
    {
        stream_no = 1;
    }

    stream_index = (fInput.fCameraId * MAX_VENC_CHN) + stream_no;

    return stream_index;
}

int VideoOpenFileSource::handleFirstFrame(int stream_index)
{
    if (fInput.fVideoReaderHandler == NULL)
    {
        fInput.fVideoReaderHandler = anj_mbuf_create_reader(stream_index, 1);
        if (!fInput.fVideoReaderHandler)
        {
            __ERR("create media mbuf reader for stream %d error!\n", stream_index);
            usleep(10 * 1000);
            return -1;
        }

        __INFO("rtsp create video mbuf reader:%p for stream %d success!\n", fInput.fVideoReaderHandler, stream_index);
    }

    // 读取初始I帧获取配置信息
    media_frame_info_t st_frame_data;
    memset(&st_frame_data, 0, sizeof(st_frame_data));
    if (anj_mbuf_read_frame(fInput.fVideoReaderHandler, 1, &st_frame_data, 2000) > 0)
    {
        getVideoFrameConfig(st_frame_data);
        handleIFrame(st_frame_data, stream_index);
        if (st_frame_data.frameParam.frameLen > iframe_buf.size())
        {
            __WARN("iframe_buf truncated: need %d, have %zu\n", st_frame_data.frameParam.frameLen, iframe_buf.size());
            iframe_buf.resize(st_frame_data.frameParam.frameLen);
        }
        memcpy(iframe_buf.data(), st_frame_data.frameBuf, st_frame_data.frameParam.frameLen);
        iframe_size = st_frame_data.frameParam.frameLen;
        anj_mbuf_read_release(fInput.fVideoReaderHandler, &st_frame_data);
        return 1;
    }
    return 0;
}

void VideoOpenFileSource::getVideoFrameConfig(media_frame_info_t &st_frame_data)
{
    if (st_frame_data.frameBuf && st_frame_data.frameParam.frameLen > 0 && fInput.fvideo_config_len == 0)
    {
        int is_h264 = !((fInput.videoType == VIDEO_TYPE_H265) ||
                        (fInput.videoType == VIDEO_TYPE_H265_CIF));

        unsigned int offset = 0;
        unsigned int param_size = 0;

        if (is_h264)
        {
            offset = stream_h264_pps_offset_get(st_frame_data.frameBuf, st_frame_data.frameParam.frameLen);
            if (offset > 0)
            {
                param_size = offset + 8;
                if (param_size > sizeof(fInput.fvideo_config))
                {
                    param_size = sizeof(fInput.fvideo_config);
                }
                memcpy(fInput.fvideo_config, st_frame_data.frameBuf, param_size);
                fInput.fvideo_config_len = param_size;
                fFrameSize = fInput.fvideo_config_len;
            }
            else
            {
                __ERR("don't find pps!\n");
                fFrameSize = 0;
            }
        }
        else
        {
            offset = stream_h265_pps_offset_get(st_frame_data.frameBuf,
                                                st_frame_data.frameParam.frameLen);
            if (offset > 0)
            {
                param_size = offset + 11;
                if (param_size > sizeof(fInput.fvideo_config))
                {
                    param_size = sizeof(fInput.fvideo_config);
                }
                memcpy(fInput.fvideo_config, st_frame_data.frameBuf, param_size);
                fInput.fvideo_config_len = param_size;
                fFrameSize = fInput.fvideo_config_len;
            }
            else
            {
                __ERR("don't find pps!\n");
                fFrameSize = 0;
            }
        }
    }
}

int VideoOpenFileSource::handleIFrame(media_frame_info_t &st_frame_data, int stream_index)
{
    char vps[256], sps[256], pps[256], sei[256];
    int vps_len = 0, sps_len = 0, pps_len = 0, sei_len = 0;
    bool haveParameterSet = false;

    bool is_h265 = (fInput.videoType == VIDEO_TYPE_H265) || (fInput.videoType == VIDEO_TYPE_H265_CIF);

    if (is_h265)
    {
        fFrameOffset = h265_get_vps_sps_pps_sei((char *)st_frame_data.frameBuf,
                                                st_frame_data.frameParam.frameLen,
                                                False, False,
                                                vps, &vps_len, sps, &sps_len,
                                                pps, &pps_len, sei, &sei_len);
        haveParameterSet = ((fFrameOffset > 0) && (vps_len > 0) && (pps_len > 0) && (sps_len > 0) &&
                            (vps_len < 256) && (pps_len < 256) && (sps_len < 256));
    }
    else
    {
        fFrameOffset = h264_get_sps_pps_sei((char *)st_frame_data.frameBuf,
                                            st_frame_data.frameParam.frameLen,
                                            False, False,
                                            sps, &sps_len, pps, &pps_len, sei, &sei_len);
        haveParameterSet = ((fFrameOffset > 0) && (pps_len > 0) && (sps_len > 0) &&
                            (pps_len < 256) && (sps_len < 256));
    }

    if (haveParameterSet)
    {
        if (is_h265)
        {
            fFrameSize = vps_len;
            if (fFrameSize > fMaxSize)
            {
                __ERR("444 VPS Frame Truncated fFrameSize:%d fMaxSize:%d\n", fFrameSize, fMaxSize);
                fNumTruncatedBytes = fFrameSize - fMaxSize;
                fFrameSize = fMaxSize;
            }
            else
            {
                fNumTruncatedBytes = 0;
            }

            memcpy(fTo, vps, fFrameSize);

            memcpy(fsps_buf, sps, sps_len);
            fsps_len = sps_len;

            memcpy(fpps_buf, pps, pps_len);
            fpps_len = pps_len;
        }
        else
        {
            fFrameSize = sps_len;
            if (fFrameSize > fMaxSize)
            {
                __ERR("555 SPS Frame Truncated fFrameSize:%d fMaxSize:%d\n",
                      fFrameSize, fMaxSize);
                fNumTruncatedBytes = fFrameSize - fMaxSize;
                fFrameSize = fMaxSize;
            }
            else
            {
                fNumTruncatedBytes = 0;
            }
            memcpy(fTo, sps, fFrameSize);

            memcpy(fpps_buf, pps, pps_len);
            fpps_len = pps_len;
        }
        iframe_size = st_frame_data.frameParam.frameLen - fFrameOffset;
        if (st_frame_data.frameParam.frameLen > iframe_buf.capacity())
        {
            __WARN("iframe_buf truncated: need %d, have %zu\n", st_frame_data.frameParam.frameLen, iframe_buf.capacity());
            iframe_buf.resize(st_frame_data.frameParam.frameLen);
        }
        memcpy(iframe_buf.data(), st_frame_data.frameBuf, st_frame_data.frameParam.frameLen);
        iframe_size = st_frame_data.frameParam.frameLen;
        return 1;
    }
    else
    {
        // 没有参数集，按P帧处理
        return handlePFrame(st_frame_data, stream_index);
    }
}

int VideoOpenFileSource::handlePFrame(media_frame_info_t &st_frame_data, int stream_index)
{
    bool is_h265 = (fInput.videoType == VIDEO_TYPE_H265) || (fInput.videoType == VIDEO_TYPE_H265_CIF);

    if (is_h265)
    {
        fFrameOffset = h265_get_pframe_offset((char *)st_frame_data.frameBuf,
                                              st_frame_data.frameParam.frameLen, False);
    }
    else
    {
        fFrameOffset = h264_get_pframe_offset((char *)st_frame_data.frameBuf,
                                              st_frame_data.frameParam.frameLen, False);
    }

    fFrameSize = st_frame_data.frameParam.frameLen - fFrameOffset;
    if (fFrameSize > fMaxSize)
    {
        __ERR("666 P Frame Truncated fFrameSize:%d fMaxSize:%d\n",
              fFrameSize, fMaxSize);
        fNumTruncatedBytes = fFrameSize - fMaxSize;
        fFrameSize = fMaxSize;
    }
    else
    {
        fNumTruncatedBytes = 0;
    }

    memcpy(fTo, st_frame_data.frameBuf + fFrameOffset, fFrameSize);
    return 1;
}

int VideoOpenFileSource::handleVideoFrame(media_frame_info_t &st_frame_data, int stream_index)
{
    MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pVideoEncCfg = &mediacfg->videoConfig[fInput.fCameraId].videoEncode;

    int result = 0;

    if (MEDIA_VFRAME_I == st_frame_data.frameParam.frameType)
    {
        result = handleIFrame(st_frame_data, stream_index);
    }
    else
    {
        result = handlePFrame(st_frame_data, stream_index);
    }

    if (result > 0)
    {
        // 设置时间戳和帧信息
        double timestamp = st_frame_data.frameParam.framePts;
        TimeStampGetTimeVal(&fPresentationTime, timestamp);
        fstFrameInfo.nFrameType = st_frame_data.frameParam.frameType;
        fstFrameInfo.ulFrameIndex = st_frame_data.frameParam.vframeIndex;
        fstFrameInfo.ulLastIFrameIndex = st_frame_data.frameParam.frameKeyIndex;
        fstFrameInfo.nFrameRate = pVideoEncCfg->encodeCfg[stream_index].frameRate;

        ContineReadZeroNum = 0;
        if (NoStreamCnt > 0 && HaveStreamCnt++ > 1000)
        {
            NoStreamCnt = 0;
            HaveStreamCnt = 0;
        }
    }

    return result;
}

int VideoOpenFileSource::processNewFrame(int stream_index)
{
    int iRet = 0;
    media_frame_info_t st_frame_data;
    memset(&st_frame_data, 0, sizeof(st_frame_data));
    /* H265 frist send vps, second send sps, third send pps, and then send Iframe
   H264 frist send sps, second send pps, and then send Iframe */
    if (fsps_len)
    {
        fFrameSize = fsps_len;
        if (fFrameSize > fMaxSize)
        {
            __ERR("SPS Frame Truncated fFrameSize:%d fMaxSize:%d\n", fFrameSize, fMaxSize);
            fNumTruncatedBytes = fFrameSize - fMaxSize;
            fFrameSize = fMaxSize;
        }
        else
        {
            fNumTruncatedBytes = 0;
        }

        memcpy(fTo, fsps_buf, fFrameSize);
        fsps_len = 0;
        return 1;
    }
    else if (fpps_len)
    {
        fFrameSize = fpps_len;
        if (fFrameSize > fMaxSize)
        {
            __ERR("PPS Frame Truncated fFrameSize:%d fMaxSize:%d\n", fFrameSize, fMaxSize);
            fNumTruncatedBytes = fFrameSize - fMaxSize;
            fFrameSize = fMaxSize;
        }
        else
        {
            fNumTruncatedBytes = 0;
        }

        memcpy(fTo, fpps_buf, fFrameSize);
        fpps_len = 0;
        return 1;
    }
    else if (iframe_size && !iframe_buf.empty())
    {
        fFrameSize = iframe_size;
        if (fFrameSize > fMaxSize)
        {
            __ERR("I Frame Truncated fFrameSize:%d fMaxSize:%d\n", fFrameSize, fMaxSize);
            fNumTruncatedBytes = fFrameSize - fMaxSize;
            fFrameSize = fMaxSize;
        }
        else
        {
            fNumTruncatedBytes = 0;
        }

        memcpy(fTo, iframe_buf.data() + fFrameOffset, fFrameSize);
        iframe_size = 0;
        fFrameOffset = 0;
        return 1;
    }

    if ((anj_mbuf_read_frame(fInput.fVideoReaderHandler, 0, &st_frame_data, 100) > 0))
    {
        if (st_frame_data.frameParam.frameType != MEDIA_AFRAME_A)
        {
            iRet = handleVideoFrame(st_frame_data, stream_index);
        }
        else
        {
            if (fInput.fAudioReaderMgr)
            {
                FRAME_ENTRY frame = {0};
                frame.pFrame = (char *)anj_mw_malloc(st_frame_data.frameParam.frameLen);
                memcpy(frame.pFrame, st_frame_data.frameBuf, st_frame_data.frameParam.frameLen);
                frame.nFrameLen = st_frame_data.frameParam.frameLen;
                frame.nFrameType = 2;
                long long video_ms = (long long)fPresentationTime.tv_sec * 1000LL + (fPresentationTime.tv_usec / 1000LL);
                frame.nTimestamp = (double)video_ms;
                frame.nFlag = st_frame_data.frameParam.aframeIndex;
                frame_mgr_push(fInput.fAudioReaderMgr, &frame);
            }
        }
        anj_mbuf_read_release(fInput.fVideoReaderHandler, &st_frame_data);
    }
    else
    {
        ContineReadZeroNum++;
        if (ContineReadZeroNum >= 500)
        {
            __ERR("video chn[%d] no stream more than 10s\n", stream_index);
            if (++NoStreamCnt > 8)
            {
                __ERR("video chn[%d] no stream reach %ds! system reboot!\n",
                      stream_index, (5 * NoStreamCnt));
                __RECORD_LOG_INFO("video chn[%d] no stream reach %ds! system reboot!\n",
                      stream_index, (5 * NoStreamCnt));
                anj_sysmng_reboot();
            }

            HaveStreamCnt = 0;
            ContineReadZeroNum = 0;
            releaseMediaResources();
            StreamFlag |= STREAM_GET_VOL;
        }
    }
    return iRet;
}

int VideoOpenFileSource::readFromFile()
{
    int stream_index = getStreamIndex();
    if (StreamFlag & STREAM_GET_VOL)
    {
        StreamFlag &= ~STREAM_GET_VOL;
        return handleFirstFrame(stream_index);
    }
    return processNewFrame(stream_index);
}

////////// AudioOpenFileSource implementation //////////

AudioOpenFileSource ::AudioOpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay)
    : OpenFileSource(env, input, usDelay)
{
    __INFO("rtsp init audio open file source!\n");
    if (fInput.fAudioReaderMgr)
    {
        frame_mgr_release(fInput.fAudioReaderMgr);
        anj_mw_free(fInput.fAudioReaderMgr);
        fInput.fAudioReaderMgr = NULL;
    }
    if (NULL == fInput.fAudioReaderMgr)
    {
        fInput.fAudioReaderMgr = (FRAME_BUFFER_MANAGER *)anj_mw_malloc(sizeof(FRAME_BUFFER_MANAGER));
        if (fInput.fAudioReaderMgr == NULL)
        {
            __ERR("rtsp audio reader mgr malloc failed\n");
        }
        else
        {
            frame_mgr_init(fInput.fAudioReaderMgr, 50);
            __INFO("create anj audio reader mgr success!\n");
        }
    }
}

AudioOpenFileSource::~AudioOpenFileSource()
{
    __ERR("rtsp uninit audio reader.\n");

    if (fInput.fAudioReaderMgr)
    {
        __INFO("rtsp destory audio mgr:%p\n", fInput.fAudioReaderMgr);
        frame_mgr_release(fInput.fAudioReaderMgr);
        anj_mw_free(fInput.fAudioReaderMgr);
        fInput.fAudioReaderMgr = NULL;
    }
    fInput.fOurAudioSource = NULL;
}

int AudioOpenFileSource::getAudioData()
{
    fFrameSize = 0;
    if (fInput.fAudioReaderMgr)
    {
        FRAME_ENTRY frame = {0};
        if (frame_mgr_pop(fInput.fAudioReaderMgr, &frame) > 0)
        {
            if ((unsigned)frame.nFrameLen > fMaxSize)
            {
                frame.nFrameLen = (int)fMaxSize;
            }

            double timestamp = frame.nTimestamp;
            TimeStampGetTimeVal(&fPresentationTime, timestamp);
            memcpy(fTo, frame.pFrame, frame.nFrameLen);
            fstFrameInfo.nFrameType = frame.nFrameType;
            fstFrameInfo.ulFrameIndex = frame.nFlag;
            fstFrameInfo.ulLastIFrameIndex = frame.nFlag;
            fstFrameInfo.nFrameRate = 25;
            fFrameSize = frame.nFrameLen;
            anj_mw_free(frame.pFrame);
            return 1;
        }
    }

    return 0;
}

int AudioOpenFileSource::readFromFile()
{
    return getAudioData();
}
