#ifndef __ANJ_AIOT_STREAM_H__
#define __ANJ_AIOT_STREAM_H__

#include "gct_common.h"

#define PB_STREAM_MAX 3

GCT_VOID anj_aiot_stream_release(const GCT_UINT32 sid);
GCT_VOID anj_aiot_stream_add(const GCT_UINT32 sid, const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype);
GCT_VOID anj_aiot_stream_del(const GCT_UINT32 sid);
GCT_VOID anj_aiot_stream_switch(const GCT_UINT32 sid, const GCT_UINT32 nFromChannelNo,
                                const GCT_UINT32 nFromStreamtype, const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype);

GCT_VOID anj_aiot_stream_pb_init();

#endif
