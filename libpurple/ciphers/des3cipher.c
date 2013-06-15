/*
 * Original des taken from gpg
 *
 * des.c - Triple-DES encryption/decryption Algorithm
 *  Copyright (C) 1998 Free Software Foundation, Inc.
 *
 *  Please see below for more legal information!
 *  
 *   According to the definition of DES in FIPS PUB 46-2 from December 1993.
 *   For a description of triple encryption, see:
 *     Bruce Schneier: Applied Cryptography. Second Edition.
 *     John Wiley & Sons, 1996. ISBN 0-471-12845-7. Pages 358 ff.
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
#include "des3cipher.h"
#include "descipher.h"

#include <string.h>

/******************************************************************************
 * Structs
 *****************************************************************************/

#define PURPLE_DES3_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_DES3_CIPHER, PurpleDES3CipherPrivate))

typedef struct _PurpleDES3CipherPrivate		PurpleDES3CipherPrivate;
struct _PurpleDES3CipherPrivate
{
	PurpleCipherBatchMode mode;
	guchar iv[8];
	/* First key for encryption */
	PurpleCipher *key1;
	/* Second key for decryption */
	PurpleCipher *key2;
	/* Third key for encryption */
	PurpleCipher *key3;
};

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_NONE,
	PROP_BATCH_MODE,
	PROP_IV,
	PROP_KEY,
	PROP_LAST,
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Cipher Stuff
 *****************************************************************************/

static size_t
purple_des3_cipher_get_key_size(PurpleCipher *cipher)
{
	return 24;
}

/*
 *  Fill a DES3 context with subkeys calculated from 3 64bit key.
 *  Does not check parity bits, but simply ignore them.
 *  Does not check for weak keys.
 **/
static void
purple_des3_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	g_return_if_fail(len == 24);

	purple_cipher_set_key(PURPLE_CIPHER(priv->key1), key +  0,
					purple_cipher_get_key_size(PURPLE_CIPHER(priv->key1)));
	purple_cipher_set_key(PURPLE_CIPHER(priv->key2), key +  8,
					purple_cipher_get_key_size(PURPLE_CIPHER(priv->key2)));
	purple_cipher_set_key(PURPLE_CIPHER(priv->key3), key + 16,
					purple_cipher_get_key_size(PURPLE_CIPHER(priv->key3)));

	g_object_notify(G_OBJECT(des3_cipher), "key");
}

static ssize_t
purple_des3_cipher_ecb_encrypt(PurpleDES3Cipher *des3_cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	ssize_t out_len;

	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	while (offset + 8 <= in_len) {
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									input + offset, output + offset, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 0);

		offset += 8;
	}

	out_len = in_len;
	if (offset < in_len) {
		out_len += in_len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		while (tmp < in_len) {
			buf[i++] = input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 0);
	}

	return out_len;
}

static ssize_t
purple_des3_cipher_cbc_encrypt(PurpleDES3Cipher *des3_cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8];
	ssize_t out_len;
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	memcpy(buf, priv->iv, 8);

	while (offset + 8 <= in_len) {
		for (i = 0; i < 8; i++)
			buf[i] ^= input[offset + i];

		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 0);

		memcpy(buf, output+offset, 8);
		offset += 8;
	}

	out_len = in_len;
	if (offset < in_len) {
		out_len += in_len - offset;
		tmp = offset;
		i = 0;
		while (tmp < in_len) {
			buf[i++] ^= input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 0);
	}

	return out_len;
}

static ssize_t
purple_des3_cipher_encrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	if (priv->mode == PURPLE_CIPHER_BATCH_MODE_ECB) {
		return purple_des3_cipher_ecb_encrypt(des3_cipher, input, in_len, output, out_size);
	} else if (priv->mode == PURPLE_CIPHER_BATCH_MODE_CBC) {
		return purple_des3_cipher_cbc_encrypt(des3_cipher, input, in_len, output, out_size);
	} else {
		g_return_val_if_reached(0);
	}

	return 0;
}

static ssize_t
purple_des3_cipher_ecb_decrypt(PurpleDES3Cipher *des3_cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	ssize_t out_len;
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	while (offset + 8 <= in_len) {
		/* NOTE: Apply key in reverse */
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									input + offset, output + offset, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 1);

		offset += 8;
	}

	out_len = in_len;
	if (offset < in_len) {
		out_len += in_len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		while (tmp < in_len) {
			buf[i++] = input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 1);
	}

	return out_len;
}

static ssize_t
purple_des3_cipher_cbc_decrypt(PurpleDES3Cipher *des3_cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	int offset = 0;
	int i = 0;
	int tmp;
	guint8 buf[8] = {0,0,0,0,0,0,0,0};
	guint8 link[8];
	ssize_t out_len;
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	memcpy(link, priv->iv, 8);
	while (offset + 8 <= in_len) {
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									input + offset, output + offset, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 1);

		for (i = 0; i < 8; i++)
			output[offset + i] ^= link[i];

		memcpy(link, input + offset, 8);

		offset+=8;
	}

	out_len = in_len;
	if(offset<in_len) {
		out_len += in_len - offset;
		tmp = offset;
		memset(buf, 0, 8);
		i = 0;
		while(tmp<in_len) {
			buf[i++] = input[tmp];
			tmp++;
		}

		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key3),
									buf, output + offset, 1);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key2),
									output + offset, buf, 0);
		purple_des_cipher_ecb_crypt(PURPLE_DES_CIPHER(priv->key1),
									buf, output + offset, 1);

		for (i = 0; i < 8; i++)
			output[offset + i] ^= link[i];
	}

	return out_len;
}

static ssize_t
purple_des3_cipher_decrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(cipher);

	if (priv->mode == PURPLE_CIPHER_BATCH_MODE_ECB) {
		return purple_des3_cipher_ecb_decrypt(des3_cipher, input, in_len, output, out_size);
	} else if (priv->mode == PURPLE_CIPHER_BATCH_MODE_CBC) {
		return purple_des3_cipher_cbc_decrypt(des3_cipher, input, in_len, output, out_size);
	} else {
		g_return_val_if_reached(0);
	}

	return 0;
}

static void
purple_des3_cipher_set_batch_mode(PurpleCipher *cipher, PurpleCipherBatchMode mode)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	priv->mode = mode;

	g_object_notify(G_OBJECT(des3_cipher), "batch_mode");
}

static PurpleCipherBatchMode
purple_des3_cipher_get_batch_mode(PurpleCipher *cipher)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	return priv->mode;
}

static void
purple_des3_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	g_return_if_fail(len == 8);

	memcpy(priv->iv, iv, len);

	g_object_notify(G_OBJECT(des3_cipher), "iv");
}

static const gchar*
purple_des3_cipher_get_name(PurpleCipher *cipher)
{
	return "des3";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_des3_cipher_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_BATCH_MODE:
			g_value_set_enum(value,
							 purple_cipher_get_batch_mode(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_des3_cipher_set_property(GObject *obj, guint param_id,
								const GValue *value, GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_BATCH_MODE:
			purple_cipher_set_batch_mode(cipher,
										 g_value_get_enum(value));
			break;
		case PROP_IV:
			{
				guchar *iv = (guchar *)g_value_get_string(value);
				purple_cipher_set_iv(cipher, iv, strlen((gchar*)iv));
			}
			break;
		case PROP_KEY:
			purple_cipher_set_key(cipher, (guchar *)g_value_get_string(value),
				purple_des3_cipher_get_key_size(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_des3_cipher_finalize(GObject *obj)
{
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(obj);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	g_object_unref(G_OBJECT(priv->key1));
	g_object_unref(G_OBJECT(priv->key2));
	g_object_unref(G_OBJECT(priv->key3));

	memset(priv->iv, 0, sizeof(priv->iv));

	parent_class->finalize(obj);
}

static void
purple_des3_cipher_class_init(PurpleDES3CipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_des3_cipher_finalize;
	obj_class->get_property = purple_des3_cipher_get_property;
	obj_class->set_property = purple_des3_cipher_set_property;

	cipher_class->set_iv = purple_des3_cipher_set_iv;
	cipher_class->encrypt = purple_des3_cipher_encrypt;
	cipher_class->decrypt = purple_des3_cipher_decrypt;
	cipher_class->set_key = purple_des3_cipher_set_key;
	cipher_class->set_batch_mode = purple_des3_cipher_set_batch_mode;
	cipher_class->get_batch_mode = purple_des3_cipher_get_batch_mode;
	cipher_class->get_key_size = purple_des3_cipher_get_key_size;
	cipher_class->get_name = purple_des3_cipher_get_name;

	pspec = g_param_spec_enum("batch_mode", "batch_mode", "batch_mode",
							  PURPLE_TYPE_CIPHER_BATCH_MODE, 0,
							  G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BATCH_MODE, pspec);

	pspec = g_param_spec_string("iv", "iv", "iv", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_IV, pspec);

	pspec = g_param_spec_string("key", "key", "key", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_KEY, pspec);

	g_type_class_add_private(klass, sizeof(PurpleDES3CipherPrivate));
}

static void
purple_des3_cipher_init(PurpleCipher *cipher) {
	PurpleDES3Cipher *des3_cipher = PURPLE_DES3_CIPHER(cipher);
	PurpleDES3CipherPrivate *priv = PURPLE_DES3_CIPHER_GET_PRIVATE(des3_cipher);

	priv->key1 = purple_des_cipher_new();
	priv->key2 = purple_des_cipher_new();
	priv->key3 = purple_des_cipher_new();
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_des3_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleDES3CipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_des3_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleDES3Cipher),
			0,
			(GInstanceInitFunc)purple_des3_cipher_init,
			NULL
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleDES3Cipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_des3_cipher_new(void) {
	return g_object_new(PURPLE_TYPE_DES3_CIPHER, NULL);
}
