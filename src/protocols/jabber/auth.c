/*
 * gaim - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"

#include "jutil.h"
#include "auth.h"
#include "xmlnode.h"
#include "jabber.h"
#include "iq.h"
#include "sha.h"

#include "debug.h"
#include "md5.h"
#include "util.h"
#include "sslconn.h"


void
jabber_auth_start(JabberStream *js, xmlnode *packet)
{
	xmlnode *mechs, *mechnode;
	xmlnode *starttls;
	xmlnode *auth;

	gboolean digest_md5 = FALSE, plain=FALSE;

	if((starttls = xmlnode_get_child(packet, "starttls"))) {
		if(gaim_account_get_bool(js->gc->account, "use_tls", TRUE) &&
						gaim_ssl_is_supported()) {
			jabber_send_raw(js,
					"<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>", -1);
			return;
		} else if(xmlnode_get_child(starttls, "required")) {
			gaim_connection_error(js->gc, _("Server requires SSL for login"));
			return;
		}
	}

	mechs = xmlnode_get_child(packet, "mechanisms");

	if(!mechs) {
		gaim_connection_error(js->gc, _("Invalid response from server"));
		return;
	}

	for(mechnode = mechs->child; mechnode; mechnode = mechnode->next)
	{
		if(mechnode->type == NODE_TYPE_TAG) {
			char *mech_name = xmlnode_get_data(mechnode);
			if(mech_name && !strcmp(mech_name, "DIGEST-MD5"))
				digest_md5 = TRUE;
			else if(mech_name && !strcmp(mech_name, "PLAIN"))
				plain = TRUE;
			g_free(mech_name);
		}
	}

	auth = xmlnode_new("auth");
	xmlnode_set_attrib(auth, "xmlns", "urn:ietf:params:xml:ns:xmpp-sasl");
	if(0 && digest_md5) {
		xmlnode_set_attrib(auth, "mechanism", "DIGEST-MD5");
		js->auth_type = JABBER_AUTH_DIGEST_MD5;
		/*
	} else if(plain) {
		xmlnode_set_attrib(auth, "mechanism", "PLAIN");
		xmlnode_insert_data(auth, "\0", 1);
		xmlnode_insert_data(auth, js->user->node, -1);
		xmlnode_insert_data(auth, "\0", 1);
		xmlnode_insert_data(auth, gaim_account_get_password(js->gc->account),
				-1);
		js->auth_type = JABBER_AUTH_PLAIN;
		*/
	} else {
		gaim_connection_error(js->gc,
				_("Server does not use any supported authentication method"));
		xmlnode_free(auth);
		return;
	}
	jabber_send(js, auth);
	xmlnode_free(auth);
}

static void auth_old_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "error")) {
		xmlnode *error = xmlnode_get_child(packet, "error");
		const char *err_code;
		char *err_text;
		char *buf;

		err_code = xmlnode_get_attrib(error, "code");
		err_text = xmlnode_get_data(error);

		if(!err_code)
			err_code = "";
		if(!err_text)
			err_text = g_strdup(_("Unknown"));

		if(!strcmp(err_code, "401"))
			js->gc->wants_to_die = TRUE;

		buf = g_strdup_printf("Error %s: %s",
				err_code, err_text);

		gaim_connection_error(js->gc, buf);
		g_free(err_text);
		g_free(buf);
	}
	jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
}

static void auth_old_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberIq *iq;
	xmlnode *query, *x;
	gboolean digest = FALSE;
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *pw = gaim_account_get_password(js->gc->account);

	if(!type) {
		return;
	} else if(!strcmp(type, "error")) {
		/* XXX: still need to handle XMPP-style errors */
		xmlnode *error;
		char *buf, *err_txt = NULL;
		const char *code = NULL;
		if((error = xmlnode_get_child(packet, "error"))) {
			code = xmlnode_get_attrib(error, "code");
			err_txt = xmlnode_get_data(error);
		}
		buf = g_strdup_printf("%s%s%s", code ? code : "", code ? ": " : "",
				err_txt ? err_txt : _("Unknown Error"));
		gaim_connection_error(js->gc, buf);
		if(err_txt)
			g_free(err_txt);
		g_free(buf);
	} else if(!strcmp(type, "result")) {
		query = xmlnode_get_child(packet, "query");
		if(js->stream_id && xmlnode_get_child(query, "digest")) {
			digest = TRUE;
		} else if(!xmlnode_get_child(query, "password")) {
			gaim_connection_error(js->gc,
					_("Server does not use any supported authentication method"));
			return;
		}

		iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:auth");
		query = xmlnode_get_child(iq->node, "query");
		x = xmlnode_new_child(query, "username");
		xmlnode_insert_data(x, js->user->node, -1);
		x = xmlnode_new_child(query, "resource");
		xmlnode_insert_data(x, js->user->resource, -1);

		if(digest) {
			unsigned char hashval[20];
			char *s, h[41], *p;
			int i;

			x = xmlnode_new_child(query, "digest");
			s = g_strdup_printf("%s%s", js->stream_id, pw);
			shaBlock((unsigned char *)s, strlen(s), hashval);
			p = h;
			for(i=0; i<20; i++, p+=2)
				snprintf(p, 3, "%02x", hashval[i]);
			xmlnode_insert_data(x, h, -1);
			g_free(s);
		} else {
			x = xmlnode_new_child(query, "password");
			xmlnode_insert_data(x, pw, -1);
		}

		jabber_iq_set_callback(iq, auth_old_result_cb, NULL);

		jabber_iq_send(iq);
	}
}

void jabber_auth_start_old(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query, *username;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:auth");

	query = xmlnode_get_child(iq->node, "query");
	username = xmlnode_new_child(query, "username");
	xmlnode_insert_data(username, js->user->node, -1);

	jabber_iq_set_callback(iq, auth_old_cb, NULL);

	jabber_iq_send(iq);
}

static GHashTable* parse_challenge(const char *challenge)
{
	GHashTable *ret = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	char **pairs;
	int i;

	pairs = g_strsplit(challenge, ",", -1);

	for(i=0; pairs[i]; i++) {
		char **keyval = g_strsplit(pairs[i], "=", 2);
		if(keyval[0] && keyval[1]) {
			if(keyval[1][0] == '"' && keyval[1][strlen(keyval[1])-1] == '"')
				g_hash_table_replace(ret, g_strdup(keyval[0]), g_strndup(keyval[1]+1, strlen(keyval[1])-2));
			else
				g_hash_table_replace(ret, g_strdup(keyval[0]), g_strdup(keyval[1]));
		}
		g_strfreev(keyval);
	}

	g_strfreev(pairs);

	return ret;
}

static unsigned char*
generate_response_value(JabberID *jid, const char *passwd, const char *nonce,
		const char *cnonce, const char *a2, const char *realm)
{
	md5_state_t ctx;
	md5_byte_t result[16];

	char *x, *y, *a1, *ha1, *ha2, *kd, *z;

	x = g_strdup_printf("%s:%s:%s", jid->node, realm, passwd);
	md5_init(&ctx);
	md5_append(&ctx, x, strlen(x));
	md5_finish(&ctx, result);

	y = g_strndup(result, 16);

	a1 = g_strdup_printf("%s:%s:%s:%s@%s", y, nonce, cnonce, jid->node,
			jid->domain);

	md5_init(&ctx);
	md5_append(&ctx, a1, strlen(a1));
	md5_finish(&ctx, result);

	ha1 = gaim_base16_encode(result, 16);

	md5_init(&ctx);
	md5_append(&ctx, a2, strlen(a2));
	md5_finish(&ctx, result);

	ha2 = gaim_base16_encode(result, 16);

	kd = g_strdup_printf("%s:%s:00000001:%s:auth:%s", ha1, nonce, cnonce, ha2);

	md5_init(&ctx);
	md5_append(&ctx, kd, strlen(kd));
	md5_finish(&ctx, result);

	z = gaim_base16_encode(result, 16);

	g_free(x);
	g_free(y);
	g_free(a1);
	g_free(ha1);
	g_free(ha2);
	g_free(kd);

	return z;
}

void
jabber_auth_handle_challenge(JabberStream *js, xmlnode *packet)
{

	if(js->auth_type == JABBER_AUTH_PLAIN) {
		/* XXX: implement me! */
	} else if(js->auth_type == JABBER_AUTH_DIGEST_MD5) {
		char *enc_in = xmlnode_get_data(packet);
		char *dec_in;
		char *enc_out;
		GHashTable *parts;

		if(!enc_in) {
			gaim_connection_error(js->gc, _("Invalid response from server"));
			return;
		}

		gaim_base64_decode(enc_in, &dec_in, NULL);
		gaim_debug(GAIM_DEBUG_MISC, "jabber", "decoded challenge (%d): %s\n",
				strlen(dec_in), dec_in);

		parts = parse_challenge(dec_in);


		if (g_hash_table_lookup(parts, "rspauth")) {
			char *rspauth = g_hash_table_lookup(parts, "rspauth");


			if(rspauth && js->expected_rspauth &&
					!strcmp(rspauth, js->expected_rspauth)) {
				jabber_send_raw(js,
						"<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl' />",
						-1);
			} else {
				gaim_connection_error(js->gc, _("Invalid challenge from server"));
			}
			g_free(js->expected_rspauth);
		} else {
			/* assemble a response, and send it */
			/* see RFC 2831 */
			GString *response = g_string_new("");
			char *a2;
			char *auth_resp;
			char *buf;
			char *cnonce;
			char *realm;
			char *nonce;

			/* we're actually supposed to prompt the user for a realm if
			 * the server doesn't send one, but that really complicates things,
			 * so i'm not gonna worry about it until is poses a problem to
			 * someone, or I get really bored */
			realm = g_hash_table_lookup(parts, "realm");
			if(!realm)
				realm = js->user->domain;

			cnonce = g_strdup_printf("%x%u%x", g_random_int(), (int)time(NULL),
					g_random_int());
			nonce = g_hash_table_lookup(parts, "nonce");


			a2 = g_strdup_printf("AUTHENTICATE:xmpp/%s", realm);
			auth_resp = generate_response_value(js->user,
					gaim_account_get_password(js->gc->account), nonce, cnonce, a2, realm);
			g_free(a2);

			a2 = g_strdup_printf(":xmpp/%s", realm);
			js->expected_rspauth = generate_response_value(js->user,
					gaim_account_get_password(js->gc->account), nonce, cnonce, a2, realm);
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
			g_string_append_printf(response, ",authzid=\"%s@%s\"",
					js->user->node, js->user->domain);

			g_free(auth_resp);
			g_free(cnonce);

			enc_out = gaim_base64_encode(response->str, response->len);

			gaim_debug(GAIM_DEBUG_MISC, "jabber", "decoded response (%d): %s\n", response->len, response->str);

			buf = g_strdup_printf("<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>%s</response>", enc_out);

			jabber_send_raw(js, buf, -1);

			g_free(buf);

			g_free(enc_out);

			g_string_free(response, TRUE);
		}

		g_free(enc_in);
		g_free(dec_in);
		g_hash_table_destroy(parts);
	}
}

void jabber_auth_handle_success(JabberStream *js, xmlnode *packet)
{
	const char *ns = xmlnode_get_attrib(packet, "xmlns");

	if(!ns || strcmp(ns, "urn:ietf:params:xml:ns:xmpp-sasl")) {
		gaim_connection_error(js->gc, _("Invalid response from server"));
		return;
	}

	jabber_stream_set_state(js, JABBER_STREAM_REINITIALIZING);
}

void jabber_auth_handle_failure(JabberStream *js, xmlnode *packet)
{
	const char *ns = xmlnode_get_attrib(packet, "xmlns");

	if(!ns)
		gaim_connection_error(js->gc, _("Invalid response from server"));
	else if(!strcmp(ns, "urn:ietf:params:xml:ns:xmpp-sasl")) {
		if(xmlnode_get_child(packet, "bad-protocol")) {
			gaim_connection_error(js->gc, _("Bad Protocol"));
		} else if(xmlnode_get_child(packet, "encryption-required")) {
			js->gc->wants_to_die = TRUE;
			gaim_connection_error(js->gc, _("Encryption Required"));
		} else if(xmlnode_get_child(packet, "invalid-authzid")) {
			js->gc->wants_to_die = TRUE;
			gaim_connection_error(js->gc, _("Invalid authzid"));
		} else if(xmlnode_get_child(packet, "invalid-mechanism")) {
			js->gc->wants_to_die = TRUE;
			gaim_connection_error(js->gc, _("Invalid Mechanism"));
		} else if(xmlnode_get_child(packet, "invalid-realm")) {
			gaim_connection_error(js->gc, _("Invalid Realm"));
		} else if(xmlnode_get_child(packet, "mechanism-too-weak")) {
			js->gc->wants_to_die = TRUE;
			gaim_connection_error(js->gc, _("Mechanism Too Weak"));
		} else if(xmlnode_get_child(packet, "not-authorized")) {
			js->gc->wants_to_die = TRUE;
			gaim_connection_error(js->gc, _("Not Authorized"));
		} else if(xmlnode_get_child(packet, "temporary-auth-failure")) {
			gaim_connection_error(js->gc,
					_("Temporary Authentication Failure"));
		} else {
			gaim_connection_error(js->gc, _("Authentication Failure"));
		}
	}
}
