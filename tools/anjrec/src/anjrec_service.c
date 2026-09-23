#include "ixml.h"
#include "anj_pri.h"
#include "anj_module.h"
#include "anj_service.h"
#include "anj_search.h"
#include "anjrec_devinfo.h"

static int anj_service_init(void)
{
    anjrec_devinfo_apply();
    anj_pri_init();
    anj_search_init();
    return 0;
}

static int anj_service_uninit(void)
{
    anj_search_uninit();
    anj_pri_uninit();
    return 0;
}

REGISTER_MODULE(anj_service, MODULE_PRIORITY_SERVICE);

int anj_service_alarm_event_notify(void *alarm_event)
{
    (void)alarm_event;
    return 0;
}

int anj_service_audio_enc_change(void)
{
    return 0;
}

int anj_service_partner_proc(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgType, char *MsgCode)
{
    (void)cmdbuf; (void)cmdlen; (void)lognum; (void)pDoc; (void)MsgType; (void)MsgCode;
    return -1;
}
