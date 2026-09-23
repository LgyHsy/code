#ifndef BASE64_H
#define BASE64_H

#include "sys_inc.h"

#ifdef __cplusplus
extern "C" {
#endif

int base64_encode(unsigned char *source, unsigned int sourcelen,
                  char *target, unsigned int targetlen);
int base64_decode(const char *source, unsigned char *target,
                  unsigned int targetlen);

/* anj_pri_cmd.c and other ipc2 modules use the _bm suffix variant */
#define base64_encode_bm(src, srclen, tgt, tgtlen) \
    base64_encode((unsigned char *)(src), (unsigned int)(srclen), (tgt), (unsigned int)(tgtlen))
#define base64_decode_bm(src, srclen, tgt, tgtlen) \
    base64_decode((src), (unsigned char *)(tgt), (unsigned int)(tgtlen))

#ifdef __cplusplus
}
#endif

#endif /* BASE64_H */
