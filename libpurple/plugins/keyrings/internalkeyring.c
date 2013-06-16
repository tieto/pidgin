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
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#include "ciphers/aescipher.h"
#include "ciphers/pbkdf2cipher.h"
#include "ciphers/sha256hash.h"

#define INTKEYRING_NAME N_("Internal keyring")
#define INTKEYRING_DESCRIPTION N_("This plugin provides the default password " \
	"storage behaviour for libpurple.")
#define INTKEYRING_AUTHOR "Tomek Wasilczyk (tomkiewicz@cpw.pidgin.im)"
#define INTKEYRING_ID PURPLE_DEFAULT_KEYRING

#define INTKEYRING_VERIFY_STR "[verification-string]"
#define INTKEYRING_PBKDF2_ITERATIONS 10000
#define INTKEYRING_PBKDF2_ITERATIONS_MIN 1000
#define INTKEYRING_PBKDF2_ITERATIONS_MAX 1000000000
#define INTKEYRING_KEY_LEN (256/8)
#define INTKEYRING_ENCRYPT_BUFF_LEN 1000
#define INTKEYRING_ENCRYPTED_MIN_LEN 50
#define INTKEYRING_ENCRYPTION_METHOD "pbkdf2-sha256-aes256"

#define INTKEYRING_PREFS "/plugins/keyrings/internal/"

/* win32 build defines such macro to override read() routine */
#undef read

typedef struct
{
	enum
	{
		INTKEYRING_REQUEST_READ,
		INTKEYRING_REQUEST_SAVE
	} type;
	PurpleAccount *account;
	gchar *password;
	union
	{
		PurpleKeyringReadCallback read;
		PurpleKeyringSaveCallback save;
	} cb;
	gpointer cb_data;
} intkeyring_request;

typedef struct
{
	guchar *data;
	size_t len;
} intkeyring_buff_t;

static intkeyring_buff_t *intkeyring_key;
static GHashTable *intkeyring_passwords = NULL;
static GHashTable *intkeyring_ciphertexts = NULL;

static gboolean intkeyring_opened = FALSE;
static gboolean intkeyring_unlocked = FALSE;

static GList *intkeyring_pending_requests = NULL;
static void *intkeyring_masterpw_uirequest = NULL;

static PurpleKeyring *keyring_handler = NULL;

static void
intkeyring_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data);
static void
intkeyring_save(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data);
static void
intkeyring_reencrypt_passwords(void);
static void
intkeyring_unlock(const gchar *message);

static void
intkeyring_request_free(intkeyring_request *req)
{
	g_return_if_fail(req != NULL);

	purple_str_wipe(req->password);
	g_free(req);
}

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
	if (buff == NULL)
		return;

	memset(buff->data, 0, buff->len);
	g_free(buff->data);
	g_free(buff);
}

static intkeyring_buff_t *
intkeyring_buff_from_base64(const gchar *base64)
{
	guchar *data;
	gsize len;

	data = purple_base64_decode(base64, &len);

	return intkeyring_buff_new(data, len);
}

/************************************************************************/
/* Generic encryption stuff                                             */
/************************************************************************/

static intkeyring_buff_t *
intkeyring_derive_key(const gchar *passphrase, intkeyring_buff_t *salt)
{
	PurpleCipher *cipher;
	PurpleHash *hash;
	gboolean succ;
	intkeyring_buff_t *ret;

	g_return_val_if_fail(passphrase != NULL, NULL);

	hash = purple_sha256_hash_new();
	cipher = purple_pbkdf2_cipher_new(hash);

	g_object_set(G_OBJECT(cipher), "iter_count",
		GUINT_TO_POINTER(purple_prefs_get_int(INTKEYRING_PREFS
		"pbkdf2_iterations")), NULL);
	g_object_set(G_OBJECT(cipher), "out_len", GUINT_TO_POINTER(
		INTKEYRING_KEY_LEN), NULL);
	purple_cipher_set_salt(cipher, salt->data, salt->len);
	purple_cipher_set_key(cipher, (const guchar*)passphrase,
		strlen(passphrase));

	ret = intkeyring_buff_new(g_new(guchar, INTKEYRING_KEY_LEN),
		INTKEYRING_KEY_LEN);
	succ = purple_cipher_digest(cipher, ret->data, ret->len);

	g_object_unref(cipher);
	g_object_unref(hash);

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

/**
 * Encrypts a plaintext using the specified key.
 *
 * Random IV will be generated and stored with ciphertext.
 *
 * Encryption scheme:
 * [ IV ] ++ AES( [ plaintext ] ++ [ min length padding ] ++
 *                [ control string ] ++ [ pkcs7 padding ] )
 * where:
 *  IV:                 Random, 128bit IV.
 *  plaintext:          The plaintext.
 *  min length padding: The padding used to hide the rough length of short
 *                      plaintexts, may have a length of 0.
 *  control string:     Constant string, verifies corectness of decryption.
 *  pkcs7 padding:      The padding used to determine total length of encrypted
 *                      content (also provides some verification).
 *
 * @param key The AES key.
 * @param str The NUL-terminated plaintext.
 * @return    The ciphertext with IV, encoded as base64. Must be g_free'd.
 */
static gchar *
intkeyring_encrypt(intkeyring_buff_t *key, const gchar *str)
{
	PurpleCipher *cipher;
	intkeyring_buff_t *iv;
	guchar plaintext[INTKEYRING_ENCRYPT_BUFF_LEN];
	size_t plaintext_len, text_len, verify_len;
	guchar encrypted_raw[INTKEYRING_ENCRYPT_BUFF_LEN];
	ssize_t encrypted_size;

	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(str != NULL, NULL);

	text_len = strlen(str);
	verify_len = strlen(INTKEYRING_VERIFY_STR);
	plaintext_len = INTKEYRING_ENCRYPTED_MIN_LEN;
	if (plaintext_len < text_len)
		plaintext_len = text_len;

	g_return_val_if_fail(plaintext_len + verify_len <= sizeof(plaintext),
		NULL);

	cipher = purple_aes_cipher_new();
	g_return_val_if_fail(cipher != NULL, NULL);

	memset(plaintext, 0, plaintext_len);
	memcpy(plaintext, str, text_len);
	memcpy(plaintext + plaintext_len, INTKEYRING_VERIFY_STR, verify_len);
	plaintext_len += verify_len;

	iv = intkeyring_gen_salt(purple_cipher_get_block_size(cipher));
	purple_cipher_set_iv(cipher, iv->data, iv->len);
	purple_cipher_set_key(cipher, key->data, key->len);
	purple_cipher_set_batch_mode(cipher,
		PURPLE_CIPHER_BATCH_MODE_CBC);

	memcpy(encrypted_raw, iv->data, iv->len);

	encrypted_size = purple_cipher_encrypt(cipher,
		plaintext, plaintext_len, encrypted_raw + iv->len,
		sizeof(encrypted_raw) - iv->len);
	encrypted_size += iv->len;

	memset(plaintext, 0, plaintext_len);
	intkeyring_buff_free(iv);
	g_object_unref(cipher);

	if (encrypted_size < 0)
		return NULL;

	return purple_base64_encode(encrypted_raw, encrypted_size);

}

static gchar *
intkeyring_decrypt(intkeyring_buff_t *key, const gchar *str)
{
	PurpleCipher *cipher;
	guchar *encrypted_raw;
	gsize encrypted_size;
	size_t iv_len, verify_len, text_len;
	guchar plaintext[INTKEYRING_ENCRYPT_BUFF_LEN];
	const gchar *verify_str = NULL;
	ssize_t plaintext_len;
	gchar *ret;

	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(str != NULL, NULL);

	cipher = purple_aes_cipher_new();
	g_return_val_if_fail(cipher != NULL, NULL);

	encrypted_raw = purple_base64_decode(str, &encrypted_size);
	g_return_val_if_fail(encrypted_raw != NULL, NULL);

	iv_len = purple_cipher_get_block_size(cipher);
	if (encrypted_size < iv_len) {
		g_free(encrypted_raw);
		return NULL;
	}

	purple_cipher_set_iv(cipher, encrypted_raw, iv_len);
	purple_cipher_set_key(cipher, key->data, key->len);
	purple_cipher_set_batch_mode(cipher,
		PURPLE_CIPHER_BATCH_MODE_CBC);

	plaintext_len = purple_cipher_decrypt(cipher,
		encrypted_raw + iv_len, encrypted_size - iv_len,
		plaintext, sizeof(plaintext));

	g_free(encrypted_raw);
	g_object_unref(cipher);

	verify_len = strlen(INTKEYRING_VERIFY_STR);
	/* Don't remove the len > 0 check! */
	if (plaintext_len > 0 && plaintext_len > verify_len &&
		plaintext[plaintext_len] == '\0')
	{
		verify_str = (gchar*)plaintext + plaintext_len - verify_len;
	}

	if (g_strcmp0(verify_str, INTKEYRING_VERIFY_STR) != 0) {
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

/************************************************************************/
/* Password encryption                                                  */
/************************************************************************/

static gboolean
intkeyring_change_masterpw(const gchar *new_password)
{
	intkeyring_buff_t *salt, *key;
	gchar *verifier = NULL, *salt_b64 = NULL;
	int old_iter;
	gboolean succ = TRUE;;

	g_return_val_if_fail(intkeyring_unlocked, FALSE);

	old_iter = purple_prefs_get_int(INTKEYRING_PREFS "pbkdf2_iterations");
	purple_prefs_set_int(INTKEYRING_PREFS "pbkdf2_iterations",
		purple_prefs_get_int(INTKEYRING_PREFS
			"pbkdf2_desired_iterations"));

	salt = intkeyring_gen_salt(32);
	key = intkeyring_derive_key(new_password, salt);

	if (salt && key && key->len == INTKEYRING_KEY_LEN) {
		/* In fact, verify str will be concatenated twice before
		 * encryption (it's used as a suffix in encryption routine),
		 * but it's not a problem.
		 */
		verifier = intkeyring_encrypt(key, INTKEYRING_VERIFY_STR);
		salt_b64 = purple_base64_encode(salt->data, salt->len);
	}

	if (!verifier || !salt_b64) {
		purple_debug_error("keyring-internal", "Failed to change "
			"master password\n");
		succ = FALSE;
		purple_prefs_set_int(INTKEYRING_PREFS "pbkdf2_iterations",
			old_iter);
	} else {
		purple_prefs_set_string(INTKEYRING_PREFS "pbkdf2_salt",
			salt_b64);
		purple_prefs_set_string(INTKEYRING_PREFS "key_verifier",
			verifier);

		intkeyring_buff_free(intkeyring_key);
		intkeyring_key = key;
		key = NULL;

		intkeyring_reencrypt_passwords();

		purple_signal_emit(purple_keyring_get_handle(),
			"password-migration", NULL);
	}

	g_free(salt_b64);
	g_free(verifier);
	intkeyring_buff_free(salt);
	intkeyring_buff_free(key);

	return succ;
}

static void
intkeyring_process_queue(void)
{
	GList *requests, *it;
	gboolean open = intkeyring_unlocked;

	requests = g_list_first(intkeyring_pending_requests);
	intkeyring_pending_requests = NULL;

	for (it = requests; it != NULL; it = g_list_next(it))
	{
		intkeyring_request *req = it->data;

		if (open && req->type == INTKEYRING_REQUEST_READ) {
			intkeyring_read(req->account, req->cb.read,
				req->cb_data);
		} else if (open && req->type == INTKEYRING_REQUEST_SAVE) {
			intkeyring_save(req->account, req->password,
				req->cb.save, req->cb_data);
		} else if (open)
			g_assert_not_reached();
		else if (req->cb.read != NULL /* || req->cb.write != NULL */ ) {
			GError *error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_CANCELLED,
				_("Operation cancelled."));
			if (req->type == INTKEYRING_REQUEST_READ) {
				req->cb.read(req->account, NULL, error,
					req->cb_data);
			} else if (req->type == INTKEYRING_REQUEST_SAVE)
				req->cb.save(req->account, error, req->cb_data);
			else
				g_assert_not_reached();
			g_error_free(error);
		}

		intkeyring_request_free(req);
	}
	g_list_free(requests);
}

static void
intkeyring_decrypt_password(PurpleAccount *account, const gchar *ciphertext)
{
	gchar *plaintext;

	plaintext = intkeyring_decrypt(intkeyring_key, ciphertext);
	if (plaintext == NULL) {
		purple_debug_warning("keyring-internal",
			"Failed to decrypt a password\n");
		return;
	}

	g_hash_table_replace(intkeyring_passwords, account, plaintext);
}

static void
intkeyring_encrypt_password_if_needed(PurpleAccount *account)
{
	const gchar *plaintext;
	gchar *ciphertext;

	ciphertext = g_hash_table_lookup(intkeyring_ciphertexts, account);
	if (ciphertext != NULL)
		return;

	plaintext = g_hash_table_lookup(intkeyring_passwords, account);
	if (plaintext == NULL)
		return;

	ciphertext = intkeyring_encrypt(intkeyring_key, plaintext);
	g_return_if_fail(ciphertext != NULL);

	g_hash_table_replace(intkeyring_ciphertexts, account, ciphertext);
}

static void
intkeyring_encrypt_passwords_if_needed_it(gpointer account, gpointer plaintext,
	gpointer _unused)
{
	intkeyring_encrypt_password_if_needed(account);
}

static void
intkeyring_reencrypt_passwords(void)
{
	g_hash_table_remove_all(intkeyring_ciphertexts);
	g_hash_table_foreach(intkeyring_passwords,
		intkeyring_encrypt_passwords_if_needed_it, NULL);
}

static void
intkeyring_unlock_decrypt(gpointer account, gpointer ciphertext,
	gpointer _unused)
{
	intkeyring_decrypt_password(account, ciphertext);
}

/************************************************************************/
/* Opening and unlocking keyring                                        */
/************************************************************************/

static void
intkeyring_unlock_ok(gpointer _unused,
	PurpleRequestFields *fields)
{
	const gchar *masterpw;
	gchar *verifier;
	intkeyring_buff_t *salt, *key;

	intkeyring_masterpw_uirequest = NULL;

	if (g_strcmp0(purple_prefs_get_string(INTKEYRING_PREFS
		"encryption_method"), INTKEYRING_ENCRYPTION_METHOD) != 0) {
		purple_notify_error(NULL,
			_("Unlocking internal keyring"),
			_("Selected encryption method is not supported."),
			_("Most probably, your passwords were encrypted with "
			"newer Pidgin/libpurple version, please update."));
		return;
	}

	masterpw = purple_request_fields_get_string(fields, "password");

	if (masterpw == NULL || masterpw[0] == '\0') {
		intkeyring_unlock(_("No password entered."));
		return;
	}

	salt = intkeyring_buff_from_base64(purple_prefs_get_string(
		INTKEYRING_PREFS "pbkdf2_salt"));
	key = intkeyring_derive_key(masterpw, salt);
	intkeyring_buff_free(salt);

	verifier = intkeyring_decrypt(key, purple_prefs_get_string(
		INTKEYRING_PREFS "key_verifier"));

	if (g_strcmp0(verifier, INTKEYRING_VERIFY_STR) != 0) {
		g_free(verifier);
		intkeyring_buff_free(key);
		intkeyring_unlock(_("Invalid master password entered, "
			"try again."));
		return;
	}

	g_free(verifier);
	intkeyring_key = key;
	intkeyring_unlocked = TRUE;

	g_hash_table_foreach(intkeyring_ciphertexts,
		intkeyring_unlock_decrypt, NULL);

	intkeyring_process_queue();
}

static void
intkeyring_unlock_cancel(gpointer _unused,
	PurpleRequestFields *fields)
{
	intkeyring_masterpw_uirequest = NULL;
	intkeyring_process_queue();
}

static void
intkeyring_unlock(const gchar *message)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	const gchar *primary_msg, *secondary_msg = NULL;

	if (intkeyring_unlocked || intkeyring_masterpw_uirequest != NULL)
		return;

	if (!purple_prefs_get_bool(INTKEYRING_PREFS "encrypt_passwords")) {
		intkeyring_unlocked = TRUE;
		intkeyring_process_queue();
		return;
	}

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password",
		_("Master password"), "", FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(group, field);

	primary_msg = _("Please, enter master password");
	if (message) {
		secondary_msg = primary_msg;
		primary_msg = message;
	}

	intkeyring_masterpw_uirequest = purple_request_fields(NULL,
		_("Unlocking internal keyring"),
		primary_msg, secondary_msg, fields,
		_("OK"), G_CALLBACK(intkeyring_unlock_ok),
		_("Cancel"), G_CALLBACK(intkeyring_unlock_cancel),
		NULL, NULL, NULL, NULL);
}

static void
intkeyring_open(void)
{
	if (intkeyring_opened)
		return;
	intkeyring_opened = TRUE;

	intkeyring_passwords = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify)purple_str_wipe);
	intkeyring_ciphertexts = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
}

/************************************************************************/
/* Keyring interface implementation                                     */
/************************************************************************/

static void
intkeyring_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	const char *password;
	GError *error;

	intkeyring_open();

	if (!intkeyring_unlocked && g_hash_table_lookup(intkeyring_ciphertexts,
		account) != NULL)
	{
		intkeyring_request *req = g_new0(intkeyring_request, 1);

		req->type = INTKEYRING_REQUEST_READ;
		req->account = account;
		req->cb.read = cb;
		req->cb_data = data;
		intkeyring_pending_requests =
			g_list_append(intkeyring_pending_requests, req);

		intkeyring_unlock(NULL);
		return;
	}

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
			PURPLE_KEYRING_ERROR_NOPASSWORD,
			_("Password not found."));
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

	if (!intkeyring_unlocked) {
		intkeyring_request *req;

		if (password == NULL) {
			g_hash_table_remove(intkeyring_ciphertexts, account);
			g_hash_table_remove(intkeyring_passwords, account);
			if (cb)
				cb(account, NULL, data);
			return;
		}

		req = g_new0(intkeyring_request, 1);
		req->type = INTKEYRING_REQUEST_SAVE;
		req->account = account;
		req->password = g_strdup(password);
		req->cb.save = cb;
		req->cb_data = data;
		intkeyring_pending_requests =
			g_list_append(intkeyring_pending_requests, req);

		intkeyring_unlock(NULL);
		return;
	}

	g_hash_table_remove(intkeyring_ciphertexts, account);

	old_password = g_hash_table_lookup(intkeyring_passwords, account);

	if (password == NULL)
		g_hash_table_remove(intkeyring_passwords, account);
	else {
		g_hash_table_replace(intkeyring_passwords, account,
			g_strdup(password));
	}

	intkeyring_encrypt_password_if_needed(account);

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
	intkeyring_unlocked = FALSE;

	if (intkeyring_masterpw_uirequest) {
		purple_request_close(PURPLE_REQUEST_FIELDS,
			intkeyring_masterpw_uirequest);
	}
	g_warn_if_fail(intkeyring_masterpw_uirequest == NULL);
	g_warn_if_fail(intkeyring_pending_requests == NULL);

	intkeyring_buff_free(intkeyring_key);
	intkeyring_key = NULL;
	g_hash_table_destroy(intkeyring_passwords);
	intkeyring_passwords = NULL;
	g_hash_table_destroy(intkeyring_ciphertexts);
	intkeyring_ciphertexts = NULL;
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
	} else if (g_strcmp0(mode, "ciphertext") == 0) {
		if (intkeyring_unlocked)
			intkeyring_decrypt_password(account, data);
		else {
			g_hash_table_replace(intkeyring_ciphertexts, account,
				g_strdup(data));
		}
		return TRUE;
	} else {
		if (error != NULL) {
			*error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				_("Invalid password storage mode."));
		}
		return FALSE;
	}
}

static gboolean
intkeyring_export_password(PurpleAccount *account, const char **mode,
	char **data, GError **error, GDestroyNotify *destroy)
{
	gchar *ciphertext = NULL;
	intkeyring_open();

	if (!purple_prefs_get_bool(INTKEYRING_PREFS "encrypt_passwords")) {
		gchar *cleartext = g_hash_table_lookup(intkeyring_passwords,
			account);

		if (cleartext == NULL)
			return FALSE;

		*mode = "cleartext";
		*data = g_strdup(cleartext);
		*destroy = (GDestroyNotify)purple_str_wipe;
		return TRUE;
	}

	ciphertext = g_strdup(g_hash_table_lookup(intkeyring_ciphertexts,
		account));

	if (ciphertext == NULL && intkeyring_unlocked) {
		gchar *plaintext = g_hash_table_lookup(intkeyring_passwords,
			account);

		if (plaintext == NULL)
			return FALSE;

		purple_debug_warning("keyring-internal", "Encrypted password "
			"is missing at export (it shouldn't happen)\n");
		ciphertext = intkeyring_encrypt(intkeyring_key, plaintext);
	}

	if (ciphertext == NULL)
		return FALSE;

	*mode = "ciphertext";
	*data = ciphertext;
	*destroy = (GDestroyNotify)g_free;
	return TRUE;
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

	intkeyring_unlock(_("You have to unlock the keyring first."));
	if (!intkeyring_unlocked)
		return FALSE;

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

	if (purple_request_fields_get_bool(fields, "encrypt_passwords") &&
		!passphrase && !intkeyring_key)
	{
		purple_notify_error(notify_handle,
			_("Internal keyring settings"),
			_("You have to set up a Master password, if you want "
			"to enable encryption"), NULL);
		return FALSE;
	}

	if (!purple_request_fields_get_bool(fields, "encrypt_passwords") &&
		passphrase)
	{
		purple_notify_error(notify_handle,
			_("Internal keyring settings"),
			_("You don't need any master password, if you won't "
			"enable passwords encryption"), NULL);
		return FALSE;
	}

	purple_prefs_set_string(INTKEYRING_PREFS "encryption_method",
		INTKEYRING_ENCRYPTION_METHOD);

	purple_prefs_set_int(INTKEYRING_PREFS "pbkdf2_desired_iterations",
		purple_request_fields_get_integer(fields,
			"pbkdf2_desired_iterations"));

	if (passphrase != NULL) {
		if (!intkeyring_change_masterpw(passphrase))
			return FALSE;
	}

	purple_prefs_set_bool(INTKEYRING_PREFS "encrypt_passwords",
		purple_request_fields_get_bool(fields, "encrypt_passwords"));

	purple_signal_emit(purple_keyring_get_handle(), "password-migration",
		NULL);


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

	if (intkeyring_key != NULL) {
		purple_debug_warning("keyring-internal", "Master key should be "
			"cleaned up at this point\n");
		intkeyring_buff_free(intkeyring_key);
		intkeyring_key = NULL;
	}

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
	purple_prefs_add_none("/plugins/keyrings/internal");
	purple_prefs_add_bool(INTKEYRING_PREFS "encrypt_passwords", FALSE);
	purple_prefs_add_string(INTKEYRING_PREFS "encryption_method",
		INTKEYRING_ENCRYPTION_METHOD);
	purple_prefs_add_int(INTKEYRING_PREFS "pbkdf2_desired_iterations",
		INTKEYRING_PBKDF2_ITERATIONS);
	purple_prefs_add_int(INTKEYRING_PREFS "pbkdf2_iterations",
		INTKEYRING_PBKDF2_ITERATIONS);
	purple_prefs_add_string(INTKEYRING_PREFS "pbkdf2_salt", "");
	purple_prefs_add_string(INTKEYRING_PREFS "key_verifier", "");
}

PURPLE_INIT_PLUGIN(internal_keyring, init_plugin, plugininfo)
