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

/*
#include "conversation.h"
#include "ft.h"
#include "multi.h"
#include "notify.h"
#include "proxy.h"
#include "request.h"
#include "util.h"

*/

#include "account.h"
#include "accountopt.h"
#include "debug.h"
#include "message.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "util.h"

#include "auth.h"
#include "buddy.h"
#include "chat.h"
#include "iq.h"
#include "jutil.h"
#include "message.h"
#include "parser.h"
#include "presence.h"
#include "jabber.h"
#include "roster.h"

#define JABBER_CONNECT_STEPS (js->gsc ? 8 : 5)

static GaimPlugin *my_protocol = NULL;

static void jabber_stream_init(JabberStream *js)
{
	char *open_stream;

	open_stream = g_strdup_printf("<stream:stream to='%s' "
				          "xmlns='jabber:client' "
						  "xmlns:stream='http://etherx.jabber.org/streams' "
						  "version='1.0'>", js->user->domain);

	jabber_send_raw(js, open_stream);

	g_free(open_stream);
}

static void jabber_session_initialized_cb(JabberStream *js, xmlnode *packet)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	if(type && !strcmp(type, "result")) {
		jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
	} else {
		gaim_connection_error(js->gc, _("Error initializing session"));
	}
}

static void jabber_session_init(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
	xmlnode *session;

	jabber_iq_set_callback(iq, jabber_session_initialized_cb);

	session = xmlnode_new_child(iq->node, "session");
	xmlnode_set_attrib(session, "xmlns", "urn:ietf:params:xml:ns:xmpp-session");

	jabber_iq_send(iq);
}

static void jabber_stream_handle_error(JabberStream *js, xmlnode *packet)
{
	xmlnode *textnode;
	char *error_text = NULL;
	const char *text;
	char *buf;

	if(xmlnode_get_child(packet, "bad-format")) {
		text = _("Bad Format");
	} else if(xmlnode_get_child(packet, "bad-namespace-prefix")) {
		text = _("Bad Namespace Prefix");
	} else if(xmlnode_get_child(packet, "conflict")) {
		js->gc->wants_to_die = TRUE;
		text = _("Resource Conflict");
	} else if(xmlnode_get_child(packet, "connection-timeout")) {
		text = _("Connection Timeout");
	} else if(xmlnode_get_child(packet, "host-gone")) {
		text = _("Host Gone");
	} else if(xmlnode_get_child(packet, "host-unknown")) {
		text = _("Host Unknown");
	} else if(xmlnode_get_child(packet, "improper-addressing")) {
		text = _("Improper Addressing");
	} else if(xmlnode_get_child(packet, "internal-server-error")) {
		text = _("Internal Server Error");
	} else if(xmlnode_get_child(packet, "invalid-id")) {
		text = _("Invalid ID");
	} else if(xmlnode_get_child(packet, "invalid-namespace")) {
		text = _("Invalid Namespace");
	} else if(xmlnode_get_child(packet, "invalid-xml")) {
		text = _("Invalid XML");
	} else if(xmlnode_get_child(packet, "nonmatching-hosts")) {
		text = _("Non-matching Hosts");
	} else if(xmlnode_get_child(packet, "not-authorized")) {
		text = _("Not Authorized");
	} else if(xmlnode_get_child(packet, "policy-violation")) {
		text = _("Policy Violation");
	} else if(xmlnode_get_child(packet, "remote-connection-failed")) {
		text = _("Remote Connection Failed");
	} else if(xmlnode_get_child(packet, "resource-constraint")) {
		text = _("Resource Constraint");
	} else if(xmlnode_get_child(packet, "restricted-xml")) {
		text = _("Restricted XML");
	} else if(xmlnode_get_child(packet, "see-other-host")) {
		text = _("See Other Host");
	} else if(xmlnode_get_child(packet, "system-shutdown")) {
		text = _("System Shutdown");
	} else if(xmlnode_get_child(packet, "undefined-condition")) {
		text = _("Undefined Condition");
	} else if(xmlnode_get_child(packet, "unsupported-encoding")) {
		text = _("Unsupported Condition");
	} else if(xmlnode_get_child(packet, "unsupported-stanza-type")) {
		text = _("Unsupported Stanza Type");
	} else if(xmlnode_get_child(packet, "unsupported-version")) {
		text = _("Unsupported Version");
	} else if(xmlnode_get_child(packet, "xml-not-well-formed")) {
		text = _("XML Not Well Formed");
	} else {
		text = _("Stream Error");
	}

	if((textnode = xmlnode_get_child(packet, "text")))
		error_text = xmlnode_get_data(textnode);

	buf = g_strdup_printf("%s%s%s", text,
			error_text ? ": " : "",
			error_text ? error_text : "");
	gaim_connection_error(js->gc, buf);
	g_free(buf);
	if(error_text)
		g_free(error_text);
}

static void tls_init(JabberStream *js);

void jabber_process_packet(JabberStream *js, xmlnode *packet)
{
	const char *id = xmlnode_get_attrib(packet, "id");
	const char *type = xmlnode_get_attrib(packet, "type");
	JabberCallback *callback;

	if(!strcmp(packet->name, "iq")) {
		if(type && (!strcmp(type, "result") || !strcmp(type, "error")) && id
				&& *id && (callback = g_hash_table_lookup(js->callbacks, id)))
			callback(js, packet);
		else
			jabber_iq_parse(js, packet);
	} else if(!strcmp(packet->name, "presence")) {
		jabber_presence_parse(js, packet);
	} else if(!strcmp(packet->name, "message")) {
		jabber_message_parse(js, packet);
	} else if(!strcmp(packet->name, "stream:features")) {
		if(!js->registration && js->state == JABBER_STREAM_AUTHENTICATING) {
			jabber_auth_start(js, packet);
		} else if(js->state == JABBER_STREAM_REINITIALIZING) {
			jabber_session_init(js);
		} else {
			gaim_debug(GAIM_DEBUG_WARNING, "jabber",
					"Unexpected stream:features packet, ignoring\n", js->state);
		}
	} else if(!strcmp(packet->name, "stream:error")) {
		jabber_stream_handle_error(js, packet);
	} else if(!strcmp(packet->name, "challenge")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_challenge(js, packet);
	} else if(!strcmp(packet->name, "success")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_success(js, packet);
	} else if(!strcmp(packet->name, "failure")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_failure(js, packet);
	} else if(!strcmp(packet->name, "proceed")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING && !js->gsc)
			tls_init(js);
	} else {
		gaim_debug(GAIM_DEBUG_WARNING, "jabber", "Unknown packet: %s\n",
				packet->name);
	}
}

void jabber_send_raw(JabberStream *js, const char *data)
{
	int ret;

	/* because printing a tab to debug every minute gets old */
	if(strcmp(data, "\t"))
		gaim_debug(GAIM_DEBUG_MISC, "jabber", "Sending%s: %s\n", 
				js->gsc ? " (ssl)" : "", data);

	if(js->gsc) {
		ret = gaim_ssl_write(js->gsc, data, strlen(data));
	} else {
		ret = write(js->fd, data, strlen(data));
	}

	if(ret < 0)
		gaim_connection_error(js->gc, _("Write error"));

}

void jabber_send(JabberStream *js, xmlnode *packet)
{
	char *txt;

	txt = xmlnode_to_str(packet);
	jabber_send_raw(js, txt);
	g_free(txt);
}

static void jabber_keepalive(GaimConnection *gc)
{
	jabber_send_raw(gc->proto_data, "\t");
}

static void
jabber_recv_cb_ssl(gpointer data, GaimSslConnection *gsc,
		GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;
	int len;
	static char buf[4096];

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	if((len = gaim_ssl_read(gsc, buf, sizeof(buf) - 1)) > 0) {
		buf[len] = '\0';
		gaim_debug(GAIM_DEBUG_INFO, "jabber", "Recv (ssl)(%d): %s\n", len, buf);
		jabber_parser_process(js, buf, len);
	}
}

static void
jabber_recv_cb(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;
	int len;
	static char buf[4096];

	if(!g_list_find(gaim_connections_get_all(), gc))
		return;

	if((len = read(js->fd, buf, sizeof(buf) - 1)) > 0) {
		buf[len] = '\0';
		gaim_debug(GAIM_DEBUG_INFO, "jabber", "Recv (%d): %s\n", len, buf);
		jabber_parser_process(js, buf, len);
	}
}

static void
jabber_login_callback_ssl(gpointer data, GaimSslConnection *gsc,
		GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	js->gsc = gsc;

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>");

	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	gaim_ssl_input_add(gsc, jabber_recv_cb_ssl, gc);
}


static void
jabber_login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	js->fd = source;

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>");

	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	gc->inpa = gaim_input_add(js->fd, GAIM_INPUT_READ, jabber_recv_cb, gc);
}

static void tls_init(JabberStream *js)
{
	gaim_input_remove(js->gc->inpa);
	js->gc->inpa = 0;
	js->gsc = gaim_ssl_connect_fd(js->gc->account, js->fd,
			jabber_login_callback_ssl, js->gc);
}

static void
jabber_login(GaimAccount *account)
{
	int rc;
	GaimConnection *gc = gaim_account_get_connection(account);
	const char *connect_server = gaim_account_get_string(account,
			"connect_server", "");
	const char *server;
	JabberStream *js;

	gc->flags |= GAIM_CONNECTION_HTML;
	js = gc->proto_data = g_new0(JabberStream, 1);
	js->gc = gc;
	js->callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, NULL);
	js->buddies = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_buddy_free);
	js->chats = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, NULL);
	js->user = jabber_id_new(gaim_account_get_username(account));

	if(!js->user->resource) {
		char *me;
		js->user->resource = g_strdup("Gaim");
		if(!js->user->node) {
			js->user->node = js->user->domain;
			js->user->domain = g_strdup("jabber.org");
		}
		me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain,
				js->user->resource);
		gaim_account_set_username(account, me);
		g_free(me);
	}

	server = connect_server[0] ? connect_server : js->user->domain;

	jabber_stream_set_state(js, JABBER_STREAM_CONNECTING);

	if(gaim_account_get_bool(account, "old_ssl", FALSE)
			&& gaim_ssl_is_supported()) {
		js->gsc = gaim_ssl_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback_ssl, gc);
	}

	if(!js->gsc) {
		rc = gaim_proxy_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback, gc);

		if (rc != 0)
			gaim_connection_error(gc, _("Unable to create socket"));
	}
}

static gboolean
conn_close_cb(gpointer data)
{
	JabberStream *js = data;
	gaim_connection_destroy(js->gc);
	return FALSE;
}

static void
jabber_connection_schedule_close(JabberStream *js)
{
	g_timeout_add(0, conn_close_cb, js);
}

static void
jabber_registration_result_cb(JabberStream *js, xmlnode *packet)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	char *buf;

	if(!strcmp(type, "result")) {
		buf = g_strdup_printf(_("Registration of %s@%s successful"),
				js->user->node, js->user->domain);
		gaim_notify_info(NULL, _("Registration Successful"),
				_("Registration Successful"), buf);
		g_free(buf);
	} else {
		char *error;
		xmlnode *y;

		if((y = xmlnode_get_child(packet, "error"))) {
			error = xmlnode_get_data(y);
		} else {
			error = g_strdup(_("Unknown Error"));
		}

		buf = g_strdup_printf(_("Registration of %s@%s failed: %s"),
				js->user->node, js->user->domain, error);
		gaim_notify_error(NULL, _("Registration Failed"),
				_("Registration Failed"), buf);
		g_free(buf);
		g_free(error);
	}
	jabber_connection_schedule_close(js);
}

static void
jabber_register_cb(JabberStream *js, GaimRequestFields *fields)
{
	GList *groups, *flds;
	xmlnode *query, *y;
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");
	query = xmlnode_get_child(iq->node, "query");

	for(groups = gaim_request_fields_get_groups(fields); groups;
			groups = groups->next) {
		for(flds = gaim_request_field_group_get_fields(groups->data);
				flds; flds = flds->next) {
			GaimRequestField *field = flds->data;
			const char *id = gaim_request_field_get_id(field);
			const char *value = gaim_request_field_string_get_value(field);

			if(!strcmp(id, "username")) {
				y = xmlnode_new_child(query, "username");
			} else if(!strcmp(id, "password")) {
				y = xmlnode_new_child(query, "password");
			} else if(!strcmp(id, "name")) {
				y = xmlnode_new_child(query, "name");
			} else if(!strcmp(id, "email")) {
				y = xmlnode_new_child(query, "email");
			} else if(!strcmp(id, "nick")) {
				y = xmlnode_new_child(query, "nick");
			} else if(!strcmp(id, "first")) {
				y = xmlnode_new_child(query, "first");
			} else if(!strcmp(id, "last")) {
				y = xmlnode_new_child(query, "last");
			} else if(!strcmp(id, "address")) {
				y = xmlnode_new_child(query, "address");
			} else if(!strcmp(id, "city")) {
				y = xmlnode_new_child(query, "city");
			} else if(!strcmp(id, "state")) {
				y = xmlnode_new_child(query, "state");
			} else if(!strcmp(id, "zip")) {
				y = xmlnode_new_child(query, "zip");
			} else if(!strcmp(id, "phone")) {
				y = xmlnode_new_child(query, "phone");
			} else if(!strcmp(id, "url")) {
				y = xmlnode_new_child(query, "url");
			} else if(!strcmp(id, "date")) {
				y = xmlnode_new_child(query, "date");
			} else {
				continue;
			}
			xmlnode_insert_data(y, value, -1);
		}
	}

	jabber_iq_set_callback(iq, jabber_registration_result_cb);

	jabber_iq_send(iq);

}

static void
jabber_register_cancel_cb(JabberStream *js, GaimRequestFields *fields)
{
	jabber_connection_schedule_close(js);
}

void jabber_register_parse(JabberStream *js, xmlnode *packet)
{
	if(js->registration) {
		GaimRequestFields *fields;
		GaimRequestFieldGroup *group;
		GaimRequestField *field;
		xmlnode *query, *y;
		char *instructions;

		/* get rid of the login thingy */
		gaim_connection_set_state(js->gc, GAIM_CONNECTED);

		query = xmlnode_get_child(packet, "query");

		if(xmlnode_get_child(query, "registered")) {
			gaim_notify_error(NULL, _("Already Registered"),
					_("Already Registered"), NULL);
			jabber_connection_schedule_close(js);
			return;
		}

		fields = gaim_request_fields_new();
		group = gaim_request_field_group_new(NULL);
		gaim_request_fields_add_group(fields, group);

		field = gaim_request_field_string_new("username", _("Username"),
				js->user->node, FALSE);
		gaim_request_field_group_add_field(group, field);

		field = gaim_request_field_string_new("password", _("Password"),
				gaim_account_get_password(js->gc->account), FALSE);
		gaim_request_field_string_set_masked(field, TRUE);
		gaim_request_field_group_add_field(group, field);

		if(xmlnode_get_child(query, "name")) {
			field = gaim_request_field_string_new("name", _("Name"),
					gaim_account_get_alias(js->gc->account), FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "email")) {
			field = gaim_request_field_string_new("email", _("E-Mail"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "nick")) {
			field = gaim_request_field_string_new("nick", _("Nickname"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "first")) {
			field = gaim_request_field_string_new("first", _("First Name"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "last")) {
			field = gaim_request_field_string_new("last", _("Last Name"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "address")) {
			field = gaim_request_field_string_new("address", _("Address"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "city")) {
			field = gaim_request_field_string_new("city", _("City"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "state")) {
			field = gaim_request_field_string_new("state", _("State"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "zip")) {
			field = gaim_request_field_string_new("zip", _("Postal Code"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "phone")) {
			field = gaim_request_field_string_new("phone", _("Phone"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "url")) {
			field = gaim_request_field_string_new("url", _("URL"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "date")) {
			field = gaim_request_field_string_new("date", _("Date"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}

		if((y = xmlnode_get_child(query, "instructions")))
			instructions = xmlnode_get_data(y);
		else
			instructions = g_strdup(_("Please fill out the information below "
						"to register your new account."));

		gaim_request_fields(js->gc, _("Register New Jabber Account"),
				_("Register New Jabber Account"), instructions, fields,
				_("Register"), G_CALLBACK(jabber_register_cb),
				_("Cancel"), G_CALLBACK(jabber_register_cancel_cb), js);
	}
}

static void jabber_register_start(JabberStream *js)
{
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:register");
	jabber_iq_send(iq);
}

static void jabber_register_account(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	JabberStream *js;
	const char *connect_server = gaim_account_get_string(account,
			"connect_server", "");
	const char *server;
	int rc;

	js = gc->proto_data = g_new0(JabberStream, 1);
	js->gc = gc;
	js->registration = TRUE;
	js->callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, NULL);
	js->user = jabber_id_new(gaim_account_get_username(account));

	if(!js->user->resource) {
		char *me;
		js->user->resource = g_strdup("Gaim");
		if(!js->user->node) {
			js->user->node = js->user->domain;
			js->user->domain = g_strdup("jabber.org");
		}
		me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain,
				js->user->resource);
		gaim_account_set_username(account, me);
		g_free(me);
	}

	server = connect_server[0] ? connect_server : js->user->domain;

	jabber_stream_set_state(js, JABBER_STREAM_CONNECTING);

	if(gaim_account_get_bool(account, "old_ssl", FALSE)
			&& gaim_ssl_is_supported()) {
		js->gsc = gaim_ssl_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback_ssl, gc);
	}

	if(!js->gsc) {
		rc = gaim_proxy_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback, gc);

		if (rc != 0)
			gaim_connection_error(gc, _("Unable to create socket"));
	}
}

static void jabber_close(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;

	jabber_send_raw(js, "</stream:stream>");

	if(js->gsc) {
		gaim_ssl_close(js->gsc);
	} else {
		if(js->gc->inpa)
			gaim_input_remove(js->gc->inpa);
		close(js->fd);
	}

	g_markup_parse_context_free(js->context);

	if(js->callbacks)
		g_hash_table_destroy(js->callbacks);
	if(js->buddies)
		g_hash_table_destroy(js->buddies);
	if(js->chats)
		g_hash_table_destroy(js->chats);
	if(js->stream_id)
		g_free(js->stream_id);
	jabber_id_free(js->user);
	g_free(js);
}

void jabber_stream_set_state(JabberStream *js, JabberStreamState state)
{
	js->state = state;
	switch(state) {
		case JABBER_STREAM_OFFLINE:
			break;
		case JABBER_STREAM_CONNECTING:
			gaim_connection_update_progress(js->gc, _("Connecting"), 1,
					JABBER_CONNECT_STEPS);
			break;
		case JABBER_STREAM_INITIALIZING:
			gaim_connection_update_progress(js->gc, _("Initializing Stream"),
					js->gsc ? 5 : 2, JABBER_CONNECT_STEPS);
			jabber_stream_init(js);
			jabber_parser_setup(js);
			break;
		case JABBER_STREAM_AUTHENTICATING:
			gaim_connection_update_progress(js->gc, _("Authenticating"),
					js->gsc ? 6 : 3, JABBER_CONNECT_STEPS);
			if(js->registration)
				jabber_register_start(js);
			else if(js->protocol_version == JABBER_PROTO_0_9)
				jabber_auth_start_old(js);
			break;
		case JABBER_STREAM_REINITIALIZING:
			gaim_connection_update_progress(js->gc, _("Re-initializing Stream"),
					6, JABBER_CONNECT_STEPS);
			jabber_stream_init(js);
			break;
		case JABBER_STREAM_CONNECTED:
			gaim_connection_set_state(js->gc, GAIM_CONNECTED);
			jabber_roster_request(js);
			jabber_presence_send(js->gc, js->gc->away_state, js->gc->away);
			serv_finish_login(js->gc);
			break;
	}
}

char *jabber_get_next_id(JabberStream *js)
{
	return g_strdup_printf("gaim%d", js->next_id++);
}

void jabber_idle_set(GaimConnection *gc, int idle)
{
	JabberStream *js = gc->proto_data;

	js->idle = idle ? time(NULL) - idle : idle;
}

static const char *jabber_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "jabber";
}

static void jabber_list_emblems(GaimBuddy *b, char **se, char **sw,
		char **nw, char **ne)
{
	JabberStream *js;
	JabberBuddy *jb;

	if(!b->account->gc)
		return;
	js = b->account->gc->proto_data;
	jb = jabber_buddy_find(js, b->name, FALSE);

	if(!GAIM_BUDDY_IS_ONLINE(b)) {
		if(jb && jb->error_msg)
			*nw = "error";

		if(jb && (jb->subscription & JABBER_SUB_PENDING ||
					!(jb->subscription & JABBER_SUB_TO)))
			*se = "notauthorized";
		else
			*se = "offline";
	} else {
		switch (b->uc) {
			case JABBER_STATE_AWAY:
				*se = "away";
				break;
			case JABBER_STATE_CHAT:
				*se = "chat";
				break;
			case JABBER_STATE_XA:
				*se = "extendedaway";
				break;
			case JABBER_STATE_DND:
				*se = "extendedaway";
				break;
			case JABBER_STATE_ERROR:
				*se = "error";
				break;
		}
	}
}

static char *jabber_status_text(GaimBuddy *b)
{
	JabberBuddy *jb = jabber_buddy_find(b->account->gc->proto_data, b->name,
			FALSE);
	char *ret = NULL;

	if(jb && !GAIM_BUDDY_IS_ONLINE(b) && (jb->subscription & JABBER_SUB_PENDING || !(jb->subscription & JABBER_SUB_TO))) {
		ret = g_strdup(_("Not Authorized"));
	} else if(jb && !GAIM_BUDDY_IS_ONLINE(b) && jb->error_msg) {
		ret = g_strdup(jb->error_msg);
	} else {
		char *stripped;

		stripped = gaim_markup_strip_html(jabber_buddy_get_status_msg(jb));

		if(!stripped && b->uc & UC_UNAVAILABLE)
			stripped = g_strdup(jabber_get_state_string(b->uc));

		if(stripped) {
			ret = g_markup_escape_text(stripped, -1);
			g_free(stripped);
		}
	}

	return ret;
}

static char *jabber_tooltip_text(GaimBuddy *b)
{
	JabberBuddy *jb = jabber_buddy_find(b->account->gc->proto_data, b->name,
			FALSE);
	JabberBuddyResource *jbr = jabber_buddy_find_resource(jb, NULL);
	char *ret = NULL;

	if(jbr) {
		char *text = NULL;
		if(jbr->status) {
			char *stripped;
			stripped = gaim_markup_strip_html(jbr->status);
			text = g_markup_escape_text(stripped, -1);
			g_free(stripped);
		}

		ret = g_strdup_printf("<b>%s:</b> %s%s%s",
				_("Status"),
				jabber_get_state_string(jbr->state),
				text ? ": " : "",
				text ? text : "");
		if(text)
			g_free(text);
	} else if(jb && !GAIM_BUDDY_IS_ONLINE(b) && jb->error_msg) {
		ret = g_strdup_printf("<b>%s:</b> %s",
				_("Error"), jb->error_msg);
	} else if(jb && !GAIM_BUDDY_IS_ONLINE(b) &&
			(jb->subscription & JABBER_SUB_PENDING ||
			 !(jb->subscription & JABBER_SUB_TO))) {
		ret = g_strdup_printf("<b>%s:</b> %s",
				_("Status"), _("Not Authorized"));
	}

	return ret;
}

static GList *jabber_away_states(GaimConnection *gc)
{
	GList *m = NULL;

	m = g_list_append(m, _("Online"));
	m = g_list_append(m, _("Chatty"));
	m = g_list_append(m, _("Away"));
	m = g_list_append(m, _("Extended Away"));
	m = g_list_append(m, _("Do Not Disturb"));
	m = g_list_append(m, _("Invisible"));
	m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void jabber_password_change_result_cb(JabberStream *js, xmlnode *packet)
{
	const char *type;

	type = xmlnode_get_attrib(packet, "type");

	if(!strcmp(type, "result")) {
		gaim_notify_info(js->gc, _("Password Changed"), _("Password Changed"),
				_("Your password has been changed."));
	} else {
		xmlnode *error;
		char *buf, *error_txt = NULL;


		if((error = xmlnode_get_child(packet, "error")))
			error_txt = xmlnode_get_data(error);

		if(error_txt) {
			buf = g_strdup_printf(_("Error changing password: %s"),
					error_txt);
			g_free(error_txt);
		} else {
			buf = g_strdup(_("Unknown error occurred changing password"));
		}

		gaim_notify_error(js->gc, _("Error"), _("Error"), buf);
		g_free(buf);
	}
}

static void jabber_password_change_cb(JabberStream *js,
		GaimRequestFields *fields)
{
	const char *p1, *p2;
	JabberIq *iq;
	xmlnode *query, *y;

	p1 = gaim_request_fields_get_string(fields, "password1");
	p2 = gaim_request_fields_get_string(fields, "password2");

	if(strcmp(p1, p2)) {
		gaim_notify_error(js->gc, NULL, _("New passwords do not match."), NULL);
		return;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");

	xmlnode_set_attrib(iq->node, "to", js->user->domain);

	query = xmlnode_get_child(iq->node, "query");

	y = xmlnode_new_child(query, "username");
	xmlnode_insert_data(y, js->user->node, -1);
	y = xmlnode_new_child(query, "password");
	xmlnode_insert_data(y, p1, -1);

	jabber_iq_set_callback(iq, jabber_password_change_result_cb);

	jabber_iq_send(iq);

	gaim_account_set_password(js->gc->account, p1);
}

static void jabber_password_change(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("password1", _("Password"),
			"", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password2", _("Password (again)"),
			"", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(js->gc, _("Change Jabber Password"),
			_("Change Jabber Password"), _("Please enter your new password"),
			fields, _("OK"), G_CALLBACK(jabber_password_change_cb),
			_("Cancel"), NULL, js);
}

static GList *jabber_actions(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set User Info");
	pam->callback = jabber_setup_set_info;
	pam->gc = gc;
	m = g_list_append(m, pam);

	if(js->protocol_version == JABBER_PROTO_0_9) {
		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Change Password");
		pam->callback = jabber_password_change;
		pam->gc = gc;
		m = g_list_append(m, pam);
	}

	return m;
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_JABBER,
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME,
	NULL,
	NULL,
	jabber_list_icon,
	jabber_list_emblems,
	jabber_status_text,
	jabber_tooltip_text,
	jabber_away_states,
	jabber_actions,
	jabber_buddy_menu,
	jabber_chat_info,
	jabber_login,
	jabber_close,
	jabber_message_send_im,
	jabber_set_info,
	jabber_send_typing,
	jabber_buddy_get_info,
	jabber_presence_send,
	NULL,
	NULL,
	NULL,
	NULL,
	jabber_idle_set,
	NULL, /* change_passwd */ /* XXX */
	jabber_roster_add_buddy,
	NULL,
	jabber_roster_remove_buddy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	jabber_chat_join,
	jabber_chat_invite,
	jabber_chat_leave,
	jabber_chat_whisper,
	jabber_message_send_chat,
	jabber_keepalive,
	jabber_register_account,
	jabber_buddy_get_info_chat,
	NULL,
	jabber_roster_alias_change,
	jabber_roster_group_change,
	jabber_roster_group_rename,
	NULL,
	NULL, /* convo_closed */ /* XXX: thread_ids */
	NULL /* normalize */
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-jabber",                                    /**< id             */
	"Jabber",                                         /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Jabber Protocol Plugin"),
	                                                  /**  description    */
	N_("Jabber Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), "jabber.org", '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	split = gaim_account_user_split_new(_("Resource"), "Gaim", '/');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_bool_new(_("Force Old SSL"), "old_ssl", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_int_new(_("Port"), "port", 5222);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_string_new(_("Connect server"),
			"connect_server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	my_protocol = plugin;

	gaim_prefs_add_none("/plugins/prpl/jabber");
	gaim_prefs_add_bool("/plugins/prpl/jabber/hide_os", FALSE);
}

GAIM_INIT_PLUGIN(jabber, init_plugin, info);
