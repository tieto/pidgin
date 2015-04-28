/*
 * An example plugin that demonstrates exporting of a cipher object
 * type to be used in another plugin.
 *
 * This plugin only provides the CaesarCipher type. See caesarcipher_consumer
 * plugin for its use.
 *
 * Copyright (C) 2013, Ankit Vani <a@nevitus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

/* When writing a third-party plugin, do not include libpurple's internal.h
 * included below. This file is for internal libpurple use only. We're including
 * it here for our own convenience. */
#include "internal.h"

/* This file defines PURPLE_PLUGINS and includes all the libpurple headers */
#include <purple.h>

#include "caesarcipher.h"

#define CAESAR_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), CAESAR_TYPE_CIPHER, CaesarCipherPrivate))

typedef struct {
	gint8 offset;
} CaesarCipherPrivate;

enum {
	PROP_NONE,
	PROP_OFFSET,
	PROP_LAST,
};

/******************************************************************************
 * Cipher stuff
 *****************************************************************************/
static void
caesar_shift(const guchar input[], size_t in_len, guchar output[], gint8 offset)
{
	size_t i;

	for (i = 0; i < in_len; ++i) {
		if (input[i] >= 'a' && input[i] <= 'z')
			output[i] = (((input[i] - 'a') + offset + 26) % 26) + 'a';
		else if (input[i] >= 'A' && input[i] <= 'Z')
			output[i] = (((input[i] - 'A') + offset + 26) % 26) + 'A';
		else
			output[i] = input[i];
	}

	output[i] = '\0';
}

static void
caesar_cipher_set_offset(PurpleCipher *cipher, gint8 offset)
{
	CaesarCipherPrivate *priv = CAESAR_CIPHER_GET_PRIVATE(cipher);

	priv->offset = offset % 26;
}

static ssize_t
caesar_cipher_encrypt(PurpleCipher *cipher, const guchar input[], size_t in_len,
		guchar output[], size_t out_size)
{
	CaesarCipherPrivate *priv = CAESAR_CIPHER_GET_PRIVATE(cipher);

	g_return_val_if_fail(out_size > in_len, -1);

	caesar_shift(input, in_len, output, priv->offset);

	return in_len;
}

static ssize_t
caesar_cipher_decrypt(PurpleCipher *cipher, const guchar input[], size_t in_len,
		guchar output[], size_t out_size)
{
	CaesarCipherPrivate *priv = CAESAR_CIPHER_GET_PRIVATE(cipher);

	g_return_val_if_fail(out_size > in_len, -1);

	caesar_shift(input, in_len, output, -priv->offset);

	return in_len;
}

static void
caesar_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len)
{
	caesar_cipher_set_offset(cipher, len);
}

/******************************************************************************
 * Object stuff
 *****************************************************************************/
static void
caesar_cipher_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_OFFSET:
#if GLIB_CHECK_VERSION(2, 32, 0)
			caesar_cipher_set_offset(cipher, g_value_get_schar(value));
#else
			caesar_cipher_set_offset(cipher, g_value_get_char(value));
#endif
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* initialize the cipher object. used in PURPLE_DEFINE_TYPE. */
static void
caesar_cipher_init(CaesarCipher *cipher)
{
	/* classic caesar cipher uses a shift of 3 */
	CAESAR_CIPHER_GET_PRIVATE(cipher)->offset = 3;
}

/* initialize the cipher class. used in PURPLE_DEFINE_TYPE. */
static void
caesar_cipher_class_init(PurpleCipherClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->set_property = caesar_cipher_set_property;
	g_type_class_add_private(obj_class, sizeof(CaesarCipherPrivate));

	klass->encrypt  = caesar_cipher_encrypt;
	klass->decrypt  = caesar_cipher_decrypt;
	klass->set_key  = caesar_cipher_set_key;

	g_object_class_install_property(obj_class, PROP_OFFSET,
		g_param_spec_char("offset", "offset",
			"The offset by which to shift alphabets",
			-25, 25, 3,
			G_PARAM_WRITABLE)
	);
}

/*
 * define the CaesarCipher type. this function defines
 * caesar_cipher_get_type() and caesar_cipher_register_type().
 */
PURPLE_DEFINE_TYPE(CaesarCipher, caesar_cipher, PURPLE_TYPE_CIPHER);

G_MODULE_EXPORT PurpleCipher *
caesar_cipher_new(void)
{
	return g_object_new(CAESAR_TYPE_CIPHER, NULL);
}

/******************************************************************************
 * Plugin stuff
 *****************************************************************************/
static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Ankit Vani <a@nevitus.org>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           "core-caesarcipher",
		"name",         N_("Caesar Cipher Provider Example"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Example"),
		"summary",      N_("An example plugin that demonstrates exporting of a "
		                   "cipher type."),
		"description",  N_("An example plugin that demonstrates exporting of "
		                   "a cipher object type to be used in another "
		                   "plugin."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	/* register the CaesarCipher type in the type system */
	caesar_cipher_register_type(plugin);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(caesarcipher, plugin_query, plugin_load, plugin_unload);
