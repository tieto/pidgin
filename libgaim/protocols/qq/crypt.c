/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

/*Notes: (QQ uses 16 rounds, and modified something...)

IN : 64  bits of data in v[0] - v[1].
OUT: 64  bits of data in w[0] - w[1].
KEY: 128 bits of key  in k[0] - k[3].

delta is chosen to be the real part of 
the golden ratio: Sqrt(5/4) - 1/2 ~ 0.618034 multiplied by 2^32. 

0x61C88647 is what we can track on the ASM codes.!!
*/

#include <string.h>

#include "crypt.h"
#include "debug.h"

/********************************************************************
 * encryption 
 *******************************************************************/

/* Tiny Encryption Algorithm (TEA) */
static void qq_encipher(guint32 *const v, const guint32 *const k, guint32 *const w)
{
	register guint32 y = g_ntohl(v[0]), 
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

static gint rand(void) {	/* it can be the real random seed function */
	return 0xdead;
}			/* override with number, convenient for debug */

/* 64-bit blocks and some kind of feedback mode of operation */
static void encrypt_block(guint8 *plain, guint8 *plain_pre_8, guint8 **crypted, 
		guint8 **crypted_pre_8, const guint8 *const key, gint *count, 
		gint *pos_in_block, gint *is_header) 
{
	/* prepare input text */
	if (!*is_header)
		*(guint64 *) plain ^= **(guint64 **) crypted_pre_8;

	/* encrypt it */
	qq_encipher((guint32 *) plain, (guint32 *) key, (guint32 *) *crypted);

	**(guint64 **) crypted ^= *(guint64 *) plain_pre_8;

	memcpy(plain_pre_8, plain, 8);	/* prepare next */

	*crypted_pre_8 = *crypted;	/* store position of previous 8 byte */
	*crypted += 8;			/* prepare next output */
	*count += 8;			/* outstrlen increase by 8 */
	*pos_in_block = 0;		/* back to start */
	*is_header = 0;			/* and exit header */
}					/* encrypt_block */

static void qq_encrypt(const guint8 *const instr, gint instrlen, 
		const guint8 *const key, 
		guint8 *outstr, gint *outstrlen_ptr)
{
	guint8 plain[8],		/* plain text buffer */
		plain_pre_8[8],		/* plain text buffer, previous 8 bytes */
		*crypted,		/* crypted text */
		*crypted_pre_8;		/* crypted text, previous 8 bytes */
	const guint8 *inp;		/* current position in instr */
	gint pos_in_block = 1,		/* loop in the byte */
		is_header = 1,		/* header is one byte */
		count = 0,		/* number of bytes being crypted */
		padding = 0;		/* number of padding stuff */

	pos_in_block = (instrlen + 0x0a) % 8;	/* header padding decided by instrlen */
	if (pos_in_block)
		pos_in_block = 8 - pos_in_block;

	/* initialization vector */
	plain[0] = (rand() & 0xf8) | pos_in_block;
	memset(plain + 1, rand() & 0xff, pos_in_block++);

	memset(plain_pre_8, 0x00, sizeof(plain_pre_8));

	crypted = crypted_pre_8 = outstr;

	padding = 1;		/* pad some stuff in header */
	while (padding <= 2) {	/* at most two bytes */
		if (pos_in_block < 8) {
			plain[pos_in_block++] = rand() & 0xff;
			padding++;
		}
		if (pos_in_block == 8) {
			encrypt_block(plain, plain_pre_8, &crypted, &crypted_pre_8, 
					key, &count, &pos_in_block, &is_header);
		}
	}

	inp = instr;
	while (instrlen > 0) {
		if (pos_in_block < 8) {
			plain[pos_in_block++] = *(inp++);
			instrlen--;
		}
		if (pos_in_block == 8) {
			encrypt_block(plain, plain_pre_8, &crypted, &crypted_pre_8, 
					key, &count, &pos_in_block, &is_header);
		}
	}

	padding = 1;		/* pad some stuff in tail */
	while (padding <= 7) {	/* at most seven bytes */
		if (pos_in_block < 8) {
			plain[pos_in_block++] = 0x00;
			padding++;
		}
		if (pos_in_block == 8) {
			encrypt_block(plain, plain_pre_8, &crypted, &crypted_pre_8, 
					key, &count, &pos_in_block, &is_header);
		}
	}

	*outstrlen_ptr = count;
}


/******************************************************************** 
 * decryption 
 ********************************************************************/

static void qq_decipher(guint32 *const v, const guint32 *const k, guint32 *const w)
{
	register guint32 y = g_ntohl(v[0]), 
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

static gint decrypt_block(const guint8 **crypt_buff, const gint instrlen, 
		const guint8 *const key, gint *context_start, 
		guint8 *decrypted, gint *pos_in_block)
{
	if (*context_start == instrlen)
		return 1;

	*(guint64 *) decrypted ^= **(guint64 **) crypt_buff;

	qq_decipher((guint32 *) decrypted, (guint32 *) key, (guint32 *) decrypted);

	*context_start += 8;
	*crypt_buff += 8;
	*pos_in_block = 0;

	return 1;
}

/* return 0 if failed, 1 otherwise */
static gint qq_decrypt(const guint8 *const instr, gint instrlen, 
		const guint8 *const key,
		guint8 *outstr, gint *outstrlen_ptr)
{
	guint8 decrypted[8], m[8], *outp;
	const guint8 *crypt_buff, *crypt_buff_pre_8;
	gint count, context_start, pos_in_block, padding;

	/* at least 16 bytes and %8 == 0 */
	if ((instrlen % 8) || (instrlen < 16)) { 
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", 
			"Ciphertext len is either too short or not a multiple of 8 bytes, read %d bytes\n", 
			instrlen);
		return 0;
	}
	/* get information from header */
	qq_decipher((guint32 *) instr, (guint32 *) key, (guint32 *) decrypted);
	pos_in_block = decrypted[0] & 0x7;
	count = instrlen - pos_in_block - 10;	/* this is the plaintext length */
	/* return if outstr buffer is not large enough or error plaintext length */
	if (*outstrlen_ptr < count || count < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Buffer len %d is less than real len %d", 
			*outstrlen_ptr, count);
		return 0;
	}

	memset(m, 0, 8);
	crypt_buff_pre_8 = m;
	*outstrlen_ptr = count;	/* everything is ok! set return string length */

	crypt_buff = instr + 8;	/* address of real data start */
	context_start = 8;	/* context is at the second block of 8 bytes */
	pos_in_block++;		/* start of paddng stuff */

	padding = 1;		/* at least one in header */
	while (padding <= 2) {	/* there are 2 byte padding stuff in header */
		if (pos_in_block < 8) {	/* bypass the padding stuff, it's nonsense data */
			pos_in_block++;
			padding++;
		}
		if (pos_in_block == 8) {
			crypt_buff_pre_8 = instr;
			if (!decrypt_block(&crypt_buff, instrlen, key, 
						&context_start, decrypted, &pos_in_block)) {
				gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error A");
				return 0;
			}
		}
	}

	outp = outstr;
	while (count != 0) {
		if (pos_in_block < 8) {
			*outp = crypt_buff_pre_8[pos_in_block] ^ decrypted[pos_in_block];
			outp++;
			count--;
			pos_in_block++;
		}
		if (pos_in_block == 8) {
			crypt_buff_pre_8 = crypt_buff - 8;
			if (!decrypt_block(&crypt_buff, instrlen, key, 
						&context_start, decrypted, &pos_in_block)) {
				gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error B");
				return 0;
			}
		}
	}

	for (padding = 1; padding < 8; padding++) {
		if (pos_in_block < 8) {
			if (crypt_buff_pre_8[pos_in_block] ^ decrypted[pos_in_block])
				return 0;
			pos_in_block++;
		}
		if (pos_in_block == 8) {
			crypt_buff_pre_8 = crypt_buff;
			if (!decrypt_block(&crypt_buff, instrlen, key, 
						&context_start, decrypted, &pos_in_block)) {
				gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error C");
				return 0;
			}
		}
	}
	return 1;
}

/* return 1 is succeed, otherwise return 0 */
gint qq_crypt(gint flag,
		const guint8 *const instr, gint instrlen, 
		const guint8 *const key, 
		guint8 *outstr, gint *outstrlen_ptr)
{
	if (flag == DECRYPT)
		return qq_decrypt(instr, instrlen, key, outstr, outstrlen_ptr);
	else if (flag == ENCRYPT)
		qq_encrypt(instr, instrlen, key, outstr, outstrlen_ptr);
	else 
		return 0;

	return 1;
}
