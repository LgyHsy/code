
#ifndef __DEV_BIND_H__
#define __DEV_BIND_H__

#if 0
int dev_bind_task(const char *product_key, const char *device_name, const char *accountName, const char *client_code);
int dev_unbind_task(const char *product_key, const char *device_name);
int dev_bind_report(char *szPartner, char *product_key, char *device_name, char *szIccid, char *szMsisdn);
#endif

int dev_bind_task(const char *product_key, const char *device_name, const char *accountName, const char *client_code,
                  char *svraddr, char *svrport);
int dev_bind_report(char *szPartner, char *pk, char *dn, char *szIccid, char *szMsisdn,
                  char *svraddr, char *svrport);
int dev_bind_location(const char *url, const char *lac, const char *ci, const char *mnc,
                      const char *imei, const char *devid, const char *ua, const char *type,
                      char *svraddr, char *svrport);
int dev_bind_check_iccid_cs(char *szIccidList);

#endif // __DEV_BIND_H__
