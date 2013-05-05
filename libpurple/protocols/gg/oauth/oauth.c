/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Code adapted from libgadu (C) 2008 Wojtek Kaniewski <wojtekka@irc.pl>
 * (http://toxygen.net/libgadu/) during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
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

#include "oauth.h"

#include "oauth-parameter.h"
#include <cipher.h>

char *gg_oauth_static_nonce;		/* dla unit testów */
char *gg_oauth_static_timestamp;	/* dla unit testów */

static void gg_oauth_generate_nonce(char *buf, int len)
{
	const char charset[] = "0123456789";

	if (buf == NULL || len < 1)
		return;

	while (len > 1) {
		*buf++ = charset[(unsigned) (((float) sizeof(charset) - 1.0) * rand() / (RAND_MAX + 1.0))];
		len--;
	}

	*buf = 0;
}

static gchar *gg_hmac_sha1(const char *key, const char *message)
{
	PurpleCipherContext *context;
	guchar digest[20];
	
	context = purple_cipher_context_new_by_name("hmac", NULL);
	purple_cipher_context_set_option(context, "hash", "sha1");
	purple_cipher_context_set_key(context, (guchar *)key, strlen(key));
	purple_cipher_context_append(context, (guchar *)message, strlen(message));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	purple_cipher_context_destroy(context);
	
	return purple_base64_encode(digest, sizeof(digest));
}

static char *gg_oauth_generate_signature(const char *method, const char *url, const char *request, const char *consumer_secret, const char *token_secret)
{
	char *text, *key, *res;
	gchar *url_e, *request_e, *consumer_secret_e, *token_secret_e;

	url_e = g_uri_escape_string(url, "?", FALSE);
	g_strdelimit(url_e, "?", '\0');
	request_e = g_uri_escape_string(request, NULL, FALSE);
	text = g_strdup_printf("%s&%s&%s", method, url_e, request_e);
	g_free(url_e);
	g_free(request_e);

	consumer_secret_e = g_uri_escape_string(consumer_secret, NULL, FALSE);
	token_secret_e = token_secret ? g_uri_escape_string(token_secret, NULL, FALSE) : NULL;
	key = g_strdup_printf("%s&%s", consumer_secret_e, token_secret ? token_secret_e : "");
	g_free(consumer_secret_e);
	g_free(token_secret_e);

	res = gg_hmac_sha1(key, text);

	free(key);
	free(text);

	return res;
}

char *gg_oauth_generate_header(const char *method, const char *url, const const char *consumer_key, const char *consumer_secret, const char *token, const char *token_secret)
{
	char *request, *signature, *res;
	char nonce[80], timestamp[16];
	gg_oauth_parameter_t *params = NULL;

	if (gg_oauth_static_nonce == NULL)
		gg_oauth_generate_nonce(nonce, sizeof(nonce));
	else {
		strncpy(nonce, gg_oauth_static_nonce, sizeof(nonce) - 1);
		nonce[sizeof(nonce) - 1] = 0;
	}

	if (gg_oauth_static_timestamp == NULL)
		snprintf(timestamp, sizeof(timestamp), "%ld", time(NULL));
	else {
		strncpy(timestamp, gg_oauth_static_timestamp, sizeof(timestamp) - 1);
		timestamp[sizeof(timestamp) - 1] = 0;
	}

	gg_oauth_parameter_set(&params, "oauth_consumer_key", consumer_key);
	gg_oauth_parameter_set(&params, "oauth_nonce", nonce);
	gg_oauth_parameter_set(&params, "oauth_signature_method", "HMAC-SHA1");
	gg_oauth_parameter_set(&params, "oauth_timestamp", timestamp);
	gg_oauth_parameter_set(&params, "oauth_token", token);
	gg_oauth_parameter_set(&params, "oauth_version", "1.0");

	request = gg_oauth_parameter_join(params, 0);

	signature = gg_oauth_generate_signature(method, url, request, consumer_secret, token_secret);

	free(request);

	gg_oauth_parameter_free(params);
	params = NULL;

	if (signature == NULL)
		return NULL;

	gg_oauth_parameter_set(&params, "oauth_version", "1.0");
	gg_oauth_parameter_set(&params, "oauth_nonce", nonce);
	gg_oauth_parameter_set(&params, "oauth_timestamp", timestamp);
	gg_oauth_parameter_set(&params, "oauth_consumer_key", consumer_key);
	gg_oauth_parameter_set(&params, "oauth_token", token);
	gg_oauth_parameter_set(&params, "oauth_signature_method", "HMAC-SHA1");
	gg_oauth_parameter_set(&params, "oauth_signature", signature);

	free(signature);

	res = gg_oauth_parameter_join(params, 1);

	gg_oauth_parameter_free(params);

	return res;
}

