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
// C++ header

#ifndef _MEDIASTREAM_INPUT_HH
#define _MEDIASTREAM_INPUT_HH

#include <vector>
#include <MediaSink.hh>
#include "protocol_queue.h"

class MediaStreamInput : public Medium
{
public:
  static MediaStreamInput *createNew(UsageEnvironment &env, int vType);
  static MediaStreamInput *createNew(UsageEnvironment &env, int vType, int vChn, int iCameraId);
  FramedSource *videoSource();
  FramedSource *audioSource();
  unsigned char fvideo_config[256];
  int fvideo_config_len;

  int videoType;

private:
  MediaStreamInput(UsageEnvironment &env, int vType);                // called only by createNew()
  MediaStreamInput(UsageEnvironment &env, int vType, int vChn, int iCameraId); // called only by createNew()
  virtual ~MediaStreamInput();

  friend class VideoOpenFileSource;
  friend class AudioOpenFileSource;
  FramedSource *fOurVideoSource;
  FramedSource *fOurAudioSource;

  ANJ_MBUF_HANDLE *fVideoReaderHandler;
  FRAME_BUFFER_MANAGER *fAudioReaderMgr;

  int fCameraId;
};

////////// OpenFileSource definition //////////

// A common "FramedSource" subclass, used for reading from an open file:

class OpenFileSource : public FramedSource
{
public:
  OpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay);
  virtual ~OpenFileSource();

  virtual int readFromFile() = 0;

private: // redefined virtual functions:
  virtual void doGetNextFrame();

private:
  static void incomingDataHandler(OpenFileSource *source);
  void incomingDataHandler1();

protected:
  MediaStreamInput &fInput;
  int fusDelay;
  int fusCurDelay;
};

////////// VideoOpenFileSource definition //////////

class VideoOpenFileSource : public OpenFileSource
{
public:
  VideoOpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay);
  virtual ~VideoOpenFileSource();

protected: // redefined virtual functions:
  virtual int readFromFile();
  void releaseMediaResources();
  int getStreamIndex();
  int handleFirstFrame(int stream_index);
  void getVideoFrameConfig(media_frame_info_t &st_frame_data);
  int handleIFrame(media_frame_info_t &st_frame_data, int stream_index);
  int handlePFrame(media_frame_info_t &st_frame_data, int stream_index);
  int handleVideoFrame(media_frame_info_t &st_frame_data, int stream_index);
  int processNewFrame(int stream_index);

  int StreamFlag;
  int ContineReadZeroNum;
  unsigned int NoStreamCnt;
  unsigned int HaveStreamCnt;

public:
  std::vector<unsigned char> iframe_buf;
  int iframe_buf_size;
  int iframe_size;

  unsigned char fsps_buf[512];
  int fsps_len;

  unsigned char fpps_buf[512];
  int fpps_len;

  unsigned char fsei_buf[512];
  int fsei_len;
};

#define STREAM_GET_VOL 0x0001
#define STREAM_NEW_GOP 0x0002

#define STREAM_READER_DELAY_MICROSECOND 20000
#define STREAM_READER_DELAY_MIN_MICROSECOND 10000

////////// AudioOpenFileSource definition //////////

class AudioOpenFileSource : public OpenFileSource
{
public:
  AudioOpenFileSource(UsageEnvironment &env, MediaStreamInput &input, int usDelay);
  virtual ~AudioOpenFileSource();

protected: // redefined virtual functions:
  virtual int readFromFile();
  int getAudioData();
};

enum
{
  VIDEO_TYPE_MPEG4 = 0,
  VIDEO_TYPE_MPEG4_CIF,
  VIDEO_TYPE_H264,
  VIDEO_TYPE_H264_CIF,
  VIDEO_TYPE_MJPEG,
  VIDEO_TYPE_REPLAY,
  VIDEO_TYPE_H265,
  VIDEO_TYPE_H265_CIF,
};

// Functions to set the optimal buffer size for RTP sink objects.
// These should be called before each RTPSink is created.
#define AUDIO_MAX_FRAME_SIZE 20 * 1024

#define VIDEO_MAX_FRAME_SIZE ANJ_CAMERA_VIDEO_MAX_SIZE
inline void setAudioRTPSinkBufferSize() { OutPacketBuffer::maxSize = AUDIO_MAX_FRAME_SIZE; }
inline void setVideoRTPSinkBufferSize() { OutPacketBuffer::maxSize = VIDEO_MAX_FRAME_SIZE; }

#endif
