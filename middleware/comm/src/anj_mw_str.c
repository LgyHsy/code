#include <iconv.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "anj_mw_comm.h"
#include "aes.h"

char *copy_with_shell(char *out, char *in)
{
    if (in == NULL)
    {
        out[0] = '\0';
        return out;
    }

    int len = strlen(in);
    if (len == 0)
    {
        out[0] = '\0';
        return out;
    }

    int i;
    char *curPos = out;
    for (i = 0; i < len; i++)
    {
        switch (in[i])
        {
        case '<':
            strcpy(curPos, "\\<");
            curPos += strlen("\\<");
            break;
        case '>':
            strcpy(curPos, "\\>");
            curPos += strlen("\\>");
            break;
        case '&':
            strcpy(curPos, "\\&");
            curPos += strlen("\\&");
            break;
        case '|':
            strcpy(curPos, "\\|");
            curPos += strlen("\\|");
            break;
        case '\\':
            strcpy(curPos, "\\\\");
            curPos += strlen("\\\\");
            break;

        default:
            memcpy(curPos, &(in[i]), 1);
            curPos += 1;
            break;
        }
    }

    curPos[0] = '\0';

    return out;
}

unsigned char char2hex(const char *str)
{
    char ch1 = tolower(*str);
    char ch2 = tolower(*(str + 1));
    unsigned char val = 0;

    if ((ch1 >= 'a') && (ch1 <= 'f'))
        val += (ch1 - 'a' + 10) * 16;
    else
        val += (ch1 - '0') * 16;

    if ((ch2 >= 'a') && (ch2 <= 'f'))
        val += ch2 - 'a' + 10;
    else
        val += ch2 - '0';

    return val;
}

unsigned char char2int(char c)
{
    if(c >= '0' && c <= '9') 
        return (c - '0');
    else if(c >= 'a' && c <= 'f')
        return c - 'a' + 10; 
    else if(c >= 'A' && c <= 'F') 
        return c - 'A' + 10;
    else 
        return 0;
}

char *bin_to_hexstr(char *out, int buflen, char *in)
{
    if (in == NULL)
    {
        out[0] = '\0';
        return out;
    }

    if (buflen <= 0)
        return NULL;

    if (buflen <= 2)
    {
        out[0] = 0;
        return out;
    }

    int len = strlen(in);
    if (len == 0)
    {
        out[0] = '\0';
        return out;
    }

    if (len >= (buflen >> 1))
        len = (buflen >> 1) - 1;

    int i;
    char *curPos = out;
    for (i = 0; i < len; i++)
    {
        const unsigned char a = in[i];
        sprintf(curPos, "%02x", a);
        curPos += 2;
    }
    curPos[0] = '\0';

    return out;
}

int hex2ascii(const char *hex, int len, char *out_buf, int buf_len)
{
    int count = len / 2;
    const char *ptr = hex;
    int i;

    if (count > (buf_len - 1))
        count = buf_len - 1;

    for (i = 0; i < count; i++)
    {
        *out_buf++ = char2hex(ptr);
        ptr += 2;
    }
    *out_buf = '\0';

    return count;
}

int Str2Num(const char *buf)
{
    if (buf == NULL)
        return 0;
    else
        return atoi(buf);
}

double Str2Double(const char *buf)
{
    if (buf == NULL)
        return 0.0;
    else
        return atof(buf);
}

int IPStr2Num(char *IPStr)
{
    if (IPStr == NULL)
        return 0;

    struct in_addr in;

    int r = inet_aton(IPStr, &in);
    if (r == 0)
    {
        __ERR("IP is not valid\n");
        return 0;
    }
    return in.s_addr;
}

char *Num2IPStr(int IP)
{
    struct in_addr in;
    in.s_addr = IP;

    return inet_ntoa(in);
}

void StrCpy(char *dest, int destLen, const char *src)
{
    if (src == NULL)
    {
        dest[0] = '\0';
    }
    else
    {
        snprintf(dest, destLen, "%s", src);
    }
}

char *replace_str(const char *inStr, const char *pSrc, const char *pReplace)
{
    if (!inStr || !pSrc || !pReplace)
        return NULL;

    size_t srcLen = strlen(pSrc);
    if (srcLen == 0)
        return strdup(inStr);

    size_t inLen = strlen(inStr);
    size_t replaceLen = strlen(pReplace);
    size_t maxNewLen = inLen + 1;

    char *result = (char *)malloc(maxNewLen);
    if (!result)
        return NULL;
    *result = '\0';

    const char *current = inStr;
    const char *next;
    char *buffer = result;

    while ((next = strstr(current, pSrc)) != NULL)
    {
        size_t segLen = next - current;
        strncpy(buffer, current, segLen);
        buffer += segLen;

        strcpy(buffer, pReplace);
        buffer += replaceLen;

        current = next + srcLen;
    }

    strcpy(buffer, current);

    size_t actualLen = buffer - result + strlen(current);
    char *final = (char *)realloc(result, actualLen + 1);
    return final ? final : result;
}

char *restore_with_escape(const char *inStr)
{
    if (!inStr)
        return NULL;

    char *current = strdup(inStr);
    if (!current)
        return NULL;

    const char *replacements[][2] = {
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&amp;", "&"},
        {"&apos;", "'"},
        {"&quot;", "\""},
        {NULL, NULL}};

    for (int i = 0; replacements[i][0]; i++)
    {
        char *temp = replace_str(current, replacements[i][0], replacements[i][1]);
        if (temp)
        {
            free(current);
            current = temp;
        }
    }

    return current;
}

int gb2312_to_utf8(const char *pIn, char *pOut)
{
    if (pIn == NULL || pOut == NULL)
    {
        return -1;
    }
    iconv_t cd;
    size_t in_len, out_len;
    char *in_ptr, *out_ptr;
    size_t result;

    cd = iconv_open("UTF-8", "GB2312");
    if (cd == (iconv_t)-1)
    {
        perror("iconv_open failed");
        return -1;
    }

    in_len = strlen(pIn);
    out_len = in_len * 3 + 1;

    in_ptr = (char *)pIn;
    out_ptr = pOut;

    result = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);

    iconv_close(cd);

    if (result == (size_t)-1)
    {
        perror("iconv conversion failed");
        return -2;
    }

    *out_ptr = '\0';

    return 0;
}

int utf8_to_gb2312(const char *pIn, char *pOut)
{
    if (pIn == NULL || pOut == NULL)
    {
        return -1;
    }

    iconv_t cd = iconv_open("GB2312", "UTF-8");
    if (cd == (iconv_t)-1)
    {
        return -1;
    }

    size_t in_len = strlen(pIn);
    size_t out_len = in_len * 3;
    size_t in_bytes_left = in_len;
    size_t out_bytes_left = out_len;

    char *in_ptr = (char *)pIn;
    char *out_ptr = pOut;

    size_t result = iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left);

    iconv_close(cd);

    if (result == (size_t)-1)
    {
        return -1;
    }

    if (out_bytes_left > 0)
    {
        *out_ptr = '\0';
    }
    else
    {
        pOut[out_len - 1] = '\0';
    }

    return 0;
}

int str_is_utf8(char *str, int length)
{
    if (str == NULL)
    {
        __ERR("######str is NULL!!!!\n");
        return 0;
    }

    unsigned long nBytes = 0; /*UFT8可用1-6个字节编码,ASCII用一个字节*/
    unsigned char chr;
    int bAllAscii = 1; /*如果全部都是ASCII, 说明不是UTF-8*/
    int i = 0;
    for (i = 0; i < length; ++i)
    {
        chr = *(str + i);
        if ((chr & 0x80) != 0) /*判断是否ASCII编码,如果不是,说明有可能是UTF-8,ASCII用7位编码,但用一个字节存,最高位标记为0,o0xxxxxxx*/
        {
            bAllAscii = 0;
        }
        if (nBytes == 0) /*如果不是ASCII码,应该是多字节符,计算字节数*/
        {
            if (chr >= 0x80)
            {
                if (chr >= 0xFC && chr <= 0xFD)
                    nBytes = 6;
                else if (chr >= 0xF8)
                    nBytes = 5;
                else if (chr >= 0xF0)
                    nBytes = 4;
                else if (chr >= 0xE0)
                    nBytes = 3;
                else if (chr >= 0xC0)
                    nBytes = 2;
                else
                    return 0;

                nBytes--;
            }
        }
        else /*多字节符的非首字节,应为 10xxxxxx*/
        {
            if ((chr & 0xC0) != 0x80)
                return 0;

            nBytes--;
        }
    }
    if (nBytes > 0) /*违返规则*/
        return 0;
    if (bAllAscii) /*如果全部都是ASCII, 说明不是UTF-8*/
        return 0;

    return 1;
}

char *copy_with_escape(char *out, char *in)
{
    if (in == NULL)
    {
        out[0] = '\0';
        return out;
    }

    int len = strlen(in);
    if (len == 0)
    {
        out[0] = '\0';
        return out;
    }

    int i;
    char *curPos = out;
    for (i = 0; i < len; i++)
    {
        switch (in[i])
        {
        case '<':
            strcpy(curPos, "&lt;");
            curPos += strlen("&lt;");
            break;
        case '>':
            strcpy(curPos, "&gt;");
            curPos += strlen("&gt;");
            break;
        case '&':
            strcpy(curPos, "&amp;");
            curPos += strlen("&amp;");
            break;
        case '\'':
            strcpy(curPos, "&apos;");
            curPos += strlen("&apos;");
            break;
        case '\"':
            strcpy(curPos, "&quot;");
            curPos += strlen("&quot;");
            break;
        default:
            memcpy(curPos, &(in[i]), 1);
            curPos += 1;
            break;
        }
    }

    curPos[0] = '\0';

    return out;
}

void copy_with_quoted_escape(char *out, int outlen, const char *in)
{
    int i = 0;
    int j = 0;

    if (out == NULL || outlen <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (in == NULL)
    {
        return;
    }
    while (in[i] != '\0' && j < outlen - 2)
    {
        if (in[i] == '"' || in[i] == '\\')
        {
            if (j + 2 >= outlen)
            {
                break;
            }
            out[j++] = '\\';
        }
        out[j++] = in[i++];
    }
    out[j] = '\0';
}

// int hexdataTohexStr(const char *buf, unsigned int len, char *out,
//                     unsigned int outbuflen, int uppercase)
// {
//     if (out == NULL || outbuflen == 0)
//     {
//         return -1;
//     }

//     if (outbuflen < 2 * len + 1)
//     {
//         if (outbuflen > 0)
//             out[0] = '\0';
//         return -2;
//     }

//     static const char hexLower[] = "0123456789abcdef";
//     static const char hexUpper[] = "0123456789ABCDEF";

//     const char *hexTable = uppercase ? hexUpper : hexLower;

//     unsigned int i;
//     for (i = 0; i < len; i++)
//     {
//         unsigned char byte = (unsigned char)buf[i];

//         out[2 * i] = hexTable[byte >> 4];
//         out[2 * i + 1] = hexTable[byte & 0x0F];
//     }

//     out[2 * i] = '\0';

//     return 2 * i;
// }

char *toLowerStr(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    char *original = str;

    while (*str != '\0')
    {
        *str = tolower(*str);
        str++;
    }

    return original;
}

char *toUpperStr(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    char *original = str;

    while (*str != '\0')
    {
        *str = toupper(*str);
        str++;
    }

    return original;
}

int passwd_key[32] =
    {
        0x19, 0x42, 0x50, 0x15, 0x53, 0x88, 0xab, 0xcd,
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0xde, 0xef,
        0x12, 0x34, 0x57, 0x98, 0x0a, 0xba, 0xcd, 0xfe,
        0xdd, 0xcc, 0x99, 0x55, 0x66, 0x88, 0x3f, 0xfe};

int StringEncrypt(const char *source, char *dst, int dstlen)
{
    if (dst == NULL || dstlen <= 32)
        return -1;

    struct crypto_aes_ctx ctx;
    unsigned char data_src[16 + 4] = {0};
    unsigned char data_dst[16 + 4] = {0};
    memset(data_src, 0, sizeof(data_src));
    memset(data_dst, 0, sizeof(data_dst));
    memset(dst, 0, dstlen);
    if (source != NULL)
    {
        strncpy((char *)data_src, source, 16);
    }

    u8 *key = (u8 *)passwd_key;
    crypto_aes_expand_key(&ctx, (u8 *)key, 32);
    aes_encrypt_(&ctx, data_dst, data_src);
    hexdataTohexStr((const char *)data_dst, 16, dst, dstlen);

    return 0;
}

int StringDecrypt(const char *source, char *dst, int dstlen)
{
    if (source == NULL)
        return -1;

    int srclen = strlen(source);
    if (srclen != 32)
        return -1;

    if (dst == NULL || dstlen <= 16)
        return -1;

    struct crypto_aes_ctx ctx;
    unsigned char data_src[16 + 4] = {0};
    unsigned char data_dst[16 + 4] = {0};
    memset(data_src, 0, sizeof(data_src));
    memset(data_dst, 0, sizeof(data_dst));
    memset(dst, 0, dstlen);

    if (source != NULL)
    {
        hexStrToUInt(source, strlen(source), data_src);
    }

    u8 *key = (u8 *)passwd_key;

    crypto_aes_expand_key(&ctx, (u8 *)key, 32);
    aes_decrypt_(&ctx, data_dst, data_src);

    strncpy(dst, (char *)data_dst, dstlen - 1);

    return 0;
}

void string_trim_tail(char *str)
{
    if (!str)
        return;

    int len = strlen(str);
    if (len == 0)
        return;

    int tail = len - 1;

    while (tail > 0)
    {
        //__ERR("tail = [%c], 0x%02x\n", *(str + tail), *(str + tail));
        if (isspace(*(str + tail)))
        {
            *(str + tail) = '\0';
            tail--;
        }
        else
            break;
    }
}

void string_trim_head(char *str)
{
    if (!str)
        return;

    int len = strlen(str);
    if (len == 0)
        return;

    char *p = str;
    while (p < str + len)
    {
        if (!isspace(*p))
            break;
        else
            p++;
    }

    if (p == str)
        return;

    char *pDst = str;
    while (p < str + len)
    {
        *pDst = *p;
        pDst++;
        p++;
    }
    *pDst = 0;
}

void string_remove(char *str, char c)
{
    if (!str)
        return;

    int len = strlen(str);
    if (len == 0)
        return;
    int curr_index = 0;
    int i = 0;
    for (i = 0; i < len; i++)
    {
        if (str[i] != c)
        {
            str[curr_index] = str[i];
            curr_index++;
        }
    }
    str[curr_index] = 0;
}

int strFindNoSpace(const char *strToLkup)
{
    int strIdx = 0;
    int loopMax = strlen(strToLkup);
    while (strIdx < loopMax)
    {
        if (' ' != strToLkup[strIdx] && '\t' != strToLkup[strIdx])
            return strIdx;
        strIdx++;
    }
    return -1;
}

int strFindSpace(const char *strToLkup)
{
    int strIdx = 0;
    int loopMax = strlen(strToLkup);
    while (strIdx < loopMax)
    {
        if (' ' == strToLkup[strIdx] || '\t' == strToLkup[strIdx])
            return strIdx;
        strIdx++;
    }
    return -1;
}

const char *strGetFilename(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    // 从后往前查找路径分隔符
    const char *filename = strrchr(path, '/');      // Linux/Unix 分隔符
    const char *filename_win = strrchr(path, '\\'); // Windows 分隔符

    // 取最后一个路径分隔符的位置
    if (filename_win != NULL && filename_win > filename)
    {
        filename = filename_win;
    }

    if (filename == NULL)
    {
        return path;
    }

    return filename + 1;
}

int strFindchrIndex(const char *str, char c) // 
{
    const char *pindex = strchr(str, c);
    if (NULL == pindex)
    {
        return -1;
    }

    return pindex - str;
}

void safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0)
        return;
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    
    if (strlen(src) >= dest_size)
    {
        __ERR("warning: string dest:%s is truncated to len:%d!\n", dest, dest_size - 1);
    }
}