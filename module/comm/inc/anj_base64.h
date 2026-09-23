#ifndef ANJ_BASE64_H
#define ANJ_BASE64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int anj_base64_encode(uint8_t *source, uint32_t sourcelen, char *target, uint32_t targetlen);
int anj_base64_decode(const char *source, uint32_t sourcelen, uint8_t *target, uint32_t targetlen);

#ifdef __cplusplus
}
#endif


#endif /* BASE64_H */


