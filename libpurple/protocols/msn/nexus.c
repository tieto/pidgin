/**
 * @file nexus.c MSN Nexus functions
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "msn.h"
#include "soap2.h"
#include "nexus.h"
#include "notification.h"


/**************************************************************************
 * Valid Ticket Tokens
 **************************************************************************/

#define SSO_VALID_TICKET_DOMAIN 0
#define SSO_VALID_TICKET_POLICY 1
static char *ticket_domains[][2] = {
	/* http://msnpiki.msnfanatic.com/index.php/MSNP15:SSO */
	/* {"Domain", "Policy Ref URI"}, Purpose */
	{"messengerclear.live.com", NULL},       /* Authentication for messenger. */
	{"messenger.msn.com", "?id=507"},        /* Authentication for receiving OIMs. */
	{"contacts.msn.com", "MBI"},             /* Authentication for the Contact server. */
	{"messengersecure.live.com", "MBI_SSL"}, /* Authentication for sending OIMs. */
	{"spaces.live.com", "MBI"},              /* Authentication for the Windows Live Spaces */
	{"livecontacts.live.com", "MBI"},        /* Live Contacts API, a simplified version of the Contacts SOAP service */
	{"storage.live.com", "MBI"},             /* Storage REST API */
};

/**************************************************************************
 * Main
 **************************************************************************/

MsnNexus *
msn_nexus_new(MsnSession *session)
{
	MsnNexus *nexus;
	int i;

	nexus = g_new0(MsnNexus, 1);
	nexus->session = session;

	nexus->token_len = sizeof(ticket_domains) / sizeof(char *[2]);
	nexus->tokens = g_new0(MsnTicketToken, nexus->token_len);

	for (i = 0; i < nexus->token_len; i++)
		nexus->tokens[i].token = g_hash_table_new_full(g_str_hash, g_str_equal,
		                                               g_free, g_free);

	return nexus;
}

void
msn_nexus_destroy(MsnNexus *nexus)
{
	int i;
	for (i = 0; i < nexus->token_len; i++) {
		g_hash_table_destroy(nexus->tokens[i].token);
		g_free(nexus->tokens[i].secret);
	}

	g_free(nexus->tokens);
	g_free(nexus->policy);
	g_free(nexus->nonce);
	g_free(nexus);
}

/**************************************************************************
 * RPS/SSO Authentication
 **************************************************************************/

static char *
sha1_hmac(const char *key, int key_len, const char *message, int msg_len)
{
	PurpleCipherContext *context;
	char *result;
	gboolean ret;

	context = purple_cipher_context_new_by_name("hmac", NULL);
	purple_cipher_context_set_option(context, "hash", "sha1");
	purple_cipher_context_set_key_with_len(context, (guchar *)key, key_len);

	purple_cipher_context_append(context, (guchar *)message, msg_len);
	result = g_malloc(20);
	ret = purple_cipher_context_digest(context, 20, (guchar *)result, NULL);

	purple_cipher_context_destroy(context);

	return result;
}

static char *
rps_create_key(const char *key, int key_len, const char *data, size_t data_len)
{
	char *hash1, *hash2, *hash3, *hash4;
	char *result;

	hash1 = sha1_hmac(key, key_len, data, data_len);
	hash1 = g_realloc(hash1, 20 + data_len);
	memcpy(hash1 + 20, data, data_len);
	hash2 = sha1_hmac(key, key_len, hash1, 20 + data_len);

	hash3 = sha1_hmac(key, key_len, hash1, 20);

	hash3 = g_realloc(hash3, 20 + data_len);
	memcpy(hash3 + 20, data, data_len);
	hash4 = sha1_hmac(key, key_len, hash3, 20 + data_len);

	result = g_malloc(24);
	memcpy(result, hash2, 20);
	memcpy(result + 20, hash4, 4);

	g_free(hash1);
	g_free(hash2);
	g_free(hash3);
	g_free(hash4);

	return result;
}

static char *
des3_cbc(const char *key, const char *iv, const char *data, int len)
{
	PurpleCipherContext *des3;
	char *out;
	size_t outlen;

	des3 = purple_cipher_context_new_by_name("des3", NULL);
	purple_cipher_context_set_key(des3, (guchar *)key);
	purple_cipher_context_set_batch_mode(des3, PURPLE_CIPHER_BATCH_MODE_CBC);
	purple_cipher_context_set_iv(des3, (guchar *)iv, 8);

	out = g_malloc(len);
	purple_cipher_context_encrypt(des3, (guchar *)data, len, (guchar *)out, &outlen);

	purple_cipher_context_destroy(des3);

	return out;
}

#define CRYPT_MODE_CBC 1
#define CIPHER_TRIPLE_DES 0x6603
#define HASH_SHA1 0x8004
static char *
msn_rps_encrypt(MsnNexus *nexus)
{
	MsnUsrKey *usr_key;
	const char *magic1 = "WS-SecureConversationSESSION KEY HASH";
	const char *magic2 = "WS-SecureConversationSESSION KEY ENCRYPTION";
	size_t len;
	char *hash;
	char *key1, *key2, *key3;
	gsize key1_len;
	char *nonce_fixed;
	char *cipher;
	char *response;

	usr_key = g_malloc(sizeof(MsnUsrKey));
	usr_key->size = GUINT32_TO_LE(28);
	usr_key->crypt_mode = GUINT32_TO_LE(CRYPT_MODE_CBC);
	usr_key->cipher_type = GUINT32_TO_LE(CIPHER_TRIPLE_DES);
	usr_key->hash_type = GUINT32_TO_LE(HASH_SHA1);
	usr_key->iv_len = GUINT32_TO_LE(8);
	usr_key->hash_len = GUINT32_TO_LE(20);
	usr_key->cipher_len = GUINT32_TO_LE(72);

	key1 = (char *)purple_base64_decode((const char *)nexus->tokens[MSN_AUTH_MESSENGER].secret, &key1_len);
	len = strlen(magic1);
	key2 = rps_create_key(key1, key1_len, magic1, len);
	len = strlen(magic2);
	key3 = rps_create_key(key1, key1_len, magic2, len);

	usr_key->iv[0] = 0x46; //rand() % 256;
	usr_key->iv[1] = 0xC4;
	usr_key->iv[2] = 0x14;
	usr_key->iv[3] = 0x9F;
	usr_key->iv[4] = 0xFF;
	usr_key->iv[5] = 0xFC;
	usr_key->iv[6] = 0x91;
	usr_key->iv[7] = 0x61;

	len = strlen(nexus->nonce);
	hash = sha1_hmac(key2, 24, nexus->nonce, len);

	/* We need to pad this to 72 bytes, apparently */
	nonce_fixed = g_malloc(len + 8);
	memcpy(nonce_fixed, nexus->nonce, len);
	memset(nonce_fixed + len, 0x08, 8);
	cipher = des3_cbc(key3, usr_key->iv, nonce_fixed, len + 8);
	g_free(nonce_fixed);

	memcpy(usr_key->hash, hash, 20);
	memcpy(usr_key->cipher, cipher, 72);

	g_free(key1);
	g_free(key2);
	g_free(key3);
	g_free(hash);
	g_free(cipher);

	response = purple_base64_encode((guchar *)usr_key, sizeof(MsnUsrKey));

	g_free(usr_key);

	return response;
}

/**************************************************************************
 * Login
 **************************************************************************/

static gboolean
nexus_parse_response(MsnNexus *nexus, xmlnode *xml)
{
	xmlnode *node;
	gboolean result = FALSE;

	node = xmlnode_get_child(xml, "Body/RequestSecurityTokenResponseCollection/RequestSecurityTokenResponse");

	if (node)
		node = node->next;	/* The first one is not useful */
	else
		return FALSE;

	for (; node; node = node->next) {
		xmlnode *token = xmlnode_get_child(node, "RequestedSecurityToken/BinarySecurityToken");
		xmlnode *secret = xmlnode_get_child(node, "RequestedProofToken/BinarySecret");
		xmlnode *expires = xmlnode_get_child(node, "LifeTime/Expires");

		if (token) {
			char *token_str, *expiry_str;
			const char *id_str = xmlnode_get_attrib(token, "Id");
			char **elems, **cur, **tokens;
			int id;

			if (id_str == NULL) continue;

			id = atol(id_str + 7) - 1;	/* 'Compact#' or 'PPToken#' */
			if (id >= nexus->token_len)
				continue;	/* Where did this come from? */

			token_str = xmlnode_get_data(token);
			if (token_str == NULL) continue;
			elems = g_strsplit(token_str, "&", 0);

			for (cur = elems; *cur != NULL; cur++){
				tokens = g_strsplit(*cur, "=", 2);
				g_hash_table_insert(nexus->tokens[id].token, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
			}

			g_free(token_str);
			g_strfreev(elems);

			if (secret)
				nexus->tokens[id].secret = xmlnode_get_data(secret);
			else
				nexus->tokens[id].secret = NULL;

			/* Yay for MS using ISO-8601 */
			expiry_str = xmlnode_get_data(expires);

			nexus->tokens[id].expiry = purple_str_to_time(expiry_str,
				FALSE, NULL, NULL, NULL);

			g_free(expiry_str);

			purple_debug_info("msnp15", "Updated ticket for domain '%s'\n",
			                  ticket_domains[id][SSO_VALID_TICKET_DOMAIN]);
			result = TRUE;
		}
	}

	return result;
}

static void
nexus_got_response_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	MsnNexus *nexus = data;
	MsnSession *session = nexus->session;
	char *msn_twn_t, *msn_twn_p, *ticket;
	char *response;

	if (resp == NULL) {
		msn_session_set_error(session, MSN_ERROR_SERVCONN, _("Windows Live ID authentication:Unable to connect"));
		return;
	}

	if (!nexus_parse_response(nexus, resp->xml)) {
		msn_session_set_error(session, MSN_ERROR_SERVCONN, _("Windows Live ID authentication:Invalid response"));
		return;
	}

	/*setup the t and p parameter for session*/
	msn_twn_t = g_hash_table_lookup(nexus->tokens[MSN_AUTH_MESSENGER].token, "t");
	msn_twn_p = g_hash_table_lookup(nexus->tokens[MSN_AUTH_MESSENGER].token, "p");
	g_free(session->passport_info.t);
	session->passport_info.t = g_strdup(msn_twn_t);
	g_free(session->passport_info.p);
	session->passport_info.p = g_strdup(msn_twn_p);

	ticket = g_strdup_printf("t=%s&p=%s", msn_twn_t, msn_twn_p);
	response = msn_rps_encrypt(nexus);
	msn_got_login_params(session, ticket, response);
	g_free(ticket);
	g_free(response);
}

/*when connect, do the SOAP Style windows Live ID authentication */
void
msn_nexus_connect(MsnNexus *nexus)
{
	MsnSession *session = nexus->session;
	char *username, *password;
	GString *domains;
	char *request;
	int i;

	MsnSoapMessage *soap;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

	username = g_strdup(purple_account_get_username(session->account));
	password = g_strndup(purple_connection_get_password(session->account->gc), 16);

	purple_debug_info("msnp15", "Logging on %s, with policy '%s', nonce '%s'\n",
	                  username, nexus->policy, nexus->nonce);

	domains = g_string_new(NULL);
	for (i = 0; i < nexus->token_len; i++) {
		g_string_append_printf(domains, MSN_SSO_RST_TEMPLATE,
		                       i+1,
		                       ticket_domains[i][SSO_VALID_TICKET_DOMAIN],
		                       ticket_domains[i][SSO_VALID_TICKET_POLICY] != NULL ?
		                           ticket_domains[i][SSO_VALID_TICKET_POLICY] :
		                           nexus->policy);
	}

	request = g_strdup_printf(MSN_SSO_TEMPLATE, username, password, domains->str);
	g_free(username);
	g_free(password);
	g_string_free(domains, TRUE);

	soap = msn_soap_message_new(NULL, xmlnode_from_str(request, -1));
	g_free(request);
	msn_soap_message_send(session, soap, MSN_SSO_SERVER, SSO_POST_URL,
	                      nexus_got_response_cb, nexus);
}

static void
nexus_got_update_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	MsnNexus *nexus = data;

	nexus_parse_response(nexus, resp->xml);
}

static void
msn_nexus_update_token(MsnNexus *nexus, int id)
{
	MsnSession *session = nexus->session;
	char *username, *password;
	char *domain;
	char *request;

	MsnSoapMessage *soap;

	username = g_strdup(purple_account_get_username(session->account));
	password = g_strndup(purple_connection_get_password(session->account->gc), 16);

	purple_debug_info("msnp15", "Updating ticket for user '%s' on domain '%s'\n",
	                  username, ticket_domains[id][SSO_VALID_TICKET_DOMAIN]);

	/* TODO: This really assumes if we send RSTn, the server responds with
	 Compactn, even if there is no RST(n-1). This needs checking.
	*/
	domain = g_strdup_printf(MSN_SSO_RST_TEMPLATE,
		                       id,
		                       ticket_domains[id][SSO_VALID_TICKET_DOMAIN],
		                       ticket_domains[id][SSO_VALID_TICKET_POLICY] != NULL ?
		                           ticket_domains[id][SSO_VALID_TICKET_POLICY] :
		                           nexus->policy);

	request = g_strdup_printf(MSN_SSO_TEMPLATE, username, password, domain);
	g_free(username);
	g_free(password);
	g_free(domain);

	soap = msn_soap_message_new(NULL, xmlnode_from_str(request, -1));
	g_free(request);
	msn_soap_message_send(session, soap, MSN_SSO_SERVER, SSO_POST_URL,
	                      nexus_got_update_cb, nexus);
}

GHashTable *
msn_nexus_get_token(MsnNexus *nexus, MsnAuthDomains id)
{
	g_return_val_if_fail(nexus != NULL, NULL);
	g_return_val_if_fail(id < nexus->token_len, NULL);

	if (time(NULL) > nexus->tokens[id].expiry)
		msn_nexus_update_token(nexus, id);

	return nexus->tokens[id].token;
}

const char *
msn_nexus_get_token_str(MsnNexus *nexus, MsnAuthDomains id)
{
	static char buf[1024];
	GHashTable *token = msn_nexus_get_token(nexus, id);
	const char *msn_t;
	const char *msn_p;
	gint ret;

	g_return_val_if_fail(token != NULL, NULL);

	msn_t = g_hash_table_lookup(token, "t");
	msn_p = g_hash_table_lookup(token, "p");

	g_return_val_if_fail(msn_t != NULL, NULL);
	g_return_val_if_fail(msn_p != NULL, NULL);

	ret = g_snprintf(buf, sizeof(buf) - 1, "t=%s&amp;p=%s", msn_t, msn_p);
	g_return_val_if_fail(ret != -1, NULL);

	return buf;
}

