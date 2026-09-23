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
// Common routines used by both RTSP clients and servers
// Implementation

#include "RTSPCommon.hh"
#include "Locale.hh"
#include <string.h>
#include <stdio.h>

#include "project_option.h"


/* 老的分析函数不支持解析"Content-Length:" */
#if 0  
Boolean parseRTSPRequestString(char const* reqStr,
			       unsigned reqStrSize,
			       char* resultCmdName,
			       unsigned resultCmdNameMaxSize,
			       char* resultURLPreSuffix,
			       unsigned resultURLPreSuffixMaxSize,
			       char* resultURLSuffix,
			       unsigned resultURLSuffixMaxSize,
			       char* resultCSeq,
			       unsigned resultCSeqMaxSize) {
  // This parser is currently rather dumb; it should be made smarter #####

  // Read everything up to the first space as the command name:
  Boolean parseSucceeded = False;
  unsigned i;
  for (i = 0; i < resultCmdNameMaxSize-1 && i < reqStrSize; ++i) {
    char c = reqStr[i];
    if (c == ' ' || c == '\t') {
      parseSucceeded = True;
      break;
    }

    resultCmdName[i] = c;
  }
  resultCmdName[i] = '\0';
  if (!parseSucceeded) return False;

  // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
  unsigned j = i+1;
  while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j; // skip over any additional white space
  for (; (int)j < (int)(reqStrSize-8); ++j) {
    if ((reqStr[j] == 'r' || reqStr[j] == 'R')
	&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
	&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
	&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
	&& reqStr[j+4] == ':' && reqStr[j+5] == '/') {
      j += 6;
      if (reqStr[j] == '/') {
	// This is a "rtsp://" URL; skip over the host:port part that follows:
	++j;
	while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
      } else {
	// This is a "rtsp:/" URL; back up to the "/":
	--j;
      }
      i = j;
      break;
    }
  }

  // Look for the URL suffix (before the following "RTSP/"):
  parseSucceeded = False;
  for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k) {
    if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
	reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') {
      while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
      unsigned k1 = k;
      while (k1 > i && reqStr[k1] != '/') --k1;
      // the URL suffix comes from [k1+1,k]

      // Copy "resultURLSuffix":
      if (k - k1 + 1 > resultURLSuffixMaxSize) return False; // there's no room
      unsigned n = 0, k2 = k1+1;
      while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
      resultURLSuffix[n] = '\0';

      // Also look for the URL 'pre-suffix' before this:
      unsigned k3 = (k1 == 0) ? 0 : --k1;
      while (k3 > i && reqStr[k3] != '/') --k3;
      // the URL pre-suffix comes from [k3+1,k1]

      // Copy "resultURLPreSuffix":
      if (k1 - k3 + 1 > resultURLPreSuffixMaxSize) return False; // there's no room
      n = 0; k2 = k3+1;
      while (k2 <= k1) resultURLPreSuffix[n++] = reqStr[k2++];
      resultURLPreSuffix[n] = '\0';

      i = k + 7; // to go past " RTSP/"
      parseSucceeded = True;
      break;
    }
  }
  if (!parseSucceeded) return False;

  // Look for "CSeq:", skip whitespace,
  // then read everything up to the next \r or \n as 'CSeq':
  parseSucceeded = False;
  for (j = i; (int)j < (int)(reqStrSize-5); ++j) {
    if (reqStr[j] == 'C' && reqStr[j+1] == 'S' && reqStr[j+2] == 'e' &&
	reqStr[j+3] == 'q' && reqStr[j+4] == ':') {
      j += 5;
      unsigned n;
      while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
      for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
	char c = reqStr[j];
	if (c == '\r' || c == '\n') {
	  parseSucceeded = True;
	  break;
	}

	resultCSeq[n] = c;
      }
      resultCSeq[n] = '\0';
      break;
    }
  }
  if (!parseSucceeded) return False;

  return True;
}

#else
#include <ctype.h> // for "isxdigit()
#include <stdlib.h> // for "strtol()
static void decodeURL(char* url) {
  // Replace (in place) any %<hex><hex> sequences with the appropriate 8-bit character.
  char* cursor = url;
  while (*cursor) {
    if ((cursor[0] == '%') &&
	cursor[1] && isxdigit(cursor[1]) &&
	cursor[2] && isxdigit(cursor[2])) {
      // We saw a % followed by 2 hex digits, so we copy the literal hex value into the URL, then advance the cursor past it:
      char hex[3];
      hex[0] = cursor[1];
      hex[1] = cursor[2];
      hex[2] = '\0';
      *url++ = (char)strtol(hex, NULL, 16);
      cursor += 3;
    } else {
      // Common case: This is a normal character or a bogus % expression, so just copy it
      *url++ = *cursor++;
    }
  }
  
  *url = '\0';
}

Boolean parseRTSPRequestString(char const* reqStr,
			       unsigned reqStrSize,
			       char* resultCmdName,
			       unsigned resultCmdNameMaxSize,
			       char* resultURLPreSuffix,
			       unsigned resultURLPreSuffixMaxSize,
			       char* resultURLSuffix,
			       unsigned resultURLSuffixMaxSize,
			       char* resultCSeq,
			       unsigned resultCSeqMaxSize,
                   char* resultSessionIdStr,
                   unsigned resultSessionIdStrMaxSize,
			       unsigned& contentLength) {
  // This parser is currently rather dumb; it should be made smarter #####

  // "Be liberal in what you accept": Skip over any whitespace at the start of the request:
    unsigned i;
    for (i = 0; i < reqStrSize; ++i)
    {
        char c = reqStr[i];
        if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0'))
            break;
    }

    if (i == reqStrSize)
        return False; // The request consisted of nothing but whitespace!

    // Then read everything up to the next space (or tab) as the command name:
    Boolean parseSucceeded = False;
    unsigned i1 = 0;
    for (; i1 < resultCmdNameMaxSize-1 && i < reqStrSize; ++i,++i1)
    {
        char c = reqStr[i];
        if (c == ' ' || c == '\t')
        {
            parseSucceeded = True;
            break;
        }
        resultCmdName[i1] = c;
    }

    resultCmdName[i1] = '\0';
    if (!parseSucceeded)
        return False;

    // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
    unsigned j = i+1;
    while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) 
        ++j; // skip over any additional white space

    for (; (int)j < (int)(reqStrSize-8); ++j)
    {
        if ((reqStr[j] == 'r' || reqStr[j] == 'R')
            && (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
            && (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
            && (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
            && reqStr[j+4] == ':' && reqStr[j+5] == '/')
        {
            j += 6;
            if (reqStr[j] == '/')
            {
                // This is a "rtsp://" URL; skip over the host:port part that follows:
                ++j;
                while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ')
                    ++j;
            }
            else
            {
                // This is a "rtsp:/" URL; back up to the "/":
                --j;
            }

            i = j;
            break;
        }
    }

    // Look for the URL suffix (before the following "RTSP/"):
    parseSucceeded = False;
    for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k)
    {
        if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
        reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/')
        {
            while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"

            unsigned k1 = k;

            while (k1 > i && reqStr[k1] != '/')
                --k1;

            // ASSERT: At this point
            //   i: first space or slash after "host" or "host:port"
            //   k: last non-space before "RTSP/"
            //   k1: last slash in the range [i,k]

            // The URL suffix comes from [k1+1, k]
            // Copy "resultURLSuffix":
            unsigned n = 0, k2 = k1+1;
            if (k2 <= k)
            {
                if (k - k1 + 1 > resultURLSuffixMaxSize)
                    return False; // there's no room

                while (k2 <= k) 
                    resultURLSuffix[n++] = reqStr[k2++];
            }
            resultURLSuffix[n] = '\0';

            // The URL 'pre-suffix' comes from [i+1,k1-1]
            // Copy "resultURLPreSuffix":
            n = 0; k2 = i+1;
            if (k2+1 <= k1)
            {
                if (k1 - i > resultURLPreSuffixMaxSize)
                    return False; // there's no room
                while (k2 <= k1-1)
                    resultURLPreSuffix[n++] = reqStr[k2++];
            }

            resultURLPreSuffix[n] = '\0';

/*
    情况1：如果存在前缀ch01~ch0x时，追加到后缀中，live555初始化的session记录的key是 ch0x/stream，需要拿整个去匹配
    情况2：如果存在前缀，但不是 ch01~ch0x，不对前缀和后缀做处理
    情况3：如果是多目设备，url不带ch0x，默认追加ch01到前面（搜索工具一个url是 rtsp://192.168.69.102:554/stream1，一个是rtsp://192.168.69.102/ch02/stream1）
*/
            int isAnjMutiLensPreSuffix = -1;
            if (ANJ_CAMERA_MAX_NUMS > 1 && resultURLSuffix[0] != '\0')
            {
                if (resultURLPreSuffix[0] != '\0')
                {
                    if (resultURLPreSuffix[0] != '\0' &&
                        strlen(resultURLPreSuffix) == 4 &&
                        resultURLPreSuffix[0] == 'c' &&
                        resultURLPreSuffix[1] == 'h' &&
                        isdigit(resultURLPreSuffix[2]) &&
                        isdigit(resultURLPreSuffix[3]) )
                    {
                        isAnjMutiLensPreSuffix = 1;
                    }
                    else
                    {
                        isAnjMutiLensPreSuffix = -1;
                    }
                }
                else
                {
                    isAnjMutiLensPreSuffix = 0;
                }

                char MergeStr[64] = {0};
                if (isAnjMutiLensPreSuffix == 1)
                {
                    snprintf(MergeStr, sizeof(MergeStr), "%s/%s", resultURLPreSuffix, resultURLSuffix);
                }
                else if (isAnjMutiLensPreSuffix == 0)
                {
                    snprintf(MergeStr, sizeof(MergeStr), "ch01/%s", resultURLSuffix);
                }

                if (strlen(MergeStr) > 0 && strlen(MergeStr) < resultURLSuffixMaxSize)
                {
                    strcpy(resultURLSuffix, MergeStr);
                    resultURLPreSuffix[0] = '\0';
                }
            }

            decodeURL(resultURLPreSuffix);

            i = k + 7; // to go past " RTSP/"
            parseSucceeded = True;
            break;
        }
    }

    if (!parseSucceeded)
        return False;

    // Look for "CSeq:" (mandatory, case insensitive), skip whitespace,
    // then read everything up to the next \r or \n as 'CSeq':
    parseSucceeded = False;
    for (j = i; (int)j < (int)(reqStrSize-5); ++j)
    {
        if (_strncasecmp("CSeq:", &reqStr[j], 5) == 0)
        {
            j += 5;
            while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t'))
                ++j;

            unsigned n;
            for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j)
            {
                char c = reqStr[j];
                if (c == '\r' || c == '\n')
                {
                    parseSucceeded = True;
                    break;
                }

                resultCSeq[n] = c;
            }
            resultCSeq[n] = '\0';
            break;
        }
    }

    if (!parseSucceeded)
        return False;

    // Look for "Session:" (optional, case insensitive), skip whitespace,
    // then read everything up to the next \r or \n as 'Session':
    resultSessionIdStr[0] = '\0'; // default value (empty string)
    for (j = i; (int)j < (int)(reqStrSize-8); ++j)
    {
        if (_strncasecmp("Session:", &reqStr[j], 8) == 0)
        {
            j += 8;
            while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t'))
                ++j;

            unsigned n;
            for (n = 0; n < resultSessionIdStrMaxSize-1 && j < reqStrSize; ++n,++j)
            {
                char c = reqStr[j];
                if (c == '\r' || c == '\n')
                {
                    break;
                }

                resultSessionIdStr[n] = c;
            }
            resultSessionIdStr[n] = '\0';
            break;
        }
    }

    // Also: Look for "Content-Length:" (optional, case insensitive)
    contentLength = 0; // default value
    for (j = i; (int)j < (int)(reqStrSize-15); ++j)
    {
        if (_strncasecmp("Content-Length:", &(reqStr[j]), 15) == 0)
        {
            j += 15;
            while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t'))
                ++j;

            unsigned num;
            if (sscanf(&reqStr[j], "%u", &num) == 1)
            {
                contentLength = num;
            }
        }
    }

    return True;
}

#endif

Boolean parseRangeParam(char const* paramStr, double& rangeStart, double& rangeEnd) {
  double start, end;
  int numCharsMatched = 0;
  Locale l("C", LC_NUMERIC);
  if (sscanf(paramStr, "npt = %lf - %lf", &start, &end) == 2) {
    rangeStart = start;
    rangeEnd = end;
  } else if (sscanf(paramStr, "npt = %lf -", &start) == 1) {
    rangeStart = start;
    rangeEnd = 0.0;
  } else if (strcmp(paramStr, "npt=now-") == 0) {
    rangeStart = 0.0;
    rangeEnd = 0.0;
  } else if (sscanf(paramStr, "clock = %*s%n", &numCharsMatched) == 0 && numCharsMatched > 0) {
    // We accept "clock=" parameters, but currently do no interpret them.
  } else if (sscanf(paramStr, "smtpe = %*s%n", &numCharsMatched) == 0 && numCharsMatched > 0) {
    // We accept "smtpe=" parameters, but currently do no interpret them.
  } else {
    return False; // The header is malformed
  }

  return True;
}

Boolean parseRangeHeader(char const* buf, double& rangeStart, double& rangeEnd) {
  // First, find "Range:"
  while (1) {
    if (*buf == '\0') return False; // not found
    if (_strncasecmp(buf, "Range: ", 7) == 0) break;
    ++buf;
  }

  // Then, run through each of the fields, looking for ones we handle:
  char const* fields = buf + 7;
  while (*fields == ' ') ++fields;
  return parseRangeParam(fields, rangeStart, rangeEnd);
}
