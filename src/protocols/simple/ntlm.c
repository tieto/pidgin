/**
 * @file ntlm.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
 *
 * hashing done according to description of NTLM on
 * http://www.innovation.ch/java/ntlm.html
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
 */

#include "util.h"
#include "ntlm.h"
#include "cipher.h"
#include <string.h>

gchar *ntlm_type1_message(gchar *hostname, gchar *domain) {
	char *msg = g_malloc0(sizeof(struct type1_message) + strlen(hostname) + strlen(domain));
	struct type1_message *tmsg = (struct type1_message*)msg;
	tmsg->protocol[0] = 'N';
	tmsg->protocol[1] = 'T';
	tmsg->protocol[2] = 'L';
	tmsg->protocol[3] = 'M';
	tmsg->protocol[4] = 'S';
	tmsg->protocol[5] = 'S';
	tmsg->protocol[6] = 'P';
	tmsg->protocol[7] = '\0';
	tmsg->type= 0x01;
	tmsg->flags = 0xb203;
	tmsg->dom_len1 = tmsg->dom_len2 = strlen(domain);
	tmsg->dom_off = 32+strlen(hostname);
	tmsg->host_len1 = tmsg->host_len2 = strlen(hostname);
	tmsg->host_off= 32;
	memcpy(msg+sizeof(struct type1_message),hostname,strlen(hostname));
	memcpy(msg+sizeof(struct type1_message)+strlen(hostname),domain,strlen(domain));

	return gaim_base64_encode(msg, sizeof(struct type1_message) + strlen(hostname) + strlen(domain));
}

gchar *ntlm_get_nonce(gchar *type2) {
	int retlen;
	static gchar nonce[8];
	struct type2_message *tmsg = (struct type2_message*)gaim_base64_decode(type2, &retlen);
	memcpy(nonce, tmsg->nonce, 8);
	g_free(tmsg);
	return nonce;
}

static void setup_des_key(unsigned char key_56[], char *key)
{
	key[0] = key_56[0];
	key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
	key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
	key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
	key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
	key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
	key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
	key[7] =  (key_56[6] << 1) & 0xFF;
}

/*
 * helper function for gaim cipher.c
 */
static void des_ecb_encrypt(char *plaintext, char *result, char *key) {
	GaimCipher *cipher;
	GaimCipherContext *context;
	int outlen;
	
	cipher = gaim_ciphers_find_cipher("des");
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_set_key(context, key);
	gaim_cipher_context_encrypt(context, plaintext, 8, result, &outlen);
	gaim_cipher_context_destroy(context);
}

/*
 * takes a 21 byte array and treats it as 3 56-bit DES keys. The
 * 8 byte plaintext is encrypted with each key and the resulting 24
 * bytes are stored in the results array.
 */
static void calc_resp(unsigned char *keys, unsigned char *plaintext, unsigned char *results)
{
	gchar key[8];
	setup_des_key(keys, key);
	des_ecb_encrypt(plaintext, results, key);

	setup_des_key(keys+7, key);
	des_ecb_encrypt(plaintext, (results+8), key);

	setup_des_key(keys+14, key);
	des_ecb_encrypt(plaintext, (results+16), key);
}

gchar *ntlm_type3_message(gchar *username, gchar *passw, gchar *hostname, gchar *domain, gchar *nonce) {
	char  lm_pw[14];
	unsigned char lm_hpw[21];
	gchar key[8];
	int   len = strlen(passw);
	unsigned char lm_resp[24], nt_resp[24];
	unsigned char magic[] = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
	unsigned char nt_hpw[21];
	int lennt;
	char  nt_pw[128];
	GaimCipher *cipher;
	GaimCipherContext *context;
	int idx = 0;
	
	if (len > 14)  len = 14;
	
	for (idx=0; idx<len; idx++)
		lm_pw[idx] = g_ascii_toupper(passw[idx]);
	for (; idx<14; idx++)
		lm_pw[idx] = 0;

	setup_des_key(lm_pw, key);
	des_ecb_encrypt(magic, lm_hpw, key);

	setup_des_key(lm_pw+7, key);
	des_ecb_encrypt(magic, lm_hpw+8, key);

	memset(lm_hpw+16, 0, 5);


	lennt = strlen(passw);
	for (idx=0; idx<lennt; idx++)
	{
		nt_pw[2*idx]   = passw[idx];
		nt_pw[2*idx+1] = 0;
	}

	cipher = gaim_ciphers_find_cipher("md4");
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, nt_pw, 2*lennt);
	gaim_cipher_context_digest(context, 21, nt_hpw, NULL);
	gaim_cipher_context_destroy(context);

	memset(nt_hpw+16, 0, 5);


	calc_resp(lm_hpw, nonce, lm_resp);
	calc_resp(nt_hpw, nonce, nt_resp);
	return NULL;
}
