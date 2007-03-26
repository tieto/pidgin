/*
 * purple - Jabber Protocol Plugin
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

#include "account.h"
#include "debug.h"
#include "cipher.h"
#include "conversation.h"
#include "request.h"
#include "sslconn.h"
#include "util.h"
#include "xmlnode.h"

#include "jutil.h"
#include "auth.h"
#include "jabber.h"
#include "iq.h"

static void auth_old_result_cb(JabberStream *js, xmlnode *packet,
		gpointer data);

gboolean
jabber_process_starttls(JabberStream *js, xmlnode *packet)
{
	xmlnode *starttls;

	if((starttls = xmlnode_get_child(packet, "starttls"))) {
		if(purple_ssl_is_supported()) {
			jabber_send_raw(js,
					"<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>", -1);
			return TRUE;
		} else if(xmlnode_get_child(starttls, "required")) {
			purple_connection_error(js->gc, _("Server requires TLS/SSL for login.  No TLS/SSL support found."));
			return TRUE;
		}
	}

	return FALSE;
}

static void finish_plaintext_authentication(JabberStream *js)
{
	if(js->auth_type == JABBER_AUTH_PLAIN) {
		xmlnode *auth;
		GString *response;
		gchar *enc_out;

		auth = xmlnode_new("auth");
		xmlnode_set_namespace(auth, "urn:ietf:params:xml:ns:xmpp-sasl");
		
		xmlnode_set_attrib(auth, "xmlns:ga", "http://www.google.com/talk/protocol/auth");
		xmlnode_set_attrib(auth, "ga:client-uses-full-bind-result", "true");
		
		response = g_string_new("");
		response = g_string_append_len(response, "\0", 1);
		response = g_string_append(response, js->user->node);
		response = g_string_append_len(response, "\0", 1);
		response = g_string_append(response,
				purple_connection_get_password(js->gc));

		enc_out = purple_base64_encode((guchar *)response->str, response->len);

		xmlnode_set_attrib(auth, "mechanism", "PLAIN");
		xmlnode_insert_data(auth, enc_out, -1);
		g_free(enc_out);
		g_string_free(response, TRUE);

		jabber_send(js, auth);
		xmlnode_free(auth);
	} else if(js->auth_type == JABBER_AUTH_IQ_AUTH) {
		JabberIq *iq;
		xmlnode *query, *x;

		iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:auth");
		query = xmlnode_get_child(iq->node, "query");
		x = xmlnode_new_child(query, "username");
		xmlnode_insert_data(x, js->user->node, -1);
		x = xmlnode_new_child(query, "resource");
		xmlnode_insert_data(x, js->user->resource, -1);
		x = xmlnode_new_child(query, "password");
		xmlnode_insert_data(x, purple_connection_get_password(js->gc), -1);
		jabber_iq_set_callback(iq, auth_old_result_cb, NULL);
		jabber_iq_send(iq);
	}
}

static void allow_plaintext_auth(PurpleAccount *account)
{
	purple_account_set_bool(account, "auth_plain_in_clear", TRUE);

	finish_plaintext_authentication(account->gc->proto_data);
}

static void disallow_plaintext_auth(PurpleAccount *account)
{
	purple_connection_error(account->gc, _("Server requires plaintext authentication over an unencrypted stream"));
}

#ifdef HAVE_CYRUS_SASL

static void jabber_auth_start_cyrus(JabberStream *);

/* Callbacks for Cyrus SASL */

static int jabber_sasl_cb_realm(void *ctx, int id, const char **avail, const char **result)
{
	JabberStream *js = (JabberStream *)ctx;

	if (id != SASL_CB_GETREALM || !result) return SASL_BADPARAM;

	*result = js->user->domain;

	return SASL_OK;
}

static int jabber_sasl_cb_simple(void *ctx, int id, const char **res, unsigned *len)
{
	JabberStream *js = (JabberStream *)ctx;

	switch(id) {
		case SASL_CB_AUTHNAME:
			*res = js->user->node;
			break;
		case SASL_CB_USER:
			*res = "";
			break;
		default:
			return SASL_BADPARAM;
	}
	if (len) *len = strlen((char *)*res);
	return SASL_OK;
}

static int jabber_sasl_cb_secret(sasl_conn_t *conn, void *ctx, int id, sasl_secret_t **secret)
{
	JabberStream *js = (JabberStream *)ctx;
	const char *pw = purple_account_get_password(js->gc->account);
	size_t len;
	static sasl_secret_t *x = NULL;

	if (!conn || !secret || id != SASL_CB_PASS)
		return SASL_BADPARAM;

	len = strlen(pw);
	x = (sasl_secret_t *) realloc(x, sizeof(sasl_secret_t) + len);

	if (!x)
		return SASL_NOMEM;

	x->len = len;
	strcpy((char*)x->data, pw);

	*secret = x;
	return SASL_OK;
}

static void allow_cyrus_plaintext_auth(PurpleAccount *account)
{
	purple_account_set_bool(account, "auth_plain_in_clear", TRUE);

	jabber_auth_start_cyrus(account->gc->proto_data);
}

static void jabber_auth_start_cyrus(JabberStream *js)
{
	const char *clientout = NULL, *mech = NULL;
	char *enc_out;
	unsigned coutlen = 0;
	xmlnode *auth;
	sasl_security_properties_t secprops;
	gboolean again;
	gboolean plaintext = TRUE;

	/* Set up security properties and options */
	secprops.min_ssf = 0;
	secprops.security_flags = SASL_SEC_NOANONYMOUS;

	if (!js->gsc) {
		secprops.max_ssf = -1;
		secprops.maxbufsize = 4096;
		plaintext = purple_account_get_bool(js->gc->account, "auth_plain_in_clear", FALSE);
		if (!plaintext)
			secprops.security_flags |= SASL_SEC_NOPLAINTEXT;
	} else {
		secprops.max_ssf = 0;
		secprops.maxbufsize = 0;
		plaintext = TRUE;
	}
	secprops.property_names = 0;
	secprops.property_values = 0;

	do {
		again = FALSE;

		js->sasl_state = sasl_client_new("xmpp", js->serverFQDN, NULL, NULL, js->sasl_cb, 0, &js->sasl);
		if (js->sasl_state==SASL_OK) {
			sasl_setprop(js->sasl, SASL_SEC_PROPS, &secprops);
			purple_debug_info("sasl", "Mechs found: %s\n", js->sasl_mechs->str);
			js->sasl_state = sasl_client_start(js->sasl, js->sasl_mechs->str, NULL, &clientout, &coutlen, &mech);
		}
		switch (js->sasl_state) {
			/* Success */
			case SASL_OK:
			case SASL_CONTINUE:
				break;
			case SASL_NOMECH:
				/* No mechanisms do what we want. See if we can add
				 * plaintext ones to the list. */

				if (!purple_account_get_password(js->gc->account)) {
					purple_connection_error(js->gc, _("Server couldn't authenticate you without a password"));
					return;
				} else if (!plaintext) {
					purple_request_yes_no(js->gc, _("Plaintext Authentication"),
							_("Plaintext Authentication"),
							_("This server requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
							2, js->gc->account,
							allow_cyrus_plaintext_auth,
							disallow_plaintext_auth);
					return;
				} else {
					purple_connection_error(js->gc, _("Server does not use any supported authentication method"));
					return;
				}
				/* not reached */
				break;

				/* Fatal errors. Give up and go home */
			case SASL_BADPARAM:
			case SASL_NOMEM:
				break;

				/* For everything else, fail the mechanism and try again */
			default:
				purple_debug_info("sasl", "sasl_state is %d, failing the mech and trying again\n", js->sasl_state);

				/*
				 * DAA: is this right?
				 * The manpage says that "mech" will contain the chosen mechanism on success.
				 * Presumably, if we get here that isn't the case and we shouldn't try again?
				 * I suspect that this never happens.
				 */
				/*
				 * SXW: Yes, this is right. What this handles is the situation where a
				 * mechanism, say GSSAPI, is tried. If that mechanism fails, it may be
				 * due to mechanism specific issues, so we want to try one of the other
				 * supported mechanisms. This code handles that case
				 */
				if (mech && strlen(mech) > 0) {
					char *pos;
					if ((pos = strstr(js->sasl_mechs->str, mech))) {
						g_string_erase(js->sasl_mechs, pos-js->sasl_mechs->str, strlen(mech));
					}
					again = TRUE;
				}

				sasl_dispose(&js->sasl);
		}
	} while (again);

	if (js->sasl_state == SASL_CONTINUE || js->sasl_state == SASL_OK) {
		auth = xmlnode_new("auth");
		xmlnode_set_namespace(auth, "urn:ietf:params:xml:ns:xmpp-sasl");
		xmlnode_set_attrib(auth, "mechanism", mech);
		if (clientout) {
			if (coutlen == 0) {
				xmlnode_insert_data(auth, "=", -1);
			} else {
				enc_out = purple_base64_encode((unsigned char*)clientout, coutlen);
				xmlnode_insert_data(auth, enc_out, -1);
				g_free(enc_out);
			}
		}
		jabber_send(js, auth);
		xmlnode_free(auth);
	} else {
		purple_connection_error(js->gc, "SASL authentication failed\n");
	}
}

static int
jabber_sasl_cb_log(void *context, int level, const char *message)
{
	if(level <= SASL_LOG_TRACE)
		purple_debug_info("sasl", "%s\n", message);

	return SASL_OK;
}

#endif

void
jabber_auth_start(JabberStream *js, xmlnode *packet)
{
#ifdef HAVE_CYRUS_SASL
	int id;
#else
	gboolean digest_md5 = FALSE, plain=FALSE;
#endif

	xmlnode *mechs, *mechnode;


	if(js->registration) {
		jabber_register_start(js);
		return;
	}

	mechs = xmlnode_get_child(packet, "mechanisms");

	if(!mechs) {
		purple_connection_error(js->gc, _("Invalid response from server."));
		return;
	}

#ifdef HAVE_CYRUS_SASL
	js->sasl_mechs = g_string_new("");
#endif

	for(mechnode = xmlnode_get_child(mechs, "mechanism"); mechnode;
			mechnode = xmlnode_get_next_twin(mechnode))
	{
		char *mech_name = xmlnode_get_data(mechnode);
#ifdef HAVE_CYRUS_SASL
		g_string_append(js->sasl_mechs, mech_name);
		g_string_append_c(js->sasl_mechs, ' ');
#else
		if(mech_name && !strcmp(mech_name, "DIGEST-MD5"))
			digest_md5 = TRUE;
		else if(mech_name && !strcmp(mech_name, "PLAIN"))
			plain = TRUE;
#endif
		g_free(mech_name);
	}

#ifdef HAVE_CYRUS_SASL
	js->auth_type = JABBER_AUTH_CYRUS;

	/* Set up our callbacks structure */
	js->sasl_cb = g_new0(sasl_callback_t,6);

	id = 0;
	js->sasl_cb[id].id = SASL_CB_GETREALM;
	js->sasl_cb[id].proc = jabber_sasl_cb_realm;
	js->sasl_cb[id].context = (void *)js;
	id++;

	js->sasl_cb[id].id = SASL_CB_AUTHNAME;
	js->sasl_cb[id].proc = jabber_sasl_cb_simple;
	js->sasl_cb[id].context = (void *)js;
	id++;

	js->sasl_cb[id].id = SASL_CB_USER;
	js->sasl_cb[id].proc = jabber_sasl_cb_simple;
	js->sasl_cb[id].context = (void *)js;
	id++;

	if (purple_account_get_password(js->gc->account)) {
		js->sasl_cb[id].id = SASL_CB_PASS;
		js->sasl_cb[id].proc = jabber_sasl_cb_secret;
		js->sasl_cb[id].context = (void *)js;
		id++;
	}

	js->sasl_cb[id].id = SASL_CB_LOG;
	js->sasl_cb[id].proc = jabber_sasl_cb_log;
	js->sasl_cb[id].context = (void*)js;
	id++;

	js->sasl_cb[id].id = SASL_CB_LIST_END;

	jabber_auth_start_cyrus(js);
#else

	if(digest_md5) {
		xmlnode *auth;

		js->auth_type = JABBER_AUTH_DIGEST_MD5;
		auth = xmlnode_new("auth");
		xmlnode_set_namespace(auth, "urn:ietf:params:xml:ns:xmpp-sasl");
		xmlnode_set_attrib(auth, "mechanism", "DIGEST-MD5");

		jabber_send(js, auth);
		xmlnode_free(auth);
	} else if(plain) {
		js->auth_type = JABBER_AUTH_PLAIN;

		if(js->gsc == NULL && !purple_account_get_bool(js->gc->account, "auth_plain_in_clear", FALSE)) {
			purple_request_yes_no(js->gc, _("Plaintext Authentication"),
					_("Plaintext Authentication"),
					_("This server requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
					2, js->gc->account, allow_plaintext_auth,
					disallow_plaintext_auth);
			return;
		}
		finish_plaintext_authentication(js);
	} else {
		purple_connection_error(js->gc,
				_("Server does not use any supported authentication method"));
	}
#endif
}

static void auth_old_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "result")) {
		jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
	} else {
		char *msg = jabber_parse_error(js, packet);
		xmlnode *error;
		const char *err_code;

		if((error = xmlnode_get_child(packet, "error")) &&
					(err_code = xmlnode_get_attrib(error, "code")) &&
					!strcmp(err_code, "401")) {
			js->gc->wants_to_die = TRUE;
		}

		purple_connection_error(js->gc, msg);
		g_free(msg);
	}
}

static void auth_old_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberIq *iq;
	xmlnode *query, *x;
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *pw = purple_connection_get_password(js->gc);

	if(!type) {
		purple_connection_error(js->gc, _("Invalid response from server."));
		return;
	} else if(!strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);
		purple_connection_error(js->gc, msg);
		g_free(msg);
	} else if(!strcmp(type, "result")) {
		query = xmlnode_get_child(packet, "query");
		if(js->stream_id && xmlnode_get_child(query, "digest")) {
			unsigned char hashval[20];
			char *s, h[41], *p;
			int i;

			iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:auth");
			query = xmlnode_get_child(iq->node, "query");
			x = xmlnode_new_child(query, "username");
			xmlnode_insert_data(x, js->user->node, -1);
			x = xmlnode_new_child(query, "resource");
			xmlnode_insert_data(x, js->user->resource, -1);

			x = xmlnode_new_child(query, "digest");
			s = g_strdup_printf("%s%s", js->stream_id, pw);

			purple_cipher_digest_region("sha1", (guchar *)s, strlen(s),
									  sizeof(hashval), hashval, NULL);

			p = h;
			for(i=0; i<20; i++, p+=2)
				snprintf(p, 3, "%02x", hashval[i]);
			xmlnode_insert_data(x, h, -1);
			g_free(s);
			jabber_iq_set_callback(iq, auth_old_result_cb, NULL);
			jabber_iq_send(iq);

		} else if(xmlnode_get_child(query, "password")) {
			if(js->gsc == NULL && !purple_account_get_bool(js->gc->account,
						"auth_plain_in_clear", FALSE)) {
				purple_request_yes_no(js->gc, _("Plaintext Authentication"),
						_("Plaintext Authentication"),
						_("This server requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
						2, js->gc->account, allow_plaintext_auth,
						disallow_plaintext_auth);
				return;
			}
			finish_plaintext_authentication(js);
		} else {
			purple_connection_error(js->gc,
					_("Server does not use any supported authentication method"));
			return;
		}
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

static char *
generate_response_value(JabberID *jid, const char *passwd, const char *nonce,
		const char *cnonce, const char *a2, const char *realm)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	guchar result[16];
	size_t a1len;

	gchar *a1, *convnode=NULL, *convpasswd = NULL, *ha1, *ha2, *kd, *x, *z;

	if((convnode = g_convert(jid->node, strlen(jid->node), "iso-8859-1", "utf-8",
					NULL, NULL, NULL)) == NULL) {
		convnode = g_strdup(jid->node);
	}
	if(passwd && ((convpasswd = g_convert(passwd, strlen(passwd), "iso-8859-1",
						"utf-8", NULL, NULL, NULL)) == NULL)) {
		convpasswd = g_strdup(passwd);
	}

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);

	x = g_strdup_printf("%s:%s:%s", convnode, realm, convpasswd ? convpasswd : "");
	purple_cipher_context_append(context, (const guchar *)x, strlen(x));
	purple_cipher_context_digest(context, sizeof(result), result, NULL);

	a1 = g_strdup_printf("xxxxxxxxxxxxxxxx:%s:%s", nonce, cnonce);
	a1len = strlen(a1);
	g_memmove(a1, result, 16);

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)a1, a1len);
	purple_cipher_context_digest(context, sizeof(result), result, NULL);

	ha1 = purple_base16_encode(result, 16);

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)a2, strlen(a2));
	purple_cipher_context_digest(context, sizeof(result), result, NULL);

	ha2 = purple_base16_encode(result, 16);

	kd = g_strdup_printf("%s:%s:00000001:%s:auth:%s", ha1, nonce, cnonce, ha2);

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)kd, strlen(kd));
	purple_cipher_context_digest(context, sizeof(result), result, NULL);
	purple_cipher_context_destroy(context);

	z = purple_base16_encode(result, 16);

	g_free(convnode);
	g_free(convpasswd);
	g_free(x);
	g_free(a1);
	g_free(ha1);
	g_free(ha2);
	g_free(kd);

	return z;
}

void
jabber_auth_handle_challenge(JabberStream *js, xmlnode *packet)
{

	if(js->auth_type == JABBER_AUTH_DIGEST_MD5) {
		char *enc_in = xmlnode_get_data(packet);
		char *dec_in;
		char *enc_out;
		GHashTable *parts;

		if(!enc_in) {
			purple_connection_error(js->gc, _("Invalid response from server."));
			return;
		}

		dec_in = (char *)purple_base64_decode(enc_in, NULL);
		purple_debug(PURPLE_DEBUG_MISC, "jabber", "decoded challenge (%d): %s\n",
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
				purple_connection_error(js->gc, _("Invalid challenge from server"));
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
					purple_connection_get_password(js->gc), nonce, cnonce, a2, realm);
			g_free(a2);

			a2 = g_strdup_printf(":xmpp/%s", realm);
			js->expected_rspauth = generate_response_value(js->user,
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

			enc_out = purple_base64_encode((guchar *)response->str, response->len);

			purple_debug(PURPLE_DEBUG_MISC, "jabber", "decoded response (%d): %s\n", response->len, response->str);

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
#ifdef HAVE_CYRUS_SASL
	else if (js->auth_type == JABBER_AUTH_CYRUS) {
		char *enc_in = xmlnode_get_data(packet);
		unsigned char *dec_in;
		char *enc_out;
		const char *c_out;
		unsigned int clen;
		gsize declen;
		xmlnode *response;

		dec_in = purple_base64_decode(enc_in, &declen);

		js->sasl_state = sasl_client_step(js->sasl, (char*)dec_in, declen,
						  NULL, &c_out, &clen);
		g_free(enc_in);
		g_free(dec_in);
		if (js->sasl_state != SASL_CONTINUE && js->sasl_state != SASL_OK) {
			purple_debug_error("jabber", "Error is %d : %s\n",js->sasl_state,sasl_errdetail(js->sasl));
			purple_connection_error(js->gc, _("SASL error"));
			return;
		} else {
			response = xmlnode_new("response");
			xmlnode_set_namespace(response, "urn:ietf:params:xml:ns:xmpp-sasl");
			if (c_out) {
				enc_out = purple_base64_encode((unsigned char*)c_out, clen);
				xmlnode_insert_data(response, enc_out, -1);
				g_free(enc_out);
			}
			jabber_send(js, response);
			xmlnode_free(response);
		}
	}
#endif
}

void jabber_auth_handle_success(JabberStream *js, xmlnode *packet)
{
	const char *ns = xmlnode_get_namespace(packet);
#ifdef HAVE_CYRUS_SASL
	const int *x;
#endif

	if(!ns || strcmp(ns, "urn:ietf:params:xml:ns:xmpp-sasl")) {
		purple_connection_error(js->gc, _("Invalid response from server."));
		return;
	}

#ifdef HAVE_CYRUS_SASL
	/* The SASL docs say that if the client hasn't returned OK yet, we
	 * should try one more round against it
	 */
	if (js->sasl_state != SASL_OK) {
		char *enc_in = xmlnode_get_data(packet);
		unsigned char *dec_in = NULL;
		const char *c_out;
		unsigned int clen;
		gsize declen = 0;

		if(enc_in != NULL)
			dec_in = purple_base64_decode(enc_in, &declen);

		js->sasl_state = sasl_client_step(js->sasl, (char*)dec_in, declen, NULL, &c_out, &clen);

		g_free(enc_in);
		g_free(dec_in);

		if (js->sasl_state != SASL_OK) {
			/* This should never happen! */
			purple_connection_error(js->gc, _("Invalid response from server."));
		}
	}
	/* If we've negotiated a security layer, we need to enable it */
	sasl_getprop(js->sasl, SASL_SSF, &x);
	if (*x > 0) {
		sasl_getprop(js->sasl, SASL_MAXOUTBUF, &x);
		js->sasl_maxbuf = *x;
	}
#endif

	jabber_stream_set_state(js, JABBER_STREAM_REINITIALIZING);
}

void jabber_auth_handle_failure(JabberStream *js, xmlnode *packet)
{
	char *msg = jabber_parse_error(js, packet);

	if(!msg) {
		purple_connection_error(js->gc, _("Invalid response from server."));
	} else {
		purple_connection_error(js->gc, msg);
		g_free(msg);
	}
}
