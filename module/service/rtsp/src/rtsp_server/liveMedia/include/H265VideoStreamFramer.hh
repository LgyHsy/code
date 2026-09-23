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
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
#include "rtsp_func_macro.hh"

#ifdef _H265_FUNC_

#ifndef _H265_VIDEO_STREAM_FRAMER_HH
#define _H265_VIDEO_STREAM_FRAMER_HH

#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif


class H265VideoStreamFramer: public FramedFilter {
public:

  static H265VideoStreamFramer* createNew(UsageEnvironment& env, FramedSource* inputSource);
  virtual Boolean currentNALUnitEndsAccessUnit();
  Boolean& pictureEndMarker() { return fPictureEndMarker; }    // a hack for implementing the RTP 'M' bit

protected:
  // Constructor called only by createNew(), or by subclass constructors
  H265VideoStreamFramer(UsageEnvironment& env,
                            FramedSource* inputSource,
                            Boolean createParser = True);
  virtual ~H265VideoStreamFramer();


public: 
  static void continueReadProcessing(void* clientData,
                     unsigned char* ptr, unsigned size,
                     struct timeval presentationTime);
  void continueReadProcessing();


private:
  virtual void doGetNextFrame();
  virtual Boolean isH265VideoStreamFramer() const;
  

protected:

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

  double   fFrameRate;    // Note: For MPEG-4, this is really a 'tick rate' ??
  unsigned fPictureCount; // hack used to implement doGetNextFrame() ??
  Boolean  fPictureEndMarker;

private:
  class H265VideoStreamParser* fParser;
  struct timeval fPresentationTimeBase;
};

#endif

#endif

