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

#include "internal.h"
#include "cipher.h"
#include "ciphers.h"
#include "debug.h"

/* 1024bit */
#define PBKDF2_HASH_MAX_LEN 128

typedef struct
{
	gchar *hash_func;
	guint iter_count;
	size_t out_len;

	guchar *salt;
	size_t salt_len;
	guchar *passphrase;
	size_t passphrase_len;
} Pbkdf2Context;

static void
purple_pbkdf2_init(PurpleCipherContext *context, void *extra)
{
	Pbkdf2Context *ctx_data;

	ctx_data = g_new0(Pbkdf2Context, 1);
	purple_cipher_context_set_data(context, ctx_data);

	purple_cipher_context_reset(context, extra);
}

static void
purple_pbkdf2_uninit(PurpleCipherContext *context)
{
	Pbkdf2Context *ctx_data;

	purple_cipher_context_reset(context, NULL);

	ctx_data = purple_cipher_context_get_data(context);
	g_free(ctx_data);
	purple_cipher_context_set_data(context, NULL);
}

static void
purple_pbkdf2_reset(PurpleCipherContext *context, void *extra)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	g_free(ctx_data->hash_func);
	ctx_data->hash_func = NULL;
	ctx_data->iter_count = 1;
	ctx_data->out_len = 256;

	purple_cipher_context_reset_state(context, extra);
}

static void
purple_pbkdf2_reset_state(PurpleCipherContext *context, void *extra)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	purple_cipher_context_set_salt(context, NULL, 0);
	purple_cipher_context_set_key(context, NULL, 0);
}

static void
purple_pbkdf2_set_option(PurpleCipherContext *context, const gchar *name,
	void *value)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	if (g_strcmp0(name, "hash") == 0) {
		g_free(ctx_data->hash_func);
		ctx_data->hash_func = g_strdup(value);
		return;
	}

	if (g_strcmp0(name, "iter_count") == 0) {
		ctx_data->iter_count = GPOINTER_TO_UINT(value);
		return;
	}

	if (g_strcmp0(name, "out_len") == 0) {
		ctx_data->out_len = GPOINTER_TO_UINT(value);
		return;
	}

	purple_debug_warning("pbkdf2", "Unknown option: %s\n",
		name ? name : "(null)");
}

static void *
purple_pbkdf2_get_option(PurpleCipherContext *context, const gchar *name)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_val_if_fail(ctx_data != NULL, NULL);

	if (g_strcmp0(name, "hash") == 0)
		return ctx_data->hash_func;

	if (g_strcmp0(name, "iter_count") == 0)
		return GUINT_TO_POINTER(ctx_data->iter_count);

	if (g_strcmp0(name, "out_len") == 0)
		return GUINT_TO_POINTER(ctx_data->out_len);

	purple_debug_warning("pbkdf2", "Unknown option: %s\n",
		name ? name : "(null)");
	return NULL;
}

static size_t
purple_pbkdf2_get_digest_size(PurpleCipherContext *context)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_val_if_fail(ctx_data != NULL, 0);

	return ctx_data->out_len;
}

static void
purple_pbkdf2_set_salt(PurpleCipherContext *context, const guchar *salt, size_t len)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	g_free(ctx_data->salt);
	ctx_data->salt = NULL;
	ctx_data->salt_len = 0;

	if (len == 0)
		return;
	g_return_if_fail(salt != NULL);

	ctx_data->salt = g_memdup(salt, len);
	ctx_data->salt_len = len;
}

static void
purple_pbkdf2_set_key(PurpleCipherContext *context, const guchar *key,
	size_t len)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	if (ctx_data->passphrase != NULL) {
		memset(ctx_data->passphrase, 0, ctx_data->passphrase_len);
		g_free(ctx_data->passphrase);
		ctx_data->passphrase = NULL;
	}
	ctx_data->passphrase_len = 0;

	if (len == 0)
		return;
	g_return_if_fail(key != NULL);

	ctx_data->passphrase = g_memdup(key, len);
	ctx_data->passphrase_len = len;
}

/* inspired by gnutls 3.1.10, pbkdf2-sha1.c */
static gboolean
purple_pbkdf2_digest(PurpleCipherContext *context, guchar digest[], size_t len)
{
	Pbkdf2Context *ctx_data = purple_cipher_context_get_data(context);
	guchar halfkey[PBKDF2_HASH_MAX_LEN], halfkey_hash[PBKDF2_HASH_MAX_LEN];
	guint halfkey_len, halfkey_count, halfkey_pad, halfkey_no;
	guchar *salt_ext;
	size_t salt_ext_len;
	guint iter_no;
	PurpleCipherContext *hash;

	g_return_val_if_fail(ctx_data != NULL, FALSE);
	g_return_val_if_fail(digest != NULL, FALSE);
	g_return_val_if_fail(len >= ctx_data->out_len, FALSE);

	g_return_val_if_fail(ctx_data->hash_func != NULL, FALSE);
	g_return_val_if_fail(ctx_data->iter_count > 0, FALSE);
	g_return_val_if_fail(ctx_data->passphrase != NULL ||
		ctx_data->passphrase_len == 0, FALSE);
	g_return_val_if_fail(ctx_data->salt != NULL || ctx_data->salt_len == 0,
		FALSE);
	g_return_val_if_fail(ctx_data->out_len > 0, FALSE);
	g_return_val_if_fail(ctx_data->out_len < 0xFFFFFFFFU, FALSE);

	salt_ext_len = ctx_data->salt_len + 4;

	hash = purple_cipher_context_new_by_name("hmac", NULL);
	if (hash == NULL) {
		purple_debug_error("pbkdf2", "Couldn't create new hmac "
			"context\n");
		return FALSE;
	}
	purple_cipher_context_set_option(hash, "hash",
		(void*)ctx_data->hash_func);
	purple_cipher_context_set_key(hash, (const guchar*)ctx_data->passphrase,
		ctx_data->passphrase_len);

	halfkey_len = purple_cipher_context_get_digest_size(hash);
	if (halfkey_len <= 0 || halfkey_len > PBKDF2_HASH_MAX_LEN) {
		purple_debug_error("pbkdf2", "Unsupported hash function: %s "
			"(digest size: %d)\n",
			ctx_data->hash_func ? ctx_data->hash_func : "(null)",
			halfkey_len);
		return FALSE;
	}

	halfkey_count = ((ctx_data->out_len - 1) / halfkey_len) + 1;
	halfkey_pad = ctx_data->out_len - (halfkey_count - 1) * halfkey_len;

	salt_ext = g_new(guchar, salt_ext_len);
	memcpy(salt_ext, ctx_data->salt, ctx_data->salt_len);

	for (halfkey_no = 1; halfkey_no <= halfkey_count; halfkey_no++) {
		memset(halfkey, 0, halfkey_len);

		for (iter_no = 1; iter_no <= ctx_data->iter_count; iter_no++) {
			guint i;

			purple_cipher_context_reset_state(hash, NULL);

			if (iter_no == 1) {
				salt_ext[salt_ext_len - 4] =
					(halfkey_no & 0xff000000) >> 24;
				salt_ext[salt_ext_len - 3] =
					(halfkey_no & 0x00ff0000) >> 16;
				salt_ext[salt_ext_len - 2] =
					(halfkey_no & 0x0000ff00) >> 8;
				salt_ext[salt_ext_len - 1] =
					(halfkey_no & 0x000000ff) >> 0;

				purple_cipher_context_append(hash, salt_ext,
					salt_ext_len);
			}
			else
				purple_cipher_context_append(hash, halfkey_hash,
					halfkey_len);

			if (!purple_cipher_context_digest(hash, halfkey_hash,
				halfkey_len)) {
				purple_debug_error("pbkdf2",
					"Couldn't retrieve a digest\n");
				g_free(salt_ext);
				purple_cipher_context_destroy(hash);
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
	purple_cipher_context_destroy(hash);

	return TRUE;
}

static PurpleCipherOps PBKDF2Ops = {
	purple_pbkdf2_set_option,      /* set_option */
	purple_pbkdf2_get_option,      /* get_option */
	purple_pbkdf2_init,            /* init */
	purple_pbkdf2_reset,           /* reset */
	purple_pbkdf2_reset_state,     /* reset_state */
	purple_pbkdf2_uninit,          /* uninit */
	NULL,                          /* set_iv */
	NULL,                          /* append */
	purple_pbkdf2_digest,          /* digest */
	purple_pbkdf2_get_digest_size, /* get_digest_size */
	NULL,                          /* encrypt */
	NULL,                          /* decrypt */
	purple_pbkdf2_set_salt,        /* set_salt */
	NULL,                          /* get_salt_size */
	purple_pbkdf2_set_key,         /* set_key */
	NULL,                          /* get_key_size */
	NULL,                          /* set_batch_mode */
	NULL,                          /* get_batch_mode */
	NULL,                          /* get_block_size */
	NULL, NULL, NULL, NULL         /* reserved */
};

PurpleCipherOps *
purple_pbkdf2_cipher_get_ops(void) {
	return &PBKDF2Ops;
}
