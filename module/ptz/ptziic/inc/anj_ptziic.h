#ifndef __ANJ_PTZIIC__H__
#define __ANJ_PTZIIC__H__

#include "anj_ptz.h"

#if defined (__cplusplus)
extern "C" {
#endif

int anj_ptziic_operate(int mode, int arg, int speed);
int anj_ptziic_init();
int anj_ptziic_uninit();
void anj_ptziic_dir_set(PtzDir *pstPtzDir);
void anj_ptziic_debug();

#if defined (__cplusplus)
}
#endif

#endif
