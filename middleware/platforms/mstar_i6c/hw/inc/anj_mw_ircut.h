#ifndef __ANJ_MW_IRCUT_H__
#define __ANJ_MW_IRCUT_H__

#define ANJ_IRCUT_DEV   "/dev/irled"
#define IRCUT_NIGHT 0
#define IRCUT_DAY   1

int anj_mw_ircut_init();
int anj_mw_ircut_uninit();
int anj_mw_ircut_set(int status);

#endif
