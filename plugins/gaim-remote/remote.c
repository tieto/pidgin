/**
 * Remote control plugin for Gaim
 *
 * Copyright (C) 2003 Christian Hammond.
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * @todo Make this a core plugin!
 */
#include "internal.h"
#include "gtkgaim.h"

#ifndef _WIN32
# include <sys/un.h>
#endif

#include <signal.h>
#include <getopt.h>

#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "prpl.h"
#include "notify.h"
#include "util.h"
#include "version.h"

/* XXX */
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gaim.h"
#include "prefs.h"

#include <gaim-remote/remote.h>

#define REMOTE_PLUGIN_ID "gtk-remote"

struct UI {
	GIOChannel *channel;
	guint inpa;
};

#ifndef _WIN32
static gint UI_fd = -1;
static guint watcher = 0;
#endif
static int gaim_session = 0;
static GSList *uis = NULL;

/* AIM URI's ARE FUN :-D */
static const char *
gaim_remote_handle_uri(const char *uri)
{
	const char *username;
	GString *str;
	GList *conn;
	GaimConnection *gc = NULL;
	GaimAccount *account;

	gaim_debug_info("gaim_remote_handle_uri", "Handling URI: %s\n", uri);

	/* Well, we'd better check to make sure we have at least one
	   AIM account connected. */
	for (conn = gaim_connections_get_all(); conn != NULL; conn = conn->next) {
		gc = conn->data;
		account = gaim_connection_get_account(gc);
		username = gaim_account_get_username(account);

		if (strcmp(gaim_account_get_protocol_id(account), "prpl-oscar") == 0 &&
			username != NULL && isalpha(*username)) {

			break;
		}
	}

	if (gc == NULL)
		return _("Not connected to AIM");

 	/* aim:goim?screenname=screenname&message=message */
	if (!g_ascii_strncasecmp(uri, "aim:goim?", strlen("aim:goim?"))) {
		char *who, *what;
		GaimConversation *c;
		uri = uri + strlen("aim:goim?");

		if (!(who = strstr(uri, "screenname="))) {
			return _("No screenname given.");
		}
		/* spaces are encoded as +'s */
		who = who + strlen("screenname=");
		str = g_string_new(NULL);
		while (*who && (*who != '&')) {
			g_string_append_c(str, *who == '+' ? ' ' : *who);
			who++;
		}
		who = g_strdup(str->str);
		g_string_free(str, TRUE);

		what = strstr(uri, "message=");
		if (what) {
			what = what + strlen("message=");
			str = g_string_new(NULL);
			while (*what && (*what != '&' || !g_ascii_strncasecmp(what, "&amp;", 5))) {
				g_string_append_c(str, *what == '+' ? ' ' : *what);
				what++;
			}
			what = g_strdup(str->str);
			g_string_free(str, TRUE);
		}

		c = gaim_conversation_new(GAIM_CONV_IM, gc->account, who);
		g_free(who);

		if (what) {
			GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);

			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, what, -1);
			g_free(what);
		}
	} else if (!g_ascii_strncasecmp(uri, "aim:addbuddy?", strlen("aim:addbuddy?"))) {
		char *who, *group;
		uri = uri + strlen("aim:addbuddy?");
		/* spaces are encoded as +'s */

		if (!(who = strstr(uri, "screenname="))) {
			return _("No screenname given.");
		}
		who = who + strlen("screenname=");
		str = g_string_new(NULL);
		while (*who && (*who != '&')) {
			g_string_append_c(str, *who == '+' ? ' ' : *who);
			who++;
		}
		who = g_strdup(str->str);
		g_string_free(str, TRUE);

		group = strstr(uri, "group=");
		if (group) {
			group = group + strlen("group=");
			str = g_string_new(NULL);
			while (*group && (*group != '&' || !g_ascii_strncasecmp(group, "&amp;", 5))) {
				g_string_append_c(str, *group == '+' ? ' ' : *group);
				group++;
			}
			group = g_strdup(str->str);
			g_string_free(str, TRUE);
		}

		gaim_debug_misc("gaim_remote_handle_uri", "who: %s\n", who);
		gaim_blist_request_add_buddy(gc->account, who, group, NULL);
		g_free(who);
		if (group)
			g_free(group);
	} else if (!g_ascii_strncasecmp(uri, "aim:gochat?", strlen("aim:gochat?"))) {
		char *room;
		GHashTable *components;
		int exch = 5;

		uri = uri + strlen("aim:gochat?");
		/* spaces are encoded as +'s */

		if (!(room = strstr(uri, "roomname="))) {
			return _("No roomname given.");
		}
		room = room + strlen("roomname=");
		str = g_string_new(NULL);
		while (*room && (*room != '&')) {
			g_string_append_c(str, *room == '+' ? ' ' : *room);
			room++;
		}
		room = g_strdup(str->str);
		g_string_free(str, TRUE);
		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				g_free);
		g_hash_table_replace(components, g_strdup("room"), room);
		g_hash_table_replace(components, g_strdup("exchange"),
				g_strdup_printf("%d", exch));

		serv_join_chat(gc, components);
		g_hash_table_destroy(components);
	} else {
		return _("Invalid AIM URI");
	}

	return NULL;
}



#if 0
static guchar *
UI_build(guint32 *len, guchar type, guchar subtype, va_list args)
{
	guchar *buffer;
	guint32 pos;
	int size;
	void *data;

	*len = sizeof(guchar) * 2 + 4;
	buffer = g_malloc(*len);
	pos = 0;

	memcpy(buffer + pos, &type, sizeof(type)); pos += sizeof(type);
	memcpy(buffer + pos, &subtype, sizeof(subtype)); pos += sizeof(subtype);

	/* we come back and do size last */
	pos += 4;

	size = va_arg(args, int);
	while (size != -1) {
		*len += size;
		buffer = g_realloc(buffer, *len);

		data = va_arg(args, void *);
		memcpy(buffer + pos, data, size);
		pos += size;

		size = va_arg(args, int);
	}

	pos -= sizeof(guchar) * 2 + 4;

	/* now we do size */
	memcpy(buffer + sizeof(guchar) * 2, &pos, 4);

	return buffer;
}

static gint
UI_write(struct UI *ui, guchar *data, gint len)
{
	GError *error = NULL;
	gint sent;
	/* we'll let the write silently fail because the read will pick it up as dead */
	g_io_channel_write_chars(ui->channel, data, len, &sent, &error);
	if (error)
		g_error_free(error);
	return sent;
}

static void
UI_build_write(struct UI *ui, guchar type, guchar subtype, ...)
{
	va_list ap;
	gchar *data;
	guint32 len;

	va_start(ap, subtype);
	data = UI_build(&len, type, subtype, ap);
	va_end(ap);

	UI_write(ui, data, len);

	g_free(data);
}

static void
UI_broadcast(guchar *data, gint len)
{
	GSList *u = uis;
	while (u) {
		struct UI *ui = u->data;
		UI_write(ui, data, len);
		u = u->next;
	}
}

static void
UI_build_broadcast(guchar type, guchar subtype, ...)
{
	va_list ap;
	gchar *data;
	guint32 len;

	if (!uis)
		return;

	va_start(ap, subtype);
	data = UI_build(&len, type, subtype, ap);
	va_end(ap);

	UI_broadcast(data, len);

	g_free(data);
}
#endif

#ifndef _WIN32
static void
meta_handler(struct UI *ui, guchar subtype, gchar *data)
{
	GaimRemotePacket *p;
	GError *error = NULL;

	switch (subtype) {
	case CUI_META_LIST:
		break;
	case CUI_META_QUIT:
		while (uis) {
			ui = uis->data;
			uis = g_slist_remove(uis, ui);
			g_io_channel_shutdown(ui->channel, TRUE, &error);
			g_source_remove(ui->inpa);
			g_free(ui);
		}
		g_timeout_add(0, gaim_core_quit_cb, NULL);
		break;
	case CUI_META_DETACH:
		uis = g_slist_remove(uis, ui);
		g_io_channel_shutdown(ui->channel, TRUE, &error);
		g_source_remove(ui->inpa);
		g_free(ui);
		break;
	case CUI_META_PING:
		p = gaim_remote_packet_new(CUI_TYPE_META, CUI_META_ACK);
		gaim_remote_session_send_packet(g_io_channel_unix_get_fd(ui->channel),
										p);
		gaim_remote_packet_free(p);
		break;
	default:
		gaim_debug_warning("cui", "Unhandled meta subtype %d\n", subtype);
		break;
	}

	if(error)
		g_error_free(error);
}

static void
plugin_handler(struct UI *ui, guchar subtype, gpointer data)
{
#ifdef GAIM_PLUGINS
	guint id;
	GaimPlugin *p;

	switch (subtype) {
		/*
	case CUI_PLUGIN_LIST:
		break;
		*/
	case CUI_PLUGIN_LOAD:
		gaim_plugin_load(gaim_plugin_probe(data));
		break;
	case CUI_PLUGIN_UNLOAD:
		memcpy(&id, data, sizeof(id));
		p = g_list_nth_data(gaim_plugins_get_loaded(), id);
		if (p) {
			gaim_plugin_unload(p);
		}
		break;
	default:
		gaim_debug_warning("cui", "Unhandled plugin subtype %d\n", subtype);
		break;
	}
#endif
}

static void
user_handler(struct UI *ui, guchar subtype, gchar *data)
{
	guint id;
	GaimAccount *account;

	switch (subtype) {
	/*
	case CUI_USER_LIST:
		break;
	case CUI_USER_ADD:
		break;
	case CUI_USER_REMOVE:
		break;
	case CUI_USER_MODIFY:
		break;
	*/

	case CUI_USER_SIGNON:
		if (!data)
			return;
		memcpy(&id, data, sizeof(id));
		account = g_list_nth_data(gaim_accounts_get_all(), id);
		if (account)
			/* XXX: someone might want to extend this to allow connecting with a different status */
			gaim_account_connect(account, gaim_account_get_status(account, "online"));
		/* don't need to do anything here because the UI will get updates from other handlers */
		break;

#if 0 /* STATUS */
	case CUI_USER_AWAY:
		{
			GSList* l;
			const char* default_away_name = gaim_prefs_get_string("/core/away/default_message");

			for (l = away_messages; l; l = l->next) {
				if (!strcmp(default_away_name, ((struct away_message *)l->data)->name)) {
					do_away_message(NULL, l->data);
					break;
				}
			}
		}
		break;

	case CUI_USER_BACK:
		do_im_back(NULL, NULL);
		break;

#endif /* STATUS */

	case CUI_USER_LOGOUT:
		gaim_connections_disconnect_all();
		break;

	default:
		gaim_debug_warning("cui", "Unhandled user subtype %d\n", subtype);
		break;
	}
}

static void
message_handler(struct UI *ui, guchar subtype, gchar *data)
{
	switch (subtype) {
	case CUI_MESSAGE_LIST:
		break;
	case CUI_MESSAGE_SEND:
		if (!data)
			return;
		{
			guint id;
			GaimConnection *gc;
			guint len;
			char *who, *msg;
			gint flags;
			int pos = 0;

			memcpy(&id, data + pos, sizeof(id));
			pos += sizeof(id);
			gc = g_list_nth_data(gaim_connections_get_all(), id);
			if (!gc)
				return;

			memcpy(&len, data + pos, sizeof(len));
			pos += sizeof(len);
			who = g_strndup(data + pos, len + 1);
			pos += len;

			memcpy(&len, data + pos, sizeof(len));
			pos += sizeof(len);
			msg = g_strndup(data + pos, len + 1);
			pos += len;

			memcpy(&flags, data + pos, sizeof(flags));
			serv_send_im(gc, who, msg, flags);

			g_free(who);
			g_free(msg);
		}
		break;
	case CUI_MESSAGE_RECV:
		break;
	default:
		gaim_debug_warning("cui", "Unhandled message subtype %d\n", subtype);
		break;
	}
}

static gint
gaim_recv(GIOChannel *source, gchar *buf, gint len)
{
	gint total = 0;
	gsize cur;

	GError *error = NULL;

	while (total < len) {
		if (g_io_channel_read_chars(source, buf + total, len - total, &cur, &error) != G_IO_STATUS_NORMAL) {
			if (error)
				g_error_free(error);
			return -1;
		}
		if (cur == 0)
			return total;
		total += cur;
	}

	return total;
}

static void
remote_handler(struct UI *ui, guchar subtype, gchar *data, int len)
{
	const char *resp;
	char *send;
	GList *c = gaim_connections_get_all();
	GaimConnection *gc = NULL;
	GaimAccount *account;

	switch (subtype) {
	case CUI_REMOTE_CONNECTIONS:
		break;
	case CUI_REMOTE_SEND:
		if (!data)
			return;
		{
   			GaimConversation *conv;
			guint tlen, len, len2, quiet;
			char *who, *msg;
			char *tmp, *from, *proto;
			int pos = 0;

			gaim_debug_info("cui", "Got `gaim-remote send` packet\n",data);
			gaim_debug_info("cui", "g-r>%s;\n",data);

			tmp = g_strndup(data + pos, 4);
			tlen = atoi(tmp);
			pos += 4;

			who = g_strndup(data+pos, tlen);
			pos += tlen;

			tmp = g_strndup(data + pos, 4);
			tlen = atoi(tmp); len=tlen; /* length for 'from' compare */
			pos += 4;

			from = g_strndup(data+pos, tlen);
			pos += tlen;

			tmp = g_strndup(data + pos, 4);
			tlen = atoi(tmp); len2=tlen; /* length for 'proto' compare */
			pos += 4;

			proto = g_strndup(data+pos, tlen);
			pos += tlen;

			tmp = g_strndup(data + pos, 4);
			tlen = atoi(tmp);
			pos += 4;

			msg = g_strndup(data+pos, tlen);
			pos += tlen;

			tmp = g_strndup(data + pos, 1);
			quiet = atoi(tmp); /* quiet flag - not in use yet */

			/* find acct */
	   		while (c) {
				gc = c->data;
				account=gaim_connection_get_account(gc);
				if ((!gaim_utf8_strcasecmp(from, gaim_account_get_username(account))) && (!g_ascii_strncasecmp(proto, gaim_account_get_protocol_id(account), len2)) ) 
					break;
				c = c->next;
			}
			if (!gc)
				return;
			/* end acct find */

			/* gaim_debug_info("cui", "g-r>To: %s; From: %s; Protocol: %s; Message: %s; Quiet: %d\n",who,from,proto,msg,quiet); */
   			conv = gaim_conversation_new(GAIM_CONV_IM, gaim_connection_get_account(gc), who);
   			gaim_conv_im_send(GAIM_CONV_IM(conv), msg);

			/* likely to be used for quiet:
			serv_send_im(gc, who, msg, -1, 0);
			*/

			g_free(who);
			g_free(msg);
			g_free(from);
			g_free(tmp);
		}
		break;

	case CUI_REMOTE_URI:
		send = g_malloc(len + 1);
		memcpy(send, data, len);
		send[len] = 0;
		resp = gaim_remote_handle_uri(send);
		g_free(send);
		/* report error */
		break;

	default:
		gaim_debug_warning("cui", "Unhandled remote subtype %d\n", subtype);
		break;
	}
}

static gboolean
UI_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct UI *ui = data;
	gchar type;
	gchar subtype;
	gint len;
	GError *error = NULL;
	gchar *in;

	/* no byte order worries! this'll change if we go to TCP */
	if (gaim_recv(source, &type, sizeof(type)) != sizeof(type)) {
		gaim_debug_error("cui", "UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_shutdown(ui->channel, TRUE, &error);
		if(error) {
			g_error_free(error);
			error = NULL;
		}
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, &subtype, sizeof(subtype)) != sizeof(subtype)) {
		gaim_debug_error("cui", "UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_shutdown(ui->channel, TRUE, &error);
		if(error) {
			g_error_free(error);
			error = NULL;
		}
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, (gchar *)&len, sizeof(len)) != sizeof(len)) {
		gaim_debug_error("cui", "UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_shutdown(ui->channel, TRUE, &error);
		if(error) {
			g_error_free(error);
			error = NULL;
		}
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (len) {
		in = g_new0(gchar, len);
		if (gaim_recv(source, in, len) != len) {
			gaim_debug_error("cui", "UI has abandoned us!\n");
			uis = g_slist_remove(uis, ui);
			g_io_channel_shutdown(ui->channel, TRUE, &error);
			if(error) {
				g_error_free(error);
				error = NULL;
			}
			g_source_remove(ui->inpa);
			g_free(ui);
			return FALSE;
		}
	} else
		in = NULL;

	switch (type) {
		case CUI_TYPE_META:
			meta_handler(ui, subtype, in);
			break;
		case CUI_TYPE_PLUGIN:
			plugin_handler(ui, subtype, in);
			break;
		case CUI_TYPE_USER:
			user_handler(ui, subtype, in);
			break;
			/*
		case CUI_TYPE_CONN:
			conn_handler(ui, subtype, in);
			break;
		case CUI_TYPE_BUDDY:
			buddy_handler(ui, subtype, in);
			break;
			*/
		case CUI_TYPE_MESSAGE:
			message_handler(ui, subtype, in);
			break;
			/*
		case CUI_TYPE_CHAT:
			chat_handler(ui, subtype, in);
			break;
			*/
		case CUI_TYPE_REMOTE:
			remote_handler(ui, subtype, in, len);
			break;
		default:
			gaim_debug_warning("cui", "Unhandled type %d\n", type);
			break;
	}

	if (in)
		g_free(in);
	return TRUE;
}

static gboolean
socket_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct sockaddr_un saddr;
	guint len = sizeof(saddr);
	gint fd;

	struct UI *ui;

	if ((fd = accept(UI_fd, (struct sockaddr *)&saddr, &len)) == -1)
		return FALSE;

	ui = g_new0(struct UI, 1);
	uis = g_slist_append(uis, ui);

	ui->channel = g_io_channel_unix_new(fd);
	ui->inpa = g_io_add_watch(ui->channel, G_IO_IN | G_IO_HUP | G_IO_ERR, UI_readable, ui);
	g_io_channel_unref(ui->channel);

	gaim_debug_misc("cui", "Got one\n");
	return TRUE;
}

static gint
open_socket(char **error)
{
	struct sockaddr_un saddr;
	gint fd;

	while (gaim_remote_session_exists(gaim_session))
		gaim_session++;

	gaim_debug_misc("cui", "Session: %d\n", gaim_session);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		mode_t m = umask(0177);
		saddr.sun_family = AF_UNIX;

		g_snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s" G_DIR_SEPARATOR_S "gaim_%s.%d",
				g_get_tmp_dir(), g_get_user_name(), gaim_session);
		if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) != -1)
			listen(fd, 100);
		else {
			char *tmp = g_locale_to_utf8(strerror(errno), -1, NULL, NULL, NULL);
			*error = g_strdup_printf(_("Failed to assign %s to a socket:\n%s"),
					   saddr.sun_path, tmp);
			g_log(NULL, G_LOG_LEVEL_CRITICAL,
			      "Failed to assign %s to a socket (Error: %s)",
			      saddr.sun_path, strerror(errno));
			umask(m);
			g_free(tmp);
			return -1;
		}
		umask(m);
	} else
		g_log(NULL, G_LOG_LEVEL_CRITICAL, "Unable to open socket: %s", strerror(errno));
	return fd;
}
#endif /*! _WIN32*/

static gboolean
plugin_load(GaimPlugin *plugin)
{
#ifndef _WIN32
	GIOChannel *channel;
	char *buf = NULL;

	if ((UI_fd = open_socket(&buf)) < 0) {
		gaim_notify_error(NULL, NULL, _("Unable to open socket"), buf);
		g_free(buf);
		return FALSE;
	}

	channel = g_io_channel_unix_new(UI_fd);
	watcher = g_io_add_watch(channel, G_IO_IN, socket_readable, NULL);
	g_io_channel_unref(channel);

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	/* don't save prefs after plugins are gone... */
#ifndef _WIN32
	char buf[1024];

	g_source_remove(watcher);
	close(UI_fd);

	g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S "gaim_%s.%d",
			g_get_tmp_dir(), g_get_user_name(), gaim_session);

	unlink(buf);

	gaim_debug_misc("core", "Removed core\n");

	return TRUE;
#else
	return FALSE;
#endif
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	REMOTE_PLUGIN_ID,                                 /**< id             */
	N_("Remote Control"),                             /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides remote control for gaim applications."),
	                                                  /**  description    */
	N_("Gives Gaim the ability to be remote-controlled through third-party "
	   "applications or through the gaim-remote tool."),
	"Sean Egan <sean.egan@binghamton.edu>",
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
_init_plugin(GaimPlugin *plugin)
{
}

/* This may be horribly wrong. Oh the mayhem! */
GAIM_INIT_PLUGIN(remote, _init_plugin, info)
