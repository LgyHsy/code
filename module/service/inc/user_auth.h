#ifndef __USER_AUTH_H__
#define __USER_AUTH_H__

#ifdef __cplusplus
extern "C"
{
#endif

void DeleteUserSession(char *username);

void ClearUserSession(void);

int UserAuthInit();

int UserAuthLogin(char *username, char *password, char *group, char *sessionid);

int UserAuthLoginEx(int authmethod, char *vendorid,
					char *username, char *password, char *group, char *sessionid);
int UserAuthGetPassword(char *username, char *password);

int UserAuthUpdateSession(char *session);

int UserAuthCheckSession(char *session);

int UserAuthLogout(char *session);

int UserAuthAddUser(char *username, char *password, char *group, char *status);

int UserAuthEditUser(char *username, char *password, char *group, char *status, char *secureLogin);

int UserAuthAddOrEditUser(char *username, char *password, char *group, char *status);

int UserAuthDeleteUser(char *username);

int UserAuthAdminLoginGet();

int UserAuthGetUsernamePassword(char *username, int username_len, char *password, int password_len);

#ifdef __cplusplus
}
#endif

#endif
