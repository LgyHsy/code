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
// A "ServerMediaSubsession" subclass for on-demand unicast streaming
// of PCM audio from a WIS GO7007 capture device.
// Implementation

#include "rtsp_func_macro.hh"
#include "anj_mbuf.h"
#include "sdk_option.h"
#include "anj_config.h"

#ifdef _AUDIO_FUNC_

#include "WISPCMAudioServerMediaSubsession.hh"
//#include "Options.hh"
//#include "AudioRTPCommon.hh"
#include <liveMedia.hh>


WISPCMAudioServerMediaSubsession* WISPCMAudioServerMediaSubsession
::createNew(UsageEnvironment& env, MediaStreamInput& Input) {
  return new WISPCMAudioServerMediaSubsession(env, Input);
}

WISPCMAudioServerMediaSubsession
::WISPCMAudioServerMediaSubsession(UsageEnvironment& env, MediaStreamInput& Input)
  : WISServerMediaSubsession(env, Input,
			     8000*8*1) {
}

WISPCMAudioServerMediaSubsession::~WISPCMAudioServerMediaSubsession() {
}

FramedSource* WISPCMAudioServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  estBitrate = fEstimatedKbps;
  return fWISInput.audioSource();
}


RTPSink* WISPCMAudioServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {
  setVideoRTPSinkBufferSize();

  MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
  AudioConfig *audiocfg = &mediacfg->audioConfig;

  return SimpleRTPSink::createNew(envir(), rtpGroupsock, 0,
					              audiocfg->audioCapture.samplerate, "audio", "PCMU",
					              audiocfg->audioCapture.channels, False, True); //johnnyling 20121219
}

char const* WISPCMAudioServerMediaSubsession
::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) 
{
    char buf[1024];

    memset(buf, 0, sizeof(buf));
	sprintf(buf,"a=framerate: %d\r\n", 25);

	const char *auxInfo=strdup(buf);
	return auxInfo;	    
}


#endif

