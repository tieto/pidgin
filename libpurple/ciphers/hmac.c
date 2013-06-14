/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "hmac.h"

#include <string.h>

/*******************************************************************************
 * Structs
 ******************************************************************************/
#define PURPLE_HMAC_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_HMAC_CIPHER, PurpleHMACCipherPrivate))

typedef struct {
	PurpleCipher *hash;
	guchar *ipad;
	guchar *opad;
} PurpleHMACCipherPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_NONE,
	PROP_HASH,
	PROP_LAST,
};

/*******************************************************************************
 * Globals
 ******************************************************************************/
static GObjectClass *parent_class = NULL;

/*******************************************************************************
 * Helpers
 ******************************************************************************/
static void
purple_hmac_cipher_set_hash(PurpleCipher *cipher,
							PurpleCipher *hash)
{
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	priv->hash = g_object_ref(G_OBJECT(hash));
	g_object_notify(G_OBJECT(cipher), "hash");
}

/*******************************************************************************
 * Cipher Stuff
 ******************************************************************************/
static void
purple_hmac_cipher_reset(PurpleCipher *cipher) {
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	if(PURPLE_IS_CIPHER(priv->hash))
		purple_cipher_reset(priv->hash);

	if(priv->ipad) {
		g_free(priv->ipad);
		priv->ipad = NULL;
	}
	if(priv->opad) {
		g_free(priv->opad);
		priv->opad = NULL;
	}
}

static void
purple_hmac_cipher_reset_state(PurpleCipher *cipher) {
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	if(PURPLE_IS_CIPHER(priv->hash)) {
		purple_cipher_reset_state(priv->hash);
		purple_cipher_append(priv->hash, priv->ipad,
								purple_cipher_get_block_size(priv->hash));
	}
}

static void
purple_hmac_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len) {
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	if(PURPLE_IS_CIPHER(priv->hash))
		purple_cipher_set_iv(priv->hash, iv, len);
}

static void
purple_hmac_cipher_append(PurpleCipher *cipher, const guchar *d, size_t l) {
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(PURPLE_IS_CIPHER(priv->hash));

	purple_cipher_append(priv->hash, d, l);
}

static gboolean
purple_hmac_cipher_digest(PurpleCipher *cipher, guchar *out, size_t len)
{
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);
	guchar *digest = NULL;
	size_t hash_len, block_size;
	gboolean result = FALSE;

	g_return_val_if_fail(PURPLE_IS_CIPHER(priv->hash), FALSE);

	hash_len = purple_cipher_get_digest_size(priv->hash);
	g_return_val_if_fail(hash_len > 0, FALSE);

	block_size = purple_cipher_get_block_size(priv->hash);
	digest = g_malloc(hash_len);

	/* get the digest of the data */
	result = purple_cipher_digest(priv->hash, digest, hash_len);
	purple_cipher_reset(priv->hash);

	if(!result) {
		g_free(digest);

		return FALSE;
	}

	/* now append the opad and the digest from above */
	purple_cipher_append(priv->hash, priv->opad, block_size);
	purple_cipher_append(priv->hash, digest, hash_len);

	/* do our last digest */
	result = purple_cipher_digest(priv->hash, out, len);

	/* cleanup */
	g_free(digest);

	return result;
}

static size_t
purple_hmac_cipher_get_digest_size(PurpleCipher *cipher)
{
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	g_return_val_if_fail(priv->hash != NULL, 0);

	return purple_cipher_get_digest_size(priv->hash);
}

static void
purple_hmac_cipher_set_key(PurpleCipher *cipher, const guchar *key,
								size_t key_len)
{
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);
	gint block_size, i;
	guchar *full_key;

	g_return_if_fail(priv->hash);

	g_free(priv->ipad);
	g_free(priv->opad);

	block_size = purple_cipher_get_block_size(priv->hash);
	priv->ipad = g_malloc(block_size);
	priv->opad = g_malloc(block_size);

	if (key_len > block_size) {
		purple_cipher_reset(priv->hash);
		purple_cipher_append(priv->hash, key, key_len);

		key_len = purple_cipher_get_digest_size(priv->hash);
		full_key = g_malloc(key_len);
		purple_cipher_digest(priv->hash, full_key, key_len);
	} else {
		full_key = g_memdup(key, key_len);
	}

    if (key_len < block_size) {
		full_key = g_realloc(full_key, block_size);
		memset(full_key + key_len, 0, block_size - key_len);
    }

	for(i = 0; i < block_size; i++) {
		priv->ipad[i] = 0x36 ^ full_key[i];
		priv->opad[i] = 0x5c ^ full_key[i];
	}

	g_free(full_key);

	purple_cipher_reset(priv->hash);
	purple_cipher_append(priv->hash, priv->ipad, block_size);
}

static size_t
purple_hmac_cipher_get_block_size(PurpleCipher *cipher)
{
	PurpleHMACCipherPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HMAC_CIPHER(cipher), 0);

	priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	return purple_hmac_cipher_get_block_size(priv->hash);
}

static const gchar*
purple_hmac_cipher_get_name(PurpleCipher *cipher)
{
	return "hmac";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_hmac_cipher_set_property(GObject *obj, guint param_id,
								const GValue *value,
								GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_HASH:
			purple_hmac_cipher_set_hash(cipher, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_hmac_cipher_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	PurpleHMACCipher *cipher = PURPLE_HMAC_CIPHER(obj);

	switch(param_id) {
		case PROP_HASH:
			g_value_set_object(value,
							   purple_hmac_cipher_get_hash(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_hmac_cipher_finalize(GObject *obj) {
	PurpleCipher *cipher = PURPLE_CIPHER(obj);
	PurpleHMACCipherPrivate *priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	if (priv->hash != NULL)
		g_object_unref(priv->hash);

	parent_class->finalize(obj);
}

static void
purple_hmac_cipher_class_init(PurpleHMACCipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleHMACCipherPrivate));

	obj_class->finalize = purple_hmac_cipher_finalize;
	obj_class->get_property = purple_hmac_cipher_get_property;
	obj_class->set_property = purple_hmac_cipher_set_property;

	cipher_class->reset = purple_hmac_cipher_reset;
	cipher_class->reset_state = purple_hmac_cipher_reset_state;
	cipher_class->set_iv = purple_hmac_cipher_set_iv;
	cipher_class->append = purple_hmac_cipher_append;
	cipher_class->digest = purple_hmac_cipher_digest;
	cipher_class->get_digest_size = purple_hmac_cipher_get_digest_size;
	cipher_class->set_key = purple_hmac_cipher_set_key;
	cipher_class->get_block_size = purple_hmac_cipher_get_block_size;
	cipher_class->get_name = purple_hmac_cipher_get_name;

	pspec = g_param_spec_object("hash", "hash", "hash", PURPLE_TYPE_CIPHER,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(obj_class, PROP_HASH, pspec);
}

/******************************************************************************
 * PurpleHMACCipher API
 *****************************************************************************/
GType
purple_hmac_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleHMACCipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_hmac_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleHMACCipher),
			0,
			(GInstanceInitFunc)purple_cipher_reset,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleHMACCipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_hmac_cipher_new(PurpleCipher *hash) {
	g_return_val_if_fail(PURPLE_IS_CIPHER(hash), NULL);

	return g_object_new(PURPLE_TYPE_HMAC_CIPHER,
						"hash", hash,
						NULL);
}

PurpleCipher *
purple_hmac_cipher_get_hash(const PurpleHMACCipher *cipher) {
	PurpleHMACCipherPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HMAC_CIPHER(cipher), NULL);

	priv = PURPLE_HMAC_CIPHER_GET_PRIVATE(cipher);

	if(priv && priv->hash)
		return priv->hash;

	return NULL;
}

