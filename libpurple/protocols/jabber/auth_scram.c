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


GString *jabber_auth_scram_hi(const gchar *hash, const GString *str,
                              GString *salt, guint iterations)
{
	PurpleCipherContext *context;
	GString *result;
	guint i;

	g_return_val_if_fail(hash != NULL, NULL);
	g_return_val_if_fail(str != NULL && str->len > 0, NULL);
	g_return_val_if_fail(salt != NULL && salt->len > 0, NULL);
	g_return_val_if_fail(iterations > 0, NULL);

	context = purple_cipher_context_new_by_name("hmac", NULL);

	/* Append INT(1), a four-octet encoding of the integer 1, most significant
	 * octet first. */
	g_string_append_len(salt, "\0\0\0\1", 4);

	result = g_string_sized_new(20); /* FIXME: Hardcoded 20 */

	/* Compute U0 */
	purple_cipher_context_set_option(context, "hash", (gpointer)hash);
	purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
	purple_cipher_context_append(context, (guchar *)salt->str, salt->len);
	purple_cipher_context_digest(context, result->allocated_len, (guchar *)result->str, &(result->len));

	/* Compute U1...Ui */
	for (i = 1; i < iterations; ++i) {
		guchar tmp[20]; /* FIXME: hardcoded 20 */
		guint j;
		purple_cipher_context_set_option(context, "hash", (gpointer)hash);
		purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
		purple_cipher_context_append(context, (guchar *)result->str, result->len);
		purple_cipher_context_digest(context, sizeof(tmp), tmp, NULL);

		for (j = 0; j < 20; ++j)
			result->str[j] ^= tmp[j];
	}

	purple_cipher_context_destroy(context);
	return result;
}
