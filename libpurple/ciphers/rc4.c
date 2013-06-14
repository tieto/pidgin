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
#include "rc4.h"

/*******************************************************************************
 * Structs
 ******************************************************************************/
#define PURPLE_RC4_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_RC4_CIPHER, PurpleRC4CipherPrivate))

typedef struct {
  guchar state[256];
  guchar x;
  guchar y;
  gint key_len;
} PurpleRC4CipherPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_ZERO,
	PROP_KEY_LEN,
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
static void
purple_rc4_cipher_reset(PurpleCipher *cipher) {
	PurpleRC4Cipher *rc4_cipher = PURPLE_RC4_CIPHER(cipher);
	PurpleRC4CipherPrivate *priv = PURPLE_RC4_CIPHER_GET_PRIVATE(rc4_cipher);
	guint i;

	for(i = 0; i < 256; i++)
		priv->state[i] = i;
	priv->x = 0;
	priv->y = 0;

	/* default is 5 bytes (40bit key) */
	priv->key_len = 5;
}

static void
purple_rc4_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len) {
	PurpleRC4Cipher *rc4_cipher = PURPLE_RC4_CIPHER(cipher);
	PurpleRC4CipherPrivate *priv = PURPLE_RC4_CIPHER_GET_PRIVATE(rc4_cipher);
	guchar *state;
	guchar temp_swap;
	guchar x, y;
	guint i;

	x = 0;
	y = 0;
	state = &priv->state[0];
	priv->key_len = len;
	for(i = 0; i < 256; i++)
	{
		y = (key[x] + state[i] + y) % 256;
		temp_swap = state[i];
		state[i] = state[y];
		state[y] = temp_swap;
		x = (x + 1) % len;
	}

	g_object_notify(G_OBJECT(rc4_cipher), "key");
}

static ssize_t
purple_rc4_cipher_encrypt(PurpleCipher *cipher, const guchar input[], size_t in_len,
							guchar output[], size_t out_size)
{
	PurpleRC4Cipher *rc4_cipher = PURPLE_RC4_CIPHER(cipher);
	guchar temp_swap;
	guchar x, y, z;
	guchar *state;
	guint i;
	PurpleRC4CipherPrivate *priv = PURPLE_RC4_CIPHER_GET_PRIVATE(rc4_cipher);

	x = priv->x;
	y = priv->y;
	state = &priv->state[0];

	for(i = 0; i < in_len; i++)
	{
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		temp_swap = state[x];
		state[x] = state[y];
		state[y] = temp_swap;
		z = state[x] + (state[y]) % 256;
		output[i] = input[i] ^ state[z];
	}
	priv->x = x;
	priv->y = y;

	return in_len;
}

static const gchar*
purple_rc4_cipher_get_name(PurpleCipher *cipher)
{
	return "rc4";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_rc4_cipher_set_property(GObject *obj, guint param_id,
							   const GValue *value, GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);
	PurpleRC4Cipher *rc4_cipher = PURPLE_RC4_CIPHER(obj);

	switch(param_id) {
		case PROP_KEY_LEN:
			purple_rc4_cipher_set_key_len(rc4_cipher, g_value_get_int(value));
			break;
		case PROP_KEY:
			{
				guchar *key = (guchar *)g_value_get_string(value);
				purple_rc4_cipher_set_key(cipher, key, strlen((gchar *) key));
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_rc4_cipher_get_property(GObject *obj, guint param_id, GValue *value,
							   GParamSpec *pspec)
{
	PurpleRC4Cipher *rc4_cipher = PURPLE_RC4_CIPHER(obj);

	switch(param_id) {
		case PROP_KEY_LEN:
			g_value_set_int(value,
							purple_rc4_cipher_get_key_len(rc4_cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_rc4_cipher_class_init(PurpleRC4CipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec = NULL;

	parent_class = g_type_class_peek_parent(klass);

	obj_class->set_property = purple_rc4_cipher_set_property;
	obj_class->get_property = purple_rc4_cipher_get_property;

	cipher_class->reset = purple_rc4_cipher_reset;
	cipher_class->encrypt = purple_rc4_cipher_encrypt;
	cipher_class->set_key = purple_rc4_cipher_set_key;
	cipher_class->get_name = purple_rc4_cipher_get_name;

	pspec = g_param_spec_int("key_len", "key_len", "key_len",
							 G_MININT, G_MAXINT, 0,
							 G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_KEY_LEN, pspec);

	pspec = g_param_spec_string("key", "key", "key", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_KEY, pspec);

	g_type_class_add_private(klass, sizeof(PurpleRC4CipherPrivate));
}

static void
purple_rc4_cipher_init(PurpleCipher *cipher) {
	purple_rc4_cipher_reset(cipher);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_rc4_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleRC4CipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_rc4_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleRC4Cipher),
			0,
			(GInstanceInitFunc)purple_rc4_cipher_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleRC4Cipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_rc4_cipher_new(void) {
	return g_object_new(PURPLE_TYPE_RC4_CIPHER, NULL);
}

void
purple_rc4_cipher_set_key_len(PurpleRC4Cipher *rc4_cipher,
							  gint key_len)
{
	PurpleRC4CipherPrivate *priv;

	g_return_if_fail(PURPLE_IS_RC4_CIPHER(rc4_cipher));

	priv = PURPLE_RC4_CIPHER_GET_PRIVATE(rc4_cipher);
	priv->key_len = key_len;

	g_object_notify(G_OBJECT(rc4_cipher), "key_len");
}

gint
purple_rc4_cipher_get_key_len(PurpleRC4Cipher *rc4_cipher)
{
	PurpleRC4CipherPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_RC4_CIPHER(rc4_cipher), 0);

	priv = PURPLE_RC4_CIPHER_GET_PRIVATE(rc4_cipher);

	return priv->key_len;
}
