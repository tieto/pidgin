/*
 * Original des taken from gpg
 *
 * des.c - DES encryption/decryption Algorithm
 *  Copyright (C) 1998 Free Software Foundation, Inc.
 *
 *  Please see below for more legal information!
 *  
 *   According to the definition of DES in FIPS PUB 46-2 from December 1993.
 *   For a description of triple encryption, see:
 *     Bruce Schneier: Applied Cryptography. Second Edition.
 *     John Wiley & Sons, 1996. ISBN 0-471-12845-7. Pages 358 ff.
 *  
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "descipher.h"
#include "enums.h"

#include <string.h>

/******************************************************************************
 * Structs
 *****************************************************************************/
#define PURPLE_DES_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_DES_CIPHER, PurpleDESCipherPrivate))

typedef struct {
	guint32 encrypt_subkeys[32];
	guint32 decrypt_subkeys[32];
} PurpleDESCipherPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_NONE,
	PROP_KEY,
	PROP_LAST,
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/*  
 *  The s-box values are permuted according to the 'primitive function P'
 */
static guint32 sbox1[64] = {
	0x00808200, 0x00000000, 0x00008000, 0x00808202, 0x00808002, 0x00008202,
	0x00000002, 0x00008000, 0x00000200, 0x00808200, 0x00808202, 0x00000200,
	0x00800202, 0x00808002, 0x00800000, 0x00000002, 0x00000202, 0x00800200,
	0x00800200, 0x00008200, 0x00008200, 0x00808000, 0x00808000, 0x00800202,
	0x00008002, 0x00800002, 0x00800002, 0x00008002, 0x00000000, 0x00000202,
	0x00008202, 0x00800000, 0x00008000, 0x00808202, 0x00000002, 0x00808000,
	0x00808200, 0x00800000, 0x00800000, 0x00000200, 0x00808002, 0x00008000,
	0x00008200, 0x00800002, 0x00000200, 0x00000002, 0x00800202, 0x00008202,
	0x00808202, 0x00008002, 0x00808000, 0x00800202, 0x00800002, 0x00000202,
	0x00008202, 0x00808200, 0x00000202, 0x00800200, 0x00800200, 0x00000000,
	0x00008002, 0x00008200, 0x00000000, 0x00808002
};

static guint32 sbox2[64] = {
	0x40084010, 0x40004000, 0x00004000, 0x00084010, 0x00080000, 0x00000010,
	0x40080010, 0x40004010, 0x40000010, 0x40084010, 0x40084000, 0x40000000,
	0x40004000, 0x00080000, 0x00000010, 0x40080010, 0x00084000, 0x00080010,
	0x40004010, 0x00000000, 0x40000000, 0x00004000, 0x00084010, 0x40080000,
	0x00080010, 0x40000010, 0x00000000, 0x00084000, 0x00004010, 0x40084000,
	0x40080000, 0x00004010, 0x00000000, 0x00084010, 0x40080010, 0x00080000,
	0x40004010, 0x40080000, 0x40084000, 0x00004000, 0x40080000, 0x40004000,
	0x00000010, 0x40084010, 0x00084010, 0x00000010, 0x00004000, 0x40000000,
	0x00004010, 0x40084000, 0x00080000, 0x40000010, 0x00080010, 0x40004010,
	0x40000010, 0x00080010, 0x00084000, 0x00000000, 0x40004000, 0x00004010,
	0x40000000, 0x40080010, 0x40084010, 0x00084000
};

static guint32 sbox3[64] = {
	0x00000104, 0x04010100, 0x00000000, 0x04010004, 0x04000100, 0x00000000,
	0x00010104, 0x04000100, 0x00010004, 0x04000004, 0x04000004, 0x00010000,
	0x04010104, 0x00010004, 0x04010000, 0x00000104, 0x04000000, 0x00000004,
	0x04010100, 0x00000100, 0x00010100, 0x04010000, 0x04010004, 0x00010104,
	0x04000104, 0x00010100, 0x00010000, 0x04000104, 0x00000004, 0x04010104,
	0x00000100, 0x04000000, 0x04010100, 0x04000000, 0x00010004, 0x00000104,
	0x00010000, 0x04010100, 0x04000100, 0x00000000, 0x00000100, 0x00010004,
	0x04010104, 0x04000100, 0x04000004, 0x00000100, 0x00000000, 0x04010004,
	0x04000104, 0x00010000, 0x04000000, 0x04010104, 0x00000004, 0x00010104,
	0x00010100, 0x04000004, 0x04010000, 0x04000104, 0x00000104, 0x04010000,
	0x00010104, 0x00000004, 0x04010004, 0x00010100
};

static guint32 sbox4[64] = {
	0x80401000, 0x80001040, 0x80001040, 0x00000040, 0x00401040, 0x80400040,
	0x80400000, 0x80001000, 0x00000000, 0x00401000, 0x00401000, 0x80401040,
	0x80000040, 0x00000000, 0x00400040, 0x80400000, 0x80000000, 0x00001000,
	0x00400000, 0x80401000, 0x00000040, 0x00400000, 0x80001000, 0x00001040,
	0x80400040, 0x80000000, 0x00001040, 0x00400040, 0x00001000, 0x00401040,
	0x80401040, 0x80000040, 0x00400040, 0x80400000, 0x00401000, 0x80401040,
	0x80000040, 0x00000000, 0x00000000, 0x00401000, 0x00001040, 0x00400040,
	0x80400040, 0x80000000, 0x80401000, 0x80001040, 0x80001040, 0x00000040,
	0x80401040, 0x80000040, 0x80000000, 0x00001000, 0x80400000, 0x80001000,
	0x00401040, 0x80400040, 0x80001000, 0x00001040, 0x00400000, 0x80401000,
	0x00000040, 0x00400000, 0x00001000, 0x00401040
};

static guint32 sbox5[64] = {
	0x00000080, 0x01040080, 0x01040000, 0x21000080, 0x00040000, 0x00000080,
	0x20000000, 0x01040000, 0x20040080, 0x00040000, 0x01000080, 0x20040080,
	0x21000080, 0x21040000, 0x00040080, 0x20000000, 0x01000000, 0x20040000,
	0x20040000, 0x00000000, 0x20000080, 0x21040080, 0x21040080, 0x01000080,
	0x21040000, 0x20000080, 0x00000000, 0x21000000, 0x01040080, 0x01000000,
	0x21000000, 0x00040080, 0x00040000, 0x21000080, 0x00000080, 0x01000000,
	0x20000000, 0x01040000, 0x21000080, 0x20040080, 0x01000080, 0x20000000,
	0x21040000, 0x01040080, 0x20040080, 0x00000080, 0x01000000, 0x21040000,
	0x21040080, 0x00040080, 0x21000000, 0x21040080, 0x01040000, 0x00000000,
	0x20040000, 0x21000000, 0x00040080, 0x01000080, 0x20000080, 0x00040000,
	0x00000000, 0x20040000, 0x01040080, 0x20000080
};

static guint32 sbox6[64] = {
	0x10000008, 0x10200000, 0x00002000, 0x10202008, 0x10200000, 0x00000008,
	0x10202008, 0x00200000, 0x10002000, 0x00202008, 0x00200000, 0x10000008,
	0x00200008, 0x10002000, 0x10000000, 0x00002008, 0x00000000, 0x00200008,
	0x10002008, 0x00002000, 0x00202000, 0x10002008, 0x00000008, 0x10200008,
	0x10200008, 0x00000000, 0x00202008, 0x10202000, 0x00002008, 0x00202000,
	0x10202000, 0x10000000, 0x10002000, 0x00000008, 0x10200008, 0x00202000,
	0x10202008, 0x00200000, 0x00002008, 0x10000008, 0x00200000, 0x10002000,
	0x10000000, 0x00002008, 0x10000008, 0x10202008, 0x00202000, 0x10200000,
	0x00202008, 0x10202000, 0x00000000, 0x10200008, 0x00000008, 0x00002000,
	0x10200000, 0x00202008, 0x00002000, 0x00200008, 0x10002008, 0x00000000,
	0x10202000, 0x10000000, 0x00200008, 0x10002008
};

static guint32 sbox7[64] = {
	0x00100000, 0x02100001, 0x02000401, 0x00000000, 0x00000400, 0x02000401,
	0x00100401, 0x02100400, 0x02100401, 0x00100000, 0x00000000, 0x02000001,
	0x00000001, 0x02000000, 0x02100001, 0x00000401, 0x02000400, 0x00100401,
	0x00100001, 0x02000400, 0x02000001, 0x02100000, 0x02100400, 0x00100001,
	0x02100000, 0x00000400, 0x00000401, 0x02100401, 0x00100400, 0x00000001,
	0x02000000, 0x00100400, 0x02000000, 0x00100400, 0x00100000, 0x02000401,
	0x02000401, 0x02100001, 0x02100001, 0x00000001, 0x00100001, 0x02000000,
	0x02000400, 0x00100000, 0x02100400, 0x00000401, 0x00100401, 0x02100400,
	0x00000401, 0x02000001, 0x02100401, 0x02100000, 0x00100400, 0x00000000,
	0x00000001, 0x02100401, 0x00000000, 0x00100401, 0x02100000, 0x00000400,
	0x02000001, 0x02000400, 0x00000400, 0x00100001
};

static guint32 sbox8[64] = {
	0x08000820, 0x00000800, 0x00020000, 0x08020820, 0x08000000, 0x08000820,
	0x00000020, 0x08000000, 0x00020020, 0x08020000, 0x08020820, 0x00020800,
	0x08020800, 0x00020820, 0x00000800, 0x00000020, 0x08020000, 0x08000020,
	0x08000800, 0x00000820, 0x00020800, 0x00020020, 0x08020020, 0x08020800,
	0x00000820, 0x00000000, 0x00000000, 0x08020020, 0x08000020, 0x08000800,
	0x00020820, 0x00020000, 0x00020820, 0x00020000, 0x08020800, 0x00000800,
	0x00000020, 0x08020020, 0x00000800, 0x00020820, 0x08000800, 0x00000020,
	0x08000020, 0x08020000, 0x08020020, 0x08000000, 0x00020000, 0x08000820,
	0x00000000, 0x08020820, 0x00020020, 0x08000020, 0x08020000, 0x08000800,
	0x08000820, 0x00000000, 0x08020820, 0x00020800, 0x00020800, 0x00000820,
	0x00000820, 0x00020020, 0x08000000, 0x08020800
};

/*
 * These two tables are part of the 'permuted choice 1' function.
 * In this implementation several speed improvements are done.
 */
static guint32 leftkey_swap[16] = {
	0x00000000, 0x00000001, 0x00000100, 0x00000101, 0x00010000, 0x00010001,
	0x00010100, 0x00010101, 0x01000000, 0x01000001, 0x01000100, 0x01000101,
	0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static guint32 rightkey_swap[16] = {
	0x00000000, 0x01000000, 0x00010000, 0x01010000, 0x00000100, 0x01000100,
	0x00010100, 0x01010100, 0x00000001, 0x01000001, 0x00010001, 0x01010001,
	0x00000101, 0x01000101, 0x00010101, 0x01010101,
};

/*
 *  Numbers of left shifts per round for encryption subkey schedule
 *  To calculate the decryption key scheduling we just reverse the
 *  ordering of the subkeys so we can omit the table for decryption
 *  subkey schedule.
 */
static guint8 encrypt_rotate_tab[16] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};


/******************************************************************************
 * Helpers
 *****************************************************************************/
/*
 * Macro to swap bits across two words
 */
#define DO_PERMUTATION(a, temp, b, offset, mask)	\
	temp = ((a>>offset) ^ b) & mask;				\
	b ^= temp;										\
	a ^= temp<<offset;

/*
 * This performs the 'initial permutation' for the data to be encrypted or
 * decrypted
 */
#define INITIAL_PERMUTATION(left, temp, right)			\
	DO_PERMUTATION(left, temp, right, 4, 0x0f0f0f0f)	\
	DO_PERMUTATION(left, temp, right, 16, 0x0000ffff)	\
	DO_PERMUTATION(right, temp, left, 2, 0x33333333)	\
	DO_PERMUTATION(right, temp, left, 8, 0x00ff00ff)	\
	DO_PERMUTATION(left, temp, right, 1, 0x55555555)

/*
 * The 'inverse initial permutation'
 */
#define FINAL_PERMUTATION(left, temp, right)			\
    DO_PERMUTATION(left, temp, right, 1, 0x55555555)	\
	DO_PERMUTATION(right, temp, left, 8, 0x00ff00ff)	\
	DO_PERMUTATION(right, temp, left, 2, 0x33333333)	\
	DO_PERMUTATION(left, temp, right, 16, 0x0000ffff)	\
	DO_PERMUTATION(left, temp, right, 4, 0x0f0f0f0f)

/*
 * A full DES round including 'expansion function', 'sbox substitution'
 * and 'primitive function P' but without swapping the left and right word.
 */
#define DES_ROUND(from, to, work, subkey)			\
	work = ((from<<1) | (from>>31)) ^ *subkey++;	\
	to ^= sbox8[  work      & 0x3f ];				\
	to ^= sbox6[ (work>>8)  & 0x3f ];				\
	to ^= sbox4[ (work>>16) & 0x3f ];				\
	to ^= sbox2[ (work>>24) & 0x3f ];				\
	work = ((from>>3) | (from<<29)) ^ *subkey++;	\
	to ^= sbox7[  work      & 0x3f ];				\
	to ^= sbox5[ (work>>8)  & 0x3f ];				\
	to ^= sbox3[ (work>>16) & 0x3f ];				\
	to ^= sbox1[ (work>>24) & 0x3f ];


/*
 * Macros to convert 8 bytes from/to 32bit words
 */
#define READ_64BIT_DATA(data, left, right)									\
	left  = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];	\
	right = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];

#define WRITE_64BIT_DATA(data, left, right)							\
	data[0] = (left >> 24) &0xff; data[1] = (left >> 16) &0xff;		\
	data[2] = (left >> 8) &0xff; data[3] = left &0xff;				\
	data[4] = (right >> 24) &0xff; data[5] = (right >> 16) &0xff;	\
	data[6] = (right >> 8) &0xff; data[7] = right &0xff;

/******************************************************************************
 * Cipher Stuff
 *****************************************************************************/
/*
 * des_key_schedule():    Calculate 16 subkeys pairs (even/odd) for
 *            16 encryption rounds.
 *            To calculate subkeys for decryption the caller
 *                have to reorder the generated subkeys.
 *     
 *        rawkey:       8 Bytes of key data
 *        subkey:       Array of at least 32 guint32s. Will be filled
 *              with calculated subkeys.
 *     
 */
static void
purple_des_cipher_key_schedule(const guint8 * rawkey, guint32 * subkey) {
	guint32 left, right, work;
	int round;

	READ_64BIT_DATA (rawkey, left, right)

	DO_PERMUTATION (right, work, left, 4, 0x0f0f0f0f)
	DO_PERMUTATION (right, work, left, 0, 0x10101010)

	left = (leftkey_swap[(left >>  0) & 0xf] << 3)
		 | (leftkey_swap[(left >>  8) & 0xf] << 2)
		 | (leftkey_swap[(left >> 16) & 0xf] << 1)
		 | (leftkey_swap[(left >> 24) & 0xf]     )
		 | (leftkey_swap[(left >>  5) & 0xf] << 7)
		 | (leftkey_swap[(left >> 13) & 0xf] << 6)
		 | (leftkey_swap[(left >> 21) & 0xf] << 5)
		 | (leftkey_swap[(left >> 29) & 0xf] << 4);

	left &= 0x0fffffff;

	right = (rightkey_swap[(right >>  1) & 0xf] << 3)
		  | (rightkey_swap[(right >>  9) & 0xf] << 2)
		  | (rightkey_swap[(right >> 17) & 0xf] << 1)
		  | (rightkey_swap[(right >> 25) & 0xf]     )
		  | (rightkey_swap[(right >>  4) & 0xf] << 7)
		  | (rightkey_swap[(right >> 12) & 0xf] << 6)
		  | (rightkey_swap[(right >> 20) & 0xf] << 5)
		  | (rightkey_swap[(right >> 28) & 0xf] << 4);

	right &= 0x0fffffff;

	for (round = 0; round < 16; ++round) {
		left = ((left << encrypt_rotate_tab[round]) |
			    (left >> (28 - encrypt_rotate_tab[round]))) & 0x0fffffff;
		right = ((right << encrypt_rotate_tab[round]) |
				 (right >> (28 - encrypt_rotate_tab[round]))) & 0x0fffffff;

		*subkey++ = ((left  <<  4) & 0x24000000)
				  | ((left  << 28) & 0x10000000)
				  | ((left  << 14) & 0x08000000)
				  | ((left  << 18) & 0x02080000)
				  | ((left  <<  6) & 0x01000000)
				  | ((left  <<  9) & 0x00200000)
				  | ((left  >>  1) & 0x00100000)
				  | ((left  << 10) & 0x00040000)
				  | ((left  <<  2) & 0x00020000)
				  | ((left  >> 10) & 0x00010000)
				  | ((right >> 13) & 0x00002000)
				  | ((right >>  4) & 0x00001000)
				  | ((right <<  6) & 0x00000800)
				  | ((right >>  1) & 0x00000400)
				  | ((right >> 14) & 0x00000200)
				  | (right         & 0x00000100)
				  | ((right >>  5) & 0x00000020)
				  | ((right >> 10) & 0x00000010)
				  | ((right >>  3) & 0x00000008)
				  | ((right >> 18) & 0x00000004)
				  | ((right >> 26) & 0x00000002)
				  | ((right >> 24) & 0x00000001);

		*subkey++ = ((left  << 15) & 0x20000000)
				  | ((left  << 17) & 0x10000000)
				  | ((left  << 10) & 0x08000000)
				  | ((left  << 22) & 0x04000000)
				  | ((left  >>  2) & 0x02000000)
				  | ((left  <<  1) & 0x01000000)
				  | ((left  << 16) & 0x00200000)
				  | ((left  << 11) & 0x00100000)
				  | ((left  <<  3) & 0x00080000)
				  | ((left  >>  6) & 0x00040000)
				  | ((left  << 15) & 0x00020000)
				  | ((left  >>  4) & 0x00010000)
				  | ((right >>  2) & 0x00002000)
				  | ((right <<  8) & 0x00001000)
				  | ((right >> 14) & 0x00000808)
				  | ((right >>  9) & 0x00000400)
				  | ((right)       & 0x00000200)
				  | ((right <<  7) & 0x00000100)
				  | ((right >>  7) & 0x00000020)
				  | ((right >>  3) & 0x00000011)
				  | ((right <<  2) & 0x00000004)
				  | ((right >> 21) & 0x00000002);
	}
}

/*
 * Fill a DES context with subkeys calculated from a 64bit key.
 * Does not check parity bits, but simply ignore them.
 * Does not check for weak keys.
 */
static void
purple_des_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len) {
	PurpleDESCipher *des_cipher = PURPLE_DES_CIPHER(cipher);
	PurpleDESCipherPrivate *priv = PURPLE_DES_CIPHER_GET_PRIVATE(des_cipher);
	int i;

	g_return_if_fail(len == 8);

	purple_des_cipher_key_schedule(key, priv->encrypt_subkeys);

	for(i = 0; i < 32; i += 2) {
		priv->decrypt_subkeys[i] = priv->encrypt_subkeys[30 - i];
		priv->decrypt_subkeys[i + 1] = priv->encrypt_subkeys[31 - i];
	}

	g_object_notify(G_OBJECT(cipher), "key");
}

static size_t
purple_des_cipher_get_key_size(PurpleCipher *cipher)
{
	return 8;
}

/*
 * Electronic Codebook Mode DES encryption/decryption of data according to
 * 'mode'.
 */
int
purple_des_cipher_ecb_crypt(PurpleDESCipher *des_cipher, const guint8 * from, guint8 * to,
			  int mode)
{
	guint32 left, right, work;
	guint32 *keys;
	PurpleDESCipherPrivate *priv = PURPLE_DES_CIPHER_GET_PRIVATE(des_cipher);

	keys = mode ? priv->decrypt_subkeys :
				  priv->encrypt_subkeys;

	READ_64BIT_DATA (from, left, right)
	INITIAL_PERMUTATION (left, work, right)

	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)
	DES_ROUND (right, left, work, keys) DES_ROUND (left, right, work, keys)

	FINAL_PERMUTATION (right, work, left)
	WRITE_64BIT_DATA (to, right, left)

	return 0;
}

static ssize_t
purple_des_cipher_encrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleDESCipher *des_cipher = PURPLE_DES_CIPHER(cipher);
	gint offset = 0, i = 0, tmp;
	guint8 buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	ssize_t out_len;

	g_return_val_if_fail(out_size >= in_len, -1);

	while(offset + 8 <= in_len) {
		purple_des_cipher_ecb_crypt(des_cipher, input + offset, output + offset, 0);
		offset += 8;
	}

	out_len = in_len;

	if(offset<in_len) {
		out_len += in_len - offset;
		g_return_val_if_fail(out_size >= out_len, -1);
		tmp = offset;
		while(tmp<in_len) {
			buf[i++] = input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(des_cipher, buf, output + offset, 0);
	}

	return out_len;
}

static ssize_t
purple_des_cipher_decrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleDESCipher *des_cipher = PURPLE_DES_CIPHER(cipher);
	int offset = 0, i = 0, tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	ssize_t out_len;

	g_return_val_if_fail(out_size >= in_len, -1);

	while(offset + 8 <= in_len) {
		purple_des_cipher_ecb_crypt(des_cipher, input + offset, output + offset, 1);
		offset += 8;
	}

	out_len = in_len;
	if(offset<in_len) {
		out_len += in_len - offset;
		g_return_val_if_fail(out_size >= out_len, -1);
		tmp = offset;
		while(tmp<in_len) {
			buf[i++] = input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(des_cipher, buf, output + offset, 1);
	}

	return out_len;
}

static const gchar*
purple_des_cipher_get_name(PurpleCipher *cipher)
{
	return "des";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_des_cipher_set_property(GObject *obj, guint param_id,
							   const GValue *value, GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_KEY:
			purple_cipher_set_key(cipher, (guchar *)g_value_get_string(value),
										purple_des_cipher_get_key_size(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_des_cipher_class_init(PurpleDESCipherClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	obj_class->set_property = purple_des_cipher_set_property;

	cipher_class->encrypt = purple_des_cipher_encrypt;
	cipher_class->decrypt = purple_des_cipher_decrypt;
	cipher_class->set_key = purple_des_cipher_set_key;
	cipher_class->get_key_size = purple_des_cipher_get_key_size;
	cipher_class->get_name = purple_des_cipher_get_name;

	pspec = g_param_spec_string("key", "key", "key", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_KEY, pspec);

	g_type_class_add_private(klass, sizeof(PurpleDESCipherPrivate));
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_des_cipher_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleDESCipherClass),
			.class_init = (GClassInitFunc)purple_des_cipher_class_init,
			.instance_size = sizeof(PurpleDESCipher),
			.instance_init = (GInstanceInitFunc)purple_cipher_reset,
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleDESCipher",
									  &info, 0);
	}

	return type;
}

/**
 * purple_des_cipher_new:
 *
 * Creates a new #PurpleCipher instance which implements the DES block cipher.
 *
 * Return Value: The new DES implementation of #PurpleCipher.
 */
PurpleCipher *
purple_des_cipher_new(void) {
	return g_object_new(PURPLE_TYPE_DES_CIPHER, NULL);
}

