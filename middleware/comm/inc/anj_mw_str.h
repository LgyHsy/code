#ifndef __ANJ_MW_STR_H__
#define __ANJ_MW_STR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

char *copy_with_shell(char *out, char *in);

unsigned char char2hex(const char *str);
unsigned char char2int(char c);

char *bin_to_hexstr(char *out, int buflen, char *in);

int hex2ascii(const char *hex, int len, char *out_buf, int buf_len);

int Str2Num(const char *buf);

double Str2Double(const char *buf);

int IPStr2Num(char *IPStr);

char *Num2IPStr(int IP);

void StrCpy(char *dest, int destLen, const char *src);

char *replace_str(const char *inStr, const char *pSrc, const char *pReplace);

char *restore_with_escape(const char *inStr);

int gb2312_to_utf8(const char *pIn, char *pOut);
int utf8_to_gb2312(const char *pIn, char *pOut);
int str_is_utf8(char *str, int length);

char *copy_with_escape(char *out, char *in);

void copy_with_quoted_escape(char *out, int outlen, const char *in);

char *toLowerStr(char *str);
char *toUpperStr(char *str);

int StringEncrypt(const char *source, char *dst, int dstlen);
int StringDecrypt(const char *source, char *dst, int dstlen);

void string_trim_tail(char *str);

void string_trim_head(char *str);

void string_remove(char *str, char c);

int strFindNoSpace(const char *strToLkup);
int strFindSpace(const char *strToLkup);
const char *strGetFilename(const char *path);


/*
*   在字符串中查询特定字符位置索引
*   const char *str ，字符串
*   char c，要查找的字符
*/
int strFindchrIndex(const char *str, char c);

void safe_strcpy(char *dest, size_t dest_size, const char *src);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
