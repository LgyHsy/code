#ifndef ANJREC_VERSION_MATCH_H
#define ANJREC_VERSION_MATCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char re[16];
    char downurl[512];
    char version[128];
    char md5[40];
    char file_size[32];
    char remark[128];
} anjrec_fw_meta_t;

/*
 * Query firmware metadata from g_version_match.php.
 * Returns:
 *   0  - package available (re=="1" and downurl is http(s))
 *   1  - server responded but no upgrade (re!="1" or empty/invalid downurl)
 *  <0  - network / protocol / parse error
 */
int anjrec_version_match_query(anjrec_fw_meta_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ANJREC_VERSION_MATCH_H */
