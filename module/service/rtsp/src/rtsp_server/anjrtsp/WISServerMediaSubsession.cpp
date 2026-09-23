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
// of audio or video from a WIS GO7007 capture device.
// Implementation

#include "anj_mbuf.h"
#include "anj_config.h"

#include "WISServerMediaSubsession.hh"

WISServerMediaSubsession
::WISServerMediaSubsession(UsageEnvironment& env, MediaStreamInput& Input, unsigned estimatedBitrate)
  : OnDemandServerMediaSubsession(env, True /*reuse the first source*/),
    fWISInput(Input) {

    MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
    estimatedBitrate = mediacfg->audioConfig.audioCapture.samplerate * mediacfg->audioConfig.audioCapture.channels;
  fEstimatedKbps = (estimatedBitrate + 500)/1000;
}


WISServerMediaSubsession::~WISServerMediaSubsession() {
}

void WISServerMediaSubsession
::WISServerModifySubsessionBitRate(int iBitrate)
{
    fEstimatedKbps = (iBitrate + 500)/1000;
}
