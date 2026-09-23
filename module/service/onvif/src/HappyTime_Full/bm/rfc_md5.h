/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef	RFC_MD5_H
#define	RFC_MD5_H

typedef struct
{
    uint32  total[2];
    uint32  state[4];
    uint8   buffer[64];
} md5_context;


#ifdef __cplusplus
extern "C"{
#endif

HT_API void md5_starts(md5_context *ctx);
HT_API void md5_update(md5_context *ctx, uint8 *input, uint32 length);
HT_API void md5_finish(md5_context *ctx, uint8 digest[16]);

#ifdef __cplusplus
}
#endif

#endif



