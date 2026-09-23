/**
 * encrypt.c
 *
 * Functions for encryption and decryption
 *
 * Created by Leon Liu  2013-01-24
 */
#include <stdlib.h>
#include "encrypt.h"
#include <string.h>


char base64_code[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
    'w', 'x', 'y', 'z', '0', '1', '2', '3', 
    '4', '5', '6', '7', '8', '9', '+', '/' 
};

void swap_bytes(unsigned char *a, unsigned char *b)
{
    unsigned char temp;

    temp = *a;
    *a = *b;
    *b = temp;
}

int base64_index(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9')
        return ch - '0' + 52;
    if (ch == '+')
        return 62;
    if (ch == '/')
        return 63;
    else
        return -1;
}

int base64_enc(const char *src, char *dst)
{
	return base64_enc2(src,dst,strlen(src));
}

int base64_enc2(const char *src, char *dst,int srclen)
{
	unsigned char src_code[3];
    unsigned int i, j, len;
    const unsigned char* unsigned_src = (const unsigned char*)src; // 转换为无符号字符指针

    len = srclen/3;
    for (i = 0; i < len; i++)
    {
        src_code[0] = unsigned_src[i * 3 + 0];
        src_code[1] = unsigned_src[i * 3 + 1];
        src_code[2] = unsigned_src[i * 3 + 2];

        dst[i * 4 + 0] = base64_code[(src_code[0] >> 2) & 0x3F];
        dst[i * 4 + 1] = base64_code[(((src_code[0]&0x03)<<4) + (src_code[1]>>4)) & 0x3F];
        dst[i * 4 + 2] = base64_code[(((src_code[1]&0x0f)<<2) + (src_code[2]>>6)) & 0x3F];
        dst[i * 4 + 3] = base64_code[src_code[2]&0x3f];
    }

    i = len;
    j = srclen - len * 3;
    if (j > 0)
    {
        src_code[0] = unsigned_src[i * 3 + 0];
        src_code[1] = (j > 1) ? unsigned_src[i * 3 + 1] : 0;
        src_code[2] = 0;

        dst[i * 4 + 0] = base64_code[(src_code[0] >> 2) & 0x3F];
        dst[i * 4 + 1] = base64_code[(((src_code[0]&0x03)<<4) + (src_code[1]>>4)) & 0x3F];
        dst[i * 4 + 2] = (j == 1) ? '=' : base64_code[(((src_code[1]&0x0f)<<2)) & 0x3F];
        dst[i * 4 + 3] = '=';

        i++;
    }

    dst[i * 4] = '\0';
    return i * 4;
}


int base64_dec(const char *src, char *dst)
{
    unsigned char src_code[4];
    unsigned int i, len;
	int length = 0;
    len = strlen(src)/4 - 1;
    for (i = 0; i < len; i++)
    {
    	//debug_info("i=%d", i);
        src_code[0] = base64_index(src[i * 4 + 0]);
        src_code[1] = base64_index(src[i * 4 + 1]);
        src_code[2] = base64_index(src[i * 4 + 2]);
        src_code[3] = base64_index(src[i * 4 + 3]);

        dst[i * 3 + 0] = (src_code[0]<<2) + (src_code[1]>>4);
        dst[i * 3 + 1] = (src_code[1]<<4) + (src_code[2]>>2);
        dst[i * 3 + 2] = ((src_code[2]&0x03)<<6) + src_code[3];
        length += 3;
    }

    i = len;

    src_code[0] = base64_index(src[i * 4 + 0]);
    src_code[1] = base64_index(src[i * 4 + 1]);

    dst[i * 3 + 0] = (src_code[0]<<2) + (src_code[1]>>4);
    //length ++;
    if (src[i * 4 + 2] == '=')
    {
        dst[i * 3 + 1] = '\0';
		length += 1;
    }
    else if (src[i * 4 + 3] == '=')
    { 
        src_code[2] = base64_index(src[i * 4 + 2]);

        dst[i * 3 + 1] = (src_code[1]<<4) + (src_code[2]>>2);
        dst[i * 3 + 2] = '\0';
		length += 2;
    }
    else
    {
        src_code[2] = base64_index(src[i * 4 + 2]);
        src_code[3] = base64_index(src[i * 4 + 3]);

        dst[i * 3 + 1] = (src_code[1]<<4) + (src_code[2]>>2);
        dst[i * 3 + 2] = ((src_code[2]&0x03)<<6) + src_code[3];
        dst[i * 3 + 3] = '\0';
		length += 3;
    }
	return length;
}


void rc4_init(struct rc4_state *state, const char *key, int keylen)
{
    unsigned char j;
    int i, k;

    /* Initialize state with identity permutation */
    for (i = 0; i < 256; i++)
    {
        state->perm[i] = (unsigned char)i; 
    }
    
    state->index1 = 0;
    state->index2 = 0;

    /* Randomize the permutation using key data */
    for (j = i = k = 0; i < 256; i++) 
    {
        j += state->perm[i] + key[k]; 
        swap_bytes(&state->perm[i], &state->perm[j]);
        
        if (++k >= keylen)
        {
            k = 0;
        }
    }
}

void rc4_crypt(struct rc4_state *state, unsigned char *inbuf, unsigned char *outbuf, int buflen)
{
    int i;
    unsigned char j;

    for (i = 0; i < buflen; i++) 
    {
        /* Update modification indicies */
        state->index1++;
        state->index2 += state->perm[state->index1];

        /* Modify permutation */
        swap_bytes(&state->perm[state->index1],  &state->perm[state->index2]);

        /* Encrypt/decrypt next byte */
        j = state->perm[state->index1] + state->perm[state->index2];
        outbuf[i] = inbuf[i] ^ state->perm[j];
    }
}

void rc4_transfer(const char *key, const char *inbuf, char *outbuf, int length)
{
	unsigned char j;
	int i, keylen = strlen(key), buflen=length;
	unsigned char key2[256] = {0};
	struct rc4_state state;

	/* Initialize state with identity permutation */
	for (i = 0; i < 256; i++)
	{
		key2[i] = key[i % keylen];
		state.perm[i] = (unsigned char)i; 
	}

	state.index1 = 0;
	state.index2 = 0;

	/* Randomize the permutation using key data */
	for (j = i = 0; i < 256; i++)
	{
		j = (j+state.perm[i] + key2[i]) % 256;
		//        swap_bytes(&state.perm[i], &state.perm[j]);
		unsigned char tmp;
		tmp = state.perm[i];
			state.perm[i] = state.perm[j];
			state.perm[j] = tmp;
	}

	state.index1 = 0;
	state.index2 = 0;

	for (i = 0; i < buflen; i++) 
	{
		/* Update modification indicies */
		state.index1 = (state.index1 + 1) % 256;
		state.index2 = (state.index2 + state.perm[state.index1]) % 256;

		/* Modify permutation */
		//        swap_bytes(&state.perm[state.index1],  &state.perm[state.index2]);
		unsigned char tmp;
		tmp = state.perm[state.index1];
		state.perm[state.index1] = state.perm[state.index2];
		state.perm[state.index2] = tmp;

		/* Encrypt/decrypt next byte */
		j = (state.perm[state.index1] + state.perm[state.index2]) % 256;

		//printf("i=%d, j = %d\n", i , j);
		outbuf[i] = inbuf[i] ^ state.perm[j];
	}

}

void rc4base64_encode(const char* src, char* dst, const char* key)
{
	int32_t rc4len = strlen(src);
	char *rc4buf = (char*)calloc(rc4len, 1);
	rc4_transfer(key, (char*)src, rc4buf, rc4len);
	base64_enc2(rc4buf, dst, rc4len);
    free(rc4buf);
	return ;
}

void base64rc4_decode(const char* src, char* dst, const char* key)
{
	int32_t base64len = strlen(src);
	char *rc4buf = (char*)calloc(base64len, 1);    
	int32_t rc4len = base64_dec((char*)src, rc4buf);
	rc4_transfer(key, rc4buf, dst, rc4len);
    free(rc4buf);
	//printf("rc4len=%d", rc4len);
	dst[rc4len] = 0;
	return ;
}


