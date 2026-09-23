/**
 * encrypt.h
 *
 * Functions used for encryption and decryption
 *
 * Created by Leon Liu  2013-01-24
 */

#ifndef __ENCRYPT_H__
#define __ENCRYPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


struct rc4_state 
{
	unsigned char  perm[256];
	unsigned char  index1;
	unsigned char  index2;
};

int base64_enc(const char *src, char *dst);
int base64_enc2(const char *src, char *dst,int srclen);
int base64_dec(const char *src, char *dst);

// Functionality:
//   Initialize a RC4 state buffer using the supplied key, which
//   can have arbitary length.
// Parameters:
//   @[in/out]state: RC4 state buffer
//   @[in]key: the key to use
//   @[in]keylen: key length
// Return:
//   void
void rc4_init(struct rc4_state *state, const char *key, int keylen);

// Functionality:
//   Encrypt some data using the supplied RC4 state buffer.
//   The input and output may be the same buffer. Since RC4
//   is a stream cypher, this function is used for both 
//   encryption and decryption.
// Parameters:
//   @[in]state: state buffer
//   @[in]inbuf: input buffer
//   @[out]outbuf: output buffer
//   @[in]buflen: output buffer length
// Return:
//   void
void rc4_crypt(struct rc4_state *state, unsigned char *inbuf, unsigned char *outbuf, int buflen);

void rc4_transfer(const char *key, const char *inbuf, char *outbuf, int length);

void rc4base64_encode(const char* src, char* dst, const char* key);

void base64rc4_decode(const char* src, char* dst, const char* key);


#ifdef __cplusplus
}
#endif

#endif


