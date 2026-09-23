/* d3des.h -
 *
 *	Headers and defines for d3des.c
 *	Graven Imagery, 1992.
 *
 * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge
 *	(GEnie : OUTER; CIS : [71755,204])
 */

#ifndef UINT8
typedef unsigned char UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int UINT32;
#endif

#define SINGLE_KEY_SINGLE_TEXT	0
#define DOUBLE_KEY_SINGLE_TEXT	1
#define DOUBLE_KEY_DOUBLE_TEXT	2
#define TRIPLE_KEY_SINGLE_TEXT	3
#define TRIPLE_KEY_DOUBLE_TEXT	4

#define D2_DES		/* include double-length support */
#define D3_DES		/* include triple-length support */

#ifdef D3_DES
#ifndef D2_DES
#define D2_DES		/* D2_DES is needed for D3_DES */
#endif
#endif

#define EN0	0	/* MODE == encrypt */
#define DE1	1	/* MODE == decrypt */

/* A useful alias on 68000-ish machines, but NOT USED HERE. */

typedef union {
	UINT32 blok[2];
	UINT16 word[4];
	UINT8 byte[8];
	} M68K;

extern void deskey(UINT8 *, int);
/*		      hexkey[8]     MODE
 * Sets the internal key register according to the hexadecimal
 * key contained in the 8 bytes of hexkey, according to the DES,
 * for encryption or decryption according to MODE.
 */

extern void usekey(UINT32 *);
/*		    cookedkey[32]
 * Loads the internal key register with the data in cookedkey.
 */

extern void cpkey(UINT32 *);
/*		   cookedkey[32]
 * Copies the contents of the internal key register into the storage
 * located at &cookedkey[0].
 */

extern void des(UINT8 *, UINT8 *);
/*		    from[8]	      to[8]
 * Encrypts/Decrypts (according to the key currently loaded in the
 * internal key register) one block of eight bytes at address 'from'
 * into the block at address 'to'.  They can be the same.
 */

#ifdef D2_DES

#define desDkey(a,b)	des2key((a),(b))
extern void des2key(UINT8 *, int);
/*		      hexkey[16]     MODE
 * Sets the internal key registerS according to the hexadecimal
 * keyS contained in the 16 bytes of hexkey, according to the DES,
 * for DOUBLE encryption or decryption according to MODE.
 * NOTE: this clobbers all three key registers!
 */

extern void Ddes(UINT8 *, UINT8 *);
/*		    from[8]	      to[8]
 * Encrypts/Decrypts (according to the keyS currently loaded in the
 * internal key registerS) one block of eight bytes at address 'from'
 * into the block at address 'to'.  They can be the same.
 */

extern void D2des(UINT8 *, UINT8 *);
/*		    from[16]	      to[16]
 * Encrypts/Decrypts (according to the keyS currently loaded in the
 * internal key registerS) one block of SIXTEEN bytes at address 'from'
 * into the block at address 'to'.  They can be the same.
 */

extern void makekey(char *, UINT8 *);
/*		*password,	single-length key[8]
 * With a double-length default key, this routine hashes a NULL-terminated
 * string into an eight-byte random-looking key, suitable for use with the
 * deskey() routine.
 */

#define makeDkey(a,b)	make2key((a),(b))
extern void make2key(char *, UINT8 *);
/*		*password,	double-length key[16]
 * With a double-length default key, this routine hashes a NULL-terminated
 * string into a sixteen-byte random-looking key, suitable for use with the
 * des2key() routine.
 */

#ifndef D3_DES	/* D2_DES only */

#define useDkey(a)	use2key((a))
#define cpDkey(a)	cp2key((a))

extern void use2key(UINT32 *);
/*		    cookedkey[64]
 * Loads the internal key registerS with the data in cookedkey.
 * NOTE: this clobbers all three key registers!
 */

extern void cp2key(UINT32 *);
/*		   cookedkey[64]
 * Copies the contents of the internal key registerS into the storage
 * located at &cookedkey[0].
 */

#else	/* D3_DES too */

#define useDkey(a)	use3key((a))
#define cpDkey(a)	cp3key((a))

extern void des3key(UINT8 *, int);
/*		      hexkey[24]     MODE
 * Sets the internal key registerS according to the hexadecimal
 * keyS contained in the 24 bytes of hexkey, according to the DES,
 * for DOUBLE encryption or decryption according to MODE.
 */

extern void use3key(UINT32 *);
/*		    cookedkey[96]
 * Loads the 3 internal key registerS with the data in cookedkey.
 */

extern void cp3key(UINT32 *);
/*		   cookedkey[96]
 * Copies the contents of the 3 internal key registerS into the storage
 * located at &cookedkey[0].
 */

extern void make3key(char *, UINT8 *);
/*		*password,	triple-length key[24]
 * With a triple-length default key, this routine hashes a NULL-terminated
 * string into a twenty-four-byte random-looking key, suitable for use with
 * the des3key() routine.
 */

#endif	/* D3_DES */
#endif	/* D2_DES */

/* d3des.h V5.09 rwo 9208.04 15:06 Graven Imagery
 ********************************************************************/
