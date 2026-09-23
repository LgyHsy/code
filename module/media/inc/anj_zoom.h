#ifndef __ANJ_ZOOM_H__
#define __ANJ_ZOOM_H__

#include "anj_mw_comm.h"
#include "anj_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void anj_zoom_ctrl(int iCameraIdex, double run_multiple);
void anj_zoom_interrupt(int iCameraIdex, double run_multiple);
void anj_zoom_track_start(int iCameraIdex, PD_AREA_ENTRY *zoomTrackArea, int reset);
void anj_zoom_track_stop(int iCameraIdex);
int anj_zoom_run_get(int iCameraIdex, DOUBLE_AREA_ENTRY *cur_area, double *pRunPercent);
void anj_zoom_stop(int iCameraIdex, double *pRunPercent);
void anj_zoom_osd_type_set();

#if defined(__cplusplus)
}
#endif

#endif
