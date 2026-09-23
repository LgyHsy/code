/**********
Copyright (C) 1998-2016 Topsee Electronic Tech Co.,Ltd.

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2015 Live Networks, Inc.  All rights reserved.
// RTP sink for H.265 video
// C++ header
#include "rtsp_func_macro.hh"

#ifdef _H265_FUNC_

#ifndef _H265_VIDEO_RTP_SINK_HH
#define _H265_VIDEO_RTP_SINK_HH

#ifndef _VIDEO_RTP_SINK_HH
#include "VideoRTPSink.hh"
#endif
#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif

class H265FUAFragmenter;

class H265VideoRTPSink: public VideoRTPSink {
public:
  static H265VideoRTPSink* createNew(UsageEnvironment& env,
				                     Groupsock* RTPgs,
				                     unsigned char rtpPayloadFormat,
				                     unsigned profile_level_id,
				                     char const* sprop_parameter_sets_str);
    // an optional variant of "createNew()", useful if we know, in advance,
    // the stream's VPS, SPS and PPS NAL units.
    // This avoids us having to 'pre-read' from the input source in order to get these values.

protected:
  H265VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		           unsigned char rtpPayloadFormat,
		           unsigned profile_level_id,
		           char const* sprop_parameter_sets_str,
		           int nHNumber);
	// called only by createNew()
  virtual ~H265VideoRTPSink();

protected: // redefined virtual functions:
  virtual char const* auxSDPLine();

private: // redefined virtual functions:
  virtual Boolean sourceIsCompatibleWithUs(MediaSource& source);
  virtual Boolean continuePlaying();
  virtual void stopPlaying();
  virtual void doSpecialFrameHandling(unsigned fragmentationOffset,
                                      unsigned char* frameStart,
                                      unsigned numBytesInFrame,
                                      struct timeval frameTimestamp,
                                      unsigned numRemainingBytes,
                                      MediaFrameInfo stFrameInfo);
  virtual Boolean frameCanAppearAfterPacketStart(unsigned char const* frameStart,
						 unsigned numBytesInFrame) const;

protected:
  int fnHNumber;    
  H265FUAFragmenter* fOurFragmenter;

private:
  char* fFmtpSDPLine;  
};

////////// H265FUAFragmenter definition //////////

// Because of the ideosyncracies of the H.264 RTP payload format, we implement
// "H265VideoRTPSink" using a separate "H265FUAFragmenter" class that delivers,
// to the "H265VideoRTPSink", only fragments that will fit within an outgoing
// RTP packet.  I.e., we implement fragmentation in this separate "H265FUAFragmenter"
// class, rather than in "H265VideoRTPSink".
// (Note: This class should be used only by "H265VideoRTPSink", or a subclass.)

class H265FUAFragmenter: public FramedFilter {
public:
  H265FUAFragmenter(UsageEnvironment& env, FramedSource* inputSource,
		    unsigned inputBufferMax, unsigned maxOutputPacketSize);
  virtual ~H265FUAFragmenter();

  Boolean lastFragmentCompletedNALUnit() const { return fLastFragmentCompletedNALUnit; }

private: // redefined virtual functions:
  virtual void doGetNextFrame();

private:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds,
                                MediaFrameInfo stFrameInfo);
  void afterGettingFrame1(unsigned frameSize,
                          unsigned numTruncatedBytes,
                          struct timeval presentationTime,
                          unsigned durationInMicroseconds,
                          MediaFrameInfo stFrameInfo);

private:
  unsigned fInputBufferSize;
  unsigned fMaxOutputPacketSize;
  unsigned char* fInputBuffer;
  unsigned fNumValidDataBytes;
  unsigned fCurDataOffset;
  unsigned fSaveNumTruncatedBytes;
  Boolean fLastFragmentCompletedNALUnit;
};


#endif
#endif

