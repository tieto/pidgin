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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 *
 * Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>
 */

#include "pbkdf2cipher.h"
#include "hmaccipher.h"
#include "debug.h"

/* 1024bit */
#define PBKDF2_HASH_MAX_LEN 128

/******************************************************************************
 * Structs
 *****************************************************************************/
#define PURPLE_PBKDF2_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PBKDF2_CIPHER, PurplePBKDF2CipherPrivate))

typedef struct {
	PurpleHash *hash;
	guint iter_count;
	size_t out_len;

	guchar *salt;
	size_t salt_len;
	guchar *passphrase;
	size_t passphrase_len;
} PurplePBKDF2CipherPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_NONE,
	PROP_HASH,
	PROP_ITER_COUNT,
	PROP_OUT_LEN,
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
purple_pbkdf2_cipher_set_hash(PurpleCipher *cipher,
								PurpleCipher *hash)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	priv->hash = g_object_ref(G_OBJECT(hash));
	g_object_notify(G_OBJECT(cipher), "hash");
}

/******************************************************************************
 * Cipher Stuff
 *****************************************************************************/
static void
purple_pbkdf2_cipher_reset(PurpleCipher *cipher)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(priv != NULL);

	if(PURPLE_IS_HASH(priv->hash))
		purple_hash_reset(priv->hash);
	priv->iter_count = 1;
	priv->out_len = 256;

	purple_cipher_reset_state(cipher);
}

static void
purple_pbkdf2_cipher_reset_state(PurpleCipher *cipher)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(priv != NULL);

	purple_cipher_set_salt(cipher, NULL, 0);
	purple_cipher_set_key(cipher, NULL, 0);
}

static size_t
purple_pbkdf2_cipher_get_digest_size(PurpleCipher *cipher)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->out_len;
}

static void
purple_pbkdf2_cipher_set_salt(PurpleCipher *cipher, const guchar *salt, size_t len)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(priv != NULL);

	g_free(priv->salt);
	priv->salt = NULL;
	priv->salt_len = 0;

	if (len == 0)
		return;
	g_return_if_fail(salt != NULL);

	priv->salt = g_memdup(salt, len);
	priv->salt_len = len;
}

static void
purple_pbkdf2_cipher_set_key(PurpleCipher *cipher, const guchar *key,
	size_t len)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(priv != NULL);

	if (priv->passphrase != NULL) {
		memset(priv->passphrase, 0, priv->passphrase_len);
		g_free(priv->passphrase);
		priv->passphrase = NULL;
	}
	priv->passphrase_len = 0;

	if (len == 0)
		return;
	g_return_if_fail(key != NULL);

	priv->passphrase = g_memdup(key, len);
	priv->passphrase_len = len;
}

/* inspired by gnutls 3.1.10, pbkdf2-sha1.c */
static gboolean
purple_pbkdf2_cipher_digest(PurpleCipher *cipher, guchar digest[], size_t len)
{
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	guchar halfkey[PBKDF2_HASH_MAX_LEN], halfkey_hash[PBKDF2_HASH_MAX_LEN];
	guint halfkey_len, halfkey_count, halfkey_pad, halfkey_no;
	guchar *salt_ext;
	size_t salt_ext_len;
	guint iter_no;
	PurpleCipher *hash;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(digest != NULL, FALSE);
	g_return_val_if_fail(len >= priv->out_len, FALSE);

	g_return_val_if_fail(priv->hash != NULL, FALSE);
	g_return_val_if_fail(priv->iter_count > 0, FALSE);
	g_return_val_if_fail(priv->passphrase != NULL ||
		priv->passphrase_len == 0, FALSE);
	g_return_val_if_fail(priv->salt != NULL || priv->salt_len == 0,
		FALSE);
	g_return_val_if_fail(priv->out_len > 0, FALSE);
	g_return_val_if_fail(priv->out_len < 0xFFFFFFFFU, FALSE);

	salt_ext_len = priv->salt_len + 4;

	hash = purple_hmac_cipher_new(priv->hash);
	if (hash == NULL) {
		purple_debug_error("pbkdf2", "Couldn't create new hmac "
			"cipher\n");
		return FALSE;
	}
	purple_cipher_set_key(hash, (const guchar*)priv->passphrase,
		priv->passphrase_len);

	halfkey_len = purple_cipher_get_digest_size(hash);
	if (halfkey_len <= 0 || halfkey_len > PBKDF2_HASH_MAX_LEN) {
		purple_debug_error("pbkdf2", "Unsupported hash function. "
			"(digest size: %d)\n", halfkey_len);
		return FALSE;
	}

	halfkey_count = ((priv->out_len - 1) / halfkey_len) + 1;
	halfkey_pad = priv->out_len - (halfkey_count - 1) * halfkey_len;

	salt_ext = g_new(guchar, salt_ext_len);
	memcpy(salt_ext, priv->salt, priv->salt_len);

	for (halfkey_no = 1; halfkey_no <= halfkey_count; halfkey_no++) {
		memset(halfkey, 0, halfkey_len);

		for (iter_no = 1; iter_no <= priv->iter_count; iter_no++) {
			int i;

			purple_cipher_reset_state(hash);

			if (iter_no == 1) {
				salt_ext[salt_ext_len - 4] =
					(halfkey_no & 0xff000000) >> 24;
				salt_ext[salt_ext_len - 3] =
					(halfkey_no & 0x00ff0000) >> 16;
				salt_ext[salt_ext_len - 2] =
					(halfkey_no & 0x0000ff00) >> 8;
				salt_ext[salt_ext_len - 1] =
					(halfkey_no & 0x000000ff) >> 0;

				purple_cipher_append(hash, salt_ext,
					salt_ext_len);
			}
			else
				purple_cipher_append(hash, halfkey_hash,
					halfkey_len);

			if (!purple_cipher_digest(hash, halfkey_hash,
				halfkey_len)) {
				purple_debug_error("pbkdf2",
					"Couldn't retrieve a digest\n");
				g_free(salt_ext);
				g_object_unref(hash);
				return FALSE;
			}

			for (i = 0; i < halfkey_len; i++)
				halfkey[i] ^= halfkey_hash[i];
		}

		memcpy(digest + (halfkey_no - 1) * halfkey_len, halfkey,
			(halfkey_no == halfkey_count) ? halfkey_pad :
				halfkey_len);
	}

	g_free(salt_ext);
	g_object_unref(hash);

	return TRUE;
}

static const gchar*
purple_pbkdf2_cipher_get_name(PurpleCipher *cipher)
{
	return "pbkdf2";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_pbkdf2_cipher_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	PurplePBKDF2Cipher *cipher = PURPLE_PBKDF2_CIPHER(obj);
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	switch(param_id) {
		case PROP_HASH:
			g_value_set_object(value, purple_pbkdf2_cipher_get_hash(cipher));
			break;
		case PROP_ITER_COUNT:
			g_value_set_uint(value, priv->iter_count);
			break;
		case PROP_OUT_LEN:
			g_value_set_uint(value, priv->out_len);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_pbkdf2_cipher_set_property(GObject *obj, guint param_id,
								const GValue *value, GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	switch(param_id) {
		case PROP_HASH:
			purple_pbkdf2_cipher_set_hash(cipher, g_value_get_object(value));
			break;
		case PROP_ITER_COUNT:
			priv->iter_count = GPOINTER_TO_UINT(value);
			break;
		case PROP_OUT_LEN:
			priv->out_len = GPOINTER_TO_UINT(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_pbkdf2_cipher_finalize(GObject *obj)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);
	PurplePBKDF2CipherPrivate *priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	purple_pbkdf2_cipher_reset(cipher);

	if (priv->hash != NULL)
		g_object_unref(priv->hash);

	parent_class->finalize(obj);
}

static void
purple_pbkdf2_cipher_class_init(PurplePBKDF2CipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_pbkdf2_cipher_finalize;
	obj_class->get_property = purple_pbkdf2_cipher_get_property;
	obj_class->set_property = purple_pbkdf2_cipher_set_property;

	cipher_class->reset = purple_pbkdf2_cipher_reset;
	cipher_class->reset_state = purple_pbkdf2_cipher_reset_state;
	cipher_class->digest = purple_pbkdf2_cipher_digest;
	cipher_class->get_digest_size = purple_pbkdf2_cipher_get_digest_size;
	cipher_class->set_salt = purple_pbkdf2_cipher_set_salt;
	cipher_class->set_key = purple_pbkdf2_cipher_set_key;
	cipher_class->get_name = purple_pbkdf2_cipher_get_name;

	pspec = g_param_spec_object("hash", "hash", "hash", PURPLE_TYPE_CIPHER,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(obj_class, PROP_HASH, pspec);

	pspec = g_param_spec_uint("iter_count", "iter_count", "iter_count", 0,
								G_MAXUINT, 0, G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_ITER_COUNT, pspec);

	pspec = g_param_spec_uint("out_len", "out_len", "out_len", 0,
								G_MAXUINT, 0, G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_OUT_LEN, pspec);

	g_type_class_add_private(klass, sizeof(PurplePBKDF2CipherPrivate));
}

static void
purple_pbkdf2_cipher_init(PurpleCipher *cipher)
{
	purple_cipher_reset(cipher);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_pbkdf2_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurplePBKDF2CipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_pbkdf2_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurplePBKDF2Cipher),
			0,
			(GInstanceInitFunc)purple_pbkdf2_cipher_init,
			NULL
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurplePBKDF2Cipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_pbkdf2_cipher_new(PurpleHash *hash) {
	g_return_val_if_fail(PURPLE_IS_HASH(hash), NULL);

	return g_object_new(PURPLE_TYPE_PBKDF2_CIPHER,
						"hash", hash,
						NULL);
}

PurpleHash *
purple_pbkdf2_cipher_get_hash(const PurplePBKDF2Cipher *cipher) {
	PurplePBKDF2CipherPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PBKDF2_CIPHER(cipher), NULL);

	priv = PURPLE_PBKDF2_CIPHER_GET_PRIVATE(cipher);

	if(priv && priv->hash)
		return priv->hash;

	return NULL;
}
