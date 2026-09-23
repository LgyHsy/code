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
// An abstraction of a network interface used for RTP (or RTCP).
// (This allows the RTP-over-TCP hack (RFC 2326, section 10.12) to
// be implemented transparently.)
// Implementation

#include "RTPInterface.hh"
#include <GroupsockHelper.hh>
#include <stdio.h>

#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_mw_time.h"

#define MAX_TCP_STREAM_BUFFER 		200*1024
#define MAX_TCP_STREAM_BUFFERTIME 	1000
#define MAX_TCP_STREAM_WAISENDTIME 	10000

extern int get_ms();

////////// Helper Functions - Definition //////////

// Reading RTP-over-TCP is implemented using two levels of hash tables.
// The top-level hash table maps TCP socket numbers to a
// "SocketDescriptor" that contains a hash table for each of the
// sub-channels that are reading from this socket.

static HashTable* socketHashTable(UsageEnvironment& env, Boolean createIfNotPresent = True) {
  _Tables* ourTables = _Tables::getOurTables(env, createIfNotPresent);
  if (ourTables == NULL) return NULL;

  if (ourTables->socketTable == NULL) {
    // Create a new socket number -> SocketDescriptor mapping table:
    ourTables->socketTable = HashTable::create(ONE_WORD_HASH_KEYS);
  }
  return (HashTable*)(ourTables->socketTable);
}

class SocketDescriptor {
public:
  SocketDescriptor(UsageEnvironment& env, int socketNum);
  virtual ~SocketDescriptor();

  void registerRTPInterface(unsigned char streamChannelId,
			    RTPInterface* rtpInterface);
  RTPInterface* lookupRTPInterface(unsigned char streamChannelId);
  void deregisterRTPInterface(unsigned char streamChannelId);

  void setServerRequestAlternativeByteHandler(ServerRequestAlternativeByteHandler* handler, void* clientData) {
    fServerRequestAlternativeByteHandler = handler;
    fServerRequestAlternativeByteHandlerClientData = clientData;
  }
  Boolean fSocketWRErrorOccurred;
  
private:
  static void tcpReadHandler(SocketDescriptor*, int mask);
  Boolean tcpReadHandler1(int mask);

private:
  UsageEnvironment& fEnv;
  int fOurSocketNum;
  HashTable* fSubChannelHashTable;
  ServerRequestAlternativeByteHandler* fServerRequestAlternativeByteHandler;
  void* fServerRequestAlternativeByteHandlerClientData;
  u_int8_t fStreamChannelId, fSizeByte1;
  int fRtpSocketRecvZero;
  Boolean fDeleteMyselfNext, fAreInReadHandlerLoop;
  enum { AWAITING_DOLLAR, AWAITING_STREAM_CHANNEL_ID, AWAITING_SIZE1, AWAITING_SIZE2, AWAITING_PACKET_DATA } fTCPReadingState;
};

#define RTPINTERFACE_BLOCKING_WRITE_TIMEOUT_MS 500
#define RTPINTERFACE_ALL_CHANNELID             0xFF

////////// Helper Functions - Implementation /////////

void sendRTPOverTCP(unsigned char* packet, unsigned packetSize,
                    int socketNum, unsigned char streamChannelId) {
//#ifdef DEBUG
//  fprintf(stderr, "sendRTPOverTCP: %d bytes over channel %d (socket %d)\n",
//	  packetSize, streamChannelId, socketNum); fflush(stderr);
//#endif
  // Send RTP over TCP, using the encoding defined in
  // RFC 2326, section 10.12:
  do {
    char const dollar = '$';
    if (send(socketNum, &dollar, 1, 0) != 1) break;
    if (send(socketNum, (char*)&streamChannelId, 1, 0) != 1) break;

    char netPacketSize[2];
    netPacketSize[0] = (char) ((packetSize&0xFF00)>>8);
    netPacketSize[1] = (char) (packetSize&0xFF);
    if (send(socketNum, netPacketSize, 2, 0) != 2) break;

    if (send(socketNum, (char*)packet, packetSize, 0) != (int)packetSize) break;

//#ifdef DEBUG
//    fprintf(stderr, "sendRTPOverTCP: completed\n"); fflush(stderr);
//#endif

    return;
  } while (0);
#ifdef DEBUG
  fprintf(stderr, "sendRTPOverTCP: %d bytes over channel %d (socket %d)\n",
	  packetSize, streamChannelId, socketNum); fflush(stderr);
#endif
#ifdef DEBUG
  fprintf(stderr, "sendRTPOverTCP: failed!\n"); fflush(stderr);
#endif
}

static SocketDescriptor* lookupSocketDescriptor(UsageEnvironment& env, int sockNum, Boolean createIfNotFound = True) {
  HashTable* table = socketHashTable(env, createIfNotFound);
  if (table == NULL) return NULL;

  char const* key = (char const*)(long)sockNum;
  SocketDescriptor* socketDescriptor = (SocketDescriptor*)(table->Lookup(key));
  if (socketDescriptor == NULL) {
    if (createIfNotFound) {
      socketDescriptor = new SocketDescriptor(env, sockNum);
      table->Add((char const*)(long)(sockNum), socketDescriptor);
    } else if (table->IsEmpty()) {
      // We can also delete the table (to reclaim space):
      _Tables* ourTables = _Tables::getOurTables(env);
      delete table;
      ourTables->socketTable = NULL;
      ourTables->reclaimIfPossible();
    }
  }

  return socketDescriptor;
}

static void removeSocketDescription(UsageEnvironment& env, int sockNum) {
  char const* key = (char const*)(long)sockNum;
  HashTable* table = socketHashTable(env);
  table->Remove(key);

  if (table->IsEmpty()) {
    // We can also delete the table (to reclaim space):
    _Tables* ourTables = _Tables::getOurTables(env);
    delete table;
    ourTables->socketTable = NULL;
    ourTables->reclaimIfPossible();
  }
}

////////// RTPInterface - Implementation //////////
static void deregisterSocket(UsageEnvironment& env, int sockNum, unsigned char streamChannelId) {
  SocketDescriptor* socketDescriptor = lookupSocketDescriptor(env, sockNum, False);
  if (socketDescriptor != NULL) {    
    socketDescriptor->deregisterRTPInterface(streamChannelId);
        // Note: This may delete "socketDescriptor",
        // if no more interfaces are using this socket
  }
}

static void removeSocketAfterSentError(void *pvDate)
{
    SocketDescriptor* socketDescriptor = (SocketDescriptor *)pvDate;
    if (socketDescriptor != NULL) {
        socketDescriptor->deregisterRTPInterface(RTPINTERFACE_ALL_CHANNELID);
    }
}


RTPInterface::RTPInterface(Medium* owner, Groupsock* gs)
  : fOwner(owner), fGS(gs),
    fTCPStreams(NULL),
    fNextTCPReadSize(0), fNextTCPReadStreamSocketNum(-1),
    fNextTCPReadStreamChannelId(0xFF), fReadHandlerProc(NULL),
    fAuxReadHandlerFunc(NULL), fAuxReadHandlerClientData(NULL) {
  // Make the socket non-blocking, even though it will be read from only asynchronously, when packets arrive.
  // The reason for this is that, in some OSs, reads on a blocking socket can (allegedly) sometimes block,
  // even if the socket was previously reported (e.g., by "select()") as having data available.
  // (This can supposedly happen if the UDP checksum fails, for example.)
  net_makeSocketNonBlocking(fGS->socketNum());
  increaseSendBufferTo(envir(), fGS->socketNum(), 600*1024);
  //__ERR("set socket send buffer to 200K, sock=%d\n", fGS->socketNum());
}

RTPInterface::~RTPInterface() {
  __INFO("RTPInterface[%p]::~RTPInterface\n",this);
  stopNetworkReading();
  delete fTCPStreams;
}

void RTPInterface::setStreamSocket(int sockNum,
				   unsigned char streamChannelId) {
  fGS->removeAllDestinations();
  addStreamSocket(sockNum, streamChannelId);
}

void RTPInterface::addStreamSocket(int sockNum,
				   unsigned char streamChannelId) {				   
  if (sockNum < 0) return;

  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    if (streams->fStreamSocketNum == sockNum
	&& streams->fStreamChannelId == streamChannelId) {
      return; // we already have it
    }
  }
  
  fTCPStreams = new tcpStreamRecord(sockNum, streamChannelId, fTCPStreams);

  // Also, make sure this new socket is set up for receiving RTP/RTCP-over-TCP:
  SocketDescriptor* socketDescriptor = lookupSocketDescriptor(envir(), sockNum);
  socketDescriptor->registerRTPInterface(streamChannelId, this);  
}



void RTPInterface::removeStreamSocket(int sockNum,
				                      unsigned char streamChannelId) 
{
  while (1) 
  {
    tcpStreamRecord** streamsPtr = &fTCPStreams;
    while (*streamsPtr != NULL) 
    {      
      if ( ((*streamsPtr)->fStreamSocketNum == sockNum)
	       && ((streamChannelId == RTPINTERFACE_ALL_CHANNELID) || (streamChannelId == (*streamsPtr)->fStreamChannelId))) 
	  {
        // Delete the record pointed to by *streamsPtr :
        tcpStreamRecord* next = (*streamsPtr)->fNext;
        (*streamsPtr)->fNext = NULL;

        // And 'deregister' this socket,channelId pair:
        deregisterSocket(envir(), sockNum, streamChannelId);	  
        
        delete (*streamsPtr);
        *streamsPtr = next;
                        
        if (streamChannelId != RTPINTERFACE_ALL_CHANNELID)
        {
            return; // we're done
        }
        
        break;// start again from the beginning of the list, in case the list has changed
      } 
      else 
      {
	    streamsPtr = &((*streamsPtr)->fNext);
      }
    }

    if (*streamsPtr == NULL) break;
  }
}


void RTPInterface
::setServerRequestAlternativeByteHandler(int socketNum, ServerRequestAlternativeByteHandler* handler, void* clientData) {
  SocketDescriptor* socketDescriptor = lookupSocketDescriptor(envir(), socketNum);

  if (socketDescriptor != NULL) 
    socketDescriptor->setServerRequestAlternativeByteHandler(handler, clientData);
}


int RTPInterface::sendRTPOverTCP(unsigned char* packet, unsigned packetSize,
			  tcpStreamRecord* curStream)
{
	unsigned char header[4];
    int header_size = 4;
    int ret;
    
    header[0] = 0x24;
    header[1] = curStream->fStreamChannelId;
   	header[2] = (char) ((packetSize&0xFF00)>>8);
    header[3] = (char) (packetSize&0xFF);
    
    tcpStreamRecord* stream = NULL;

	for (tcpStreamRecord* streamEntry = fTCPStreams; 
		streamEntry != NULL;
   		streamEntry = streamEntry->fNext) 
   	{
   		//__ERR("sock = %d, chnid = %d\n", streamEntry->fStreamSocketNum, streamEntry->fStreamChannelId);
   		
   		if(streamEntry->fStreamSocketNum == curStream->fStreamSocketNum
   			&& streamEntry->fSendBuf)
   		{
   			stream = streamEntry;
   			break;
   		}
   	}
   	
   	if(stream == NULL)
   		stream = curStream;
    
    if(stream == NULL)
    {
    	__ERR("can not found stream, cursock = %d, curchnid = %d!!!\n",
    		curStream->fStreamSocketNum,
    		curStream->fStreamChannelId);
    		
    	return -1;
    }

	//to avoid buffer copy just send directly
	if(stream->isBufferEmpty())
	{
		net_makeSocketBlockingWithTimeout(stream->fStreamSocketNum, RTPINTERFACE_BLOCKING_WRITE_TIMEOUT_MS);		
 		ret = send(stream->fStreamSocketNum, header, header_size, 0);	
		net_makeSocketNonBlocking(stream->fStreamSocketNum);
		
		
   		if(ret <= 0)
   		{
   			__ERR("[RTPInterface::sendRTPOverTCP]send header return %d, errno = %d, err = %s, fStreamSocketNum = %d\n", ret, errno, strerror(errno), stream->fStreamSocketNum); 
   			
   			if((errno == ECONNRESET) || (errno == EPIPE))
   				return -1;		
   			
   			if(stream->checkSendTimeout(MAX_TCP_STREAM_WAISENDTIME))
   				return -1;
   			else
   			{
   				if(stream->fStreamChannelId == 0)				// 仅缓存码流数据通道，RTCP和音频通道不缓存，节省内存。
   				{
	   				stream->bufferData(header, header_size);	
					stream->bufferData(packet, packetSize);			
   				}
			}
		
			return 0;
   		}
   		else
   		{
   			stream->updateSendTime();
   			
   			if(ret < header_size)
   			{
   				 __ERR("[RTPInterface::sendRTPOverTCP]send header part, ret = %d, header_size= %d\n", ret, header_size);
   				if(stream->fStreamChannelId == 0)
   				{
	   				stream->bufferData(header + ret, header_size - ret);
	   				stream->bufferData(packet, packetSize);
   				}	
   				return 0;
   			}
   		}
	
		//send data
//
//		send_start = get_ms();
		net_makeSocketBlockingWithTimeout(stream->fStreamSocketNum, RTPINTERFACE_BLOCKING_WRITE_TIMEOUT_MS);		
 		ret = send(stream->fStreamSocketNum, packet, packetSize, 0);
		net_makeSocketNonBlocking(stream->fStreamSocketNum);
		
   		if(ret <= 0)
   		{
   			__ERR("[RTPInterface::sendRTPOverTCP]send packet return %d, errno = %d, err = %s, fStreamSocketNum = %d, packetSize = %d\n", ret, errno, strerror(errno), stream->fStreamSocketNum, packetSize); 
 
   			if((errno == ECONNRESET) || (errno == EPIPE))
   				return -1;
    			
   			if(stream->checkSendTimeout(MAX_TCP_STREAM_WAISENDTIME))
   				return -1;
   			else
   			{
   				if(stream->fStreamChannelId == 0)
   				{
					stream->bufferData(packet, packetSize);
   				}
   			}
			
			return 0;
   		}
   		else
   		{
   			stream->updateSendTime();
   			
   			if((unsigned)ret < packetSize)
   			{
   				__ERR("[[RTPInterface::sendRTPOverTCP]]send packet part, ret = %d, packetSize= %d\n", ret, packetSize);
				if(stream->fStreamChannelId == 0)
   				{
   					stream->bufferData(packet + ret, packetSize - ret);   		
				}
   			}
   			
   			return 0;
   		}				
	}
	else //buffer and send
	{	
		if(stream->checkBufferTimeout(MAX_TCP_STREAM_BUFFERTIME)) //reduce time delay
		{
			__ERR("[[RTPInterface::sendRTPOverTCP]]buffered data more than %d ms, drop all the buffered data!!!\n", MAX_TCP_STREAM_BUFFERTIME);
			stream->cleanBuffer();
		
			stream->updateBufferCheckTime();
		}
		if(stream->fStreamChannelId == 0)
		{
			stream->bufferData(header, header_size);	
			stream->bufferData(packet, packetSize);	
		}
		return stream->sendBuffer();
	}
	
	return 0;
}


void RTPInterface::sendPacket(unsigned char* packet, unsigned packetSize) {
  // Normal case: Send as a UDP packet:
  fGS->output(envir(), fGS->ttl(), packet, packetSize);

  int ret;

  // Also, send over each of our TCP sockets:
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    ret = sendRTPOverTCP(packet, packetSize, streams);
    
	if(ret<0)
	{
		__ERR("111 sendRTPOverTCP failed, sock: %d, chn: %d\n",
			streams->fStreamSocketNum,
			streams->fStreamChannelId);
			        
        SocketDescriptor* socketDescriptor = lookupSocketDescriptor(envir(), streams->fStreamSocketNum, False);
        if (socketDescriptor != NULL)
        {
    		__ERR("222 lookupSocketDescriptor, sock: %d, chn: %d socketDescriptor:%p\n",
    			streams->fStreamSocketNum,
    			streams->fStreamChannelId,
    			socketDescriptor);
            socketDescriptor->fSocketWRErrorOccurred = True;
            envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)removeSocketAfterSentError, (void*)socketDescriptor);
        }
	}
  }

}


void RTPInterface
::startNetworkReading(TaskScheduler::BackgroundHandlerProc* handlerProc) {
  // Normal case: Arrange to read UDP packets:
  envir().taskScheduler().
    turnOnBackgroundReadHandling(fGS->socketNum(), handlerProc, fOwner);

  // Also, receive RTP over TCP, on each of our TCP connections:
  fReadHandlerProc = handlerProc;
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    // Get a socket descriptor for "streams->fStreamSocketNum":
    SocketDescriptor* socketDescriptor = lookupSocketDescriptor(envir(), streams->fStreamSocketNum);

    // Tell it about our subChannel:
    socketDescriptor->registerRTPInterface(streams->fStreamChannelId, this);
  }
}



Boolean RTPInterface::handleRead(unsigned char* buffer, unsigned bufferMaxSize,
				 unsigned& bytesRead, struct sockaddr_in& fromAddress,
				 int& tcpSocketNum, unsigned char& tcpStreamChannelId,
				 Boolean& packetReadWasIncomplete) {
  packetReadWasIncomplete = False; // by default
  Boolean readSuccess = False;
  if (fNextTCPReadStreamSocketNum < 0) {
    // Normal case: read from the (datagram) 'groupsock':
    tcpSocketNum = -1;
    readSuccess = fGS->handleRead(buffer, bufferMaxSize, bytesRead, fromAddress);
  } else {
    // Read from the TCP connection:
    tcpSocketNum = fNextTCPReadStreamSocketNum;
    tcpStreamChannelId = fNextTCPReadStreamChannelId;

    bytesRead = 0;
    unsigned totBytesToRead = fNextTCPReadSize;
    if (totBytesToRead > bufferMaxSize) totBytesToRead = bufferMaxSize;
    unsigned curBytesToRead = totBytesToRead;
    int curBytesRead;
    while ((curBytesRead = readSocket(envir(), fNextTCPReadStreamSocketNum,
				      &buffer[bytesRead], curBytesToRead,
				      fromAddress)) > 0) {
      bytesRead += curBytesRead;
      if (bytesRead >= totBytesToRead) break;
      curBytesToRead -= curBytesRead;
    }
    fNextTCPReadSize -= bytesRead;
    if (fNextTCPReadSize == 0) {
      // We've read all of the data that we asked for
      readSuccess = True;
    } else if (curBytesRead < 0) {
      // There was an error reading the socket
      bytesRead = 0;
      readSuccess = False;
    } else {
      // We need to read more bytes, and there was not an error reading the socket
      packetReadWasIncomplete = True;
      return True;
    }
    fNextTCPReadStreamSocketNum = -1; // default, for next time
  }

  if (readSuccess && fAuxReadHandlerFunc != NULL) {
    // Also pass the newly-read packet data to our auxilliary handler:
    (*fAuxReadHandlerFunc)(fAuxReadHandlerClientData, buffer, bytesRead);
  }
  return readSuccess;
}


void RTPInterface::stopNetworkReading() {
  // Normal case
  envir().taskScheduler().turnOffBackgroundReadHandling(fGS->socketNum());

  // Also turn off read handling on each of our TCP connections:
  for (tcpStreamRecord* streams = fTCPStreams; streams != NULL;
       streams = streams->fNext) {
    deregisterSocket(envir(), streams->fStreamSocketNum, streams->fStreamChannelId);
  }
}


SocketDescriptor::SocketDescriptor(UsageEnvironment& env, int socketNum)
  :fSocketWRErrorOccurred(False), 
   fEnv(env), fOurSocketNum(socketNum),
   fSubChannelHashTable(HashTable::create(ONE_WORD_HASH_KEYS)),
   fServerRequestAlternativeByteHandler(NULL), fServerRequestAlternativeByteHandlerClientData(NULL),
   fRtpSocketRecvZero(0),
   fDeleteMyselfNext(False), fAreInReadHandlerLoop(False),
   fTCPReadingState(AWAITING_DOLLAR) {
}

SocketDescriptor::~SocketDescriptor() 
{
    __INFO("####### ~SocketDescriptor start fSocketWRErrorOccurred %d\n", fSocketWRErrorOccurred);
    removeSocketDescription(fEnv, fOurSocketNum);

    if (fSubChannelHashTable != NULL) {
      // Remove knowledge of this socket from any "RTPInterface"s that are using it:
      HashTable::Iterator* iter = HashTable::Iterator::create(*fSubChannelHashTable);
      RTPInterface* rtpInterface;
      char const* key;
  
      while ((rtpInterface = (RTPInterface*)(iter->next(key))) != NULL) {
        u_int64_t streamChannelIdLong = (u_int64_t)key;
        unsigned char streamChannelId = (unsigned char)streamChannelIdLong;
  
        rtpInterface->removeStreamSocket(fOurSocketNum, streamChannelId);
      }
      delete iter;
  
      // Then remove the hash table entries themselves, and then remove the hash table:
      while (fSubChannelHashTable->RemoveNext() != NULL) {}
      delete fSubChannelHashTable;
      fSubChannelHashTable = NULL;
    }
  
    // Hack: Pass a special character to our alternative byte handler, to tell it that either
    // - an error occurred when reading the TCP socket, or
    // - no error occurred, but it needs to take over control of the TCP socket once again.
    if (fSocketWRErrorOccurred == True)
    {
        if (fServerRequestAlternativeByteHandler != NULL)
        {
            (*fServerRequestAlternativeByteHandler)
                (fServerRequestAlternativeByteHandlerClientData, 0xFF);
        }
    }
    __INFO("####### ~SocketDescriptor end fSocketWRErrorOccurred %d\n", fSocketWRErrorOccurred);    
}

void SocketDescriptor::registerRTPInterface(unsigned char streamChannelId,
					    RTPInterface* rtpInterface) {
  Boolean isFirstRegistration = fSubChannelHashTable->IsEmpty();
  fSubChannelHashTable->Add((char const*)(long)streamChannelId,
			    rtpInterface);

  if (isFirstRegistration) {
    // Arrange to handle reads on this TCP socket:
    TaskScheduler::BackgroundHandlerProc* handler
      = (TaskScheduler::BackgroundHandlerProc*)&tcpReadHandler;
    fEnv.taskScheduler().turnOnBackgroundReadHandling(fOurSocketNum, handler, this);
  }
}

RTPInterface* SocketDescriptor
::lookupRTPInterface(unsigned char streamChannelId) {
  char const* lookupArg = (char const*)(long)streamChannelId;
  return (RTPInterface*)(fSubChannelHashTable->Lookup(lookupArg));
}

void SocketDescriptor::deregisterRTPInterface(unsigned char streamChannelId) 
{
    if (fSubChannelHashTable == NULL)
    {
        return;
    }

    if (streamChannelId != RTPINTERFACE_ALL_CHANNELID)
    {
        fSubChannelHashTable->Remove((char const*)(long)streamChannelId);
    }
    
    if (fSubChannelHashTable->IsEmpty() || (streamChannelId == RTPINTERFACE_ALL_CHANNELID))
    {    
      // No more interfaces are using us, so it's curtains for us now
        if (fAreInReadHandlerLoop) 
        {        
            // we can't delete ourself yet, but we'll do so from "tcpReadHandler()" below
            fDeleteMyselfNext = True; 
        }
        else 
        {
            delete this;
        }    
    }
}


void SocketDescriptor::tcpReadHandler(SocketDescriptor* socketDescriptor, int mask) {
  // Call the read handler until it returns false, with a limit to avoid starving other sockets
    unsigned count = 2000;
    Boolean bTcpReadRet;
    if (socketDescriptor != NULL)
    {
        socketDescriptor->fAreInReadHandlerLoop = True;  
        while ((socketDescriptor->fDeleteMyselfNext != True)
               && (--count > 0))
        {
            bTcpReadRet = socketDescriptor->tcpReadHandler1(mask);
            if (False == bTcpReadRet)
            {
                break;
            }
        }
        socketDescriptor->fAreInReadHandlerLoop = False;
        
        if (socketDescriptor->fDeleteMyselfNext == True)
        {
            delete socketDescriptor;
        }
    }
}

Boolean SocketDescriptor::tcpReadHandler1(int mask) 
{
    // We expect the following data over the TCP channel:
    //   optional RTSP command or response bytes (before the first '$' character)
    //   a '$' character
    //   a 1-byte channel id
    //   a 2-byte packet size (in network byte order)
    //   the packet data.
    // However, because the socket is being read asynchronously, this data might arrive in pieces.
    
    u_int8_t c;
    struct sockaddr_in fromAddress;
    if (fTCPReadingState != AWAITING_PACKET_DATA) 
    {
        int result = readSocket(fEnv, fOurSocketNum, &c, 1, fromAddress);
        if (result == 0) 
        { // There was no more data to read
            fRtpSocketRecvZero++;
            if (5 <= fRtpSocketRecvZero)
            {
                __ERR("readSocket(%d) Remote Socket close!\n",
                      fOurSocketNum); 
                fSocketWRErrorOccurred = True;
                fDeleteMyselfNext = True;
            }
            return False;
        } 
        else if (result != 1) 
        { // error reading TCP socket, so we will no longer handle it
            __ERR("readSocket(%d)(1 byte) returned %d (error)\n",
                  fOurSocketNum, result);
            fSocketWRErrorOccurred = True;
            fDeleteMyselfNext = True;
            return False;
        }

        fRtpSocketRecvZero = 0;
    }
  
    Boolean callAgain = True;
    switch (fTCPReadingState) 
    {
        case AWAITING_DOLLAR:
        {
            if (c == '$') 
            {
    	        //__INFO("Socket %d Saw '$'\n", fOurSocketNum);
    	        fTCPReadingState = AWAITING_STREAM_CHANNEL_ID;
            } 
            else
            {
    	        // This character is part of a RTSP request or command, which is handled separately:
    	        if (fServerRequestAlternativeByteHandler != NULL && c != 0xFF && c != 0xFE)
                {
    	            // Hack: 0xFF and 0xFE are used as special signaling characters, so don't send them
    	            (*fServerRequestAlternativeByteHandler)(fServerRequestAlternativeByteHandlerClientData, c);
    	        }
            }
            break;
        }
        case AWAITING_STREAM_CHANNEL_ID: 
        {
            // The byte that we read is the stream channel id.
            if (lookupRTPInterface(c) != NULL)
            { // sanity check
    	        fStreamChannelId = c;
    	        fTCPReadingState = AWAITING_SIZE1;
            } 
            else
            {
    	        __ERR("Socket (%d) Saw nonexistent stream channel id: 0x%02x\n",
                      fOurSocketNum, c);
    	        fTCPReadingState = AWAITING_DOLLAR;
				
				fSocketWRErrorOccurred = True;
                fDeleteMyselfNext = True;
    			callAgain = False;
            }
            break;
        }
        case AWAITING_SIZE1: 
        {
            // The byte that we read is the first (high) byte of the 16-bit RTP or RTCP packet 'size'.
            fSizeByte1 = c;
            fTCPReadingState = AWAITING_SIZE2;
            break;
        }
        case AWAITING_SIZE2: 
        {
            // The byte that we read is the second (low) byte of the 16-bit RTP or RTCP packet 'size'.
            unsigned short size = (fSizeByte1<<8)|c;
          
            // Record the information about the packet data that will be read next:
            RTPInterface* rtpInterface = lookupRTPInterface(fStreamChannelId);
            if (rtpInterface != NULL)
            {
    	        rtpInterface->fNextTCPReadSize = size;
            	rtpInterface->fNextTCPReadStreamSocketNum = fOurSocketNum;
            	rtpInterface->fNextTCPReadStreamChannelId = fStreamChannelId;
            }
            fTCPReadingState = AWAITING_PACKET_DATA;
            break;
        }
        case AWAITING_PACKET_DATA: 
        {
            callAgain = False;
            fTCPReadingState = AWAITING_DOLLAR; // the next state, unless we end up having to read more data in the current state
            // Call the appropriate read handler to get the packet data from the TCP stream:
            RTPInterface* rtpInterface = lookupRTPInterface(fStreamChannelId);
            if (rtpInterface != NULL)
            {
    	        if (rtpInterface->fNextTCPReadSize == 0) 
                {
    	            // We've already read all the data for this packet.
    	            break;
    	        }
                
    	        if (rtpInterface->fReadHandlerProc != NULL) 
                {
    	            //__INFO("Socket %d reading %d bytes on channel %d\n",
                    //       fOurSocketNum, rtpInterface->fNextTCPReadSize, rtpInterface->fNextTCPReadStreamChannelId);
    	            fTCPReadingState = AWAITING_PACKET_DATA;
    	            rtpInterface->fReadHandlerProc(rtpInterface->fOwner, mask);
    	        } 
                else 
                {
    	            //__INFO("Socket %d No handler proc for \"rtpInterface\" for channel %d; need to skip %d remaining bytes\n",
                    //       fOurSocketNum, fStreamChannelId, rtpInterface->fNextTCPReadSize);
    	            int result = readSocket(fEnv, fOurSocketNum, &c, 1, fromAddress);
                    if (result == 0) 
                    { // There was no more data to read
                        fRtpSocketRecvZero++;
                        if (5 <= fRtpSocketRecvZero)
                        {
                            __ERR("AWAITING_PACKET_DATA readSocket(%d) Remote Socket close!\n",
                                  fOurSocketNum); 
                            fSocketWRErrorOccurred = True;
                            fDeleteMyselfNext = True;
                        }
                        else
                        {
                            fTCPReadingState = AWAITING_PACKET_DATA;
                        }
                    } 
                    else if (result != 1) 
                    { // error reading TCP socket, so we will no longer handle it
                        __ERR("AWAITING_PACKET_DATA readSocket(%d)(1 byte) returned %d (error)\n",
                              fOurSocketNum, result);
                        fSocketWRErrorOccurred = True;
                        fDeleteMyselfNext = True;
                    }
                    else 
                    {
                        fRtpSocketRecvZero = 0;
    	                fTCPReadingState = AWAITING_PACKET_DATA;
    	                --rtpInterface->fNextTCPReadSize;
    	                callAgain = True;
    	            }  
    	        }
            }
            else 
            {
                __ERR("Socket(%d) No \"rtpInterface\" for channel %d\n",
                      fOurSocketNum, fStreamChannelId);
            }        
        }
    }
  
    return callAgain;
}


////////// tcpStreamRecord implementation //////////
/*
tcpStreamRecord
::tcpStreamRecord(int streamSocketNum, unsigned char streamChannelId,
		  tcpStreamRecord* next)
  : fNext(next),
    fStreamSocketNum(streamSocketNum), fStreamChannelId(streamChannelId) {
}

tcpStreamRecord::~tcpStreamRecord() {
  delete fNext;
}
*/

////////// tcpStreamRecord implementation //////////

tcpStreamRecord
::tcpStreamRecord(int streamSocketNum, unsigned char streamChannelId,
		  tcpStreamRecord* next)
  : fNext(next),
    fStreamSocketNum(streamSocketNum), fStreamChannelId(streamChannelId) 
{
//	__ERR("sock = %d, chnid = %d\n",
//		fStreamSocketNum, streamChannelId);
		
  fSendBuf = NULL;
  fSendBufPos = 0;
  fSendBufOffset = 0;
  fSendBufSize = 0;
  
  updateSendTime();
  updateBufferCheckTime();
}

tcpStreamRecord::~tcpStreamRecord() 
{
//  __ERR("fSendBuf: 0x%08x, fSendBufSize: %d\n", fSendBuf, fSendBufSize);
	 
  if(fSendBuf && fSendBufSize)
  {
  	 delete fSendBuf;
//  	 __ERR("delete fSendBuf: 0x%08x\n", fSendBuf);
     fSendBuf = NULL;
  }

  if(fNext)
  {
      delete fNext;
  }
}

int tcpStreamRecord::isBufferEmpty(void)
{
	if(fSendBuf == NULL)
		return 1;
		
	if(fSendBufOffset - fSendBufPos == 0)
		return 1;
	
	return 0;
}

void tcpStreamRecord::cleanBuffer(void)
{
	//drop all the buffered data  
	fSendBufPos = 0;
	fSendBufOffset = 0;
}

void tcpStreamRecord::updateSendTime(void)
{
	SystemGetTimeofRun(&fLastSentTime, NULL);	
}

int tcpStreamRecord::checkSendTimeout(int countMs)
{
	struct timeval nowtime;
	SystemGetTimeofRun(&nowtime, NULL);
	long diff = 0;
	
	if(fLastSentTime.tv_sec > 0)
	{
		diff = (nowtime.tv_sec - fLastSentTime.tv_sec)*1000 
			+ (nowtime.tv_usec - fLastSentTime.tv_usec)/1000;
		
		if(diff > countMs)
		{
			__ERR("don't send data for %d seconds!!!\n", countMs/1000);
			return 1;
		}
	}
	
	return 0;
}

void tcpStreamRecord::updateBufferCheckTime(void)
{
	SystemGetTimeofRun(&fLastBufferCheckTime, NULL);	
}

int tcpStreamRecord::checkBufferTimeout(int countMs)
{
	struct timeval nowtime;
	SystemGetTimeofRun(&nowtime, NULL);
	long diff = 0;
	
	if(fLastSentTime.tv_sec > 0 && fLastBufferCheckTime.tv_sec > 0)
	{
		diff = (nowtime.tv_sec - fLastBufferCheckTime.tv_sec)*1000 
			+ (nowtime.tv_usec - fLastBufferCheckTime.tv_usec)/1000;
		
		if(diff > countMs)
		{				
			diff = (nowtime.tv_sec - fLastSentTime.tv_sec)*1000 
				+ (nowtime.tv_usec - fLastSentTime.tv_usec)/1000;
			
			if(diff > countMs)
			{
				__ERR("checkBufferTimeout for %d seconds!!!\n", countMs/1000);
				return 1;
			}
		}
	}
	
	return 0;
}

int tcpStreamRecord::bufferData(unsigned char *buf, int size)
{
	if(fSendBuf == NULL)
	{
		fSendBuf = new unsigned char[MAX_TCP_STREAM_BUFFER];
		if(NULL == fSendBuf)
		{
			__ERR("no memory!!!\n");
			return -1;
		}
		
		__ERR("malloc fSendBuf, size = %d\n", MAX_TCP_STREAM_BUFFER);
		
		fSendBufPos    = 0;
		fSendBufOffset = 0;		
		fSendBufSize = MAX_TCP_STREAM_BUFFER;
	}
	
	if(fSendBufOffset + size > fSendBufSize)
	{
		if(fSendBufPos > 0 && fSendBufOffset > fSendBufPos)
		{
			memmove(fSendBuf, fSendBuf + fSendBufPos,
				fSendBufOffset - fSendBufPos);
								
			fSendBufOffset -= fSendBufPos;
			fSendBufPos = 0;
			
			if(fSendBufOffset + size > fSendBufSize)
			{
				__ERR("drop all the buffered data!!!\n");
				
				fSendBufPos 	= 0;
				fSendBufOffset 	= 0;
			}
		}
		else
		{
			__ERR("drop all the buffered data!!!\n");
			
			fSendBufPos 	= 0;
			fSendBufOffset 	= 0;
		}
	}
	
	memcpy(fSendBuf + fSendBufOffset, buf, size);
	fSendBufOffset+=size;
	
	return 1;
}

int tcpStreamRecord::sendBuffer(void)
{
//	int send_start = 0;
//	int send_end = 0;


//	send_start = get_ms();	
	
	int ret = send(fStreamSocketNum, 
					fSendBuf + fSendBufPos,
					fSendBufOffset - fSendBufPos, 0);
	
	if(ret <= 0)
	{
		__ERR("[tcpStreamRecord::sendBuffer]send return %d, errno = %d, err = %s, fStreamSocketNum = %d\n", ret, errno, strerror(errno), fStreamSocketNum);
		
		if((errno == ECONNRESET) || (errno == EPIPE))
			return -1;
					
		if(checkSendTimeout(MAX_TCP_STREAM_WAISENDTIME))
			return -1;
		else
			return 0;
		
		return 0;
	}
	else
	{
		updateSendTime();
		
		fSendBufPos+=ret;
		if(fSendBufPos < fSendBufOffset)
		{
			 __INFO("send part, ret = %d, tosend = %d\n", ret, fSendBufOffset - fSendBufPos);
			 return 0; 						
		}
		else
		{
			fSendBufPos 	= 0;
			fSendBufOffset 	= 0;
			
			return 1;
		}
	}
}
