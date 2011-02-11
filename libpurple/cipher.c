/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Original md5
 * Copyright (C) 2001-2003  Christophe Devine <c.devine@cr0.net>
 *
 * Original md4 taken from linux kernel
 * MD4 Message Digest Algorithm (RFC1320).
 *
 * Implementation derived from Andrew Tridgell and Steve French's
 * CIFS MD4 implementation, and the cryptoapi implementation
 * originally based on the public domain implementation written
 * by Colin Plumb in 1993.
 *
 * Copyright (c) Andrew Tridgell 1997-1998.
 * Modified by Steve French (sfrench@us.ibm.com) 2002
 * Copyright (c) Cryptoapi developers.
 * Copyright (c) 2002 David S. Miller (davem@redhat.com)
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 *
 * Original des taken from gpg
 *
 * des.c - DES and Triple-DES encryption/decryption Algorithm
 *	Copyright (C) 1998 Free Software Foundation, Inc.
 *
 *	Please see below for more legal information!
 *
 *	 According to the definition of DES in FIPS PUB 46-2 from December 1993.
 *	 For a description of triple encryption, see:
 *	   Bruce Schneier: Applied Cryptography. Second Edition.
 *	   John Wiley & Sons, 1996. ISBN 0-471-12845-7. Pages 358 ff.
 *
 *	 This file is part of GnuPG.
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
#include "internal.h"
#include "cipher.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "signals.h"
#include "value.h"

#if GLIB_CHECK_VERSION(2,16,0)
static void
purple_g_checksum_init(PurpleCipherContext *context, GChecksumType type)
{
	GChecksum *checksum;

	checksum = g_checksum_new(type);
	purple_cipher_context_set_data(context, checksum);
}

static void
purple_g_checksum_reset(PurpleCipherContext *context, GChecksumType type)
{
	GChecksum *checksum;

	checksum = purple_cipher_context_get_data(context);
	g_return_if_fail(checksum != NULL);

#if GLIB_CHECK_VERSION(2,18,0)
	g_checksum_reset(checksum);
#else
	g_checksum_free(checksum);
	checksum = g_checksum_new(type);
	purple_cipher_context_set_data(context, checksum);
#endif
}

static void
purple_g_checksum_uninit(PurpleCipherContext *context)
{
	GChecksum *checksum;

	checksum = purple_cipher_context_get_data(context);
	g_return_if_fail(checksum != NULL);

	g_checksum_free(checksum);
}

static void
purple_g_checksum_append(PurpleCipherContext *context, const guchar *data,
                         gsize len)
{
	GChecksum *checksum;

	checksum = purple_cipher_context_get_data(context);
	g_return_if_fail(checksum != NULL);

	while (len >= G_MAXSSIZE) {
		g_checksum_update(checksum, data, G_MAXSSIZE);
		len -= G_MAXSSIZE;
		data += G_MAXSSIZE;
	}

	if (len)
		g_checksum_update(checksum, data, len);
}

static gboolean
purple_g_checksum_digest(PurpleCipherContext *context, GChecksumType type,
                         gsize len, guchar *digest, gsize *out_len)
{
	GChecksum *checksum;
	const gssize required_length = g_checksum_type_get_length(type);

	checksum = purple_cipher_context_get_data(context);

	g_return_val_if_fail(len >= required_length, FALSE);
	g_return_val_if_fail(checksum != NULL, FALSE);

	g_checksum_get_digest(checksum, digest, &len);

	purple_cipher_context_reset(context, NULL);

	if (out_len)
		*out_len = len;

	return TRUE;
}
#endif


/*******************************************************************************
 * MD5
 ******************************************************************************/
#define MD5_HMAC_BLOCK_SIZE	64

static size_t
md5_get_block_size(PurpleCipherContext *context)
{
	/* This does not change (in this case) */
	return MD5_HMAC_BLOCK_SIZE;
}

#if GLIB_CHECK_VERSION(2,16,0)

static void
md5_init(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_init(context, G_CHECKSUM_MD5);
}

static void
md5_reset(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_reset(context, G_CHECKSUM_MD5);
}

static gboolean
md5_digest(PurpleCipherContext *context, gsize in_len, guchar digest[16],
           size_t *out_len)
{
	return purple_g_checksum_digest(context, G_CHECKSUM_MD5, in_len,
	                                digest, out_len);
}

static PurpleCipherOps MD5Ops = {
	NULL,			/* Set Option		*/
	NULL,			/* Get Option		*/
	md5_init,		/* init				*/
	md5_reset,		/* reset			*/
	purple_g_checksum_uninit,	/* uninit			*/
	NULL,			/* set iv			*/
	purple_g_checksum_append,	/* append			*/
	md5_digest,		/* digest			*/
	NULL,			/* encrypt			*/
	NULL,			/* decrypt			*/
	NULL,			/* set salt			*/
	NULL,			/* get salt size	*/
	NULL,			/* set key			*/
	NULL,			/* get key size		*/
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	md5_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#else /* GLIB_CHECK_VERSION(2,16,0) */

struct MD5Context {
	guint32 total[2];
	guint32 state[4];
	guchar buffer[64];
};

#define MD5_GET_GUINT32(n,b,i) {			\
	(n) = ((guint32)(b) [(i)    ]      )	\
		| ((guint32)(b) [(i) + 1] <<  8)	\
		| ((guint32)(b) [(i) + 2] << 16)	\
		| ((guint32)(b) [(i) + 3] << 24);	\
}
#define MD5_PUT_GUINT32(n,b,i) {			\
	(b)[(i)    ] = (guchar)((n)      );		\
	(b)[(i) + 1] = (guchar)((n) >>  8);		\
	(b)[(i) + 2] = (guchar)((n) >> 16);		\
	(b)[(i) + 3] = (guchar)((n) >> 24);		\
}

static void
md5_init(PurpleCipherContext *context, gpointer extra) {
	struct MD5Context *md5_context;

	md5_context = g_new0(struct MD5Context, 1);

	purple_cipher_context_set_data(context, md5_context);

	purple_cipher_context_reset(context, extra);
}

static void
md5_reset(PurpleCipherContext *context, gpointer extra) {
	struct MD5Context *md5_context;

	md5_context = purple_cipher_context_get_data(context);

	md5_context->total[0] = 0;
	md5_context->total[1] = 0;

	md5_context->state[0] = 0x67452301;
	md5_context->state[1] = 0xEFCDAB89;
	md5_context->state[2] = 0x98BADCFE;
	md5_context->state[3] = 0x10325476;

	memset(md5_context->buffer, 0, sizeof(md5_context->buffer));
}

static void
md5_uninit(PurpleCipherContext *context) {
	struct MD5Context *md5_context;

	purple_cipher_context_reset(context, NULL);

	md5_context = purple_cipher_context_get_data(context);
	memset(md5_context, 0, sizeof(*md5_context));

	g_free(md5_context);
	md5_context = NULL;
}

static void
md5_process(struct MD5Context *md5_context, const guchar data[64]) {
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
md5_append(PurpleCipherContext *context, const guchar *data, size_t len) {
	struct MD5Context *md5_context = NULL;
	guint32 left = 0, fill = 0;

	g_return_if_fail(context != NULL);

	md5_context = purple_cipher_context_get_data(context);
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
md5_digest(PurpleCipherContext *context, size_t in_len, guchar digest[16],
		   size_t *out_len)
{
	struct MD5Context *md5_context = NULL;
	guint32 last, pad;
	guint32 high, low;
	guchar message[8];
	guchar padding[64] = {
		0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	g_return_val_if_fail(in_len >= 16, FALSE);

	md5_context = purple_cipher_context_get_data(context);

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

static PurpleCipherOps MD5Ops = {
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
	NULL,			/* get key size */
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	md5_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#endif /* GLIB_CHECK_VERSION(2,16,0) */

/*******************************************************************************
 * MD4
 ******************************************************************************/
#define MD4_DIGEST_SIZE		16
#define MD4_HMAC_BLOCK_SIZE	64
#define MD4_BLOCK_WORDS		16
#define MD4_HASH_WORDS		4



struct MD4_Context {
	guint32 hash[MD4_HASH_WORDS];
	guint32 block[MD4_BLOCK_WORDS];
	guint64 byte_count;
};

static inline guint32 lshift(guint32 x, unsigned int s)
{
	x &= 0xFFFFFFFF;
	return ((x << s) & 0xFFFFFFFF) | (x >> (32 - s));
}

static inline guint32 F(guint32 x, guint32 y, guint32 z)
{
	return (x & y) | ((~x) & z);
}

static inline guint32 G(guint32 x, guint32 y, guint32 z)
{
	return (x & y) | (x & z) | (y & z);
}

static inline guint32 H(guint32 x, guint32 y, guint32 z)
{
	return x ^ y ^ z;
}

#define ROUND1(a,b,c,d,k,s) (a = lshift(a + F(b,c,d) + k, s))
#define ROUND2(a,b,c,d,k,s) (a = lshift(a + G(b,c,d) + k + (guint32)0x5A827999,s))
#define ROUND3(a,b,c,d,k,s) (a = lshift(a + H(b,c,d) + k + (guint32)0x6ED9EBA1,s))

static inline void le32_to_cpu_array(guint32 *buf, unsigned int words)
{
	while (words--) {
		*buf=GUINT_FROM_LE(*buf);
		buf++;
	}
}

static inline void cpu_to_le32_array(guint32 *buf, unsigned int words)
{
	while (words--) {
		*buf=GUINT_TO_LE(*buf);
		buf++;
	}
}

static void md4_transform(guint32 *hash, guint32 const *in)
{
	guint32 a, b, c, d;

	a = hash[0];
	b = hash[1];
	c = hash[2];
	d = hash[3];

	ROUND1(a, b, c, d, in[0], 3);
	ROUND1(d, a, b, c, in[1], 7);
	ROUND1(c, d, a, b, in[2], 11);
	ROUND1(b, c, d, a, in[3], 19);
	ROUND1(a, b, c, d, in[4], 3);
	ROUND1(d, a, b, c, in[5], 7);
	ROUND1(c, d, a, b, in[6], 11);
	ROUND1(b, c, d, a, in[7], 19);
	ROUND1(a, b, c, d, in[8], 3);
	ROUND1(d, a, b, c, in[9], 7);
	ROUND1(c, d, a, b, in[10], 11);
	ROUND1(b, c, d, a, in[11], 19);
	ROUND1(a, b, c, d, in[12], 3);
	ROUND1(d, a, b, c, in[13], 7);
	ROUND1(c, d, a, b, in[14], 11);
	ROUND1(b, c, d, a, in[15], 19);

	ROUND2(a, b, c, d,in[ 0], 3);
	ROUND2(d, a, b, c, in[4], 5);
	ROUND2(c, d, a, b, in[8], 9);
	ROUND2(b, c, d, a, in[12], 13);
	ROUND2(a, b, c, d, in[1], 3);
	ROUND2(d, a, b, c, in[5], 5);
	ROUND2(c, d, a, b, in[9], 9);
	ROUND2(b, c, d, a, in[13], 13);
	ROUND2(a, b, c, d, in[2], 3);
	ROUND2(d, a, b, c, in[6], 5);
	ROUND2(c, d, a, b, in[10], 9);
	ROUND2(b, c, d, a, in[14], 13);
	ROUND2(a, b, c, d, in[3], 3);
	ROUND2(d, a, b, c, in[7], 5);
	ROUND2(c, d, a, b, in[11], 9);
	ROUND2(b, c, d, a, in[15], 13);

	ROUND3(a, b, c, d,in[ 0], 3);
	ROUND3(d, a, b, c, in[8], 9);
	ROUND3(c, d, a, b, in[4], 11);
	ROUND3(b, c, d, a, in[12], 15);
	ROUND3(a, b, c, d, in[2], 3);
	ROUND3(d, a, b, c, in[10], 9);
	ROUND3(c, d, a, b, in[6], 11);
	ROUND3(b, c, d, a, in[14], 15);
	ROUND3(a, b, c, d, in[1], 3);
	ROUND3(d, a, b, c, in[9], 9);
	ROUND3(c, d, a, b, in[5], 11);
	ROUND3(b, c, d, a, in[13], 15);
	ROUND3(a, b, c, d, in[3], 3);
	ROUND3(d, a, b, c, in[11], 9);
	ROUND3(c, d, a, b, in[7], 11);
	ROUND3(b, c, d, a, in[15], 15);

	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
}

static inline void md4_transform_helper(struct MD4_Context *ctx)
{
	le32_to_cpu_array(ctx->block, sizeof(ctx->block) / sizeof(guint32));
	md4_transform(ctx->hash, ctx->block);
}

static void
md4_init(PurpleCipherContext *context, gpointer extra) {
	struct MD4_Context *mctx;
	mctx = g_new0(struct MD4_Context, 1);
	purple_cipher_context_set_data(context, mctx);
	purple_cipher_context_reset(context, extra);

	mctx->hash[0] = 0x67452301;
	mctx->hash[1] = 0xefcdab89;
	mctx->hash[2] = 0x98badcfe;
	mctx->hash[3] = 0x10325476;
	mctx->byte_count = 0;
}

static void
md4_reset(PurpleCipherContext *context, gpointer extra) {
	struct MD4_Context *mctx;

	mctx = purple_cipher_context_get_data(context);

	mctx->hash[0] = 0x67452301;
	mctx->hash[1] = 0xefcdab89;
	mctx->hash[2] = 0x98badcfe;
	mctx->hash[3] = 0x10325476;
	mctx->byte_count = 0;
}

static void
md4_append(PurpleCipherContext *context, const guchar *data, size_t len)
{
	struct MD4_Context *mctx = purple_cipher_context_get_data(context);
	const guint32 avail = sizeof(mctx->block) - (mctx->byte_count & 0x3f);

	mctx->byte_count += len;

	if (avail > len) {
		memcpy((char *)mctx->block + (sizeof(mctx->block) - avail),
				data, len);
		return;
	}

	memcpy((char *)mctx->block + (sizeof(mctx->block) - avail),
			data, avail);

	md4_transform_helper(mctx);
	data += avail;
	len -= avail;

	while (len >= sizeof(mctx->block)) {
		memcpy(mctx->block, data, sizeof(mctx->block));
		md4_transform_helper(mctx);
		data += sizeof(mctx->block);
		len -= sizeof(mctx->block);
	}

	memcpy(mctx->block, data, len);
}

static gboolean
md4_digest(PurpleCipherContext *context, size_t in_len, guchar *out,
		                   size_t *out_len)
{
	struct MD4_Context *mctx = purple_cipher_context_get_data(context);
	const unsigned int offset = mctx->byte_count & 0x3f;
	char *p = (char *)mctx->block + offset;
	int padding = 56 - (offset + 1);


	if(in_len<16) return FALSE;
	if(out_len) *out_len = 16;
	*p++ = 0x80;
	if (padding < 0) {
		memset(p, 0x00, padding + sizeof (guint64));
		md4_transform_helper(mctx);
		p = (char *)mctx->block;
		padding = 56;
	}

	memset(p, 0, padding);
	mctx->block[14] = mctx->byte_count << 3;
	mctx->block[15] = mctx->byte_count >> 29;
	le32_to_cpu_array(mctx->block, (sizeof(mctx->block) -
				sizeof(guint64)) / sizeof(guint32));
	md4_transform(mctx->hash, mctx->block);
	cpu_to_le32_array(mctx->hash, sizeof(mctx->hash) / sizeof(guint32));
	memcpy(out, mctx->hash, sizeof(mctx->hash));
	memset(mctx, 0, sizeof(*mctx));
	return TRUE;
}

static void
md4_uninit(PurpleCipherContext *context) {
	struct MD4_Context *md4_context;

	purple_cipher_context_reset(context, NULL);

	md4_context = purple_cipher_context_get_data(context);
	memset(md4_context, 0, sizeof(*md4_context));

	g_free(md4_context);
	md4_context = NULL;
}

static size_t
md4_get_block_size(PurpleCipherContext *context)
{
	/* This does not change (in this case) */
	return MD4_HMAC_BLOCK_SIZE;
}

static PurpleCipherOps MD4Ops = {
	NULL,                   /* Set option */
	NULL,                   /* Get option */
	md4_init,               /* init */
	md4_reset,              /* reset */
	md4_uninit,             /* uninit */
	NULL,                   /* set iv */
	md4_append,             /* append */
	md4_digest,             /* digest */
	NULL,                   /* encrypt */
	NULL,                   /* decrypt */
	NULL,                   /* set salt */
	NULL,                   /* get salt size */
	NULL,                   /* set key */
	NULL,                   /* get key size */
	NULL,                   /* set batch mode */
	NULL,                   /* get batch mode */
	md4_get_block_size,     /* get block size */
	NULL                    /* set key with len */
};

/*******************************************************************************
 * HMAC
 ******************************************************************************/

struct HMAC_Context {
	PurpleCipherContext *hash;
	char *name;
	int blocksize;
	guchar *opad;
};

static void
hmac_init(PurpleCipherContext *context, gpointer extra)
{
	struct HMAC_Context *hctx;
	hctx = g_new0(struct HMAC_Context, 1);
	purple_cipher_context_set_data(context, hctx);
	purple_cipher_context_reset(context, extra);
}

static void
hmac_reset(PurpleCipherContext *context, gpointer extra)
{
	struct HMAC_Context *hctx;

	hctx = purple_cipher_context_get_data(context);

	g_free(hctx->name);
	hctx->name = NULL;
	if (hctx->hash)
		purple_cipher_context_destroy(hctx->hash);
	hctx->hash = NULL;
	hctx->blocksize = 0;
	g_free(hctx->opad);
	hctx->opad = NULL;
}

static void
hmac_set_opt(PurpleCipherContext *context, const gchar *name, void *value)
{
	struct HMAC_Context *hctx;

	hctx = purple_cipher_context_get_data(context);

	if (purple_strequal(name, "hash")) {
		g_free(hctx->name);
		if (hctx->hash)
			purple_cipher_context_destroy(hctx->hash);
		hctx->name = g_strdup((char*)value);
		hctx->hash = purple_cipher_context_new_by_name((char *)value, NULL);
		hctx->blocksize = purple_cipher_context_get_block_size(hctx->hash);
	}
}

static void *
hmac_get_opt(PurpleCipherContext *context, const gchar *name)
{
	struct HMAC_Context *hctx;

	hctx = purple_cipher_context_get_data(context);

	if (purple_strequal(name, "hash")) {
		return hctx->name;
	}

	return NULL;
}

static void
hmac_append(PurpleCipherContext *context, const guchar *data, size_t len)
{
	struct HMAC_Context *hctx = purple_cipher_context_get_data(context);

	g_return_if_fail(hctx->hash != NULL);

	purple_cipher_context_append(hctx->hash, data, len);
}

static gboolean
hmac_digest(PurpleCipherContext *context, size_t in_len, guchar *out, size_t *out_len)
{
	struct HMAC_Context *hctx = purple_cipher_context_get_data(context);
	PurpleCipherContext *hash = hctx->hash;
	guchar *inner_hash;
	size_t hash_len;
	gboolean result;

	g_return_val_if_fail(hash != NULL, FALSE);

	inner_hash = g_malloc(100); /* TODO: Should be enough for now... */
	result = purple_cipher_context_digest(hash, 100, inner_hash, &hash_len);

	purple_cipher_context_reset(hash, NULL);

	purple_cipher_context_append(hash, hctx->opad, hctx->blocksize);
	purple_cipher_context_append(hash, inner_hash, hash_len);

	g_free(inner_hash);

	result = result && purple_cipher_context_digest(hash, in_len, out, out_len);

	return result;
}

static void
hmac_uninit(PurpleCipherContext *context)
{
	struct HMAC_Context *hctx;

	purple_cipher_context_reset(context, NULL);

	hctx = purple_cipher_context_get_data(context);

	g_free(hctx);
}

static void
hmac_set_key_with_len(PurpleCipherContext *context, const guchar * key, size_t key_len)
{
	struct HMAC_Context *hctx = purple_cipher_context_get_data(context);
	int blocksize, i;
	guchar *ipad;
	guchar *full_key;

	g_return_if_fail(hctx->hash != NULL);

	g_free(hctx->opad);

	blocksize = hctx->blocksize;
	ipad = g_malloc(blocksize);
	hctx->opad = g_malloc(blocksize);

	if (key_len > blocksize) {
		purple_cipher_context_reset(hctx->hash, NULL);
		purple_cipher_context_append(hctx->hash, key, key_len);
		full_key = g_malloc(100); /* TODO: Should be enough for now... */
		purple_cipher_context_digest(hctx->hash, 100, full_key, &key_len);
	} else
		full_key = g_memdup(key, key_len);

    if (key_len < blocksize) {
    	full_key = g_realloc(full_key, blocksize);
    	memset(full_key + key_len, 0, blocksize - key_len);
    }

	for(i = 0; i < blocksize; i++) {
		ipad[i] = 0x36 ^ full_key[i];
		hctx->opad[i] = 0x5c ^ full_key[i];
	}

	g_free(full_key);

	purple_cipher_context_reset(hctx->hash, NULL);
	purple_cipher_context_append(hctx->hash, ipad, blocksize);
	g_free(ipad);
}

static void
hmac_set_key(PurpleCipherContext *context, const guchar * key)
{
	hmac_set_key_with_len(context, key, strlen((char *)key));
}

static size_t
hmac_get_block_size(PurpleCipherContext *context)
{
	struct HMAC_Context *hctx = purple_cipher_context_get_data(context);

	return hctx->blocksize;
}

static PurpleCipherOps HMACOps = {
	hmac_set_opt,           /* Set option */
	hmac_get_opt,           /* Get option */
	hmac_init,               /* init */
	hmac_reset,              /* reset */
	hmac_uninit,             /* uninit */
	NULL,                   /* set iv */
	hmac_append,             /* append */
	hmac_digest,             /* digest */
	NULL,                   /* encrypt */
	NULL,                   /* decrypt */
	NULL,                   /* set salt */
	NULL,                   /* get salt size */
	hmac_set_key,           /* set key */
	NULL,                   /* get key size */
	NULL,                   /* set batch mode */
	NULL,                   /* get batch mode */
	hmac_get_block_size,    /* get block size */
	hmac_set_key_with_len   /* set key with len */
};

/******************************************************************************
 * DES
 *****************************************************************************/

typedef struct _des_ctx
{
	guint32 encrypt_subkeys[32];
	guint32 decrypt_subkeys[32];
} des_ctx[1];

/*
 *  The s-box values are permuted according to the 'primitive function P'
 */
static const guint32 sbox1[64] =
{
	0x00808200, 0x00000000, 0x00008000, 0x00808202, 0x00808002, 0x00008202, 0x00000002, 0x00008000,
	0x00000200, 0x00808200, 0x00808202, 0x00000200, 0x00800202, 0x00808002, 0x00800000, 0x00000002,
	0x00000202, 0x00800200, 0x00800200, 0x00008200, 0x00008200, 0x00808000, 0x00808000, 0x00800202,
	0x00008002, 0x00800002, 0x00800002, 0x00008002, 0x00000000, 0x00000202, 0x00008202, 0x00800000,
	0x00008000, 0x00808202, 0x00000002, 0x00808000, 0x00808200, 0x00800000, 0x00800000, 0x00000200,
	0x00808002, 0x00008000, 0x00008200, 0x00800002, 0x00000200, 0x00000002, 0x00800202, 0x00008202,
	0x00808202, 0x00008002, 0x00808000, 0x00800202, 0x00800002, 0x00000202, 0x00008202, 0x00808200,
	0x00000202, 0x00800200, 0x00800200, 0x00000000, 0x00008002, 0x00008200, 0x00000000, 0x00808002
};

static const guint32 sbox2[64] =
{
	0x40084010, 0x40004000, 0x00004000, 0x00084010, 0x00080000, 0x00000010, 0x40080010, 0x40004010,
	0x40000010, 0x40084010, 0x40084000, 0x40000000, 0x40004000, 0x00080000, 0x00000010, 0x40080010,
	0x00084000, 0x00080010, 0x40004010, 0x00000000, 0x40000000, 0x00004000, 0x00084010, 0x40080000,
	0x00080010, 0x40000010, 0x00000000, 0x00084000, 0x00004010, 0x40084000, 0x40080000, 0x00004010,
	0x00000000, 0x00084010, 0x40080010, 0x00080000, 0x40004010, 0x40080000, 0x40084000, 0x00004000,
	0x40080000, 0x40004000, 0x00000010, 0x40084010, 0x00084010, 0x00000010, 0x00004000, 0x40000000,
	0x00004010, 0x40084000, 0x00080000, 0x40000010, 0x00080010, 0x40004010, 0x40000010, 0x00080010,
	0x00084000, 0x00000000, 0x40004000, 0x00004010, 0x40000000, 0x40080010, 0x40084010, 0x00084000
};

static const guint32 sbox3[64] =
{
	0x00000104, 0x04010100, 0x00000000, 0x04010004, 0x04000100, 0x00000000, 0x00010104, 0x04000100,
	0x00010004, 0x04000004, 0x04000004, 0x00010000, 0x04010104, 0x00010004, 0x04010000, 0x00000104,
	0x04000000, 0x00000004, 0x04010100, 0x00000100, 0x00010100, 0x04010000, 0x04010004, 0x00010104,
	0x04000104, 0x00010100, 0x00010000, 0x04000104, 0x00000004, 0x04010104, 0x00000100, 0x04000000,
	0x04010100, 0x04000000, 0x00010004, 0x00000104, 0x00010000, 0x04010100, 0x04000100, 0x00000000,
	0x00000100, 0x00010004, 0x04010104, 0x04000100, 0x04000004, 0x00000100, 0x00000000, 0x04010004,
	0x04000104, 0x00010000, 0x04000000, 0x04010104, 0x00000004, 0x00010104, 0x00010100, 0x04000004,
	0x04010000, 0x04000104, 0x00000104, 0x04010000, 0x00010104, 0x00000004, 0x04010004, 0x00010100
};

static const guint32 sbox4[64] =
{
	0x80401000, 0x80001040, 0x80001040, 0x00000040, 0x00401040, 0x80400040, 0x80400000, 0x80001000,
	0x00000000, 0x00401000, 0x00401000, 0x80401040, 0x80000040, 0x00000000, 0x00400040, 0x80400000,
	0x80000000, 0x00001000, 0x00400000, 0x80401000, 0x00000040, 0x00400000, 0x80001000, 0x00001040,
	0x80400040, 0x80000000, 0x00001040, 0x00400040, 0x00001000, 0x00401040, 0x80401040, 0x80000040,
	0x00400040, 0x80400000, 0x00401000, 0x80401040, 0x80000040, 0x00000000, 0x00000000, 0x00401000,
	0x00001040, 0x00400040, 0x80400040, 0x80000000, 0x80401000, 0x80001040, 0x80001040, 0x00000040,
	0x80401040, 0x80000040, 0x80000000, 0x00001000, 0x80400000, 0x80001000, 0x00401040, 0x80400040,
	0x80001000, 0x00001040, 0x00400000, 0x80401000, 0x00000040, 0x00400000, 0x00001000, 0x00401040
};

static const guint32 sbox5[64] =
{
	0x00000080, 0x01040080, 0x01040000, 0x21000080, 0x00040000, 0x00000080, 0x20000000, 0x01040000,
	0x20040080, 0x00040000, 0x01000080, 0x20040080, 0x21000080, 0x21040000, 0x00040080, 0x20000000,
	0x01000000, 0x20040000, 0x20040000, 0x00000000, 0x20000080, 0x21040080, 0x21040080, 0x01000080,
	0x21040000, 0x20000080, 0x00000000, 0x21000000, 0x01040080, 0x01000000, 0x21000000, 0x00040080,
	0x00040000, 0x21000080, 0x00000080, 0x01000000, 0x20000000, 0x01040000, 0x21000080, 0x20040080,
	0x01000080, 0x20000000, 0x21040000, 0x01040080, 0x20040080, 0x00000080, 0x01000000, 0x21040000,
	0x21040080, 0x00040080, 0x21000000, 0x21040080, 0x01040000, 0x00000000, 0x20040000, 0x21000000,
	0x00040080, 0x01000080, 0x20000080, 0x00040000, 0x00000000, 0x20040000, 0x01040080, 0x20000080
};

static const guint32 sbox6[64] =
{
	0x10000008, 0x10200000, 0x00002000, 0x10202008, 0x10200000, 0x00000008, 0x10202008, 0x00200000,
	0x10002000, 0x00202008, 0x00200000, 0x10000008, 0x00200008, 0x10002000, 0x10000000, 0x00002008,
	0x00000000, 0x00200008, 0x10002008, 0x00002000, 0x00202000, 0x10002008, 0x00000008, 0x10200008,
	0x10200008, 0x00000000, 0x00202008, 0x10202000, 0x00002008, 0x00202000, 0x10202000, 0x10000000,
	0x10002000, 0x00000008, 0x10200008, 0x00202000, 0x10202008, 0x00200000, 0x00002008, 0x10000008,
	0x00200000, 0x10002000, 0x10000000, 0x00002008, 0x10000008, 0x10202008, 0x00202000, 0x10200000,
	0x00202008, 0x10202000, 0x00000000, 0x10200008, 0x00000008, 0x00002000, 0x10200000, 0x00202008,
	0x00002000, 0x00200008, 0x10002008, 0x00000000, 0x10202000, 0x10000000, 0x00200008, 0x10002008
};

static const guint32 sbox7[64] =
{
	0x00100000, 0x02100001, 0x02000401, 0x00000000, 0x00000400, 0x02000401, 0x00100401, 0x02100400,
	0x02100401, 0x00100000, 0x00000000, 0x02000001, 0x00000001, 0x02000000, 0x02100001, 0x00000401,
	0x02000400, 0x00100401, 0x00100001, 0x02000400, 0x02000001, 0x02100000, 0x02100400, 0x00100001,
	0x02100000, 0x00000400, 0x00000401, 0x02100401, 0x00100400, 0x00000001, 0x02000000, 0x00100400,
	0x02000000, 0x00100400, 0x00100000, 0x02000401, 0x02000401, 0x02100001, 0x02100001, 0x00000001,
	0x00100001, 0x02000000, 0x02000400, 0x00100000, 0x02100400, 0x00000401, 0x00100401, 0x02100400,
	0x00000401, 0x02000001, 0x02100401, 0x02100000, 0x00100400, 0x00000000, 0x00000001, 0x02100401,
	0x00000000, 0x00100401, 0x02100000, 0x00000400, 0x02000001, 0x02000400, 0x00000400, 0x00100001
};

static const guint32 sbox8[64] =
{
	0x08000820, 0x00000800, 0x00020000, 0x08020820, 0x08000000, 0x08000820, 0x00000020, 0x08000000,
	0x00020020, 0x08020000, 0x08020820, 0x00020800, 0x08020800, 0x00020820, 0x00000800, 0x00000020,
	0x08020000, 0x08000020, 0x08000800, 0x00000820, 0x00020800, 0x00020020, 0x08020020, 0x08020800,
	0x00000820, 0x00000000, 0x00000000, 0x08020020, 0x08000020, 0x08000800, 0x00020820, 0x00020000,
	0x00020820, 0x00020000, 0x08020800, 0x00000800, 0x00000020, 0x08020020, 0x00000800, 0x00020820,
	0x08000800, 0x00000020, 0x08000020, 0x08020000, 0x08020020, 0x08000000, 0x00020000, 0x08000820,
	0x00000000, 0x08020820, 0x00020020, 0x08000020, 0x08020000, 0x08000800, 0x08000820, 0x00000000,
	0x08020820, 0x00020800, 0x00020800, 0x00000820, 0x00000820, 0x00020020, 0x08000000, 0x08020800
};



/*
 *  * These two tables are part of the 'permuted choice 1' function.
 *   * In this implementation several speed improvements are done.
 *    */
static const guint32 leftkey_swap[16] =
{
	0x00000000, 0x00000001, 0x00000100, 0x00000101,
	0x00010000, 0x00010001, 0x00010100, 0x00010101,
	0x01000000, 0x01000001, 0x01000100, 0x01000101,
	0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static const guint32 rightkey_swap[16] =
{
	0x00000000, 0x01000000, 0x00010000, 0x01010000,
	0x00000100, 0x01000100, 0x00010100, 0x01010100,
	0x00000001, 0x01000001, 0x00010001, 0x01010001,
	0x00000101, 0x01000101, 0x00010101, 0x01010101,
};



/*
 *  Numbers of left shifts per round for encryption subkey schedule
 *  To calculate the decryption key scheduling we just reverse the
 *  ordering of the subkeys so we can omit the table for decryption
 *  subkey schedule.
 */
static const guint8 encrypt_rotate_tab[16] =
{
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

/*
 *  Macro to swap bits across two words
 **/
#define DO_PERMUTATION(a, temp, b, offset, mask)	\
	temp = ((a>>offset) ^ b) & mask;			\
b ^= temp;						\
a ^= temp<<offset;


/*
 *  This performs the 'initial permutation' for the data to be encrypted or decrypted
 **/
#define INITIAL_PERMUTATION(left, temp, right)		\
	DO_PERMUTATION(left, temp, right, 4, 0x0f0f0f0f)	\
DO_PERMUTATION(left, temp, right, 16, 0x0000ffff)	\
DO_PERMUTATION(right, temp, left, 2, 0x33333333)	\
DO_PERMUTATION(right, temp, left, 8, 0x00ff00ff)	\
DO_PERMUTATION(left, temp, right, 1, 0x55555555)


/*
 * The 'inverse initial permutation'
 **/
#define FINAL_PERMUTATION(left, temp, right)		\
	DO_PERMUTATION(left, temp, right, 1, 0x55555555)	\
DO_PERMUTATION(right, temp, left, 8, 0x00ff00ff)	\
DO_PERMUTATION(right, temp, left, 2, 0x33333333)	\
DO_PERMUTATION(left, temp, right, 16, 0x0000ffff)	\
DO_PERMUTATION(left, temp, right, 4, 0x0f0f0f0f)


/*
 * A full DES round including 'expansion function', 'sbox substitution'
 * and 'primitive function P' but without swapping the left and right word.
 **/
#define DES_ROUND(from, to, work, subkey)		\
	work = ((from<<1) | (from>>31)) ^ *subkey++;	\
to ^= sbox8[  work	    & 0x3f ];			\
to ^= sbox6[ (work>>8)  & 0x3f ];			\
to ^= sbox4[ (work>>16) & 0x3f ];			\
to ^= sbox2[ (work>>24) & 0x3f ];			\
work = ((from>>3) | (from<<29)) ^ *subkey++;	\
to ^= sbox7[  work	    & 0x3f ];			\
to ^= sbox5[ (work>>8)  & 0x3f ];			\
to ^= sbox3[ (work>>16) & 0x3f ];			\
to ^= sbox1[ (work>>24) & 0x3f ];


/*
 * Macros to convert 8 bytes from/to 32bit words
 **/
#define READ_64BIT_DATA(data, left, right)					\
	left  = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];	\
right = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];

#define WRITE_64BIT_DATA(data, left, right)					\
	data[0] = (left >> 24) &0xff; data[1] = (left >> 16) &0xff; 		\
data[2] = (left >> 8) &0xff; data[3] = left &0xff;				\
data[4] = (right >> 24) &0xff; data[5] = (right >> 16) &0xff;		\
data[6] = (right >> 8) &0xff; data[7] = right &0xff;






/*
 * des_key_schedule():	  Calculate 16 subkeys pairs (even/odd) for
 *			  16 encryption rounds.
 *			  To calculate subkeys for decryption the caller
 *    			  have to reorder the generated subkeys.
 *
 *        rawkey:	    8 Bytes of key data
 *        subkey:	    Array of at least 32 guint32s. Will be filled
 *    		    with calculated subkeys.
 *
 **/
static void
des_key_schedule (const guint8 * rawkey, guint32 * subkey)
{
	guint32 left, right, work;
	int round;

	READ_64BIT_DATA (rawkey, left, right)

		DO_PERMUTATION (right, work, left, 4, 0x0f0f0f0f)
		DO_PERMUTATION (right, work, left, 0, 0x10101010)

		left = (leftkey_swap[(left >> 0) & 0xf] << 3) | (leftkey_swap[(left >> 8) & 0xf] << 2)
		| (leftkey_swap[(left >> 16) & 0xf] << 1) | (leftkey_swap[(left >> 24) & 0xf])
		| (leftkey_swap[(left >> 5) & 0xf] << 7) | (leftkey_swap[(left >> 13) & 0xf] << 6)
		| (leftkey_swap[(left >> 21) & 0xf] << 5) | (leftkey_swap[(left >> 29) & 0xf] << 4);

	left &= 0x0fffffff;

	right = (rightkey_swap[(right >> 1) & 0xf] << 3) | (rightkey_swap[(right >> 9) & 0xf] << 2)
		| (rightkey_swap[(right >> 17) & 0xf] << 1) | (rightkey_swap[(right >> 25) & 0xf])
		| (rightkey_swap[(right >> 4) & 0xf] << 7) | (rightkey_swap[(right >> 12) & 0xf] << 6)
		| (rightkey_swap[(right >> 20) & 0xf] << 5) | (rightkey_swap[(right >> 28) & 0xf] << 4);

	right &= 0x0fffffff;

	for (round = 0; round < 16; ++round)
	{
		left = ((left << encrypt_rotate_tab[round]) | (left >> (28 - encrypt_rotate_tab[round]))) & 0x0fffffff;
		right = ((right << encrypt_rotate_tab[round]) | (right >> (28 - encrypt_rotate_tab[round]))) & 0x0fffffff;

		*subkey++ = ((left << 4) & 0x24000000)
			| ((left << 28) & 0x10000000)
			| ((left << 14) & 0x08000000)
			| ((left << 18) & 0x02080000)
			| ((left << 6) & 0x01000000)
			| ((left << 9) & 0x00200000)
			| ((left >> 1) & 0x00100000)
			| ((left << 10) & 0x00040000)
			| ((left << 2) & 0x00020000)
			| ((left >> 10) & 0x00010000)
			| ((right >> 13) & 0x00002000)
			| ((right >> 4) & 0x00001000)
			| ((right << 6) & 0x00000800)
			| ((right >> 1) & 0x00000400)
			| ((right >> 14) & 0x00000200)
			| (right & 0x00000100)
			| ((right >> 5) & 0x00000020)
			| ((right >> 10) & 0x00000010)
			| ((right >> 3) & 0x00000008)
			| ((right >> 18) & 0x00000004)
			| ((right >> 26) & 0x00000002)
			| ((right >> 24) & 0x00000001);

		*subkey++ = ((left << 15) & 0x20000000)
			| ((left << 17) & 0x10000000)
			| ((left << 10) & 0x08000000)
			| ((left << 22) & 0x04000000)
			| ((left >> 2) & 0x02000000)
			| ((left << 1) & 0x01000000)
			| ((left << 16) & 0x00200000)
			| ((left << 11) & 0x00100000)
			| ((left << 3) & 0x00080000)
			| ((left >> 6) & 0x00040000)
			| ((left << 15) & 0x00020000)
			| ((left >> 4) & 0x00010000)
			| ((right >> 2) & 0x00002000)
			| ((right << 8) & 0x00001000)
			| ((right >> 14) & 0x00000808)
			| ((right >> 9) & 0x00000400)
			| ((right) & 0x00000200)
			| ((right << 7) & 0x00000100)
			| ((right >> 7) & 0x00000020)
			| ((right >> 3) & 0x00000011)
			| ((right << 2) & 0x00000004)
			| ((right >> 21) & 0x00000002);
	}
}



/*
 *  Fill a DES context with subkeys calculated from a 64bit key.
 *  Does not check parity bits, but simply ignore them.
 *  Does not check for weak keys.
 **/
static void
des_set_key (PurpleCipherContext *context, const guchar * key)
{
	struct _des_ctx *ctx = purple_cipher_context_get_data(context);
	int i;

	des_key_schedule (key, ctx->encrypt_subkeys);

	for(i=0; i<32; i+=2)
	{
		ctx->decrypt_subkeys[i]	= ctx->encrypt_subkeys[30-i];
		ctx->decrypt_subkeys[i+1] = ctx->encrypt_subkeys[31-i];
	}
}



/*
 *  Electronic Codebook Mode DES encryption/decryption of data according
 *  to 'mode'.
 **/
static int
des_ecb_crypt (struct _des_ctx *ctx, const guint8 * from, guint8 * to, int mode)
{
	guint32 left, right, work;
	guint32 *keys;

	keys = mode ? ctx->decrypt_subkeys : ctx->encrypt_subkeys;

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

static gint
des_encrypt(PurpleCipherContext *context, const guchar data[],
	    size_t len, guchar output[], size_t *outlen) {
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	while(offset+8<=len) {
		des_ecb_crypt(purple_cipher_context_get_data(context),
				data+offset,
				output+offset,
				0);
		offset+=8;
	}
	*outlen = len;
	if(offset<len) {
		*outlen += len - offset;
		tmp = offset;
		while(tmp<len) {
			buf[i++] = data[tmp];
			tmp++;
		}
		des_ecb_crypt(purple_cipher_context_get_data(context),
				buf,
				output+offset,
				0);
	}
	return 0;
}

static gint
des_decrypt(PurpleCipherContext *context, const guchar data[],
	    size_t len, guchar output[], size_t *outlen) {
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	while(offset+8<=len) {
		des_ecb_crypt(purple_cipher_context_get_data(context),
				data+offset,
				output+offset,
				1);
		offset+=8;
	}
	*outlen = len;
	if(offset<len) {
		*outlen += len - offset;
		tmp = offset;
		while(tmp<len) {
			buf[i++] = data[tmp];
			tmp++;
		}
		des_ecb_crypt(purple_cipher_context_get_data(context),
				buf,
				output+offset,
				1);
	}
	return 0;
}

static void
des_init(PurpleCipherContext *context, gpointer extra) {
	struct _des_ctx *mctx;
	mctx = g_new0(struct _des_ctx, 1);
	purple_cipher_context_set_data(context, mctx);
}

static void
des_uninit(PurpleCipherContext *context) {
	struct _des_ctx *des_context;

	des_context = purple_cipher_context_get_data(context);
	memset(des_context, 0, sizeof(*des_context));

	g_free(des_context);
	des_context = NULL;
}

static PurpleCipherOps DESOps = {
	NULL,              /* Set option */
	NULL,              /* Get option */
	des_init,          /* init */
 	NULL,              /* reset */
	des_uninit,        /* uninit */
	NULL,              /* set iv */
	NULL,              /* append */
	NULL,              /* digest */
	des_encrypt,       /* encrypt */
	des_decrypt,       /* decrypt */
	NULL,              /* set salt */
	NULL,              /* get salt size */
	des_set_key,       /* set key */
	NULL,              /* get key size */
	NULL,              /* set batch mode */
	NULL,              /* get batch mode */
	NULL,              /* get block size */
	NULL               /* set key with len */
};

/******************************************************************************
 * Triple-DES
 *****************************************************************************/

typedef struct _des3_ctx
{
	PurpleCipherBatchMode mode;
	guchar iv[8];
	/* First key for encryption */
	struct _des_ctx key1;
	/* Second key for decryption */
	struct _des_ctx key2;
	/* Third key for encryption */
	struct _des_ctx key3;
} des3_ctx[1];

/*
 *  Fill a DES3 context with subkeys calculated from 3 64bit key.
 *  Does not check parity bits, but simply ignore them.
 *  Does not check for weak keys.
 **/
static void
des3_set_key(PurpleCipherContext *context, const guchar * key)
{
	struct _des3_ctx *ctx = purple_cipher_context_get_data(context);
	int i;

	des_key_schedule (key +  0, ctx->key1.encrypt_subkeys);
	des_key_schedule (key +  8, ctx->key2.encrypt_subkeys);
	des_key_schedule (key + 16, ctx->key3.encrypt_subkeys);

	for (i = 0; i < 32; i += 2)
	{
		ctx->key1.decrypt_subkeys[i]	= ctx->key1.encrypt_subkeys[30-i];
		ctx->key1.decrypt_subkeys[i+1]	= ctx->key1.encrypt_subkeys[31-i];
		ctx->key2.decrypt_subkeys[i]	= ctx->key2.encrypt_subkeys[30-i];
		ctx->key2.decrypt_subkeys[i+1]	= ctx->key2.encrypt_subkeys[31-i];
		ctx->key3.decrypt_subkeys[i]	= ctx->key3.encrypt_subkeys[30-i];
		ctx->key3.decrypt_subkeys[i+1]	= ctx->key3.encrypt_subkeys[31-i];
	}
}

static gint
des3_ecb_encrypt(struct _des3_ctx *ctx, const guchar data[],
                 size_t len, guchar output[], size_t *outlen)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	while (offset + 8 <= len) {
		des_ecb_crypt(&ctx->key1,
		              data+offset,
		              output+offset,
		              0);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              1);
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              0);
		offset += 8;
	}
	*outlen = len;
	if (offset < len) {
		*outlen += len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		while (tmp < len) {
			buf[i++] = data[tmp];
			tmp++;
		}
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              0);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              1);
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              0);
	}
	return 0;
}

static gint
des3_cbc_encrypt(struct _des3_ctx *ctx, const guchar data[],
                 size_t len, guchar output[], size_t *outlen)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8];
	memcpy(buf, ctx->iv, 8);
	while (offset + 8 <= len) {
		for (i = 0; i < 8; i++)
			buf[i] ^= data[offset + i];
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              0);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              1);
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              0);
		memcpy(buf, output+offset, 8);
		offset += 8;
	}
	*outlen = len;
	if (offset < len) {
		*outlen += len - offset;
		tmp = offset;
		i = 0;
		while (tmp < len) {
			buf[i++] ^= data[tmp];
			tmp++;
		}
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              0);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              1);
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              0);
	}
	return 0;
}

static gint
des3_encrypt(PurpleCipherContext *context, const guchar data[],
             size_t len, guchar output[], size_t *outlen)
{
	struct _des3_ctx *ctx = purple_cipher_context_get_data(context);

	if (ctx->mode == PURPLE_CIPHER_BATCH_MODE_ECB) {
		return des3_ecb_encrypt(ctx, data, len, output, outlen);
	} else if (ctx->mode == PURPLE_CIPHER_BATCH_MODE_CBC) {
		return des3_cbc_encrypt(ctx, data, len, output, outlen);
	} else {
		g_return_val_if_reached(0);
	}

	return 0;
}

static gint
des3_ecb_decrypt(struct _des3_ctx *ctx, const guchar data[],
                 size_t len, guchar output[], size_t *outlen)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	while (offset + 8 <= len) {
		/* NOTE: Apply key in reverse */
		des_ecb_crypt(&ctx->key3,
		              data+offset,
		              output+offset,
		              1);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              0);
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              1);
		offset+=8;
	}
	*outlen = len;
	if (offset < len) {
		*outlen += len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		while (tmp < len) {
			buf[i++] = data[tmp];
			tmp++;
		}
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              1);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              0);
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              1);
	}
	return 0;
}

static gint
des3_cbc_decrypt(struct _des3_ctx *ctx, const guchar data[],
                 size_t len, guchar output[], size_t *outlen)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	guint8 link[8];
	memcpy(link, ctx->iv, 8);
	while (offset + 8 <= len) {
		des_ecb_crypt(&ctx->key3,
		              data+offset,
		              output+offset,
		              1);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              0);
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              1);
		for (i = 0; i < 8; i++)
			output[offset + i] ^= link[i];
		memcpy(link, data + offset, 8);
		offset+=8;
	}
	*outlen = len;
	if(offset<len) {
		*outlen += len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		i = 0;
		while(tmp<len) {
			buf[i++] = data[tmp];
			tmp++;
		}
		des_ecb_crypt(&ctx->key3,
		              buf,
		              output+offset,
		              1);
		des_ecb_crypt(&ctx->key2,
		              output+offset,
		              buf,
		              0);
		des_ecb_crypt(&ctx->key1,
		              buf,
		              output+offset,
		              1);
		for (i = 0; i < 8; i++)
			output[offset + i] ^= link[i];
	}
	return 0;
}

static gint
des3_decrypt(PurpleCipherContext *context, const guchar data[],
             size_t len, guchar output[], size_t *outlen)
{
	struct _des3_ctx *ctx = purple_cipher_context_get_data(context);

	if (ctx->mode == PURPLE_CIPHER_BATCH_MODE_ECB) {
		return des3_ecb_decrypt(ctx, data, len, output, outlen);
	} else if (ctx->mode == PURPLE_CIPHER_BATCH_MODE_CBC) {
		return des3_cbc_decrypt(ctx, data, len, output, outlen);
	} else {
		g_return_val_if_reached(0);
	}

	return 0;
}

static void
des3_set_batch(PurpleCipherContext *context, PurpleCipherBatchMode mode)
{
	struct _des3_ctx *ctx = purple_cipher_context_get_data(context);

	ctx->mode = mode;
}

static PurpleCipherBatchMode
des3_get_batch(PurpleCipherContext *context)
{
	struct _des3_ctx *ctx = purple_cipher_context_get_data(context);

	return ctx->mode;
}

static void
des3_set_iv(PurpleCipherContext *context, guchar *iv, size_t len)
{
	struct _des3_ctx *ctx;

	g_return_if_fail(len == 8);

	ctx = purple_cipher_context_get_data(context);

	memcpy(ctx->iv, iv, len);
}

static void
des3_init(PurpleCipherContext *context, gpointer extra)
{
	struct _des3_ctx *mctx;
	mctx = g_new0(struct _des3_ctx, 1);
	purple_cipher_context_set_data(context, mctx);
}

static void
des3_uninit(PurpleCipherContext *context)
{
	struct _des3_ctx *des3_context;

	des3_context = purple_cipher_context_get_data(context);
	memset(des3_context, 0, sizeof(*des3_context));

	g_free(des3_context);
	des3_context = NULL;
}

static PurpleCipherOps DES3Ops = {
	NULL,              /* Set option */
	NULL,              /* Get option */
	des3_init,         /* init */
	NULL,              /* reset */
	des3_uninit,       /* uninit */
	des3_set_iv,       /* set iv */
	NULL,              /* append */
	NULL,              /* digest */
	des3_encrypt,      /* encrypt */
	des3_decrypt,      /* decrypt */
	NULL,              /* set salt */
	NULL,              /* get salt size */
	des3_set_key,      /* set key */
	NULL,              /* get key size */
	des3_set_batch,    /* set batch mode */
	des3_get_batch,    /* get batch mode */
	NULL,              /* get block size */
	NULL               /* set key with len */
};

/*******************************************************************************
 * SHA-1
 ******************************************************************************/
#define SHA1_HMAC_BLOCK_SIZE	64

static size_t
sha1_get_block_size(PurpleCipherContext *context)
{
	/* This does not change (in this case) */
	return SHA1_HMAC_BLOCK_SIZE;
}


#if GLIB_CHECK_VERSION(2,16,0)

static void
sha1_init(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_init(context, G_CHECKSUM_SHA1);
}

static void
sha1_reset(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_reset(context, G_CHECKSUM_SHA1);
}

static gboolean
sha1_digest(PurpleCipherContext *context, gsize in_len, guchar digest[20],
            gsize *out_len)
{
	return purple_g_checksum_digest(context, G_CHECKSUM_SHA1, in_len,
	                                digest, out_len);
}

static PurpleCipherOps SHA1Ops = {
	NULL,			/* Set Option		*/
	NULL,			/* Get Option		*/
	sha1_init,		/* init				*/
	sha1_reset,		/* reset			*/
	purple_g_checksum_uninit,	/* uninit			*/
	NULL,			/* set iv			*/
	purple_g_checksum_append,	/* append			*/
	sha1_digest,	/* digest			*/
	NULL,			/* encrypt			*/
	NULL,			/* decrypt			*/
	NULL,			/* set salt			*/
	NULL,			/* get salt size	*/
	NULL,			/* set key			*/
	NULL,			/* get key size		*/
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	sha1_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#else /* GLIB_CHECK_VERSION(2,16,0) */

#define SHA1_HMAC_BLOCK_SIZE	64
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
sha1_set_opt(PurpleCipherContext *context, const gchar *name, void *value) {
	struct SHA1Context *ctx;

	ctx = purple_cipher_context_get_data(context);

	if(purple_strequal(name, "sizeHi")) {
		ctx->sizeHi = GPOINTER_TO_INT(value);
	} else if(purple_strequal(name, "sizeLo")) {
		ctx->sizeLo = GPOINTER_TO_INT(value);
	} else if(purple_strequal(name, "lenW")) {
		ctx->lenW = GPOINTER_TO_INT(value);
	}
}

static void *
sha1_get_opt(PurpleCipherContext *context, const gchar *name) {
	struct SHA1Context *ctx;

	ctx = purple_cipher_context_get_data(context);

	if(purple_strequal(name, "sizeHi")) {
		return GINT_TO_POINTER(ctx->sizeHi);
	} else if(purple_strequal(name, "sizeLo")) {
		return GINT_TO_POINTER(ctx->sizeLo);
	} else if(purple_strequal(name, "lenW")) {
		return GINT_TO_POINTER(ctx->lenW);
	}

	return NULL;
}

static void
sha1_init(PurpleCipherContext *context, void *extra) {
	struct SHA1Context *sha1_ctx;

	sha1_ctx = g_new0(struct SHA1Context, 1);

	purple_cipher_context_set_data(context, sha1_ctx);

	purple_cipher_context_reset(context, extra);
}

static void
sha1_reset(PurpleCipherContext *context, void *extra) {
	struct SHA1Context *sha1_ctx;
	gint i;

	sha1_ctx = purple_cipher_context_get_data(context);

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
sha1_uninit(PurpleCipherContext *context) {
	struct SHA1Context *sha1_ctx;

	purple_cipher_context_reset(context, NULL);

	sha1_ctx = purple_cipher_context_get_data(context);

	memset(sha1_ctx, 0, sizeof(struct SHA1Context));

	g_free(sha1_ctx);
	sha1_ctx = NULL;
}


static void
sha1_append(PurpleCipherContext *context, const guchar *data, size_t len) {
	struct SHA1Context *sha1_ctx;
	gint i;

	sha1_ctx = purple_cipher_context_get_data(context);

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
sha1_digest(PurpleCipherContext *context, size_t in_len, guchar digest[20],
			size_t *out_len)
{
	struct SHA1Context *sha1_ctx;
	guchar pad0x80 = 0x80, pad0x00 = 0x00;
	guchar padlen[8];
	gint i;

	g_return_val_if_fail(in_len >= 20, FALSE);

	sha1_ctx = purple_cipher_context_get_data(context);

	g_return_val_if_fail(sha1_ctx, FALSE);

	padlen[0] = (guchar)((sha1_ctx->sizeHi >> 24) & 255);
	padlen[1] = (guchar)((sha1_ctx->sizeHi >> 16) & 255);
	padlen[2] = (guchar)((sha1_ctx->sizeHi >> 8) & 255);
	padlen[3] = (guchar)((sha1_ctx->sizeHi >> 0) & 255);
	padlen[4] = (guchar)((sha1_ctx->sizeLo >> 24) & 255);
	padlen[5] = (guchar)((sha1_ctx->sizeLo >> 16) & 255);
	padlen[6] = (guchar)((sha1_ctx->sizeLo >> 8) & 255);
	padlen[7] = (guchar)((sha1_ctx->sizeLo >> 0) & 255);

	/* pad with a 1, then zeroes, then length */
	purple_cipher_context_append(context, &pad0x80, 1);
	while(sha1_ctx->lenW != 56)
		purple_cipher_context_append(context, &pad0x00, 1);
	purple_cipher_context_append(context, padlen, 8);

	for(i = 0; i < 20; i++) {
		digest[i] = (guchar)(sha1_ctx->H[i / 4] >> 24);
		sha1_ctx->H[i / 4] <<= 8;
	}

	purple_cipher_context_reset(context, NULL);

	if(out_len)
		*out_len = 20;

	return TRUE;
}

static PurpleCipherOps SHA1Ops = {
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
	NULL,			/* get key size		*/
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	sha1_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#endif /* GLIB_CHECK_VERSION(2,16,0) */

/*******************************************************************************
 * SHA-256
 ******************************************************************************/
#define SHA256_HMAC_BLOCK_SIZE	64

static size_t
sha256_get_block_size(PurpleCipherContext *context)
{
	/* This does not change (in this case) */
	return SHA256_HMAC_BLOCK_SIZE;
}

#if GLIB_CHECK_VERSION(2,16,0)

static void
sha256_init(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_init(context, G_CHECKSUM_SHA256);
}

static void
sha256_reset(PurpleCipherContext *context, void *extra)
{
	purple_g_checksum_reset(context, G_CHECKSUM_SHA256);
}

static gboolean
sha256_digest(PurpleCipherContext *context, gsize in_len, guchar digest[20],
            gsize *out_len)
{
	return purple_g_checksum_digest(context, G_CHECKSUM_SHA256, in_len,
	                                digest, out_len);
}

static PurpleCipherOps SHA256Ops = {
	NULL,			/* Set Option		*/
	NULL,			/* Get Option		*/
	sha256_init,	/* init				*/
	sha256_reset,	/* reset			*/
	purple_g_checksum_uninit,	/* uninit			*/
	NULL,			/* set iv			*/
	purple_g_checksum_append,	/* append			*/
	sha256_digest,	/* digest			*/
	NULL,			/* encrypt			*/
	NULL,			/* decrypt			*/
	NULL,			/* set salt			*/
	NULL,			/* get salt size	*/
	NULL,			/* set key			*/
	NULL,			/* get key size		*/
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	sha256_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#else /* GLIB_CHECK_VERSION(2,16,0) */

#define SHA256_ROTR(X,n) ((((X) >> (n)) | ((X) << (32-(n)))) & 0xFFFFFFFF)

static const guint32 sha256_K[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct SHA256Context {
	guint32 H[8];
	guint32 W[64];

	gint lenW;

	guint32 sizeHi;
	guint32 sizeLo;
};

static void
sha256_hash_block(struct SHA256Context *sha256_ctx) {
	gint i;
	guint32 A, B, C, D, E, F, G, H, T1, T2;

	for(i = 16; i < 64; i++) {
		sha256_ctx->W[i] =
			  (SHA256_ROTR(sha256_ctx->W[i-2], 17) ^ SHA256_ROTR(sha256_ctx->W[i-2],  19) ^ (sha256_ctx->W[i-2] >> 10))
			+ sha256_ctx->W[i-7]
			+ (SHA256_ROTR(sha256_ctx->W[i-15], 7) ^ SHA256_ROTR(sha256_ctx->W[i-15], 18) ^ (sha256_ctx->W[i-15] >> 3))
			+ sha256_ctx->W[i-16];
	}

	A = sha256_ctx->H[0];
	B = sha256_ctx->H[1];
	C = sha256_ctx->H[2];
	D = sha256_ctx->H[3];
	E = sha256_ctx->H[4];
	F = sha256_ctx->H[5];
	G = sha256_ctx->H[6];
	H = sha256_ctx->H[7];

	for(i = 0; i < 64; i++) {
        T1 = H
			+ (SHA256_ROTR(E, 6) ^ SHA256_ROTR(E, 11) ^ SHA256_ROTR(E, 25))
			+ ((E & F) ^ ((~E) & G))
			+ sha256_K[i] + sha256_ctx->W[i];
        T2 = (SHA256_ROTR(A, 2) ^ SHA256_ROTR(A, 13) ^ SHA256_ROTR(A, 22))
			+ ((A & B) ^ (A & C) ^ (B & C));
		H = G;
		G = F;
		F = E;
		E = D + T1;
		D = C;
		C = B;
		B = A;
		A = T1 + T2;
	}

	sha256_ctx->H[0] += A;
	sha256_ctx->H[1] += B;
	sha256_ctx->H[2] += C;
	sha256_ctx->H[3] += D;
	sha256_ctx->H[4] += E;
	sha256_ctx->H[5] += F;
	sha256_ctx->H[6] += G;
	sha256_ctx->H[7] += H;
}

static void
sha256_set_opt(PurpleCipherContext *context, const gchar *name, void *value) {
	struct SHA256Context *ctx;

	ctx = purple_cipher_context_get_data(context);

	if(!strcmp(name, "sizeHi")) {
		ctx->sizeHi = GPOINTER_TO_INT(value);
	} else if(!strcmp(name, "sizeLo")) {
		ctx->sizeLo = GPOINTER_TO_INT(value);
	} else if(!strcmp(name, "lenW")) {
		ctx->lenW = GPOINTER_TO_INT(value);
	}
}

static void *
sha256_get_opt(PurpleCipherContext *context, const gchar *name) {
	struct SHA256Context *ctx;

	ctx = purple_cipher_context_get_data(context);

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
sha256_init(PurpleCipherContext *context, void *extra) {
	struct SHA256Context *sha256_ctx;

	sha256_ctx = g_new0(struct SHA256Context, 1);

	purple_cipher_context_set_data(context, sha256_ctx);

	purple_cipher_context_reset(context, extra);
}

static void
sha256_reset(PurpleCipherContext *context, void *extra) {
	struct SHA256Context *sha256_ctx;
	gint i;

	sha256_ctx = purple_cipher_context_get_data(context);

	g_return_if_fail(sha256_ctx);

	sha256_ctx->lenW = 0;
	sha256_ctx->sizeHi = 0;
	sha256_ctx->sizeLo = 0;

	sha256_ctx->H[0] = 0x6a09e667;
	sha256_ctx->H[1] = 0xbb67ae85;
	sha256_ctx->H[2] = 0x3c6ef372;
	sha256_ctx->H[3] = 0xa54ff53a;
	sha256_ctx->H[4] = 0x510e527f;
	sha256_ctx->H[5] = 0x9b05688c;
	sha256_ctx->H[6] = 0x1f83d9ab;
	sha256_ctx->H[7] = 0x5be0cd19;

	for(i = 0; i < 64; i++)
		sha256_ctx->W[i] = 0;
}

static void
sha256_uninit(PurpleCipherContext *context) {
	struct SHA256Context *sha256_ctx;

	purple_cipher_context_reset(context, NULL);

	sha256_ctx = purple_cipher_context_get_data(context);

	memset(sha256_ctx, 0, sizeof(struct SHA256Context));

	g_free(sha256_ctx);
	sha256_ctx = NULL;
}


static void
sha256_append(PurpleCipherContext *context, const guchar *data, size_t len) {
	struct SHA256Context *sha256_ctx;
	gint i;

	sha256_ctx = purple_cipher_context_get_data(context);

	g_return_if_fail(sha256_ctx);

	for(i = 0; i < len; i++) {
		sha256_ctx->W[sha256_ctx->lenW / 4] <<= 8;
		sha256_ctx->W[sha256_ctx->lenW / 4] |= data[i];

		if((++sha256_ctx->lenW) % 64 == 0) {
			sha256_hash_block(sha256_ctx);
			sha256_ctx->lenW = 0;
		}

		sha256_ctx->sizeLo += 8;
		sha256_ctx->sizeHi += (sha256_ctx->sizeLo < 8);
	}
}

static gboolean
sha256_digest(PurpleCipherContext *context, size_t in_len, guchar digest[32],
			size_t *out_len)
{
	struct SHA256Context *sha256_ctx;
	guchar pad0x80 = 0x80, pad0x00 = 0x00;
	guchar padlen[8];
	gint i;

	g_return_val_if_fail(in_len >= 32, FALSE);

	sha256_ctx = purple_cipher_context_get_data(context);

	g_return_val_if_fail(sha256_ctx, FALSE);

	padlen[0] = (guchar)((sha256_ctx->sizeHi >> 24) & 255);
	padlen[1] = (guchar)((sha256_ctx->sizeHi >> 16) & 255);
	padlen[2] = (guchar)((sha256_ctx->sizeHi >> 8) & 255);
	padlen[3] = (guchar)((sha256_ctx->sizeHi >> 0) & 255);
	padlen[4] = (guchar)((sha256_ctx->sizeLo >> 24) & 255);
	padlen[5] = (guchar)((sha256_ctx->sizeLo >> 16) & 255);
	padlen[6] = (guchar)((sha256_ctx->sizeLo >> 8) & 255);
	padlen[7] = (guchar)((sha256_ctx->sizeLo >> 0) & 255);

	/* pad with a 1, then zeroes, then length */
	purple_cipher_context_append(context, &pad0x80, 1);
	while(sha256_ctx->lenW != 56)
		purple_cipher_context_append(context, &pad0x00, 1);
	purple_cipher_context_append(context, padlen, 8);

	for(i = 0; i < 32; i++) {
		digest[i] = (guchar)(sha256_ctx->H[i / 4] >> 24);
		sha256_ctx->H[i / 4] <<= 8;
	}

	purple_cipher_context_reset(context, NULL);

	if(out_len)
		*out_len = 32;

	return TRUE;
}

static PurpleCipherOps SHA256Ops = {
	sha256_set_opt,	/* Set Option		*/
	sha256_get_opt,	/* Get Option		*/
	sha256_init,	/* init				*/
	sha256_reset,	/* reset			*/
	sha256_uninit,	/* uninit			*/
	NULL,			/* set iv			*/
	sha256_append,	/* append			*/
	sha256_digest,	/* digest			*/
	NULL,			/* encrypt			*/
	NULL,			/* decrypt			*/
	NULL,			/* set salt			*/
	NULL,			/* get salt size	*/
	NULL,			/* set key			*/
	NULL,			/* get key size		*/
	NULL,			/* set batch mode */
	NULL,			/* get batch mode */
	sha256_get_block_size,	/* get block size */
	NULL			/* set key with len */
};

#endif /* GLIB_CHECK_VERSION(2,16,0) */

/*******************************************************************************
 * RC4
 ******************************************************************************/

struct RC4Context {
  guchar state[256];
  guchar x;
  guchar y;
  gint key_len;
};

static void
rc4_init(PurpleCipherContext *context, void *extra) {
	struct RC4Context *rc4_ctx;
	rc4_ctx = g_new0(struct RC4Context, 1);
	purple_cipher_context_set_data(context, rc4_ctx);
	purple_cipher_context_reset(context, extra);
}


static void
rc4_reset(PurpleCipherContext *context, void *extra) {
	struct RC4Context *rc4_ctx;
	guint i;

	rc4_ctx = purple_cipher_context_get_data(context);

	g_return_if_fail(rc4_ctx);

	for(i = 0; i < 256; i++)
		rc4_ctx->state[i] = i;
	rc4_ctx->x = 0;
	rc4_ctx->y = 0;

	/* default is 5 bytes (40bit key) */
	rc4_ctx->key_len = 5;

}

static void
rc4_uninit(PurpleCipherContext *context) {
	struct RC4Context *rc4_ctx;

	rc4_ctx = purple_cipher_context_get_data(context);
	memset(rc4_ctx, 0, sizeof(*rc4_ctx));

	g_free(rc4_ctx);
	rc4_ctx = NULL;
}



static void
rc4_set_key (PurpleCipherContext *context, const guchar * key) {
	struct RC4Context *ctx;
	guchar *state;
	guchar temp_swap;
	guchar x, y;
	guint i;

	ctx = purple_cipher_context_get_data(context);

	x = 0;
	y = 0;
	state = &ctx->state[0];
	for(i = 0; i < 256; i++)
	{
		y = (key[x] + state[i] + y) % 256;
		temp_swap = state[i];
		state[i] = state[y];
		state[y] = temp_swap;
		x = (x + 1) % ctx->key_len;
	}
}

static void
rc4_set_opt(PurpleCipherContext *context, const gchar *name, void *value) {
	struct RC4Context *ctx;

	ctx = purple_cipher_context_get_data(context);

	if(purple_strequal(name, "key_len")) {
		ctx->key_len = GPOINTER_TO_INT(value);
	}
}

static size_t
rc4_get_key_size (PurpleCipherContext *context)
{
	struct RC4Context *ctx;

	g_return_val_if_fail(context, -1);

	ctx = purple_cipher_context_get_data(context);

	g_return_val_if_fail(ctx, -1);

	return ctx->key_len;
}

static void *
rc4_get_opt(PurpleCipherContext *context, const gchar *name) {
	struct RC4Context *ctx;

	ctx = purple_cipher_context_get_data(context);

	if(purple_strequal(name, "key_len")) {
		return GINT_TO_POINTER(ctx->key_len);
	}

	return NULL;
}

static gint
rc4_encrypt(PurpleCipherContext *context, const guchar data[],
	    size_t len, guchar output[], size_t *outlen) {
	struct RC4Context *ctx;
	guchar temp_swap;
	guchar x, y, z;
	guchar *state;
	guint i;

	ctx = purple_cipher_context_get_data(context);

	x = ctx->x;
	y = ctx->y;
	state = &ctx->state[0];

	for(i = 0; i < len; i++)
	{
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		temp_swap = state[x];
		state[x] = state[y];
		state[y] = temp_swap;
		z = state[x] + (state[y]) % 256;
		output[i] = data[i] ^ state[z];
	}
	ctx->x = x;
	ctx->y = y;
	if(outlen)
		*outlen = len;

	return 0;
}

static PurpleCipherOps RC4Ops = {
	rc4_set_opt,   /* Set Option    */
	rc4_get_opt,   /* Get Option    */
	rc4_init,      /* init          */
	rc4_reset,     /* reset         */
	rc4_uninit,    /* uninit        */
	NULL,          /* set iv        */
	NULL,          /* append        */
	NULL,          /* digest        */
	rc4_encrypt,   /* encrypt       */
	NULL,          /* decrypt       */
	NULL,          /* set salt      */
	NULL,          /* get salt size */
	rc4_set_key,   /* set key       */
	rc4_get_key_size, /* get key size  */
	NULL,          /* set batch mode */
	NULL,          /* get batch mode */
	NULL,          /* get block size */
	NULL           /* set key with len */
};

/*******************************************************************************
 * Structs
 ******************************************************************************/
struct _PurpleCipher {
	gchar *name;          /**< Internal name - used for searching */
	PurpleCipherOps *ops; /**< Operations supported by this cipher */
	guint ref;            /**< Reference count */
};

struct _PurpleCipherContext {
	PurpleCipher *cipher; /**< Cipher this context is under */
	gpointer data;        /**< Internal cipher state data */
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GList *ciphers = NULL;

/******************************************************************************
 * PurpleCipher API
 *****************************************************************************/
const gchar *
purple_cipher_get_name(PurpleCipher *cipher) {
	g_return_val_if_fail(cipher, NULL);

	return cipher->name;
}

guint
purple_cipher_get_capabilities(PurpleCipher *cipher) {
	PurpleCipherOps *ops = NULL;
	guint caps = 0;

	g_return_val_if_fail(cipher, 0);

	ops = cipher->ops;
	g_return_val_if_fail(ops, 0);

	if(ops->set_option)
		caps |= PURPLE_CIPHER_CAPS_SET_OPT;
	if(ops->get_option)
		caps |= PURPLE_CIPHER_CAPS_GET_OPT;
	if(ops->init)
		caps |= PURPLE_CIPHER_CAPS_INIT;
	if(ops->reset)
		caps |= PURPLE_CIPHER_CAPS_RESET;
	if(ops->uninit)
		caps |= PURPLE_CIPHER_CAPS_UNINIT;
	if(ops->set_iv)
		caps |= PURPLE_CIPHER_CAPS_SET_IV;
	if(ops->append)
		caps |= PURPLE_CIPHER_CAPS_APPEND;
	if(ops->digest)
		caps |= PURPLE_CIPHER_CAPS_DIGEST;
	if(ops->encrypt)
		caps |= PURPLE_CIPHER_CAPS_ENCRYPT;
	if(ops->decrypt)
		caps |= PURPLE_CIPHER_CAPS_DECRYPT;
	if(ops->set_salt)
		caps |= PURPLE_CIPHER_CAPS_SET_SALT;
	if(ops->get_salt_size)
		caps |= PURPLE_CIPHER_CAPS_GET_SALT_SIZE;
	if(ops->set_key)
		caps |= PURPLE_CIPHER_CAPS_SET_KEY;
	if(ops->get_key_size)
		caps |= PURPLE_CIPHER_CAPS_GET_KEY_SIZE;
	if(ops->set_batch_mode)
		caps |= PURPLE_CIPHER_CAPS_SET_BATCH_MODE;
	if(ops->get_batch_mode)
		caps |= PURPLE_CIPHER_CAPS_GET_BATCH_MODE;
	if(ops->get_block_size)
		caps |= PURPLE_CIPHER_CAPS_GET_BLOCK_SIZE;
	if(ops->set_key_with_len)
		caps |= PURPLE_CIPHER_CAPS_SET_KEY_WITH_LEN;

	return caps;
}

gboolean
purple_cipher_digest_region(const gchar *name, const guchar *data,
						  size_t data_len, size_t in_len,
						  guchar digest[], size_t *out_len)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	gboolean ret = FALSE;

	g_return_val_if_fail(name, FALSE);
	g_return_val_if_fail(data, FALSE);

	cipher = purple_ciphers_find_cipher(name);

	g_return_val_if_fail(cipher, FALSE);

	if(!cipher->ops->append || !cipher->ops->digest) {
		purple_debug_warning("cipher", "purple_cipher_region failed: "
						"the %s cipher does not support appending and or "
						"digesting.", cipher->name);
		return FALSE;
	}

	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, data, data_len);
	ret = purple_cipher_context_digest(context, in_len, digest, out_len);
	purple_cipher_context_destroy(context);

	return ret;
}

/******************************************************************************
 * PurpleCiphers API
 *****************************************************************************/
PurpleCipher *
purple_ciphers_find_cipher(const gchar *name) {
	PurpleCipher *cipher;
	GList *l;

	g_return_val_if_fail(name, NULL);

	for(l = ciphers; l; l = l->next) {
		cipher = PURPLE_CIPHER(l->data);

		if(!g_ascii_strcasecmp(cipher->name, name))
			return cipher;
	}

	return NULL;
}

PurpleCipher *
purple_ciphers_register_cipher(const gchar *name, PurpleCipherOps *ops) {
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(name, NULL);
	g_return_val_if_fail(ops, NULL);
	g_return_val_if_fail(!purple_ciphers_find_cipher(name), NULL);

	cipher = g_new0(PurpleCipher, 1);
	PURPLE_DBUS_REGISTER_POINTER(cipher, PurpleCipher);

	cipher->name = g_strdup(name);
	cipher->ops = ops;

	ciphers = g_list_append(ciphers, cipher);

	purple_signal_emit(purple_ciphers_get_handle(), "cipher-added", cipher);

	return cipher;
}

gboolean
purple_ciphers_unregister_cipher(PurpleCipher *cipher) {
	g_return_val_if_fail(cipher, FALSE);
	g_return_val_if_fail(cipher->ref == 0, FALSE);

	purple_signal_emit(purple_ciphers_get_handle(), "cipher-removed", cipher);

	ciphers = g_list_remove(ciphers, cipher);

	g_free(cipher->name);

	PURPLE_DBUS_UNREGISTER_POINTER(cipher);
	g_free(cipher);

	return TRUE;
}

GList *
purple_ciphers_get_ciphers() {
	return ciphers;
}

/******************************************************************************
 * PurpleCipher Subsystem API
 *****************************************************************************/
gpointer
purple_ciphers_get_handle() {
	static gint handle;

	return &handle;
}

void
purple_ciphers_init() {
	gpointer handle;

	handle = purple_ciphers_get_handle();

	purple_signal_register(handle, "cipher-added",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CIPHER));
	purple_signal_register(handle, "cipher-removed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CIPHER));

	purple_ciphers_register_cipher("md5", &MD5Ops);
	purple_ciphers_register_cipher("sha1", &SHA1Ops);
	purple_ciphers_register_cipher("sha256", &SHA256Ops);
	purple_ciphers_register_cipher("md4", &MD4Ops);
	purple_ciphers_register_cipher("hmac", &HMACOps);
	purple_ciphers_register_cipher("des", &DESOps);
	purple_ciphers_register_cipher("des3", &DES3Ops);
	purple_ciphers_register_cipher("rc4", &RC4Ops);
}

void
purple_ciphers_uninit() {
	PurpleCipher *cipher;
	GList *l, *ll;

	for(l = ciphers; l; l = ll) {
		ll = l->next;

		cipher = PURPLE_CIPHER(l->data);
		purple_ciphers_unregister_cipher(cipher);
	}

	g_list_free(ciphers);

	purple_signals_unregister_by_instance(purple_ciphers_get_handle());
}
/******************************************************************************
 * PurpleCipherContext API
 *****************************************************************************/
void
purple_cipher_context_set_option(PurpleCipherContext *context, const gchar *name,
							   gpointer value)
{
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);
	g_return_if_fail(name);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_option)
		cipher->ops->set_option(context, name, value);
	else
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"set_option operation\n", cipher->name);
}

gpointer
purple_cipher_context_get_option(PurpleCipherContext *context, const gchar *name) {
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, NULL);
	g_return_val_if_fail(name, NULL);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, NULL);

	if(cipher->ops && cipher->ops->get_option)
		return cipher->ops->get_option(context, name);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"get_option operation\n", cipher->name);

		return NULL;
	}
}

PurpleCipherContext *
purple_cipher_context_new(PurpleCipher *cipher, void *extra) {
	PurpleCipherContext *context = NULL;

	g_return_val_if_fail(cipher, NULL);

	cipher->ref++;

	context = g_new0(PurpleCipherContext, 1);
	context->cipher = cipher;

	if(cipher->ops->init)
		cipher->ops->init(context, extra);

	return context;
}

PurpleCipherContext *
purple_cipher_context_new_by_name(const gchar *name, void *extra) {
	PurpleCipher *cipher;

	g_return_val_if_fail(name, NULL);

	cipher = purple_ciphers_find_cipher(name);

	g_return_val_if_fail(cipher, NULL);

	return purple_cipher_context_new(cipher, extra);
}

void
purple_cipher_context_reset(PurpleCipherContext *context, void *extra) {
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->reset)
		context->cipher->ops->reset(context, extra);
}

void
purple_cipher_context_destroy(PurpleCipherContext *context) {
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	cipher->ref--;

	if(cipher->ops && cipher->ops->uninit)
		cipher->ops->uninit(context);

	memset(context, 0, sizeof(*context));
	g_free(context);
	context = NULL;
}

void
purple_cipher_context_set_iv(PurpleCipherContext *context, guchar *iv, size_t len)
{
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);
	g_return_if_fail(iv);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_iv)
		cipher->ops->set_iv(context, iv, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not support the set"
						"initialization vector operation\n", cipher->name);
}

void
purple_cipher_context_append(PurpleCipherContext *context, const guchar *data,
								size_t len)
{
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->append)
		cipher->ops->append(context, data, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not support the append "
						"operation\n", cipher->name);
}

gboolean
purple_cipher_context_digest(PurpleCipherContext *context, size_t in_len,
						   guchar digest[], size_t *out_len)
{
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, FALSE);

	cipher = context->cipher;

	if(cipher->ops && cipher->ops->digest)
		return cipher->ops->digest(context, in_len, digest, out_len);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the digest "
						"operation\n", cipher->name);
		return FALSE;
	}
}

gboolean
purple_cipher_context_digest_to_str(PurpleCipherContext *context, size_t in_len,
								   gchar digest_s[], size_t *out_len)
{
	/* 8k is a bit excessive, will tweak later. */
	guchar digest[BUF_LEN * 4];
	gint n = 0;
	size_t dlen = 0;

	g_return_val_if_fail(context, FALSE);
	g_return_val_if_fail(digest_s, FALSE);

	if(!purple_cipher_context_digest(context, sizeof(digest), digest, &dlen))
		return FALSE;

	/* in_len must be greater than dlen * 2 so we have room for the NUL. */
	if(in_len <= dlen * 2)
		return FALSE;

	for(n = 0; n < dlen; n++)
		sprintf(digest_s + (n * 2), "%02x", digest[n]);

	digest_s[n * 2] = '\0';

	if(out_len)
		*out_len = dlen * 2;

	return TRUE;
}

gint
purple_cipher_context_encrypt(PurpleCipherContext *context, const guchar data[],
							size_t len, guchar output[], size_t *outlen)
{
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->encrypt)
		return cipher->ops->encrypt(context, data, len, output, outlen);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the encrypt"
						"operation\n", cipher->name);

		if(outlen)
			*outlen = -1;

		return -1;
	}
}

gint
purple_cipher_context_decrypt(PurpleCipherContext *context, const guchar data[],
							size_t len, guchar output[], size_t *outlen)
{
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->decrypt)
		return cipher->ops->decrypt(context, data, len, output, outlen);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the decrypt"
						"operation\n", cipher->name);

		if(outlen)
			*outlen = -1;

		return -1;
	}
}

void
purple_cipher_context_set_salt(PurpleCipherContext *context, guchar *salt) {
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_salt)
		cipher->ops->set_salt(context, salt);
	else
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"set_salt operation\n", cipher->name);
}

size_t
purple_cipher_context_get_salt_size(PurpleCipherContext *context) {
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_salt_size)
		return cipher->ops->get_salt_size(context);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"get_salt_size operation\n", cipher->name);

		return -1;
	}
}

void
purple_cipher_context_set_key(PurpleCipherContext *context, const guchar *key) {
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_key)
		cipher->ops->set_key(context, key);
	else
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"set_key operation\n", cipher->name);
}

size_t
purple_cipher_context_get_key_size(PurpleCipherContext *context) {
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_key_size)
		return cipher->ops->get_key_size(context);
	else {
		purple_debug_warning("cipher", "the %s cipher does not support the "
						"get_key_size operation\n", cipher->name);

		return -1;
	}
}

void
purple_cipher_context_set_batch_mode(PurpleCipherContext *context,
                                     PurpleCipherBatchMode mode)
{
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_batch_mode)
		cipher->ops->set_batch_mode(context, mode);
	else
		purple_debug_warning("cipher", "The %s cipher does not support the "
		                            "set_batch_mode operation\n", cipher->name);
}

PurpleCipherBatchMode
purple_cipher_context_get_batch_mode(PurpleCipherContext *context)
{
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_batch_mode)
		return cipher->ops->get_batch_mode(context);
	else {
		purple_debug_warning("cipher", "The %s cipher does not support the "
		                            "get_batch_mode operation\n", cipher->name);
		return -1;
	}
}

size_t
purple_cipher_context_get_block_size(PurpleCipherContext *context)
{
	PurpleCipher *cipher = NULL;

	g_return_val_if_fail(context, -1);

	cipher = context->cipher;
	g_return_val_if_fail(cipher, -1);

	if(cipher->ops && cipher->ops->get_block_size)
		return cipher->ops->get_block_size(context);
	else {
		purple_debug_warning("cipher", "The %s cipher does not support the "
		                            "get_block_size operation\n", cipher->name);
		return -1;
	}
}

void
purple_cipher_context_set_key_with_len(PurpleCipherContext *context,
                                       const guchar *key, size_t len)
{
	PurpleCipher *cipher = NULL;

	g_return_if_fail(context);

	cipher = context->cipher;
	g_return_if_fail(cipher);

	if(cipher->ops && cipher->ops->set_key_with_len)
		cipher->ops->set_key_with_len(context, key, len);
	else
		purple_debug_warning("cipher", "The %s cipher does not support the "
		                            "set_key_with_len operation\n", cipher->name);
}

void
purple_cipher_context_set_data(PurpleCipherContext *context, gpointer data) {
	g_return_if_fail(context);

	context->data = data;
}

gpointer
purple_cipher_context_get_data(PurpleCipherContext *context) {
	g_return_val_if_fail(context, NULL);

	return context->data;
}

gchar *purple_cipher_http_digest_calculate_session_key(
		const gchar *algorithm,
		const gchar *username,
		const gchar *realm,
		const gchar *password,
		const gchar *nonce,
		const gchar *client_nonce)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	gchar hash[33]; /* We only support MD5. */

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(realm    != NULL, NULL);
	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(nonce    != NULL, NULL);

	/* Check for a supported algorithm. */
	g_return_val_if_fail(algorithm == NULL ||
						 *algorithm == '\0' ||
						 g_ascii_strcasecmp(algorithm, "MD5") ||
						 g_ascii_strcasecmp(algorithm, "MD5-sess"), NULL);

	cipher = purple_ciphers_find_cipher("md5");
	g_return_val_if_fail(cipher != NULL, NULL);

	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (guchar *)username, strlen(username));
	purple_cipher_context_append(context, (guchar *)":", 1);
	purple_cipher_context_append(context, (guchar *)realm, strlen(realm));
	purple_cipher_context_append(context, (guchar *)":", 1);
	purple_cipher_context_append(context, (guchar *)password, strlen(password));

	if (algorithm != NULL && !g_ascii_strcasecmp(algorithm, "MD5-sess"))
	{
		guchar digest[16];

		if (client_nonce == NULL)
		{
			purple_cipher_context_destroy(context);
			purple_debug_error("cipher", "Required client_nonce missing for MD5-sess digest calculation.\n");
			return NULL;
		}

		purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
		purple_cipher_context_destroy(context);

		context = purple_cipher_context_new(cipher, NULL);
		purple_cipher_context_append(context, digest, sizeof(digest));
		purple_cipher_context_append(context, (guchar *)":", 1);
		purple_cipher_context_append(context, (guchar *)nonce, strlen(nonce));
		purple_cipher_context_append(context, (guchar *)":", 1);
		purple_cipher_context_append(context, (guchar *)client_nonce, strlen(client_nonce));
	}

	purple_cipher_context_digest_to_str(context, sizeof(hash), hash, NULL);
	purple_cipher_context_destroy(context);

	return g_strdup(hash);
}

gchar *purple_cipher_http_digest_calculate_response(
		const gchar *algorithm,
		const gchar *method,
		const gchar *digest_uri,
		const gchar *qop,
		const gchar *entity,
		const gchar *nonce,
		const gchar *nonce_count,
		const gchar *client_nonce,
		const gchar *session_key)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	static gchar hash2[33]; /* We only support MD5. */

	g_return_val_if_fail(method      != NULL, NULL);
	g_return_val_if_fail(digest_uri  != NULL, NULL);
	g_return_val_if_fail(nonce       != NULL, NULL);
	g_return_val_if_fail(session_key != NULL, NULL);

	/* Check for a supported algorithm. */
	g_return_val_if_fail(algorithm == NULL ||
						 *algorithm == '\0' ||
						 g_ascii_strcasecmp(algorithm, "MD5") ||
						 g_ascii_strcasecmp(algorithm, "MD5-sess"), NULL);

	/* Check for a supported "quality of protection". */
	g_return_val_if_fail(qop == NULL ||
						 *qop == '\0' ||
						 g_ascii_strcasecmp(qop, "auth") ||
						 g_ascii_strcasecmp(qop, "auth-int"), NULL);

	cipher = purple_ciphers_find_cipher("md5");
	g_return_val_if_fail(cipher != NULL, NULL);

	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (guchar *)method, strlen(method));
	purple_cipher_context_append(context, (guchar *)":", 1);
	purple_cipher_context_append(context, (guchar *)digest_uri, strlen(digest_uri));

	if (qop != NULL && !g_ascii_strcasecmp(qop, "auth-int"))
	{
		PurpleCipherContext *context2;
		gchar entity_hash[33];

		if (entity == NULL)
		{
			purple_cipher_context_destroy(context);
			purple_debug_error("cipher", "Required entity missing for auth-int digest calculation.\n");
			return NULL;
		}

		context2 = purple_cipher_context_new(cipher, NULL);
		purple_cipher_context_append(context2, (guchar *)entity, strlen(entity));
		purple_cipher_context_digest_to_str(context2, sizeof(entity_hash), entity_hash, NULL);
		purple_cipher_context_destroy(context2);

		purple_cipher_context_append(context, (guchar *)":", 1);
		purple_cipher_context_append(context, (guchar *)entity_hash, strlen(entity_hash));
	}

	purple_cipher_context_digest_to_str(context, sizeof(hash2), hash2, NULL);
	purple_cipher_context_destroy(context);

	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, (guchar *)session_key, strlen(session_key));
	purple_cipher_context_append(context, (guchar *)":", 1);
	purple_cipher_context_append(context, (guchar *)nonce, strlen(nonce));
	purple_cipher_context_append(context, (guchar *)":", 1);

	if (qop != NULL && *qop != '\0')
	{
		if (nonce_count == NULL)
		{
			purple_cipher_context_destroy(context);
			purple_debug_error("cipher", "Required nonce_count missing for digest calculation.\n");
			return NULL;
		}

		if (client_nonce == NULL)
		{
			purple_cipher_context_destroy(context);
			purple_debug_error("cipher", "Required client_nonce missing for digest calculation.\n");
			return NULL;
		}

		purple_cipher_context_append(context, (guchar *)nonce_count, strlen(nonce_count));
		purple_cipher_context_append(context, (guchar *)":", 1);
		purple_cipher_context_append(context, (guchar *)client_nonce, strlen(client_nonce));
		purple_cipher_context_append(context, (guchar *)":", 1);

		purple_cipher_context_append(context, (guchar *)qop, strlen(qop));

		purple_cipher_context_append(context, (guchar *)":", 1);
	}

	purple_cipher_context_append(context, (guchar *)hash2, strlen(hash2));
	purple_cipher_context_digest_to_str(context, sizeof(hash2), hash2, NULL);
	purple_cipher_context_destroy(context);

	return g_strdup(hash2);
}
