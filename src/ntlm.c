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

#include <glib.h>
#include <stdlib.h>
#include "util.h"
#include "ntlm.h"
#include "cipher.h"
#include <string.h>

struct type1_message {
	guint8    protocol[8];     /* 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0' */
	guint8    type;            /* 0x01 */
	guint8    zero1[3];
	short   flags;           /* 0xb203 */
	guint8    zero2[2];

	short   dom_len1;         /* domain string length */
	short   dom_len2;         /* domain string length */
	short   dom_off;         /* domain string offset */
	guint8    zero3[2];

	short   host_len1;        /* host string length */
	short   host_len2;        /* host string length */
	short   host_off;        /* host string offset (always 0x20) */
	guint8    zero4[2];

/*	guint8    host[*];         // host string (ASCII)
	guint8    dom[*];          // domain string (ASCII) */
};

struct type2_message {
	guint8    protocol[8];     /* 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0'*/
	guint8    type;            /* 0x02 */
	guint8    zero1[7];
	short   msg_len;         /* 0x28 */
	guint8    zero2[2];
	short   flags;           /* 0x8201 */
	guint8    zero3[2];

	guint8    nonce[8];        /* nonce */
	guint8    zero[8];
};

struct type3_message {
	guint8    protocol[8];     /* 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0'*/
	guint8    type;            /* 0x03 */
	guint8    zero1[3];

	short   lm_resp_len1;     /* LanManager response length (always 0x18)*/
	short   lm_resp_len2;     /* LanManager response length (always 0x18)*/
	short   lm_resp_off;     /* LanManager response offset */
	guint8    zero2[2];

	short   nt_resp_len1;     /* NT response length (always 0x18) */
	short   nt_resp_len2;     /* NT response length (always 0x18) */
	short   nt_resp_off;     /* NT response offset */
	guint8    zero3[2];

	short   dom_len1;         /* domain string length */
	short   dom_len2;         /* domain string length */
	short   dom_off;         /* domain string offset (always 0x40) */
	guint8    zero4[2];

	short   user_len1;        /* username string length */
	short   user_len2;        /* username string length */
	short   user_off;        /* username string offset */
	guint8    zero5[2];

	short   host_len1;        /* host string length */
	short   host_len2;        /* host string length */
	short   host_off;        /* host string offset */
	guint8    zero6[6];

	short   msg_len;         /* message length */
	guint8    zero7[2];

	short   flags;           /* 0x8201 */
	guint8    zero8[2];

/*	guint8    dom[*];          // domain string (unicode UTF-16LE)
	guint8    user[*];         // username string (unicode UTF-16LE)
	guint8    host[*];         // host string (unicode UTF-16LE)
	guint8    lm_resp[*];      // LanManager response
	guint8    nt_resp[*];      // NT response*/
};

gchar *gaim_ntlm_gen_type1(gchar *hostname, gchar *domain) {
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
	tmsg->flags = 0xb202;
	tmsg->dom_len1 = tmsg->dom_len2 = strlen(domain);
	tmsg->dom_off = 32+strlen(hostname);
	tmsg->host_len1 = tmsg->host_len2 = strlen(hostname);
	tmsg->host_off= 32;
	memcpy(msg+sizeof(struct type1_message),hostname,strlen(hostname));
	memcpy(msg+sizeof(struct type1_message)+strlen(hostname),domain,strlen(domain));

	return gaim_base64_encode(msg, sizeof(struct type1_message) + strlen(hostname) + strlen(domain));
}

gchar *gaim_ntlm_parse_type2(gchar *type2) {
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

gchar *gaim_ntlm_gen_type3(gchar *username, gchar *passw, gchar *hostname, gchar *domain, gchar *nonce) {
	char  lm_pw[14];
	unsigned char lm_hpw[21];
	gchar key[8];
	struct type3_message *tmsg = g_malloc0(sizeof(struct type3_message)+
		strlen(domain) + strlen(username) + strlen(hostname) + 24 +24);
	int   len = strlen(passw);
	unsigned char lm_resp[24], nt_resp[24];
	unsigned char magic[] = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
	unsigned char nt_hpw[21];
	int lennt;
	char  nt_pw[128];
	GaimCipher *cipher;
	GaimCipherContext *context;
	char *tmp = 0;
	int idx = 0;

	/* type3 message initialization */	
	tmsg->protocol[0] = 'N';
	tmsg->protocol[1] = 'T';
	tmsg->protocol[2] = 'L';
	tmsg->protocol[3] = 'M';
	tmsg->protocol[4] = 'S';
	tmsg->protocol[5] = 'S';
	tmsg->protocol[6] = 'P';
	tmsg->type = 0x03;
	tmsg->lm_resp_len1 = tmsg->lm_resp_len2 = 0x18;
	tmsg->lm_resp_off = sizeof(struct type3_message) + strlen(domain) + strlen(username) + strlen(hostname);
	tmsg->nt_resp_len1 = tmsg->nt_resp_len2 = 0x18;
	tmsg->nt_resp_off = sizeof(struct type3_message) + strlen(domain) + strlen(username) + strlen(hostname) + 0x18;

	tmsg->dom_len1 = tmsg->dom_len2 = strlen(domain);
	tmsg->dom_off = 0x40;

	tmsg->user_len1 = tmsg->user_len2 = strlen(username);
	tmsg->user_off = sizeof(struct type3_message) + strlen(domain);

	tmsg->host_len1 = tmsg->host_len2 = strlen(hostname);
	tmsg->host_off = sizeof(struct type3_message) + strlen(domain) + strlen(username);

	tmsg->msg_len = sizeof(struct type3_message) + strlen(domain) + strlen(username) + strlen(hostname) + 0x18 + 0x18;
	tmsg->flags = 0x8200;

	tmp = ((char*) tmsg) + sizeof(struct type3_message);
	strcpy(tmp, domain);
	tmp += strlen(domain);
	strcpy(tmp, username);
	tmp += strlen(username);
	strcpy(tmp, hostname);
	tmp += strlen(hostname);
	
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
	memcpy(tmp, lm_resp, 0x18);
	memcpy(tmp+0x18, nt_resp, 0x18);
	tmp = gaim_base64_encode((guchar*) tmsg, tmsg->msg_len);
	g_free(tmsg);
	return tmp;
}
