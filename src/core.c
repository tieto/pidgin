/*
 * gaim
 *
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>

#include "gaim.h"
#include "gaim-socket.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#ifndef _WIN32
static gint UI_fd = -1;
#endif
int gaim_session = 0;
GSList *uis = NULL;

static guchar *UI_build(guint32 *len, guchar type, guchar subtype, va_list args)
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

gint UI_write(struct UI *ui, guchar *data, gint len)
{
	gint sent;
	/* we'll let the write silently fail because the read will pick it up as dead */
	g_io_channel_write(ui->channel, data, len, &sent);
	return sent;
}

void UI_build_write(struct UI *ui, guchar type, guchar subtype, ...)
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

void UI_broadcast(guchar *data, gint len)
{
	GSList *u = uis;
	while (u) {
		struct UI *ui = u->data;
		UI_write(ui, data, len);
		u = u->next;
	}
}

void UI_build_broadcast(guchar type, guchar subtype, ...)
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

#ifndef _WIN32
static void meta_handler(struct UI *ui, guchar subtype, guchar *data)
{
	struct gaim_cui_packet *p;
	switch (subtype) {
	case CUI_META_LIST:
		break;
	case CUI_META_QUIT:
		while (uis) {
			ui = uis->data;
			uis = g_slist_remove(uis, ui);
			g_io_channel_close(ui->channel);
			g_source_remove(ui->inpa);
			g_free(ui);
		}
		do_quit();
		break;
	case CUI_META_DETACH:
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		break;
	case CUI_META_PING:
		p = cui_packet_new(CUI_TYPE_META, CUI_META_ACK);
		cui_send_packet(g_io_channel_unix_get_fd(ui->channel), p);
		cui_packet_free(p);
		break;
	default:
		debug_printf("unhandled meta subtype %d\n", subtype);
		break;
	}
}

static void plugin_handler(struct UI *ui, guchar subtype, guchar *data)
{
#ifdef GAIM_PLUGINS
	guint id;
	struct gaim_plugin *p;

	switch (subtype) {
		/*
	case CUI_PLUGIN_LIST:
		break;
		*/
	case CUI_PLUGIN_LOAD:
		p = load_plugin(data);
		/* XXX need to broadcast to UIs that plugin has been loaded */
		break;
	case CUI_PLUGIN_UNLOAD:
		memcpy(&id, data, sizeof(id));
		p = g_list_nth_data(plugins, id);
		if (p) {
			unload_plugin(p);
			/* XXX need to broadcast to UIs that plugin has been unloaded */
		}
		break;
	default:
		debug_printf("unhandled plugin subtype %d\n", subtype);
		break;
	}
#endif
}

static void user_handler(struct UI *ui, guchar subtype, guchar *data)
{
	guint id;
	struct aim_user *u;

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
		u = g_slist_nth_data(aim_users, id);
		if (u)
			serv_login(u);
		/* don't need to do anything here because the UI will get updates from other handlers */
		break;
	default:
		debug_printf("unhandled user subtype %d\n", subtype);
		break;
	}
}

static void message_handler(struct UI *ui, guchar subtype, guchar *data)
{
	switch (subtype) {
	case CUI_MESSAGE_LIST:
		break;
	case CUI_MESSAGE_SEND:
		if (!data)
			return;
		{
			guint id;
			struct gaim_connection *gc;
			guint len;
			char *who, *msg;
			gint flags;
			int pos = 0;

			memcpy(&id, data + pos, sizeof(id));
			pos += sizeof(id);
			gc = g_slist_nth_data(connections, id);
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
			serv_send_im(gc, who, msg, -1, flags);

			g_free(who);
			g_free(msg);
		}
		break;
	case CUI_MESSAGE_RECV:
		break;
	default:
		debug_printf("unhandled message subtype %d\n", subtype);
		break;
	}
}

static gint gaim_recv(GIOChannel *source, guchar *buf, gint len)
{
	gint total = 0;
	gint cur;

	while (total < len) {
		if (g_io_channel_read(source, buf + total, len - total, &cur) != G_IO_ERROR_NONE)
			return -1;
		if (cur == 0)
			return total;
		total += cur;
	}

	return total;
}

static void remote_handler(struct UI *ui, guchar subtype, guchar *data, int len)
{
	const char *resp;
	char *send;
	switch (subtype) {
	case CUI_REMOTE_CONNECTIONS:
		break;
	case CUI_REMOTE_URI:
		send = g_malloc(len + 1);
		memcpy(send, data, len);
		send[len] = 0;
		resp = handle_uri(send);
		g_free(send);
		/* report error */
		break;
	default:
		debug_printf("Unhandled remote subtype %d\n", subtype);
		break;
	}
}

static gboolean UI_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct UI *ui = data;

	guchar type;
	guchar subtype;
	guint32 len;

	guchar *in;

	/* no byte order worries! this'll change if we go to TCP */
	if (gaim_recv(source, &type, sizeof(type)) != sizeof(type)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, &subtype, sizeof(subtype)) != sizeof(subtype)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (gaim_recv(source, (guchar *)&len, sizeof(len)) != sizeof(len)) {
		debug_printf("UI has abandoned us!\n");
		uis = g_slist_remove(uis, ui);
		g_io_channel_close(ui->channel);
		g_source_remove(ui->inpa);
		g_free(ui);
		return FALSE;
	}

	if (len) {
		in = g_new0(guchar, len);
		if (gaim_recv(source, in, len) != len) {
			debug_printf("UI has abandoned us!\n");
			uis = g_slist_remove(uis, ui);
			g_io_channel_close(ui->channel);
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
			debug_printf("unhandled type %d\n", type);
			break;
	}

	if (in)
		g_free(in);
	return TRUE;
}

static gboolean socket_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
	struct sockaddr_un saddr;
	gint len;
	gint fd;

	struct UI *ui;

	if ((fd = accept(UI_fd, (struct sockaddr *)&saddr, &len)) == -1)
		return FALSE;

	ui = g_new0(struct UI, 1);
	uis = g_slist_append(uis, ui);

	ui->channel = g_io_channel_unix_new(fd);
	ui->inpa = g_io_add_watch(ui->channel, G_IO_IN | G_IO_HUP | G_IO_ERR, UI_readable, ui);
	g_io_channel_unref(ui->channel);

	debug_printf("got one\n");
	return TRUE;
}

static gint open_socket()
{
	struct sockaddr_un saddr;
	gint fd;
	
	while (gaim_session_exists(gaim_session))
		gaim_session++;
	
	debug_printf("session: %d\n", gaim_session);
	
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		mode_t m = umask(0177);
		saddr.sun_family = AF_UNIX;

		g_snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s" G_DIR_SEPARATOR_S "gaim_%s.%d",
				g_get_tmp_dir(), g_get_user_name(), gaim_session);
		if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) != -1)
			listen(fd, 100);
		else {
			g_log(NULL, G_LOG_LEVEL_CRITICAL,
			      "Failed to assign %s to a socket (Error: %s)",	
			      saddr.sun_path, strerror(errno));
			return -1;
		}
		umask(m);
	} else
		g_log(NULL, G_LOG_LEVEL_CRITICAL, "Unable to open socket: %s", strerror(errno));
	return fd;
}
#endif /*! _WIN32*/

int core_main()
{
	/*
	GMainLoop *loop;
	 */

#ifndef _WIN32
	GIOChannel *channel;

	UI_fd = open_socket();
	if (UI_fd < 0)
		return 1;

	channel = g_io_channel_unix_new(UI_fd);
	g_io_add_watch(channel, G_IO_IN, socket_readable, NULL);
	g_io_channel_unref(channel);
#endif

	/*
	loop = g_main_new(TRUE);
	g_main_run(loop);
	 */

	return 0;
}

void core_quit()
{
	save_prefs();
#ifndef _WIN32
	{
	char buf[1024];
	close(UI_fd);
	sprintf(buf, "%s" G_DIR_SEPARATOR_S "gaim_%s.%d", g_get_tmp_dir(), g_get_user_name(), gaim_session);
	unlink(buf);
	debug_printf("Removed core\n");
	}
#endif
}
