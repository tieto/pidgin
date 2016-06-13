/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

/**
 * This file implements AIM's kerberos procedure for authenticating
 * users.  This replaces the older MD5-based and XOR-based
 * authentication methods that use SNAC family 0x0017.
 *
 * This doesn't use SNACs or FLAPs at all.  It makes http and https
 * POSTs to AOL to validate the user based on the password they
 * provided to us.  Upon successful authentication we request a
 * connection to the BOS server by calling startOSCARsession.  The
 * AOL server gives us the hostname and port number to use, as well
 * as the cookie to use to authenticate to the BOS server.  And then
 * everything else is the same as with BUCP.
 *
 * For details, see:
 * http://dev.aol.com/aim/oscar/#AUTH
 * http://dev.aol.com/authentication_for_clients
 */

#include "oscar.h"
#include "oscarcommon.h"
#include "core.h"

#define MAXAIMPASSLEN 16

/*
 * Incomplete X-SNAC format taken from reverse engineering doen by digsby:
 * https://github.com/ifwe/digsby/blob/master/digsby/src/oscar/login2.py
 */
typedef struct {
	aim_tlv_t *main_tlv;
	gchar *principal1;
	gchar *service;
	gchar *principal1_again;
	gchar *principal2;
	gchar unknown;
	guint8 *footer;
	struct {
		guint32 unknown1;
		guint32 unknown2;
		guint32 epoch_now;
		guint32 epoch_valid;
		guint32 epoch_renew;
		guint32 epoch_expire;
		guint32 unknown3;
		guint32 unknown4;
		guint32 unknown5;
	} dates;
	GSList *tlvlist;
} aim_xsnac_token_t;

typedef struct {
	guint16 family;
	guint16 subtype;
	guint8 flags[8];
	guint16 request_id;
	guint32 epoch;
	guint32 unknown;
	gchar *principal1;
	gchar *principal2;
	guint16 num_tokens;
	aim_xsnac_token_t *tokens;
	GSList *tlvlist;
} aim_xsnac_t;

static gchar *get_kdc_url(OscarData *od)
{
	PurpleAccount *account = purple_connection_get_account(od->gc);
	const gchar *server;
	gchar *url;
	gchar *port_str = NULL;
	gint port;

	server = purple_account_get_string(account, "server", AIM_DEFAULT_KDC_SERVER);
	port = purple_account_get_int(account, "port", AIM_DEFAULT_KDC_PORT);
	if (port != 443)
		port_str = g_strdup_printf (":%d", port);
	url = g_strdup_printf ("https://%s%s/", server, port_str ? port_str : "");
	if (port_str)
		g_free (port_str);

	return url;
}

/*
 * Using kerberos auth requires a developer ID. This key is for libpurple.
 * It is the default key for all libpurple-based clients.  AOL encourages
 * UIs (especially ones with lots of users) to override this with their
 * own key.  This key is owned by the AIM account "markdoliner"
 *
 * Keys can be managed at http://developer.aim.com/manageKeys.jsp
 */
#define DEFAULT_CLIENT_KEY "ma15d7JTxbmVG-RP"

static const char *get_client_key(OscarData *od)
{
	return oscar_get_ui_info_string(
			od->icq ? "prpl-icq-clientkey" : "prpl-aim-clientkey",
			DEFAULT_CLIENT_KEY);
}

static void
aim_encode_password(const char *password, guint8 *encoded)
{
	guint8 encoding_table[] = {
		0x76, 0x91, 0xc5, 0xe7,
		0xd0, 0xd9, 0x95, 0xdd,
		0x9e, 0x2F, 0xea, 0xd8,
		0x6B, 0x21, 0xc2, 0xbc,

	};
	guint i;

	/*
	 * We truncate AIM passwords to 16 characters since that's what
	 * the official client does as well.
	 */
	for (i = 0; i < strlen(password) && i < MAXAIMPASSLEN; i++)
		encoded[i] = (password[i] ^ encoding_table[i]);
}

static void
aim_xsnac_free(aim_xsnac_t *xsnac)
{
	gint i;

	if (xsnac->principal1)
		g_free (xsnac->principal1);
	if (xsnac->principal2)
		g_free (xsnac->principal2);
	aim_tlvlist_free (xsnac->tlvlist);

	for (i = 0; i < xsnac->num_tokens; i++) {
		g_free(xsnac->tokens[i].main_tlv->value);
		g_free(xsnac->tokens[i].main_tlv);
		if (xsnac->tokens[i].principal1)
			g_free (xsnac->tokens[i].principal1);
		if (xsnac->tokens[i].principal1_again)
		if (xsnac->tokens[i].service)
			g_free (xsnac->tokens[i].service);
			g_free (xsnac->tokens[i].principal1_again);
		if (xsnac->tokens[i].principal2)
			g_free (xsnac->tokens[i].principal2);
		if (xsnac->tokens[i].footer)
			g_free (xsnac->tokens[i].footer);
		aim_tlvlist_free (xsnac->tokens[i].tlvlist);
	}
	g_free (xsnac->tokens);
}

static void
kerberos_login_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _od)
{
	OscarData *od = _od;
	PurpleConnection *gc;
	const gchar *got_data;
	size_t got_len;
	ByteStream bs;
	aim_xsnac_t xsnac = {0};
	guint16 len;
	gchar *bosip = NULL;
	gchar *tlsCertName = NULL;
	guint8 *cookie = NULL;
	guint32 cookie_len = 0;
	char *host; int port;
	gsize i;

	gc = od->gc;

	od->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		gchar *tmp;
		gchar *url;

		url = get_kdc_url(od);
		tmp = g_strdup_printf(_("Error requesting %s: %s"),
				url,
				purple_http_response_get_error(response));
		purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		g_free(url);
		return;
	}

	got_data = purple_http_response_get_data(response, &got_len);
	purple_debug_info("oscar", "Received kerberos login HTTP response %lu : ", got_len);

	byte_stream_init (&bs, (guint8 *)got_data, got_len);

	xsnac.family = byte_stream_get16 (&bs);
	xsnac.subtype = byte_stream_get16(&bs);
	byte_stream_getrawbuf(&bs, (guint8 *) xsnac.flags, 8);

	if (xsnac.family == 0x50C && xsnac.subtype == 0x0005) {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
			_("Incorrect password"));
		return;
	}
	if (xsnac.family != 0x50C || xsnac.subtype != 0x0003) {
		purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Error parsing response from authentication server"));
		return;
	}
	xsnac.request_id = byte_stream_get16(&bs);
	xsnac.epoch = byte_stream_get32(&bs);
	xsnac.unknown = byte_stream_get32(&bs);
	len = byte_stream_get16(&bs);
	xsnac.principal1 = byte_stream_getstr(&bs, len);
	len = byte_stream_get16(&bs);
	xsnac.principal2 = byte_stream_getstr(&bs, len);
	xsnac.num_tokens = byte_stream_get16(&bs);

	purple_debug_info("oscar", "KDC: %d tokens between '%s' and '%s'\n",
		xsnac.num_tokens, xsnac.principal1, xsnac.principal2);
	xsnac.tokens = g_new0 (aim_xsnac_token_t, xsnac.num_tokens);
	for (i = 0; i < xsnac.num_tokens; i++) {
		GSList *tlv;

		tlv = aim_tlvlist_readnum(&bs, 1);
		if (tlv)
			xsnac.tokens[i].main_tlv = tlv->data;
		g_slist_free (tlv);

		len = byte_stream_get16(&bs);
		xsnac.tokens[i].principal1 = byte_stream_getstr(&bs, len);
		len = byte_stream_get16(&bs);
		xsnac.tokens[i].service = byte_stream_getstr(&bs, len);
		len = byte_stream_get16(&bs);
		xsnac.tokens[i].principal1_again = byte_stream_getstr(&bs, len);
		len = byte_stream_get16(&bs);
		xsnac.tokens[i].principal2 = byte_stream_getstr(&bs, len);
		xsnac.tokens[i].unknown = byte_stream_get8(&bs);
		len = byte_stream_get16(&bs);
		xsnac.tokens[i].footer = byte_stream_getraw(&bs, len);

		xsnac.tokens[i].dates.unknown1 = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.unknown2 = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.epoch_now = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.epoch_valid = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.epoch_renew = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.epoch_expire = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.unknown3 = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.unknown4 = byte_stream_get32(&bs);
		xsnac.tokens[i].dates.unknown5 = byte_stream_get32(&bs);

		len = byte_stream_get16(&bs);
		xsnac.tokens[i].tlvlist = aim_tlvlist_readnum(&bs, len);

		purple_debug_info("oscar", "Token %lu has %d TLVs for service '%s'\n",
			i, len, xsnac.tokens[i].service);
	}
	len = byte_stream_get16(&bs);
	xsnac.tlvlist = aim_tlvlist_readnum(&bs, len);

	for (i = 0; i < xsnac.num_tokens; i++) {
		if (strcmp (xsnac.tokens[i].service, "im/boss") == 0) {
			aim_tlv_t *tlv;
			GSList *tlvlist;
			ByteStream tbs;

			tlv = aim_tlv_gettlv(xsnac.tokens[i].tlvlist, 0x0003, 1);
			if (tlv != NULL) {
				byte_stream_init(&tbs, tlv->value, tlv->length);
				byte_stream_get32(&tbs);
				tlvlist =  aim_tlvlist_read (&tbs);
				if (aim_tlv_gettlv (tlvlist, 0x0005, 1))
					bosip = aim_tlv_getstr (tlvlist, 0x0005, 1);
				if (aim_tlv_gettlv (tlvlist, 0x0005, 1))
					tlsCertName = aim_tlv_getstr (tlvlist, 0x008D, 1);
				tlv = aim_tlv_gettlv(tlvlist, 0x0006, 1);
				if (tlv) {
					cookie_len = tlv->length;
					cookie = tlv->value;
				}
			}
			break;
		}
	}
	if (bosip && cookie) {
		port = AIM_DEFAULT_KDC_PORT;
		for (i = 0; i < strlen(bosip); i++) {
			if (bosip[i] == ':') {
				port = atoi(&(bosip[i+1]));
				break;
			}
		}
		host = g_strndup(bosip, i);
		oscar_connect_to_bos(gc, od, host, port, cookie, cookie_len, tlsCertName);
		g_free (host);
	} else {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unknown error during authentication"));
	}
	aim_xsnac_free (&xsnac);
	if (tlsCertName)
		g_free (tlsCertName);
	if (bosip)
		g_free (bosip);
}

/**
 * This function sends a binary blob request to the Kerberos KDC server
 * https://kdc.uas.aol.com with the user's username and password and
 * receives the IM cookie, which is used to request a connection to the
 * BOSS server.
 * The binary data below is what AIM 8.0.8.1 sends in order to authenticate
 * to the KDC server. It is an 'X-SNAC' packet, which is relatively similar
 * to SNAC packets but somehow different.
 * The header starts with the 0x50C family follow by 0x0002 subtype, then
 * some fixed length data and TLVs. The string "COOL" appears in there for
 * some reason followed by the 'US' and 'en' strings.
 * Then the 'imApp key=<client key>' comes after that, and then the username
 * and the string "im/boss" which seems to represent the service we are
 * requesting the authentication for. Changing that will lead to a
 * 'unknown service' error. The client key is then added again (without the
 * 'imApp key' string prepended to it) then a XOR-ed version of the password.
 * The meaning of the header/footer/in-between bytes is not known but never
 * seems to change so there is no need to reverse engineer their meaning at
 * this point.
 */
void send_kerberos_login(OscarData *od, const char *username)
{
	PurpleConnection *gc;
	PurpleHttpRequest *req;
	gchar *url;
	const gchar *password;
	guint8 password_xored[MAXAIMPASSLEN];
	const gchar *client_key;
	gchar *imapp_key;
	GString *body;
	guint16 len_be;
	guint16 reqid;
	const gchar header[] = {
		0x05, 0x0C, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
		0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x05,
		0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05,
		0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x18, 0x99,
		0x00, 0x05, 0x00, 0x04, 0x43, 0x4F, 0x4F, 0x4C,
		0x00, 0x0A, 0x00, 0x02, 0x00, 0x01, 0x00, 0x0B,
		0x00, 0x04, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
		0x55, 0x53, 0x00, 0x02, 0x65, 0x6E, 0x00, 0x04,
		0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0D,
		0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
		0x00, 0x05};
	const gchar pre_username[] = {
		0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x01, 0x8B,
		0x01, 0x00, 0x00, 0x00, 0x00};
	const gchar post_username[] = {
		0x00, 0x07, 0x69, 0x6D, 0x2F, 0x62, 0x6F, 0x73,
		0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x00, 0x02};
	const gchar pre_password[] = {
		0x40, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01,
		0x00, 0x00};
	const gchar post_password[] = {0x00, 0x00, 0x00, 0x1D};
	const gchar footer[] = {
		0x00, 0x21, 0x00, 0x32, 0x00, 0x01, 0x10, 0x03,
		0x00, 0x2C, 0x00, 0x07, 0x00, 0x14, 0x00, 0x04,
		0x00, 0x00, 0x01, 0x8B, 0x00, 0x16, 0x00, 0x02,
		0x00, 0x26, 0x00, 0x17, 0x00, 0x02, 0x00, 0x07,
		0x00, 0x18, 0x00, 0x02, 0x00, 0x00, 0x00, 0x19,
		0x00, 0x02, 0x00, 0x0D, 0x00, 0x1A, 0x00, 0x02,
		0x00, 0x04, 0x00, 0xAB, 0x00, 0x00, 0x00, 0x28,
		0x00, 0x00};

	gc = od->gc;

	password = purple_connection_get_password(gc);
	aim_encode_password (password, password_xored);

	client_key = get_client_key(od);
	imapp_key = g_strdup_printf ("imApp key=%s", client_key);

	/* Construct the body of the HTTP POST request */
	body = g_string_new(NULL);
	g_string_append_len (body, header, sizeof(header));
	reqid = (guint16) g_random_int();
	g_string_overwrite_len (body, 0xC, (void *)&reqid, sizeof(guint16));

	len_be = GUINT16_TO_BE (strlen (imapp_key));
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	g_string_append (body, imapp_key);

	len_be = GUINT16_TO_BE (strlen (username));
	g_string_append_len (body, pre_username, sizeof(pre_username));
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	g_string_append (body, username);
	g_string_append_len (body, post_username, sizeof(post_username));

	len_be = GUINT16_TO_BE (strlen (password) + 0x10);
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	g_string_append_len (body, pre_password, sizeof(pre_password));
	len_be = GUINT16_TO_BE (strlen (password) + 4);
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	len_be = GUINT16_TO_BE (strlen (password));
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	g_string_append_len (body, password_xored, strlen (password));
	g_string_append_len (body, post_password, sizeof(post_password));

	len_be = GUINT16_TO_BE (strlen (client_key));
	g_string_append_len (body, (void *)&len_be, sizeof(guint16));
	g_string_append (body, client_key);
	g_string_append_len (body, footer, sizeof(footer));

	g_free(imapp_key);

	url = get_kdc_url(od);
	req = purple_http_request_new(url);
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "Content-Type",
		"application/x-snac");
	purple_http_request_header_set(req, "Accept",
		"application/x-snac");
	purple_http_request_set_contents(req, body->str, body->len);
	od->hc = purple_http_request(gc, req, kerberos_login_cb, od);
	purple_http_request_unref(req);

	g_string_free (body, TRUE);
	g_free (url);
}
