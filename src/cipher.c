/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Original md5
 * Copyright (C) 2001-2003  Christophe Devine <c.devine@cr0.net>
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
#include <string.h>
#include <stdio.h>

#include "internal.h"
#include "cipher.h"
#include "debug.h"
#include "signals.h"
#include "value.h"

/*******************************************************************************
 * MD5
 ******************************************************************************/
struct MD5Context {
	guint32 total[2];
	guint32 state[4];
	guint8 buffer[64];
};

#define MD5_GET_GUINT32(n,b,i) {			\
	(n) = ((guint32)(b) [(i)    ]      )	\
		| ((guint32)(b) [(i) + 1] <<  8)	\
		| ((guint32)(b) [(i) + 2] << 16)	\
		| ((guint32)(b) [(i) + 3] << 24);	\
}
#define MD5_PUT_GUINT32(n,b,i) { 			\
	(b)[(i)    ] = (guint8)((n)      );		\
    (b)[(i) + 1] = (guint8)((n) >>  8);     \
	(b)[(i) + 2] = (guint8)((n) >> 16); 	\
	(b)[(i) + 3] = (guint8)((n) >> 24); 	\
}

static void
md5_init(GaimCipherContext *context, gpointer extra) {
	struct MD5Context *md5_context;

	md5_context = g_new0(struct MD5Context, 1);

	gaim_cipher_context_set_data(context, md5_context);

	gaim_cipher_context_reset(context, extra);
}

static void
md5_reset(GaimCipherContext *context, gpointer extra) {
	struct MD5Context *md5_context;

	md5_context = gaim_cipher_context_get_data(context);

	md5_context->total[0] = 0;
	md5_context->total[1] = 0;

	md5_context->state[0] = 0x67452301;
	md5_context->state[1] = 0xEFCDAB89;
	md5_context->state[2] = 0x98BADCFE;
	md5_context->state[3] = 0x10325476;

	memset(md5_context->buffer, 0, sizeof(md5_context->buffer));
}

static void
md5_uninit(GaimCipherContext *context) {
	struct MD5Context *md5_context;

	gaim_cipher_context_reset(context, NULL);

	md5_context = gaim_cipher_context_get_data(context);
	memset(md5_context, 0, sizeof(md5_context));

	g_free(md5_context);
	md5_context = NULL;
}

static void
md5_process(struct MD5Context *md5_context, const guint8 data[64]) {
	guint32 X[16], A, B, C, D;

	A = md5_context->state[0];
	B = md5_context->state[1];
	C = md5_context->state[2];
	D = md5_context->state[3];

	MD5_GET_GUINT32(X[ 0], data,  0);
	MD5_GET_GUINT32(X[ 1], data,  4);
	MD5_GET_GUINT32(X[ 2], data,  8);
	MD5_GET_GUINT32(X[ 3], data, 12);
	MD5_GET_GUINT32(X[ 4], data, 16);
	MD5_GET_GUINT32(X[ 5], data, 20);
	MD5_GET_GUINT32(X[ 6], data, 24);
	MD5_GET_GUINT32(X[ 7], data, 28);
	MD5_GET_GUINT32(X[ 8], data, 32);
	MD5_GET_GUINT32(X[ 9], data, 36);
	MD5_GET_GUINT32(X[10], data, 40);
	MD5_GET_GUINT32(X[11], data, 44);
	MD5_GET_GUINT32(X[12], data, 48);
	MD5_GET_GUINT32(X[13], data, 52);
	MD5_GET_GUINT32(X[14], data, 56);
	MD5_GET_GUINT32(X[15], data, 60);

	#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))
	#define P(a,b,c,d,k,s,t) {		\
		a += F(b,c,d) + X[k] + t;	\
		a = S(a,s) + b;				\
	}

	/* first pass */
	#define F(x,y,z) (z ^ (x & (y ^ z)))
	P(A, B, C, D,  0,  7, 0xD76AA478);
	P(D, A, B, C,  1, 12, 0xE8C7B756);
	P(C, D, A, B,  2, 17, 0x242070DB);
	P(B, C, D, A,  3, 22, 0xC1BDCEEE);
	P(A, B, C, D,  4,  7, 0xF57C0FAF);
	P(D, A, B, C,  5, 12, 0x4787C62A);
	P(C, D, A, B,  6, 17, 0xA8304613);
	P(B, C, D, A,  7, 22, 0xFD469501);
	P(A, B, C, D,  8,  7, 0x698098D8);
	P(D, A, B, C,  9, 12, 0x8B44F7AF);
	P(C, D, A, B, 10, 17, 0xFFFF5BB1);
	P(B, C, D, A, 11, 22, 0x895CD7BE);
	P(A, B, C, D, 12,  7, 0x6B901122);
	P(D, A, B, C, 13, 12, 0xFD987193);
	P(C, D, A, B, 14, 17, 0xA679438E);
	P(B, C, D, A, 15, 22, 0x49B40821);
	#undef F

	/* second pass */
	#define F(x,y,z) (y ^ (z & (x ^ y)))
	P(A, B, C, D,  1,  5, 0xF61E2562);
	P(D, A, B, C,  6,  9, 0xC040B340);
	P(C, D, A, B, 11, 14, 0x265E5A51);
	P(B, C, D, A,  0, 20, 0xE9B6C7AA);
	P(A, B, C, D,  5,  5, 0xD62F105D);
	P(D, A, B, C, 10,  9, 0x02441453);
	P(C, D, A, B, 15, 14, 0xD8A1E681);
	P(B, C, D, A,  4, 20, 0xE7D3FBC8);
	P(A, B, C, D,  9,  5, 0x21E1CDE6);
	P(D, A, B, C, 14,  9, 0xC33707D6);
	P(C, D, A, B,  3, 14, 0xF4D50D87);
	P(B, C, D, A,  8, 20, 0x455A14ED);
	P(A, B, C, D, 13,  5, 0xA9E3E905);
	P(D, A, B, C,  2,  9, 0xFCEFA3F8);
	P(C, D, A, B,  7, 14, 0x676F02D9);
	P(B, C, D, A, 12, 20, 0x8D2A4C8A);
	#undef F
    
	/* third pass */
	#define F(x,y,z) (x ^ y ^ z)
	P(A, B, C, D,  5,  4, 0xFFFA3942);
	P(D, A, B, C,  8, 11, 0x8771F681);
	P(C, D, A, B, 11, 16, 0x6D9D6122);
	P(B, C, D, A, 14, 23, 0xFDE5380C);
	P(A, B, C, D,  1,  4, 0xA4BEEA44);
	P(D, A, B, C,  4, 11, 0x4BDECFA9);
	P(C, D, A, B,  7, 16, 0xF6BB4B60);
	P(B, C, D, A, 10, 23, 0xBEBFBC70);
	P(A, B, C, D, 13,  4, 0x289B7EC6);
	P(D, A, B, C,  0, 11, 0xEAA127FA);
	P(C, D, A, B,  3, 16, 0xD4EF3085);
	P(B, C, D, A,  6, 23, 0x04881D05);
	P(A, B, C, D,  9,  4, 0xD9D4D039);
	P(D, A, B, C, 12, 11, 0xE6DB99E5);
	P(C, D, A, B, 15, 16, 0x1FA27CF8);
	P(B, C, D, A,  2, 23, 0xC4AC5665);
	#undef F

	/* forth pass */
	#define F(x,y,z) (y ^ (x | ~z))
	P(A, B, C, D,  0,  6, 0xF4292244);
	P(D, A, B, C,  7, 10, 0x432AFF97);
	P(C, D, A, B, 14, 15, 0xAB9423A7);
	P(B, C, D, A,  5, 21, 0xFC93A039);
	P(A, B, C, D, 12,  6, 0x655B59C3);
	P(D, A, B, C,  3, 10, 0x8F0CCC92);
	P(C, D, A, B, 10, 15, 0xFFEFF47D);
	P(B, C, D, A,  1, 21, 0x85845DD1);
	P(A, B, C, D,  8,  6, 0x6FA87E4F);
	P(D, A, B, C, 15, 10, 0xFE2CE6E0);
	P(C, D, A, B,  6, 15, 0xA3014314);
	P(B, C, D, A, 13, 21, 0x4E0811A1);
	P(A, B, C, D,  4,  6, 0xF7537E82);
	P(D, A, B, C, 11, 10, 0xBD3AF235);
	P(C, D, A, B,  2, 15, 0x2AD7D2BB);
	P(B, C, D, A,  9, 21, 0xEB86D391);
	#undef F
	#undef P
	#undef S

	md5_context->state[0] += A;
	md5_context->state[1] += B;
	md5_context->state[2] += C;
	md5_context->state[3] += D;
}

static void
md5_append(GaimCipherContext *context, const guint8 *data, size_t len) {
	struct MD5Context *md5_context = NULL;
	guint32 left = 0, fill = 0;

	g_return_if_fail(context != NULL);

	md5_context = gaim_cipher_context_get_data(context);
	g_return_if_fail(md5_context != NULL);

	left = md5_context->total[0] & 0x3F;
	fill = 64 - left;

	md5_context->total[0] += len;
	md5_context->total[0] &= 0xFFFFFFFF;

	if(md5_context->total[0] < len)
		md5_context->total[1]++;

	if(left && len >= fill) {
		memcpy((md5_context->buffer + left), data, fill);
		md5_process(md5_context, md5_context->buffer);
		len -= fill;
		data += fill;
		left = 0;
	}

	while(len >= 64) {
		md5_process(md5_context, data);
		len -= 64;
		data += 64;
	}

	if(len) {
		memcpy((md5_context->buffer + left), data, len);
	}
}

static gboolean
md5_digest(GaimCipherContext *context, size_t in_len, guint8 digest[16],
		   size_t *out_len)
{
	struct MD5Context *md5_context = NULL;
	guint32 last, pad;
	guint32 high, low;
	guint8 message[8];
	guint8 padding[64] = {
		0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	g_return_val_if_fail(in_len >= 16, FALSE);

	md5_context = gaim_cipher_context_get_data(context);

	high = (md5_context->total[0] >> 29)
		 | (md5_context->total[1] << 3);
	low = (md5_context->total[0] << 3);

	MD5_PUT_GUINT32(low, message, 0);
	MD5_PUT_GUINT32(high, message, 4);

	last = md5_context->total[0] & 0x3F;
	pad = (last < 56) ? (56 - last) : (120 - last);

	md5_append(context, padding, pad);
	md5_append(context, message, 8);

	MD5_PUT_GUINT32(md5_context->state[0], digest, 0);
	MD5_PUT_GUINT32(md5_context->state[1], digest, 4);
	MD5_PUT_GUINT32(md5_context->state[2], digest, 8);
	MD5_PUT_GUINT32(md5_context->state[3], digest, 12);

	if(out_len)
		*out_len = 16;

	return TRUE;
}

static GaimCipherOps MD5Ops = {
	NULL,			/* Set option */
	NULL,			/* Get option */
	md5_init,		/* init */
	md5_reset,		/* reset */
	md5_uninit,		/* uninit */
	NULL,			/* set iv */
	md5_append,		/* append */
	md5_digest,		/* digest */
	NULL,			/* encrypt */
	NULL,			/* decrypt */
	NULL,			/* set salt */
	NULL,			/* get salt size */
	NULL,			/* set key */
	NULL			/* get key size */
};

/*******************************************************************************
 * SHA-1
 ******************************************************************************/
#define SHA1_ROTL(X,n) ((((X) << (n)) | ((X) >> (32-(n)))) & 0xFFFFFFFF)

struct SHA1Context {
	guint32 H[5];
	guint32 W[80];

    gint lenW;

	guint32 sizeHi;
	guint32 sizeLo;
};

static void
sha1_hash_block(struct SHA1Context *sha1_ctx) {
	gint i;
	guint32 A, B, C, D, E, T;

	for(i = 16; i < 80; i++) {
		sha1_ctx->W[i] = SHA1_ROTL(sha1_ctx->W[i -  3] ^
								   sha1_ctx->W[i -  8] ^
								   sha1_ctx->W[i - 14] ^
								   sha1_ctx->W[i - 16], 1);
	}

	A = sha1_ctx->H[0];
	B = sha1_ctx->H[1];
	C = sha1_ctx->H[2];
	D = sha1_ctx->H[3];
	E = sha1_ctx->H[4];

	for(i = 0; i < 20; i++) {
		T = (SHA1_ROTL(A, 5) + (((C ^ D) & B) ^ D) + E + sha1_ctx->W[i] + 0x5A827999) & 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1_ROTL(B, 30);
		B = A;
		A = T;
	}

	for(i = 20; i < 40; i++) {
		T = (SHA1_ROTL(A, 5) + (B ^ C ^ D) + E + sha1_ctx->W[i] + 0x6ED9EBA1) & 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1_ROTL(B, 30);
		B = A;
		A = T;
	}

	for(i = 40; i < 60; i++) {
		T = (SHA1_ROTL(A, 5) + ((B & C) | (D & (B | C))) + E + sha1_ctx->W[i] + 0x8F1BBCDC) & 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1_ROTL(B, 30);
		B = A;
		A = T;
	}

	for(i = 60; i < 80; i++) {
		T = (SHA1_ROTL(A, 5) + (B ^ C ^ D) + E + sha1_ctx->W[i] + 0xCA62C1D6) & 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1_ROTL(B, 30);
		B = A;
		A = T;
	}

	sha1_ctx->H[0] += A;
	sha1_ctx->H[1] += B;
	sha1_ctx->H[2] += C;
	sha1_ctx->H[3] += D;
	sha1_ctx->H[4] += E;	
}

static void
sha1_set_opt(GaimCipherContext *context, const gchar *name, void *value) {
	struct SHA1Context *ctx;

	ctx = gaim_cipher_context_get_data(context);

	if(!strcmp(name, "sizeHi")) {
		ctx->sizeHi = GPOINTER_TO_INT(value);
	} else if(!strcmp(name, "sizeLo")) {
		ctx->sizeLo = GPOINTER_TO_INT(value);
	} else if(!strcmp(name, "lenW")) {
		ctx->lenW = GPOINTER_TO_INT(value);
	}
}

static void *
sha1_get_opt(GaimCipherContext *context, const gchar *name) {
	struct SHA1Context *ctx;

	ctx = gaim_cipher_context_get_data(context);

	if(!strcmp(name, "sizeHi")) {
		return GINT_TO_POINTER(ctx->sizeHi);
	} else if(!strcmp(name, "sizeLo")) {
		return GINT_TO_POINTER(ctx->sizeLo);
	} else if(!strcmp(name, "lenW")) {
		return GINT_TO_POINTER(ctx->lenW);
	}

	return NULL;
}

static void
sha1_init(GaimCipherContext *context, void *extra) {
	struct SHA1Context *sha1_ctx;

	sha1_ctx = g_new0(struct SHA1Context, 1);

	gaim_cipher_context_set_data(context, sha1_ctx);

	gaim_cipher_context_reset(context, extra);
}

static void
sha1_reset(GaimCipherContext *context, void *extra) {
	struct SHA1Context *sha1_ctx;
	gint i;

	sha1_ctx = gaim_cipher_context_get_data(context);

	g_return_if_fail(sha1_ctx);

	sha1_ctx->lenW = 0;
	sha1_ctx->sizeHi = 0;
	sha1_ctx->sizeLo = 0;

	sha1_ctx->H[0] = 0x67452301;
	sha1_ctx->H[1] = 0xEFCDAB89;
	sha1_ctx->H[2] = 0x98BADCFE;
	sha1_ctx->H[3] = 0x10325476;
	sha1_ctx->H[4] = 0xC3D2E1F0;

	for(i = 0; i < 80; i++)
		sha1_ctx->W[i] = 0;
}

static void
sha1_uninit(GaimCipherContext *context) {
	struct SHA1Context *sha1_ctx;

	gaim_cipher_context_reset(context, NULL);

	sha1_ctx = gaim_cipher_context_get_data(context);

	memset(sha1_ctx, 0, sizeof(struct SHA1Context));

	g_free(sha1_ctx);
	sha1_ctx = NULL;
}


static void
sha1_append(GaimCipherContext *context, const guint8 *data, size_t len) {
	struct SHA1Context *sha1_ctx;
	gint i;

	sha1_ctx = gaim_cipher_context_get_data(context);

	g_return_if_fail(sha1_ctx);

	for(i = 0; i < len; i++) {
		sha1_ctx->W[sha1_ctx->lenW / 4] <<= 8;
		sha1_ctx->W[sha1_ctx->lenW / 4] |= data[i];

		if((++sha1_ctx->lenW) % 64 == 0) {
			sha1_hash_block(sha1_ctx);
			sha1_ctx->lenW = 0;
		}

		sha1_ctx->sizeLo += 8;
		sha1_ctx->sizeHi += (sha1_ctx->sizeLo < 8);
	}
}

static gboolean
sha1_digest(GaimCipherContext *context, size_t in_len, guint8 digest[20],
			size_t *out_len)
{
	struct SHA1Context *sha1_ctx;
	guint8 pad0x80 = 0x80, pad0x00 = 0x00;
	guint8 padlen[8];
	gint i;

	g_return_val_if_fail(in_len >= 20, FALSE);

	sha1_ctx = gaim_cipher_context_get_data(context);

	g_return_val_if_fail(sha1_ctx, FALSE);

	padlen[0] = (guint8)((sha1_ctx->sizeHi >> 24) & 255);
	padlen[1] = (guint8)((sha1_ctx->sizeHi >> 16) & 255);
	padlen[2] = (guint8)((sha1_ctx->sizeHi >> 8) & 255);
	padlen[3] = (guint8)((sha1_ctx->sizeHi >> 0) & 255);
	padlen[4] = (guint8)((sha1_ctx->sizeLo >> 24) & 255);
	padlen[5] = (guint8)((sha1_ctx->sizeLo >> 16) & 255);
	padlen[6] = (guint8)((sha1_ctx->sizeLo >> 8) & 255);
	padlen[7] = (guint8)((sha1_ctx->sizeLo >> 0) & 255);

	/* pad with a 1, then zeroes, then length */
	gaim_cipher_context_append(context, &pad0x80, 1);
	while(sha1_ctx->lenW != 56)
		gaim_cipher_context_append(context, &pad0x00, 1);
	gaim_cipher_context_append(context, padlen, 8);

	for(i = 0; i < 20; i++) {
		digest[i] = (guint8)(sha1_ctx->H[i / 4] >> 24);
		sha1_ctx->H[i / 4] <<= 8;
	}

	gaim_cipher_context_reset(context, NULL);

	if(out_len)
		*out_len = 20;

	return TRUE;
}

static GaimCipherOps SHA1Ops = {
	sha1_set_opt,	/* Set Option		*/
	sha1_get_opt,	/* Get Option		*/
	sha1_init,		/* init				*/
	sha1_reset,		/* reset			*/
	sha1_uninit,	/* uninit			*/
	NULL,			/* set iv			*/
	sha1_append,	/* append			*/
	sha1_digest,	/* digest			*/
	NULL,			/* encrypt			*/
	NULL,			/* decrypt			*/
	NULL,			/* set salt			*/
	NULL,			/* get salt size	*/
	NULL,			/* set key			*/
	NULL			/* get key size		*/
};

/*******************************************************************************
 * Structs
 ******************************************************************************/
struct _GaimCipher {
	gchar *name;
	GaimCipherOps *ops;
	guint ref;
};

struct _GaimCipherContext {
	GaimCipher *cipher;
	gpointer data;
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GList *ciphers = NULL;

/******************************************************************************
 * GaimCipher API
 *****************************************************************************/
const gchar *
gaim_cipher_get_name(GaimCipher *cipher) {
	g_return_val_if_fail(cipher, NULL);

	return cipher->name;
}

guint
gaim_cipher_get_capabilities(GaimCipher *cipher) {
	GaimCipherOps *ops = NULL;
	guint caps = 0;

	g_return_val_if_fail(cipher, 0);

	ops = cipher->ops;
	g_return_val_if_fail(ops, 0);

	if(ops->set_option)
		caps |= GAIM_CIPHER_CAPS_SET_OPT;
	if(ops->get_option)
		caps |= GAIM_CIPHER_CAPS_GET_OPT;
	if(ops->init)
		caps |= GAIM_CIPHER_CAPS_INIT;
	if(ops->reset)
		caps |= GAIM_CIPHER_CAPS_RESET;
	if(ops->uninit)
		caps |= GAIM_CIPHER_CAPS_UNINIT;
	if(ops->set_iv)
		caps |= GAIM_CIPHER_CAPS_SET_IV;
	if(ops->append)
		caps |= GAIM_CIPHER_CAPS_APPEND;
	if(ops->digest)
		caps |= GAIM_CIPHER_CAPS_DIGEST;
	if(ops->encrypt)
		caps |= GAIM_CIPHER_CAPS_ENCRYPT;
	if(ops->decrypt)
		caps |= GAIM_CIPHER_CAPS_DECRYPT;
	if(ops->set_salt)
		caps |= GAIM_CIPHER_CAPS_SET_SALT;
	if(ops->get_salt_size)
		caps |= GAIM_CIPHER_CAPS_GET_SALT_SIZE;
	if(ops->set_key)
		caps |= GAIM_CIPHER_CAPS_SET_KEY;
	if(ops->get_key_size)
		caps |= GAIM_CIPHER_CAPS_GET_KEY_SIZE;

	return caps;
}

gboolean
gaim_cipher_digest_region(const gchar *name, const guint8 *data,
						  size_t data_len, size_t in_len,
						  guint8 digest[], size_t *out_len)
{
	GaimCipher *cipher;
	GaimCipherContext *context;
	gboolean ret = FALSE;

	g_return_val_if_fail(name, FALSE);
	g_return_val_if_fail(data, FALSE);

	cipher = gaim_ciphers_find_cipher(name);

	g_return_val_if_fail(cipher, FALSE);

	if(!cipher->ops->append || !cipher->ops->digest) {
		gaim_debug_info("cipher", "gaim_cipher_region failed: "
						"the %s cipher does not support appending and or "
						"digesting.", cipher->name);
		return FALSE;
	}

	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, data, data_len);
	ret = gaim_cipher_context_digest(context, in_len, digest, out_len);
	gaim_cipher_context_destroy(context);

	return ret;
}

/******************************************************************************
 * GaimCiphers API
 *****************************************************************************/
GaimCipher *
gaim_ciphers_find_cipher(const gchar *name) {
	GaimCipher *cipher;
	GList *l;

	g_return_val_if_fail(name, NULL);

	for(l = ciphers; l; l = l->next) {
		cipher = GAIM_CIPHER(l->data);

		if(!g_ascii_strcasecmp(cipher->name, name))
			return cipher;
	}

	return NULL;
}

GaimCipher *
gaim_ciphers_register_cipher(const gchar *name, GaimCipherOps *ops) {
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(name, NULL);
	g_return_val_if_fail(ops, NULL);
	g_return_val_if_fail(!gaim_ciphers_find_cipher(name), NULL);

	cipher = g_new0(GaimCipher, 1);

	cipher->name = g_strdup(name);
	cipher->ops = ops;

	ciphers = g_list_append(ciphers, cipher);

	gaim_signal_emit(gaim_ciphers_get_handle(), "cipher-added", cipher);

	return cipher;
}

gboolean
gaim_ciphers_unregister_cipher(GaimCipher *cipher) {
	g_return_val_if_fail(cipher, FALSE);
	g_return_val_if_fail(cipher->ref == 0, FALSE);

	gaim_signal_emit(gaim_ciphers_get_handle(), "cipher-removed", cipher);

	ciphers = g_list_remove(ciphers, cipher);

	g_free(cipher->name);
	g_free(cipher);

	return TRUE;
}

GList *
gaim_ciphers_get_ciphers() {
	return ciphers;
}

/******************************************************************************
 * GaimCipher Subsystem API
 *****************************************************************************/
gpointer
gaim_ciphers_get_handle() {
	static gint handle;

	return &handle;
}

void
gaim_ciphers_init() {
	gpointer handle;

	handle = gaim_ciphers_get_handle();

	gaim_signal_register(handle, "cipher-added",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CIPHER));
	gaim_signal_register(handle, "cipher-removed",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CIPHER));

	gaim_ciphers_register_cipher("md5", &MD5Ops);
	gaim_ciphers_register_cipher("sha1", &SHA1Ops);
}

void
gaim_ciphers_uninit() {
	GaimCipher *cipher;
	GList *l, *ll;

	for(l = ciphers; l; l = ll) {
		ll = l->next;

		cipher = GAIM_CIPHER(l->data);
		gaim_ciphers_unregister_cipher(cipher);

		ciphers = g_list_remove(ciphers, cipher);
	}

	g_list_free(ciphers);

	gaim_signals_unregister_by_instance(gaim_ciphers_get_handle());
}
/******************************************************************************
 * GaimCipherContext API
 *****************************************************************************/
void
gaim_cipher_context_set_option(GaimCipherContext *context, const gchar *name,
							   gpointer value)
{
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);
	g_return_if_fail(name);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_option)
		cipher->ops->set_option(context, name, value);
	else
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"set_option operation\n", cipher->name);
}

gpointer
gaim_cipher_context_get_option(GaimCipherContext *context, const gchar *name) {
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, NULL);
	g_return_val_if_fail(name, NULL);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, NULL);

	if(cipher->ops && cipher->ops->get_option)
		return cipher->ops->get_option(context, name);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"get_option operation\n", cipher->name);

		return NULL;
	}
}

GaimCipherContext *
gaim_cipher_context_new(GaimCipher *cipher, void *extra) {
	GaimCipherContext *context = NULL;

	g_return_val_if_fail(cipher, NULL);

	cipher->ref++;

	context = g_new0(GaimCipherContext, 1);
	context->cipher = cipher;

	if(cipher->ops->init)
		cipher->ops->init(context, extra);

	return context;
}

GaimCipherContext *
gaim_cipher_context_new_by_name(const gchar *name, void *extra) {
	GaimCipher *cipher;

	g_return_val_if_fail(name, NULL);

	cipher = gaim_ciphers_find_cipher(name);

	g_return_val_if_fail(cipher, NULL);

	return gaim_cipher_context_new(cipher, extra);
}

void
gaim_cipher_context_reset(GaimCipherContext *context, void *extra) {
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->reset)
		context->cipher->ops->reset(context, extra);
}

void
gaim_cipher_context_destroy(GaimCipherContext *context) {
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	cipher->ref--;

	if(cipher->ops && cipher->ops->uninit)
		cipher->ops->uninit(context);

	memset(context, 0, sizeof(context));
	g_free(context);
	context = NULL;
}

void
gaim_cipher_context_set_iv(GaimCipherContext *context, guint8 *iv, size_t len)
{
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);
	g_return_if_fail(iv);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_iv)
		cipher->ops->set_iv(context, iv, len);
	else
		gaim_debug_info("cipher", "the %s cipher does not support the set"
						"initialization vector operation\n", cipher->name);
}

void
gaim_cipher_context_append(GaimCipherContext *context, const guint8 *data,
								size_t len)
{
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->append)
		cipher->ops->append(context, data, len);
	else
		gaim_debug_info("cipher", "the %s cipher does not support the append "
						"operation\n", cipher->name);
}

gboolean
gaim_cipher_context_digest(GaimCipherContext *context, size_t in_len,
						   guint8 digest[], size_t *out_len)
{
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, FALSE);

	cipher = context->cipher;
	g_return_val_if_fail(context, FALSE);

	if(cipher->ops && cipher->ops->digest)
		return cipher->ops->digest(context, in_len, digest, out_len);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the digest "
						"operation\n", cipher->name);
		return FALSE;
	}
}

gboolean
gaim_cipher_context_digest_to_str(GaimCipherContext *context, size_t in_len,
								   gchar digest_s[], size_t *out_len)
{
	/* 8k is a bit excessive, will tweak later. */
	guint8 digest[BUF_LEN * 4];
	gint n = 0;
	size_t dlen = 0;

	g_return_val_if_fail(context, FALSE);
	g_return_val_if_fail(digest_s, FALSE);

	if(!gaim_cipher_context_digest(context, sizeof(digest), digest, &dlen))
		return FALSE;

	if(in_len < dlen * 2)
		return FALSE;

	for(n = 0; n < dlen; n++)
		sprintf(digest_s + (n * 2), "%02x", digest[n]);

	digest_s[n * 2] = '\0';

	if(out_len)
		*out_len = dlen * 2;

	return TRUE;
}

gint
gaim_cipher_context_encrypt(GaimCipherContext *context, const guint8 data[],
							size_t len, guint8 output[], size_t *outlen)
{
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->encrypt)
		return cipher->ops->encrypt(context, data, len, output, outlen);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the encrypt"
						"operation\n", cipher->name);

		if(outlen)
			*outlen = -1;

		return -1;
	}
}

gint
gaim_cipher_context_decrypt(GaimCipherContext *context, const guint8 data[],
							size_t len, guint8 output[], size_t *outlen)
{
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->decrypt)
		return cipher->ops->decrypt(context, data, len, output, outlen);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the decrypt"
						"operation\n", cipher->name);

		if(outlen)
			*outlen = -1;

		return -1;
	}
}

void
gaim_cipher_context_set_salt(GaimCipherContext *context, guint8 *salt) {
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_salt)
		cipher->ops->set_salt(context, salt);
	else
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"set_salt operation\n", cipher->name);
}

size_t
gaim_cipher_context_get_salt_size(GaimCipherContext *context) {
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_salt_size)
		return cipher->ops->get_salt_size(context);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"get_salt_size operation\n", cipher->name);

		return -1;
	}
}

void
gaim_cipher_context_set_key(GaimCipherContext *context, guint8 *key) {
	GaimCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_key)
		cipher->ops->set_key(context, key);
	else
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"set_key operation\n", cipher->name);
}

size_t
gaim_cipher_context_get_key_size(GaimCipherContext *context) {
	GaimCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_key_size)
		return cipher->ops->get_key_size(context);
	else {
		gaim_debug_info("cipher", "the %s cipher does not support the "
						"get_key_size operation\n", cipher->name);

		return -1;
	}
}

void
gaim_cipher_context_set_data(GaimCipherContext *context, gpointer data) {
	g_return_if_fail(context);

	context->data = data;
}

gpointer
gaim_cipher_context_get_data(GaimCipherContext *context) {
	g_return_val_if_fail(context, NULL);

	return context->data;
}
