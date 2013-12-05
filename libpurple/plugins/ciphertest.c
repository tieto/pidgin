/*
 * A plugin to test the ciphers that ship with purple
 *
 * Copyright (C) 2004, Gary Kramlich <amc_grim@users.sf.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "internal.h"

#include <glib.h>
#include <string.h>

#include "debug.h"
#include "plugin.h"
#include "version.h"
#include "util.h"

#include "ciphers/aescipher.h"
#include "ciphers/md5hash.h"
#include "ciphers/pbkdf2cipher.h"
#include "ciphers/sha1hash.h"
#include "ciphers/sha256hash.h"

struct test {
	const gchar *question;
	const gchar *answer;
};

/**************************************************************************
 * MD5 Stuff
 **************************************************************************/
struct test md5_tests[8] = {
	{							"",	"d41d8cd98f00b204e9800998ecf8427e"},
	{						   "a",	"0cc175b9c0f1b6a831c399e269772661"},
	{						 "abc",	"900150983cd24fb0d6963f7d28e17f72"},
	{			  "message digest",	"f96b697d7cb7938d525a2f31aaf161d0"},
	{ "abcdefghijklmnopqrstuvwxyz",	"c3fcd3d76192e4007dfb496cca67e13b"},
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	  "abcdefghijklmnopqrstuvwxyz"
					  "0123456789",	"d174ab98d277d9f5a5611c2c9f419d9f"},
	{"123456789012345678901234567"
	 "890123456789012345678901234"
	  "56789012345678901234567890",	"57edf4a22be3c955ac49da2e2107b67a"},
	{						  NULL, NULL							  }
};

static void
cipher_test_md5(void) {
	PurpleHash *hash;
	gchar digest[33];
	gboolean ret;
	gint i = 0;

	hash = purple_md5_hash_new();
	if(!hash) {
		purple_debug_info("cipher-test",
						"could not find md5 cipher, not testing\n");
		return;
	}

	purple_debug_info("cipher-test", "Running md5 tests\n");

	while(md5_tests[i].answer) {
		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test", "Testing '%s'\n", md5_tests[i].question);

		purple_hash_append(hash, (guchar *)md5_tests[i].question,
								   strlen(md5_tests[i].question));

		ret = purple_hash_digest_to_str(hash, digest, sizeof(digest));

		if(!ret) {
			purple_debug_info("cipher-test", "failed\n");
		} else {
			purple_debug_info("cipher-test", "\tGot:    %s\n", digest);
			purple_debug_info("cipher-test", "\tWanted: %s\n",
							md5_tests[i].answer);
		}

		purple_hash_reset(hash);
		i++;
	}

	g_object_unref(hash);

	purple_debug_info("cipher-test", "md5 tests completed\n\n");
}

/**************************************************************************
 * SHA-1 stuff
 **************************************************************************/
struct test sha1_tests[5] = {
	{"a", "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8"},
	{"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"} ,
	{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1"} ,
	{NULL, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
	{NULL, NULL}
};

static void
cipher_test_sha1(void) {
	PurpleHash *hash;
	gchar digest[41];
	gint i = 0;
	gboolean ret;

	hash = purple_sha1_hash_new();
	if(!hash) {
		purple_debug_info("cipher-test",
						"could not find sha1 cipher, not testing\n");
		return;
	}

	purple_debug_info("cipher-test", "Running sha1 tests\n");

	while(sha1_tests[i].answer) {
		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test", "Testing '%s'\n",
						(sha1_tests[i].question != NULL) ?
						sha1_tests[i].question : "'a'x1000, 1000 times");

		if(sha1_tests[i].question) {
			purple_hash_append(hash, (guchar *)sha1_tests[i].question,
									   strlen(sha1_tests[i].question));
		} else {
			gint j;
			guchar buff[1000];

			memset(buff, 'a', 1000);

			for(j = 0; j < 1000; j++)
				purple_hash_append(hash, buff, 1000);
		}

		ret = purple_hash_digest_to_str(hash, digest, sizeof(digest));

		if(!ret) {
			purple_debug_info("cipher-test", "failed\n");
		} else {
			purple_debug_info("cipher-test", "\tGot:    %s\n", digest);
			purple_debug_info("cipher-test", "\tWanted: %s\n",
							sha1_tests[i].answer);
		}

		purple_hash_reset(hash);
		i++;
	}

	g_object_unref(hash);

	purple_debug_info("cipher-test", "sha1 tests completed\n\n");
}

static void
cipher_test_digest(void)
{
	const gchar *nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
	const gchar *client_nonce = "0a4f113b";
	const gchar *username = "Mufasa";
	const gchar *realm = "testrealm@host.com";
	const gchar *password = "Circle Of Life";
	const gchar *algorithm = "md5";
	const gchar *nonce_count = "00000001";
	const gchar *method = "GET";
	const gchar *qop = "auth";
	const gchar *digest_uri = "/dir/index.html";
	const gchar *entity = NULL;

	gchar *session_key;

	purple_debug_info("cipher-test", "Running HTTP Digest tests\n");

	session_key = purple_http_digest_calculate_session_key(
						algorithm, username, realm, password,
						nonce, client_nonce);

	if (session_key == NULL)
	{
		purple_debug_info("cipher-test",
						"purple_cipher_http_digest_calculate_session_key failed.\n");
	}
	else
	{
		gchar *response;

		purple_debug_info("cipher-test", "\tsession_key: Got:    %s\n", session_key);
		purple_debug_info("cipher-test", "\tsession_key: Wanted: %s\n", "939e7578ed9e3c518a452acee763bce9");

		response = purple_http_digest_calculate_response(
				algorithm, method, digest_uri, qop, entity,
				nonce, nonce_count, client_nonce, session_key);

		g_free(session_key);

		if (response == NULL)
		{
			purple_debug_info("cipher-test",
							"purple_cipher_http_digest_calculate_session_key failed.\n");
		}
		else
		{
			purple_debug_info("cipher-test", "\tresponse: Got:    %s\n", response);
			purple_debug_info("cipher-test", "\tresponse: Wanted: %s\n", "6629fae49393a05397450978507c4ef1");
			g_free(response);
		}
	}

	purple_debug_info("cipher-test", "HTTP Digest tests completed\n\n");
}

/**************************************************************************
 * PBKDF2 stuff
 **************************************************************************/

#ifdef HAVE_NSS
#  include <nss.h>
#  include <secmod.h>
#  include <pk11func.h>
#  include <prerror.h>
#  include <secerr.h>
#endif

typedef struct {
	const gchar *hash;
	const guint iter_count;
	const gchar *passphrase;
	const gchar *salt;
	const guint out_len;
	const gchar *answer;
} pbkdf2_test;

pbkdf2_test pbkdf2_tests[] = {
	{ "sha256", 1, "password", "salt", 32, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
	{ "sha1", 1, "password", "salt", 32, "0c60c80f961f0e71f3a9b524af6012062fe037a6e0f0eb94fe8fc46bdc637164"},
	{ "sha1", 1000, "ala ma kota", "", 16, "924dba137b5bcf6d0de84998f3d8e1f9"},
	{ "sha1", 1, "", "", 32, "1e437a1c79d75be61e91141dae20affc4892cc99abcc3fe753887bccc8920176"},
	{ "sha256", 100, "some password", "and salt", 1, "c7"},
	{ "sha1", 10000, "pretty long password W Szczebrzeszynie chrzaszcz brzmi w trzcinie i Szczebrzeszyn z tego slynie", "Grzegorz Brzeczyszczykiewicz", 32, "8cb0cb164f2554733ae02f5751b0e84a88fb385446e85a3991bdcdf1ea11795c"},
	{ NULL, 0, NULL, NULL, 0, NULL}
};

#ifdef HAVE_NSS

static gchar*
cipher_pbkdf2_nss_sha1(const gchar *passphrase, const gchar *salt,
	guint iter_count, guint out_len)
{
	PK11SlotInfo *slot;
	SECAlgorithmID *algorithm = NULL;
	PK11SymKey *symkey = NULL;
	const SECItem *symkey_data = NULL;
	SECItem salt_item, passphrase_item;
	guchar *passphrase_buff, *salt_buff;
	gchar *ret;

	g_return_val_if_fail(passphrase != NULL, NULL);
	g_return_val_if_fail(iter_count > 0, NULL);
	g_return_val_if_fail(out_len > 0, NULL);

	NSS_NoDB_Init(NULL);

	slot = PK11_GetBestSlot(PK11_AlgtagToMechanism(SEC_OID_PKCS5_PBKDF2),
		NULL);
	if (slot == NULL) {
		purple_debug_error("cipher-test", "NSS: couldn't get slot: "
			"%d\n", PR_GetError());
		return NULL;
	}

	salt_buff = (guchar*)g_strdup(salt ? salt : "");
	salt_item.type = siBuffer;
	salt_item.data = salt_buff;
	salt_item.len = salt ? strlen(salt) : 0;

	algorithm = PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2,
		SEC_OID_AES_256_CBC, SEC_OID_HMAC_SHA1, out_len, iter_count,
		&salt_item);
	if (algorithm == NULL) {
		purple_debug_error("cipher-test", "NSS: couldn't create "
			"algorithm ID: %d\n", PR_GetError());
		PK11_FreeSlot(slot);
		g_free(salt_buff);
		return NULL;
	}

	passphrase_buff = (guchar*)g_strdup(passphrase);
	passphrase_item.type = siBuffer;
	passphrase_item.data = passphrase_buff;
	passphrase_item.len = strlen(passphrase);

	symkey = PK11_PBEKeyGen(slot, algorithm, &passphrase_item, PR_FALSE,
		NULL);
	if (symkey == NULL) {
		purple_debug_error("cipher-test", "NSS: Couldn't generate key: "
			"%d\n", PR_GetError());
		SECOID_DestroyAlgorithmID(algorithm, PR_TRUE);
		PK11_FreeSlot(slot);
		g_free(passphrase_buff);
		g_free(salt_buff);
		return NULL;
	}

	if (PK11_ExtractKeyValue(symkey) == SECSuccess)
		symkey_data = PK11_GetKeyData(symkey);

	if (symkey_data == NULL || symkey_data->data == NULL) {
		purple_debug_error("cipher-test", "NSS: Couldn't extract key "
			"value: %d\n", PR_GetError());
		PK11_FreeSymKey(symkey);
		SECOID_DestroyAlgorithmID(algorithm, PR_TRUE);
		PK11_FreeSlot(slot);
		g_free(passphrase_buff);
		g_free(salt_buff);
		return NULL;
	}

	if (symkey_data->len != out_len) {
		purple_debug_error("cipher-test", "NSS: Invalid key length: %d "
			"(should be %d)\n", symkey_data->len, out_len);
		PK11_FreeSymKey(symkey);
		SECOID_DestroyAlgorithmID(algorithm, PR_TRUE);
		PK11_FreeSlot(slot);
		g_free(passphrase_buff);
		g_free(salt_buff);
		return NULL;
	}

	ret = purple_base16_encode(symkey_data->data, symkey_data->len);

	PK11_FreeSymKey(symkey);
	SECOID_DestroyAlgorithmID(algorithm, PR_TRUE);
	PK11_FreeSlot(slot);
	g_free(passphrase_buff);
	g_free(salt_buff);
	return ret;
}

#endif /* HAVE_NSS */

static void
cipher_test_pbkdf2(void)
{
	PurpleCipher *cipher;
	PurpleHash *hash;
	int i = 0;
	gboolean fail = FALSE;

	purple_debug_info("cipher-test", "Running PBKDF2 tests\n");

	while (!fail && pbkdf2_tests[i].answer) {
		pbkdf2_test *test = &pbkdf2_tests[i];
		gchar digest[2 * 32 + 1 + 10];
		gchar *digest_nss = NULL;
		gboolean ret, skip_nss = FALSE;

		i++;

		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test",
			"\tTesting '%s' with salt:'%s' hash:%s iter_count:%d \n",
			test->passphrase, test->salt, test->hash,
			test->iter_count);

		if (!strcmp(test->hash, "sha1"))
			hash = purple_sha1_hash_new();
		else if (!strcmp(test->hash, "sha256"))
			hash = purple_sha256_hash_new();
		else
			hash = NULL;

		cipher = purple_pbkdf2_cipher_new(hash);

		g_object_set(G_OBJECT(cipher), "iter_count", GUINT_TO_POINTER(test->iter_count), NULL);
		g_object_set(G_OBJECT(cipher), "out_len", GUINT_TO_POINTER(test->out_len), NULL);
		purple_cipher_set_salt(cipher, (const guchar*)test->salt, test->salt ? strlen(test->salt): 0);
		purple_cipher_set_key(cipher, (const guchar*)test->passphrase, strlen(test->passphrase));

		ret = purple_cipher_digest_to_str(cipher, digest, sizeof(digest));
		purple_cipher_reset(cipher);

		if (!ret) {
			purple_debug_info("cipher-test", "\tfailed\n");
			fail = TRUE;
			g_object_unref(cipher);
			g_object_unref(hash);
			continue;
		}

		if (g_strcmp0(test->hash, "sha1") != 0)
			skip_nss = TRUE;
		if (test->out_len != 16 && test->out_len != 32)
			skip_nss = TRUE;

#ifdef HAVE_NSS
		if (!skip_nss) {
			digest_nss = cipher_pbkdf2_nss_sha1(test->passphrase,
				test->salt, test->iter_count, test->out_len);
		}
#else
		skip_nss = TRUE;
#endif

		if (!ret) {
			purple_debug_info("cipher-test", "\tnss test failed\n");
			fail = TRUE;
		}

		purple_debug_info("cipher-test", "\tGot:          %s\n", digest);
		if (digest_nss)
			purple_debug_info("cipher-test", "\tGot from NSS: %s\n", digest_nss);
		purple_debug_info("cipher-test", "\tWanted:       %s\n", test->answer);

		if (g_strcmp0(digest, test->answer) == 0 &&
			(skip_nss || g_strcmp0(digest, digest_nss) == 0)) {
			purple_debug_info("cipher-test", "\tTest OK\n");
		}
		else {
			purple_debug_info("cipher-test", "\twrong answer\n");
			fail = TRUE;
		}

		g_object_unref(cipher);
		g_object_unref(hash);
	}

	if (fail)
		purple_debug_info("cipher-test", "PBKDF2 tests FAILED\n\n");
	else
		purple_debug_info("cipher-test", "PBKDF2 tests completed successfully\n\n");
}

/**************************************************************************
 * AES stuff
 **************************************************************************/

typedef struct {
	const gchar *iv;
	const gchar *key;
	const gchar *plaintext;
	const gchar *cipher;
} aes_test;

aes_test aes_tests[] = {
	{ NULL, "000102030405060708090a0b0c0d0e0f", "plaintext", "152e5b950e5f28fafadee9e96fcc59c9" },
	{ NULL, "000102030405060708090a0b0c0d0e0f", NULL, "954f64f2e4e86e9eee82d20216684899" },
	{ "01010101010101010101010101010101", "000102030405060708090a0b0c0d0e0f", NULL, "35d14e6d3e3a279cf01e343e34e7ded3" },
	{ "01010101010101010101010101010101", "000102030405060708090a0b0c0d0e0f", "plaintext", "19d1996e8c098cf3c94bba5dcf5bc57e" },
	{ "01010101010101010101010101010101", "000102030405060708090a0b0c0d0e0f1011121314151617", "plaintext", "e8fba0deae94f63fe72ae9b8ef128aed" },
	{ "01010101010101010101010101010101", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "plaintext", "e2dc50f6c60b33ac3b5953b6503cb684" },
	{ "01010101010101010101010101010101", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "W Szczebrzeszynie chrzaszcz brzmi w trzcinie i Szczebrzeszyn z tego slynie", "8fcc068964e3505f0c2fac61c24592e5c8a9582d3a3264217cf605e9fd1cb056e679e159c4ac3110e8ce6c76c6630d42658c566ba3750c0e6da385b3a9baaa8b3a735b4c1ecaacf58037c8c281e523d2" },
	{ NULL, NULL, NULL, NULL }
};

static void
cipher_test_aes(void)
{
	PurpleCipher *cipher;
	int i = 0;
	gboolean fail = FALSE;

	purple_debug_info("cipher-test", "Running AES tests\n");

	cipher = purple_aes_cipher_new();
	if (cipher == NULL) {
		purple_debug_error("cipher-test", "AES cipher not found\n");
		fail = TRUE;
	}

	while (!fail && aes_tests[i].cipher) {
		aes_test *test = &aes_tests[i];
		gsize key_size;
		guchar *key;
		guchar cipher_s[1024], decipher_s[1024];
		ssize_t cipher_len, decipher_len;
		gchar *cipher_b16, *deciphered;

		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test", "\tTesting '%s' (%" G_GSIZE_FORMAT "bit) \n",
			test->plaintext ? test->plaintext : "(null)",
			strlen(test->key) * 8 / 2);

		i++;

		purple_cipher_reset(cipher);

		if (test->iv) {
			gsize iv_size;
			guchar *iv = purple_base16_decode(test->iv, &iv_size);
			g_assert(iv != NULL);
			purple_cipher_set_iv(cipher, iv, iv_size);
			g_free(iv);
		}

		key = purple_base16_decode(test->key, &key_size);
		g_assert(key != NULL);
		purple_cipher_set_key(cipher, key, key_size);
		g_free(key);

		if (purple_cipher_get_key_size(cipher) != key_size) {
			purple_debug_info("cipher-test", "\tinvalid key size\n");
			fail = TRUE;
			continue;
		}

		cipher_len = purple_cipher_encrypt(cipher,
			(const guchar*)(test->plaintext ? test->plaintext : ""),
			test->plaintext ? (strlen(test->plaintext) + 1) : 0,
			cipher_s, sizeof(cipher_s));
		if (cipher_len < 0) {
			purple_debug_info("cipher-test", "\tencryption failed\n");
			fail = TRUE;
			continue;
		}

		cipher_b16 = purple_base16_encode(cipher_s, cipher_len);

		purple_debug_info("cipher-test", "\tGot:          %s\n", cipher_b16);
		purple_debug_info("cipher-test", "\tWanted:       %s\n", test->cipher);

		if (g_strcmp0(cipher_b16, test->cipher) != 0) {
			purple_debug_info("cipher-test",
				"\tencrypted data doesn't match\n");
			g_free(cipher_b16);
			fail = TRUE;
			continue;
		}
		g_free(cipher_b16);

		decipher_len = purple_cipher_decrypt(cipher,
			cipher_s, cipher_len, decipher_s, sizeof(decipher_s));
		if (decipher_len < 0) {
			purple_debug_info("cipher-test", "\tdecryption failed\n");
			fail = TRUE;
			continue;
		}

		deciphered = (decipher_len > 0) ? (gchar*)decipher_s : NULL;

		if (g_strcmp0(deciphered, test->plaintext) != 0) {
			purple_debug_info("cipher-test",
				"\tdecrypted data doesn't match\n");
			fail = TRUE;
			continue;
		}

		purple_debug_info("cipher-test", "\tTest OK\n");
	}

	if (cipher != NULL)
		g_object_unref(cipher);

	if (fail)
		purple_debug_info("cipher-test", "AES tests FAILED\n\n");
	else
		purple_debug_info("cipher-test", "AES tests completed successfully\n\n");
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin) {
	cipher_test_md5();
	cipher_test_sha1();
	cipher_test_digest();
	cipher_test_pbkdf2();
	cipher_test_aes();

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin) {
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,								/**< type           */
	NULL,												/**< ui_requirement */
	0,													/**< flags          */
	NULL,												/**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,								/**< priority       */

	"core-cipher-test",									/**< id             */
	N_("Cipher Test"),									/**< name           */
	DISPLAY_VERSION,										/**< version        */
														/**  summary        */
	N_("Tests the ciphers that ship with libpurple."),
														/**  description    */
	N_("Tests the ciphers that ship with libpurple."),
	"Gary Kramlich <amc_grim@users.sf.net>",			/**< author         */
	PURPLE_WEBSITE,										/**< homepage       */

	plugin_load,										/**< load           */
	plugin_unload,										/**< unload         */
	NULL,												/**< destroy        */

	NULL,												/**< ui_info        */
	NULL,												/**< extra_info     */
	NULL,
	NULL,
	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin) {
}

PURPLE_INIT_PLUGIN(cipher_test, init_plugin, info)
