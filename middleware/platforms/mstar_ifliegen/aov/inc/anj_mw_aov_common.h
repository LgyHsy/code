#ifndef __ANJ_MW_AOV_COMMON_H__
#define __ANJ_MW_AOV_COMMON_H__

#include "mi_vif.h"
#include "mi_common_datatype.h"

#include "st_vif.h"


void aov_com_notify_encode_status_chg(int status);
// 通知编码开始
int aov_com_notify_encode_start();

// 通知编码完成
int aov_com_notify_encode_done();

// 等待通知开始编码
int aov_com_wait_notify_encode_start();

// 等待通知编码完成
int aov_com_wait_notify_encode_done();

void aov_com_notify_algo_detect_status_change(int status);
int aov_com_notify_algo_detect_start();
int aov_com_notify_algo_detect_done();
int aov_com_wait_notify_algo_detect_start();
int aov_com_wait_notify_algo_detect_done();


MI_S32 ST_Common_Aov_Sleep_Enter();
MI_S32 ST_Common_Aov_Sleep_Exit();



#endif

