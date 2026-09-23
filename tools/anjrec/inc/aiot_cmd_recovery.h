#ifndef AIOT_CMD_RECOVERY_H
#define AIOT_CMD_RECOVERY_H

int anjrec_aiot_cmd_init(void);
void anjrec_aiot_cmd_uninit(void);
void anjrec_aiot_cmd_push(char *pBuf, unsigned int nBufLen, unsigned int sid);

#endif
