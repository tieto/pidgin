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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "cipher.h"
#include "debug.h"

/******************************************************************************
 * PurpleCipher API
 *****************************************************************************/
const gchar *
purple_cipher_get_name(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(cipher, NULL);
	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), NULL);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);
	g_return_val_if_fail(klass->get_name, NULL);

	return klass->get_name(cipher);
}

GType
purple_cipher_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleCipherClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(PurpleCipher),
			0,
			NULL,
			NULL
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "PurpleCipher",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

void
purple_cipher_reset(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->reset)
		klass->reset(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"reset method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

void
purple_cipher_reset_state(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->reset_state)
		klass->reset_state(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"reset_state method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

void
purple_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));
	g_return_if_fail(iv);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_iv)
		klass->set_iv(cipher, iv, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_iv method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

void
purple_cipher_append(PurpleCipher *cipher, const guchar *data,
								size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->append)
		klass->append(cipher, data, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"append method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

gboolean
purple_cipher_digest(PurpleCipher *cipher, guchar digest[], size_t len)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), FALSE);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->digest)
		return klass->digest(cipher, digest, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"digest method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return FALSE;
}

gboolean
purple_cipher_digest_to_str(PurpleCipher *cipher, gchar digest_s[], size_t len)
{
	/* 8k is a bit excessive, will tweak later. */
	guchar digest[BUF_LEN * 4];
	size_t digest_size, n;

	g_return_val_if_fail(cipher, FALSE);
	g_return_val_if_fail(digest_s, FALSE);

	digest_size = purple_cipher_get_digest_size(cipher);

	g_return_val_if_fail(digest_size <= BUF_LEN * 4, FALSE);

	if(!purple_cipher_digest(cipher, digest, sizeof(digest)))
		return FALSE;

	/* Every digest byte occupies 2 chars + the NUL at the end. */
	g_return_val_if_fail(digest_size * 2 + 1 <= len, FALSE);

	for(n = 0; n < digest_size; n++)
		sprintf(digest_s + (n * 2), "%02x", digest[n]);

	digest_s[n * 2] = '\0';

	return TRUE;
}

size_t
purple_cipher_get_digest_size(PurpleCipher *cipher)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), FALSE);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_digest_size)
		return klass->get_digest_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_digest_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return FALSE;
}

ssize_t
purple_cipher_encrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);
	g_return_val_if_fail(input != NULL, -1);
	g_return_val_if_fail(output != NULL, -1);
	g_return_val_if_fail(out_size >= in_len, -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->encrypt)
		return klass->encrypt(cipher, input, in_len, output, out_size);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"encrypt method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

ssize_t
purple_cipher_decrypt(PurpleCipher *cipher, const guchar input[],
							size_t in_len, guchar output[], size_t out_size)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);
	g_return_val_if_fail(input != NULL, -1);
	g_return_val_if_fail(output != NULL, -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->decrypt)
		return klass->decrypt(cipher, input, in_len, output, out_size);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"decrypt method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

void
purple_cipher_set_salt(PurpleCipher *cipher, const guchar *salt, size_t len) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_salt)
		klass->set_salt(cipher, salt, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_salt method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

void
purple_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len) {
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_key)
		klass->set_key(cipher, key, len);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_key method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

size_t
purple_cipher_get_key_size(PurpleCipher *cipher) {
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_key_size)
		return klass->get_key_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_key_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

void
purple_cipher_set_batch_mode(PurpleCipher *cipher,
                                     PurpleCipherBatchMode mode)
{
	PurpleCipherClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIPHER(cipher));

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->set_batch_mode)
		klass->set_batch_mode(cipher, mode);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"set_batch_mode method\n",
						klass->get_name ? klass->get_name(cipher) : "");
}

PurpleCipherBatchMode
purple_cipher_get_batch_mode(PurpleCipher *cipher)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_batch_mode)
		return klass->get_batch_mode(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_batch_mode method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}

size_t
purple_cipher_get_block_size(PurpleCipher *cipher)
{
	PurpleCipherClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIPHER(cipher), -1);

	klass = PURPLE_CIPHER_GET_CLASS(cipher);

	if(klass && klass->get_block_size)
		return klass->get_block_size(cipher);
	else
		purple_debug_warning("cipher", "the %s cipher does not implement the "
						"get_block_size method\n",
						klass->get_name ? klass->get_name(cipher) : "");

	return -1;
}
