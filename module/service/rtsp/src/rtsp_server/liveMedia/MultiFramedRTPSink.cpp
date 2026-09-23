/**********
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
// Copyright (c) 1996-2011 Live Networks, Inc.  All rights reserved.
// RTP sink for a common kind of payload format: Those which pack multiple,
// complete codec frames (as many as possible) into each RTP packet.
// Implementation
#include "anj_mw_log.h"
#include "anj_config.h"

#include "MultiFramedRTPSink.hh"
#include "GroupsockHelper.hh"

static unsigned const rtpHeaderSize = 12;


void MultiFramedRTPSink::setPacketSizes(unsigned preferredPacketSize,
					unsigned maxPacketSize) {
  if (preferredPacketSize > maxPacketSize || preferredPacketSize == 0) return;

  delete fOutBuf;
  fOutBuf = new OutPacketBuffer(preferredPacketSize, maxPacketSize);
  fOurMaxPacketSize = maxPacketSize; // save value, in case subclasses need it
}

MultiFramedRTPSink::MultiFramedRTPSink(UsageEnvironment& env,
				       Groupsock* rtpGS,
				       unsigned char rtpPayloadType,
				       unsigned rtpTimestampFrequency,
				       char const* rtpPayloadFormatName,
				       unsigned numChannels)
  : RTPSink(env, rtpGS, rtpPayloadType, rtpTimestampFrequency,
	    rtpPayloadFormatName, numChannels),
  fOutBuf(NULL), fCurFragmentationOffset(0), fPreviousFrameEndedFragmentation(False){

  if((strcmp(rtpPayloadFormatName,"PCMU") == 0) ||
     (strcmp(rtpPayloadFormatName,"AAC-hbr") == 0))
  {
      // Audio
      setPacketSizes(1400, 1440);
  }
  else
  {
      // Video
      //setPacketSizes(4096, 8192); //for hfgd      
      //setPacketSizes(1400, 1448);
#if 0
      //todo
      MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
      setPacketSizes(1400, pstMediaStreamConfig->streamAccess.RTPPacketSize); //use rtpsize.small and rtpsize.big to control, default 4096, ONVIF NVR/CLIENT
#else
    setPacketSizes(1400, 8192);
#endif
  }
  // Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
  // (Also, make it a multiple of 4 bytes, just in case that matters.)
}

MultiFramedRTPSink::~MultiFramedRTPSink() {
  delete fOutBuf;
}


void MultiFramedRTPSink
::doSpecialFrameHandling(unsigned /*fragmentationOffset*/,
			 unsigned char* /*frameStart*/,
			 unsigned /*numBytesInFrame*/,
			 struct timeval frameTimestamp,
			 unsigned /*numRemainingBytes*/,
			 MediaFrameInfo stFrameInfo) {
  //__DBG("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);			 
  // default implementation: If this is the first frame in the packet,
  // use its timestamp for the RTP timestamp:
  
    if (isFirstFrameInPacket()) {
      setTimestamp(frameTimestamp);      
    }

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

Boolean MultiFramedRTPSink::allowFragmentationAfterStart() const {
  return False; // by default
}

Boolean MultiFramedRTPSink::allowOtherFramesAfterLastFragment() const {
  return False; // by default
}

Boolean MultiFramedRTPSink
::frameCanAppearAfterPacketStart(unsigned char const* /*frameStart*/,
				 unsigned /*numBytesInFrame*/) const {
  return True; // by default
}

unsigned MultiFramedRTPSink::specialHeaderSize() const {
  // default implementation: Assume no special header:
  // 每个RTP分包的包头 @20170622

#if 0       //todo  不再有h265 smartp模式
    MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pVideoEncCfg = &mediacfg->videoConfig.videoEncode;

    if ((pstVideoEncode->h265_smartp_main_enable == True) || (pstVideoEncode->h265_smartp_sub_enable == True))
    {
        return RTP_EXSENSION_HEADER_LEN + (4 * RTP_EXSENSION_CONTENT_WORD_NUM);
    }
    else
    {
        return 0;
    }
#else
    return 0;
#endif
}

unsigned MultiFramedRTPSink::frameSpecificHeaderSize() const {
  return 0;
}

unsigned MultiFramedRTPSink::computeOverflowForNewFrame(unsigned newFrameSize) const {
  // default implementation: Just call numOverflowBytes()
  return fOutBuf->numOverflowBytes(newFrameSize);
}

void MultiFramedRTPSink::setMarkerBit() {
  unsigned rtpHdr = fOutBuf->extractWord(0);
  rtpHdr |= 0x00800000;
  fOutBuf->insertWord(rtpHdr, 0);
}

void MultiFramedRTPSink::setExsensionBit() {
  unsigned rtpHdr = fOutBuf->extractWord(0);
  rtpHdr |= 0x10000000;
  fOutBuf->insertWord(rtpHdr, 0);
}

void MultiFramedRTPSink::setTimestamp(struct timeval timestamp) {
  // First, convert the timestamp to a 32-bit RTP timestamp:
  fCurrentTimestamp = convertToRTPTimestamp(timestamp);

  // Then, insert it into the RTP packet:
  fOutBuf->insertWord(fCurrentTimestamp, fTimestampPosition);
}

void MultiFramedRTPSink::setSpecialHeaderWord(unsigned word,
					      unsigned wordPosition) {
  fOutBuf->insertWord(word, fSpecialHeaderPosition + 4*wordPosition);
}

void MultiFramedRTPSink::setSpecialHeaderBytes(unsigned char const* bytes,
					       unsigned numBytes,
					       unsigned bytePosition) {
  fOutBuf->insert(bytes, numBytes, fSpecialHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderWord(unsigned word,
						    unsigned wordPosition) {
  fOutBuf->insertWord(word, fCurFrameSpecificHeaderPosition + 4*wordPosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderBytes(unsigned char const* bytes,
						     unsigned numBytes,
						     unsigned bytePosition) {
  fOutBuf->insert(bytes, numBytes, fCurFrameSpecificHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFramePadding(unsigned numPaddingBytes) {
  if (numPaddingBytes > 0) {
    // Add the padding bytes (with the last one being the padding size):
    unsigned char paddingBuffer[255]; //max padding
    memset(paddingBuffer, 0, numPaddingBytes);
    paddingBuffer[numPaddingBytes-1] = numPaddingBytes;
    fOutBuf->enqueue(paddingBuffer, numPaddingBytes);

    // Set the RTP padding bit:
    unsigned rtpHdr = fOutBuf->extractWord(0);
    rtpHdr |= 0x20000000;
    fOutBuf->insertWord(rtpHdr, 0);
  }
}

Boolean MultiFramedRTPSink::continuePlaying() {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  // Send the first packet.
  // (This will also schedule any future sends.)
  buildAndSendPacket(True);
  return True;
}

void MultiFramedRTPSink::stopPlaying() {
  fOutBuf->resetPacketStart();
  fOutBuf->resetOffset();
  fOutBuf->resetOverflowData();

  // Then call the default "stopPlaying()" function:
  MediaSink::stopPlaying();
}

void MultiFramedRTPSink::buildAndSendPacket(Boolean isFirstPacket) {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);

  fIsFirstPacket = isFirstPacket;

  // Set up the RTP header:
  unsigned rtpHdr = 0x80000000; // RTP version 2
  rtpHdr |= (fRTPPayloadType<<16);
  rtpHdr |= fSeqNo; // sequence number
  fOutBuf->enqueueWord(rtpHdr);

  // Note where the RTP timestamp will go.
  // (We can't fill this in until we start packing payload frames.)
  fTimestampPosition = fOutBuf->curPacketSize();
  fOutBuf->skipBytes(4); // leave a hole for the timestamp

  fOutBuf->enqueueWord(SSRC());

  // Allow for a special, payload-format-specific header following the
  // RTP header:
  fSpecialHeaderPosition = fOutBuf->curPacketSize();
  fSpecialHeaderSize = specialHeaderSize();
  fOutBuf->skipBytes(fSpecialHeaderSize);

  // Begin packing as many (complete) frames into the packet as we can:
  fTotalFrameSpecificHeaderSizes = 0;
  fNoFramesLeft = False;
  fNumFramesUsedSoFar = 0;
  packFrame();
}

void MultiFramedRTPSink::packFrame() {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  // Get the next packet.

  // First, see if we have an overflow frame that was too big for the last pkt
  if (fOutBuf->haveOverflowData()) {
    //printf("#@#@#@ 111 %s:%s\n", __FILE__, __FUNCTION__);
    // Use this frame before reading a new one from the source
    // 分包的比原来分包大的情况下没发完需要继续发的数据
    unsigned frameSize = fOutBuf->overflowDataSize();
    struct timeval presentationTime = fOutBuf->overflowPresentationTime();
    unsigned durationInMicroseconds = fOutBuf->overflowDurationInMicroseconds();
    MediaFrameInfo stFrameInfo = fOutBuf->overflowFrameInfo();
    fOutBuf->useOverflowData();

    afterGettingFrame1(frameSize, 0, presentationTime, durationInMicroseconds, stFrameInfo);
  } else {
    //printf("#@#@#@ 222 %s:%s\n", __FILE__, __FUNCTION__);
    // Normal case: we need to read a new frame from the source
    if (fSource == NULL) return;

    fCurFrameSpecificHeaderPosition = fOutBuf->curPacketSize();
    fCurFrameSpecificHeaderSize = frameSpecificHeaderSize();
    fOutBuf->skipBytes(fCurFrameSpecificHeaderSize);
    fTotalFrameSpecificHeaderSizes += fCurFrameSpecificHeaderSize;
        
    //FramedSource.cpp:FramedSource::getNextFrame
    fSource->getNextFrame(fOutBuf->curPtr(), fOutBuf->totalBytesAvailable(),
			  afterGettingFrame, this, ourHandleClosure, this);
  }
}

void MultiFramedRTPSink
::afterGettingFrame(void* clientData, unsigned numBytesRead,
		    unsigned numTruncatedBytes,
		    struct timeval presentationTime,
		    unsigned durationInMicroseconds,
		    MediaFrameInfo stFrameInfo) {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
  sink->afterGettingFrame1(numBytesRead, numTruncatedBytes,
			   presentationTime, durationInMicroseconds,
			   stFrameInfo);
}

void MultiFramedRTPSink
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
		     struct timeval presentationTime,
		     unsigned durationInMicroseconds,
		     MediaFrameInfo stFrameInfo) {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);
  if (fIsFirstPacket) {
    // Record the fact that we're starting to play now:
    SystemGetTimeofRun(&fNextSendTime, NULL);
  }


  if (numTruncatedBytes > 0) {
    unsigned const bufferSize = fOutBuf->totalBytesAvailable();
    envir() << "MultiFramedRTPSink::afterGettingFrame1(): The input frame data was too large for our buffer size ("
	    << bufferSize << ").  "
	    << numTruncatedBytes << " bytes of trailing data was dropped!  Correct this by increasing \"OutPacketBuffer::maxSize\" to at least "
	    << OutPacketBuffer::maxSize + numTruncatedBytes << ", *before* creating this 'RTPSink'.  (Current value is "
	    << OutPacketBuffer::maxSize << ".)\n";
  }
  unsigned curFragmentationOffset = fCurFragmentationOffset;
  unsigned numFrameBytesToUse = frameSize;
  unsigned overflowBytes = 0;

  // If we have already packed one or more frames into this packet,
  // check whether this new frame is eligible to be packed after them.
  // (This is independent of whether the packet has enough room for this
  // new frame; that check comes later.)
  if (fNumFramesUsedSoFar > 0) {
    if ((fPreviousFrameEndedFragmentation
	 && !allowOtherFramesAfterLastFragment())
	|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr(), frameSize)) {
      // Save away this frame for next time:
      numFrameBytesToUse = 0;
      fOutBuf->setOverflowData(fOutBuf->curPacketSize(), frameSize,
			       presentationTime, durationInMicroseconds, stFrameInfo);
    }
  }
  
  fPreviousFrameEndedFragmentation = False;

  if (numFrameBytesToUse > 0) {
    // Check whether this frame overflows the packet
    if (fOutBuf->wouldOverflow(frameSize)) {
      // Don't use this frame now; instead, save it as overflow data, and
      // send it in the next packet instead.  However, if the frame is too
      // big to fit in a packet by itself, then we need to fragment it (and
      // use some of it in this packet, if the payload format permits this.)
      if (isTooBigForAPacket(frameSize)
          && (fNumFramesUsedSoFar == 0 || allowFragmentationAfterStart())) {
        // We need to fragment this frame, and use some of it now:
        overflowBytes = computeOverflowForNewFrame(frameSize);
        numFrameBytesToUse -= overflowBytes;
        fCurFragmentationOffset += numFrameBytesToUse;
      } else {
        // We don't use any of this frame now:
        overflowBytes = frameSize;
        numFrameBytesToUse = 0;
      }
      fOutBuf->setOverflowData(fOutBuf->curPacketSize() + numFrameBytesToUse,
			       overflowBytes, presentationTime, durationInMicroseconds, stFrameInfo);
    } else if (fCurFragmentationOffset > 0) {
      // This is the last fragment of a frame that was fragmented over
      // more than one packet.  Do any special handling for this case:
      fCurFragmentationOffset = 0;
      fPreviousFrameEndedFragmentation = True;
    }
  }

  if (numFrameBytesToUse == 0) {
    // Send our packet now, because we have filled it up:
    sendPacketIfNecessary();
  } else {
    // Use this frame in our outgoing packet:
    unsigned char* frameStart = fOutBuf->curPtr();
    fOutBuf->increment(numFrameBytesToUse);
        // do this now, in case "doSpecialFrameHandling()" calls "setFramePadding()" to append padding bytes

    // Here's where any payload format specific processing gets done:
    doSpecialFrameHandling(curFragmentationOffset, frameStart,
			   numFrameBytesToUse, presentationTime,
			   overflowBytes, stFrameInfo);

    ++fNumFramesUsedSoFar;

    // Update the time at which the next packet should be sent, based
    // on the duration of the frame that we just packed into it.
    // However, if this frame has overflow data remaining, then don't
    // count its duration yet.
    if (overflowBytes == 0) {
      fNextSendTime.tv_usec += durationInMicroseconds;
      fNextSendTime.tv_sec += fNextSendTime.tv_usec/1000000;
      fNextSendTime.tv_usec %= 1000000;
    }

    // Send our packet now if (i) it's already at our preferred size, or
    // (ii) (heuristic) another frame of the same size as the one we just
    //      read would overflow the packet, or
    // (iii) it contains the last fragment of a fragmented frame, and we
    //      don't allow anything else to follow this or
    // (iv) one frame per packet is allowed:
    if (fOutBuf->isPreferredSize()
        || fOutBuf->wouldOverflow(numFrameBytesToUse)
        || (fPreviousFrameEndedFragmentation &&
            !allowOtherFramesAfterLastFragment())
        || !frameCanAppearAfterPacketStart(fOutBuf->curPtr() - frameSize,
					   frameSize) ) {
      // The packet is ready to be sent now
      sendPacketIfNecessary();
    } else {
      // There's room for more frames; try getting another:
      packFrame();
    }
  }
}

 unsigned MultiFramedRTPSink::RtpFrameHeaderLen()  {
   unsigned RtpHeaderLen = rtpHeaderSize + specialHeaderSize() + frameSpecificHeaderSize();
   return RtpHeaderLen;
 }

Boolean MultiFramedRTPSink::isTooBigForAPacket(unsigned numBytes) const {
  // Check whether a 'numBytes'-byte frame - together with a RTP header and
  // (possible) special headers - would be too big for an output packet:
  // (Later allow for RTP extension header!) #####
  numBytes += rtpHeaderSize + specialHeaderSize() + frameSpecificHeaderSize();
  return fOutBuf->isTooBigForAPacket(numBytes);
}

void MultiFramedRTPSink::sendPacketIfNecessary() {
    //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);

  if (fNumFramesUsedSoFar > 0) {
    // Send the packet:
#ifdef TEST_LOSS
    if ((our_random()%10) != 0) // simulate 10% packet loss #####
#endif
    fRTPInterface.sendPacket(fOutBuf->packet(), fOutBuf->curPacketSize());
    ++fPacketCount;
    fTotalOctetCount += fOutBuf->curPacketSize();
    fOctetCount += fOutBuf->curPacketSize()
      - rtpHeaderSize - fSpecialHeaderSize - fTotalFrameSpecificHeaderSizes;

    ++fSeqNo; // for next time
  }

  if (fOutBuf->haveOverflowData()
      && fOutBuf->totalBytesAvailable() > fOutBuf->totalBufferSize()/2) {
    // Efficiency hack: Reset the packet start pointer to just in front of
    // the overflow data (allowing for the RTP header and special headers),
    // so that we probably don't have to "memmove()" the overflow data
    // into place when building the next packet:
    unsigned newPacketStart = fOutBuf->curPacketSize()
      - (rtpHeaderSize + fSpecialHeaderSize + frameSpecificHeaderSize());
    fOutBuf->adjustPacketStart(newPacketStart);
  } else {
    // Normal case: Reset the packet start pointer back to the start:
    fOutBuf->resetPacketStart();
  }
  
  fOutBuf->resetOffset();
  fNumFramesUsedSoFar = 0;

  if (fNoFramesLeft) {
    // We're done:
    onSourceClosure(this);
  } else {
    // We have more frames left to send.  Figure out when the next frame
    // is due to start playing, then make sure that we wait this long before
    // sending the next packet.
    // Delay this amount of time:
    //nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecondsToGo, (TaskFunc*)sendNext, this);
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)sendNext, this);
  }
}

// The following is called after each delay between packet sends:
void MultiFramedRTPSink::sendNext(void* firstArg) {
  //printf("#@#@#@ %s:%s\n", __FILE__, __FUNCTION__);

  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)firstArg;
  sink->buildAndSendPacket(False);
}

void MultiFramedRTPSink::ourHandleClosure(void* clientData) {
  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
  // There are no frames left, but we may have a partially built packet
  //  to send
  sink->fNoFramesLeft = True;
  sink->sendPacketIfNecessary();
}
