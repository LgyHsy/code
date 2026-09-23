#ifndef __ANJ_H5_AUTH_H__
#define __ANJ_H5_AUTH_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 on success, 0 on failure, -1 if timestamp invalid (unused). */
int anj_h5_check_user_auth(const char *username, const char *password, long long time_stamp);

#ifdef __cplusplus
}
#endif

#endif
