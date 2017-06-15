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

#include "debug.h"
#include "util.h"
#include "xmlnode.h"

#include "auth_digest_md5.h"
#include "auth.h"
#include "jabber.h"

static JabberSaslState
digest_md5_start(JabberStream *js, PurpleXmlNode *packet, PurpleXmlNode **response,
                 char **error)
{
	PurpleXmlNode *auth = purple_xmlnode_new("auth");
	purple_xmlnode_set_namespace(auth, NS_XMPP_SASL);
	purple_xmlnode_set_attrib(auth, "mechanism", "DIGEST-MD5");

	*response = auth;
	return JABBER_SASL_STATE_CONTINUE;
}

/* Parts of this algorithm are inspired by stuff in libgsasl */
GHashTable* jabber_auth_digest_md5_parse(const char *challenge)
{
	const char *token_start, *val_start, *val_end, *cur;
	GHashTable *ret = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);

	cur = challenge;
	while(*cur != '\0') {
		/* Find the end of the token */
		gboolean in_quotes = FALSE;
		char *name, *value = NULL;
		token_start = cur;
		while(*cur != '\0' && (in_quotes || (!in_quotes && *cur != ','))) {
			if (*cur == '"')
				in_quotes = !in_quotes;
			cur++;
		}

		/* Find start of value.  */
		val_start = strchr(token_start, '=');
		if (val_start == NULL || val_start > cur)
			val_start = cur;

		if (token_start != val_start) {
			name = g_strndup(token_start, val_start - token_start);

			if (val_start != cur) {
				val_start++;
				while (val_start != cur && (*val_start == ' ' || *val_start == '\t'
						|| *val_start == '\r' || *val_start == '\n'
						|| *val_start == '"'))
					val_start++;

				val_end = cur;
				while (val_end >= val_start && (*val_end == ' ' || *val_end == ',' || *val_end == '\t'
						|| *val_end == '\r' || *val_end == '\n'
						|| *val_end == '"'  || *val_end == '\0'))
					val_end--;

				if (val_end - val_start + 1 >= 0)
					value = g_strndup(val_start, val_end - val_start + 1);
			}

			g_hash_table_replace(ret, name, value);
		}

		/* Find the start of the next token, if there is one */
		if (*cur != '\0') {
			cur++;
			while (*cur == ' ' || *cur == ',' || *cur == '\t'
					|| *cur == '\r' || *cur == '\n')
				cur++;
		}
	}

	return ret;
}

static char *
generate_response_value(JabberID *jid, const char *passwd, const char *nonce,
		const char *cnonce, const char *a2, const char *realm)
{
	GChecksum *hash;
	gsize digest_len = 16;

	gchar *a1, *convnode=NULL, *convpasswd = NULL, *ha1, *ha2, *kd, *x, *z;

	if((convnode = g_convert(jid->node, -1, "iso-8859-1", "utf-8",
					NULL, NULL, NULL)) == NULL) {
		convnode = g_strdup(jid->node);
	}
	if(passwd && ((convpasswd = g_convert(passwd, -1, "iso-8859-1",
						"utf-8", NULL, NULL, NULL)) == NULL)) {
		convpasswd = g_strdup(passwd);
	}

	hash = g_checksum_new(G_CHECKSUM_MD5);

	x = g_strdup_printf("%s:%s:%s", convnode, realm, convpasswd ? convpasswd : "");
	g_checksum_update(hash, (const guchar *)x, -1);

	a1 = g_strdup_printf("xxxxxxxxxxxxxxxx:%s:%s", nonce, cnonce);

	g_checksum_get_digest(hash, (guint8 *)a1, &digest_len);
	g_checksum_free(hash);

	ha1 = g_compute_checksum_for_string(G_CHECKSUM_MD5, a1, -1);
	ha2 = g_compute_checksum_for_string(G_CHECKSUM_MD5, a2, -1);

	kd = g_strdup_printf("%s:%s:00000001:%s:auth:%s", ha1, nonce, cnonce, ha2);

	z = g_compute_checksum_for_string(G_CHECKSUM_MD5, kd, -1);

	g_free(convnode);
	g_free(convpasswd);
	g_free(x);
	g_free(a1);
	g_free(ha1);
	g_free(ha2);
	g_free(kd);

	return z;
}

static JabberSaslState
digest_md5_handle_challenge(JabberStream *js, PurpleXmlNode *packet,
                            PurpleXmlNode **response, char **msg)
{
	PurpleXmlNode *reply = NULL;
	char *enc_in = purple_xmlnode_get_data(packet);
	char *dec_in;
	char *enc_out;
	gsize size = 0;
	GHashTable *parts;
	JabberSaslState state = JABBER_SASL_STATE_CONTINUE;

	if (!enc_in) {
		*msg = g_strdup(_("Invalid response from server"));
		return JABBER_SASL_STATE_FAIL;
	}

	dec_in = (char *)g_base64_decode(enc_in, &size);
	purple_debug_misc("jabber", "decoded challenge (%"
			G_GSIZE_FORMAT "): %s\n",
			size,
			dec_in);

	parts = jabber_auth_digest_md5_parse(dec_in);

	if (g_hash_table_lookup(parts, "rspauth")) {
		char *rspauth = g_hash_table_lookup(parts, "rspauth");
		char *expected_rspauth = js->auth_mech_data;

		if (rspauth && purple_strequal(rspauth, expected_rspauth)) {
			reply = purple_xmlnode_new("response");
			purple_xmlnode_set_namespace(reply, NS_XMPP_SASL);
		} else {
			*msg = g_strdup(_("Invalid challenge from server"));
			state = JABBER_SASL_STATE_FAIL;
		}
		g_free(js->auth_mech_data);
		js->auth_mech_data = NULL;
	} else {
		/* assemble a response, and send it */
		/* see RFC 2831 */
		char *realm;
		char *nonce;

		/* Make sure the auth string contains everything that should be there.
		   This isn't everything in RFC2831, but it is what we need. */

		nonce = g_hash_table_lookup(parts, "nonce");

		/* we're actually supposed to prompt the user for a realm if
		 * the server doesn't send one, but that really complicates things,
		 * so i'm not gonna worry about it until is poses a problem to
		 * someone, or I get really bored */
		realm = g_hash_table_lookup(parts, "realm");
		if(!realm)
			realm = js->user->domain;

		if (nonce == NULL || realm == NULL) {
			*msg = g_strdup(_("Invalid challenge from server"));
			state = JABBER_SASL_STATE_FAIL;
		} else {
			GString *response = g_string_new("");
			char *a2;
			char *auth_resp;
			char *cnonce;

			cnonce = g_strdup_printf("%x%u%x", g_random_int(), (int)time(NULL),
					g_random_int());

			a2 = g_strdup_printf("AUTHENTICATE:xmpp/%s", realm);
			auth_resp = generate_response_value(js->user,
					purple_connection_get_password(js->gc), nonce, cnonce, a2, realm);
			g_free(a2);

			a2 = g_strdup_printf(":xmpp/%s", realm);
			js->auth_mech_data = generate_response_value(js->user,
					purple_connection_get_password(js->gc), nonce, cnonce, a2, realm);
			g_free(a2);

			g_string_append_printf(response, "username=\"%s\"", js->user->node);
			g_string_append_printf(response, ",realm=\"%s\"", realm);
			g_string_append_printf(response, ",nonce=\"%s\"", nonce);
			g_string_append_printf(response, ",cnonce=\"%s\"", cnonce);
			g_string_append_printf(response, ",nc=00000001");
			g_string_append_printf(response, ",qop=auth");
			g_string_append_printf(response, ",digest-uri=\"xmpp/%s\"", realm);
			g_string_append_printf(response, ",response=%s", auth_resp);
			g_string_append_printf(response, ",charset=utf-8");

			g_free(auth_resp);
			g_free(cnonce);

			enc_out = g_base64_encode((guchar *)response->str, response->len);

			purple_debug_misc("jabber", "decoded response (%"
					G_GSIZE_FORMAT "): %s\n",
					response->len, response->str);

			reply = purple_xmlnode_new("response");
			purple_xmlnode_set_namespace(reply, NS_XMPP_SASL);
			purple_xmlnode_insert_data(reply, enc_out, -1);

			g_free(enc_out);

			g_string_free(response, TRUE);
		}
	}

	g_free(enc_in);
	g_free(dec_in);
	g_hash_table_destroy(parts);

	*response = reply;
	return state;
}

static void
digest_md5_dispose(JabberStream *js)
{
	g_free(js->auth_mech_data);
	js->auth_mech_data = NULL;
}

static JabberSaslMech digest_md5_mech = {
	10, /* priority */
	"DIGEST-MD5", /* name */
	digest_md5_start,
	digest_md5_handle_challenge,
	NULL, /* handle_success */
	NULL, /* handle_failure */
	digest_md5_dispose,
};

JabberSaslMech *jabber_auth_get_digest_md5_mech(void)
{
	return &digest_md5_mech;
}
