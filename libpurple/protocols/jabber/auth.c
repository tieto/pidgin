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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "account.h"
#include "debug.h"
#include "cipher.h"
#include "core.h"
#include "conversation.h"
#include "request.h"
#include "sslconn.h"
#include "util.h"
#include "xmlnode.h"

#include "jutil.h"
#include "auth.h"
#include "jabber.h"
#include "iq.h"
#include "notify.h"

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
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("Server requires TLS/SSL for login.  No TLS/SSL support found."));
			return TRUE;
		} else if(purple_account_get_bool(js->gc->account, "require_tls", FALSE)) {
			purple_connection_error_reason (js->gc,
				 PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("You require encryption, but no TLS/SSL support found."));
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
	purple_connection_error_reason (account->gc,
		PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
		_("Server requires plaintext authentication over an unencrypted stream"));
}

#ifdef HAVE_CYRUS_SASL

static void jabber_auth_start_cyrus(JabberStream *);
static void jabber_sasl_build_callbacks(JabberStream *);

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

static gboolean auth_pass_generic(JabberStream *js, PurpleRequestFields *fields)
{
	const char *entry;
	gboolean remember;

	entry = purple_request_fields_get_string(fields, "password");
	remember = purple_request_fields_get_bool(fields, "remember");

	if (!entry || !*entry)
	{
		purple_notify_error(js->gc->account, NULL, _("Password is required to sign on."), NULL);
		return FALSE;
	}

	if (remember)
		purple_account_set_remember_password(js->gc->account, TRUE);

	purple_account_set_password(js->gc->account, entry);

	return TRUE;
}

static void auth_pass_cb(PurpleConnection *conn, PurpleRequestFields *fields)
{
	JabberStream *js;

	/* The password prompt dialog doesn't get disposed if the account disconnects */
	if (!PURPLE_CONNECTION_IS_VALID(conn))
		return;

	js = conn->proto_data;

	if (!auth_pass_generic(js, fields))
		return;

	/* Rebuild our callbacks as we now have a password to offer */
	jabber_sasl_build_callbacks(js);

	/* Restart our connection */
	jabber_auth_start_cyrus(js);
}

static void
auth_old_pass_cb(PurpleConnection *conn, PurpleRequestFields *fields)
{
	JabberStream *js;

	/* The password prompt dialog doesn't get disposed if the account disconnects */
	if (!PURPLE_CONNECTION_IS_VALID(conn))
		return;

	js = conn->proto_data;

	if (!auth_pass_generic(js, fields))
		return;

	/* Restart our connection */
	jabber_auth_start_old(js);
}


static void
auth_no_pass_cb(PurpleConnection *conn, PurpleRequestFields *fields)
{
	JabberStream *js;

	/* The password prompt dialog doesn't get disposed if the account disconnects */
	if (!PURPLE_CONNECTION_IS_VALID(conn))
		return;

	js = conn->proto_data;

	/* Disable the account as the user has canceled connecting */
	purple_account_set_enabled(conn->account, purple_core_get_ui(), FALSE);
}

static void jabber_auth_start_cyrus(JabberStream *js)
{
	const char *clientout = NULL;
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
			js->sasl_state = sasl_client_start(js->sasl, js->sasl_mechs->str, NULL, &clientout, &coutlen, &js->current_mech);
		}
		switch (js->sasl_state) {
			/* Success */
			case SASL_OK:
			case SASL_CONTINUE:
				break;
			case SASL_NOMECH:
				/* No mechanisms have offered to help */

				/* Firstly, if we don't have a password try
				 * to get one
				 */

				if (!purple_account_get_password(js->gc->account)) {
					purple_account_request_password(js->gc->account, G_CALLBACK(auth_pass_cb), G_CALLBACK(auth_no_pass_cb), js->gc);
					return;

				/* If we've got a password, but aren't sending
				 * it in plaintext, see if we can turn on
				 * plaintext auth
				 */
				} else if (!plaintext) {
					char *msg = g_strdup_printf(_("%s requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
							js->gc->account->username);
					purple_request_yes_no(js->gc, _("Plaintext Authentication"),
							_("Plaintext Authentication"),
							msg,
							1, js->gc->account, NULL, NULL, js->gc->account,
							allow_cyrus_plaintext_auth,
							disallow_plaintext_auth);
					g_free(msg);
					return;

				} else {
					/* We have no mechs which can work.
					 * Try falling back on the old jabber:iq:auth method. We get here if the server supports
					 * one or more sasl mechs, we are compiled with cyrus-sasl support, but we support or can connect with none of
					 * the offerred mechs. jabberd 2.0 w/ SASL and Apple's iChat Server 10.5 both handle and expect
					 * jabber:iq:auth in this situation.  iChat Server in particular offers SASL GSSAPI by default, which is often
					 * not configured on the client side, and expects a fallback to jabber:iq:auth when it (predictably) fails.
					 *
					 * Note: xep-0078 points out that using jabber:iq:auth after a sasl failure is wrong. However,
					 * I believe this refers to actual authentication failure, not a simple lack of concordant mechanisms.
					 * Doing otherwise means that simply compiling with SASL support renders the client unable to connect to servers
					 * which would connect without issue otherwise. -evands
					 */
					js->auth_type = JABBER_AUTH_IQ_AUTH;
					jabber_auth_start_old(js);
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
				if (js->current_mech && strlen(js->current_mech) > 0) {
					char *pos;
					if ((pos = strstr(js->sasl_mechs->str, js->current_mech))) {
						g_string_erase(js->sasl_mechs, pos-js->sasl_mechs->str, strlen(js->current_mech));
					}
					/* Remove space which separated this mech from the next */
					if (strlen(js->sasl_mechs->str) > 0 && ((js->sasl_mechs->str)[0] == ' ')) {
						g_string_erase(js->sasl_mechs, 0, 1);	
					}
					again = TRUE;
				}

				sasl_dispose(&js->sasl);
		}
	} while (again);

	if (js->sasl_state == SASL_CONTINUE || js->sasl_state == SASL_OK) {
		auth = xmlnode_new("auth");
		xmlnode_set_namespace(auth, "urn:ietf:params:xml:ns:xmpp-sasl");
		xmlnode_set_attrib(auth, "mechanism", js->current_mech);
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
		purple_connection_error_reason (js->gc,
			PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE,
			"SASL authentication failed\n");
	}
}

static int
jabber_sasl_cb_log(void *context, int level, const char *message)
{
	if(level <= SASL_LOG_TRACE)
		purple_debug_info("sasl", "%s\n", message);

	return SASL_OK;
}

void
jabber_sasl_build_callbacks(JabberStream *js)
{
	int id;

	/* Set up our callbacks structure */
	if (js->sasl_cb == NULL)
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

	if (purple_account_get_password(js->gc->account) != NULL ) {
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
}

#endif

void
jabber_auth_start(JabberStream *js, xmlnode *packet)
{
#ifndef HAVE_CYRUS_SASL
	gboolean digest_md5 = FALSE, plain=FALSE;
#endif

	xmlnode *mechs, *mechnode;


	if(js->registration) {
		jabber_register_start(js);
		return;
	}

	mechs = xmlnode_get_child(packet, "mechanisms");

	if(!mechs) {
		purple_connection_error_reason (js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response from server."));
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

	jabber_sasl_build_callbacks(js);

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
			char *msg = g_strdup_printf(_("%s requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
					js->gc->account->username);
			purple_request_yes_no(js->gc, _("Plaintext Authentication"),
					_("Plaintext Authentication"),
					msg,
					1,
					purple_connection_get_account(js->gc), NULL, NULL,
					purple_connection_get_account(js->gc), allow_plaintext_auth,
					disallow_plaintext_auth);
			g_free(msg);
			return;
		}
		finish_plaintext_authentication(js);
	} else {
		purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE,
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
		PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		char *msg = jabber_parse_error(js, packet, &reason);
		xmlnode *error;
		const char *err_code;

		/* FIXME: Why is this not in jabber_parse_error? */
		if((error = xmlnode_get_child(packet, "error")) &&
					(err_code = xmlnode_get_attrib(error, "code")) &&
					!strcmp(err_code, "401")) {
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			/* Clear the pasword if it isn't being saved */
			if (!purple_account_get_remember_password(js->gc->account))
				purple_account_set_password(js->gc->account, NULL);
		}

		purple_connection_error_reason (js->gc, reason, msg);
		g_free(msg);
	}
}

/*!
 * @brief Given the server challenge (message) and the key (password), calculate the HMAC-MD5 digest
 *
 * This is the crammd5 response.  Inspired by cyrus-sasl's _sasl_hmac_md5()
 */
static void
auth_hmac_md5(const char *challenge, size_t challenge_len, const char *key, size_t key_len, guchar *digest)
{
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	int i;
	/* inner padding - key XORd with ipad */
	unsigned char k_ipad[65];    
	/* outer padding - key XORd with opad */
	unsigned char k_opad[65];    

	cipher = purple_ciphers_find_cipher("md5");

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (strlen(key) > 64) {
		guchar keydigest[16];

		context = purple_cipher_context_new(cipher, NULL);
		purple_cipher_context_append(context, (const guchar *)key, strlen(key));
		purple_cipher_context_digest(context, 16, keydigest, NULL);
		purple_cipher_context_destroy(context);

		key = (char *)keydigest;
		key_len = 16;
	} 

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	memset(k_ipad, '\0', sizeof k_ipad);
	memset(k_opad, '\0', sizeof k_opad);
	memcpy(k_ipad, (void *)key, key_len);
	memcpy(k_opad, (void *)key, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/* perform inner MD5 */
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, k_ipad, 64); /* start with inner pad */
	purple_cipher_context_append(context, (const guchar *)challenge, challenge_len); /* then text of datagram */
	purple_cipher_context_digest(context, 16, digest, NULL); /* finish up 1st pass */
	purple_cipher_context_destroy(context);

	/* perform outer MD5 */	
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, k_opad, 64); /* start with outer pad */
	purple_cipher_context_append(context, digest, 16); /* then results of 1st hash */
	purple_cipher_context_digest(context, 16, digest, NULL); /* finish up 2nd pass */
	purple_cipher_context_destroy(context);
}

static void auth_old_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberIq *iq;
	xmlnode *query, *x;
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *pw = purple_connection_get_password(js->gc);

	if(!type) {
		purple_connection_error_reason (js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response from server."));
		return;
	} else if(!strcmp(type, "error")) {
		PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		char *msg = jabber_parse_error(js, packet, &reason);
		purple_connection_error_reason (js->gc, reason, msg);
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

		} else if(js->stream_id && xmlnode_get_child(query, "crammd5")) {
			const char *challenge;
			guchar digest[16];
			char h[33], *p;
			int i;

			challenge = xmlnode_get_attrib(xmlnode_get_child(query, "crammd5"), "challenge");
			auth_hmac_md5(challenge, strlen(challenge), pw, strlen(pw), digest);

			/* Create the response query */
			iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:auth");
			query = xmlnode_get_child(iq->node, "query");

			x = xmlnode_new_child(query, "username");
			xmlnode_insert_data(x, js->user->node, -1);
			x = xmlnode_new_child(query, "resource");
			xmlnode_insert_data(x, js->user->resource, -1);

			x = xmlnode_new_child(query, "crammd5");

			/* Translate the digest to a hexadecimal notation */
			p = h;
			for(i=0; i<16; i++, p+=2)
				snprintf(p, 3, "%02x", digest[i]);
			xmlnode_insert_data(x, h, -1);

			jabber_iq_set_callback(iq, auth_old_result_cb, NULL);
			jabber_iq_send(iq);

		} else if(xmlnode_get_child(query, "password")) {
			if(js->gsc == NULL && !purple_account_get_bool(js->gc->account,
						"auth_plain_in_clear", FALSE)) {
				char *msg = g_strdup_printf(_("%s requires plaintext authentication over an unencrypted connection.  Allow this and continue authentication?"),
											js->gc->account->username);
				purple_request_yes_no(js->gc, _("Plaintext Authentication"),
						_("Plaintext Authentication"),
						msg,
						1,
						purple_connection_get_account(js->gc), NULL, NULL,
						purple_connection_get_account(js->gc), allow_plaintext_auth,
						disallow_plaintext_auth);
				g_free(msg);
				return;
			}
			finish_plaintext_authentication(js);
		} else {
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE,
				_("Server does not use any supported authentication method"));
			return;
		}
	}
}

void jabber_auth_start_old(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query, *username;

#ifdef HAVE_CYRUS_SASL
	/* If we have Cyrus SASL, then passwords will have been set
	 * to OPTIONAL for this protocol. So, we need to do our own
	 * password prompting here
	 */

	if (!purple_account_get_password(js->gc->account)) {
		purple_account_request_password(js->gc->account, G_CALLBACK(auth_old_pass_cb), G_CALLBACK(auth_no_pass_cb), js->gc);
		return;
	}
#endif
	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:auth");

	query = xmlnode_get_child(iq->node, "query");
	username = xmlnode_new_child(query, "username");
	xmlnode_insert_data(username, js->user->node, -1);

	jabber_iq_set_callback(iq, auth_old_cb, NULL);

	jabber_iq_send(iq);
}

/* Parts of this algorithm are inspired by stuff in libgsasl */
static GHashTable* parse_challenge(const char *challenge)
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
				while (val_end != val_start && (*val_end == ' ' || *val_end == ',' || *val_end == '\t'
						|| *val_end == '\r' || *val_start == '\n'
						|| *val_end == '"'))
					val_end--;

				if (val_start != val_end)
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
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	guchar result[16];
	size_t a1len;

	gchar *a1, *convnode=NULL, *convpasswd = NULL, *ha1, *ha2, *kd, *x, *z;

	if((convnode = g_convert(jid->node, -1, "iso-8859-1", "utf-8",
					NULL, NULL, NULL)) == NULL) {
		convnode = g_strdup(jid->node);
	}
	if(passwd && ((convpasswd = g_convert(passwd, -1, "iso-8859-1",
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
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Invalid response from server."));
			return;
		}

		dec_in = (char *)purple_base64_decode(enc_in, NULL);
		purple_debug(PURPLE_DEBUG_MISC, "jabber", "decoded challenge (%"
				G_GSIZE_FORMAT "): %s\n", strlen(dec_in), dec_in);

		parts = parse_challenge(dec_in);


		if (g_hash_table_lookup(parts, "rspauth")) {
			char *rspauth = g_hash_table_lookup(parts, "rspauth");


			if(rspauth && js->expected_rspauth &&
					!strcmp(rspauth, js->expected_rspauth)) {
				jabber_send_raw(js,
						"<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl' />",
						-1);
			} else {
				purple_connection_error_reason (js->gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Invalid challenge from server"));
			}
			g_free(js->expected_rspauth);
			js->expected_rspauth = NULL;
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

			if (nonce == NULL || realm == NULL)
				purple_connection_error_reason (js->gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Invalid challenge from server"));
			else {
				GString *response = g_string_new("");
				char *a2;
				char *auth_resp;
				char *buf;
				char *cnonce;

				cnonce = g_strdup_printf("%x%u%x", g_random_int(), (int)time(NULL),
						g_random_int());

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

				purple_debug_misc("jabber", "decoded response (%"
						G_GSIZE_FORMAT "): %s\n",
						response->len, response->str);

				buf = g_strdup_printf("<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>%s</response>", enc_out);

				jabber_send_raw(js, buf, -1);

				g_free(buf);

				g_free(enc_out);

				g_string_free(response, TRUE);
			}
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
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("SASL error"));
			return;
		} else {
			response = xmlnode_new("response");
			xmlnode_set_namespace(response, "urn:ietf:params:xml:ns:xmpp-sasl");
			if (clen > 0) {
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
	const void *x;
#endif

	if(!ns || strcmp(ns, "urn:ietf:params:xml:ns:xmpp-sasl")) {
		purple_connection_error_reason (js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response from server."));
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
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Invalid response from server."));
		}
	}
	/* If we've negotiated a security layer, we need to enable it */
	if (js->sasl) {
		sasl_getprop(js->sasl, SASL_SSF, &x);
		if (*(int *)x > 0) {
			sasl_getprop(js->sasl, SASL_MAXOUTBUF, &x);
			js->sasl_maxbuf = *(int *)x;
		}
	}
#endif

	jabber_stream_set_state(js, JABBER_STREAM_REINITIALIZING);
}

void jabber_auth_handle_failure(JabberStream *js, xmlnode *packet)
{
	PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
	char *msg;

#ifdef HAVE_CYRUS_SASL
	if(js->auth_fail_count++ < 5) {
		if (js->current_mech && strlen(js->current_mech) > 0) {
			char *pos;
			if ((pos = strstr(js->sasl_mechs->str, js->current_mech))) {
				g_string_erase(js->sasl_mechs, pos-js->sasl_mechs->str, strlen(js->current_mech));
			}
			/* Remove space which separated this mech from the next */
			if (strlen(js->sasl_mechs->str) > 0 && ((js->sasl_mechs->str)[0] == ' ')) {
				g_string_erase(js->sasl_mechs, 0, 1);	
			}			
		}
		if (strlen(js->sasl_mechs->str)) {
			/* If we have remaining mechs to try, do so */
			sasl_dispose(&js->sasl);
			
			jabber_auth_start_cyrus(js);
			return;
		}
	}
#endif
	msg = jabber_parse_error(js, packet, &reason);
	if(!msg) {
		purple_connection_error_reason (js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response from server."));
	} else {
		purple_connection_error_reason (js->gc, reason, msg);
		g_free(msg);
	}
}
