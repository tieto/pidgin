/*
 * gaim - Gadu-Gadu Protocol Plugin
 * $Id: gg.c 4474 2003-01-07 20:57:48Z thekingant $
 *
 * Copyright (C) 2001 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
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
#include <config.h>
#endif

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#else
#include <winsock.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
/* Library from EKG (Eksperymentalny Klient Gadu-Gadu) */
#include "libgg.h"
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "pixmaps/protocols/gg/gg_suncloud.xpm"
#include "pixmaps/protocols/gg/gg_sunred.xpm"
#include "pixmaps/protocols/gg/gg_sunwhitered.xpm"
#include "pixmaps/protocols/gg/gg_sunyellow.xpm"

#define USEROPT_NICK 0

#define AGG_BUF_LEN 1024

#define AGG_GENDER_NONE -1

#define AGG_PUBDIR_USERLIST_EXPORT_FORM "/appsvc/fmcontactsput.asp"
#define AGG_PUBDIR_USERLIST_IMPORT_FORM "/appsvc/fmcontactsget.asp"
#define AGG_PUBDIR_SEARCH_FORM "/appsvc/fmpubquery2.asp"
#define AGG_REGISTER_DATA_FORM "/appsvc/fmregister.asp"
#define AGG_PUBDIR_MAX_ENTRIES 200

#define AGG_STATUS_AVAIL              _("Available")
#define AGG_STATUS_AVAIL_FRIENDS      _("Available for friends only")
#define AGG_STATUS_BUSY               _("Away")
#define AGG_STATUS_BUSY_FRIENDS       _("Away for friends only")
#define AGG_STATUS_INVISIBLE          _("Invisible")
#define AGG_STATUS_INVISIBLE_FRIENDS  _("Invisible for friends only")
#define AGG_STATUS_NOT_AVAIL          _("Unavailable")

#define AGG_HTTP_NONE			0
#define AGG_HTTP_SEARCH			1
#define AGG_HTTP_USERLIST_IMPORT	2
#define AGG_HTTP_USERLIST_EXPORT	3
#define AGG_HTTP_USERLIST_DELETE	4
#define AGG_HTTP_PASSWORD_CHANGE	5

#define UC_NORMAL 2

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;


struct agg_data {
	struct gg_session *sess;
	int own_status;
};

struct agg_http {
	struct gaim_connection *gc;
	gchar *request;
	gchar *form;
	gchar *host;
	int inpa;
	int type;
};


static gchar *charset_convert(const gchar *locstr, const char *encsrc, const char *encdst)
{
	return (g_convert (locstr, strlen (locstr), encdst, encsrc, NULL, NULL, NULL));
}

static gboolean invalid_uin(const char *uin)
{
	unsigned long int res = strtol(uin, (char **)NULL, 10);
	if (res == LONG_MIN || res == LONG_MAX || res == 0)
		return TRUE;
	return FALSE;
}

static gint args_compare(gconstpointer a, gconstpointer b)
{
	gchar *arg_a = (gchar *)a;
	gchar *arg_b = (gchar *)b;

	return g_strcasecmp(arg_a, arg_b);
}

static gboolean allowed_uin(struct gaim_connection *gc, char *uin)
{
	switch (gc->permdeny) {
	case 1:
		/* permit all, deny none */
		return TRUE;
		break;
	case 2:
		/* deny all, permit none. */
		return FALSE;
		break;
	case 3:
		/* permit some. */
		if (g_slist_find_custom(gc->permit, uin, args_compare))
			return TRUE;
		return FALSE;
		break;
	case 4:
		/* deny some. */
		if (g_slist_find_custom(gc->deny, uin, args_compare))
			return FALSE;
		return TRUE;
		break;
	default:
		return TRUE;
		break;
	}
}

static char *handle_errcode(struct gaim_connection *gc, int errcode)
{
	static char msg[AGG_BUF_LEN];

	switch (errcode) {
	case GG_FAILURE_RESOLVING:
		g_snprintf(msg, sizeof(msg), _("Unable to resolve hostname."));
		break;
	case GG_FAILURE_CONNECTING:
		g_snprintf(msg, sizeof(msg), _("Unable to connect to server."));
		break;
	case GG_FAILURE_INVALID:
		g_snprintf(msg, sizeof(msg), _("Invalid response from server."));
		break;
	case GG_FAILURE_READING:
		g_snprintf(msg, sizeof(msg), _("Error while reading from socket."));
		break;
	case GG_FAILURE_WRITING:
		g_snprintf(msg, sizeof(msg), _("Error while writing to socket."));
		break;
	case GG_FAILURE_PASSWORD:
		g_snprintf(msg, sizeof(msg), _("Authentication failed."));
		break;
	default:
		g_snprintf(msg, sizeof(msg), _("Unknown Error Code."));
		break;
	}

	hide_login_progress(gc, msg);

	return msg;
}

static void agg_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	int status = gd->own_status;

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (!g_strcasecmp(state, AGG_STATUS_AVAIL))
		status = GG_STATUS_AVAIL;
	else if (!g_strcasecmp(state, AGG_STATUS_AVAIL_FRIENDS)) {
		status = GG_STATUS_AVAIL | GG_STATUS_FRIENDS_MASK;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, AGG_STATUS_BUSY)) {
		status = GG_STATUS_BUSY;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, AGG_STATUS_BUSY_FRIENDS)) {
		status =  GG_STATUS_BUSY | GG_STATUS_FRIENDS_MASK;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, AGG_STATUS_INVISIBLE)) {
		status = GG_STATUS_INVISIBLE;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, AGG_STATUS_INVISIBLE_FRIENDS)) {
		status = GG_STATUS_INVISIBLE | GG_STATUS_FRIENDS_MASK;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, AGG_STATUS_NOT_AVAIL)) {
		status = GG_STATUS_NOT_AVAIL;
		gc->away = g_strdup("");
	} else if (!g_strcasecmp(state, GAIM_AWAY_CUSTOM)) {
		if (msg) {
			status = GG_STATUS_BUSY;
			gc->away = g_strdup("");
		} else
			status = GG_STATUS_AVAIL;

		if (gd->own_status & GG_STATUS_FRIENDS_MASK)
			status |= GG_STATUS_FRIENDS_MASK;
	}

	gd->own_status = status;
	gg_change_status(gd->sess, status);
}

static gchar *get_away_text(int uc)
{
	if (uc == UC_UNAVAILABLE)
		return AGG_STATUS_NOT_AVAIL;
	uc = uc >> 5;
	switch (uc) {
	case GG_STATUS_AVAIL:
	default:
		return AGG_STATUS_AVAIL;
	case GG_STATUS_AVAIL | GG_STATUS_FRIENDS_MASK:
		return AGG_STATUS_AVAIL_FRIENDS;
	case GG_STATUS_BUSY:
		return AGG_STATUS_BUSY;
	case GG_STATUS_BUSY | GG_STATUS_FRIENDS_MASK:
		return AGG_STATUS_BUSY_FRIENDS;
	case GG_STATUS_INVISIBLE:
		return AGG_STATUS_INVISIBLE;
	case GG_STATUS_INVISIBLE | GG_STATUS_FRIENDS_MASK:
		return AGG_STATUS_INVISIBLE_FRIENDS;
	case GG_STATUS_NOT_AVAIL:
		return AGG_STATUS_NOT_AVAIL;
	}
}

static GList *agg_away_states(struct gaim_connection *gc)
{
	GList *m = NULL;

	m = g_list_append(m, AGG_STATUS_AVAIL);
	m = g_list_append(m, AGG_STATUS_BUSY);
	m = g_list_append(m, AGG_STATUS_INVISIBLE);
	m = g_list_append(m, AGG_STATUS_AVAIL_FRIENDS);
	m = g_list_append(m, AGG_STATUS_BUSY_FRIENDS);
	m = g_list_append(m, AGG_STATUS_INVISIBLE_FRIENDS);
	m = g_list_append(m, AGG_STATUS_NOT_AVAIL);
	return m;
}

/* Enhance these functions, more options and such stuff */
static GList *agg_buddy_menu(struct gaim_connection *gc, char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	struct buddy *b = find_buddy(gc, who);
	static char buf[AGG_BUF_LEN];

	if (!b) {
		return m;
	}

	pbm = g_new0(struct proto_buddy_menu, 1);
	g_snprintf(buf, sizeof(buf), _("Status: %s"), get_away_text(b->uc));
	pbm->label = buf;
	pbm->callback = NULL;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static void main_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct agg_data *gd = gc->proto_data;
	struct gg_event *e;

	debug_printf("main_callback enter: begin\n");

	if (gd->sess->fd != source)
		gd->sess->fd = source;

	if (source == -1) {
		hide_login_progress(gc, _("Could not connect"));
		signoff(gc);
		return;
	}

	if (!(e = gg_watch_fd(gd->sess))) {
		debug_printf("main_callback: gg_watch_fd failed - CRITICAL!\n");
		hide_login_progress(gc, _("Unable to read socket"));
		signoff(gc);
		return;
	}

	switch (e->type) {
	case GG_EVENT_NONE:
		/* do nothing */
		break;
	case GG_EVENT_CONN_SUCCESS:
		debug_printf("main_callback: CONNECTED AGAIN!?\n");
		break;
	case GG_EVENT_CONN_FAILED:
		if (gc->inpa)
			gaim_input_remove(gc->inpa);
		handle_errcode(gc, e->event.failure);
		signoff(gc);
		break;
	case GG_EVENT_MSG:
		{
			gchar *imsg;
			gchar user[20];

			g_snprintf(user, sizeof(user), "%lu", e->event.msg.sender);
			if (!allowed_uin(gc, user))
				break;
			imsg = charset_convert(e->event.msg.message, "CP1250", "UTF-8");
			strip_linefeed(imsg);
			/* e->event.msg.time - we don't know what this time is for */
			serv_got_im(gc, user, imsg, 0, time(NULL), -1);
			g_free(imsg);
		}
		break;
	case GG_EVENT_NOTIFY:
		{
			gchar user[20];
			struct gg_notify_reply *n = e->event.notify;
			guint status;

			while (n->uin) {
				switch (n->status) {
				case GG_STATUS_NOT_AVAIL:
					status = UC_UNAVAILABLE;
					break;
				case GG_STATUS_AVAIL:
				case GG_STATUS_BUSY:
				case GG_STATUS_INVISIBLE:
					status = UC_NORMAL | (n->status << 5);
					break;
				default:
					status = UC_NORMAL;
					break;
				}

				g_snprintf(user, sizeof(user), "%lu", n->uin);
				serv_got_update(gc, user, (status == UC_UNAVAILABLE) ? 0 : 1, 0, 0, 0,
						status, 0);
				n++;
			}
		}
		break;
	case GG_EVENT_STATUS:
		{
			gchar user[20];
			guint status;

			switch (e->event.status.status) {
			case GG_STATUS_NOT_AVAIL:
				status = UC_UNAVAILABLE;
				break;
			case GG_STATUS_AVAIL:
			case GG_STATUS_BUSY:
			case GG_STATUS_INVISIBLE:
				status = UC_NORMAL | (e->event.status.status << 5);
				break;
			default:
				status = UC_NORMAL;
				break;
			}

			g_snprintf(user, sizeof(user), "%lu", e->event.status.uin);
			serv_got_update(gc, user, (status == UC_UNAVAILABLE) ? 0 : 1, 0, 0, 0,
					status, 0);
		}
		break;
	case GG_EVENT_ACK:
		debug_printf("main_callback: message %d to %lu sent with status %d\n",
			     e->event.ack.seq, e->event.ack.recipient, e->event.ack.status);
		break;
	default:
		debug_printf("main_callback: unsupported event %d\n", e->type);
		break;
	}
	gg_free_event(e);
}

void login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct agg_data *gd = gc->proto_data;
	struct gg_event *e;

	debug_printf("GG login_callback...\n");
	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}
	debug_printf("Found GG connection\n");
	if (gd->sess->fd != source) {
		gd->sess->fd = source;
		debug_printf("Setting sess->fd to source\n");
	}
	if (source == -1) {
		hide_login_progress(gc, _("Unable to connect."));
		signoff(gc);
		return;
	}
	debug_printf("Source is valid.\n");
	if (gc->inpa == 0) {
		debug_printf("login_callback.. checking gc->inpa .. is 0.. setting fd watch\n");
		gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, login_callback, gc);
		debug_printf("Adding watch on fd\n"); 
	}
	debug_printf("Checking State.\n");
	switch (gd->sess->state) {
	case GG_STATE_READING_DATA:
		set_login_progress(gc, 2, _("Reading data"));
		break;
	case GG_STATE_CONNECTING_GG:
		set_login_progress(gc, 3, _("Balancer handshake"));
		break;
	case GG_STATE_READING_KEY:
		set_login_progress(gc, 4, _("Reading server key"));
		break;
	case GG_STATE_READING_REPLY:
		set_login_progress(gc, 5, _("Exchanging key hash"));
		break;
	default:
		debug_printf("No State found\n");
		break;
	}
	debug_printf("gg_watch_fd\n");
	if (!(e = gg_watch_fd(gd->sess))) {
		debug_printf("login_callback: gg_watch_fd failed - CRITICAL!\n");
		hide_login_progress(gc, _("Critical error in GG library\n"));
		signoff(gc);
		return;
	}

	/* If we are GG_STATE_CONNECTING_GG then we still need to connect, as
	   we could not use proxy_connect in libgg.c */
	switch( gd->sess->state ) {
	case GG_STATE_CONNECTING_GG:
		{
			struct in_addr ip;
			char buf[256];
			
			/* Remove watch on initial socket - now that we have ip and port of login server */
			gaim_input_remove(gc->inpa);

			ip.s_addr = gd->sess->server_ip;
			gd->sess->fd = proxy_connect(inet_ntoa(ip), gd->sess->port, login_callback, gc);
			
			if (gd->sess->fd < 0) {
				g_snprintf(buf, sizeof(buf), _("Connect to %s failed"), inet_ntoa(ip));
				hide_login_progress(gc, buf);
				signoff(gc);
				return;
			}
			break;
		}
	case GG_STATE_READING_KEY:
		/* Set new watch on login server ip */
		if(gc->inpa)
			gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, login_callback, gc);
		debug_printf("Setting watch on connection with login server.\n"); 
		break;
	}/* end switch() */

	debug_printf("checking gg_event\n");
	switch (e->type) {
	case GG_EVENT_NONE:
		/* nothing */
		break;
	case GG_EVENT_CONN_SUCCESS:
		/* Setup new input handler */
		if (gc->inpa)
			gaim_input_remove(gc->inpa);
		gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, main_callback, gc);

		/* Our signon is complete */
		account_online(gc);
		serv_finish_login(gc);

		if (bud_list_cache_exists(gc))
			do_import(gc, NULL);
		break;
	case GG_EVENT_CONN_FAILED:
		gaim_input_remove(gc->inpa);
		gc->inpa = 0;
		handle_errcode(gc, e->event.failure);
		signoff(gc);
		break;
	default:
		debug_printf("no gg_event\n");
		break;
	}
	debug_printf("Returning from login_callback\n");
	gg_free_event(e);
}

static void agg_keepalive(struct gaim_connection *gc)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (gg_ping(gd->sess) < 0) {
		hide_login_progress(gc, _("Unable to ping server"));
		signoff(gc);
		return;
	}
}

static void agg_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct agg_data *gd = gc->proto_data = g_new0(struct agg_data, 1);
	char buf[80];

	gc->checkbox = _("Send as message");

	gd->sess = g_new0(struct gg_session, 1);

	if (user->proto_opt[USEROPT_NICK][0])
		g_snprintf(gc->displayname, sizeof(gc->displayname), "%s",
			   user->proto_opt[USEROPT_NICK]);

	set_login_progress(gc, 1, _("Looking up GG server"));

	if (invalid_uin(user->username)) {
		hide_login_progress(gc, _("Invalid Gadu-Gadu UIN specified"));
		signoff(gc);
		return;
	}

	gc->inpa = 0;

	/*
	   if (gg_login(gd->sess, strtol(user->username, (char **)NULL, 10), user->password, 1) < 0) {
	   debug_printf("uin=%u, pass=%s", strtol(user->username, (char **)NULL, 10), user->password); 
	   hide_login_progress(gc, "Unable to connect.");
	   signoff(gc);
	   return;
	   }

	   gg_login() sucks for me, so I'm using proxy_connect()
	 */

	gd->sess->uin = (uin_t) strtol(user->username, (char **)NULL, 10);
	gd->sess->password = g_strdup(user->password);
	gd->sess->state = GG_STATE_CONNECTING;
	gd->sess->check = GG_CHECK_WRITE;
	gd->sess->async = 1;
	gd->sess->fd = proxy_connect(GG_APPMSG_HOST, GG_APPMSG_PORT, login_callback, gc);

	if (gd->sess->fd < 0) {
		g_snprintf(buf, sizeof(buf), _("Connect to %s failed"), GG_APPMSG_HOST);
		hide_login_progress(gc, buf);
		signoff(gc);
		return;
	}
}

static void agg_close(struct gaim_connection *gc)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (gc->inpa)
		gaim_input_remove(gc->inpa);
	gg_logoff(gd->sess);
	gd->own_status = GG_STATUS_NOT_AVAIL;
	gg_free_session(gd->sess);
	g_free(gc->proto_data);
}

static int agg_send_im(struct gaim_connection *gc, char *who, char *msg, int len, int flags)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	gchar *imsg;

	if (invalid_uin(who)) {
		do_error_dialog(_("You are trying to send a message to an invalid Gadu-Gadu UIN."), NULL,
				GAIM_ERROR);
		return -1;
	}

	if (strlen(msg) > 0) {
		imsg = charset_convert(msg, "UTF-8", "CP1250");
		if (gg_send_message(gd->sess, (flags & IM_FLAG_CHECKBOX)
				    ? GG_CLASS_MSG : GG_CLASS_CHAT,
				    strtol(who, (char **)NULL, 10), imsg) < 0)
			return -1;
		g_free(imsg);
	}
	return 1;
}

static void agg_add_buddy(struct gaim_connection *gc, const char *who)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (invalid_uin(who))
		return;
	gg_add_notify(gd->sess, strtol(who, (char **)NULL, 10));
}

static void agg_rem_buddy(struct gaim_connection *gc, char *who, char *group)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (invalid_uin(who))
		return;
	gg_remove_notify(gd->sess, strtol(who, (char **)NULL, 10));
}

static void agg_add_buddies(struct gaim_connection *gc, GList *whos)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	uin_t *userlist = NULL;
	int userlist_size = 0;

	while (whos) {
		if (!invalid_uin(whos->data)) {
			userlist_size++;
			userlist = g_renew(uin_t, userlist, userlist_size);
			userlist[userlist_size - 1] =
			    (uin_t) strtol((char *)whos->data, (char **)NULL, 10);
		}
		whos = g_list_next(whos);
	}

	if (userlist) {
		gg_notify(gd->sess, userlist, userlist_size);
		g_free(userlist);
	}
}

static void search_results(struct gaim_connection *gc, gchar *webdata)
{
	gchar **webdata_tbl;
	gchar *buf;
	char *ptr;
	int i, j;

	if ((ptr = strstr(webdata, "query_results:")) == NULL || (ptr = strchr(ptr, '\n')) == NULL) {
		debug_printf("search_callback: pubdir result [%s]\n", webdata);
		do_error_dialog(_("Couldn't get search results"), NULL, GAIM_ERROR);
		return;
	}
	ptr++;

	buf = g_strconcat("<B>", _("Gadu-Gadu Search Engine"), "</B><BR>\n", NULL);

	webdata_tbl = g_strsplit(ptr, "\n", AGG_PUBDIR_MAX_ENTRIES);

	j = 0;

	/* Parse array */
	for (i = 0; webdata_tbl[i] != NULL; i++) {
		gchar *p, *oldibuf;
		static gchar *ibuf;

		g_strdelimit(webdata_tbl[i], "\t\n", ' ');

		/* GG_PUBDIR_HOST service returns 7 lines of data per directory entry */
		if (i % 8 == 0)
			j = 0;

		p = charset_convert(g_strstrip(webdata_tbl[i]), "CP1250", "UTF-8");

		oldibuf = ibuf;

		switch (j) {
		case 0:
			ibuf = g_strconcat("---------------------------------<BR>\n", NULL);
			oldibuf = ibuf;
			ibuf = g_strconcat(oldibuf, "<B>", _("Active"), ":</B> ",
					   (atoi(p) == 2) ? _("yes") : _("no"), "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 1:
			ibuf = g_strconcat(oldibuf, "<B>", _("UIN"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 2:
			ibuf = g_strconcat(oldibuf, "<B>", _("First name"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 3:
			ibuf =
			    g_strconcat(oldibuf, "<B>", _("Second Name"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 4:
			ibuf = g_strconcat(oldibuf, "<B>", _("Nick"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 5:
			/* Hack, invalid_uin does what we really want here but may change in future */
			if (invalid_uin(p))
				ibuf =
				    g_strconcat(oldibuf, "<B>", _("Birth year"), ":</B> <BR>\n", NULL);
			else
				ibuf =
				    g_strconcat(oldibuf, "<B>", _("Birth year"), ":</B> ", p, "<BR>\n",
						NULL);
			g_free(oldibuf);
			break;
		case 6:
			if (atoi(p) == GG_GENDER_FEMALE)
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> woman<BR>\n", NULL);
			else if (atoi(p) == GG_GENDER_MALE)
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> man<BR>\n", NULL);
			else
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> <BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 7:
			ibuf = g_strconcat(oldibuf, "<B>", _("City"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);

			/* We have all lines, so add them to buffer */
			{
				gchar *oldbuf = buf;
				buf = g_strconcat(oldbuf, ibuf, NULL);
				g_free(oldbuf);
			}

			g_free(ibuf);
			break;
		}

		g_free(p);

		j++;
	}

	g_strfreev(webdata_tbl);

	g_show_info_text(gc, NULL, 2, buf, NULL);

	g_free(buf);
}

static void import_buddies_server_results(struct gaim_connection *gc, gchar *webdata)
{
	gchar *ptr;
	gchar **users_tbl;
	int i;
	if (strstr(webdata, "no_data:")) {
		do_error_dialog(_("There is no Buddy List stored on the Gadu-Gadu server."), NULL, GAIM_ERROR);
		return;
	}

	if ((ptr = strstr(webdata, "get_results:")) == NULL || (ptr = strchr(ptr, ':')) == NULL) {
		debug_printf("import_buddies_server_results: import buddies result [%s]\n", webdata);
		do_error_dialog(_("Couldn't Import Buddy List from Server"), NULL, GAIM_ERROR);
		return;
	}
	ptr++;

	users_tbl = g_strsplit(ptr, "\n", AGG_PUBDIR_MAX_ENTRIES);

	/* Parse array of Buddies List */
	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show;

		g_strdelimit(users_tbl[i], "\r\t\n\015", ' ');
		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = data_tbl[3];
		name = data_tbl[6];

		if (invalid_uin(name)) {
			continue;
		}

		debug_printf("import_buddies_server_results: uin: %s\n", name);
		if (!find_buddy(gc, name)) {
			/* Default group if none specified on server */
			gchar *group = g_strdup("Gadu-Gadu");
			if (strlen(data_tbl[5])) {
				gchar **group_tbl = g_strsplit(data_tbl[5], ",", 2);
				if (strlen(group_tbl[0])) {
					g_free(group);
					group = g_strdup(group_tbl[0]);
				}
				g_strfreev(group_tbl);
			}
			/* Add Buddy to our userlist */
			add_buddy(gc, group, name, strlen(show) ? show : name);
			do_export(gc);
			g_free(group);
		}
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);
}

static void export_buddies_server_results(struct gaim_connection *gc, gchar *webdata)
{
	if (strstr(webdata, "put_success:")) {
		do_error_dialog(_("Buddy List successfully transferred to Gadu-Gadu server"), NULL, GAIM_INFO);
		return;
	}

	debug_printf("export_buddies_server_results: webdata [%s]\n", webdata);
	do_error_dialog(_("Couldn't transfer Buddy List to Gadu-Gadu server"), NULL, GAIM_ERROR);
}

static void delete_buddies_server_results(struct gaim_connection *gc, gchar *webdata)
{
	if (strstr(webdata, "put_success:")) {
		do_error_dialog(_("Buddy List successfully deleted from Gadu-Gadu server"), NULL, GAIM_INFO);
		return;
	}

	debug_printf("delete_buddies_server_results: webdata [%s]\n", webdata);
	do_error_dialog(_("Couldn't delete Buddy List from Gadu-Gadu server"), NULL, GAIM_ERROR);
}

static void password_change_server_results(struct gaim_connection *gc, gchar *webdata)
{
	if (strstr(webdata, "reg_success:")) {
		do_error_dialog(_("Password changed successfully"), NULL, GAIM_INFO);
		return;
	}

	debug_printf("delete_buddies_server_results: webdata [%s]\n", webdata);
	do_error_dialog(_("Password couldn't be changed"), NULL, GAIM_ERROR);
}

static void http_results(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_http *hdata = data;
	struct gaim_connection *gc = hdata->gc;
	char *webdata;
	int len;
	char read_data;

	debug_printf("http_results: begin\n");

	if (!g_slist_find(connections, gc)) {
		debug_printf("search_callback: g_slist_find error\n");
		gaim_input_remove(hdata->inpa);
		g_free(hdata);
		close(source);
		return;
	}

	webdata = NULL;
	len = 0;

	while (read(source, &read_data, 1) > 0 || errno == EWOULDBLOCK) {
		if (errno == EWOULDBLOCK) {
			errno = 0;
			continue;
		}

		if (!read_data)
			continue;

		len++;
		webdata = g_realloc(webdata, len);
		webdata[len - 1] = read_data;
	}

	webdata = g_realloc(webdata, len + 1);
	webdata[len] = 0;

	gaim_input_remove(hdata->inpa);
	close(source);

	debug_printf("http_results: type %d, webdata [%s]\n", hdata->type, webdata);

	switch (hdata->type) {
	case AGG_HTTP_SEARCH:
		search_results(gc, webdata);
		break;
	case AGG_HTTP_USERLIST_IMPORT:
		import_buddies_server_results(gc, webdata);
		break;
	case AGG_HTTP_USERLIST_EXPORT:
		export_buddies_server_results(gc, webdata);
		break;
	case AGG_HTTP_USERLIST_DELETE:
	        delete_buddies_server_results(gc, webdata);
		break;
	case AGG_HTTP_PASSWORD_CHANGE:
		password_change_server_results(gc, webdata);
		break;
	case AGG_HTTP_NONE:
	default:
		debug_printf("http_results: unsupported type %d\n", hdata->type);
		break;
	}

	g_free(webdata);
	g_free(hdata);
}

static void http_req_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_http *hdata = data;
	struct gaim_connection *gc = hdata->gc;
	gchar *request = hdata->request;
	gchar *buf;

	debug_printf("http_req_callback: begin\n");

	if (!g_slist_find(connections, gc)) {
		debug_printf("http_req_callback: g_slist_find error\n");
		g_free(request);
		g_free(hdata);
		close(source);
		return;
	}

	if (source == -1) {
		g_free(request);
		g_free(hdata);
		return;
	}

	debug_printf("http_req_callback: http request [%s]\n", request);

	buf = g_strdup_printf("POST %s HTTP/1.0\r\n"
			      "Host: %s\r\n"
			      "Content-Type: application/x-www-form-urlencoded\r\n"
			      "User-Agent: " GG_HTTP_USERAGENT "\r\n"
			      "Content-Length: %d\r\n"
			      "Pragma: no-cache\r\n" "\r\n" "%s\r\n",
			      hdata->form, hdata->host, strlen(request), request);

	g_free(request);

	if (write(source, buf, strlen(buf)) < strlen(buf)) {
		g_free(buf);
		g_free(hdata);
		close(source);
		do_error_dialog(_("Error communicating with Gadu-Gadu server"),
				_("Gaim was unable to complete your request due to a problem "
				  "communicating with the Gadu-Gadu HTTP server.  Please try again "
				  "later."), GAIM_ERROR);
		return;
	}

	g_free(buf);

	hdata->inpa = gaim_input_add(source, GAIM_INPUT_READ, http_results, hdata);
}

static void import_buddies_server(struct gaim_connection *gc)
{
	struct agg_http *hi = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gc->username);
	gchar *p = gg_urlencode(gc->password);

	hi->gc = gc;
	hi->type = AGG_HTTP_USERLIST_IMPORT;
	hi->form = AGG_PUBDIR_USERLIST_IMPORT_FORM;
	hi->host = GG_PUBDIR_HOST;
	hi->request = g_strdup_printf("FmNum=%s&Pass=%s", u, p);

	g_free(u);
	g_free(p);

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, hi) < 0) {
		do_error_dialog(_("Unable to import Gadu-Gadu buddy list"), 
				_("Gaim was unable to connect to the Gadu-Gadu buddy list "
				  "server.  Please try again later."), GAIM_ERROR);
		g_free(hi->request);
		g_free(hi);
		return;
	}
}

static void export_buddies_server(struct gaim_connection *gc)
{
	struct agg_http *he = g_new0(struct agg_http, 1);
	gchar *ptr;
	gchar *u = gg_urlencode(gc->username);
	gchar *p = gg_urlencode(gc->password);

	GSList *gr = gc->groups;

	he->gc = gc;
	he->type = AGG_HTTP_USERLIST_EXPORT;
	he->form = AGG_PUBDIR_USERLIST_EXPORT_FORM;
	he->host = GG_PUBDIR_HOST;
	he->request = g_strdup_printf("FmNum=%s&Pass=%s&Contacts=", u, p);

	g_free(u);
	g_free(p);

	while (gr) {
		struct group *g = gr->data;
		GSList *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			gchar *newdata;
			/* GG Number */
			gchar *name = gg_urlencode(b->name);
			/* GG Pseudo */
			gchar *show = gg_urlencode(b->alias[0] ? b->alias : b->name);
			/* Group Name */
			gchar *gname = gg_urlencode(g->name);

			ptr = he->request;
			newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s\r\n",
						  show, show, show, show, "", gname, name);
			he->request = g_strconcat(ptr, newdata, NULL);

			g_free(newdata);
			g_free(ptr);

			g_free(gname);
			g_free(show);
			g_free(name);

			m = g_slist_next(m);
		}
		gr = g_slist_next(gr);
	}

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, he) < 0) {
		do_error_dialog(_("Couldn't export buddy list"), 
				_("Gaim was unable to connect to the buddy list server.  "
				  "Please try again later."), GAIM_ERROR);
		g_free(he->request);
		g_free(he);
		return;
	}
}

static void delete_buddies_server(struct gaim_connection *gc)
{
	struct agg_http *he = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gc->username);
	gchar *p = gg_urlencode(gc->password);

	he->gc = gc;
	he->type = AGG_HTTP_USERLIST_DELETE;
	he->form = AGG_PUBDIR_USERLIST_EXPORT_FORM;
	he->host = GG_PUBDIR_HOST;
	he->request = g_strdup_printf("FmNum=%s&Pass=%s&Delete=1", u, p);

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, he) < 0) {
		do_error_dialog(_("Unable to delete Gadu-Gadu buddy list"), 
				_("Gaim was unable to connect to the buddy list server.  "
				  "Please try again later."), GAIM_ERROR);
		g_free(he->request);
		g_free(he);
		return;
	}
}

static void agg_dir_search(struct gaim_connection *gc, const char *first, const char *middle,
			   const char *last, const char *maiden, const char *city, const char *state,
			   const char *country, const char *email)
{
	struct agg_http *srch = g_new0(struct agg_http, 1);
	srch->gc = gc;
	srch->type = AGG_HTTP_SEARCH;
	srch->form = AGG_PUBDIR_SEARCH_FORM;
	srch->host = GG_PUBDIR_HOST;

	if (email && strlen(email)) {
		gchar *eemail = gg_urlencode(email);
		srch->request = g_strdup_printf("Mode=1&Email=%s", eemail);
		g_free(eemail);
	} else {
		gchar *new_first = charset_convert(first, "UTF-8", "CP1250");
		gchar *new_last = charset_convert(last, "UTF-8", "CP1250");
		gchar *new_city = charset_convert(city, "UTF-8", "CP1250");

		gchar *enew_first = gg_urlencode(new_first);
		gchar *enew_last = gg_urlencode(new_last);
		gchar *enew_city = gg_urlencode(new_city);

		g_free(new_first);
		g_free(new_last);
		g_free(new_city);

		/* For active only add &ActiveOnly= */
		srch->request = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						"&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						enew_first, enew_last, AGG_GENDER_NONE,
						"", enew_city, 0, 0);

		g_free(enew_first);
		g_free(enew_last);
		g_free(enew_city);
	}

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, srch) < 0) {
		do_error_dialog(_("Unable to access directory"), 
				_("Gaim was unable to search the Directory because it "
				  "was unable to connect to the directory server.  Please try "
				  "again later."), GAIM_ERROR);
		g_free(srch->request);
		g_free(srch);
		return;
	}
}

static void agg_change_passwd(struct gaim_connection *gc, const char *old, const char *new)
{
	struct agg_http *hpass = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gc->username);
	gchar *p = gg_urlencode(gc->password);
	gchar *enew = gg_urlencode(new);
	gchar *eold = gg_urlencode(old);

	hpass->gc = gc;
	hpass->type = AGG_HTTP_PASSWORD_CHANGE;
	hpass->form = AGG_REGISTER_DATA_FORM;
	hpass->host = GG_REGISTER_HOST;

	/* We are using old password as place for email - it's ugly */
	hpass->request = g_strdup_printf("fmnumber=%s&fmpwd=%s&pwd=%s&email=%s&code=%u",
					 u, p, enew, eold, gg_http_hash(old, new));

	g_free(u);
	g_free(p);
	g_free(enew);
	g_free(eold);

	if (proxy_connect(GG_REGISTER_HOST, GG_REGISTER_PORT, http_req_callback, hpass) < 0) {
	       	do_error_dialog(_("Unable to change Gadu-Gadu password"), 
				_("Gaim was unable to change your password due to an error connecting "
				  "to the Gadu-Gadu server.  Please try again later."), GAIM_ERROR);
		g_free(hpass->request);
		g_free(hpass);
		return;
	}                                        
}

static void agg_do_action(struct gaim_connection *gc, char *action)
{
	if (!strcmp(action, _("Directory Search"))) {
		show_find_info(gc);
	} else if (!strcmp(action, _("Change Password"))) {
		show_change_passwd(gc);
	} else if (!strcmp(action, _("Import Buddy List from Server"))) {
		import_buddies_server(gc);
	} else if (!strcmp(action, _("Export Buddy List to Server"))) {
		export_buddies_server(gc);
	} else if (!strcmp(action, _("Delete Buddy List from Server"))) {
	        delete_buddies_server(gc);
	}
}

static GList *agg_actions()
{
	GList *m = NULL;

	m = g_list_append(m, _("Directory Search"));
	m = g_list_append(m, NULL);
	m = g_list_append(m, _("Change Password"));
	m = g_list_append(m, NULL);
	m = g_list_append(m, _("Import Buddy List from Server"));
	m = g_list_append(m, _("Export Buddy List to Server"));
	m = g_list_append(m, _("Delete Buddy List from Server"));

	return m;
}

static void agg_get_info(struct gaim_connection *gc, char *who)
{
	struct agg_http *srch = g_new0(struct agg_http, 1);
	srch->gc = gc;
	srch->type = AGG_HTTP_SEARCH;
	srch->form = AGG_PUBDIR_SEARCH_FORM;
	srch->host = GG_PUBDIR_HOST;

	/* If it's invalid uin then maybe it's nickname? */
	if (invalid_uin(who)) {
		gchar *new_who = charset_convert(who, "UTF-8", "CP1250");
		gchar *enew_who = gg_urlencode(new_who);
		
		g_free(new_who);

		srch->request = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						"&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						"", "", AGG_GENDER_NONE, enew_who, "", 0, 0);

		g_free(enew_who);
	} else
		srch->request = g_strdup_printf("Mode=3&UserId=%s", who);

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, srch) < 0) {
		do_error_dialog(_("Unable to access user profile."), 
				_("Gaim was unable to access this user's profile due to an error "
				  "connecting to the directory server.  Please try again later."), GAIM_ERROR);
		g_free(srch->request);
		g_free(srch);
		return;
	}
}

static char **agg_list_icon(int uc)
{
	guint status;
	if (uc == UC_UNAVAILABLE)
		return (char **)gg_sunred_xpm;
	status = uc >> 5;
	/* Drop all masks */
	status &= ~(GG_STATUS_FRIENDS_MASK);
	if (status == GG_STATUS_AVAIL)
		return (char **)gg_sunyellow_xpm;
	if (status == GG_STATUS_BUSY)
		return (char **)gg_suncloud_xpm;
	if (status == GG_STATUS_INVISIBLE)
		return (char **)gg_sunwhitered_xpm;
	return (char **)gg_sunyellow_xpm;
}

static void agg_set_permit_deny_dummy(struct gaim_connection *gc)
{
	/* It's implemented on client side because GG server doesn't support this */
}

static void agg_permit_deny_dummy(struct gaim_connection *gc, char *who)
{
	/* It's implemented on client side because GG server doesn't support this */
}

static struct prpl *my_protocol = NULL;

G_MODULE_EXPORT void gg_init(struct prpl *ret)
{
	struct proto_user_opt *puo;
	ret->protocol = PROTO_GADUGADU;
	ret->options = 0;
	ret->name = g_strdup("Gadu-Gadu");
	ret->list_icon = agg_list_icon;
	ret->away_states = agg_away_states;
	ret->actions = agg_actions;
	ret->do_action = agg_do_action;
	ret->buddy_menu = agg_buddy_menu;
	ret->chat_info = NULL;
	ret->login = agg_login;
	ret->close = agg_close;
	ret->send_im = agg_send_im;
	ret->set_info = NULL;
	ret->get_info = agg_get_info;
	ret->set_away = agg_set_away;
	ret->set_dir = NULL;
	ret->get_dir = agg_get_info;
	ret->dir_search = agg_dir_search;
	ret->set_idle = NULL;
	ret->change_passwd = agg_change_passwd;
	ret->add_buddy = agg_add_buddy;
	ret->add_buddies = agg_add_buddies;
	ret->remove_buddy = agg_rem_buddy;
	ret->add_permit = agg_permit_deny_dummy;
	ret->add_deny = agg_permit_deny_dummy;
	ret->rem_permit = agg_permit_deny_dummy;
	ret->rem_deny = agg_permit_deny_dummy;
	ret->set_permit_deny = agg_set_permit_deny_dummy;
	ret->warn = NULL;
	ret->join_chat = NULL;
	ret->chat_invite = NULL;
	ret->chat_leave = NULL;
	ret->chat_whisper = NULL;
	ret->chat_send = NULL;
	ret->keepalive = agg_keepalive;
	ret->normalize = NULL;
     
	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Nick:"));
	puo->def = g_strdup(_("Gadu-Gadu User"));
	puo->pos = USEROPT_NICK;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	my_protocol = ret;
}

#ifndef STATIC

G_MODULE_EXPORT void gaim_prpl_init(struct prpl *prpl)
{
	gg_init(prpl);
	prpl->plug->desc.api_version = PLUGIN_API_VERSION;
}

#endif

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
