#ifndef __SN_UTILS_H__
#define __SN_UTILS_H__

#include <netinet/in.h>
#include <stddef.h>

#define SECTOR_SIZE     (4 * 1024)
#define SPI_ENV_SIZE    (4 * 1024)

int read_sndata_by_file(char* pBuffer, int nSize);
int write_sndata_by_file(char* pBuffer, int nSize);

int soft_enc_xml_cmd_parse(const char *xmlBuf);
int soft_enc_xml_serial_data_parse(const char *xmlBuf, char *serialid, int buflen1, char *data, int buflen2, unsigned int *nType);
int soft_enc_xml_uuid_data_get(const char *xmlBuf, char *identity, int buflen1, char *data, int buflen2, char *checksum, int buflen3);
int soft_enc_xml_sn_data_parse(const char *xmlBuf, char *cameraid, int buflen1, char *data, int buflen2, char *checksum, int buflen3, int *version);

int broardcast_send_request(int sockfd, int nServerPort, struct sockaddr_in remote, char *send_buf);

int format_utc_timestamp(unsigned long long timestamp_ms, char *out_str, size_t str_len);

int check_ubootargs(const char *cmd, int print);
int check_ubootargs2(const char *name, const char* value, int print);

int WriteMtdData(unsigned char *buf, int len, const char* mtd_block, int from_sect, int end_sect, int total_sects);

int WriteUboot(char *buf, int len);
int WriteUboot_byFile(char *param);

#endif

