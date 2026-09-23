/** ===========================================================================
 * @file user_auth.c
 *
 * @path $(IPNCPATH)\sys_adm\system_server
 *
 * @desc
 * .
 * Copyright (c) Topsee Electronic Tech Co.,Ltd. 2008
 *
 * Use of this software is controlled by the terms and conditions found
 * in the license agreement under which this software has been supplied
 *
 * =========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_crypt.h"
#include "anj_mw_time.h"
#include "anj_config.h"
#include "anj_pri.h"
#include "user_auth.h"

#define ERROR_TOO_MANY_SESSIONS -1
#define ERROR_USERNAME_NOT_FOUND -2
#define ERROR_BAD_PASSWORD -3
#define ERROR_USERACCOUNT_DISABLE -4
#define ERROR_USER_ALREAD_LOGIN -5
#define ERROR_SESSION_NOT_FOUND -6
#define ERROR_USERNAME_DUPLICATED -7
#define ERROR_TOO_MANY_USERS -8
#define ERROR_LAST_ADMINISTRATOR -9

#define ERROR_TOO_MANY_SESSIONS -1
#define ERROR_USERNAME_NOT_FOUND -2
#define ERROR_BAD_PASSWORD -3
#define ERROR_USERACCOUNT_DISABLE -4
#define ERROR_USER_ALREAD_LOGIN -5
#define ERROR_SESSION_NOT_FOUND -6
#define ERROR_USERNAME_DUPLICATED -7
#define ERROR_TOO_MANY_USERS -8
#define ERROR_LAST_ADMINISTRATOR -9
#define MAX_FILE_SIZE 1048576

#define MAX_USER_SESSION_COUNT 20
#define MAX_SESSION_TIMEOUT_SECOND 2400

#define USER_ADMIN_LOGIN_FLAG DATA_BLOCK_MOUNT_PATH "/user_admin_login.flag"
#define VENDOR_ID_FILE_PATH AJ_APP_PATH "/vendor_id.vip"

typedef struct
{
	char username[ACCOUNT_NAME_MAX_LEN];
	char password[ACCOUNT_PASSWORD_MAX_LEN];
	char group[GROUP_NAME_MAX_LEN];
	char session[MAX_USER_SESSION];
	struct timeval last_session_time;
} USER_SESSION_ITEM;

typedef struct
{
	USER_SESSION_ITEM session_list[MAX_USER_SESSION_COUNT];
	int session_count;
} USER_SESSION_LIST;

typedef struct
{
	int fileLength;
	int fileCrc;	 // 后面所有数据的CRC校验
	int fileVersion; // 不同fileVersion，后面的格式不同，默认是0
	unsigned char keyData[8];
	// 加密密钥，随机产生，保存时，后面所有数据按字节进行异或，读取时，按字节进行异或可以恢复。不同fileVersion, 算法可能不同。
	unsigned char vendorName[200]; // 厂商名称
	unsigned char vendorId[40];	   // 厂商ID的MD5
	unsigned short startYear;
	unsigned char startMonth;
	unsigned char startDay;	  // 授权开始的时间，年月日
	unsigned long validDays;  // 授权有效天数
	unsigned char padding[0]; // 不足2K，补足2K字节，任意数据。
} VENDOR_ID_FILE;

static int g_adminCnt = 0;
static int g_user_admin_login = 0;
static UserConfig *pUserConfig = NULL;
static USER_SESSION_LIST g_user_session_list;

static int LoadVendorId(char *vendor_id)
{
	VENDOR_ID_FILE *vendorFile;
	char buf[4096] = {0};
	int i = 0, j = 0;

	strcpy(vendor_id, "");

	anj_mw_read_file_limit_len(VENDOR_ID_FILE_PATH, buf, sizeof(buf));
	if (strlen(buf) > sizeof(VENDOR_ID_FILE))
	{
		vendorFile = (VENDOR_ID_FILE *)buf;

		// verify file length
		if (vendorFile->fileLength != strlen(buf))
		{
			__ERR("file length verify failed!!!\n");

			return -1;
		}
		__ERR("file length verify OK!\n");

		// verify crc
		if (vendorFile->fileCrc != anj_crc32_update(0, (unsigned char *)buf + 8, strlen(buf) - 8))
		{
			__ERR("file crc verify failed!!!\n");
			return -2;
		}
		__ERR("file crc verify OK!\n");

		// verify file version
		if (!vendorFile->fileVersion)
		{
			__ERR("file version not supported!!!\n");

			return -3;
		}
		__ERR("file version verify OK!\n");

		char *ptr = buf;
		for (i = 20; i < strlen(buf); i += 8)
		{
			for (j = 0; j < 8; j++)
				*(ptr + i + j) ^= vendorFile->keyData[j];
		}

		__ERR("vendor_id: %s, vendor_name: %s, start date: %04d-%02d-%02d\n",
			  vendorFile->vendorId,
			  vendorFile->vendorName,
			  vendorFile->startYear,
			  vendorFile->startMonth,
			  vendorFile->startDay);

		strcpy(vendor_id, (char *)vendorFile->vendorId);
	}

	return 0;
}

static int DeleteTimeOutSession(void)
{
	if (g_user_session_list.session_count == 0)
		return 0;

	int idx;
	struct timeval tv;
	int timeout_arr[g_user_session_list.session_count];
	int timeoutCnt = 0;

	SystemGetTimeofRun(&tv, 0); // SystemGetTimeofRun(&tv, 0);
	for (idx = 0; idx < g_user_session_list.session_count; idx++)
	{
		if (tv.tv_sec - g_user_session_list.session_list[idx].last_session_time.tv_sec > MAX_SESSION_TIMEOUT_SECOND)
		{
			timeout_arr[idx] = 1;
			timeoutCnt++;
		}
		else
		{
			timeout_arr[idx] = 0;
			;
		}
	}

	if (timeoutCnt == 0)
		return 0;

	int curPos = 0;
	for (idx = 0; idx < g_user_session_list.session_count; idx++)
	{
		if (timeout_arr[idx] == 0)
		{
			g_user_session_list.session_list[curPos] = g_user_session_list.session_list[idx];
			curPos++;
		}
	}

	g_user_session_list.session_count = curPos;

	return 0;
}

void DeleteUserSession(char *username)
{
	int i, j;

	for (i = 0; i < g_user_session_list.session_count; i++)
	{
		if (!strcasecmp(g_user_session_list.session_list[i].username, username))
		{
			__ERR("remove the existed session of user: %s\n", username);

			// remove this seesion
			if (i != g_user_session_list.session_count - 1)
			{
				for (j = i; j < g_user_session_list.session_count - 1; j++)
				{
					memcpy(&(g_user_session_list.session_list[j]), &(g_user_session_list.session_list[j + 1]), sizeof(USER_SESSION_ITEM));
				}
			}

			g_user_session_list.session_count--;
			break;
		}
	}
}

void ClearUserSession(void)
{
	__ERR("clear all user session!!!\n");

	g_user_session_list.session_count = 0;
}

int UserAuthInit()
{
	int i = 0;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	g_user_session_list.session_count = 0;
	pUserConfig = &pstSystemConfig->userCfg;

	g_adminCnt = 0;

	if (anj_mw_file_exists(USER_ADMIN_LOGIN_FLAG))
	{
		__INFO("g_user_admin_login = 1!!!\n");
		g_user_admin_login = 1;
	}

	for (i = 0; i < pUserConfig->count; i++)
	{
		__INFO("user %02d: name=%s, password=%s, group=%s, status=%s\n",
			   i,
			   pUserConfig->accounts[i].userName,
			   pUserConfig->accounts[i].password,
			   pUserConfig->accounts[i].group.groupName,
			   pUserConfig->accounts[i].status);

		if (strcasecmp(pUserConfig->accounts[i].group.groupName, "Administrator") == 0)
		{
			if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
			{
				g_adminCnt++;
			}
		}
	}

	__INFO("adminCnt = %d\n", g_adminCnt);

	if (g_adminCnt <= 0)
	{
		__WARN("admin count <= 0, added default admin account.\n");
		strcpy(pUserConfig->accounts[pUserConfig->count].userName, "admin");
		strcpy(pUserConfig->accounts[pUserConfig->count].password, "admin");
		strcpy(pUserConfig->accounts[pUserConfig->count].group.groupName, "Administrator");
		strcpy(pUserConfig->accounts[pUserConfig->count].status, "Enable");

		pUserConfig->count++;
		g_adminCnt++;
		anj_config_system_save(pstSystemConfig);
	}

	char vendor_id[100] = "";
	LoadVendorId(vendor_id);

	// save vendor_id to /tmp/vendor_id_md5.dat, and then other process can use it
	if (strlen(vendor_id))
	{
		anj_mw_create_file("/tmp/vendor_id_md5.dat", vendor_id);
		anj_mw_create_file("/tmp/vendor_check.dat", vendor_id);
	}

	return 0;
}

int UserAuthLogin(char *username, char *password, char *group, char *sessionid)
{
	DeleteTimeOutSession(); // first to call

	struct timeval tv;
	struct tm ptm;
	int i;

	SystemGetTimeofRun(&tv, NULL);
	SystemLocalTime(&ptm);
	srand((unsigned)tv.tv_sec);

	// check vendorid support
	char vendor_id[256] = {0};
	anj_mw_read_file_limit_len("/tmp/vendor_id_md5.dat", vendor_id, sizeof(vendor_id));

	if (strlen(vendor_id))
	{
		__ERR("we have vendor_id, basic login disabled!!!\n");
		return ERROR_BAD_PASSWORD;
	}

	// if the name is in sesssion list, just return the session
	for (i = 0; i < g_user_session_list.session_count; i++)
	{
		if ((strcasecmp(g_user_session_list.session_list[i].username, username) == 0) && (strcmp(g_user_session_list.session_list[i].password, password) == 0))
		{
			// find existed session
			__ERR("find one existed session, index=%d\n", i);

			// update sesstion time
			SystemGetTimeofRun(&tv, NULL);
			memcpy(&(g_user_session_list.session_list[i].last_session_time), &tv, sizeof(struct timeval));

			memcpy(group, g_user_session_list.session_list[i].group, GROUP_NAME_MAX_LEN);
			memcpy(sessionid, g_user_session_list.session_list[i].session, MAX_USER_SESSION);

			return 0;
		}
	}

	// if(g_user_session_list.session_count >= MAX_USER_SESSION)
	if (g_user_session_list.session_count >= MAX_USER_SESSION_COUNT)
	{
		__ERR("ERROR_TOO_MANY_SESSIONS\n");

		return ERROR_TOO_MANY_SESSIONS;
	}

	if (g_user_admin_login)
	{
		if (!strcasecmp(username, "user"))
		{
			strcpy(group, "VIEWER");
			sprintf(sessionid, "%04d%02d%02d%02d%02d%02d_%08x%08x",
					1900 + ptm.tm_year,
					ptm.tm_mon + 1,
					ptm.tm_mday,
					ptm.tm_hour,
					ptm.tm_min,
					ptm.tm_sec,
					rand() * rand(),
					rand() * rand());

			int sindex = g_user_session_list.session_count;
			__INFO("sindex = %d, sessionid len = %d\n", sindex, strlen(sessionid));

			strcpy(g_user_session_list.session_list[sindex].username, username);
			strcpy(g_user_session_list.session_list[sindex].password, "admin");
			strcpy(g_user_session_list.session_list[sindex].group, group);
			strcpy(g_user_session_list.session_list[sindex].session, sessionid);

			memcpy(&(g_user_session_list.session_list[sindex].last_session_time), &tv, sizeof(struct timeval));
			g_user_session_list.session_count++;

			__INFO("session count=%d\n", g_user_session_list.session_count);

			return 0;
		}
	}

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			if (strcmp(pUserConfig->accounts[i].password, password) == 0)
			{
				if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
				{
					__ERR("account active, login OK!!!\n");

					strcpy(group, pUserConfig->accounts[i].group.groupName);
					sprintf(sessionid, "%04d%02d%02d%02d%02d%02d_%08x%08x",
							1900 + ptm.tm_year,
							ptm.tm_mon + 1,
							ptm.tm_mday,
							ptm.tm_hour,
							ptm.tm_min,
							ptm.tm_sec,
							rand() * rand(),
							rand() * rand());

					int sindex = g_user_session_list.session_count;
					__INFO("sindex = %d, sessionid len = %d\n", sindex, strlen(sessionid));

					strcpy(g_user_session_list.session_list[sindex].username, username);
					strcpy(g_user_session_list.session_list[sindex].password, password);
					strcpy(g_user_session_list.session_list[sindex].group, group);
					strcpy(g_user_session_list.session_list[sindex].session, sessionid);

					SystemGetTimeofRun(&tv, NULL);
					memcpy(&(g_user_session_list.session_list[sindex].last_session_time), &tv, sizeof(struct timeval));
					g_user_session_list.session_count++;

					__INFO("session count=%d\n", g_user_session_list.session_count);

					return 0;
				}
				else
				{
					__ERR("account: %s is disabled, login failed!!!\n", username);
					return ERROR_USERACCOUNT_DISABLE;
				}
			}
			else
			{
				__ERR("ERROR_BAD_PASSWORD: %s\n", password);
				return ERROR_BAD_PASSWORD;
			}
		}
	}

	__ERR("username: %s not found.\n", username);
	return ERROR_USERNAME_NOT_FOUND;
}

int UserAuthLoginEx(int authmethod, char *vendorid,
					char *username, char *password, char *group, char *sessionid)
{
	DeleteTimeOutSession(); // first to call

	int i = 0;
	int iRet = 0;
	struct timeval tv;
	struct tm ptm;

	SystemGetTimeofRun(&tv, NULL);
	SystemLocalTime(&ptm);
	srand((unsigned)tv.tv_sec);

	char vendor_id[256] = {0};
	anj_mw_read_file_limit_len("/tmp/vendor_id_md5.dat", vendor_id, sizeof(vendor_id));

	if (strlen(vendor_id))
	{
		if (strlen(vendorid))
		{
			if (!strcmp(vendorid, vendor_id))
			{
				__INFO("vendor_id match!!!\n");
			}
			else
			{
				__ERR("vendor_id not match!!!\n");
				return ERROR_BAD_PASSWORD;
			}
		}
	}
	else
	{
		if (strlen(vendorid))
		{
			__ERR("we don't have vendor_id !!!\n");
			return ERROR_BAD_PASSWORD;
		}
	}

	char passwordMD5[40] = {0};
	char passwordMD5MD5[40] = {0};

	// if the name is in sesssion list, just return the session
	for (i = 0; i < g_user_session_list.session_count && i < MAX_USER_SESSION_COUNT; i++)
	{
		if (!strcasecmp(g_user_session_list.session_list[i].username, username))
		{
			our_md5_encode(passwordMD5, (unsigned char *)g_user_session_list.session_list[i].password,
						   strlen(g_user_session_list.session_list[i].password));
			passwordMD5[32] = 0;

			our_md5_encode(passwordMD5MD5, (unsigned char *)passwordMD5, strlen(passwordMD5));
			passwordMD5MD5[32] = 0;

			if ((!strcmp(vendor_id, vendorid) && strlen(vendor_id)) || (authmethod == 2))
			{
				iRet = !strcmp(passwordMD5, password) || !strcmp(passwordMD5MD5, password);
			}
			else // authmethod <=1, password use text
			{
				__ERR("22loginpass: [%s], pass: [%s], md5: [%s], md5md5: [%s]\n",
					  password,
					  g_user_session_list.session_list[i].password,
					  passwordMD5, passwordMD5MD5);

				iRet = !strcmp(g_user_session_list.session_list[i].password, password) ||
					   !strcmp(passwordMD5, password) || !strcmp(passwordMD5MD5, password);
			}

			if (iRet)
			{
				// find existed session
				__ERR("find one existed session, index=%d\n",i);

				// update sesstion time
				SystemGetTimeofRun(&tv, NULL);
				memcpy(&(g_user_session_list.session_list[i].last_session_time), &tv, sizeof(struct timeval));

				memcpy(group, g_user_session_list.session_list[i].group, GROUP_NAME_MAX_LEN);
				memcpy(sessionid, g_user_session_list.session_list[i].session, MAX_USER_SESSION);

				return 0;
			}
		}
	}

	// if(g_user_session_list.session_count >= MAX_USER_SESSION)
	if (g_user_session_list.session_count >= MAX_USER_SESSION_COUNT)
	{
		__ERR("ERROR_TOO_MANY_SESSIONS\n");

		return ERROR_TOO_MANY_SESSIONS;
	}

	if (g_user_admin_login)
	{
		if (!strcasecmp(username, "user") && authmethod == 1)
		{
			strcpy(group, pUserConfig->accounts[i].group.groupName);
			sprintf(sessionid, "%04d%02d%02d%02d%02d%02d_%08x%08x",
					1900 + ptm.tm_year,
					ptm.tm_mon + 1,
					ptm.tm_mday,
					ptm.tm_hour,
					ptm.tm_min,
					ptm.tm_sec,
					rand() * rand(),
					rand() * rand());

			//__ERR("sessionid=%s\n",sessionid);

			int sindex = g_user_session_list.session_count;
			__INFO("sindex = %d, sessionid len = %d\n", sindex, strlen(sessionid));

			our_md5_encode(passwordMD5, (unsigned char *)"admin", 6);
			passwordMD5[32] = 0;

			strcpy(g_user_session_list.session_list[sindex].username, username);	// added by XXX 20090217
			strcpy(g_user_session_list.session_list[sindex].password, passwordMD5); // password); //added by XXX 20090225
			strcpy(g_user_session_list.session_list[sindex].group, group);			// added by XXX 20090217
			strcpy(g_user_session_list.session_list[sindex].session, sessionid);

			SystemGetTimeofRun(&tv, NULL);
			memcpy(&(g_user_session_list.session_list[sindex].last_session_time), &tv, sizeof(struct timeval));
			g_user_session_list.session_count++;
			__INFO("session count=%d\n", g_user_session_list.session_count);

			return 0;
		}
	}

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (!strcasecmp(pUserConfig->accounts[i].userName, username))
		{
			__INFO("name found.\n");

			our_md5_encode(passwordMD5, (unsigned char *)pUserConfig->accounts[i].password, strlen(pUserConfig->accounts[i].password));
			passwordMD5[32] = 0;

			our_md5_encode(passwordMD5MD5, (unsigned char *)passwordMD5, strlen(passwordMD5));
			passwordMD5MD5[32] = 0;

			if ((!strcmp(vendor_id, vendorid) && strlen(vendor_id)) || (authmethod == 2))
			{
				iRet = !strcmp(passwordMD5, password) || !strcmp(passwordMD5MD5, password);
			}
			else // authmethod <=1, password use text
			{
				/*__ERR("2222loginpass: [%s], pass: [%s], md5: [%s], md5md5: [%s]\n",
				password,
				g_user_session_list.session_list[i].password,
				passwordMD5, passwordMD5MD5);*/

				iRet = !strcmp(pUserConfig->accounts[i].password, password) || !strcmp(passwordMD5, password) || !strcmp(passwordMD5MD5, password);
			}

			//__ERR("iRet =%d\n",iRet);

			if (iRet)
			{
				__INFO("password match.\n");

				if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
				{
					strcpy(group, pUserConfig->accounts[i].group.groupName);
					sprintf(sessionid, "%04d%02d%02d%02d%02d%02d_%08x%08x",
							1900 + ptm.tm_year,
							ptm.tm_mon + 1,
							ptm.tm_mday,
							ptm.tm_hour,
							ptm.tm_min,
							ptm.tm_sec,
							rand() * rand(),
							rand() * rand());

					int sindex = g_user_session_list.session_count;
					__INFO("sindex = %d, sessionid len = %d\n", sindex, strlen(sessionid));

					strcpy(g_user_session_list.session_list[sindex].username, username);
					strcpy(g_user_session_list.session_list[sindex].password, pUserConfig->accounts[i].password);
					strcpy(g_user_session_list.session_list[sindex].group, group);
					strcpy(g_user_session_list.session_list[sindex].session, sessionid);

					SystemGetTimeofRun(&tv, NULL);
					memcpy(&(g_user_session_list.session_list[sindex].last_session_time), &tv, sizeof(struct timeval));
					g_user_session_list.session_count++;

					__INFO("session count=%d\n", g_user_session_list.session_count);

					return 0;
				}
				else
				{
					__ERR("account: %s is disabled.\n", username);
					return ERROR_USERACCOUNT_DISABLE;
				}
			}
			else
			{
				__ERR("ERROR_BAD_PASSWORD\n");
				return ERROR_BAD_PASSWORD;
			}
		}
	}

	__ERR("username: %s not found.\n", username);
	return ERROR_USERNAME_NOT_FOUND;
}

int UserAuthGetPassword(char *username, char *password)
{
	int i = 0;
	if (g_user_admin_login)
	{
		if (!strcasecmp(username, "user"))
		{
			strncpy(password, "admin", ACCOUNT_PASSWORD_MAX_LEN);
			return 0;
		}
	}

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
			{
				__INFO("password= %s, len=%d\n", pUserConfig->accounts[i].password,
					   strlen(pUserConfig->accounts[i].password));

				strncpy(password, pUserConfig->accounts[i].password, ACCOUNT_PASSWORD_MAX_LEN);

				return 0;
			}
			else
			{
				__ERR("account: %s is disabled.\n", username);
				return ERROR_USERACCOUNT_DISABLE;
			}
		}
	}

	__ERR("username: %s not found.\n", username);
	return ERROR_USERNAME_NOT_FOUND;
}

int UserAuthGetUsernamePassword(char *username, int username_len, char *password, int password_len)
{
    snprintf(username, username_len, "%s", pUserConfig->accounts[0].userName);    
    snprintf(password, password_len, "%s", pUserConfig->accounts[0].password);
	return 0;
}

int UserAuthUpdateSession(char *session)
{
	// if session was closed, returne false
	// else update last session time
	int i = 0;
	struct timeval tv;
	for (i = 0; i < g_user_session_list.session_count && i < MAX_USER_SESSION_COUNT; i++)
	{
		if (strncmp(g_user_session_list.session_list[i].session, session, MAX_USER_SESSION) == 0)
		{
			SystemGetTimeofRun(&tv, NULL); // SystemGetTimeofRun(&tv, NULL);
			memcpy(&(g_user_session_list.session_list[i].last_session_time), &tv, sizeof(struct timeval));

			return 0;
		}
	}

	return ERROR_SESSION_NOT_FOUND;
}

int UserAuthCheckSession(char *session)
{
	// if last session time is too old, return LOGOUT, else return LOGIN
	int login = 0;
	struct timeval tv;
	int i = 0, j = 0;

	for (i = 0; i < g_user_session_list.session_count && i < MAX_USER_SESSION_COUNT; i++)
	{
		if (strncmp(g_user_session_list.session_list[i].session, session, MAX_USER_SESSION) == 0)
		{
			SystemGetTimeofRun(&tv, NULL); // SystemGetTimeofRun(&tv, NULL);
			// session time out
			if (tv.tv_sec - g_user_session_list.session_list[i].last_session_time.tv_sec > MAX_SESSION_TIMEOUT_SECOND)
			{
				login = 0;

				// remove this seesion
				if (i != g_user_session_list.session_count - 1)
				{
					for (j = i; j < g_user_session_list.session_count - 1; j++)
					{
						memcpy(&(g_user_session_list.session_list[j]), &(g_user_session_list.session_list[j + 1]), sizeof(USER_SESSION_ITEM));
					}
				}
				g_user_session_list.session_count--;
			}
			else
			{
				login = 1;
			}

			return login;
		}
	}

	return ERROR_SESSION_NOT_FOUND;
}

int UserAuthLogout(char *session)
{
	// delete the session
	int i = 0, j = 0;

	__INFO("session: %s logout, session_count=%d\n", session, g_user_session_list.session_count);

	for (i = 0; i < g_user_session_list.session_count && i < MAX_USER_SESSION_COUNT; i++)
	{
		if (strncmp(g_user_session_list.session_list[i].session, session, MAX_USER_SESSION) == 0)
		{
			__INFO("existed session found.\n");

			if (i != (g_user_session_list.session_count - 1))
			{
				for (j = i; j < g_user_session_list.session_count - 1; j++)
				{
					memcpy(&(g_user_session_list.session_list[j]), &(g_user_session_list.session_list[j + 1]), sizeof(USER_SESSION_ITEM));
				}
			}
			g_user_session_list.session_count--;

			__INFO("after logout, session count=%d\n", g_user_session_list.session_count);

			return 0;
		}
	}

	__ERR("session %s not found\n", session);
	return ERROR_SESSION_NOT_FOUND;
}

int UserAuthAddUser(char *username, char *password, char *group, char *status)
{
	int i = 0;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	pUserConfig = &pstSystemConfig->userCfg;

	__ERR("%s,%s,%s,%s\n", username, password, group, status);

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			__ERR("ERROR_USERNAME_DUPLICATED\n");
			return ERROR_USERNAME_DUPLICATED;
		}
	}

	if (pUserConfig->count >= MAX_ACCOUNT_COUNT)
	{
		__ERR("ERROR_TOO_MANY_USERS\n");
		return ERROR_TOO_MANY_USERS;
	}

	strcpy(pUserConfig->accounts[i].userName, username);
	strcpy(pUserConfig->accounts[i].password, password);
	strcpy(pUserConfig->accounts[i].group.groupName, group);
	strcpy(pUserConfig->accounts[i].status, status);
	pUserConfig->count++;

	if ((strcasecmp(group, "Administrator") == 0) && (strcmp(status, "Enable") == 0))
	{
		g_adminCnt++;
	}

	anj_config_system_save(pstSystemConfig);

	__ERR("adminCnt=%d\n", g_adminCnt);
	return 0;
}

int UserAuthEditUser(char *username, char *password, char *group, char *status, char *secureLogin)
{
	int i = 0, j = 0;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	pUserConfig = &pstSystemConfig->userCfg;
	__ERR("%s,%s,%s,%s,%s,adminCnt=%d\n", username, password, group, status, secureLogin, g_adminCnt);

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			if (strcasecmp(pUserConfig->accounts[i].group.groupName, "Administrator") == 0)
			{
				if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
				{
					if ((strcasecmp(group, "Administrator") != 0) || (strcmp(status, "Enable") != 0))
					{
						if (g_adminCnt <= 1)
						{
							return ERROR_LAST_ADMINISTRATOR;
						}
						else
						{
							g_adminCnt--;
						}
					}
				}

				if (strcmp(pUserConfig->accounts[i].status, "Enable") != 0)
				{
					if ((strcasecmp(group, "Administrator") == 0) && (strcmp(status, "Enable") == 0))
					{
						g_adminCnt++;
					}
				}
			}
			else
			{
				if ((strcasecmp(group, "Administrator") == 0) && (strcmp(status, "Enable") == 0))
				{
					g_adminCnt++;
				}
			}

			strcpy(pUserConfig->accounts[i].userName, username);
			strcpy(pUserConfig->accounts[i].password, password);
			strcpy(pUserConfig->accounts[i].group.groupName, group);
			strcpy(pUserConfig->accounts[i].status, status);

			anj_config_system_save(pstSystemConfig);

			// update session if this account in session list
			for (j = 0; j < g_user_session_list.session_count && j < MAX_USER_SESSION_COUNT; j++)
			{
				if (strcasecmp(g_user_session_list.session_list[j].username, username) == 0)
				{
					// find existed session
					__ERR("find one existed session, index=%d\n", i);

					if (strcmp(status, "Disable") == 0) // remove it
					{
						__ERR("the disabled one in session list, remove it.\n");

						g_user_session_list.session_list[j].last_session_time.tv_sec = 0;
						g_user_session_list.session_list[j].last_session_time.tv_usec = 0;

						DeleteTimeOutSession();
					}
					else // update it
					{
						__ERR("update user,pass to: [%s/%s]\n", username, password);

						strcpy(g_user_session_list.session_list[j].username, username);
						strcpy(g_user_session_list.session_list[j].password, password);
						strcpy(g_user_session_list.session_list[j].group, group);
					}
				}
			}

			__ERR("adminCnt=%d\n", g_adminCnt);
			return 0;
		}
	}

	__ERR("ERROR_USERNAME_NOT_FOUND\n");
	return ERROR_USERNAME_NOT_FOUND;
}

int UserAuthAddOrEditUser(char *username, char *password, char *group, char *status)
{
	int i = 0, j = 0;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	pUserConfig = &pstSystemConfig->userCfg;
	__INFO("%s,%s,%s,%s\n", username, password, group, status);

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			if (strcasecmp(pUserConfig->accounts[i].group.groupName, "Administrator") == 0)
			{
				if (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)
				{
					if ((strcasecmp(group, "Administrator") != 0) || (strcmp(status, "Enable") != 0))
					{
						if (g_adminCnt <= 1)
						{
							return ERROR_LAST_ADMINISTRATOR;
						}
						else
						{
							g_adminCnt--;
						}
					}
				}
			}
			else
			{
				if ((strcasecmp(group, "Administrator") == 0) && (strcmp(status, "Enable") == 0))
				{
					g_adminCnt++;
				}
			}

			strcpy(pUserConfig->accounts[i].userName, username);
			strcpy(pUserConfig->accounts[i].password, password);
			strcpy(pUserConfig->accounts[i].group.groupName, group);
			strcpy(pUserConfig->accounts[i].status, status);

			anj_config_system_save(pstSystemConfig);

			// update session if this account in session list
			for (j = 0; j < g_user_session_list.session_count && j < MAX_USER_SESSION_COUNT; j++)
			{
				if (strcasecmp(g_user_session_list.session_list[j].username, username) == 0)
				{
					__ERR("found existed session, index=%d\n", j);

					if (strcmp(status, "Disable") == 0) // remove it
					{
						__ERR("the disabled one in session list, remove it.\n");

						g_user_session_list.session_list[j].last_session_time.tv_sec = 0;
						g_user_session_list.session_list[j].last_session_time.tv_usec = 0;

						DeleteTimeOutSession();
					}
					else // update it
					{
						__ERR("update user,pass to: [%s/%s]\n", username, password);

						strcpy(g_user_session_list.session_list[j].username, username);
						strcpy(g_user_session_list.session_list[j].password, password);
						strcpy(g_user_session_list.session_list[j].group, group);
					}
				}
			}
			__ERR("adminCnt=%d\n", g_adminCnt);
			return 0;
		}
	}

	// NOT FOUND;
	if (pUserConfig->count >= MAX_ACCOUNT_COUNT)
	{
		__ERR("ERROR_TOO_MANY_USERS\n");
		return ERROR_TOO_MANY_USERS;
	}

	strcpy(pUserConfig->accounts[i].userName, username);
	strcpy(pUserConfig->accounts[i].password, password);
	strcpy(pUserConfig->accounts[i].group.groupName, group);
	strcpy(pUserConfig->accounts[i].status, status);
	pUserConfig->count++;

	if ((strcasecmp(group, "Administrator") == 0) && (strcmp(status, "Enable") == 0))
	{
		g_adminCnt++;
	}

	anj_config_system_save(pstSystemConfig);

	__ERR("adminCnt=%d\n", g_adminCnt);
	return 0;
}

int UserAuthDeleteUser(char *username)
{
	int i = 0, j = 0;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	pUserConfig = &pstSystemConfig->userCfg;

	for (i = 0; i < pUserConfig->count && i < MAX_ACCOUNT_COUNT; i++)
	{
		if (strcasecmp(pUserConfig->accounts[i].userName, username) == 0)
		{
			if ((g_adminCnt <= 1) &&
				((strcasecmp(pUserConfig->accounts[i].group.groupName, "Administrator") == 0) &&
				 (strcmp(pUserConfig->accounts[i].status, "Enable") == 0)))
			{
				return ERROR_LAST_ADMINISTRATOR;
			}

			if ((strcasecmp(pUserConfig->accounts[i].group.groupName, "Administrator") == 0) &&
				(strcmp(pUserConfig->accounts[i].status, "Enable") == 0))
			{
				g_adminCnt--;
			}

			if (i != (pUserConfig->count - 1))
			{
				for (j = i; j < pUserConfig->count - 1; j++)
				{
					strcpy(pUserConfig->accounts[j].userName, pUserConfig->accounts[j + 1].userName);
					strcpy(pUserConfig->accounts[j].password, pUserConfig->accounts[j + 1].password);
					strcpy(pUserConfig->accounts[j].group.groupName, pUserConfig->accounts[j + 1].group.groupName);
					strcpy(pUserConfig->accounts[j].status, pUserConfig->accounts[j + 1].status);
				}
			}

			pUserConfig->count--;

			anj_config_system_save(pstSystemConfig);

			// update session if this account in session list
			for (j = 0; j < g_user_session_list.session_count && j < MAX_USER_SESSION_COUNT; j++)
			{
				if (strcasecmp(g_user_session_list.session_list[j].username, username) == 0)
				{
					// find existed session
					__ERR("the one removed in session list, remove it.\n");

					g_user_session_list.session_list[j].last_session_time.tv_sec = 0;
					g_user_session_list.session_list[j].last_session_time.tv_usec = 0;

					DeleteTimeOutSession();
				}
			}

			__ERR("adminCnt=%d\n", g_adminCnt);
			return 0;
		}
	}

	__ERR("ERROR_USERNAME_NOT_FOUND\n");
	return ERROR_USERNAME_NOT_FOUND;
}

int UserAuthAdminLoginGet()
{
    return g_user_admin_login;
}
