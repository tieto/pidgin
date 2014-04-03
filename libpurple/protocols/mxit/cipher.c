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
 */

#include "internal.h"
#include "debug.h"
#include "libpurple/cipher.h"
#include "ciphers/aescipher.h"

#include "cipher.h"

#define INITIAL_KEY "6170383452343567"
#define SECRET_HEADER "<mxit/>"

/**
 * Encrypt the user's cleartext password using the AES 128-bit (ECB)
 * encryption algorithm.
 *
 * @param session The MXit session object
 *
 * @return The encrypted & encoded password. Must be g_free'd when
 *         no longer needed.
 */
gchar *
mxit_encrypt_password(struct MXitSession* session)
{
	guchar key[16];
	size_t clientkey_len, header_len, pass_len, plaintext_len;
	const gchar *plaintext_passwd;
	guchar *plaintext;
	guchar encrypted[64]; /* shouldn't be longer than 17 */
	PurpleCipher *cipher;
	ssize_t encrypted_size;

	purple_debug_info(MXIT_PLUGIN_ID, "mxit_encrypt_password");

	/* build the AES encryption key */
	g_assert(strlen(INITIAL_KEY) == sizeof(key));
	memcpy(key, INITIAL_KEY, sizeof(key));
	clientkey_len = strlen(session->clientkey);
	if (clientkey_len > sizeof(key))
		clientkey_len = sizeof(key);
	memcpy(key, session->clientkey, clientkey_len);

	/* build the secret data to be encrypted: SECRET_HEADER + password */
	plaintext_passwd = purple_connection_get_password(session->con);
	g_return_val_if_fail(plaintext_passwd, NULL);
	pass_len = strlen(plaintext_passwd);
	header_len = strlen(SECRET_HEADER);
	/* Trailing NUL, just to be safe. But PKCS#7 seems to be enough. */
	plaintext_len = header_len + pass_len + 1;
	plaintext = g_new0(guchar, plaintext_len);
	memcpy(plaintext, SECRET_HEADER, header_len);
	memcpy(plaintext + header_len, plaintext_passwd, pass_len);

	/* encrypt */
	cipher = purple_aes_cipher_new();
	purple_cipher_set_key(cipher, key, sizeof(key));
	purple_cipher_set_batch_mode(cipher, PURPLE_CIPHER_BATCH_MODE_ECB);
	encrypted_size = purple_cipher_encrypt(cipher,
		plaintext, plaintext_len, encrypted, sizeof(encrypted));
	g_return_val_if_fail(encrypted_size > 0, NULL);

	return purple_base64_encode(encrypted, encrypted_size);
}
