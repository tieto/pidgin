/*
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
 */
#include "md4.h"

#include <string.h>

#define MD4_DIGEST_SIZE		16
#define MD4_BLOCK_WORDS		16
#define MD4_CIPHER_WORDS		4

#define PURPLE_MD4_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MD4_CIPHER, PurpleMD4CipherPrivate))

/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	guint32 hash[MD4_CIPHER_WORDS];
	guint32 block[MD4_BLOCK_WORDS];
	guint64 byte_count;
} PurpleMD4CipherPrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
#define ROUND1(a,b,c,d,k,s) \
	(a = lshift(a + F(b,c,d) + k, s))

#define ROUND2(a,b,c,d,k,s) \
	(a = lshift(a + G(b,c,d) + k + (guint32)0x5a827999,s))

#define ROUND3(a,b,c,d,k,s) \
	(a = lshift(a + H(b,c,d) + k + (guint32)0x6ed9eba1,s))

static inline guint32
lshift(guint32 x, unsigned int s) {
	x &= 0xffffffff;
	return (((x << s) & 0xffffffff) | (x >> (32 - s)));
}

static inline guint32
F(guint32 x, guint32 y, guint32 z) {
	return ((x & y) | ((~x) & z));
}

static inline guint32
G(guint32 x, guint32 y, guint32 z) {
	return ((x & y) | (x & z) | (y & z));
}

static inline guint32
H(guint32 x, guint32 y, guint32 z) {
	return (x ^ y ^ z);
}

static inline void
le32_to_cpu_array(guint32 *buf, unsigned int words) {
	while(words--) {
		*buf = GUINT_FROM_LE(*buf);
		buf++;
	}
}

static inline void
cpu_to_le32_array(guint32 *buf, unsigned int words) {
	while(words--) {
		*buf = GUINT_TO_LE(*buf);
		buf++;
	}
}

static void
md4_transform(guint32 *hash, guint32 const *in) {
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

static inline void
md4_transform_helper(PurpleCipher *cipher) {
	PurpleMD4CipherPrivate *priv = PURPLE_MD4_CIPHER_GET_PRIVATE(cipher);

	le32_to_cpu_array(priv->block, sizeof(priv->block) / sizeof(guint32));
	md4_transform(priv->hash, priv->block);
}

/******************************************************************************
 * Hash Stuff
 *****************************************************************************/
static void
purple_md4_cipher_reset(PurpleCipher *cipher) {
	PurpleMD4CipherPrivate *priv = PURPLE_MD4_CIPHER_GET_PRIVATE(cipher);

	priv->hash[0] = 0x67452301;
	priv->hash[1] = 0xefcdab89;
	priv->hash[2] = 0x98badcfe;
	priv->hash[3] = 0x10325476;

	priv->byte_count = 0;

	memset(priv->block, 0, sizeof(priv->block));
}

static void
purple_md4_cipher_append(PurpleCipher *cipher, const guchar *data, size_t len) {
	PurpleMD4CipherPrivate *priv = PURPLE_MD4_CIPHER_GET_PRIVATE(cipher);
	const guint32 avail = sizeof(priv->block) - (priv->byte_count & 0x3f);

	priv->byte_count += len;

	if(avail > len) {
		memcpy((char *)priv->block +
			   (sizeof(priv->block) - avail),
			   data, len);
		return;
	}

	memcpy((char *)priv->block +
		   (sizeof(priv->block) - avail),
		   data, avail);

	md4_transform_helper(cipher);
	data += avail;
	len -= avail;

	while(len >= sizeof(priv->block)) {
		memcpy(priv->block, data, sizeof(priv->block));
		md4_transform_helper(cipher);
		data += sizeof(priv->block);
		len -= sizeof(priv->block);
	}

	memcpy(priv->block, data, len);
}

static gboolean
purple_md4_cipher_digest(PurpleCipher *cipher, guchar *out, size_t len)
{
	PurpleMD4CipherPrivate *priv = PURPLE_MD4_CIPHER_GET_PRIVATE(cipher);
	const unsigned int offset = priv->byte_count & 0x3f;
	gchar *p = (gchar *)priv->block + offset;
	gint padding = 56 - (offset + 1);

	if(len < 16)
		return FALSE;

	*p++ = 0x80;

	if(padding < 0) {
		memset(p, 0x00, padding + sizeof(guint64));
		md4_transform_helper(cipher);
		p = (gchar *)priv->block;
		padding = 56;
	}

	memset(p, 0, padding);
	priv->block[14] = priv->byte_count << 3;
	priv->block[15] = priv->byte_count >> 29;
	le32_to_cpu_array(priv->block,
					  (sizeof(priv->block) - sizeof(guint64)) /
					  sizeof(guint32));
	md4_transform(priv->hash, priv->block);
	cpu_to_le32_array(priv->hash, sizeof(priv->hash) / sizeof(guint32));
	memcpy(out, priv->hash, sizeof(priv->hash));

	return TRUE;
}

static size_t
purple_md4_cipher_get_digest_size(PurpleCipher *cipher)
{
	return 16;
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_md4_cipher_class_init(PurpleMD4CipherClass *klass) {
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleMD4CipherPrivate));

	cipher_class->reset = purple_md4_cipher_reset;
	cipher_class->reset_state = purple_md4_cipher_reset;
	cipher_class->append = purple_md4_cipher_append;
	cipher_class->digest = purple_md4_cipher_digest;
	cipher_class->get_digest_size = purple_md4_cipher_get_digest_size;
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_md4_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMD4CipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_md4_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleMD4Cipher),
			0,
			(GInstanceInitFunc)purple_cipher_reset,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleMD4Cipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_md4_cipher_new(void) {
	return g_object_new(PURPLE_TYPE_MD4_CIPHER, NULL);
}
