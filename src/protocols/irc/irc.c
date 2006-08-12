/**
 * @file irc.c
 *
 * gaim
 *
 * Copyright (C) 2003, Robbert Haarman <gaim@inglorion.net>
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
 * Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 */

#include "internal.h"

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"

#include "irc.h"

static void irc_buddy_append(char *name, struct irc_buddy *ib, GString *string);

static const char *irc_blist_icon(GaimAccount *a, GaimBuddy *b);
static void irc_blist_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne);
static GList *irc_status_types(GaimAccount *account);
static GList *irc_actions(GaimPlugin *plugin, gpointer context);
/* static GList *irc_chat_info(GaimConnection *gc); */
static void irc_login(GaimAccount *account);
static void irc_login_cb_ssl(gpointer data, GaimSslConnection *gsc, GaimInputCondition cond);
static void irc_login_cb(gpointer data, gint source);
static void irc_ssl_connect_failure(GaimSslConnection *gsc, GaimSslErrorType error, gpointer data);
static void irc_close(GaimConnection *gc);
static int irc_im_send(GaimConnection *gc, const char *who, const char *what, GaimMessageFlags flags);
static int irc_chat_send(GaimConnection *gc, int id, const char *what, GaimMessageFlags flags);
static void irc_chat_join (GaimConnection *gc, GHashTable *data);
static void irc_input_cb(gpointer data, gint source, GaimInputCondition cond);
static void irc_input_cb_ssl(gpointer data, GaimSslConnection *gsc, GaimInputCondition cond);

static guint irc_nick_hash(const char *nick);
static gboolean irc_nick_equal(const char *nick1, const char *nick2);
static void irc_buddy_free(struct irc_buddy *ib);

static GaimPlugin *_irc_plugin = NULL;

static const char *status_chars = "@+%&";

static void irc_view_motd(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	struct irc_conn *irc;
	char *title;

	if (gc == NULL || gc->proto_data == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "got MOTD request for NULL gc\n");
		return;
	}
	irc = gc->proto_data;
	if (irc->motd == NULL) {
		gaim_notify_error(gc, _("Error displaying MOTD"), _("No MOTD available"),
				  _("There is no MOTD associated with this connection."));
		return;
	}
	title = g_strdup_printf(_("MOTD for %s"), irc->server);
	gaim_notify_formatted(gc, title, title, NULL, irc->motd->str, NULL, NULL);
}

static int do_send(struct irc_conn *irc, const char *buf, gsize len)
{
	int ret;

	if (irc->gsc) {
		ret = gaim_ssl_write(irc->gsc, buf, len);
	} else {
		ret = write(irc->fd, buf, len);
	}

	return ret;
}

static void
irc_send_cb(gpointer data, gint source, GaimInputCondition cond)
{
	struct irc_conn *irc = data;
	int ret, writelen;

	writelen = gaim_circ_buffer_get_max_read(irc->outbuf);

	if (writelen == 0) {
		gaim_input_remove(irc->writeh);
		irc->writeh = 0;
		return;
	}

	ret = do_send(irc, irc->outbuf->outptr, writelen);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		gaim_connection_error(gaim_account_get_connection(irc->account),
			      _("Server has disconnected"));
		return;
	}

	gaim_circ_buffer_mark_read(irc->outbuf, ret);

#if 0
	/* We *could* try to write more if we wrote it all */
	if (ret == write_len) {
		irc_send_cb(data, source, cond);
	}
#endif
}

int irc_send(struct irc_conn *irc, const char *buf)
{
	int ret, buflen = strlen(buf);

	/* If we're not buffering writes, try to send immediately */
	if (!irc->writeh)
		ret = do_send(irc, buf, buflen);
	else {
		ret = -1;
		errno = EAGAIN;
	}

	/* gaim_debug(GAIM_DEBUG_MISC, "irc", "sent%s: %s",
		irc->gsc ? " (ssl)" : "", buf); */
	if (ret <= 0 && errno != EAGAIN) {
		gaim_connection_error(gaim_account_get_connection(irc->account),
				      _("Server has disconnected"));
	} else if (ret < buflen) {
		if (ret < 0)
			ret = 0;
		if (!irc->writeh)
			irc->writeh = gaim_input_add(
				irc->gsc ? irc->gsc->fd : irc->fd,
				GAIM_INPUT_WRITE, irc_send_cb, irc);
		gaim_circ_buffer_append(irc->outbuf, buf + ret,
			buflen - ret);
	}

	return ret;
}

/* XXX I don't like messing directly with these buddies */
gboolean irc_blist_timeout(struct irc_conn *irc)
{
	GString *string = g_string_sized_new(512);
	char *list, *buf;

	g_hash_table_foreach(irc->buddies, (GHFunc)irc_buddy_append, (gpointer)string);

	list = g_string_free(string, FALSE);
	if (!list || !strlen(list)) {
		g_free(list);
		return TRUE;
	}

	buf = irc_format(irc, "vn", "ISON", list);
	g_free(list);
	irc_send(irc, buf);
	g_free(buf);

	return TRUE;
}

static void irc_buddy_append(char *name, struct irc_buddy *ib, GString *string)
{
	ib->flag = FALSE;
	g_string_append_printf(string, "%s ", name);
}

static void irc_ison_one(struct irc_conn *irc, struct irc_buddy *ib)
{
	char *buf;

	ib->flag = FALSE;
	buf = irc_format(irc, "vn", "ISON", ib->name);
	irc_send(irc, buf);
	g_free(buf);
}


static const char *irc_blist_icon(GaimAccount *a, GaimBuddy *b)
{
	return "irc";
}

static void irc_blist_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
{
	GaimPresence *presence = gaim_buddy_get_presence(b);

	if (gaim_presence_is_online(presence) == FALSE) {
		*se = "offline";
	}
}

static GList *irc_status_types(GaimAccount *account)
{
	GaimStatusType *type;
	GList *types = NULL;

	type = gaim_status_type_new(GAIM_STATUS_AVAILABLE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new(GAIM_STATUS_OFFLINE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	return types;
}

static GList *irc_actions(GaimPlugin *plugin, gpointer context)
{
	GList *list = NULL;
	GaimPluginAction *act = NULL;

	act = gaim_plugin_action_new(_("View MOTD"), irc_view_motd);
	list = g_list_append(list, act);

	return list;
}

static GList *irc_chat_join_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Channel:");
	pce->identifier = "channel";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Password:");
	pce->identifier = "password";
	pce->secret = TRUE;
	m = g_list_append(m, pce);

	return m;
}

static GHashTable *irc_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

	return defaults;
}

static void irc_login(GaimAccount *account)
{
	GaimConnection *gc;
	struct irc_conn *irc;
	char **userparts;
	const char *username = gaim_account_get_username(account);
	GaimProxyConnectInfo *connect_info;

	gc = gaim_account_get_connection(account);
	gc->flags |= GAIM_CONNECTION_NO_NEWLINES;

	if (strpbrk(username, " \t\v\r\n") != NULL) {
		gaim_connection_error(gc, _("IRC nicks may not contain whitespace"));
		return;
	}

	gc->proto_data = irc = g_new0(struct irc_conn, 1);
	irc->fd = -1;
	irc->account = account;
	irc->outbuf = gaim_circ_buffer_new(512);

	userparts = g_strsplit(username, "@", 2);
	gaim_connection_set_display_name(gc, userparts[0]);
	irc->server = g_strdup(userparts[1]);
	g_strfreev(userparts);

	irc->buddies = g_hash_table_new_full((GHashFunc)irc_nick_hash, (GEqualFunc)irc_nick_equal,
					     NULL, (GDestroyNotify)irc_buddy_free);
	irc->cmds = g_hash_table_new(g_str_hash, g_str_equal);
	irc_cmd_table_build(irc);
	irc->msgs = g_hash_table_new(g_str_hash, g_str_equal);
	irc_msg_table_build(irc);

	gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	if (gaim_account_get_bool(account, "ssl", FALSE)) {
		if (gaim_ssl_is_supported()) {
			irc->gsc = gaim_ssl_connect(account, irc->server,
					gaim_account_get_int(account, "port", IRC_DEFAULT_SSL_PORT),
					irc_login_cb_ssl, irc_ssl_connect_failure, gc);
		} else {
			gaim_connection_error(gc, _("SSL support unavailable"));
			return;
		}
	}

	if (!irc->gsc) {

		connect_info = gaim_proxy_connect(account, irc->server,
				 gaim_account_get_int(account, "port", IRC_DEFAULT_PORT),
				 irc_login_cb, NULL, gc);

		if (!connect_info || !gaim_account_get_connection(account)) {
			gaim_connection_error(gc, _("Couldn't create socket"));
			return;
		}
	}
}

static gboolean do_login(GaimConnection *gc) {
	char *buf;
	char hostname[256];
	const char *username, *realname;
	struct irc_conn *irc = gc->proto_data;
	const char *pass = gaim_connection_get_password(gc);

	if (pass && *pass) {
		buf = irc_format(irc, "vv", "PASS", pass);
		if (irc_send(irc, buf) < 0) {
/*			gaim_connection_error(gc, "Error sending password"); */
			g_free(buf);
			return FALSE;
		}
		g_free(buf);
	}

	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	username = gaim_account_get_string(irc->account, "username", "");
	realname = gaim_account_get_string(irc->account, "realname", "");
	buf = irc_format(irc, "vvvv:", "USER", strlen(username) ? username : g_get_user_name(), hostname, irc->server,
			      strlen(realname) ? realname : IRC_DEFAULT_ALIAS);
	if (irc_send(irc, buf) < 0) {
/*		gaim_connection_error(gc, "Error registering with server");*/
		g_free(buf);
		return FALSE;
	}
	g_free(buf);
	buf = irc_format(irc, "vn", "NICK", gaim_connection_get_display_name(gc));
	if (irc_send(irc, buf) < 0) {
/*		gaim_connection_error(gc, "Error sending nickname");*/
		g_free(buf);
		return FALSE;
	}
	g_free(buf);

	return TRUE;
}

static void irc_login_cb_ssl(gpointer data, GaimSslConnection *gsc,
	GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct irc_conn *irc = gc->proto_data;

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	irc->gsc = gsc;

	if (do_login(gc)) {
		gaim_ssl_input_add(gsc, irc_input_cb_ssl, gc);
	}
}

static void irc_login_cb(gpointer data, gint source)
{
	GaimConnection *gc = data;
	struct irc_conn *irc = gc->proto_data;
	GList *connections = gaim_connections_get_all();

	if (source < 0) {
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	if (!g_list_find(connections, gc)) {
		close(source);
		return;
	}

	irc->fd = source;

	if (do_login(gc)) {
		gc->inpa = gaim_input_add(irc->fd, GAIM_INPUT_READ, irc_input_cb, gc);
	}
}

static void
irc_ssl_connect_failure(GaimSslConnection *gsc, GaimSslErrorType error,
		gpointer data)
{
	GaimConnection *gc = data;
	struct irc_conn *irc = gc->proto_data;

	switch(error) {
		case GAIM_SSL_CONNECT_FAILED:
			gaim_connection_error(gc, _("Connection Failed"));
			break;
		case GAIM_SSL_HANDSHAKE_FAILED:
			gaim_connection_error(gc, _("SSL Handshake Failed"));
			break;
	}

	irc->gsc = NULL;
}

static void irc_close(GaimConnection *gc)
{
	struct irc_conn *irc = gc->proto_data;

	if (irc == NULL)
		return;

	irc_cmd_quit(irc, "quit", NULL, NULL);

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	g_free(irc->inbuf);
	if (irc->gsc) {
		gaim_ssl_close(irc->gsc);
	} else if (irc->fd > 0) {
		close(irc->fd);
	}
	if (irc->timer)
		gaim_timeout_remove(irc->timer);
	g_hash_table_destroy(irc->cmds);
	g_hash_table_destroy(irc->msgs);
	g_hash_table_destroy(irc->buddies);
	if (irc->motd)
		g_string_free(irc->motd, TRUE);
	g_free(irc->server);

	if (irc->writeh)
		gaim_input_remove(irc->writeh);

	gaim_circ_buffer_destroy(irc->outbuf);

	g_free(irc);
}

static int irc_im_send(GaimConnection *gc, const char *who, const char *what, GaimMessageFlags flags)
{
	struct irc_conn *irc = gc->proto_data;
	char *plain;
	const char *args[2];

	if (strchr(status_chars, *who) != NULL)
		args[0] = who + 1;
	else
		args[0] = who;

	plain = gaim_unescape_html(what);
	args[1] = plain;

	irc_cmd_privmsg(irc, "msg", NULL, args);
	g_free(plain);
	return 1;
}

static void irc_get_info(GaimConnection *gc, const char *who)
{
	struct irc_conn *irc = gc->proto_data;
	const char *args[2];
	args[0] = who;
	args[1] = NULL;
	irc_cmd_whois(irc, "whois", NULL, args);
}

static void irc_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	struct irc_conn *irc;
	const char *args[1];
	const char *status_id = gaim_status_get_id(status);

	g_return_if_fail(gc != NULL);
	irc = gc->proto_data;

	if (!gaim_status_is_active(status))
		return;

	args[0] = NULL;

	if (!strcmp(status_id, "away")) {
		args[0] = gaim_status_get_attr_string(status, "message");
		if ((args[0] == NULL) || (*args[0] == '\0'))
			args[0] = _("Away");
		irc_cmd_away(irc, "away", NULL, args);
	} else if (!strcmp(status_id, "available")) {
		irc_cmd_away(irc, "back", NULL, args);
	}
}

static void irc_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct irc_conn *irc = (struct irc_conn *)gc->proto_data;
	struct irc_buddy *ib = g_new0(struct irc_buddy, 1);
	ib->name = g_strdup(buddy->name);
	g_hash_table_insert(irc->buddies, ib->name, ib);

	/* if the timer isn't set, this is during signon, so we don't want to flood
	 * ourself off with ISON's, so we don't, but after that we want to know when
	 * someone's online asap */
	if (irc->timer)
		irc_ison_one(irc, ib);
}

static void irc_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct irc_conn *irc = (struct irc_conn *)gc->proto_data;
	g_hash_table_remove(irc->buddies, buddy->name);
}

static void read_input(struct irc_conn *irc, int len)
{
	char *cur, *end;

	irc->inbufused += len;
	irc->inbuf[irc->inbufused] = '\0';

	cur = irc->inbuf;
	
	/* This is a hack to work around the fact that marv gets messages
	 * with null bytes in them while using some weird irc server at work
	 */
	while ((cur < (irc->inbuf + irc->inbufused)) && !*cur)
		cur++;
	
	while (cur < irc->inbuf + irc->inbufused &&
	       ((end = strstr(cur, "\r\n")) || (end = strstr(cur, "\n")))) {
		int step = (*end == '\r' ? 2 : 1);
		*end = '\0';
		irc_parse_msg(irc, cur);
		cur = end + step;
	}
	if (cur != irc->inbuf + irc->inbufused) { /* leftover */
		irc->inbufused -= (cur - irc->inbuf);
		memmove(irc->inbuf, cur, irc->inbufused);
	} else {
		irc->inbufused = 0;
	}
}

static void irc_input_cb_ssl(gpointer data, GaimSslConnection *gsc,
		GaimInputCondition cond)
{

	GaimConnection *gc = data;
	struct irc_conn *irc = gc->proto_data;
	int len;

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	if (irc->inbuflen < irc->inbufused + IRC_INITIAL_BUFSIZE) {
		irc->inbuflen += IRC_INITIAL_BUFSIZE;
		irc->inbuf = g_realloc(irc->inbuf, irc->inbuflen);
	}

	len = gaim_ssl_read(gsc, irc->inbuf + irc->inbufused, IRC_INITIAL_BUFSIZE - 1);

	if (len < 0 && errno == EAGAIN) {
		/* Try again later */
		return;
	} else if (len < 0) {
		gaim_connection_error(gc, _("Read error"));
		return;
	} else if (len == 0) {
		gaim_connection_error(gc, _("Server has disconnected"));
		return;
	}

	read_input(irc, len);
}

static void irc_input_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct irc_conn *irc = gc->proto_data;
	int len;

	if (irc->inbuflen < irc->inbufused + IRC_INITIAL_BUFSIZE) {
		irc->inbuflen += IRC_INITIAL_BUFSIZE;
		irc->inbuf = g_realloc(irc->inbuf, irc->inbuflen);
	}

	len = read(irc->fd, irc->inbuf + irc->inbufused, IRC_INITIAL_BUFSIZE - 1);
	if (len < 0 && errno == EAGAIN) {
		return;
	} else if (len < 0) {
		gaim_connection_error(gc, _("Read error"));
		return;
	} else if (len == 0) {
		gaim_connection_error(gc, _("Server has disconnected"));
		return;
	}

	read_input(irc, len);
}

static void irc_chat_join (GaimConnection *gc, GHashTable *data)
{
	struct irc_conn *irc = gc->proto_data;
	const char *args[2];

	args[0] = g_hash_table_lookup(data, "channel");
	args[1] = g_hash_table_lookup(data, "password");
	irc_cmd_join(irc, "join", NULL, args);
}

static char *irc_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "channel"));
}

static void irc_chat_invite(GaimConnection *gc, int id, const char *message, const char *name) 
{
	struct irc_conn *irc = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Got chat invite request for bogus chat\n");
		return;
	}
	args[0] = name;
	args[1] = gaim_conversation_get_name(convo);
	irc_cmd_invite(irc, "invite", gaim_conversation_get_name(convo), args);
}


static void irc_chat_leave (GaimConnection *gc, int id)
{
	struct irc_conn *irc = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];

	if (!convo)
		return;

	args[0] = gaim_conversation_get_name(convo);
	args[1] = NULL;
	irc_cmd_part(irc, "part", gaim_conversation_get_name(convo), args);
	serv_got_chat_left(gc, id);
}

static int irc_chat_send(GaimConnection *gc, int id, const char *what, GaimMessageFlags flags)
{
	struct irc_conn *irc = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];
	char *tmp;

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "chat send on nonexistent chat\n");
		return -EINVAL;
	}
#if 0
	if (*what == '/') {
		return irc_parse_cmd(irc, convo->name, what + 1);
	}
#endif
	tmp = gaim_unescape_html(what);
	args[0] = convo->name;
	args[1] = tmp;

	irc_cmd_privmsg(irc, "msg", NULL, args);

	serv_got_chat_in(gc, id, gaim_connection_get_display_name(gc), 0, what, time(NULL));
	g_free(tmp);
	return 0;
}

static guint irc_nick_hash(const char *nick)
{
	char *lc;
	guint bucket;

	lc = g_utf8_strdown(nick, -1);
	bucket = g_str_hash(lc);
	g_free(lc);

	return bucket;
}

static gboolean irc_nick_equal(const char *nick1, const char *nick2)
{
	return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
}

static void irc_buddy_free(struct irc_buddy *ib)
{
	g_free(ib->name);
	g_free(ib);
}

static void irc_chat_set_topic(GaimConnection *gc, int id, const char *topic)
{
	char *buf;
	const char *name = NULL;
	struct irc_conn *irc;

	irc = gc->proto_data;
	name = gaim_conversation_get_name(gaim_find_chat(gc, id));

	if (name == NULL)
		return;

	buf = irc_format(irc, "vt:", "TOPIC", name, topic);
	irc_send(irc, buf);
	g_free(buf);
}

static GaimRoomlist *irc_roomlist_get_list(GaimConnection *gc)
{
	struct irc_conn *irc;
	GList *fields = NULL;
	GaimRoomlistField *f;
	char *buf;

	irc = gc->proto_data;

	if (irc->roomlist)
		gaim_roomlist_unref(irc->roomlist);

	irc->roomlist = gaim_roomlist_new(gaim_connection_get_account(gc));

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "channel", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	gaim_roomlist_set_fields(irc->roomlist, fields);

	buf = irc_format(irc, "v", "LIST");
	irc_send(irc, buf);
	g_free(buf);

	return irc->roomlist;
}

static void irc_roomlist_cancel(GaimRoomlist *list)
{
	GaimConnection *gc = gaim_account_get_connection(list->account);
	struct irc_conn *irc;

	if (gc == NULL)
		return;

	irc = gc->proto_data;

	gaim_roomlist_set_in_progress(list, FALSE);

	if (irc->roomlist == list) {
		irc->roomlist = NULL;
		gaim_roomlist_unref(list);
	}
}

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,		/* icon_spec */
	irc_blist_icon,		/* list_icon */
	irc_blist_emblems,	/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	irc_status_types,	/* away_states */
	NULL,					/* blist_node_menu */
	irc_chat_join_info,	/* chat_info */
	irc_chat_info_defaults,	/* chat_info_defaults */
	irc_login,		/* login */
	irc_close,		/* close */
	irc_im_send,		/* send_im */
	NULL,					/* set_info */
	NULL,					/* send_typing */
	irc_get_info,		/* get_info */
	irc_set_status,		/* set_status */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	irc_add_buddy,		/* add_buddy */
	NULL,					/* add_buddies */
	irc_remove_buddy,	/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	irc_chat_join,		/* join_chat */
	NULL,					/* reject_chat */
	irc_get_chat_name,	/* get_chat_name */
	irc_chat_invite,	/* chat_invite */
	irc_chat_leave,		/* chat_leave */
	NULL,					/* chat_whisper */
	irc_chat_send,		/* chat_send */
	NULL,					/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	gaim_normalize_nocase,	/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	irc_chat_set_topic,	/* set_chat_topic */
	NULL,					/* find_blist_chat */
	irc_roomlist_get_list,	/* roomlist_get_list */
	irc_roomlist_cancel,	/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	irc_dccsend_send_file,	/* send_file */
	irc_dccsend_new_xfer,	/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
};


static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-irc",                                       /**< id             */
	"IRC",                                            /**< name           */
	VERSION,                                          /**< version        */
	N_("IRC Protocol Plugin"),                        /**  summary        */
	N_("The IRC Protocol Plugin that Sucks Less"),    /**  description    */
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	irc_actions
};

static void _init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), IRC_DEFAULT_SERVER, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Encodings"), "encoding", IRC_DEFAULT_CHARSET);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Username"), "username", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Real name"), "realname", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	/*
	option = gaim_account_option_string_new(_("Quit message"), "quitmsg", IRC_DEFAULT_QUIT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	*/

	option = gaim_account_option_bool_new(_("Use SSL"), "ssl", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	_irc_plugin = plugin;

	gaim_prefs_remove("/plugins/prpl/irc/quitmsg");
	gaim_prefs_remove("/plugins/prpl/irc");

	irc_register_commands();
}

GAIM_INIT_PLUGIN(irc, _init_plugin, info);
