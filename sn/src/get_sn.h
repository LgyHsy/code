/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-06-09 18:05:51
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-06-09 18:57:23
 * @FilePath: \sn_svn\get_sn.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __GET_SN_H__
#define __GET_SN_H__

#include "anj_mw_crypt.h"

int CheckSoftSNData_v1(const char* buf, int inbuflen, char *output_decrypt, int outbuflen, int bShowInfo);
int write_encript_data_to_soft_v1(unsigned char *buf, int len, const char *uuid);
void copy_prevention_encrypt_v1(const char *uuid, void *buffer, int buflen);
void set_softsn_v1_cb(SN_GET_OK_CALLBACK cb);

#endif // __GET_SN_H__