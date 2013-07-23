/**
 * @file soap.c
 * Functions relating to SOAP connections.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA
 */

#include "soap.h"

#include "debug.h"
#include "http.h"

typedef struct _MsnSoapRequest MsnSoapRequest;

struct _MsnSoapMessage {
	gchar *action;
	xmlnode *xml;
};

struct _MsnSoapRequest {
	MsnSoapMessage *message;
	MsnSoapService *soaps;
	MsnSoapCallback cb;
	gpointer cb_data;
	gboolean secure;
};

struct _MsnSoapService {
	MsnSession *session;
	PurpleHttpKeepalivePool *keepalive_pool;
};

static void
msn_soap_service_send_message_simple(MsnSoapService *soaps,
	MsnSoapMessage *message, const gchar *url, gboolean secure,
	MsnSoapCallback cb, gpointer cb_data);

MsnSoapMessage *
msn_soap_message_new(const gchar *action, xmlnode *xml)
{
	MsnSoapMessage *msg;

	g_return_val_if_fail(xml != NULL, NULL);

	msg = g_new0(MsnSoapMessage, 1);
	msg->action = g_strdup(action);
	msg->xml = xml;

	return msg;
}

static void
msn_soap_message_free(MsnSoapMessage *msg)
{
	if (msg == NULL)
		return;

	g_free(msg->action);
	if (msg->xml != NULL)
		xmlnode_free(msg->xml);
	g_free(msg);
}

xmlnode *
msn_soap_message_get_xml(MsnSoapMessage *message)
{
	g_return_val_if_fail(message != NULL, NULL);

	return message->xml;
}

static void
msn_soap_request_free(MsnSoapRequest *sreq)
{
	g_return_if_fail(sreq != NULL);

	msn_soap_message_free(sreq->message);
	g_free(sreq);
}

MsnSoapService *
msn_soap_service_new(MsnSession *session)
{
	MsnSoapService *soaps;

	g_return_val_if_fail(session != NULL, NULL);

	soaps = g_new0(MsnSoapService, 1);
	soaps->session = session;
	soaps->keepalive_pool = purple_http_keepalive_pool_new();
	purple_http_keepalive_pool_set_limit_per_host(soaps->keepalive_pool, 1);

	return soaps;
}

void
msn_soap_service_destroy(MsnSoapService *soaps)
{
	if (soaps == NULL)
		return;

	purple_http_keepalive_pool_unref(soaps->keepalive_pool);

	g_free(soaps);
}

static void
msn_soap_service_recv(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _sreq)
{
	MsnSoapRequest *sreq = _sreq;
	xmlnode *xml_root, *xml_body, *xml_fault;
	const gchar *xml_raw;
	size_t xml_len;

	if (purple_http_response_get_code(response) == 401) {
		const gchar *errmsg;

		purple_debug_error("msn-soap", "SOAP authentication failed\n");

		errmsg = purple_http_response_get_header(response,
			"WWW-Authenticate");

		msn_session_set_error(sreq->soaps->session, MSN_ERROR_AUTH,
			errmsg ? purple_url_decode(errmsg) : NULL);

		msn_soap_request_free(sreq);
		return;
	}
	if (!purple_http_response_is_successfull(response)) {
		purple_debug_error("msn-soap", "SOAP request failed\n");
		msn_session_set_error(sreq->soaps->session,
			MSN_ERROR_SERV_UNAVAILABLE, NULL);
		msn_soap_request_free(sreq);
		return;
	}

	xml_raw = purple_http_response_get_data(response, &xml_len);
	xml_root = xmlnode_from_str(xml_raw, xml_len);

	if (purple_debug_is_verbose()) {
		if (sreq->secure && !purple_debug_is_unsafe()) {
			purple_debug_misc("msn-soap",
				"Received secure SOAP request.\n");
		} else {
			purple_debug_misc("msn-soap",
				"Received SOAP request: %s\n", xml_raw);
		}
	}

	if (xml_root == NULL) {
		purple_debug_error("msn-soap", "SOAP response malformed\n");
		msn_session_set_error(sreq->soaps->session,
			MSN_ERROR_HTTP_MALFORMED, NULL);
		msn_soap_request_free(sreq);
		return;
	}

	xml_body = xmlnode_get_child(xml_root, "Body");
	xml_fault = xmlnode_get_child(xml_root, "Fault");

	if (xml_fault != NULL) {
		xmlnode *xml_faultcode;
		gchar *faultdata = NULL;

		xml_faultcode = xmlnode_get_child(xml_fault, "faultcode");
		if (xml_faultcode != NULL)
			faultdata = xmlnode_get_data(xml_faultcode);

		if (g_strcmp0(faultdata, "psf:Redirect") == 0) {
			xmlnode *xml_url;
			gchar *url = NULL;

			xml_url = xmlnode_get_child(xml_fault, "redirectUrl");
			if (xml_url != NULL)
				url = xmlnode_get_data(xml_url);

			msn_soap_service_send_message_simple(sreq->soaps,
				sreq->message, url, sreq->secure, sreq->cb,
				sreq->cb_data);

			/* Steal the message, passed to another call. */
			sreq->message = NULL;
			msn_soap_request_free(sreq);

			g_free(url);
			g_free(faultdata);
			return;
		}
		if (g_strcmp0(faultdata, "wsse:FailedAuthentication") == 0) {
			xmlnode *xml_reason =
				xmlnode_get_child(xml_fault, "faultstring");
			gchar *reasondata = xmlnode_get_data(xml_reason);

			msn_session_set_error(sreq->soaps->session, MSN_ERROR_AUTH,
				reasondata);

			g_free(reasondata);
			g_free(faultdata);
			msn_soap_request_free(sreq);
			return;
		}
		g_free(faultdata);
	}

	if (xml_fault != NULL || xml_body != NULL) {
		MsnSoapMessage *resp;

		resp = msn_soap_message_new(NULL, xml_root);
		sreq->cb(sreq->message, resp, sreq->cb_data);
		msn_soap_message_free(resp);
	}

	/* XXX: shouldn't msn_session_set_error here? */
	msn_soap_request_free(sreq);
}

static void
msn_soap_service_send_message_simple(MsnSoapService *soaps,
	MsnSoapMessage *message, const gchar *url, gboolean secure,
	MsnSoapCallback cb, gpointer cb_data)
{
	PurpleHttpRequest *hreq;
	MsnSoapRequest *sreq;
	gchar *body;
	int body_len;

	sreq = g_new0(MsnSoapRequest, 1);
	sreq->soaps = soaps;
	sreq->cb = cb;
	sreq->cb_data = cb_data;
	sreq->secure = secure;
	sreq->message = message;

	hreq = purple_http_request_new(url);
	purple_http_request_set_method(hreq, "POST");
	purple_http_request_set_keepalive_pool(hreq, soaps->keepalive_pool);
	purple_http_request_header_set(hreq, "SOAPAction",
		message->action ? message->action : "");
	purple_http_request_header_set(hreq, "Content-Type",
		"text/xml; charset=utf-8");
	purple_http_request_header_set(hreq, "User-Agent",
		"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)");
	purple_http_request_header_set(hreq, "Cache-Control", "no-cache");

	body = xmlnode_to_str(message->xml, &body_len);
	purple_http_request_set_contents(hreq, body, body_len);
	g_free(body);

	purple_http_request(purple_account_get_connection(
		soaps->session->account), hreq, msn_soap_service_recv, sreq);
	purple_http_request_unref(hreq);
}

void
msn_soap_service_send_message(MsnSoapService *soaps, MsnSoapMessage *message,
	const gchar *host, const gchar *path, gboolean secure,
	MsnSoapCallback cb, gpointer cb_data)
{
	gchar *url;

	g_return_if_fail(host != NULL);
	g_return_if_fail(path != NULL);

	if (path[0] == '/')
		path = &path[1];

	url = g_strdup_printf("https://%s/%s", host, path);

	msn_soap_service_send_message_simple(soaps, message, url, secure,
		cb, cb_data);

	g_free(url);
}
