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
// of MPEG-4 video from a WIS GO7007 capture device.
// Implementation
#include "rtsp_func_macro.hh"
#include "anj_mw_log.h"
#include "anj_mbuf.h"
#include "sdk_option.h"

#ifdef _H264_FUNC_
#include "Base64.hh"

#include "WISH264VideoServerMediaSubsession.hh"
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamFramer.hh>

WISH264VideoServerMediaSubsession* WISH264VideoServerMediaSubsession
::createNew(UsageEnvironment& env, MediaStreamInput& mpeg4Input, unsigned estimatedBitrate,int width,int height,int framerate) {
  return new WISH264VideoServerMediaSubsession(env, mpeg4Input, estimatedBitrate,width,height,framerate);
}

WISH264VideoServerMediaSubsession
::WISH264VideoServerMediaSubsession(UsageEnvironment& env, MediaStreamInput& mpeg4Input,
				     unsigned estimatedBitrate,int width,int height,int framerate)
  : WISServerMediaSubsession(env, mpeg4Input, estimatedBitrate) {
	  
	fwidth=width;
	fheight=height;
	fframerate=framerate;
}

WISH264VideoServerMediaSubsession::~WISH264VideoServerMediaSubsession() {
}

#if 0
static void afterPlayingDummy(void* clientData) {
  WISH264VideoServerMediaSubsession* subsess
    = (WISH264VideoServerMediaSubsession*)clientData;
  // Signal the event loop that we're done:
  subsess->setDoneFlag();
}
#endif

static void checkForAuxSDPLine(void* clientData) {
  WISH264VideoServerMediaSubsession* subsess
    = (WISH264VideoServerMediaSubsession*)clientData;
  subsess->checkForAuxSDPLine1();
}

void WISH264VideoServerMediaSubsession::checkForAuxSDPLine1() {
  if (fDummyRTPSink->auxSDPLine() != NULL) {
    // Signal the event loop that we're done:
    setDoneFlag();
  } else {
    // try again after a brief delay:
    int uSecsToDelay = 100000; // 100 ms
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
			      (TaskFunc*)checkForAuxSDPLine, this);
  }
}

void print_hex_len(char *str, int len)
{
	char buf[1024];
	int  offset=0;
	while(len--)
	{
		sprintf(buf + offset, "%02x", *str++);
		offset+=2;
	}	
	// __DBG(buf);
}

int get_pps_sps(char *buf, int len, char *pps, int *pps_len, char *sps, int *sps_len)
{
	unsigned char flag[5];
	int offset = 0;	
	
	while(offset < (len - 5))
	{
		memcpy(flag, buf + offset, 5);
		
		if(flag[0]== 0 && flag[1]==0 && flag[2] == 0 && flag[3] == 1)
		{
			if((flag[4] & 0x1F) == 0x08) //nal_type == PPS, johnnyling 20100628
			{
				memcpy(sps, buf + 4, offset - 4);
				*sps_len = offset - 4;
				
				//printf("len = %03d, sps:", *sps_len);
				//print_hex_len(sps, *sps_len);
				//printf("\n");
				
				memcpy(pps, buf + offset + 4, len - offset - 4);
				*pps_len = len - offset - 4;

				//printf("len = %03d, pps:", *pps_len);
				//print_hex_len(pps, *pps_len);
				//printf("\n");
				
				return 1;
			}			
		}
		
		offset++;
	}
	
	return 0;
}

char const* WISH264VideoServerMediaSubsession
::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) 
{
	char buf[1024];
	char tmp[512];
	
	char sps[256];
	char pps[256];
	int  spslen, ppslen;
	unsigned int profile_level_id;
	
	if(get_pps_sps((char *)fWISInput.fvideo_config, fWISInput.fvideo_config_len,
			pps, &ppslen, sps, &spslen))
	{
		profile_level_id = (sps[1]<<16)|(sps[2]<<8)|sps[3];	
		
		char* sps_base64 = base64Encode((char*)sps, spslen);
  		char* pps_base64 = base64Encode((char*)pps, ppslen);
  	
		sprintf(buf,"a=fmtp:96 packetization-mode=1;profile-level-id=%06X;sprop-parameter-sets=%s,%s",
					profile_level_id,
					sps_base64,
					pps_base64);
		delete sps_base64;
		delete pps_base64;
	}
	else
	{
		strcpy(buf,"a=fmtp:96 packetization-mode=1;profile-level-id=1;sprop-parameter-sets=");
		
		char *base64_buf = base64Encode((const char *)fWISInput.fvideo_config, fWISInput.fvideo_config_len); 
		strcat(buf, base64_buf);
		delete base64_buf;
	}

#if 1
	strcat(buf, ";config=");	
	for(int i=0;i<fWISInput.fvideo_config_len;i++)
	{
		sprintf(tmp,"%02x",fWISInput.fvideo_config[i]);
		strcat(buf,tmp);	
	}
	strcat(buf,"\r\n");
#else
	strcat(buf, ";\r\n");
#endif

	sprintf(tmp,"a=x-dimensions: %d, %d\r\na=x-framerate: %d\r\n",
		fwidth,fheight,fframerate);
		
	strcat(buf,tmp);
	
	//printf("auxInfo=%s\n", buf);

	const char *auxInfo=strdup(buf);
	return auxInfo;	
}

void WISH264VideoServerMediaSubsession::modifyMediaPara(int iWidth, int iHeight, 
                                                        int iFrameRate, int iBitRate)
{    
    WISServerModifySubsessionBitRate(iBitRate);    
    
    if ((fwidth != iWidth)
        ||(fheight != iHeight)
        ||(fframerate != iFrameRate))
    {
        memset(fWISInput.fvideo_config, 0, sizeof(fWISInput.fvideo_config));
        fWISInput.fvideo_config_len = 0;
        fwidth = iWidth;
        fheight = iHeight;
        fframerate = iFrameRate;
    }
    
    return;
}

FramedSource* WISH264VideoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  estBitrate = fEstimatedKbps;

  // Create a framer for the Video Elementary Stream:
  return H264VideoStreamFramer::createNew(envir(), fWISInput.videoSource());
}

RTPSink* WISH264VideoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {
  setVideoRTPSinkBufferSize();
  //return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, 96, 0x42, "h264");
}

#endif

