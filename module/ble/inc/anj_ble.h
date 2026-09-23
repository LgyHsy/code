#ifndef __ANJ_BLE_H__
#define __ANJ_BLE_H__

int anj_ble_recv_config();
int anj_ble_config_invalid();
int anj_ble_config_pwd_err();
int anj_ble_connect_fail();
int anj_ble_p2p_ok(char *p2pidBuf);

int anj_ble_init();
int anj_ble_uninit();

#endif
