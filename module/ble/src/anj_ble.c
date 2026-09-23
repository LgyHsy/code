#include "anj_mw_comm.h"
#include "anj_bind.h"
#include "anj_ble.h"
#include "anj_net_provider.h"
#include "eventhub.h"

#include "queue_client.h"
#include "anj_sysmng.h"

static anj_thread_s s_stBleThread = {0};

static const anj_net_ble_ops s_stBleOps = {
    .init = anj_ble_init,
    .uninit = anj_ble_uninit,
    .recv_config = anj_ble_recv_config,
    .config_invalid = anj_ble_config_invalid,
    .config_pwd_err = anj_ble_config_pwd_err,
    .connect_fail = anj_ble_connect_fail,
    .p2p_ok = anj_ble_p2p_ok,
};

ANJ_LINK_KEEP(anj_keep_ble_provider);

__attribute__((constructor)) static void anj_ble_provider_register(void)
{
    anj_net_ble_provider_register(&s_stBleOps);
}

__attribute__((destructor)) static void anj_ble_provider_unregister(void)
{
    anj_net_ble_provider_unregister(&s_stBleOps);
}

static void anj_ble_msg_cb(uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0)
    {
        __ERR("invalid input!\n");
        return;
    }
    int i = 0;
    BLE_HST_MSG msg;
    DevInfo *pstDevInfo = getDevInfo();
    if (length > 0)
    {
        msg.msg_id = data[0];
        msg.len = length - 3;
        msg.buff = data + 3;
        __INFO("########msg_id:%d data length:%d #########\n", msg.msg_id, length);
        switch (msg.msg_id)
        {
        case BLE_INIT_DONE:
        {
            char ble_name[64] = {0};
            if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
            {
                snprintf(ble_name, sizeof(ble_name), "WTD_");
            }
            else
            {
                snprintf(ble_name, sizeof(ble_name), "Camera_");
            }
            char ble_gap_name[64] = {0}; //"camera01";
            char macBuf1[8] = {0};
            char macBuf2[8] = {0};
            char macBuf3[8] = {0};
            long int mac1 = 0;
            long int mac2 = 0;
            long int mac3 = 0;
            uint8_t scan_response_buff[128] = {0};
            uint8_t btaddr[6] = {0xEF, 0x00, 0x00, 0x03, 0x04, 0x05};
            uint8_t adv_data_buff[] = {0x18, 0x21, 0x00, 0x50, 0xEB, 0x37, 0x6F, 0x26,
                                       0x15, 0xB7, 0x17, 0x42, 0x56, 0x56, 0x58, 0x11,
                                       0x3D, 0x3E, 0x0B, 0x00, 0x4F, 0x4E, 0x1D, 0x00, 0x00};

            strncat(ble_name, pstDevInfo->sn + 10, strlen(pstDevInfo->sn) - 10);
            strncpy(ble_gap_name, ble_name, sizeof(ble_gap_name));
            strncpy(macBuf1, pstDevInfo->sn + 10, 2);
            strncpy(macBuf2, pstDevInfo->sn + 12, 2);
            strncpy(macBuf3, pstDevInfo->sn + 14, 2);
            __INFO("###ble_name:%s, macBuf:%s, %s, %s \n", ble_name, macBuf1, macBuf2, macBuf3);

            int nameLen = strlen(ble_name);
            scan_response_buff[0] = nameLen + 1;
            scan_response_buff[1] = 0x09;
            int n = 2;
            int m = 0;
            for (n = 2; n < nameLen + 2; n++)
            {
                int name_value = (int)ble_name[m];
                scan_response_buff[n] = name_value;
                //__ERR("m = %d, name_value: %d \n", m, name_value);
                m++;
            }

            mac1 = strtol(macBuf1, NULL, 16);
            mac2 = strtol(macBuf2, NULL, 16);
            mac3 = strtol(macBuf3, NULL, 16);
            btaddr[3] = mac1;
            btaddr[4] = mac2;
            btaddr[5] = mac3;

            __ERR("BLE  INITIATIVE  DONE\r\n");
            app_ble_set_bt_addr(btaddr);
            app_ble_gap_name_set_msg_send((uint8_t *)ble_gap_name, strlen(ble_gap_name));
            app_ble_adv_param_set_msg_send(160, 160, 6);
            app_ble_adv_data_set_msg_send(adv_data_buff, sizeof(adv_data_buff));
            app_ble_scan_response_set_msg_send(scan_response_buff, sizeof(scan_response_buff));
            // app_ble_set_scan_param(0x30,0xa0);

            // app_ble_scan_msg_start();
            // app_ble_set_lbh_print(0x10000000);
            app_ble_adv_start_msg_send();
        }
        break;
        case BLE_MSG_DONE:
            __INFO("BLE_CLEITN_MSG_SEND_DONE\r\n");
            break;
        case BLE_ADV_ENABLE:
            __INFO("BLE_ADV_ENABLE\r\n");
            break;
        case BLE_ADV_DISABLE:
            __INFO("BLE_ADV_DISABLE\r\n");
            break;
        case BLE_SCAN_ENABLE:
            __INFO("BLE_SCAN_ENABLE\r\n");
            break;
        case BLE_SCAN_DISABLE:
            __INFO("BLE_SCAN_DISABLE\r\n");
            break;
        case BLE_ADV_REPORT:
        {
            struct gapm_ext_adv_report_ind *ptr = (struct gapm_ext_adv_report_ind *)msg.buff;

            __INFO("BLE_ADV_REPORT\n");
            __INFO("actv_idx %x, info %x\n", ptr->actv_idx, ptr->info);
            __INFO("trans_addr.addr_type %x, addr %x,%x,%x,%x,%x,%x \n", ptr->trans_addr.addr_type, ptr->trans_addr.addr.addr[0], ptr->trans_addr.addr.addr[1], ptr->trans_addr.addr.addr[2], ptr->trans_addr.addr.addr[3], ptr->trans_addr.addr.addr[4], ptr->trans_addr.addr.addr[5]);
            __INFO("target_addr.addr_type %x, addr %x,%x,%x,%x,%x,%x \n", ptr->target_addr.addr_type, ptr->target_addr.addr.addr[0], ptr->target_addr.addr.addr[1], ptr->target_addr.addr.addr[2], ptr->target_addr.addr.addr[3], ptr->target_addr.addr.addr[4], ptr->target_addr.addr.addr[5]);
            __INFO("tx_pwr %x, rssi %d,phy_prim %x, phy_prim %x,phy_second %x,adv_sid %x,period_adv_intv %x\n", ptr->tx_pwr, ptr->rssi, ptr->phy_prim, ptr->phy_second, ptr->adv_sid, ptr->period_adv_intv);
            __INFO("length %d,data %x,%x,%x,%x,%x,%x,%x\n", ptr->length, ptr->data[0], ptr->data[1], ptr->data[2], ptr->data[3], ptr->data[4], ptr->data[5], ptr->data[6]);
        }
        break;
        case BLE_SMARTCONFIG_DATA_RECV:
        {
            __INFO("BLE_SMARTCONFIG_DATA_RECV\n");

            char msgtempBuf[16] = {0};
            char msgBuf[128] = {0};
            int bGetdata = 0;

            for (i = 3; i < length; i++)
            {
                if (i >= 3 + 16)
                {
                    //__ERR("[%02X] ", data[i]);
                    bGetdata = 1;

                    sprintf(msgtempBuf, "%c", data[i]);
                    strcat(msgBuf, msgtempBuf);
                }
            }
            __INFO("\r\n");
            __INFO("recv msgBuf:%s \n", msgBuf);

            if (bGetdata == 1)
            {
                anj_bind_data_proc(msgBuf, sizeof(msgBuf), BIND_TYPE_BLE);
            }
        }

        break;
        case BLE_CONNECTION_IND:
        {
            uint8_t conidx = msg.buff[0];
            struct ble_connection_ind *ptr = (struct ble_connection_ind *)(msg.buff + 1);
            __INFO("BLE_CONNECTION conhdl 0x%x,interval %d, peer_addr_type %d,addr: 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x \r\n",
                   ptr->conhdl, ptr->con_interval, ptr->peer_addr_type,
                   ptr->peer_addr.addr[0], ptr->peer_addr.addr[1], ptr->peer_addr.addr[2],
                   ptr->peer_addr.addr[3], ptr->peer_addr.addr[4], ptr->peer_addr.addr[5]);
            app_ble_set_data_len(conidx, 128, 0x200);
        }
        break;
        case BLE_DISCONNECTED:
        {
            struct ble_disconnect_ind *ptr = (struct ble_disconnect_ind *)msg.buff;
            __INFO("BLE_DISCONNECT conhdl 0x%x ,reason 0x%x \r\n", ptr->conhdl, ptr->reason);
            app_ble_adv_start_msg_send();
        }
        break;
        case BLE_CONNECTION_PARAM_UPDATE:
        {
            struct ble_param_update_req_ind *ptr = (struct ble_param_update_req_ind *)msg.buff;
            __INFO("BLE_CONNECTION_PARAM_UPDATE intv_min 0x%x ,intv_max 0x%x, latency 0x%x,time_out 0x%x\r\n",
                   ptr->intv_min, ptr->intv_max, ptr->latency, ptr->time_out);
        }
        break;
        case BLE_EXCHANGED_MTU_IND:
        {
            struct ble_mtu_changed_ind *ptr = (struct ble_mtu_changed_ind *)msg.buff;
            __INFO("BLE_EXCHANGED_MTU_IND mtu 0x%x ,seq_num 0x%x \r\n",
                   ptr->mtu, ptr->seq_num);
        }
        break;
        case BLE_PKT_SIZE_IND:
        {
            struct ble_pkt_size_ind *ptr = (struct ble_pkt_size_ind *)msg.buff;
            __INFO("BLE_PKT_SIZE_IND max_rx_octets 0x%x ,max_rx_time 0x%x ,max_tx_octets 0x%x ,max_tx_time 0x%x\r\n",
                   ptr->max_rx_octets, ptr->max_rx_time, ptr->max_tx_octets, ptr->max_tx_time);
        }
        break;
        case BLE_SERVICE_CHANGED_IND:
        {
            __INFO("BLE_SERVICE_CHANGED_IND\n");
        }
        break;
        case BLE_NTF_SENT_DONE:
        {
            __INFO("BLE_NTF_SENT_DONE\r\n");
            // app_ble_smartconfig_send_notification(test_buff,80);
        }
        break;
        case BLE_IND_SENT_DONE:
        {
            uint8_t status = msg.buff[0];
            __INFO("BLE_IND_SENT_DONE %d\r\n", status);
            if (status == 0)
            {
                uint8_t test_buff[512] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                                          0x00, 0x10, 0x00, 0x00, 0xc8, 0xfe, 0x00, 0x00};
                app_ble_smartconfig_send_indication(test_buff, 80);
            }
        }
        break;
        case BLE_CON_UPDATE_CMP_DONE:
        {
            struct ble_param_updated_ind *ptr = (struct ble_param_updated_ind *)msg.buff;
            __INFO("BLE_CON_UPDATE_CMP_DONE ,interval 0x%x, latency 0x%x, to 0x%x\r\n",
                   ptr->con_interval, ptr->con_latency, ptr->sup_to);
        }
        break;
        case BLE_HOGPRH_DATA_RECV:
        {
            struct hogprh_report_ind *ptr = (struct hogprh_report_ind *)msg.buff;
            __INFO("BLE_HOGPRH_DATA_RECV: hid_idx %d,report_idx %d,report_len %d\n",
                   ptr->hid_idx, ptr->report_idx, ptr->report.length);
        }
        break;
        case BLE_LTK_IND:
        {
            struct ble_ltk_ind *ptr = (struct ble_ltk_ind *)msg.buff;
            __INFO("BLE_LTK_IND\n");
            __INFO("ltk: \n");
            for (int i = 0; i < 16; i++)
            {
                __INFO("%02x ", ptr->ltk[i]);
            }
            __INFO("\n");
            __INFO("ediv: %d\n", ptr->ediv);
            __INFO("rand: \n");
            for (int i = 0; i < 8; i++)
            {
                __INFO("%02x ", ptr->randnb[i]);
            }
            __INFO("\n");
            __INFO("key size: %d\n", ptr->key_size);
        }
        break;
        default:
            break;
        }
    }

    return;
}

int anj_ble_msg_send(char *send_buff)
{
    int iRet = 0;
    if (send_buff == NULL || strlen(send_buff) == 0)
    {
        __ERR("input invalid!\n");
        return iRet;
    }
    uint8_t send_recv_buff[128] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                                   0x00, 0x10, 0x00, 0x00, 0xc8, 0xfe, 0x00, 0x00};
    int msg_len = strlen(send_buff) + 16;
    for (int i = 16; i < msg_len; i++)
    {
        int name_value = (int)send_buff[i - 16];
        send_recv_buff[i] = name_value;
    }
    iRet = app_ble_smartconfig_send_notification(send_recv_buff, msg_len);
    if (iRet == 0)
    {
        __INFO("send_buff:%s success.\n", send_buff);
    }
    else
    {
        __ERR("send_buff:%s failed, iRet:%d.\n", send_buff, iRet);
    }

    sleep(1);
    iRet = app_ble_smartconfig_update_read_value(send_recv_buff, msg_len);
    if (iRet == 0)
    {
        __INFO("send_buff:%s success.\n", send_buff);
    }
    else
    {
        __ERR("send_buff:%s failed, iRet:%d.\n", send_buff, iRet);
    }
    return iRet;
}

int anj_ble_recv_config()
{
    if (s_stBleThread.start == 0)
        return 0;
    return anj_ble_msg_send("recv ok");
}

int anj_ble_config_invalid()
{
    if (s_stBleThread.start == 0)
        return 0;
    return anj_ble_msg_send("config invaild");
}

int anj_ble_config_pwd_err()
{
    if (s_stBleThread.start == 0)
        return 0;
    return anj_ble_msg_send("password error");
}

int anj_ble_connect_fail()
{
    if (s_stBleThread.start == 0)
        return 0;
    return anj_ble_msg_send("connect fail");
}

int anj_ble_p2p_ok(char *p2pidBuf)
{
    int iRet = 0;
    if (s_stBleThread.start == 0 || p2pidBuf == NULL)
    {
        return iRet;
    }
    char sendbuf[64] = "p2p ok;";
    snprintf(sendbuf, sizeof(sendbuf), "p2p ok;%s", p2pidBuf);

    anj_ble_msg_send(sendbuf);

    app_ble_adv_stop_msg_send();
    app_ble_scan_msg_stop();
    app_ble_disconnect_msg_send(0);
    return iRet;
}

// void *lbh_msg_thread_handle(void *arg)
// {
//     __ERR("#########lbh_msg_thread_handle start.\n");
//     prctl(PR_SET_NAME, __func__);
//     pthread_detach(pthread_self());

//     int ret = -1;

//     while (!g_endloop)
//     {
//         if (0 == access("/tmp/lbh_recv_config_ok.flag", 0))
//         {
//         }
//         else if (0 == access("/tmp/lbh_p2p_ok.flag", 0))
//         {
//         }
//         else if (0 == access("/tmp/lbh_config_invaild.flag", 0))
//         {
//         }
//         else if (0 == access("/tmp/lbh_password_error.flag", 0))
//         {
//         }
//         else if (0 == access("/tmp/lbh_connect_fail.flag", 0))
//         {
//         }

//         usleep(500 * 1000);
//     }

//     __ERR("lbh_msg_thread_handle end.\n");

//     return NULL;
// }

static int anj_ble_thread(void *ctx, int *bStart)
{
    anj_mw_system("cd / && /opt/ch/lbh_server -p //opt/ch/ble_userconfig.json -s ble usb&");
    sleep(1);

    app_queue_reg_callback(anj_ble_msg_cb);
    lbh_client_socket_and_msg_cb_init();
    return 0;
}

int anj_ble_init()
{
    if (s_stBleThread.start)
    {
        __INFO("ble already init\n");
        return 0;
    }

    __INFO("ble init\n");
    int iRet = 0;
    s_stBleThread.bAutoDestroy = 1;
    strncpy(s_stBleThread.iThreadName, "anj_ble_thread", sizeof(s_stBleThread.iThreadName) - 1);
    s_stBleThread.iThreadjob.ctx = &s_stBleThread;
    s_stBleThread.iThreadjob.func = anj_ble_thread;
    iRet = anj_thread_task_create(&s_stBleThread);

    return iRet;
}

int anj_ble_uninit()
{
    __INFO("ble uninit\n");
    if (s_stBleThread.start == 0)
        return 0;

    anj_mw_system("killall lbh_server");
    anj_thread_task_destroy(&s_stBleThread, -1);
    return 0;
}