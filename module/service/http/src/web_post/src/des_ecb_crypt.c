#include "des_ecb_crypt.h"
#include "anj_comm.h"

#include <stdio.h>
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static char hex_char(int v)
{
    v &= 0xF;
    return v < 10 ? (char)('0' + v) : (char)('a' + v - 10);
}

static int hex_to_bytes(const char *hex, unsigned char *out, int max_out)
{
    int len = 0, hi, lo, i = 0;
    if (!hex || !out) return DES_ERR_NULL;
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) i = 2;
    while (hex[i]) {
        hi = hex_nibble(hex[i]);
        lo = hex_nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return DES_ERR_HEX;
        if (len >= max_out)    return DES_ERR_LEN;
        out[len++] = (unsigned char)((hi << 4) | lo);
        i += 2;
    }
    return len;
}

static int bytes_to_hex(const unsigned char *in, int in_len,
                        char *out, int max_out)
{
    int i, pos = 0;
    if (!in || !out) return DES_ERR_NULL;
    for (i = 0; i < in_len; i++) {
        if (pos + 2 >= max_out) return DES_ERR_LEN;
        out[pos++] = hex_char(in[i] >> 4);
        out[pos++] = hex_char(in[i]);
    }
    out[pos] = '\0';
    return pos + 1;
}

/* Standard DES S-boxes (FIPS 46-3) */
static const unsigned char sbox[8][4][16] = {
    {/* S1 */
        {14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7},
        {0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8},
        {4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0},
        {15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13}
    },
    {/* S2 */
        {15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10},
        {3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5},
        {0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15},
        {13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9}
    },
    {/* S3 */
        {10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8},
        {13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1},
        {13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7},
        {1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12}
    },
    {/* S4 */
        {7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15},
        {13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9},
        {10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4},
        {3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14}
    },
    {/* S5 */
        {2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9},
        {14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6},
        {4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14},
        {11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3}
    },
    {/* S6 */
        {12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11},
        {10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8},
        {9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6},
        {4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13}
    },
    {/* S7 */
        {4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1},
        {13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6},
        {1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2},
        {6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12}
    },
    {/* S8 */
        {13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7},
        {1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2},
        {7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8},
        {2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11}
    }
};

/* Permutation P (FIPS 46-3) - 32-bit output from S-boxes */
static const unsigned char P[32] = {
    16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
    2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25
};

/* Expansion E (FIPS 46-3) - 48-bit expansion from 32-bit half */
static const unsigned char E[48] = {
    32,1,2,3,4,5,4,5,6,7,8,9,8,9,10,11,12,13,
    12,13,14,15,16,17,16,17,18,19,20,21,20,21,
    22,23,24,25,24,25,26,27,28,29,28,29,30,31,32,1
};

/* Initial Permutation (IP) */
static const unsigned char IP[64] = {
    58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,
    62,54,46,38,30,22,14,6,64,56,48,40,32,24,16,8,
    57,49,41,33,25,17,9,1,59,51,43,35,27,19,11,3,
    61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7
};

/* Final Permutation (IP^-1) */
static const unsigned char FP[64] = {
    40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,
    38,6,46,14,54,22,62,30,37,5,45,13,53,21,61,29,
    36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,
    34,2,42,10,50,18,58,26,33,1,41,9,49,17,57,25
};

/* PC-1 (FIPS 46-3) - 56-bit from 64-bit key */
static const unsigned char PC1[56] = {
    57,49,41,33,25,17,9,1,58,50,42,34,26,18,10,
    2,59,51,43,35,27,19,11,3,60,52,44,36,63,55,
    47,39,31,23,15,7,62,54,46,38,30,22,14,6,61,
    53,45,37,29,21,13,5,28,20,12,4
};

/* PC-2 (FIPS 46-3) - 48-bit from 56-bit CD */
static const unsigned char PC2[48] = {
    14,17,11,24,1,5,3,28,15,6,21,10,23,19,12,4,
    26,8,16,7,27,20,13,2,41,52,31,37,47,55,30,40,
    51,45,33,48,44,49,39,56,34,53,46,42,50,36,29,32
};

/* Left shifts per round */
static const unsigned char shifts[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};

/* Helper: apply an 8->N bit permutation table */
static uint64_t permute(const unsigned char *table, int nbits,
                        const unsigned char *src, int src_len)
{
    uint64_t r = 0;
    int i;
    for (i = 0; i < nbits; i++) {
        int bitpos = table[i] - 1;  /* tables are 1-indexed */
        int byte_idx = bitpos >> 3;
        int bit_idx  = 7 - (bitpos & 7);
        if (byte_idx < src_len) {
            unsigned char b = src[byte_idx];
            r = (r << 1) | ((b >> bit_idx) & 1);
        }
    }
    return r;
}

/* Pack 8 bytes into a 64-bit big-endian integer */
static __attribute__((unused)) uint64_t pack64(const unsigned char *bytes)
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++)
        v = (v << 8) | bytes[i];
    return v;
}

/* Unpack a 64-bit big-endian integer to 8 bytes */
static void unpack64(uint64_t v, unsigned char *out)
{
    int i;
    for (i = 7; i >= 0; i--) {
        out[i] = (unsigned char)(v & 0xFF);
        v >>= 8;
    }
}

/* S-box substitution on 48-bit value -> 32-bit output */
static uint32_t sbox_subst(uint64_t val)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < 8; i++) {
        /* Extract 6 bits (bits 42-37, then 36-31, ...) */
        int shift = 42 - i * 6;
        unsigned int six = (unsigned int)((val >> shift) & 0x3F);
        unsigned int row = (six & 1) | ((six >> 4) & 2);
        unsigned int col = (six >> 1) & 0xF;
        unsigned int sval = sbox[i][row][col];
        r = (r << 4) | sval;
    }
    return r;
}

/* Apply P permutation to 32-bit S-box output */
static uint32_t p_perm(uint32_t val)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < 32; i++) {
        int src_bit = P[i] - 1;
        r |= ((val >> (31 - src_bit)) & 1) << (31 - i);
    }
    return r;
}

/* Feistel function f(R, K) */
static uint32_t feistel(uint32_t R, uint64_t K)
{
    /* Expand 32-bit R to 48-bit using E-box */
    uint64_t expanded = 0;
    int i;
    for (i = 0; i < 48; i++) {
        int bitpos = E[i] - 1;
        expanded = (expanded << 1) | ((R >> (31 - bitpos)) & 1);
    }
    /* XOR with round key */
    expanded ^= K;
    /* S-box substitution */
    uint32_t sbox_out = sbox_subst(expanded);
    /* P permutation */
    return p_perm(sbox_out);
}

static void des_process_block(const unsigned char in[8],
                              unsigned char out[8],
                              const uint64_t round_keys[16],
                              int encrypt)
{
    uint64_t perm = permute(IP, 64, in, 8);
    uint32_t left  = (uint32_t)(perm >> 32);
    uint32_t right = (uint32_t)(perm & 0xFFFFFFFF);
    int i;
    if (encrypt) {
        for (i = 0; i < 16; i++) {
            uint32_t temp = right;
            right = left ^ feistel(right, round_keys[i]);
            left = temp;
        }
    } else {
        for (i = 15; i >= 0; i--) {
            uint32_t temp = right;
            right = left ^ feistel(right, round_keys[i]);
            left = temp;
        }
    }
    /* Swap back */
    uint32_t temp = left; left = right; right = temp;
    uint64_t combined = ((uint64_t)left << 32) | right;
    unsigned char buf[8];
    unpack64(combined, buf);
    uint64_t fp = permute(FP, 64, buf, 8);
    unpack64(fp, out);
}

/* Generate 16 round keys from 8-byte key */
static void des_key_schedule(const unsigned char key[8],
                             uint64_t round_keys[16])
{
    /* Apply PC-1 */
    uint64_t pc1 = permute(PC1, 56, key, 8);
    /* Split into C (high 28 bits) and D (low 28 bits) */
    uint32_t C = (uint32_t)(pc1 >> 28);
    uint32_t D = (uint32_t)(pc1 & 0x0FFFFFFF);
    int i;
    for (i = 0; i < 16; i++) {
        int shift = shifts[i];
        C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFF;
        D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFF;
        /* Combine CD into 56-bit value */
        uint64_t CD = ((uint64_t)C << 28) | D;
        /* Convert CD to bytes for permute */
        unsigned char cd_bytes[7];
        int j;
        for (j = 0; j < 7; j++)
            cd_bytes[j] = (unsigned char)(CD >> (48 - j * 8));
        /* Apply PC-2 to get 48-bit round key */
        round_keys[i] = permute(PC2, 48, cd_bytes, 7);
    }
}

/* =====================================================================
 * Public API
 * ===================================================================== */

int des_ecb_decrypt(const char *hex,
                    const unsigned char *key,
                    int klen,
                    unsigned char *out,
                    int *olen)
{
    uint64_t rk[16];
    unsigned char buf[64];
    int hex_len, byte_len, buf_len, plain_len, i;
    if (!hex || !key || !olen) return DES_ERR_NULL;
    if (klen != 8)              return DES_ERR_KEY;
    hex_len = (int)strlen(hex);
    if (hex_len >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        hex_len -= 2;
    if (hex_len == 0 || (hex_len & 1)) return DES_ERR_LEN;
    byte_len = hex_len / 2;
    if (byte_len > (int)sizeof(buf)) return DES_ERR_LEN;
    if (byte_len & 7)                return DES_ERR_LEN;
    buf_len = hex_to_bytes(hex, buf, (int)sizeof(buf));
    if (buf_len < 0) return buf_len;
    des_key_schedule(key, rk);
    for (i = 0; i < buf_len; i += 8)
        des_process_block(buf + i, buf + i, rk, 0);
    plain_len = buf_len;
    while (plain_len > 0 && buf[plain_len - 1] == 0)
        plain_len--;
    if (out) {
        if (*olen < plain_len) return DES_ERR_LEN;
        memcpy(out, buf, plain_len);
    }
    *olen = plain_len;
    return DES_OK;
}

int des_ecb_encrypt(const unsigned char *in,
                    int in_len,
                    const unsigned char *key,
                    int klen,
                    char *hex_out,
                    int *hlen)
{
    uint64_t rk[16];
    unsigned char buf[64];
    int padded_len, i;
    if (!in || !key || !hlen) return DES_ERR_NULL;
    if (klen != 8)            return DES_ERR_KEY;
    if (in_len < 0)           return DES_ERR_LEN;
    padded_len = ((in_len + 7) / 8) * 8;
    if (padded_len > (int)sizeof(buf)) return DES_ERR_LEN;
    memset(buf, 0, sizeof(buf));
    memcpy(buf, in, in_len);
    des_key_schedule(key, rk);
    for (i = 0; i < padded_len; i += 8)
        des_process_block(buf + i, buf + i, rk, 1);
    int ret = bytes_to_hex(buf, padded_len, hex_out, *hlen);
    if (ret < 0) return ret;
    *hlen = ret;
    return DES_OK;
}


int test_des(void)
{
    int ret, olen, hlen;
    unsigned char key[8] = "WebLogin";
    unsigned char plain[64];
    char hex[128];
    const char *test_cases[] = {"admin", "123456", "abcdefghijklmnopqrstuvwxzy", NULL};
    int i;

    printf("=== DES ECB Round-Trip Test (key=\"WebLogin\") ===\n\n");

    for (i = 0; test_cases[i]; i++) {
        const char *input = test_cases[i];
        int in_len = (int)strlen(input);

        /* encrypt */
        hlen = sizeof(hex);
        ret = des_ecb_encrypt((const unsigned char*)input, in_len,
                              key, 8, hex, &hlen);
        if (ret != DES_OK) {
            printf("ENCRYPT error %d for \"%s\"\n", ret, input);
            continue;
        }
        printf("encrypt(\"%s\") -> hex=%s\n", input, hex);

        /* decrypt */
        olen = sizeof(plain);
        ret = des_ecb_decrypt(hex, key, 8, plain, &olen);
        if (ret != DES_OK) {
            printf("DECRYPT error %d for hex=%s\n", ret, hex);
            continue;
        }
        plain[olen] = '\0';

        if (olen == in_len && memcmp(plain, input, olen) == 0) {
            printf("  -> PASS: \"%s\"\n", plain);
        } else {
            printf("  -> FAIL: got \"%s\" (len=%d, expect len=%d)\n",
                   plain, olen, in_len);
            debug_show_data_hex_ex((const unsigned char*)input, in_len, 0);                
            debug_show_data_hex_ex((const unsigned char*)plain, olen, 0);
        }
        printf("\n");
    }

    printf("=== Done ===\n");
    return 0;
}
                    
