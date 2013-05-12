/**
 * @file internalkeyring.c internal keyring
 * @ingroup plugins
 */

/* purple
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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "account.h"
#include "cipher.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#define INTKEYRING_NAME N_("Internal keyring")
#define INTKEYRING_DESCRIPTION N_("This plugin provides the default password " \
	"storage behaviour for libpurple.")
#define INTKEYRING_AUTHOR      "Tomek Wasilczyk (tomkiewicz@cpw.pidgin.im)"
#define INTKEYRING_ID          PURPLE_DEFAULT_KEYRING

#define INTKEYRING_VERIFY_STR "[verification-string]"
#define INTKEYRING_PBKDF2_ITERATIONS 10000
#define INTKEYRING_PBKDF2_ITERATIONS_MIN 1000
#define INTKEYRING_PBKDF2_ITERATIONS_MAX 1000000000
#define INTKEYRING_KEY_LEN (256/8)
#define INTKEYRING_ENCRYPT_BUFF_LEN 1000

#define INTKEYRING_PREFS "/plugins/keyrings/internal/"

typedef struct
{
	guchar *data;
	size_t len;
} intkeyring_buff_t;

static gboolean intkeyring_opened = FALSE;
static GHashTable *intkeyring_passwords = NULL;
static PurpleKeyring *keyring_handler = NULL;

static intkeyring_buff_t *
intkeyring_buff_new(guchar *data, size_t len)
{
	intkeyring_buff_t *ret = g_new(intkeyring_buff_t, 1);

	ret->data = data;
	ret->len = len;

	return ret;
}

static void
intkeyring_buff_free(intkeyring_buff_t *buff)
{
	memset(buff->data, 0, buff->len);
	g_free(buff->data);
	g_free(buff);
}

static intkeyring_buff_t *
intkeyring_derive_key(const gchar *passphrase, intkeyring_buff_t *salt)
{
	PurpleCipherContext *context;
	gboolean succ;
	intkeyring_buff_t *ret;

	g_return_val_if_fail(passphrase != NULL, NULL);

	context = purple_cipher_context_new_by_name("pbkdf2", NULL);
	g_return_val_if_fail(context != NULL, NULL);

	purple_cipher_context_set_option(context, "hash", "sha256");
	purple_cipher_context_set_option(context, "iter_count",
		GUINT_TO_POINTER(purple_prefs_get_int(INTKEYRING_PREFS
		"pbkdf2_iterations")));
	purple_cipher_context_set_option(context, "out_len", GUINT_TO_POINTER(
		INTKEYRING_KEY_LEN));
	purple_cipher_context_set_salt(context, salt->data, salt->len);
	purple_cipher_context_set_key(context, (const guchar*)passphrase,
		strlen(passphrase));

	ret = intkeyring_buff_new(g_new(guchar, INTKEYRING_KEY_LEN),
		INTKEYRING_KEY_LEN);
	succ = purple_cipher_context_digest(context, ret->data, ret->len);

	purple_cipher_context_destroy(context);

	if (!succ) {
		intkeyring_buff_free(ret);
		return NULL;
	}

	return ret;
}

static intkeyring_buff_t *
intkeyring_gen_salt(size_t len)
{
	intkeyring_buff_t *ret;
	size_t filled = 0;

	g_return_val_if_fail(len > 0, NULL);

	ret = intkeyring_buff_new(g_new(guchar, len), len);

	while (filled < len) {
		guint32 r = g_random_int();
		int i;

		for (i = 0; i < 4; i++) {
			ret->data[filled++] = r & 0xFF;
			if (filled >= len)
				break;
			r >>= 8;
		}
	}

	return ret;
}

/* TODO: describe encrypted contents structure */
static gchar *
intkeyring_encrypt(intkeyring_buff_t *key, const gchar *str)
{
	PurpleCipherContext *context;
	intkeyring_buff_t *iv;
	guchar plaintext[INTKEYRING_ENCRYPT_BUFF_LEN];
	size_t plaintext_len, text_len, verify_len;
	guchar encrypted_raw[INTKEYRING_ENCRYPT_BUFF_LEN];
	ssize_t encrypted_size;

	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(str != NULL, NULL);

	text_len = strlen(str);
	verify_len = strlen(INTKEYRING_VERIFY_STR);
	g_return_val_if_fail(text_len + verify_len <= sizeof(plaintext), NULL);

	context = purple_cipher_context_new_by_name("aes", NULL);
	g_return_val_if_fail(context != NULL, NULL);

	memcpy(plaintext, str, text_len);
	memcpy(plaintext + text_len, INTKEYRING_VERIFY_STR, verify_len);
	plaintext_len = text_len + verify_len;

	iv = intkeyring_gen_salt(purple_cipher_context_get_block_size(context));
	purple_cipher_context_set_iv(context, iv->data, iv->len);
	purple_cipher_context_set_key(context, key->data, key->len);
	purple_cipher_context_set_batch_mode(context,
		PURPLE_CIPHER_BATCH_MODE_CBC);

	memcpy(encrypted_raw, iv->data, iv->len);

	encrypted_size = purple_cipher_context_encrypt(context,
		plaintext, plaintext_len, encrypted_raw + iv->len,
		sizeof(encrypted_raw) - iv->len);
	encrypted_size += iv->len;

	memset(plaintext, 0, plaintext_len);
	intkeyring_buff_free(iv);
	purple_cipher_context_destroy(context);

	if (encrypted_size < 0)
		return NULL;

	return purple_base64_encode(encrypted_raw, encrypted_size);

}

static gchar *
intkeyring_decrypt(intkeyring_buff_t *key, const gchar *str)
{
	PurpleCipherContext *context;
	guchar *encrypted_raw;
	gsize encrypted_size;
	size_t iv_len, verify_len, text_len;
	guchar plaintext[INTKEYRING_ENCRYPT_BUFF_LEN];
	ssize_t plaintext_len;
	gchar *ret;

	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(str != NULL, NULL);

	context = purple_cipher_context_new_by_name("aes", NULL);
	g_return_val_if_fail(context != NULL, NULL);

	encrypted_raw = purple_base64_decode(str, &encrypted_size);
	g_return_val_if_fail(encrypted_raw != NULL, NULL);

	iv_len = purple_cipher_context_get_block_size(context);
	if (encrypted_size < iv_len) {
		g_free(encrypted_raw);
		return NULL;
	}

	purple_cipher_context_set_iv(context, encrypted_raw, iv_len);
	purple_cipher_context_set_key(context, key->data, key->len);
	purple_cipher_context_set_batch_mode(context,
		PURPLE_CIPHER_BATCH_MODE_CBC);

	plaintext_len = purple_cipher_context_decrypt(context,
		encrypted_raw + iv_len, encrypted_size - iv_len,
		plaintext, sizeof(plaintext));

	g_free(encrypted_raw);
	purple_cipher_context_destroy(context);

	verify_len = strlen(INTKEYRING_VERIFY_STR);
	if (plaintext_len < verify_len || strncmp(
		(gchar*)plaintext + plaintext_len - verify_len,
		INTKEYRING_VERIFY_STR, verify_len) != 0) {
		purple_debug_warning("keyring-internal",
			"Verification failed on decryption\n");
		memset(plaintext, 0, sizeof(plaintext));
		return NULL;
	}

	text_len = plaintext_len - verify_len;
	ret = g_new(gchar, text_len + 1);
	memcpy(ret, plaintext, text_len);
	memset(plaintext, 0, plaintext_len);
	ret[text_len] = '\0';

	return ret;
}

static void
intkeyring_change_master_password(const gchar *new_password)
{
	intkeyring_buff_t *salt, *key;
	gchar *verifier, *test;

	salt = intkeyring_gen_salt(32);
	key = intkeyring_derive_key(new_password, salt);

	/* In fact, verify str will be concatenated twice before encryption
	 * (it's used as a suffix in encryption routine), but it's not
	 * a problem.
	 */
	verifier = intkeyring_encrypt(key, INTKEYRING_VERIFY_STR);
	purple_debug_info("test-tmp", "verifier=[%s]\n", verifier);

	test = intkeyring_decrypt(key, verifier);
	purple_debug_info("test-tmp", "test=[%s]\n", test);

	g_free(test);
	g_free(verifier);
}

static void
intkeyring_open(void)
{
	if (intkeyring_opened)
		return;
	intkeyring_opened = TRUE;

	intkeyring_passwords = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify)purple_str_wipe);
}

static void
intkeyring_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	const char *password;
	GError *error;

	intkeyring_open();

	password = g_hash_table_lookup(intkeyring_passwords, account);

	if (password != NULL) {
		purple_debug_misc("keyring-internal",
			"Got password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		if (cb != NULL)
			cb(account, password, NULL, data);
	} else {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("keyring-internal",
				"No password for account %s (%s).\n",
				purple_account_get_username(account),
				purple_account_get_protocol_id(account));
		}
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_NOPASSWORD, "Password not found.");
		if (cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
	}
}

static void
intkeyring_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	void *old_password;
	intkeyring_open();

	old_password = g_hash_table_lookup(intkeyring_passwords, account);

	if (password == NULL)
		g_hash_table_remove(intkeyring_passwords, account);
	else {
		g_hash_table_replace(intkeyring_passwords, account,
			g_strdup(password));
	}

	if (!(password == NULL && old_password == NULL)) {
		purple_debug_misc("keyring-internal",
			"Password %s for account %s (%s).\n",
			(password == NULL ? "removed" : (old_password == NULL ?
				"saved" : "updated")),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else if (purple_debug_is_verbose()) {
		purple_debug_misc("keyring-internal",
			"Password for account %s (%s) was already removed.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	}

	if (cb != NULL)
		cb(account, NULL, data);
}

static void
intkeyring_close(void)
{
	if (!intkeyring_opened)
		return;
	intkeyring_opened = FALSE;

	g_hash_table_destroy(intkeyring_passwords);
	intkeyring_passwords = NULL;
}

static gboolean
intkeyring_import_password(PurpleAccount *account, const char *mode,
	const char *data, GError **error)
{
	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(data != NULL, FALSE);

	intkeyring_open();

	if (mode == NULL)
		mode = "cleartext";

	if (g_strcmp0(mode, "cleartext") == 0) {
		g_hash_table_replace(intkeyring_passwords, account,
			g_strdup(data));
		return TRUE;
	} else {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				"Invalid password storage mode");
		}
		return FALSE;
	}
}

static gboolean
intkeyring_export_password(PurpleAccount *account, const char **mode,
	char **data, GError **error, GDestroyNotify *destroy)
{
	gchar *password;

	intkeyring_open();

	password = g_hash_table_lookup(intkeyring_passwords, account);

	if (password == NULL) {
		return FALSE;
	} else {
		*mode = "cleartext";
		*data = g_strdup(password);
		*destroy = (GDestroyNotify)purple_str_wipe;
		return TRUE;
	}
}

static PurpleRequestFields *
intkeyring_read_settings(void)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_bool_new("encrypt_passwords",
		_("Encrypt passwords"), purple_prefs_get_bool(
			INTKEYRING_PREFS "encrypt_passwords"));
	purple_request_field_group_add_field(group, field);

	group = purple_request_field_group_new(_("Master password"));
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("passphrase1",
		_("New passphrase:"), "", FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("passphrase2",
		_("New passphrase (again):"), "", FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(group, field);

	group = purple_request_field_group_new(_("Advanced settings"));
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_int_new("pbkdf2_desired_iterations",
		_("Number of PBKDF2 iterations:"), purple_prefs_get_int(
			INTKEYRING_PREFS "pbkdf2_desired_iterations"),
		INTKEYRING_PBKDF2_ITERATIONS_MIN,
		INTKEYRING_PBKDF2_ITERATIONS_MAX);
	purple_request_field_group_add_field(group, field);

	return fields;
}

static gboolean
intkeyring_apply_settings(void *notify_handle,
	PurpleRequestFields *fields)
{
	const gchar *passphrase, *passphrase2;

	passphrase = purple_request_fields_get_string(fields, "passphrase1");
	if (g_strcmp0(passphrase, "") == 0)
		passphrase = NULL;
	passphrase2 = purple_request_fields_get_string(fields, "passphrase2");
	if (g_strcmp0(passphrase2, "") == 0)
		passphrase2 = NULL;

	if (g_strcmp0(passphrase, passphrase2) != 0) {
		purple_notify_error(notify_handle,
			_("Internal keyring settings"),
			_("Passphrases do not match"), NULL);
		return FALSE;
	}

	if (purple_request_fields_get_bool(fields, "encrypt_passwords") && !passphrase) {
		purple_notify_error(notify_handle,
			_("Internal keyring settings"),
			_("You have to set up a Master password, if you want "
			"to enable encryption"), NULL);
		return FALSE;
	}

	purple_prefs_set_bool(INTKEYRING_PREFS "encrypt_passwords",
		purple_request_fields_get_bool(fields, "encrypt_passwords"));

	purple_prefs_set_int(INTKEYRING_PREFS "pbkdf2_desired_iterations",
		purple_request_fields_get_integer(fields,
			"pbkdf2_desired_iterations"));

	if (passphrase)
		intkeyring_change_master_password(passphrase);

	return TRUE;
}

static gboolean
intkeyring_load(PurplePlugin *plugin)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, INTKEYRING_NAME);
	purple_keyring_set_id(keyring_handler, INTKEYRING_ID);
	purple_keyring_set_read_password(keyring_handler,
		intkeyring_read);
	purple_keyring_set_save_password(keyring_handler,
		intkeyring_save);
	purple_keyring_set_close_keyring(keyring_handler,
		intkeyring_close);
	purple_keyring_set_import_password(keyring_handler,
		intkeyring_import_password);
	purple_keyring_set_export_password(keyring_handler,
		intkeyring_export_password);
	purple_keyring_set_read_settings(keyring_handler,
		intkeyring_read_settings);
	purple_keyring_set_apply_settings(keyring_handler,
		intkeyring_apply_settings);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
intkeyring_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		purple_debug_warning("keyring-internal",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	intkeyring_close();

	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,		/* magic */
	PURPLE_MAJOR_VERSION,		/* major_version */
	PURPLE_MINOR_VERSION,		/* minor_version */
	PURPLE_PLUGIN_STANDARD,		/* type */
	NULL,				/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,	/* flags */
	NULL,				/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	INTKEYRING_ID,			/* id */
	INTKEYRING_NAME,		/* name */
	DISPLAY_VERSION,		/* version */
	"Internal Keyring Plugin",	/* summary */
	INTKEYRING_DESCRIPTION,		/* description */
	INTKEYRING_AUTHOR,		/* author */
	PURPLE_WEBSITE,			/* homepage */
	intkeyring_load,		/* load */
	intkeyring_unload,		/* unload */
	NULL,				/* destroy */
	NULL,				/* ui_info */
	NULL,				/* extra_info */
	NULL,				/* prefs_info */
	NULL,				/* actions */
	NULL, NULL, NULL, NULL		/* padding */
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/keyrings");
	purple_prefs_add_none(INTKEYRING_PREFS);
	purple_prefs_add_bool(INTKEYRING_PREFS "encrypt_passwords", FALSE);
	purple_prefs_add_string(INTKEYRING_PREFS "encryption_method",
		"pbkdf2-sha256-aes256");
	purple_prefs_add_int(INTKEYRING_PREFS "pbkdf2_desired_iterations",
		INTKEYRING_PBKDF2_ITERATIONS);
	purple_prefs_add_int(INTKEYRING_PREFS "pbkdf2_iterations",
		INTKEYRING_PBKDF2_ITERATIONS);
	purple_prefs_add_string(INTKEYRING_PREFS "pbkdf2_salt", "");
	purple_prefs_add_string(INTKEYRING_PREFS "key_verifier", "");
}

PURPLE_INIT_PLUGIN(internal_keyring, init_plugin, plugininfo)
