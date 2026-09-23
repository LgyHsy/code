#ifndef ANJREC_UBOOT_ENV_H
#define ANJREC_UBOOT_ENV_H

/*
 * Modify/add/delete U-Boot environment variables.
 * value non-empty: set or add; value empty: delete.
 *
 * Returns (same as check_ubootargs2):
 *   -1: failure
 *    0: value unchanged, no write
 *    1: modified and written to flash
 */
int anjrec_uboot_env_set(const char *name, const char *value, int verbose);

/* Read env value into buf (NUL-terminated). Returns 0 on success, -1 if missing. */
int anjrec_uboot_env_get(const char *name, char *buf, int buflen);

#endif
