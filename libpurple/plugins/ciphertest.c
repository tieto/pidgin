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

#include "cipher.h"
#include "debug.h"
#include "plugin.h"
#include "version.h"

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
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	gchar digest[33];
	gboolean ret;
	gint i = 0;

	cipher = purple_ciphers_find_cipher("md5");
	if(!cipher) {
		purple_debug_info("cipher-test",
						"could not find md5 cipher, not testing\n");
		return;
	}

	purple_debug_info("cipher-test", "Running md5 tests\n");

	context = purple_cipher_context_new(cipher, NULL);

	while(md5_tests[i].answer) {
		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test", "Testing '%s'\n", md5_tests[i].question);

		purple_cipher_context_append(context, (guchar *)md5_tests[i].question,
								   strlen(md5_tests[i].question));

		ret = purple_cipher_context_digest_to_str(context, digest, sizeof(digest));

		if(!ret) {
			purple_debug_info("cipher-test", "failed\n");
		} else {
			purple_debug_info("cipher-test", "\tGot:    %s\n", digest);
			purple_debug_info("cipher-test", "\tWanted: %s\n",
							md5_tests[i].answer);
		}

		purple_cipher_context_reset(context, NULL);
		i++;
	}

	purple_cipher_context_destroy(context);

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
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	gchar digest[41];
	gint i = 0;
	gboolean ret;

	cipher = purple_ciphers_find_cipher("sha1");
	if(!cipher) {
		purple_debug_info("cipher-test",
						"could not find sha1 cipher, not testing\n");
		return;
	}

	purple_debug_info("cipher-test", "Running sha1 tests\n");

	context = purple_cipher_context_new(cipher, NULL);

	while(sha1_tests[i].answer) {
		purple_debug_info("cipher-test", "Test %02d:\n", i);
		purple_debug_info("cipher-test", "Testing '%s'\n",
						(sha1_tests[i].question != NULL) ?
						sha1_tests[i].question : "'a'x1000, 1000 times");

		if(sha1_tests[i].question) {
			purple_cipher_context_append(context, (guchar *)sha1_tests[i].question,
									   strlen(sha1_tests[i].question));
		} else {
			gint j;
			guchar buff[1000];

			memset(buff, 'a', 1000);

			for(j = 0; j < 1000; j++)
				purple_cipher_context_append(context, buff, 1000);
		}

		ret = purple_cipher_context_digest_to_str(context, digest, sizeof(digest));

		if(!ret) {
			purple_debug_info("cipher-test", "failed\n");
		} else {
			purple_debug_info("cipher-test", "\tGot:    %s\n", digest);
			purple_debug_info("cipher-test", "\tWanted: %s\n",
							sha1_tests[i].answer);
		}

		purple_cipher_context_reset(context, NULL);
		i++;
	}

	purple_cipher_context_destroy(context);

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

	session_key = purple_cipher_http_digest_calculate_session_key(
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

		response = purple_cipher_http_digest_calculate_response(
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

#include <nss.h>
#include <secmod.h>
#include <pk11func.h>
#include <prerror.h>
#include <secerr.h>

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

static void
cipher_test_pbkdf2(void)
{
	PurpleCipherContext *context;
	int i = 0;
	gboolean fail = FALSE;

	purple_debug_info("cipher-test", "Running PBKDF2 tests\n");

	context = purple_cipher_context_new_by_name("pbkdf2", NULL);

	while (pbkdf2_tests[i].answer) {
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

		purple_cipher_context_set_option(context, "hash", (gpointer)test->hash);
		purple_cipher_context_set_option(context, "iter_count", GUINT_TO_POINTER(test->iter_count));
		purple_cipher_context_set_option(context, "out_len", GUINT_TO_POINTER(test->out_len));
		purple_cipher_context_set_salt(context, (const guchar*)test->salt, test->salt ? strlen(test->salt): 0);
		purple_cipher_context_set_key(context, (const guchar*)test->passphrase, strlen(test->passphrase));

		ret = purple_cipher_context_digest_to_str(context, digest, sizeof(digest));
		purple_cipher_context_reset(context, NULL);

		if (!ret) {
			purple_debug_info("cipher-test", "\tfailed\n");
			fail = TRUE;
			continue;
		}

		if (g_strcmp0(test->hash, "sha1") != 0)
			skip_nss = TRUE;
		if (test->out_len != 16 && test->out_len != 32)
			skip_nss = TRUE;

		if (!skip_nss) {
			digest_nss = cipher_pbkdf2_nss_sha1(test->passphrase,
				test->salt, test->iter_count, test->out_len);
		}

		if (!ret) {
			purple_debug_info("cipher-test", "\tnss test failed\n");
			fail = TRUE;
		}

		if (g_strcmp0(digest, test->answer) == 0 &&
			(skip_nss || g_strcmp0(digest, digest_nss) == 0)) {
			purple_debug_info("cipher-test", "\tTest OK\n");
		}
		else {
			purple_debug_info("cipher-test", "\twrong answer\n");
			fail = TRUE;
		}

		purple_debug_info("cipher-test", "\tGot:          %s\n", digest);
		if (digest_nss)
			purple_debug_info("cipher-test", "\tGot from NSS: %s\n", digest_nss);
		purple_debug_info("cipher-test", "\tWanted:       %s\n", test->answer);
	}

	purple_cipher_context_destroy(context);

	if (fail)
		purple_debug_info("cipher-test", "PBKDF2 tests FAILED\n\n");
	else
		purple_debug_info("cipher-test", "PBKDF2 tests completed successfully\n\n");
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
