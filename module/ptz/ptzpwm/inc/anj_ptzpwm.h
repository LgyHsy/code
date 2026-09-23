#ifndef __ANJ_PTZPWM__H__
#define __ANJ_PTZPWM__H__

#include "anj_ptz.h"

#if defined (__cplusplus)
extern "C" {
#endif

int anj_ptzpwm_operate(int mode, int arg, int speed);
int anj_ptzpwm_init();
int anj_ptzpwm_uninit();

#if defined (__cplusplus)
}
#endif

#endif
