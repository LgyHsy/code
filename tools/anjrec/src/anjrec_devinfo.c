#include <stdio.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_sysmng.h"
#include "anjrec_devinfo.h"
#include "anjrec_version.h"

void anjrec_devinfo_apply(void)
{
    DevInfo *p = getDevInfo();
    char model[sizeof(p->search_devicetype)];

    if (p == NULL || p->devType[0] == '\0')
    {
        __ERR("anjrec_devinfo_apply: devType not ready\n");
        return;
    }

    snprintf(model, sizeof(model), "%s_V%s",
             p->devType, anj_sysmng_product_version_get());

    strncpy(p->subDevType, model, sizeof(p->subDevType) - 1);
    p->subDevType[sizeof(p->subDevType) - 1] = '\0';

    strncpy(p->search_devicetype, model, sizeof(p->search_devicetype) - 1);
    p->search_devicetype[sizeof(p->search_devicetype) - 1] = '\0';

    strncpy(p->version_name, ANJREC_APP_VERSION, sizeof(p->version_name) - 1);
    p->version_name[sizeof(p->version_name) - 1] = '\0';

    strncpy(p->release_date, ANJREC_BUILD_TIME, sizeof(p->release_date) - 1);
    p->release_date[sizeof(p->release_date) - 1] = '\0';

    snprintf(p->stVersionInfo.fsVersion, sizeof(p->stVersionInfo.fsVersion),
             "%s %s build %s", model, ANJREC_APP_VERSION, ANJREC_BUILD_TIME);

    __INFO("recovery devinfo: model=%s version=%s build=%s fsVersion=%s\n",
           p->subDevType, p->version_name, p->release_date, p->stVersionInfo.fsVersion);
}
