#ifndef __ANJ_MW_AOV_H__
#define __ANJ_MW_AOV_H__


/*
    通知编码状态 0-停止编码 1-正在编码
*/ 
int anj_mw_aov_notify_enc_status_chg(int status);

// 通知编码开始
int anj_mw_aov_notify_enc_start();

// 通知编码完成
int anj_mw_aov_notify_enc_done();

// 等待通知开始编码
int anj_mw_aov_wait_notify_enc_start();

// 等待通知编码完成
int anj_mw_aov_wait_notify_enc_done();

/*
    通知算法检测状态 0-停止检测 1-正在检测
*/
int anj_mw_aov_notify_algo_status_chg(int status);

// 通知算法检测开始
int anj_mw_aov_notify_algo_start();

// 通知算法检测完成
int anj_mw_aov_notify_algo_done();

// 等待通知算法检测开始
int anj_mw_aov_wait_notify_algo_start();

// 等待通知算法检测完成
int anj_mw_aov_wait_notify_algo_done();

int anj_mw_aov_sys_sleep_enter();
int anj_mw_aov_sys_sleep_exit();


#endif

