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
#include "html.h"
#include "message.h"
#include "multi.h"
#include "prpl.h"
#include "server.h"

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
		if(js->state == JABBER_STREAM_AUTHENTICATING) {
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
			g_free, NULL);
	js->chats = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, NULL);
	js->user = jabber_id_new(gaim_account_get_username(account));

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
		close(js->fd);
	}

	g_markup_parse_context_free(js->context);

	g_hash_table_destroy(js->callbacks);
	g_hash_table_destroy(js->buddies);
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
			if(js->protocol_version == JABBER_PROTO_0_9)
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
		char *stripped = strip_html(jabber_buddy_get_status_msg(jb));

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
			stripped = strip_html(jbr->status);
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

static GList *jabber_actions(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set User Info");
	pam->callback = jabber_setup_set_info;
	pam->gc = gc;
	m = g_list_append(m, pam);

	/* XXX: Change Password */

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
	NULL, /* register_user */ /* XXX tell the user success/failure */
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

	info.dependencies = g_list_append(info.dependencies, "core-ssl");

	split = gaim_account_user_split_new(_("Server"), "jabber.org", '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	split = gaim_account_user_split_new(_("Resource"), "Gaim", '/');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	if(gaim_ssl_is_supported()) {
		option = gaim_account_option_bool_new(_("Force Old SSL"), "old_ssl", FALSE);
		prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
				option);
	}

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
