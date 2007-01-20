/*
 * A plugin to test the ciphers that ship with gaim
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
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
cipher_test_md5() {
	GaimCipher *cipher;
	GaimCipherContext *context;
	gchar digest[33];
	gboolean ret;
	gint i = 0;

	cipher = gaim_ciphers_find_cipher("md5");
	if(!cipher) {
		gaim_debug_info("cipher-test",
						"could not find md5 cipher, not testing\n");
		return;
	}

	gaim_debug_info("cipher-test", "Running md5 tests\n");

	context = gaim_cipher_context_new(cipher, NULL);

	while(md5_tests[i].answer) {
		gaim_debug_info("cipher-test", "Test %02d:\n", i);
		gaim_debug_info("cipher-test", "Testing '%s'\n", md5_tests[i].question);

		gaim_cipher_context_append(context, (guchar *)md5_tests[i].question,
								   strlen(md5_tests[i].question));

		ret = gaim_cipher_context_digest_to_str(context, sizeof(digest),
												digest, NULL);

		if(!ret) {
			gaim_debug_info("cipher-test", "failed\n");
		} else {
			gaim_debug_info("cipher-test", "\tGot:    %s\n", digest);
			gaim_debug_info("cipher-test", "\tWanted: %s\n",
							md5_tests[i].answer);
		}

		gaim_cipher_context_reset(context, NULL);
		i++;
	}

	gaim_cipher_context_destroy(context);

	gaim_debug_info("cipher-test", "md5 tests completed\n\n");
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
cipher_test_sha1() {
	GaimCipher *cipher;
	GaimCipherContext *context;
	gchar digest[41];
	gint i = 0;
	gboolean ret;

	cipher = gaim_ciphers_find_cipher("sha1");
	if(!cipher) {
		gaim_debug_info("cipher-test",
						"could not find sha1 cipher, not testing\n");
		return;
	}

	gaim_debug_info("cipher-test", "Running sha1 tests\n");

	context = gaim_cipher_context_new(cipher, NULL);

	while(sha1_tests[i].answer) {
		gaim_debug_info("cipher-test", "Test %02d:\n", i);
		gaim_debug_info("cipher-test", "Testing '%s'\n",
						(sha1_tests[i].question != NULL) ?
						sha1_tests[i].question : "'a'x1000, 1000 times");

		if(sha1_tests[i].question) {
			gaim_cipher_context_append(context, (guchar *)sha1_tests[i].question,
									   strlen(sha1_tests[i].question));
		} else {
			gint j;
			guchar buff[1000];

			memset(buff, 'a', 1000);

			for(j = 0; j < 1000; j++)
				gaim_cipher_context_append(context, buff, 1000);
		}

		ret = gaim_cipher_context_digest_to_str(context, sizeof(digest),
												digest, NULL);

		if(!ret) {
			gaim_debug_info("cipher-test", "failed\n");
		} else {
			gaim_debug_info("cipher-test", "\tGot:    %s\n", digest);
			gaim_debug_info("cipher-test", "\tWanted: %s\n",
							sha1_tests[i].answer);
		}

		gaim_cipher_context_reset(context, NULL);
		i++;
	}

	gaim_cipher_context_destroy(context);

	gaim_debug_info("cipher-test", "sha1 tests completed\n\n");
}

static void
cipher_test_digest()
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

	gaim_debug_info("cipher-test", "Running HTTP Digest tests\n");

	session_key = gaim_cipher_http_digest_calculate_session_key(
						algorithm, username, realm, password,
						nonce, client_nonce);

	if (session_key == NULL)
	{
		gaim_debug_info("cipher-test",
						"gaim_cipher_http_digest_calculate_session_key failed.\n");
	}
	else
	{
		gchar *response;

		gaim_debug_info("cipher-test", "\tsession_key: Got:    %s\n", session_key);
		gaim_debug_info("cipher-test", "\tsession_key: Wanted: %s\n", "939e7578ed9e3c518a452acee763bce9");

		response = gaim_cipher_http_digest_calculate_response(
				algorithm, method, digest_uri, qop, entity,
				nonce, nonce_count, client_nonce, session_key);

		g_free(session_key);

		if (response == NULL)
		{
			gaim_debug_info("cipher-test",
							"gaim_cipher_http_digest_calculate_session_key failed.\n");
		}
		else
		{
			gaim_debug_info("cipher-test", "\tresponse: Got:    %s\n", response);
			gaim_debug_info("cipher-test", "\tresponse: Wanted: %s\n", "6629fae49393a05397450978507c4ef1");
			g_free(response);
		}
	}

	gaim_debug_info("cipher-test", "HTTP Digest tests completed\n\n");
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin) {
	cipher_test_md5();
	cipher_test_sha1();
	cipher_test_digest();

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin) {
	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,								/**< type           */
	NULL,												/**< ui_requirement */
	0,													/**< flags          */
	NULL,												/**< dependencies   */
	GAIM_PRIORITY_DEFAULT,								/**< priority       */

	"core-cipher-test",									/**< id             */
	N_("Cipher Test"),									/**< name           */
	VERSION,											/**< version        */
														/**  summary        */
	N_("Tests the ciphers that ship with gaim."),
														/**  description    */
	N_("Tests the ciphers that ship with gaim."),
	"Gary Kramlich <amc_grim@users.sf.net>",			/**< author         */
	GAIM_WEBSITE,										/**< homepage       */

	plugin_load,										/**< load           */
	plugin_unload,										/**< unload         */
	NULL,												/**< destroy        */

	NULL,												/**< ui_info        */
	NULL,												/**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin) {
}

GAIM_INIT_PLUGIN(cipher_test, init_plugin, info)
