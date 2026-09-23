/*
 * Copyright (C) 1998-2016 Topsee Electronic Tech Co.,Ltd.
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

#ifdef _H265_FUNC_
#include "Base64.hh"

#include "WISH265VideoServerMediaSubsession.hh"
#include <H265VideoRTPSink.hh>
#include <H265VideoStreamFramer.hh>

#define  H265_RTP_PAYLOAD_TYPE  96

WISH265VideoServerMediaSubsession* WISH265VideoServerMediaSubsession
::createNew(UsageEnvironment& env, MediaStreamInput& mpeg4Input,
            unsigned estimatedBitrate, int width, int height, int framerate)
{
  return new WISH265VideoServerMediaSubsession(env, mpeg4Input, estimatedBitrate,width,height,framerate);
}

WISH265VideoServerMediaSubsession
::WISH265VideoServerMediaSubsession(UsageEnvironment& env, MediaStreamInput& mpeg4Input,
				                    unsigned estimatedBitrate,int width,int height,int framerate)
  : WISServerMediaSubsession(env, mpeg4Input, estimatedBitrate) 
{
	  
	fwidth=width;
	fheight=height;
	fframerate=framerate;
}

WISH265VideoServerMediaSubsession::~WISH265VideoServerMediaSubsession()
{
}

static void checkForAuxSDPLine(void* clientData) 
{
  WISH265VideoServerMediaSubsession* subsess
    = (WISH265VideoServerMediaSubsession*)clientData;
  subsess->checkForAuxSDPLine1();
}

void WISH265VideoServerMediaSubsession::checkForAuxSDPLine1()
{
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

void h265_print_hex_len(char *str, int len)
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

//buf vps+sps+pps+vcl
//Frame format: 00 00 00 01 40 01 AA AA AA AA AA AA 00 00 00 01 42 01 BB BB BB BB BB
//              00 00 00 01 44 01 CC CC CC CC CC CC
int get_vps_pps_sps(char *buf, int len, 
                    char *vps, unsigned *vps_len, 
                    char *pps, unsigned *pps_len, 
                    char *sps, unsigned *sps_len)
{
	unsigned char flag[6];
	int offset = 0;	
    int spsoffset = 0;    
    u_int8_t nal_unit_type = 0xff;
    
	while(offset < (len - 6))
	{
		memcpy(flag, buf + offset, 6);

		if(flag[0]== 0 && flag[1]==0 && flag[2] == 0 && flag[3] == 1)
		{
            nal_unit_type = ((flag[4]&0x7E)>>1);
            if(nal_unit_type == 33) //nal_type == SPS, johnnyling 20100628
            {
				/*
				memcpy(vps, buf + 4, offset - 4);
				*vps_len = offset - 4;  
                spsoffset = offset + 4;
                */
				memcpy(vps, buf + 0, offset - 0);
				*vps_len = offset - 0;  
                spsoffset = offset + 0;
            }
			else if(nal_unit_type == 34) //nal_type == PPS, johnnyling 20100628
			{
				/*
				memcpy(sps, buf + spsoffset, offset - spsoffset);
				*sps_len = offset - spsoffset;

				memcpy(pps, buf + offset + 4, len - offset - 4);
				*pps_len = len - offset - 4;

				return 1;
				*/
				memcpy(sps, buf + spsoffset, offset - spsoffset);
				*sps_len = offset - spsoffset;

				memcpy(pps, buf + offset + 0, len - offset - 0);
				*pps_len = len - offset - 0;

				return 1;
			}
		}

		offset++;
	}
	
	return 0;
}

unsigned removeH265EmulationBytes(u_int8_t* to, unsigned toMaxSize,
                                  u_int8_t* from, unsigned fromSize) 
{
  unsigned toSize = 0;
  unsigned i = 0;
  while (i < fromSize && toSize+1 < toMaxSize) {
    if (i+2 < fromSize && from[i] == 0 && from[i+1] == 0 && from[i+2] == 3) {
      to[toSize] = to[toSize+1] = 0;
      toSize += 2;
      i += 3;
    } else {
      to[toSize] = from[i];
      toSize += 1;
      i += 1;
    }
  }

  return toSize;
}


char const* WISH265VideoServerMediaSubsession
::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) 
{
	char buf[1024];
	char tmp[512];

	char vps[256];    
	char sps[256];
	char pps[256];
	unsigned vpslen, spslen, ppslen;

    memset(buf, 0, sizeof(buf));
    memset(vps, 0, sizeof(vps));
    memset(sps, 0, sizeof(sps));
    memset(pps, 0, sizeof(pps));
	if(get_vps_pps_sps((char *)fWISInput.fvideo_config, fWISInput.fvideo_config_len,
			           vps, &vpslen, pps, &ppslen, sps, &spslen))
	{
        // Set up the "a=fmtp:" SDP line for this stream.
        u_int8_t* vpsWEB = new u_int8_t[vpslen]; // "WEB" means "Without Emulation Bytes"
        unsigned vpsWEBSize = removeH265EmulationBytes(vpsWEB, vpslen, (u_int8_t*)vps, vpslen);
        if (vpsWEBSize < 6/*'profile_tier_level' offset*/ + 12/*num 'profile_tier_level' bytes*/) 
        {
          // Bad VPS size => assume our source isn't ready
          delete[] vpsWEB;
          return NULL;
        }
        
        u_int8_t const* profileTierLevelHeaderBytes = &vpsWEB[6];
        unsigned profileSpace  = profileTierLevelHeaderBytes[0]>>6;  // general_profile_space
        unsigned profileId = profileTierLevelHeaderBytes[0]&0x1F;    // general_profile_idc
        unsigned tierFlag = (profileTierLevelHeaderBytes[0]>>5)&0x1; // general_tier_flag
        unsigned levelId = profileTierLevelHeaderBytes[11];          // general_level_idc
        u_int8_t const* interop_constraints = &profileTierLevelHeaderBytes[5];
        char interopConstraintsStr[100];
        sprintf(interopConstraintsStr, "%02X%02X%02X%02X%02X%02X", 
      	        interop_constraints[0], interop_constraints[1], interop_constraints[2],
      	        interop_constraints[3], interop_constraints[4], interop_constraints[5]);
        delete[] vpsWEB;
      
        char* sprop_vps = base64Encode((char*)vps, vpslen);
        char* sprop_sps = base64Encode((char*)sps, spslen);
        char* sprop_pps = base64Encode((char*)pps, ppslen);
      
        char const* fmtpFmt =
          "a=fmtp:%d profile-space=%u"
          ";profile-id=%u"
          ";tier-flag=%u"
          ";level-id=%u"
          ";interop-constraints=%s"
          ";sprop-vps=%s"
          ";sprop-sps=%s"
          ";sprop-pps=%s";
        
//        unsigned fmtpFmtSize = strlen(fmtpFmt)
//          + 3 /* max num chars: rtpPayloadType */ 
//          + 20 /* max num chars: profile_space */
//          + 20 /* max num chars: profile_id */
//          + 20 /* max num chars: tier_flag */
//          + 20 /* max num chars: level_id */
//          + strlen(interopConstraintsStr)
//          + strlen(sprop_vps)
//          + strlen(sprop_sps)
//          + strlen(sprop_pps);

        sprintf(buf, fmtpFmt,
                H265_RTP_PAYLOAD_TYPE, profileSpace,
        	    profileId,
        	    tierFlag,
        	    levelId,
        	    interopConstraintsStr,
        	    sprop_vps,
        	    sprop_sps,
        	    sprop_pps);   
        
        delete[] sprop_vps;
        delete[] sprop_sps;
        delete[] sprop_pps;
	}
	else
	{
        memset(tmp, 0, sizeof(tmp));
        sprintf(tmp, "a=fmtp:%d packetization-mode=1;profile-level-id=1;sprop-parameter-sets=",
                H265_RTP_PAYLOAD_TYPE);
		strcpy(buf, tmp);
		
		char *base64_buf = base64Encode((const char *)fWISInput.fvideo_config, fWISInput.fvideo_config_len); 
		strcat(buf, base64_buf);
		delete base64_buf;
	}

#if 1
	memset(tmp, 0, sizeof(tmp));
	strcat(buf, ";config=");	
	for(int i=0;i<fWISInput.fvideo_config_len;i++)
	{
		sprintf(tmp,"%02x",fWISInput.fvideo_config[i]);
		strcat(buf,tmp);	
	}
#endif	
	strcat(buf,"\r\n");

    memset(tmp, 0, sizeof(tmp));
	sprintf(tmp,"a=x-dimensions: %d, %d\r\na=x-framerate: %d\r\n",
		    fwidth,fheight,fframerate);
	strcat(buf,tmp);
    
	//printf("auxInfo=%s\n", buf);

	const char *auxInfo=strdup(buf);
	return auxInfo;	
}

void WISH265VideoServerMediaSubsession::modifyMediaPara(int iWidth, int iHeight, 
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

FramedSource* WISH265VideoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) 
{
  estBitrate = fEstimatedKbps;

  // Create a framer for the Video Elementary Stream:
  return H265VideoStreamFramer::createNew(envir(), fWISInput.videoSource());
}

RTPSink* WISH265VideoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		           unsigned char rtpPayloadTypeIfDynamic,
		           FramedSource* /*inputSource*/)
{
  setVideoRTPSinkBufferSize();
  return H265VideoRTPSink::createNew(envir(), rtpGroupsock, H265_RTP_PAYLOAD_TYPE, 0x42, "h265");
}

#endif

