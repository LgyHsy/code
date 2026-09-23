#ifndef __SN_UUID_H__
#define __SN_UUID_H__

#define MAGIC1_UUID (0x10402DE9 + 0x10)
#define MAGIC2_UUID (0xA0E15A38 + 0x20)
#define MAGIC3_UUID (0xE90140A0 + 0x88)
#define MAGIC4_UUID (0xA2DB0200 + 0x99)

int save_uuid_to_ubootenv(const char *cmd);
int GetSoftUUIDData(const char* buf, int inbuflen, char *output_decrypt, int outbuflen);
int write_uuid(unsigned char *buf, int len);

#endif // __SN_UUID_H__