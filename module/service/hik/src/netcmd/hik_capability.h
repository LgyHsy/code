#ifndef __HIK_CAPABILITY_H__
#define __HIK_CAPABILITY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Build canned capability XML into buf. Returns XML length, or <0 on error. */
int hik_capability_build_xml(unsigned int capability_type, char *buf, int buf_len);

#ifdef __cplusplus
}
#endif

#endif
