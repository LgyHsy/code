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
// Any source that feeds into a "H265VideoRTPSink" must be of this class.
// This is a virtual base class; subclasses must implement the
// "currentNALUnitEndsAccessUnit()" virtual function.
// Implementation
#include "rtsp_func_macro.hh"
#include "anj_mw_time.h"

#ifdef _H265_FUNC_
#include "H265VideoStreamFramer.hh"
#include "H265VideoStreamParser.hh"

#include <string.h>
#include <GroupsockHelper.hh>

//int check = 0;
///////////////////////////////////////////////////////////////////////////////
////////// H265VideoStreamFramer implementation //////////
//public///////////////////////////////////////////////////////////////////////
H265VideoStreamFramer* H265VideoStreamFramer::createNew(
                                                         UsageEnvironment& env,
                                                         FramedSource* inputSource)
{
   // Need to add source type checking here???  #####
//     std::cout << "H265VideoStreamFramer: in createNew" << std::endl;
   H265VideoStreamFramer* fr;
   fr = new H265VideoStreamFramer(env, inputSource);
   return fr;
}


///////////////////////////////////////////////////////////////////////////////
H265VideoStreamFramer::H265VideoStreamFramer(
                              UsageEnvironment& env,
                              FramedSource* inputSource,
                              Boolean createParser)
                              : FramedFilter(env, inputSource),
                                fFrameRate(0.0), // until we learn otherwise 
                                fPictureEndMarker(False)
{
   // Use the current wallclock time as the base 'presentation time':
   SystemGetTimeofRun(&fPresentationTimeBase, NULL);
//     std::cout << "H265VideoStreamFramer: going to create H265VideoStreamParser" << std::endl;
   fParser = createParser ? new H265VideoStreamParser(this, inputSource) : NULL;
}

///////////////////////////////////////////////////////////////////////////////
H265VideoStreamFramer::~H265VideoStreamFramer()
{
   delete   fParser;
}


///////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
static struct timeval firstPT;
#endif


///////////////////////////////////////////////////////////////////////////////
void H265VideoStreamFramer::doGetNextFrame()
{

  //fParser->registerReadInterest(fTo, fMaxSize);
  //continueReadProcessing();
  fInputSource->getNextFrame(fTo, fMaxSize,
                             afterGettingFrame, this,
                             FramedSource::handleClosure, this);
}

void H265VideoStreamFramer
::afterGettingFrame(void* clientData, unsigned frameSize,
                    unsigned numTruncatedBytes,
                    struct timeval presentationTime,
                    unsigned durationInMicroseconds,
                    MediaFrameInfo stFrameInfo) 
{
  H265VideoStreamFramer* source = (H265VideoStreamFramer*)clientData;
  source->afterGettingFrame1(frameSize, numTruncatedBytes,
                             presentationTime, durationInMicroseconds,
                             stFrameInfo);
}

void H265VideoStreamFramer
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                     struct timeval presentationTime,
                     unsigned durationInMicroseconds,
                     MediaFrameInfo stFrameInfo)
{
	
//	u_int64_t frameDuration;         
//
//	frameDuration = 5;
//	fFrameRate = frameDuration == 0 ? 0.0 : 1000./(long)frameDuration;
//
//	// Compute "fPresentationTime" 
//	fPresentationTime.tv_usec += (long) frameDuration*1000;
//
//	while (fPresentationTime.tv_usec >= 1000000) 
//	{
//	 fPresentationTime.tv_usec -= 1000000;
//	 ++fPresentationTime.tv_sec;
//	}

	// Compute "fDurationInMicroseconds" 
	
	fFrameSize = frameSize;
	fNumTruncatedBytes = numTruncatedBytes;
	fPresentationTime = presentationTime;
	fDurationInMicroseconds = durationInMicroseconds;//(unsigned int) frameDuration*1000;;
	fstFrameInfo = stFrameInfo;
	afterGetting(this);
}

///////////////////////////////////////////////////////////////////////////////
Boolean H265VideoStreamFramer::isH265VideoStreamFramer() const
{
  return True;
}

///////////////////////////////////////////////////////////////////////////////
Boolean H265VideoStreamFramer::currentNALUnitEndsAccessUnit() 
{
  return True;
}


///////////////////////////////////////////////////////////////////////////////
void H265VideoStreamFramer::continueReadProcessing(
                                   void* clientData,
                                   unsigned char* /*ptr*/, unsigned /*size*/,
                                   struct timeval /*presentationTime*/)
{
   H265VideoStreamFramer* framer = (H265VideoStreamFramer*)clientData;
   framer->continueReadProcessing();
}

///////////////////////////////////////////////////////////////////////////////
void H265VideoStreamFramer::continueReadProcessing()
{
   unsigned acquiredFrameSize; 
//     std::cout << "H265VideoStreamFramer: in continueReadProcessing" << std::endl;
   u_int64_t frameDuration;  // in ms


   acquiredFrameSize = fParser->parse();
//   fprintf(stderr, "acquiredFrameSize = %d\n",acquiredFrameSize);


   if (acquiredFrameSize > 0) {

      //  check++;
      // We were able to acquire a frame from the input.
      // It has already been copied to the reader's space.
      fFrameSize = acquiredFrameSize;
//    fNumTruncatedBytes = fParser->numTruncatedBytes(); // not needed so far
      frameDuration = 33;
      fFrameRate = frameDuration == 0 ? 0.0 : 1000./(long)frameDuration;

      // Compute "fPresentationTime" 
      if (acquiredFrameSize == 5) // first frame
         fPresentationTime = fPresentationTimeBase;
      else 
         fPresentationTime.tv_usec += (long) frameDuration*1000;

      while (fPresentationTime.tv_usec >= 1000000) {
         fPresentationTime.tv_usec -= 1000000;
         ++fPresentationTime.tv_sec;
      }

      // Compute "fDurationInMicroseconds" 
      fDurationInMicroseconds = (unsigned int) frameDuration*1000;;


      // Call our own 'after getting' function.  Because we're not a 'leaf'
      // source, we can call this directly, without risking infinite recursion.
      afterGetting(this);
   } else {

      // We were unable to parse a complete frame from the input, because:
      // - we had to read more data from the source stream, or
      // - the source stream has ended.
   }
}

#endif

