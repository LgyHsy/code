#ifndef DES_DECRYPT_H
#define DES_DESCRYPT_H

#include <stdint.h>

#define DES_OK        0
#define DES_ERR_NULL -1
#define DES_ERR_LEN  -2
#define DES_ERR_HEX  -3
#define DES_ERR_KEY  -4

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DES ECB decrypt hex string, remove zero-padding
 *
 * Reproduces the reverse of:
 *   stringToHex(des("WebLogin", plaintext, 1, 0))
 *
 * @param hex    Hex ciphertext (null-terminated, may have "0x" prefix)
 * @param key    8-byte DES key
 * @param klen   Must be 8
 * @param out    Output buffer (NULL to query needed size)
 * @param olen   In: capacity; Out: plaintext length (zero-padding stripped)
 * @return DES_OK on success, negative on error
 */
int des_ecb_decrypt(const char *hex,
                    const unsigned char *key,
                    int klen,
                    unsigned char *out,
                    int *olen);

/**
 * @brief DES ECB encrypt raw plaintext to hex string (zero-padding)
 *
 * Matches the frontend behavior:
 *   stringToHex(des("WebLogin", plaintext, 1, 0))
 *
 * @param in      Raw plaintext bytes
 * @param in_len  Plaintext length in bytes
 * @param key     8-byte DES key
 * @param klen    Must be 8
 * @param hex_out Output buffer for hex string (null-terminated)
 * @param hlen    In: capacity; Out: chars written (incl. null)
 * @return DES_OK on success, negative on error
 */
int des_ecb_encrypt(const unsigned char *in,
                    int in_len,
                    const unsigned char *key,
                    int klen,
                    char *hex_out,
                    int *hlen);

#ifdef __cplusplus
}
#endif

#endif
