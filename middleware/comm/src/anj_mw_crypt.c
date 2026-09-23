#include "anj_mw_crypt.h"
#include "anj_mw_comm.h"
#include "anj_mw_file.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

#define MD5_DIGEST_LENGTH 16

/* MD5 context. */
typedef struct MD5Context
{
    unsigned state[4];        /* state (ABCD) */
    unsigned count[2];        /* number of bits, modulo 2^64 (lsb first) */
    unsigned char buffer[64]; /* input buffer */
} MD5_CTX;

static unsigned char PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
 * F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/*
 * ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/*
 * FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4. Rotation is
 * separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += F((b), (c), (d)) + (x) + (unsigned)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define GG(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += G((b), (c), (d)) + (x) + (unsigned)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define HH(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += H((b), (c), (d)) + (x) + (unsigned)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define II(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += I((b), (c), (d)) + (x) + (unsigned)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }

/*
 * Encodes input (unsigned) into output (unsigned char). Assumes len is a
 * multiple of 4.
 */
void tps_MD5Encode(unsigned char *output, unsigned *input, unsigned int len)
{
    unsigned int i, j;

#if 0
	assert((len % 4) == 0);
#endif

    for (i = 0, j = 0; j < len; i++, j += 4)
    {
        output[j] = (unsigned char)(input[i] & 0xff);
        output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
        output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
}

/*
 * Decodes input (unsigned char) into output (unsigned). Assumes len is a
 * multiple of 4.
 */
void tps_MD5Decode(unsigned *output, unsigned char const *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((unsigned)input[j]) | (((unsigned)input[j + 1]) << 8) |
                    (((unsigned)input[j + 2]) << 16) | (((unsigned)input[j + 3]) << 24);
}

/*
 * MD5 basic transformation. Transforms state based on block.
 */
void tps_MD5Transform(unsigned state[4], const unsigned char block[64])
{
    unsigned a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    tps_MD5Decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[0], S11, 0xd76aa478);  /* 1 */
    FF(d, a, b, c, x[1], S12, 0xe8c7b756);  /* 2 */
    FF(c, d, a, b, x[2], S13, 0x242070db);  /* 3 */
    FF(b, c, d, a, x[3], S14, 0xc1bdceee);  /* 4 */
    FF(a, b, c, d, x[4], S11, 0xf57c0faf);  /* 5 */
    FF(d, a, b, c, x[5], S12, 0x4787c62a);  /* 6 */
    FF(c, d, a, b, x[6], S13, 0xa8304613);  /* 7 */
    FF(b, c, d, a, x[7], S14, 0xfd469501);  /* 8 */
    FF(a, b, c, d, x[8], S11, 0x698098d8);  /* 9 */
    FF(d, a, b, c, x[9], S12, 0x8b44f7af);  /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[1], S21, 0xf61e2562);  /* 17 */
    GG(d, a, b, c, x[6], S22, 0xc040b340);  /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa);  /* 20 */
    GG(a, b, c, d, x[5], S21, 0xd62f105d);  /* 21 */
    GG(d, a, b, c, x[10], S22, 0x2441453);  /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8);  /* 24 */
    GG(a, b, c, d, x[9], S21, 0x21e1cde6);  /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[3], S23, 0xf4d50d87);  /* 27 */
    GG(b, c, d, a, x[8], S24, 0x455a14ed);  /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8);  /* 30 */
    GG(c, d, a, b, x[7], S23, 0x676f02d9);  /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[5], S31, 0xfffa3942);  /* 33 */
    HH(d, a, b, c, x[8], S32, 0x8771f681);  /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[1], S31, 0xa4beea44);  /* 37 */
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9);  /* 38 */
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60);  /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[0], S32, 0xeaa127fa);  /* 42 */
    HH(c, d, a, b, x[3], S33, 0xd4ef3085);  /* 43 */
    HH(b, c, d, a, x[6], S34, 0x4881d05);   /* 44 */
    HH(a, b, c, d, x[9], S31, 0xd9d4d039);  /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[2], S34, 0xc4ac5665);  /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[0], S41, 0xf4292244);  /* 49 */
    II(d, a, b, c, x[7], S42, 0x432aff97);  /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[5], S44, 0xfc93a039);  /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[3], S42, 0x8f0ccc92);  /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[1], S44, 0x85845dd1);  /* 56 */
    II(a, b, c, d, x[8], S41, 0x6fa87e4f);  /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[6], S43, 0xa3014314);  /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[4], S41, 0xf7537e82);  /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb);  /* 63 */
    II(b, c, d, a, x[9], S44, 0xeb86d391);  /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /*
     * Zeroize sensitive information.
     */
    memset((unsigned char *)x, 0, sizeof(x));
}

/**
 * tps_MD5Init:
 * @context: MD5 context to be initialized.
 *
 * Initializes MD5 context for the start of message digest computation.
 **/
void tps_MD5Init(MD5_CTX *context)
{
    context->count[0] = context->count[1] = 0;
    /* Load magic initialization constants.  */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

/**
 * ourMD5Update:
 * @context: MD5 context to be updated.
 * @input: pointer to data to be fed into MD5 algorithm.
 * @inputLen: size of @input data in bytes.
 *
 * MD5 block update operation. Continues an MD5 message-digest operation,
 * processing another message block, and updating the context.
 **/

void tps_MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen)
{
    unsigned int i, index, partLen;

    /* Compute number of bytes mod 64 */
    index = (unsigned int)((context->count[0] >> 3) & 0x3F);

    /* Update number of bits */
    if ((context->count[0] += ((unsigned)inputLen << 3)) < ((unsigned)inputLen << 3))
    {
        context->count[1]++;
    }
    context->count[1] += ((unsigned)inputLen >> 29);

    partLen = 64 - index;

    /* Transform as many times as possible.  */
    if (inputLen >= partLen)
    {
        memcpy((unsigned char *)&context->buffer[index], (unsigned char *)input, partLen);
        tps_MD5Transform(context->state, context->buffer);

        for (i = partLen; i + 63 < inputLen; i += 64)
        {
            tps_MD5Transform(context->state, &input[i]);
        }
        index = 0;
    }
    else
    {
        i = 0;
    }
    /* Buffer remaining input */
    if ((inputLen - i) != 0)
    {
        memcpy((unsigned char *)&context->buffer[index], (unsigned char *)&input[i], inputLen - i);
    }
}

/**
 * tps_MD5Final:
 * @digest: 16-byte buffer to write MD5 checksum.
 * @context: MD5 context to be finalized.
 *
 * Ends an MD5 message-digest operation, writing the the message
 * digest and zeroing the context.  The context must be initialized
 * with tps_MD5Init() before being used for other MD5 checksum calculations.
 **/

void tps_MD5Final(unsigned char digest[16], MD5_CTX *context)
{
    unsigned char bits[8];
    unsigned int index, padLen;

    /* Save number of bits */
    tps_MD5Encode(bits, context->count, 8);

    /*
     * Pad out to 56 mod 64.
     */
    index = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    tps_MD5Update(context, PADDING, padLen);

    /* Append length (before padding) */
    tps_MD5Update(context, bits, 8);
    /* Store state in digest */
    tps_MD5Encode(digest, context->state, 16);

    /*
     * Zeroize sensitive information.
     */
    memset((unsigned char *)context, 0, sizeof(*context));
}

char *tps_MD5End(MD5_CTX *ctx, char *buf)
{
    int i;
    unsigned char digest[MD5_DIGEST_LENGTH];
    static const char hex[] = "0123456789abcdef";

    if (!buf)
        buf = (char *)malloc(2 * MD5_DIGEST_LENGTH + 1);
    if (!buf)
        return 0;

    tps_MD5Final(digest, ctx);
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        buf[i + i] = hex[digest[i] >> 4];
        buf[i + i + 1] = hex[digest[i] & 0x0f];
    }

    buf[i + i] = '\0';
    return buf;
}

char *out_md5_encode_data(const unsigned char *data, unsigned int len, char *buf)
{
    MD5_CTX ctx;

    tps_MD5Init(&ctx);
    tps_MD5Update(&ctx, data, len);
    return tps_MD5End(&ctx, buf);
}

void our_md5_encode(char *md5Buf, const unsigned char *data, int len)
{
    MD5_CTX md5_ctx;
    unsigned char digest[16];
    int i;

    tps_MD5Init(&md5_ctx);
    tps_MD5Update(&md5_ctx, data, len);
    tps_MD5Final(digest, &md5_ctx);

    for (i = 0; i < 16; i++)
    {
        sprintf(md5Buf + 2 * i, "%02X", digest[i]);
    }
}

static const unsigned int gCrc32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

unsigned int anj_crc32_update(unsigned int start, const unsigned char *pBuffer, unsigned int len)
{
    unsigned int c = start ^ 0xFFFFFFFF, i = 0;
    if (pBuffer == NULL)
    {
        len = 0;
    }

    for (i = 0; i < len; ++i)
    {
        c = gCrc32Table[(c ^ pBuffer[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}

int anj_crc32_file(char *fileName, unsigned int *crc)
{
    int fd;
    unsigned char buf[1024] = {0};
    int readCnt = 0;
    int totalCnt = 0;
    int crc_tmp = 0x0;

    fd = open(fileName, O_RDWR);
    if (fd == -1)
    {
        __ERR("open file (%s) for(%s)\n", fileName, strerror(errno));
        return -1;
    }

    while (1)
    {
        readCnt = safe_read(fd, buf, sizeof(buf));
        if (readCnt == 0)
        {
            __INFO("readcnt=0, totalCnt=%d\n", totalCnt);
            *crc = anj_crc32_update(crc_tmp, NULL, 0);
            close(fd);
            return 0;
        }
        else if (readCnt == -1)
        {
            close(fd);
            return -1;
        }
        else
        {
            totalCnt += readCnt;
            crc_tmp = anj_crc32_update(crc_tmp, buf, readCnt);
        }
    }

    close(fd);
    return 0;
}

static char Base64IdxTab[128] =
    {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
        255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255};
static char Base64ValTab[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BVal(x) Base64IdxTab[(unsigned char)x]
#define AVal(x) Base64ValTab[(unsigned char)x]

int Base64EncodeLen(const char *pInput)
{
    if (NULL == pInput)
        return 0;

    int len = strlen(pInput);
    int base64_len = len / 3 * 4 + (len % 3 != 0) * 4 + 1;
    return base64_len;
}

int Base64Encode(unsigned char *pInput, int srclen, char *pOutput)
{
    int i = 0;
    int loop = 0;
    int remain = 0;
    int iDstLen = 0;
    int iSrcLen = srclen;

    loop = iSrcLen / 3;
    remain = iSrcLen % 3;

    // also can encode native char one by one as decode method
    // but because all of char in native string　is to be encoded so encode 3-chars one time is easier.

    for (i = 0; i < loop; i++)
    {
        char a1 = (pInput[i * 3] >> 2);
        char a2 = (((pInput[i * 3] & 0x03) << 4) | (pInput[i * 3 + 1] >> 4));
        char a3 = (((pInput[i * 3 + 1] & 0x0F) << 2) | ((pInput[i * 3 + 2] & 0xC0) >> 6));
        char a4 = (pInput[i * 3 + 2] & 0x3F);

        pOutput[i * 4] = AVal(a1);
        pOutput[i * 4 + 1] = AVal(a2);
        pOutput[i * 4 + 2] = AVal(a3);
        pOutput[i * 4 + 3] = AVal(a4);
    }

    iDstLen = i * 4;

    if (remain == 1)
    {
        // should pad two equal sign
        i = iSrcLen - 1;
        char a1 = (pInput[i] >> 2);
        char a2 = ((pInput[i] & 0x03) << 4);

        pOutput[iDstLen++] = AVal(a1);
        pOutput[iDstLen++] = AVal(a2);
        pOutput[iDstLen++] = '=';
        pOutput[iDstLen++] = '=';
        pOutput[iDstLen] = 0x00;
    }
    else if (remain == 2)
    {
        // should pad one equal sign
        i = iSrcLen - 2;
        char a1 = (pInput[i] >> 2);
        char a2 = (((pInput[i] & 0x03) << 4) | (pInput[i + 1] >> 4));
        char a3 = ((pInput[i + 1] & 0x0F) << 2);

        pOutput[iDstLen++] = AVal(a1);
        pOutput[iDstLen++] = AVal(a2);
        pOutput[iDstLen++] = AVal(a3);
        pOutput[iDstLen++] = '=';
        pOutput[iDstLen] = 0x00;
    }
    else
    {
        // just division by 3
        pOutput[iDstLen] = 0x00;
    }
    return iDstLen;
}

int Base64Decode(char *pInput, int srclen, unsigned char *pOutput)
{
    int i = 0;
    int iCnt = 0;
    int iSrcLen = srclen;

    unsigned char *p = pOutput;

    for (i = 0; i < iSrcLen; i++)
    {
        if (pInput[i] > 127)
            continue;
        if (pInput[i] == '=')
        {
            return (int)(p - pOutput);
        }
        char a = BVal(pInput[i]);
        if (a == 255)
        {
            continue;
        }
        switch (iCnt)
        {
        case 0:
        {
            *p = a << 2;
            iCnt++;
        }
        break;
        case 1:
        {
            *p++ |= a >> 4;
            *p = a << 4;
            iCnt++;
        }
        break;

        case 2:
        {
            *p++ |= a >> 2;
            *p = a << 6;
            iCnt++;
        }
        break;
        case 3:
        {
            *p++ |= a;
            iCnt = 0;
        }
        break;
        }
    }
    *p = 0x00;
    return (int)(p - pOutput);
}

unsigned int GetCrcValue(char *c, int len)
{
    unsigned int crc;
    char *e = c + len;

    crc = 0xFFFFFFFF;
    while (c < e)
    {
        crc = ((crc >> 8) & 0x00FFFFFF) ^ gCrc32Table[(crc ^ *c) & 0xFF];
        ++c;
    }
    return (crc ^ 0xFFFFFFFF);
}

unsigned int GetCrcValueMode2(unsigned int crc, char *c, int len)
{
    crc = ( crc^0xFFFFFFFF );
    char *e = c + len;

    while (c < e) 
    {
        crc = ((crc >> 8) & 0x00FFFFFF) ^ gCrc32Table[(crc^ *c) & 0xFF];
        ++c;
    }
    return (crc^0xFFFFFFFF);
}

