/**
 * @file qq_crypt.c
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * QQ encryption algorithm
 * Convert from ASM code provided by PerlOICQ
 * 
 * Puzzlebird, Nov-Dec 2002
 */

/* Notes: (QQ uses 16 rounds, and modified something...)

IN : 64  bits of data in v[0] - v[1].
OUT: 64  bits of data in w[0] - w[1].
KEY: 128 bits of key  in k[0] - k[3].

delta is chosen to be the real part of 
the golden ratio: Sqrt(5/4) - 1/2 ~ 0.618034 multiplied by 2^32. 

0x61C88647 is what we can track on the ASM codes.!!
*/

#include <string.h>

#include "debug.h"
#include "qq_crypt.h"

#if 0
void show_binary(char *psztitle, const guint8 *const buffer, gint bytes)
{
	printf("== %s %d ==\r\n", psztitle, bytes);
	gint i, j, ch;
	for (i = 0; i < bytes; i += 16) {
		/* length label */
		printf("%07x: ", i);
		
		/* dump hex value */
		for (j = 0; j < 16; j++) {
			if (j == 8) {
				printf(" -");
			}
			if ((i + j) < bytes)
				printf(" %02x", buffer[i + j]);
			else
				printf("   ");
		}
		
		printf("  ");
		
		
		/* dump ascii value */
		for (j = 0; j < 16 && (i + j) < bytes; j++) {
			ch = buffer[i + j] & 127;
			if (ch < ' ' || ch == 127)
				printf(".");
			else
				printf("%c", ch);
		}
		printf("\r\n");
	}
	printf("========\r\n");
}
#else

#define show_binary(args... )		/* nothing */

#endif

/********************************************************************
 * encryption 
 *******************************************************************/

/* Tiny Encryption Algorithm (TEA) */
static inline void qq_encipher(guint32 *const v, const guint32 *const k, guint32 *const w)
{
	register guint32
		y = g_ntohl(v[0]), 
		 z = g_ntohl(v[1]), 
		 a = g_ntohl(k[0]), 
		 b = g_ntohl(k[1]), 
		 c = g_ntohl(k[2]), 
		 d = g_ntohl(k[3]), 
		 n = 0x10, 
		 sum = 0, 
		 delta = 0x9E3779B9;	/*  0x9E3779B9 - 0x100000000 = -0x61C88647 */

	while (n-- > 0) {
		sum += delta;
		y += ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
		z += ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
	}

	w[0] = g_htonl(y);
	w[1] = g_htonl(z);
}

/* it can be the real random seed function */
/* override with number, convenient for debug */
#ifdef DEBUG
static gint crypt_rand(void) {	
	return 0xdead; 
}
#else
#include <stdlib.h>
#define crypt_rand() rand()
#endif

/* 64-bit blocks and some kind of feedback mode of operation */
static inline void encrypt_out(guint8 *crypted, const gint crypted_len, const guint8 *key) 
{
	/* ships in encipher */
	guint32 plain32[2];
	guint32 p32_prev[2];
	guint32 key32[4];
	guint32 crypted32[2];
	guint32 c32_prev[2];
	
	guint8 *crypted_ptr;
	gint count64;
	
	/* prepare at first */
	crypted_ptr = crypted;
	
	memcpy(crypted32, crypted_ptr, sizeof(crypted32));
	c32_prev[0] = crypted32[0]; c32_prev[1] = crypted32[1];
	
	p32_prev[0] = 0; p32_prev[1] = 0;
	plain32[0] = crypted32[0] ^ p32_prev[0]; plain32[1] = crypted32[1] ^ p32_prev[1];
	
	g_memmove(key32, key, 16);
	count64 = crypted_len / 8;
	while (count64-- > 0){
		/* encrypt it */
		qq_encipher(plain32, key32, crypted32);
		
		crypted32[0] ^= p32_prev[0]; crypted32[1] ^= p32_prev[1];
		
		/* store curr 64 bits crypted */
		g_memmove(crypted_ptr, crypted32, sizeof(crypted32));
		
		/* set prev */
		p32_prev[0] = plain32[0]; p32_prev[1] = plain32[1];
		c32_prev[0] = crypted32[0]; c32_prev[1] = crypted32[1];
		
		/* set next 64 bits want to crypt*/
		if (count64 > 0) {
			crypted_ptr += 8;
			memcpy(crypted32, crypted_ptr, sizeof(crypted32));
			plain32[0] = crypted32[0] ^ c32_prev[0]; plain32[1] = crypted32[1] ^ c32_prev[1];
		}
	}
}

/* length of crypted buffer must be plain_len + 16*/
gint qq_encrypt(guint8* crypted, const guint8* const plain, const gint plain_len, const guint8* const key)
{
	guint8 *crypted_ptr = crypted;		/* current position of dest */
	gint pos, padding;
	
	padding = (plain_len + 10) % 8;
	if (padding) {
		padding = 8 - padding;
	}

	pos = 0;

	/* set first byte as padding len */
	crypted_ptr[pos] = (rand() & 0xf8) | padding;
	pos++;

	/* extra 2 bytes */
	padding += 2;

	/* faster a little
	memset(crypted_ptr + pos, rand() & 0xff, padding);
	pos += padding;
	*/

	/* more random */
	while (padding--) {
		crypted_ptr[pos++] = rand() & 0xff;
	}

	g_memmove(crypted_ptr + pos, plain, plain_len);
	pos += plain_len;

	/* header padding len + plain len must be multiple of 8
	 * tail pading len is always 8 - (1st byte)
	 */
	memset(crypted_ptr + pos, 0x00, 7);
	pos += 7;

	show_binary("After padding", crypted, pos);

	encrypt_out(crypted, pos, key);

	show_binary("Encrypted", crypted, pos);
	return pos;
}

/******************************************************************** 
 * decryption 
 ********************************************************************/

static inline void qq_decipher(guint32 *const v, const guint32 *const k, guint32 *const w)
{
	register guint32
		y = g_ntohl(v[0]), 
		z = g_ntohl(v[1]), 
		a = g_ntohl(k[0]), 
		b = g_ntohl(k[1]), 
		c = g_ntohl(k[2]), 
		d = g_ntohl(k[3]), 
		n = 0x10, 
		sum = 0xE3779B90,	/* why this ? must be related with n value */
		delta = 0x9E3779B9;

	/* sum = delta<<5, in general sum = delta * n */
	while (n-- > 0) {
		z -= ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
		y -= ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
		sum -= delta;
	}

	w[0] = g_htonl(y);
	w[1] = g_htonl(z);
}

static inline gint decrypt_out(guint8 *dest, gint crypted_len, const guint8* const key) 
{
	gint plain_len;
	guint32 key32[4];
	guint32 crypted32[2];
	guint32 c32_prev[2];
	guint32 plain32[2];
	guint32 p32_prev[2];
	gint count64;
	gint padding;
	guint8 *crypted_ptr = dest;

	/* decrypt first 64 bit */
	memcpy(key32, key, sizeof(key32));
	memcpy(crypted32, crypted_ptr, sizeof(crypted32));
	c32_prev[0] = crypted32[0]; c32_prev[1] = crypted32[1];

	qq_decipher(crypted32, key32, p32_prev);
	memcpy(crypted_ptr, p32_prev, sizeof(p32_prev));

	/* check padding len */
	padding = 2 + (crypted_ptr[0] & 0x7);
	if (padding < 2) {
		padding += 8;
	}
	plain_len = crypted_len - 1 - padding - 7;
	if( plain_len < 0 )	{
		return -2;
	}
	
	count64 = crypted_len / 8;
	while (--count64 > 0){
		c32_prev[0] = crypted32[0]; c32_prev[1] = crypted32[1];
		crypted_ptr += 8;

		memcpy(crypted32, crypted_ptr, sizeof(crypted32));
		p32_prev[0] ^= crypted32[0]; p32_prev[1] ^= crypted32[1];

		qq_decipher(p32_prev, key32, p32_prev);
		
		plain32[0] = p32_prev[0] ^ c32_prev[0]; plain32[1] = p32_prev[1] ^ c32_prev[1];
		memcpy(crypted_ptr, plain32, sizeof(plain32));
	}

	return plain_len;
}

/* length of plain buffer must be equal to crypted_len */
gint qq_decrypt(guint8 *plain, const guint8* const crypted, const gint crypted_len, const guint8* const key)
{
	gint plain_len = 0;
	gint hdr_padding;
	gint pos;

	/* at least 16 bytes and %8 == 0 */
	if ((crypted_len % 8) || (crypted_len < 16)) { 
		return -1;
	}

	memcpy(plain, crypted, crypted_len);

	plain_len = decrypt_out(plain, crypted_len, key);
	if (plain_len < 0) {
		return plain_len;	/* invalid first 64 bits */
	}

	show_binary("Decrypted with padding", plain, crypted_len);

	/* check last 7 bytes is zero or not? */
	for (pos = crypted_len - 1; pos > crypted_len - 8; pos--) {
		if (plain[pos] != 0) {
			return -3;
		}
	}
	if (plain_len == 0) {
		return plain_len;
	}

	hdr_padding = crypted_len - plain_len - 7;
	g_memmove(plain, plain + hdr_padding, plain_len);

	return plain_len;
}

