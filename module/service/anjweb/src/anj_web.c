#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"
#include "anj_config.h"
#include "anj_config_stream.h"
#include "anj_net.h"

#include "civetweb.h"
#include "http_def.h"
#include "http_handle.h"
#include "http_upload.h"

#include "anj_http.h"
#include "anj_record.h"
#include "webpost_handle.h"

#include "anj_web.h"
#include "anj_service_provider.h"
#include "eventhub.h"

#ifdef NO_SSL
#define TEST_WITHOUT_SSL
#undef USE_SSL_DH
#endif

#define TEST_WITHOUT_SSL "."

#define DOCUMENT_ROOT                       "/var/www/"
#define WEB_SERVER_REQUEST_BUFFER_SIZE      32768        // 32k

typedef struct
{
    char szUploadFileIdentity[128];
}HttpUploadFileDef;

HttpUploadFileDef g_uploadType[] =
{
    {"/IPC_FirmwareUpgrade"},
    {"/WebUploadConfig"},
    {"/PostFile_DANALE_ID"},
    {"/PostFile_CUSTOM"},
    {"/WebUploadFormFile"},
    {"/WebCertificateUploadFile"},
};

static struct mg_context *g_pstWebServerCtx = NULL;

static anj_thread_s s_AnjWebThread;

static const anj_service_provider_ops s_stWebProviderOps = {
    .provider_name = "web",
    .provider_type = ANJ_SERVICE_PROVIDER_WEB,
    .provider_priority = ANJ_SERVICE_PROVIDER_WEB,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_NONE,
    .init = anj_web_init,
    .uninit = anj_web_uninit,
    .alarm_event_notify = NULL,
    .audio_enc_change = NULL,
};

ANJ_LINK_KEEP(anj_keep_web_provider);

__attribute__((constructor)) static void anj_web_provider_register(void)
{
    anj_service_provider_register(&s_stWebProviderOps);
}

__attribute__((destructor)) static void anj_web_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stWebProviderOps);
}

#ifndef TEST_WITHOUT_SSL
int init_ssl(void *ssl_ctx, void *user_data)
{
    /* Add application specific SSL initialization */
    struct ssl_ctx_st *ctx = (struct ssl_ctx_st *)ssl_ctx;

#ifdef USE_SSL_DH
    /* example from https://github.com/civetweb/civetweb/issues/347 */
    DH *dh = get_dh2236();
    if (!dh)
        return -1;

    if (1 != SSL_CTX_set_tmp_dh(ctx, dh))
        return -1;
    DH_free(dh);

    EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ecdh)
        return -1;

    if (1 != SSL_CTX_set_tmp_ecdh(ctx, ecdh))
        return -1;
    EC_KEY_free(ecdh);

    __INFO("ECDH ciphers initialized\n");
#endif
    return 0;
}
#endif

int log_message(const struct mg_connection *conn, const char *message)
{
    __INFO("message:%s\r\n", message);
    return 1;
}

int anj_web_server_http_response(void *pInst, const char* buffer, int http_status_code)
{
    struct mg_connection *conn = (struct mg_connection *)pInst;
    if(NULL == conn)
    {
        return -1;
    }

    int len = 0;
    if( NULL != buffer)
    {
        len = strlen(buffer);
    }

    int result = mg_printf(conn,
                "HTTP/1.1 %d OK\r\n "
                "Server: gSOAP/2.8\r\n  "
                "Access-Control-Allow-Origin: *\r\n "
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Connection: keep-alive\r\n"
                "Content-Length:%d\r\n\r\n",
                (http_status_code == 1003) ? 200 : http_status_code, len);   //宇视LAPI接口用SOAP_FILE作为状态码，特殊处理下

    if (result == -1)
    {
        __ERR("result = %d :on error or the connection has been closed\n",result);
        return -1;
    }

    if(len > 0)
    {
        result =  mg_write(conn, buffer, len);
        if (result == -1)
        {
            __ERR("result = %d :on error or the connection has been closed\n",result);
            return -1;
        }
    }

    return 0;
}

/******************************************************************************\
 *
 *    Copy static page
 *
 \******************************************************************************/

int anj_web_server_http_copy_file(void *pInst, void *pmsgt, const char *name, const char *type)
{
    struct mg_connection *conn = (struct mg_connection *)pInst;
    if(NULL == conn)
    {
        return -1;
    }

    int is_attachment = 0;
    char fullname[256] = {0};
    if (name[0] == '/')
    {
        strcpy(fullname, name);
    }
    else
    {
        if((strlen(name) > strlen("playback/")) && (!strncmp(name, "playback/", strlen("playback/"))) )
        {
            strcpy(fullname, name + strlen("playback"));
            is_attachment = 1;
        }
        else
        {
            snprintf(fullname, sizeof(fullname), "/tmp/www/%s", name);
        }
    }

    __INFO("web http copy fullname:%s! is_attachment:%d!\n", fullname, is_attachment);

    if (access(fullname, F_OK) != 0)
    {
        strcpy(fullname, "/tmp/www/f404.html");
        if(access(fullname, F_OK) != 0)
        {
            __ERR("web server http can not found file%s!\n", fullname);
            return 404; /* return HTTP not found */
        }
    }

    mg_send_file(conn, fullname);
    return 0;
}

int anj_web_server_http_handle(struct mg_connection *conn, void * cbdata) 
{
    int iRet = 0;
    int result = 0;
    if (!conn)
    {
        __ERR("######### web server conn is NULL #########\r\n");
        return WEB_SERVER_RETURN_STATUS_CODE_CHECK_PARAMENT_ERROR;
    }

    char *pReadBuffer = NULL;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if(strcmp(ri->request_method, "POST") == 0 )
    {
        if(strncmp(ri->request_uri, "/onvif", strlen("/onvif")) == 0 ||
            strncmp(ri->request_uri, "/html/onvif/", strlen("/html/onvif/")) == 0||
            strncmp(ri->request_uri, "/Event", strlen("/Event")) == 0)
        {
            // __ERR("web return post uri:%s\n", ri->request_uri);
            return 1;
        }
    }

    const char *host = NULL;
    int headerIndex = 0;
    for(headerIndex = 0; headerIndex < ri->num_headers && headerIndex < MG_MAX_HEADERS; headerIndex++ )
    {
        if(strcasecmp(ri->http_headers[headerIndex].name, "Host") == 0)
        {
            host = ri->http_headers[headerIndex].value;
            break;
        }
    }

    __INFO("web request method:%s, uri:%s, host:%s, content_length:%lld\n",  
        ri->request_method, 
        ri->request_uri, 
        host ? host : "null", 
        ri->content_length);

    if(strcmp(ri->request_method, "GET") == 0)
    {
        if(ri->query_string != NULL && strlen(ri->query_string) > 0)
        {
            int buflen = strlen(ri->query_string) + strlen(ri->request_uri) + 2;
            char *buf = (char*)anj_mw_malloc(ANJ_ALIGN_UP(buflen, 4));
            if(NULL != buf)
            {
                sprintf(buf, "%s?%s", ri->request_uri, ri->query_string);
                http_get_proc(conn, NULL, buf, ri->remote_addr, host);
                anj_mw_free(buf);
                buf = NULL;
            }
        }
        else
        {
            http_get_proc(conn, NULL, ri->request_uri, ri->remote_addr, host);
        }

        return 1;
    }

    int buflen = WEB_SERVER_REQUEST_BUFFER_SIZE;
    pReadBuffer = (char*)anj_mw_malloc(buflen);
    if(NULL == pReadBuffer)
    {
        __ERR("pReadBuffer malloc failed!\n");
        return 1;
    }

    memset(pReadBuffer, 0, buflen);

    if(strcmp(ri->request_method, "POST") == 0)
    {
        unsigned int iIndex = 0;
        for(iIndex = 0; iIndex < sizeof(g_uploadType)/sizeof(g_uploadType[0]); iIndex++)
        {
            const char *pPostIdentity = g_uploadType[iIndex].szUploadFileIdentity;
            if(strncmp(ri->request_uri, pPostIdentity, strlen(pPostIdentity)) == 0)
            {
                __INFO("POST %s, ready to receive file, content length=%lld!\n", ri->request_uri, ri->content_length);

                //    debug_show_data_hex_ex((const unsigned char *)ri->conn_data,256, 1);
#if 0                
                result = mg_read(conn, pReadBuffer, buflen);
                DebugLog("read %d", result);  
                if(result <= 0 )
                {
                    web_server_http_response(conn, "", 200);
                }
#else
//WEB传输文件请求的时候，POST信令中携带了这些参数，在onvif中会作为首包去解析。
//但在civetweb取到request之后已经对参数进行解析，拿不到原始的buffer了
//因此需要重新封装这一包数据送给recv_form_file去解析文件。

                pReadBuffer[0] = 0;
                snprintf(pReadBuffer, buflen, "POST %s ", pPostIdentity);
                int headerIndex = 0;
                for(headerIndex = 0; headerIndex <ri->num_headers && headerIndex < MG_MAX_HEADERS; headerIndex++ )
                {
                    __INFO("web header %d: %s = %s\n", headerIndex, ri->http_headers[headerIndex].name, ri->http_headers[headerIndex].value);
                    sprintf(pReadBuffer + strlen(pReadBuffer), "%s:%s\r\n", ri->http_headers[headerIndex].name, ri->http_headers[headerIndex].value);
                }

                sprintf(pReadBuffer + strlen(pReadBuffer), "\r\n");
                result = strlen(pReadBuffer);
                __INFO("pReadBuffer:%s\n", pReadBuffer);  
#endif
                char szPostPath[256] = {0};
                snprintf(szPostPath, sizeof(szPostPath), "POST %s", pPostIdentity);

                int isFileEnd = -1;
                char md5_str[64] = {0};

                FormDataBoundary *ptsBoundary = http_get_form_firmware_boundary();
                ptsBoundary->socket = 0;

                int datalen = result;
                isFileEnd = http_recv_form_file(conn, szPostPath, ptsBoundary, md5_str, (int)conn, pReadBuffer, buflen, &datalen);
                __INFO("recv file isFileEnd=%d\n", isFileEnd);  

                result = 0;
                int nReadLen = 0;
                while(isFileEnd != 1)
                {
                    result = mg_read(conn, pReadBuffer, buflen);
                    __INFO("mg read %d, total=%d\n", result, nReadLen);  
                    if( result <= 0)
                        break;
    //                debug_show_data_hex_ex((const unsigned char*)pReadBuffer, result<64?result:64, 0);

                    datalen= result;
                    isFileEnd = http_recv_form_file(conn, szPostPath, ptsBoundary, md5_str, (int)conn, pReadBuffer, buflen, &datalen);

                    nReadLen += result;
                }

                iRet = 1;
                goto __quickexit;
            }
        }
        
        result = mg_read(conn, pReadBuffer, buflen);
        if (result < 0 )
        {
            __ERR("mg result=%d : read error. No more data could be read from the connection or connection has been closed by peer!\n", result);
            iRet = -1;
            goto __quickexit;
        }

        __INFO("pReadBuffer:%s\n", pReadBuffer);

        char szMsgBuffer[256] = {0};
        snprintf(szMsgBuffer, sizeof(szMsgBuffer)-1, "POST %s", ri->request_uri);
        
        if(ri->query_string != NULL && strlen(ri->query_string) > 0 )
        {
            int buflen = strlen(ri->query_string) + strlen(ri->request_uri) + 2;
            char *buf = (char*)anj_mw_malloc(ANJ_ALIGN_UP(buflen, 4));
            if(NULL != buf)
            {
                snprintf(buf, buflen, "%s?%s", ri->request_uri, ri->query_string);
                http_post_proc(conn, NULL, buf, szMsgBuffer, pReadBuffer, ri->remote_addr, host, anj_web_server_http_response);
                anj_mw_free(buf);
                buf = NULL;
            }
        }
        else
        {
            http_post_proc(conn, NULL, ri->request_uri, szMsgBuffer, pReadBuffer, ri->remote_addr, host, anj_web_server_http_response);
        }

        iRet = 1;
        goto __quickexit;
    }
    else if(strcmp(ri->request_method, "PUT") == 0)
    {
        result = mg_read(conn, pReadBuffer, buflen);
        if (result < 0)
        {
            __ERR("mg result=%d : read error. No more data could be read from the connection or connection has been closed by peer\n",result);
            iRet = -1;
            goto __quickexit;
        }

//        DebugLog("%s", pReadBuffer);

        char szMsgBuffer[256] = {0};
        snprintf(szMsgBuffer, sizeof(szMsgBuffer) - 1, "PUT %s", ri->request_uri);

        if(ri->query_string != NULL && strlen(ri->query_string) > 0 )
        {
            int buflen = strlen(ri->query_string) + strlen(ri->request_uri) + 2;
            char *buf = (char*)anj_mw_malloc(ANJ_ALIGN_UP(buflen, 4));
            if(NULL != buf)
            {
                sprintf(buf, "%s?%s", ri->request_uri, ri->query_string);
                http_put_proc(conn, buf, szMsgBuffer, pReadBuffer, ri->remote_addr, host, anj_web_server_http_response);
                anj_mw_free(buf);
                buf = NULL;
            }
        }
        else
        {
            http_put_proc(conn, ri->request_uri, szMsgBuffer, pReadBuffer, ri->remote_addr, host, anj_web_server_http_response);
        }
        
        iRet = 1;
        goto __quickexit;
    }

    result = mg_read(conn, pReadBuffer, buflen);
    if (result < 0)
    {
        __ERR("mg result=%d : read error. No more data could be read from the connection or connection has been closed by peer\n", result);
        iRet = -1;
        goto __quickexit;
    }

    debug_show_data_hex_ex((const unsigned char*)pReadBuffer, result < 64 ? result : 64, 0);

    result = mg_printf(conn,
              "HTTP/1.1 %d OK\r\n "
              "Server: gSOAP/2.8\r\n  "
              "Access-Control-Allow-Origin: *\r\n "
              "Content-Type: text/plain; charset=utf-8\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length:%d\r\n\r\n", 200, 0);
    if (result == -1)
    {
        __ERR("mg result=%d : on error or the connection has been closed\n",result);
        iRet = -1;
        goto __quickexit;
    }

__quickexit:
    if(NULL != pReadBuffer)
    {
        anj_mw_free(pReadBuffer);
        pReadBuffer = NULL;
    }

    return iRet;
}

int anj_web_server_service_init(const char **web_server_options, struct mg_context **ctx)
{
    struct mg_callbacks callbacks;
    int result = 0;

    __INFO("####### simple webserver start version:%s #######\n", CIVETWEB_VERSION);

    /* Start CivetWeb web server */
    memset(&callbacks, 0, sizeof(callbacks));
#ifndef TEST_WITHOUT_SSL
    if (!mg_check_feature(2))
    {
        __ERR("Error: Embedded example built with SSL support, but civetweb library build without.\n");
        //fprintf(stderr,
            //"Error: Embedded example built with SSL support, "
            //"but civetweb library build without.\n");
        result = 1;
    }
#endif

    if (result)
    {
        __ERR("Cannot start CivetWeb - inconsistent build.\n");
        //fprintf(stderr, "Cannot start CivetWeb - inconsistent build.\n");
        return EXIT_FAILURE;
    }

#ifndef TEST_WITHOUT_SSL
    callbacks.init_ssl = init_ssl;
#endif

    callbacks.log_message = log_message;
    *ctx = mg_start(&callbacks, 0, web_server_options);
    /* Check return value: */
    if (*ctx == NULL)
    {
        //fprintf(stderr, "Cannot start web server - mg_start failed.\n");
        __ERR("Cannot start web server - mg_start failed.\n");
        return EXIT_FAILURE;
    }

    mg_set_request_handler(*ctx, "", anj_web_server_http_handle, NULL);

    return EXIT_SUCCESS;
}


int anj_web_server_service_deinit(struct mg_context *ctx)
{
    if (ctx)
    {
        mg_stop(ctx);
        ctx = NULL;
        return EXIT_SUCCESS;
    } 
    else
    {
        __ERR("web_server_service_deinit ctx is NULL\n");
        return EXIT_FAILURE;
    }
}

static int anj_web_ctrl_thread(void *ctx, int *bStart)
{
    if (NULL == bStart)
    {
        __ERR("this thread bstart is null!\n");
        return -1;
    }

    MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (pStreamCfg == NULL)
    {
        __ERR("getMediaStreamConfig failed\n");
        return -1;
    }

    if (pStreamCfg->rtspConfig.videoPort <= 0 || pStreamCfg->rtspConfig.videoPort > 65535)
    {
        __ERR("invalid video port:%d!\n", pStreamCfg->rtspConfig.videoPort);
        return -1;
    }

    if (pStreamCfg->webConfig.webPort <= 0 || pStreamCfg->webConfig.webPort > 65535)
    {
        __ERR("invalid web port:%d!\n", pStreamCfg->webConfig.webPort);
        return -1;
    }

    int ip_ready = 0;
    while (bStart && *bStart == 1)
    {
        ip_ready = anj_net_wire_and_wireless_ip_ready_check();
        if (ip_ready)
        {
            __INFO("ip ready!\n");
            break;
        }
        usleep(200*1000);
    }

    if (1 == *bStart)
    {
        if (pStreamCfg->webConfig.enable_onvif == 1 || 
            pStreamCfg->webConfig.enable_web == 0)
        {
            __INFO("skip CivetWeb: onvif=%d web=%d\n",
                   pStreamCfg->webConfig.enable_onvif,
                   pStreamCfg->webConfig.enable_web);
            return 0;
        }

        //HTTP的相关解析/处理都放到了libcgibin中
        anj_http_init(anj_web_server_http_response, anj_web_server_http_copy_file);

        char web_server_port[16] = {0};
        snprintf(web_server_port, sizeof(web_server_port), "%d", pStreamCfg->webConfig.webPort);
        const char *options[] = 
        {
#if !defined(NO_FILES)
                "document_root",
                DOCUMENT_ROOT,
#endif
                "listening_ports",
                web_server_port,
                "request_timeout_ms",
                "10000",
                "error_log_file",
                "/tmp/error.log",
#ifdef USE_WEBSOCKET
                                                                                                                                        "websocket_timeout_ms",
                "3600000",
#endif
#ifndef TEST_WITHOUT_SSL
                "ssl_certificate",
                "/mnt/nand/ipc/ipc/smart_ipcamera/cmake-build-debug-smart_ipcamera-mstar/source/web_server/server.pem",
                "ssl_protocol_version",
                "3",
                "ssl_cipher_list",
#ifdef USE_SSL_DH
                "ECDHE-RSA-AES256-GCM-SHA384:DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256",
#else
                "DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256",
#endif
#endif
                "enable_auth_domain_check",
                "no",
                0
        };

        if(access((const char *) anj_web_server_service_init((const char **) options, &g_pstWebServerCtx), F_OK) == 0)
        {
            __ERR("web_server_service_init error\n");
        }
    }
    __INFO("WEB Server started \n");

    return 0;
}

static int anj_web_start(void)
{
    int iRet = 0;

    anj_http_web_file_init();

    memset(&s_AnjWebThread, 0, sizeof(anj_thread_s));
    s_AnjWebThread.bAutoDestroy = 1;
    strncpy(s_AnjWebThread.iThreadName, "anj_webctrl_thread", sizeof(s_AnjWebThread.iThreadName) - 1);
    s_AnjWebThread.iThreadjob.ctx = &s_AnjWebThread;
    s_AnjWebThread.iThreadjob.func = anj_web_ctrl_thread;
    iRet = anj_thread_task_create(&s_AnjWebThread);
    if (iRet)
    {
        __ERR("create anj_webctrl_thread failed\n");
        return iRet;
    }

    return 0;
}

static int anj_web_stop(void)
{
    anj_thread_task_destroy(&s_AnjWebThread, -1);

    anj_web_server_service_deinit(g_pstWebServerCtx);
    g_pstWebServerCtx = NULL;

    return 0;
}

static void anj_web_on_restart(EventResult *event_result, void *data)
{
    (void)data;
    if (event_result)
    {
        event_result->ret = 0;
    }

    __INFO("web restart\n");
    anj_web_stop();
    usleep(500 * 1000);
    anj_web_start();
}

int anj_web_init()
{
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WEB_RESTART, anj_web_on_restart);
    return anj_web_start();
}

int anj_web_uninit()
{
    anj_web_stop();
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, EVENTHUB_WEB_RESTART, anj_web_on_restart);
    return 0;
}



