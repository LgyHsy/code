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
// Inclusion of header files representing the interface
// for the entire library
//
// Programs that use the library can include this header file,
// instead of each of the individual media header files

#ifndef _LIVEMEDIA_HH
#define _LIVEMEDIA_HH

#include "H264VideoFileSink.hh"
#include "H265VideoFileSink.hh"
#include "BasicUDPSink.hh"
#include "H264VideoRTPSink.hh"
#include "H265VideoRTPSink.hh"

//#include "H264VideoStreamDiscreteFramer.hh"
#include "H264VideoStreamFramer.hh"
#include "SimpleRTPSink.hh"
#include "uLawAudioFilter.hh"
#include "ByteStreamMultiFileSource.hh"
#include "BasicUDPSource.hh"
#include "SimpleRTPSource.hh"
#include "QCELPAudioRTPSource.hh"
#include "H264VideoRTPSource.hh"
#include "H265VideoRTPSource.hh"
#include "DeviceSource.hh"
#include "AudioInputDevice.hh"
#include "RTSPServer.hh"
#include "RTSPClient.hh"
#include "SIPClient.hh"
#include "QuickTimeFileSink.hh"
#include "QuickTimeGenericRTPSource.hh"
#include "PassiveServerMediaSubsession.hh"
#include "DarwinInjector.hh"


#endif
