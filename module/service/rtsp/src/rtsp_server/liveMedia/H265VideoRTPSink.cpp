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
// "liveMedia"
// Copyright (c) 1996-2007 Live Networks, Inc.  All rights reserved.
// RTP sink for H.265 video (RFC 3984)
// Implementation
#include "rtsp_func_macro.hh"

#ifdef _H265_FUNC_
#include "H265VideoRTPSink.hh"
#include "H265VideoStreamFramer.hh"
//#include "H264VideoRTPSource.hh" // for "parseSPropParameterSets()"

#include "anj_mw_log.h"
#include "anj_config.h"

////////// H265VideoRTPSink implementation //////////

H265VideoRTPSink
::H265VideoRTPSink(UsageEnvironment& env, Groupsock* RTPgs,
		   unsigned char rtpPayloadFormat,
		   unsigned profile_level_id,
		   char const* sprop_parameter_sets_str,
		   int nHNumber)
  : VideoRTPSink(env, RTPgs, rtpPayloadFormat, 90000, "H265"), 
    fnHNumber(nHNumber),
    fOurFragmenter(NULL)
{
  // Set up the "a=fmtp:" SDP line for this stream:
  char const* fmtpFmt =
    "a=fmtp:%d packetization-mode=1"
    ";profile-level-id=%06X"
    ";sprop-parameter-sets=%s\r\n";
  unsigned fmtpFmtSize = strlen(fmtpFmt)
    + 3 /* max char len */
    + 8 /* max unsigned len in hex */
    + strlen(sprop_parameter_sets_str);
  char* fmtp = new char[fmtpFmtSize];
  fflush(stderr);

  sprintf(fmtp, fmtpFmt,
          rtpPayloadFormat,
	      profile_level_id,
          sprop_parameter_sets_str);
  fFmtpSDPLine = strDup(fmtp);
  delete[] fmtp;
}


H265VideoRTPSink::~H265VideoRTPSink()
{
  
  __ERR("destroy H265VideoRTPSink\n");
  
  fSource = fOurFragmenter; // hack: in case "fSource" had gotten set to NULL before we were called
  delete[] fFmtpSDPLine;
  stopPlaying(); // call this now, because we won't have our 'FUA fragmenter' when the base class destructor calls it later.

  // Close our 'FUA fragmenter' as well:
  Medium::close(fOurFragmenter);
  fSource = NULL; // for the base class destructor, which gets called next
}

H265VideoRTPSink*
H265VideoRTPSink::createNew(UsageEnvironment& env, Groupsock* RTPgs,
			    unsigned char rtpPayloadFormat,
			    unsigned profile_level_id,
			    char const* sprop_parameter_sets_str)
{
  return new H265VideoRTPSink(env, RTPgs, rtpPayloadFormat,
			                  profile_level_id, 
			                  sprop_parameter_sets_str, 265);
}

Boolean H265VideoRTPSink::sourceIsCompatibleWithUs(MediaSource& source) 
{
  // Our source must be an appropriate framer:
  return source.isH265VideoStreamFramer();
}

Boolean H265VideoRTPSink::continuePlaying() 
{
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  // First, check whether we have a 'fragmenter' class set up yet.
  // If not, create it now:
  if (fOurFragmenter == NULL) {
    fOurFragmenter = new H265FUAFragmenter(envir(), fSource, OutPacketBuffer::maxSize,
					   ourMaxPacketSize() - RtpFrameHeaderLen()/*RTP hdr size*/);
    fSource = fOurFragmenter;
  }
  
  //Then call the parent class's implementation:
  return MultiFramedRTPSink::continuePlaying();
}

void H265VideoRTPSink::stopPlaying() 
{
  // First, call the parent class's implementation, to stop our fragmenter object
  // (and its source):
  MultiFramedRTPSink::stopPlaying();

  // Then, close our 'fragmenter' object:
  Medium::close(fOurFragmenter); fOurFragmenter = NULL;
  //fSource = NULL;
}

void H265VideoRTPSink::doSpecialFrameHandling(unsigned /*fragmentationOffset*/,
					      unsigned char* /*frameStart*/,
					      unsigned /*numBytesInFrame*/,
					      struct timeval frameTimestamp,
					      unsigned /*numRemainingBytes*/,
					      MediaFrameInfo stFrameInfo) 
{
    //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  
    // Set the RTP 'M' (marker) bit iff
    // 1/ The most recently delivered fragment was the end of
    //    (or the only fragment of) an NAL unit, and
    // 2/ This NAL unit was the last NAL unit of an 'access unit' (i.e. video frame).
    if (fOurFragmenter != NULL) {
      H265VideoStreamFramer* framerSource
        = (H265VideoStreamFramer*)(fOurFragmenter->inputSource());
      // This relies on our fragmenter's source being a "MPEG4VideoStreamFramer".
      if (fOurFragmenter->lastFragmentCompletedNALUnit()
  	&& framerSource != NULL && framerSource->currentNALUnitEndsAccessUnit()) {
        setMarkerBit();
        //printf("#@#@#@ 111 %s:%s\n", __FILE__, __FUNCTION__);
      }
    }
  
    setTimestamp(frameTimestamp);

#if 0       //todo 不再有h265 smartp模式
    MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pVideoEncCfg = &mediacfg->videoConfig.videoEncode;

    if ((pstVideoEncode->h265_smartp_main_enable == True) || (pstVideoEncode->h265_smartp_sub_enable == True))
    {
        setExsensionBit();
        unsigned FrameSpecificLen = RTP_EXSENSION_PROFILE_NUM | (0x0000FFFF & RTP_EXSENSION_CONTENT_WORD_NUM);
        setSpecialHeaderWord(FrameSpecificLen, 0);
        setSpecialHeaderWord(RTP_EXSENSION_TPS_FLAG, 1);      
        setSpecialHeaderWord(stFrameInfo.nFrameType, 2);
        setSpecialHeaderWord(stFrameInfo.ulFrameIndex, 3);
        setSpecialHeaderWord(stFrameInfo.ulLastIFrameIndex, 4); 
        setSpecialHeaderWord(stFrameInfo.nFrameRate, 5);
    }
#endif
}

Boolean H265VideoRTPSink
::frameCanAppearAfterPacketStart(unsigned char const* /*frameStart*/,
				 unsigned /*numBytesInFrame*/) const 
{
  return False;
}

char const* H265VideoRTPSink::auxSDPLine() 
{
  return fFmtpSDPLine;
}

////////// H265FUAFragmenter implementation //////////

H265FUAFragmenter::H265FUAFragmenter(UsageEnvironment& env,
                                     FramedSource* inputSource,
                                     unsigned inputBufferMax,
                                     unsigned maxOutputPacketSize)
  : FramedFilter(env, inputSource),
    fInputBufferSize(inputBufferMax+1), fMaxOutputPacketSize(maxOutputPacketSize),
    fNumValidDataBytes(1), fCurDataOffset(1), fSaveNumTruncatedBytes(0),
    fLastFragmentCompletedNALUnit(True)
{
    fInputBuffer = new unsigned char[fInputBufferSize];
}

H265FUAFragmenter::~H265FUAFragmenter() 
{  
  delete[] fInputBuffer;
  detachInputSource(); // so that the subsequent ~FramedFilter() doesn't delete it
}

void H265FUAFragmenter::doGetNextFrame() 
{
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);

  if (fNumValidDataBytes == 1) 
  {
    //printf("111 #@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
    // We have no NAL unit data currently in the buffer.  Read a new one:
    fInputSource->getNextFrame(&fInputBuffer[1], fInputBufferSize - 1,
			                   afterGettingFrame, this,
			                   FramedSource::handleClosure, this);
  } 
  else 
  {
    //printf("222 #@#@#@ %s:%s %d %d\n", __FILE__, __FUNCTION__, fMaxSize, fMaxOutputPacketSize);
    // We have NAL unit data in the buffer.  There are three cases to consider:
    // 1. There is a new NAL unit in the buffer, and it's small enough to deliver
    //    to the RTP sink (as is).
    // 2. There is a new NAL unit in the buffer, but it's too large to deliver to
    //    the RTP sink in its entirety.  Deliver the first fragment of this data,
    //    as a FU packet, with one extra preceding header byte (for the "FU header").
    // 3. There is a NAL unit in the buffer, and we've already delivered some
    //    fragment(s) of this.  Deliver the next fragment of this data,
    //    as a FU packet, with three (H.265) extra preceding header bytes
    //    (for the "NAL header" and the "FU header").

    if (fMaxSize < fMaxOutputPacketSize) 
    { // shouldn't happen
      envir() << "H265Fragmenter::doGetNextFrame(): fMaxSize ("
	      << fMaxSize << ") is smaller than expected\n";
    } 
    else 
    {
      fMaxSize = fMaxOutputPacketSize;
    }

    fLastFragmentCompletedNALUnit = True; // by default
    if (fCurDataOffset == 1) 
    { // case 1 or 2
      if (fNumValidDataBytes - 1 <= fMaxSize) 
      { // case 1
        memmove(fTo, &fInputBuffer[1], fNumValidDataBytes - 1);
        fFrameSize = fNumValidDataBytes - 1;
        fCurDataOffset = fNumValidDataBytes;
		if ((((fInputBuffer[1] &0x7E)>>1) == 32) 
			|| (((fInputBuffer[1] &0x7E)>>1) == 33) 
			|| (((fInputBuffer[1] &0x7E)>>1) == 34))
	    {
	        fLastFragmentCompletedNALUnit = False;
	    }
      } 
      else
      { // case 2
        // We need to send the NAL unit data as FU packets.  Deliver the first
        // packet now.  Note that we add "NAL header" and "FU header" bytes to the front
        // of the packet (overwriting the existing "NAL header").
        
        u_int8_t nal_unit_type = (fInputBuffer[1]&0x7E)>>1;
        fInputBuffer[0] = (fInputBuffer[1] & 0x81) | (49<<1); // Payload header (1st byte)
        fInputBuffer[1] = fInputBuffer[2];                    // Payload header (2nd byte)
        fInputBuffer[2] = 0x80 | nal_unit_type;               // FU header (with S bit)
        
        memmove(fTo, fInputBuffer, fMaxSize);
        fFrameSize = fMaxSize;
        fCurDataOffset += fMaxSize - 1;
        fLastFragmentCompletedNALUnit = False;
      }

    } 
    else
    { // case 3
      // We are sending this NAL unit data as FU packets.  We've already sent the
      // first packet (fragment).  Now, send the next fragment.  Note that we add
      // "NAL header" and "FU header" bytes to the front.  (We reuse these bytes that
      // we already sent for the first fragment, but clear the S bit, and add the E
      // bit if this is the last fragment.)
      unsigned numExtraHeaderBytes;

      fInputBuffer[fCurDataOffset-3] = fInputBuffer[0];       // Payload header (1st byte)
      fInputBuffer[fCurDataOffset-2] = fInputBuffer[1];       // Payload header (2nd byte)
      fInputBuffer[fCurDataOffset-1] = fInputBuffer[2]&~0x80; // FU header (no S bit)
      numExtraHeaderBytes = 3;

      unsigned numBytesToSend = numExtraHeaderBytes + (fNumValidDataBytes - fCurDataOffset);
      if (numBytesToSend > fMaxSize) 
      {
        // We can't send all of the remaining data this time:
        numBytesToSend = fMaxSize;
        fLastFragmentCompletedNALUnit = False;
      } 
      else 
      {
        // This is the last fragment:
        fInputBuffer[fCurDataOffset-1] |= 0x40; // set the E bit in the FU header
        fNumTruncatedBytes = fSaveNumTruncatedBytes;
      }
      memmove(fTo, &fInputBuffer[fCurDataOffset-numExtraHeaderBytes], numBytesToSend);
      fFrameSize = numBytesToSend;
      fCurDataOffset += numBytesToSend - numExtraHeaderBytes;
      
    }

    if (fCurDataOffset >= fNumValidDataBytes) 
    {
    	if ((((fInputBuffer[1] &0x7E)>>1) == 32) 
    		|| (((fInputBuffer[1] &0x7E)>>1) == 33) 
    		|| (((fInputBuffer[1] &0x7E)>>1) == 34))
        {
            fLastFragmentCompletedNALUnit = False;
        }    
      // We're done with this data.  Reset the pointers for receiving new data:
      fNumValidDataBytes = fCurDataOffset = 1;
    }

    // Complete delivery to the client:
    FramedSource::afterGetting(this);
  }
}


void H265FUAFragmenter::afterGettingFrame(void* clientData, unsigned frameSize,
					  unsigned numTruncatedBytes,
					  struct timeval presentationTime,
					  unsigned durationInMicroseconds,
					  MediaFrameInfo stFrameInfo)
{
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  
  H265FUAFragmenter* fragmenter = (H265FUAFragmenter*)clientData;
  fragmenter->afterGettingFrame1(frameSize, numTruncatedBytes, presentationTime,
				                 durationInMicroseconds, stFrameInfo);
}

void H265FUAFragmenter::afterGettingFrame1(unsigned frameSize,
					                       unsigned numTruncatedBytes,
					                       struct timeval presentationTime,
					                       unsigned durationInMicroseconds,
					                       MediaFrameInfo stFrameInfo) 
{
  //printf("#@#@#@ %s:%s %d\n", __FILE__, __FUNCTION__, frameSize);

  fNumValidDataBytes += frameSize;
  fSaveNumTruncatedBytes = numTruncatedBytes;
  fPresentationTime = presentationTime;
  fDurationInMicroseconds = durationInMicroseconds;
  fstFrameInfo = stFrameInfo;

  // Deliver data to the client:
  doGetNextFrame();
}

#endif

