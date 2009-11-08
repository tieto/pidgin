/*
 * purple - Jabber Protocol Plugin
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
 *
 */
#include "internal.h"

#include "auth.h"
#include "auth_scram.h"

#include "cipher.h"
#include "debug.h"

static const struct {
	const char *hash;
	guint size;
} hash_sizes[] = {
	{ "sha1", 20 },
	{ "sha224", 28 },
	{ "sha256", 32 },
	{ "sha384", 48 },
	{ "sha512", 64 }
};

static guint hash_to_output_len(const gchar *hash)
{
	int i;

	g_return_val_if_fail(hash != NULL && *hash != '\0', 0);

	for (i = 0; i < G_N_ELEMENTS(hash_sizes); ++i) {
		if (g_str_equal(hash, hash_sizes[i].hash))
			return hash_sizes[i].size;
	}

	purple_debug_error("jabber", "Unknown SCRAM hash function %s\n", hash);

	return 0;
}

GString *jabber_auth_scram_hi(const gchar *hash, const GString *str,
                              GString *salt, guint iterations)
{
	PurpleCipherContext *context;
	GString *result;
	guint i;
	guint hash_len;
	guchar *prev, *tmp;

	g_return_val_if_fail(hash != NULL, NULL);
	g_return_val_if_fail(str != NULL && str->len > 0, NULL);
	g_return_val_if_fail(salt != NULL && salt->len > 0, NULL);
	g_return_val_if_fail(iterations > 0, NULL);

	hash_len = hash_to_output_len(hash);
	g_return_val_if_fail(hash_len > 0, NULL);

	prev = g_new0(guint8, hash_len);
	tmp = g_new0(guint8, hash_len);

	context = purple_cipher_context_new_by_name("hmac", NULL);

	/* Append INT(1), a four-octet encoding of the integer 1, most significant
	 * octet first. */
	g_string_append_len(salt, "\0\0\0\1", 4);

	result = g_string_sized_new(hash_len);

	/* Compute U0 */
	purple_cipher_context_set_option(context, "hash", (gpointer)hash);
	purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
	purple_cipher_context_append(context, (guchar *)salt->str, salt->len);
	purple_cipher_context_digest(context, hash_len, (guchar *)result->str, &(result->len));

	memcpy(prev, result->str, hash_len);

	/* Compute U1...Ui */
	for (i = 1; i < iterations; ++i) {
		guint j;
		purple_cipher_context_set_option(context, "hash", (gpointer)hash);
		purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
		purple_cipher_context_append(context, prev, hash_len);
		purple_cipher_context_digest(context, hash_len, tmp, NULL);

		for (j = 0; j < hash_len; ++j)
			result->str[j] ^= tmp[j];

		memcpy(prev, tmp, hash_len);
	}

	purple_cipher_context_destroy(context);
	g_free(tmp);
	g_free(prev);
	return result;
}
