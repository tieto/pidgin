/*
 * gaim - Gadu-Gadu Protocol Plugin
 * $Id: gg.c 2804 2001-11-26 20:39:54Z warmenhoven $
 *
 * Copyright (C) 2001, Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
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

#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ctype.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#ifdef HAVE_ICONV
#include <iconv.h>
#include "iconv_string.h"
#endif
/* Library from EKG (Eksperymentalny Klient Gadu-Gadu) */
#include "libgg.h"
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"

#include "pixmaps/gg_suncloud.xpm"
#include "pixmaps/gg_sunred.xpm"
#include "pixmaps/gg_sunwhitered.xpm"
#include "pixmaps/gg_sunyellow.xpm"

#define USEROPT_NICK 0

#define AGG_BUF_LEN 1024

#define AGG_GENDER_NONE -1

#define AGG_PUBDIR_FORM "/appsvc/fmpubquery2.asp"
#define AGG_PUBDIR_MAX_ENTRIES 200

#define AGG_STATUS_AVAIL              _("Available")
#define AGG_STATUS_AVAIL_FRIENDS      _("Available for friends only")
#define AGG_STATUS_BUSY               _("Away")
#define AGG_STATUS_BUSY_FRIENDS       _("Away for friends only")
#define AGG_STATUS_INVISIBLE          _("Invisible")
#define AGG_STATUS_INVISIBLE_FRIENDS  _("Invisible for friends only")
#define AGG_STATUS_NOT_AVAIL          _("Unavailable")

#define UC_NORMAL 2

struct agg_data {
	struct gg_session *sess;
};

struct agg_search {
	struct gaim_connection *gc;
	gchar *search_data;
	int inpa;
};

static char *agg_name()
{
	return "Gadu-Gadu";
}

static gchar *charset_convert(const gchar *locstr, char *encsrc, char *encdst)
{
#ifdef HAVE_ICONV
	gchar *result = NULL;
	if (iconv_string(encdst, encsrc, locstr, locstr+strlen(locstr)+1, &result, NULL) >= 0)
		return result;
#endif
	return g_strdup(locstr);
}

static gboolean invalid_uin(char *uin)
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

static gchar *find_local_charset(void)
{
	gchar *gg_localenc = g_getenv("GG_CHARSET");

	if (gg_localenc == NULL) {
#ifdef HAVE_LANGINFO_CODESET
		gg_localenc = nl_langinfo(CODESET);
#else
		gg_localenc = "US-ASCII";
#endif
	}
	return gg_localenc;
}

static char *handle_errcode(int errcode, gboolean show)
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
		g_snprintf(msg, sizeof(msg), _("Error while writting to socket."));
		break;
	case GG_FAILURE_PASSWORD:
		g_snprintf(msg, sizeof(msg), _("Authentification failed."));
		break;
	default:
		g_snprintf(msg, sizeof(msg), _("Unknown Error Code."));
		break;
	}

	if (show)
		do_error_dialog(msg, _("Gadu-Gadu Error"));

	return msg;
}

static gchar *encode_postdata(const gchar *data)
{
	gchar *p = NULL;
	int i, j = 0;
	for (i = 0; i < strlen(data); i++) {
		/* locale insensitive, doesn't reflect RFC (1738 section 2.2, 1866 section 8.2.1) */
		if ((data[i] >= 'a' && data[i] <= 'z')
		    || (data[i] >= 'A' && data[i] <= 'Z')
		    || (data[i] >= '0' && data[i] <= '9')
		    || data[i] == '=' || data[i] == '&'
		    || data[i] == '\n' || data[i] == '\r' || data[i] == '\t' || data[i] == '\014') {
			p = g_realloc(p, j + 1);
			p[j] = data[i];
			j++;
		} else {
			p = g_realloc(p, j + 4);	/* remember, sprintf appends a '\0' */
			sprintf(p + j, "%%%02x", (unsigned char)data[i]);
			j += 3;
		}
	}
	p = g_realloc(p, j + 1);
	p[j] = '\0';

	if (p && strlen(p))
		return p;
	else
		return g_strdup(data);
}

static void agg_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;

	if (gc->away)
		gc->away = NULL;

	if (!g_strcasecmp(state, AGG_STATUS_AVAIL))
		gg_change_status(gd->sess, GG_STATUS_AVAIL);
	else if (!g_strcasecmp(state, AGG_STATUS_AVAIL_FRIENDS))
		gg_change_status(gd->sess, GG_STATUS_AVAIL | GG_STATUS_FRIENDS_MASK);
	else if (!g_strcasecmp(state, AGG_STATUS_BUSY)) {
		gg_change_status(gd->sess, GG_STATUS_BUSY);
		gc->away = "";
	} else if (!g_strcasecmp(state, AGG_STATUS_BUSY_FRIENDS)) {
		gg_change_status(gd->sess, GG_STATUS_BUSY | GG_STATUS_FRIENDS_MASK);
		gc->away = "";
	} else if (!g_strcasecmp(state, AGG_STATUS_INVISIBLE)) {
		gg_change_status(gd->sess, GG_STATUS_INVISIBLE);
		gc->away = "";
	} else if (!g_strcasecmp(state, AGG_STATUS_INVISIBLE_FRIENDS)) {
		gg_change_status(gd->sess, GG_STATUS_INVISIBLE | GG_STATUS_FRIENDS_MASK);
		gc->away = "";
	} else if (!g_strcasecmp(state, AGG_STATUS_NOT_AVAIL)) {
		gg_change_status(gd->sess, GG_STATUS_NOT_AVAIL);
		gc->away = "";
	} else if (!g_strcasecmp(state, GAIM_AWAY_CUSTOM)) {
		if (msg) {
			gg_change_status(gd->sess, GG_STATUS_BUSY);
			gc->away = "";
		} else
			gg_change_status(gd->sess, GG_STATUS_AVAIL);
	}
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

static GList *agg_user_opts()
{
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = _("Nick:");
	puo->def = _("Gadu-Gadu User");
	puo->pos = USEROPT_NICK;
	m = g_list_append(m, puo);

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
		signoff(gc);
		return;
	}

	if (!(e = gg_watch_fd(gd->sess))) {
		debug_printf("main_callback: gg_watch_fd failed - CRITICAL!\n");
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
		handle_errcode(e->event.failure, TRUE);
		signoff(gc);
		break;
	case GG_EVENT_MSG:
		{
			gchar *imsg;
			gchar user[20];

			g_snprintf(user, sizeof(user), "%u", e->event.msg.sender);
			if (!allowed_uin(gc, user))
				break;
			imsg = charset_convert(e->event.msg.message, "CP1250", find_local_charset());
			serv_got_im(gc, user, imsg, 0, time((time_t) NULL));
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
				case GG_STATUS_FRIENDS_MASK:
					status = UC_NORMAL | (e->event.status.status << 5);
					break;
				default:
					status = UC_NORMAL;
					break;
				}

				g_snprintf(user, sizeof(user), "%u", n->uin);
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
			case GG_STATUS_FRIENDS_MASK:
				status = UC_NORMAL | (e->event.status.status << 5);
				break;
			default:
				status = UC_NORMAL;
				break;
			}

			g_snprintf(user, sizeof(user), "%u", e->event.status.uin);
			serv_got_update(gc, user, (status == UC_UNAVAILABLE) ? 0 : 1, 0, 0, 0, status,
					0);
		}
		break;
	case GG_EVENT_ACK:
		debug_printf("main_callback: message %d to %u sent with status %d\n",
			     e->event.ack.seq, e->event.ack.recipient, e->event.ack.status);
		break;
	default:
		debug_printf("main_callback: unsupported event %d\n", e->type);
		break;
	}
	gg_free_event(e);
}

static void login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct agg_data *gd = gc->proto_data;
	struct gg_event *e;

	if (!g_slist_find(connections, data)) {
		close(source);
		return;
	}

	if (gd->sess->fd != source)
		gd->sess->fd = source;

	if (source == -1) {
		hide_login_progress(gc, _("Unable to connect."));
		signoff(gc);
		return;
	}

	if (gc->inpa == 0)
		gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, login_callback, gc);

	switch (gd->sess->state) {
	case GG_STATE_CONNECTING_HTTP:
	case GG_STATE_WRITING_HTTP:
		set_login_progress(gc, 2, _("Handshake"));
	case GG_STATE_CONNECTING_GG:
		set_login_progress(gc, 3, _("Connecting to GG server"));
		break;
	case GG_STATE_WAITING_FOR_KEY:
		set_login_progress(gc, 4, _("Waiting for server key"));
	case GG_STATE_SENDING_KEY:
		set_login_progress(gc, 5, _("Sending key"));
		break;
	default:
		break;
	}

	if (!(e = gg_watch_fd(gd->sess))) {
		debug_printf("login_callback: gg_watch_fd failed - CRITICAL!\n");
		signoff(gc);
		return;
	}

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
		handle_errcode(e->event.failure, TRUE);
		signoff(gc);
		break;
	default:
		break;
	}

	gg_free_event(e);
}

static void agg_keepalive(struct gaim_connection *gc)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (gg_ping(gd->sess) < 0) {
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
	gd->sess->state = GG_STATE_CONNECTING_HTTP;
	gd->sess->check = GG_CHECK_WRITE;
	gd->sess->async = 1;
	gd->sess->recv_left = 0;
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
	gg_free_session(gd->sess);
	g_free(gc->proto_data);
}

static int agg_send_im(struct gaim_connection *gc, char *who, char *msg, int flags)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	gchar *imsg;

	if (invalid_uin(who)) {
		do_error_dialog(_("You are trying to send message to invalid Gadu-Gadu UIN!"),
				_("Gadu-Gadu Error"));
		return -1;
	}

	if (strlen(msg) > 0) {
		imsg = charset_convert(msg, find_local_charset(), "CP1250");
		if (gg_send_message(gd->sess, (flags & IM_FLAG_CHECKBOX) ? GG_CLASS_MSG : GG_CLASS_CHAT,
				    strtol(who, (char **)NULL, 10), imsg) < 0)
			return -1;
		g_free(imsg);
	}
	return 1;
}

static void agg_add_buddy(struct gaim_connection *gc, char *who)
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

static void search_results(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_search *srch = data;
	struct gaim_connection *gc = srch->gc;
	gchar *buf;
	char *ptr;
	char *webdata;
	int len;
	char read_data;
	gchar **webdata_tbl;
	int i, j;

	if (!g_slist_find(connections, gc)) {
		debug_printf("search_callback: g_slist_find error\n");
		gaim_input_remove(srch->inpa);
		g_free(srch);
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

	gaim_input_remove(srch->inpa);
	g_free(srch);
	close(source);

	if ((ptr = strstr(webdata, "query_results:")) == NULL || (ptr = strchr(ptr, '\n')) == NULL) {
		debug_printf("search_callback: pubdir result [%s]\n", webdata);
		g_free(webdata);
		do_error_dialog(_("Couldn't get search results"), _("Gadu-Gadu Error"));
		return;
	}
	ptr++;

	buf = g_strconcat("<B>", _("Gadu-Gadu Search Engine"), "</B><BR>\n", NULL);

	webdata_tbl = g_strsplit(ptr, "\n", AGG_PUBDIR_MAX_ENTRIES);

	g_free(webdata);

	j = 0;

	/* Parse array */
	for (i = 0; webdata_tbl[i] != NULL; i++) {
		gchar *p, *oldibuf;
		static gchar *ibuf;

		g_strdelimit(webdata_tbl[i], "\t\n", ' ');

		/* GG_PUBDIR_HOST service returns 7 lines of data per directory entry */
		if (i % 8 == 0)
			j = 0;

		p = charset_convert(g_strstrip(webdata_tbl[i]), "CP1250", find_local_charset());

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

static void search_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_search *srch = data;
	struct gaim_connection *gc = srch->gc;
	gchar *search_data = srch->search_data;
	gchar *buf;
	char *ptr;

	debug_printf("search_callback enter: begin\n");

	if (!g_slist_find(connections, gc)) {
		debug_printf("search_callback: g_slist_find error\n");
		g_free(search_data);
		g_free(srch);
		close(source);
		return;
	}

	if (source == -1) {
		g_free(search_data);
		g_free(srch);
		return;
	}

	ptr = encode_postdata(search_data);
	g_free(search_data);

	debug_printf("search_callback: pubdir request [%s]\n", ptr);

	buf = g_strdup_printf("POST " AGG_PUBDIR_FORM " HTTP/1.0\r\n"
			      "Host: " GG_PUBDIR_HOST "\r\n"
			      "Content-Type: application/x-www-form-urlencoded\r\n"
			      "User-Agent: Mozilla/4.7 [en] (Win98; I)\r\n"
			      "Content-Length: %d\r\n"
			      "Pragma: no-cache\r\n" "\r\n" "%s\r\n", strlen(ptr), ptr);

	g_free(ptr);

	if (write(source, buf, strlen(buf)) < strlen(buf)) {
		g_free(buf);
		g_free(srch);
		close(source);
		do_error_dialog(_("Couldn't send search request"), _("Gadu-Gadu Error"));
		return;
	}

	g_free(buf);

	srch->inpa = gaim_input_add(source, GAIM_INPUT_READ, search_results, srch);
}

static void agg_dir_search(struct gaim_connection *gc, char *first, char *middle,
			   char *last, char *maiden, char *city, char *state, char *country, char *email)
{
	struct agg_search *srch = g_new0(struct agg_search, 1);
	static char msg[AGG_BUF_LEN];

	srch->gc = gc;

	if (email && strlen(email)) {
		srch->search_data = g_strdup_printf("Mode=1&Email=%s", email);
	} else {
		gchar *new_first = charset_convert(first, find_local_charset(), "CP1250");
		gchar *new_last = charset_convert(last, find_local_charset(), "CP1250");
		gchar *new_city = charset_convert(city, find_local_charset(), "CP1250");

		/* For active only add &ActiveOnly= */
		srch->search_data = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						    "&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						    new_first, new_last, AGG_GENDER_NONE,
						    "", new_city, 0, 0);

		g_free(new_first);
		g_free(new_last);
		g_free(new_city);
	}

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, search_callback, srch) < 0) {
		g_snprintf(msg, sizeof(msg), _("Connect to search service failed (%s)"), GG_PUBDIR_HOST);
		do_error_dialog(msg, _("Gadu-Gadu Error"));
		g_free(srch->search_data);
		g_free(srch);
		return;
	}
}

static void agg_do_action(struct gaim_connection *gc, char *action)
{
	if (!strcmp(action, _("Directory Search"))) {
		show_find_info(gc);
	}
}

static GList *agg_actions()
{
	GList *m = NULL;

	m = g_list_append(m, _("Directory Search"));

	return m;
}

static void agg_get_info(struct gaim_connection *gc, char *who)
{
	struct agg_search *srch = g_new0(struct agg_search, 1);
	static char msg[AGG_BUF_LEN];

	srch->gc = gc;

	/* If it's invalid uin then maybe it's nickname? */
	if (invalid_uin(who)) {
		gchar *new_who = charset_convert(who, find_local_charset(), "CP1250");

		srch->search_data = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						    "&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						    "", "", AGG_GENDER_NONE, new_who, "", 0, 0);

		g_free(new_who);
	} else
		srch->search_data = g_strdup_printf("Mode=3&UserId=%s", who);

	if (proxy_connect(GG_PUBDIR_HOST, GG_PUBDIR_PORT, search_callback, srch) < 0) {
		g_snprintf(msg, sizeof(msg), _("Connect to search service failed (%s)"), GG_PUBDIR_HOST);
		do_error_dialog(msg, _("Gadu-Gadu Error"));
		g_free(srch->search_data);
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

void gg_init(struct prpl *ret)
{
	ret->protocol = PROTO_GADUGADU;
	ret->options = 0;
	ret->name = agg_name;
	ret->list_icon = agg_list_icon;
	ret->away_states = agg_away_states;
	ret->actions = agg_actions;
	ret->do_action = agg_do_action;
	ret->user_opts = agg_user_opts;
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
	ret->change_passwd = NULL;
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
	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(gg_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_GADUGADU);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "Gadu-Gadu";
}

char *description()
{
	return PRPL_DESC("Gadu-Gadu");
}

#endif
