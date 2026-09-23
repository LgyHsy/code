#ifndef __ANJ_PTZDRV__H__
#define __ANJ_PTZDRV__H__

#include "anj_ptz.h"

#if defined (__cplusplus)
extern "C" {
#endif

void anj_ptzdrv_debug();
int anj_ptzdrv_operate(int mode, int arg, int speed);
void anj_ptzdrv_dir_set(PtzDir *pstPtzDir);
int anj_ptzdrv_init();
int anj_ptzdrv_uninit();

#if defined (__cplusplus)
}
#endif

#endif
