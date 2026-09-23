#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "anj_mw_log.h"
#include "anj_mw_crypt.h"
#include "anj_config.h"
#include "anj_h5_auth.h"

int anj_h5_check_user_auth(const char *username, const char *password, long long time_stamp)
{
    (void)time_stamp;

    if (username == NULL || password == NULL)
    {
        return 0;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    if (pstSystemCfg == NULL)
    {
        return 0;
    }

    UserConfig *pUserCfg = &pstSystemCfg->userCfg;
    for (int i = 0; i < pUserCfg->count; i++)
    {
        if (strcasecmp(pUserCfg->accounts[i].userName, username) != 0)
        {
            continue;
        }

        char md5_buf[64] = {0};
        our_md5_encode(md5_buf, (unsigned char *)pUserCfg->accounts[i].password,
                       (int)strlen(pUserCfg->accounts[i].password));
        if (strcasecmp(md5_buf, password) == 0)
        {
            return 1;
        }
        return 0;
    }

    return 0;
}
