#ifndef __WEBPOST_HANDLE_H__
#define __WEBPOST_HANDLE_H__

#ifdef __cplusplus
extern "C"
{
#endif

FormDataBoundary *http_get_form_firmware_boundary();

int http_get_login_passwd_error_count();
unsigned int http_get_login_passwd_error_locktime();

char* web_post_handle(const char* pSoapMsg, const char *path);

#ifdef __cplusplus
}
#endif

#endif