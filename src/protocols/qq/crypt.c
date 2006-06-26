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
 * OICQ encryption algorithm
 * Convert from ASM code provided by PerlOICQ
 * 
 * Puzzlebird, Nov-Dec 2002
 */

// START OF FILE
/*****************************************************************************/
/*Notes: (OICQ uses 0x10 iterations, and modified something...)

IN : 64  bits of data in v[0] - v[1].
OUT: 64  bits of data in w[0] - w[1].
KEY: 128 bits of key  in k[0] - k[3].

delta is chosen to be the real part of 
the golden ratio: Sqrt(5/4) - 1/2 ~ 0.618034 multiplied by 2^32. 

0x61C88647 is what we can track on the ASM codes.!!
*/

#ifndef _WIN32
#include <arpa/inet.h>
#else
#include "win32dep.h"
#endif

#include <string.h>

#include "crypt.h"
#include "debug.h"              // gaim_debug, by gfhuang

/*****************************************************************************/
static void qq_encipher(unsigned long *const v, const unsigned long *const k, unsigned long *const w)
{
	register unsigned long y = ntohl(v[0]), z = ntohl(v[1]), a = ntohl(k[0]), b = ntohl(k[1]), c = ntohl(k[2]), d = ntohl(k[3]), n = 0x10, sum = 0, delta = 0x9E3779B9;	/*  0x9E3779B9 - 0x100000000 = -0x61C88647 */

	while (n-- > 0) {
		sum += delta;
		y += ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
		z += ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
	}			// while

	w[0] = htonl(y);
	w[1] = htonl(z);
}				// qq_enciper

/*****************************************************************************/
static void qq_decipher(unsigned long *const v, const unsigned long *const k, unsigned long *const w)
{
	register unsigned long y = ntohl(v[0]), z = ntohl(v[1]), a = ntohl(k[0]), b = ntohl(k[1]), c = ntohl(k[2]), d = ntohl(k[3]), n = 0x10, sum = 0xE3779B90,	// why this ? must be related with n value
	    delta = 0x9E3779B9;

	/* sum = delta<<5, in general sum = delta * n */
	while (n-- > 0) {
		z -= ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
		y -= ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
		sum -= delta;
	}

	w[0] = htonl(y);
	w[1] = htonl(z);
}				// qq_decipher

/********************************************************************
 * encrypt part
 *******************************************************************/

static void qq_encrypt(unsigned char *instr, int instrlen, unsigned char *key, unsigned char *outstr, int *outstrlen_prt)
{
	unsigned char plain[8],	// plain text buffer
	 plain_pre_8[8],	// plain text buffer, previous 8 bytes
	*crypted,		// crypted text
	*crypted_pre_8,		// crypted test, previous 8 bytes
	*inp;			// current position in instr
	int pos_in_byte = 1,	// loop in the byte 
	    is_header = 1,	// header is one byte
	    count = 0,		// number of bytes being crypted
	    padding = 0;	// number of padding stuff

	int rand(void) {	// it can be the real random seed function
		return 0xdead;
	}			// override with number, convenient for debug

  /*** we encrypt every eight byte ***/
	void encrypt_every_8_byte(void) {
		for (pos_in_byte = 0; pos_in_byte < 8; pos_in_byte++) {
			if (is_header) {
				plain[pos_in_byte] ^= plain_pre_8[pos_in_byte];
			} else {
				plain[pos_in_byte] ^= crypted_pre_8[pos_in_byte];
			}
		}		// prepare plain text
		qq_encipher((unsigned long *) plain, (unsigned long *) key, (unsigned long *) crypted);	// encrypt it

		for (pos_in_byte = 0; pos_in_byte < 8; pos_in_byte++) {
			crypted[pos_in_byte] ^= plain_pre_8[pos_in_byte];
		}
		memcpy(plain_pre_8, plain, 8);	// prepare next

		crypted_pre_8 = crypted;	// store position of previous 8 byte
		crypted += 8;	// prepare next output
		count += 8;	// outstrlen increase by 8
		pos_in_byte = 0;	// back to start
		is_header = 0;	// and exit header
	}			// encrypt_every_8_byte

	pos_in_byte = (instrlen + 0x0a) % 8;	// header padding decided by instrlen
	if (pos_in_byte) {
		pos_in_byte = 8 - pos_in_byte;
	}
	plain[0] = (rand() & 0xf8) | pos_in_byte;

	memset(plain + 1, rand() & 0xff, pos_in_byte++);
	memset(plain_pre_8, 0x00, sizeof(plain_pre_8));

	crypted = crypted_pre_8 = outstr;

	padding = 1;		// pad some stuff in header
	while (padding <= 2) {	// at most two byte 
		if (pos_in_byte < 8) {
			plain[pos_in_byte++] = rand() & 0xff;
			padding++;
		}
		if (pos_in_byte == 8) {
			encrypt_every_8_byte();
		}
	}

	inp = instr;
	while (instrlen > 0) {
		if (pos_in_byte < 8) {
			plain[pos_in_byte++] = *(inp++);
			instrlen--;
		}
		if (pos_in_byte == 8) {
			encrypt_every_8_byte();
		}
	}

	padding = 1;		// pad some stuff in tailer
	while (padding <= 7) {	// at most sever byte
		if (pos_in_byte < 8) {
			plain[pos_in_byte++] = 0x00;
			padding++;
		}
		if (pos_in_byte == 8) {
			encrypt_every_8_byte();
		}
	}

	*outstrlen_prt = count;
}				// qq_encrypt


/******************************************************************** 
 * [decrypt part]
 * return 0 if failed, otherwise return 1
 ********************************************************************/

static int qq_decrypt(unsigned char *instr, int instrlen, unsigned char *key, unsigned char *outstr, int *outstrlen_ptr)
{
	unsigned char decrypted[8], m[8], *crypt_buff, *crypt_buff_pre_8, *outp;
	int count, context_start, pos_in_byte, padding;

	int decrypt_every_8_byte(void) {
		for (pos_in_byte = 0; pos_in_byte < 8; pos_in_byte++) {
			if (context_start + pos_in_byte >= instrlen)
				return 1;
			decrypted[pos_in_byte] ^= crypt_buff[pos_in_byte];
		}
		qq_decipher((unsigned long *) decrypted, (unsigned long *) key, (unsigned long *) decrypted);

		context_start += 8;
		crypt_buff += 8;
		pos_in_byte = 0;
		return 1;
	}			// decrypt_every_8_byte

	// at least 16 bytes and %8 == 0
	if ((instrlen % 8) || (instrlen < 16)) { 
		//debug info by gfhuang
		 gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Packet len is not times of 8 bytes, read %d bytes\n", instrlen);
		return 0;
	}
	// get information from header
	qq_decipher((unsigned long *) instr, (unsigned long *) key, (unsigned long *) decrypted);
	pos_in_byte = decrypted[0] & 0x7;
	count = instrlen - pos_in_byte - 10;	// this is the plaintext length
	// return if outstr buffer is not large enought or error plaintext length
	if (*outstrlen_ptr < count || count < 0) {
		 gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Buffer len %d is less than real len %d", *outstrlen_ptr, count);
		return 0;
	}

	memset(m, 0, 8);
	crypt_buff_pre_8 = m;
	*outstrlen_ptr = count;	// everything is ok! set return string length

	crypt_buff = instr + 8;	// address of real data start 
	context_start = 8;	// context is at the second 8 byte
	pos_in_byte++;		// start of paddng stuff

	padding = 1;		// at least one in header
	while (padding <= 2) {	// there are 2 byte padding stuff in header
		if (pos_in_byte < 8) {	// bypass the padding stuff, none sense data
			pos_in_byte++;
			padding++;
		}
		if (pos_in_byte == 8) {
			crypt_buff_pre_8 = instr;
			if (!decrypt_every_8_byte()) {
				 gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error A");
				return 0;
			}
		}
	}			// while

	outp = outstr;
	while (count != 0) {
		if (pos_in_byte < 8) {
			*outp = crypt_buff_pre_8[pos_in_byte] ^ decrypted[pos_in_byte];
			outp++;
			count--;
			pos_in_byte++;
		}
		if (pos_in_byte == 8) {
			crypt_buff_pre_8 = crypt_buff - 8;
			if (!decrypt_every_8_byte()) {
				 gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error B");
				return 0;
			}
		}
	}			// while

	for (padding = 1; padding < 8; padding++) {
		if (pos_in_byte < 8) {
			if (crypt_buff_pre_8[pos_in_byte] ^ decrypted[pos_in_byte])
				return 0;
			pos_in_byte++;
		}
		if (pos_in_byte == 8) {
			crypt_buff_pre_8 = crypt_buff;
			if (!decrypt_every_8_byte()) {
				 gaim_debug(GAIM_DEBUG_ERROR, "QQ", "decrypt every 8 bytes error C");
				return 0;
			}
		}
	}			// for
	return 1;
}				// qq_decrypt

/*****************************************************************************/
/* This is the Public Function */
// return 1 is succeed, otherwise return 0
int qq_crypt(unsigned char flag,
	     unsigned char *instr, int instrlen, unsigned char *key, unsigned char *outstr, int *outstrlen_ptr)
{
	if (flag == DECRYPT)
		return qq_decrypt(instr, instrlen, key, outstr, outstrlen_ptr);
	else if (flag == ENCRYPT)
		qq_encrypt(instr, instrlen, key, outstr, outstrlen_ptr);

	return 1;		// flag must be DECRYPT or ENCRYPT
}				// qq_crypt

/*****************************************************************************/
// END OF FILE
