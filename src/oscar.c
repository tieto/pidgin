/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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
#include "../config.h"
#endif


#include <netdb.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "aim.h"
#include "proxy.h"

#if USE_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#endif

/*#include "pixmaps/cancel.xpm"*/
#include "pixmaps/admin_icon.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/away_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/free_icon.xpm"

/* constants to identify proto_opts */
#define USEROPT_AUTH      0
#define USEROPT_AUTHPORT  1

#define AIMHASHDATA "http://gaim.sourceforge.net/aim_data.php3"

static int gaim_caps = AIM_CAPS_CHAT |
#if USE_PIXBUF
		       AIM_CAPS_BUDDYICON |
#endif
		       AIM_CAPS_GETFILE |
		       AIM_CAPS_IMIMAGE;

static GtkWidget *join_chat_spin = NULL;
static GtkWidget *join_chat_entry = NULL;

struct oscar_data {
	struct aim_session_t *sess;
	struct aim_conn_t *conn;

	guint cnpa;
	guint paspa;

	int create_exchange;
	char *create_name;

	gboolean conf;
	gboolean reqemail;
	gboolean chpass;
	char *oldp;
	char *newp;

	GSList *oscar_chats;
	GSList *direct_ims;
	GSList *getfiles;
	GSList *hasicons;
};

struct chat_connection {
	char *name;
	char *show; /* AOL did something funny to us */
	int exchange;
	int fd; /* this is redundant since we have the conn below */
	struct aim_conn_t *conn;
	int inpa;
	int id;
	struct gaim_connection *gc; /* i hate this. */
	struct conversation *cnv; /* bah. */
};

struct direct_im {
	struct gaim_connection *gc;
	char name[80];
	struct conversation *cnv;
	int watcher;
	struct aim_conn_t *conn;
};

struct ask_direct {
	struct gaim_connection *gc;
	char *sn;
	struct aim_directim_priv *priv;
};

struct ask_getfile {
	struct gaim_connection *gc;
	char *sn;
	char *cookie;
	char *ip;
};

struct getfile_transfer {
	struct gaim_connection *gc;
	char *receiver;
	char *filename;
	struct aim_conn_t *conn;
	struct aim_fileheader_t *fh;
	int gip;
	int gop;
	FILE *listing;
	FILE *file;
	GtkWidget *window;
	GtkWidget *meter;
	GtkWidget *label;
	long pos;
	long size;
};

#if USE_PIXBUF
struct icon_req {
	char *user;
	time_t timestamp;
	unsigned long length;
	gpointer data;
	gboolean request;
	GdkPixbufAnimation *anim;
	GdkPixbuf *unanim;
	struct conversation *cnv;
	GtkWidget *pix;
	int curframe;
	int timer;
};
#endif

static struct direct_im *find_direct_im(struct oscar_data *od, char *who) {
	GSList *d = od->direct_ims;
	char *n = g_strdup(normalize(who));
	struct direct_im *m = NULL;

	while (d) {
		m = (struct direct_im *)d->data;
		if (!strcmp(n, normalize(m->name)))
			break;
		m = NULL;
		d = d->next;
	}

	g_free(n);
	return m;
}

/*
static struct getfile_transfer *find_getfile_transfer(struct oscar_data *od, struct aim_conn_t *conn) {
	GSList *g = od->getfiles;
	struct getfile_transfer *n = NULL;

	while (g) {
		n = (struct getfile_transfer *)g->data;
		if (n->conn == conn)
			return n;
		n = NULL;
		g = g->next;
	}

	return n;
}
*/

static char *extract_name(char *name) {
	char *tmp;
	int i, j;
	char *x = strchr(name, '-');
	if (!x) return NULL;
	x = strchr(++x, '-');
	if (!x) return NULL;
	tmp = g_strdup(++x);

	for (i = 0, j = 0; x[i]; i++) {
		if (x[i] != '%')
			tmp[j++] = x[i];
		else {
			char hex[3];
			hex[0] = x[++i];
			hex[1] = x[++i];
			hex[2] = 0;
			sscanf(hex, "%x", (int *)&tmp[j++]);
		}
	}

	tmp[j] = 0;
	return tmp;
}

static struct chat_connection *find_oscar_chat(struct gaim_connection *gc, int id) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;
	if (gc->protocol != PROTO_OSCAR) return NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->id == id)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct chat_connection *find_oscar_chat_by_conn(struct gaim_connection *gc,
							struct aim_conn_t *conn) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->conn == conn)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static int gaim_parse_auth_resp  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_login      (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_server_ready     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_handle_redirect  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_info_change      (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_account_confirm  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_oncoming   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_offgoing   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_incoming_im(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_misses     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_user_info  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_motd       (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chatnav_info     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_join        (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_leave       (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_info_update (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_incoming_msg(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_msgack     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_ratechange (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_evilnotify (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_searcherror(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_searchreply(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_bosrights        (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_rateresp         (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_reportinterval   (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_msgerr     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_buddyrights(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_locerr     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_parse_genericerr (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_memrequest       (struct aim_session_t *, struct command_rx_struct *, ...);

static int gaim_directim_initiate  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_directim_incoming  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_directim_disconnect(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_directim_typing    (struct aim_session_t *, struct command_rx_struct *, ...);

static char *msgerrreason[] = {
	"Invalid error",
	"Invalid SNAC",
	"Rate to host",
	"Rate to client",
	"Not logged in",
	"Service unavailable",
	"Service not defined",
	"Obsolete SNAC",
	"Not supported by host",
	"Not supported by client",
	"Refused by client",
	"Reply too big",
	"Responses lost",
	"Request denied",
	"Busted SNAC payload",
	"Insufficient rights",
	"In local permit/deny",
	"Too evil (sender)",
	"Too evil (receiver)",
	"User temporarily unavailable",
	"No match",
	"List overflow",
	"Request ambiguous",
	"Queue full",
	"Not while on AOL"
};
static int msgerrreasonlen = 25;

static void oscar_callback(gpointer data, gint source,
				GdkInputCondition condition) {
	struct aim_conn_t *conn = (struct aim_conn_t *)data;
	struct aim_session_t *sess = aim_conn_getsess(conn);
	struct gaim_connection *gc = sess ? sess->aux_data : NULL;
	struct oscar_data *odata;

	if (!gc) {
		/* gc is null. we return, else we seg SIGSEG on next line. */
		debug_printf("oscar callback for closed connection (1).\n");
		return;
	}
      
	odata = (struct oscar_data *)gc->proto_data;

	if (!g_slist_find(connections, gc)) {
		/* oh boy. this is probably bad. i guess the only thing we 
		 * can really do is return? */
		debug_printf("oscar callback for closed connection (2).\n");
		return;
	}

	if (condition & GDK_INPUT_EXCEPTION) {
		hide_login_progress(gc, _("Disconnected."));
		signoff(gc);
		return;
	}
	if (condition & GDK_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) {
			debug_printf("got information on rendezvous\n");
			if (aim_handlerendconnect(odata->sess, conn) < 0) {
				debug_printf(_("connection error (rend)\n"));
			}
		} else {
			if (aim_get_command(odata->sess, conn) >= 0) {
				aim_rxdispatch(odata->sess);
			} else {
				if ((conn->type == AIM_CONN_TYPE_BOS) ||
					   !(aim_getconn_type(odata->sess, AIM_CONN_TYPE_BOS))) {
					debug_printf(_("major connection error\n"));
					hide_login_progress(gc, _("Disconnected."));
					signoff(gc);
				} else if (conn->type == AIM_CONN_TYPE_CHAT) {
					struct chat_connection *c = find_oscar_chat_by_conn(gc, conn);
					char buf[BUF_LONG];
					debug_printf("disconnected from chat room %s\n", c->name);
					c->conn = NULL;
					if (c->inpa > 0)
						gdk_input_remove(c->inpa);
					c->inpa = 0;
					c->fd = -1;
					aim_conn_kill(odata->sess, &conn);
					sprintf(buf, _("You have been disconnected from chat room %s."), c->name);
					do_error_dialog(buf, _("Chat Error!"));
				} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
					if (odata->cnpa > 0)
						gdk_input_remove(odata->cnpa);
					odata->cnpa = 0;
					debug_printf("removing chatnav input watcher\n");
					if (odata->create_exchange) {
						odata->create_exchange = 0;
						g_free(odata->create_name);
						odata->create_name = NULL;
						do_error_dialog(_("Chat is currently unavailable"),
								_("Gaim - Chat"));
					}
					aim_conn_kill(odata->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_AUTH) {
					if (odata->paspa > 0)
						gdk_input_remove(odata->paspa);
					odata->paspa = 0;
					debug_printf("removing authconn input watcher\n");
					aim_conn_kill(odata->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
					debug_printf("No handler for rendezvous disconnect (%d).\n",
							source);
					aim_conn_kill(odata->sess, &conn);
				} else {
					debug_printf("holy crap! generic connection error! %d\n",
							conn->type);
					aim_conn_kill(odata->sess, &conn);
				}
			}
		}
	}
}

static void oscar_debug(struct aim_session_t *sess, int level, const char *format, va_list va) {
	char *s = g_strdup_vprintf(format, va);
	char buf[256];
	char *t;
	struct gaim_connection *gc = sess->aux_data;

	g_snprintf(buf, sizeof(buf), "%s %d: ", gc->username, level);
	t = g_strconcat(buf, s, NULL);
	debug_printf(t);
	if (t[strlen(t)-1] != '\n')
		debug_printf("\n");
	g_free(t);
	g_free(s);
}

static void oscar_login_connect(gpointer data, gint source, GdkInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	struct aim_session_t *sess;
	struct aim_conn_t *conn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	odata = gc->proto_data;
	sess = odata->sess;
	conn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);

	if (source < 0) {
		hide_login_progress(gc, _("Couldn't connect to host"));
		signoff(gc);
		return;
	}

	aim_conn_completeconnect(sess, conn);
	gc->inpa = gdk_input_add(conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, conn);
	debug_printf(_("Password sent, waiting for response\n"));
}

static void oscar_login(struct aim_user *user) {
	struct aim_session_t *sess;
	struct aim_conn_t *conn;
	char buf[256];
	struct gaim_connection *gc = new_gaim_conn(user);
	struct oscar_data *odata = gc->proto_data = g_new0(struct oscar_data, 1);
	odata->create_exchange = 0;

	debug_printf(_("Logging in %s\n"), user->username);

	sess = g_new0(struct aim_session_t, 1);

	aim_session_init(sess, AIM_SESS_FLAGS_NONBLOCKCONNECT, 0);
	aim_setdebuggingcb(sess, oscar_debug);

	/* we need an immediate queue because we don't use a while-loop to
	 * see if things need to be sent. */
	aim_tx_setenqueue(sess, AIM_TX_IMMEDIATE, NULL);
	odata->sess = sess;
	sess->aux_data = gc;

	conn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
	if (conn == NULL) {
		debug_printf(_("internal connection error\n"));
		hide_login_progress(gc, _("Unable to login to AIM"));
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), _("Signon: %s"), gc->username);
	set_login_progress(gc, 2, buf);

	aim_conn_addhandler(sess, conn, 0x0017, 0x0007, gaim_parse_login, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0003, gaim_parse_auth_resp, 0);

	conn->status |= AIM_CONN_STATUS_INPROGRESS;
	conn->fd = proxy_connect(user->proto_opt[USEROPT_AUTH][0] ?
					user->proto_opt[USEROPT_AUTH] : FAIM_LOGIN_SERVER,
				 user->proto_opt[USEROPT_AUTHPORT][0] ?
					atoi(user->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,
				 oscar_login_connect, gc);
	if (conn->fd < 0) {
		hide_login_progress(gc, _("Couldn't connect to host"));
		signoff(gc);
		return;
	}
	aim_request_login(sess, conn, gc->username);
}

static void oscar_close(struct gaim_connection *gc) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	if (gc->protocol != PROTO_OSCAR) return;
	
	while (odata->oscar_chats) {
		struct chat_connection *n = odata->oscar_chats->data;
		if (n->inpa > 0)
			gdk_input_remove(n->inpa);
		g_free(n->name);
		g_free(n->show);
		odata->oscar_chats = g_slist_remove(odata->oscar_chats, n);
		g_free(n);
	}
	while (odata->direct_ims) {
		struct direct_im *n = odata->direct_ims->data;
		if (n->watcher > 0)
			gdk_input_remove(n->watcher);
		odata->direct_ims = g_slist_remove(odata->direct_ims, n);
		g_free(n);
	}
#if USE_PIXBUF
	while (odata->hasicons) {
		struct icon_req *n = odata->hasicons->data;
		if (n->anim)
			gdk_pixbuf_animation_unref(n->anim);
		if (n->unanim)
			gdk_pixbuf_unref(n->unanim);
		if (n->timer)
			gtk_timeout_remove(n->timer);
		if (n->cnv && n->pix)
			gtk_container_remove(GTK_CONTAINER(n->cnv->bbox), n->pix);
		g_free(n->user);
		if (n->data)
			g_free(n->data);
		odata->hasicons = g_slist_remove(odata->hasicons, n);
		g_free(n);
	}
#endif
	if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	if (odata->cnpa > 0)
		gdk_input_remove(odata->cnpa);
	if (odata->paspa > 0)
		gdk_input_remove(odata->paspa);
	aim_session_kill(odata->sess);
	g_free(odata->sess);
	odata->sess = NULL;
	g_free(gc->proto_data);
	gc->proto_data = NULL;
	debug_printf(_("Signed off.\n"));
}

static void oscar_bos_connect(gpointer data, gint source, GdkInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	struct aim_session_t *sess;
	struct aim_conn_t *bosconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	odata = gc->proto_data;
	sess = odata->sess;
	bosconn = odata->conn;

	if (source < 0) {
		hide_login_progress(gc, _("Could Not Connect"));
		signoff(gc);
		return;
	}

	aim_conn_completeconnect(sess, bosconn);
	gc->inpa = gdk_input_add(bosconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, bosconn);
	set_login_progress(gc, 4, _("Connection established, cookie sent"));
}

int gaim_parse_auth_resp(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_conn_t *bosconn = NULL;
	char *sn = NULL, *bosip = NULL, *errurl = NULL, *email = NULL;
	unsigned char *cookie = NULL;
	int errorcode = 0, regstatus = 0;
	int latestbuild = 0, latestbetabuild = 0;
	char *latestrelease = NULL, *latestbeta = NULL;
	char *latestreleaseurl = NULL, *latestbetaurl = NULL;
	char *latestreleaseinfo = NULL, *latestbetainfo = NULL;
	int i; char *host; int port;
	struct aim_user *user;

	struct gaim_connection *gc = sess->aux_data;
	user = gc->user;
	port = user->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(user->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, command);
	sn = va_arg(ap, char *);
	errorcode = va_arg(ap, int);
	errurl = va_arg(ap, char *);
	regstatus = va_arg(ap, int);
	email = va_arg(ap, char *);
	bosip = va_arg(ap, char *);
	cookie = va_arg(ap, unsigned char *);

	latestrelease = va_arg(ap, char *);
	latestbuild = va_arg(ap, int);
	latestreleaseurl = va_arg(ap, char *);
	latestreleaseinfo = va_arg(ap, char *);

	latestbeta = va_arg(ap, char *);
	latestbetabuild = va_arg(ap, int);
	latestbetaurl = va_arg(ap, char *);
	latestbetainfo = va_arg(ap, char *);

	va_end(ap);

	debug_printf("inside auth_resp (Screen name: %s)\n", sn);

	if (errorcode || !bosip || !cookie) {
		switch (errorcode) {
		case 0x18:
			/* connecting too frequently */
			hide_login_progress(gc, _("You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer."));
			plugin_event(event_error, (void *)983, 0, 0, 0);
			break;
		case 0x05:
			/* Incorrect nick/password */
			hide_login_progress(gc, _("Incorrect nickname or password."));
			plugin_event(event_error, (void *)980, 0, 0, 0);
			break;
		case 0x1c:
			/* client too old */
			hide_login_progress(gc, _("The client version you are using is too old. Please upgrade at " WEBSITE));
			plugin_event(event_error, (void *)989, 0, 0, 0);
			break;
		default:
			hide_login_progress(gc, _("Authentication Failed"));
			break;
		}
		debug_printf("Login Error Code 0x%04x\n", errorcode);
		debug_printf("Error URL: %s\n", errurl);
		aim_conn_kill(sess, &command->conn);
		signoff(gc);
		return -1;
	}


	debug_printf("Reg status: %2d\n", regstatus);
	if (email) {
		debug_printf("Email: %s\n", email);
	} else {
		debug_printf("Email is NULL\n");
	}
	debug_printf("BOSIP: %s\n", bosip);
	if (latestbeta)
		debug_printf("Latest WinAIM beta version %s, build %d, at %s (%s)\n",
				latestbeta, latestbetabuild, latestbetaurl, latestbetainfo);
	if (latestrelease)
		debug_printf("Latest WinAIM released version %s, build %d, at %s (%s)\n",
				latestrelease, latestbuild, latestreleaseurl, latestreleaseinfo);
	debug_printf("Closing auth connection...\n");
	aim_conn_kill(sess, &command->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, NULL);
	if (bosconn == NULL) {
		hide_login_progress(gc, _("Internal Error"));
		signoff(gc);
		return -1;
	}

	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, gaim_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0007, gaim_rateresp, 0); /* rate info */
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, gaim_server_ready, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_STS, AIM_CB_STS_SETREPORTINTERVAL, gaim_reportinterval, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, gaim_parse_buddyrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, gaim_parse_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, gaim_parse_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, gaim_parse_incoming_im, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, gaim_parse_locerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, gaim_parse_ratechange, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_EVIL, gaim_parse_evilnotify, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, AIM_CB_LOK_ERROR, gaim_parse_searcherror, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, 0x0003, gaim_parse_searchreply, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, gaim_parse_msgerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, gaim_parse_user_info, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ACK, gaim_parse_msgack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, gaim_parse_motd, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0003, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x001f, gaim_memrequest, 0);

	((struct oscar_data *)gc->proto_data)->conn = bosconn;
	for (i = 0; i < (int)strlen(bosip); i++) {
		if (bosip[i] == ':') {
			port = atoi(&(bosip[i+1]));
			break;
		}
	}
	host = g_strndup(bosip, i);
	bosconn->status |= AIM_CONN_STATUS_INPROGRESS;
	bosconn->fd = proxy_connect(host, port, oscar_bos_connect, gc);
	g_free(host);
	if (bosconn->fd < 0) {
		hide_login_progress(gc, _("Could Not Connect"));
		signoff(gc);
		return -1;
	}
	aim_auth_sendcookie(sess, bosconn, cookie);
	gdk_input_remove(gc->inpa);
	return 1;
}

struct pieceofcrap {
	struct gaim_connection *gc;
	unsigned long offset;
	unsigned long len;
	char *modname;
	int fd;
	struct aim_conn_t *conn;
	unsigned int inpa;
};

static void damn_you(gpointer data, gint source, GdkInputCondition c)
{
	struct pieceofcrap *pos = data;
	struct oscar_data *od = pos->gc->proto_data;
	char in = '\0';
	int x = 0;
	unsigned char m[17];

	while (read(pos->fd, &in, 1) == 1) {
		if (in == '\n')
			x++;
		else if (in != '\r')
			x = 0;
		if (x == 2)
			break;
		in = '\0';
	}
	if (in != '\n') {
		do_error_dialog("Gaim was unable to get a valid hash for logging into AIM."
				" You may be disconnected shortly.", "Login Error");
		gdk_input_remove(pos->inpa);
		close(pos->fd);
		g_free(pos);
		return;
	}
	read(pos->fd, m, 16);
	m[16] = '\0';
	debug_printf("Sending hash: ");
	for (x = 0; x < 16; x++)
		debug_printf("%02x ", (unsigned char)m[x]);
	debug_printf("\n");
	gdk_input_remove(pos->inpa);
	close(pos->fd);
	aim_sendmemblock(od->sess, pos->conn, 0, 16, m, AIM_SENDMEMBLOCK_FLAG_ISHASH);
	g_free(pos);
}

static void straight_to_hell(gpointer data, gint source, GdkInputCondition cond) {
	struct pieceofcrap *pos = data;
	char buf[BUF_LONG];

	if (source < 0) {
		do_error_dialog("Gaim was unable to get a valid hash for logging into AIM."
				" You may be disconnected shortly.", "Login Error");
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		return;
	}

	g_snprintf(buf, sizeof(buf), "GET " AIMHASHDATA
			"?offset=%ld&len=%ld&modname=%s HTTP/1.0\n\n",
			pos->offset, pos->len, pos->modname ? pos->modname : "");
	write(pos->fd, buf, strlen(buf));
	if (pos->modname)
		g_free(pos->modname);
	pos->inpa = gdk_input_add(pos->fd, GDK_INPUT_READ, damn_you, pos);
	return;
}

/* size of icbmui.ocm, the largest module in AIM 3.5 */
#define AIM_MAX_FILE_SIZE 98304

int gaim_memrequest(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	va_list ap;
	struct pieceofcrap *pos;
	unsigned long offset, len;
	char *modname;
	int fd;

	va_start(ap, command);
	offset = va_arg(ap, unsigned long);
	len = va_arg(ap, unsigned long);
	modname = va_arg(ap, char *);
	va_end(ap);

	debug_printf("offset: %d, len: %d, file: %s\n", offset, len, modname ? modname : "aim.exe");
	if (len == 0) {
		debug_printf("len is 0, hashing NULL\n");
		aim_sendmemblock(sess, command->conn, offset, len, NULL,
				AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		return 1;
	}
	/* uncomment this when you're convinced it's right. remember, it's been wrong before.
	if (offset > AIM_MAX_FILE_SIZE || len > AIM_MAX_FILE_SIZE) {
		char *buf;
		int i = 8;
		if (modname)
			i += strlen(modname);
		buf = g_malloc(i);
		i = 0;
		if (modname) {
			memcpy(buf, modname, strlen(modname));
			i += strlen(modname);
		}
		buf[i++] = offset & 0xff;
		buf[i++] = (offset >> 8) & 0xff;
		buf[i++] = (offset >> 16) & 0xff;
		buf[i++] = (offset >> 24) & 0xff;
		buf[i++] = len & 0xff;
		buf[i++] = (len >> 8) & 0xff;
		buf[i++] = (len >> 16) & 0xff;
		buf[i++] = (len >> 24) & 0xff;
		debug_printf("len + offset is invalid, hashing request\n");
		aim_sendmemblock(sess, command->conn, offset, i, buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		g_free(buf);
		return 1;
	}
	*/

	pos = g_new0(struct pieceofcrap, 1);
	pos->gc = sess->aux_data;
	pos->conn = command->conn;

	pos->offset = offset;
	pos->len = len;
	pos->modname = modname ? g_strdup(modname) : NULL;

	fd = proxy_connect("gaim.sourceforge.net", 80, straight_to_hell, pos);
	if (fd < 0) {
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		do_error_dialog("Gaim was unable to get a valid hash for logging into AIM."
				" You may be disconnected shortly.", "Login Error");
	}
	pos->fd = fd;

	return 1;
}

void some_name(char *buf)
{
	int x[4];
	x[0] = htonl(0x45576172);
	x[1] = htonl(0x6d656e68);
	x[2] = htonl(0x6f76656e);
	x[3] = 0;
	g_snprintf(buf, 16, "%s", (char *)x);
}

int gaim_parse_login(struct aim_session_t *sess,
		     struct command_rx_struct *command, ...) {
#if 0
	struct client_info_s info = {"gaim", 4, 1, 2010, "us", "en", 0x0004, 0x0000, 0x04b};
#else
	struct client_info_s info = AIM_CLIENTINFO_KNOWNGOOD;
#endif
	char *key;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(sess, command->conn, gc->username, gc->password, &info, key);
	return 1;
}

int gaim_server_ready(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	static int id = 1;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct chat_connection *chatcon;
	switch (command->conn->type) {
	case AIM_CONN_TYPE_AUTH:
		aim_auth_setversions(sess, command->conn);
		aim_bos_reqrate(sess, command->conn);
		debug_printf("done with AUTH ServerReady\n");
		if (od->chpass) {
			debug_printf("changing password\n");
			aim_auth_changepasswd(sess, command->conn, od->newp, od->oldp);
			g_free(od->oldp);
			g_free(od->newp);
			od->chpass = FALSE;
		}
		if (od->conf) {
			debug_printf("confirming account\n");
			aim_auth_reqconfirm(sess, command->conn);
			od->conf = FALSE;
		}
		if (od->reqemail) {
			debug_printf("requesting email\n");
			aim_auth_getinfo(sess, command->conn, 0x0011);
			od->reqemail = FALSE;
		}
		break;
	case AIM_CONN_TYPE_BOS:
		aim_setversions(sess, command->conn);
		aim_bos_reqrate(sess, command->conn); /* request rate info */
		debug_printf("done with BOS ServerReady\n");
		break;
	case AIM_CONN_TYPE_CHATNAV:
		debug_printf("chatnav: got server ready\n");
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_chatnav_clientready(sess, command->conn);
		aim_chatnav_reqrights(sess, command->conn);
		break;
	case AIM_CONN_TYPE_CHAT:
		debug_printf("chat: got server ready\n");
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_chat_join, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_chat_leave, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_chat_info_update, 0);
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_chat_incoming_msg, 0);
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_chat_clientready(sess, command->conn);
		chatcon = find_oscar_chat_by_conn(gc, command->conn);
		chatcon->id = id;
		chatcon->cnv = serv_got_joined_chat(gc, id++, chatcon->show);
		break;
	case AIM_CONN_TYPE_RENDEZVOUS:
		break;
	default: /* huh? */
		debug_printf("server ready: got unexpected connection type %04x\n", command->conn->type);
		break;
	}
	return 1;
}

static void oscar_chatnav_connect(gpointer data, gint source, GdkInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	struct aim_session_t *sess;
	struct aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	odata = gc->proto_data;
	sess = odata->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_CHATNAV);

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to chatnav server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	odata->cnpa = gdk_input_add(tstconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
				oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_auth_connect(gpointer data, gint source, GdkInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	struct aim_session_t *sess;
	struct aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	odata = gc->proto_data;
	sess = odata->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to authorizer\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	odata->paspa = gdk_input_add(tstconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
				oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_chat_connect(gpointer data, gint source, GdkInputCondition cond)
{
	struct chat_connection *ccon = data;
	struct gaim_connection *gc = ccon->gc;
	struct oscar_data *odata;
	struct aim_session_t *sess;
	struct aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	odata = gc->proto_data;
	sess = odata->sess;
	tstconn = ccon->conn;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	aim_conn_completeconnect(sess, ccon->conn);
	ccon->inpa = gdk_input_add(tstconn->fd,
			GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, tstconn);
	odata->oscar_chats = g_slist_append(odata->oscar_chats, ccon);
	aim_chat_attachname(tstconn, ccon->name);
}

int gaim_handle_redirect(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	va_list ap;
	int serviceid;
	char *ip;
	unsigned char *cookie;
	struct gaim_connection *gc = sess->aux_data;
	struct aim_user *user = gc->user;
	struct aim_conn_t *tstconn;
	int i;
	char *host;
	int port;

	port = user->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(user->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, command);
	serviceid = va_arg(ap, int);
	ip        = va_arg(ap, char *);
	cookie    = va_arg(ap, unsigned char *);

	for (i = 0; i < (int)strlen(ip); i++) {
		if (ip[i] == ':') {
			port = atoi(&(ip[i+1]));
			break;
		}
	}
	host = g_strndup(ip, i);

	switch(serviceid) {
	case 0x7: /* Authorizer */
		debug_printf("Reconnecting with authorizor...\n");
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0007, gaim_rateresp, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0003, gaim_info_change, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0005, gaim_info_change, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0007, gaim_account_confirm, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		tstconn->fd = proxy_connect(host, port, oscar_auth_connect, gc);
		if (tstconn->fd < 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_auth_sendcookie(sess, tstconn, cookie);
		break;
	case 0xd: /* ChatNav */
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		tstconn->fd = proxy_connect(host, port, oscar_chatnav_connect, gc);
		if (tstconn->fd < 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_auth_sendcookie(sess, tstconn, cookie);
		break;
	case 0xe: /* Chat */
		{
		char *roomname = va_arg(ap, char *);
		int exchange = va_arg(ap, int);
		struct chat_connection *ccon;
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to connect to chat server\n");
			g_free(host);
			return 1;
		}

		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);
		ccon = g_new0(struct chat_connection, 1);
		ccon->conn = tstconn;
		ccon->gc = gc;
		ccon->fd = -1;
		ccon->name = g_strdup(roomname);
		ccon->exchange = exchange;
		ccon->show = extract_name(roomname);
		
		ccon->conn->status |= AIM_CONN_STATUS_INPROGRESS;
		ccon->conn->fd = proxy_connect(host, port, oscar_chat_connect, ccon);
		if (ccon->conn->fd < 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to chat server\n");
			g_free(host);
			g_free(ccon->show);
			g_free(ccon->name);
			g_free(ccon);
			return 1;
		}
		aim_auth_sendcookie(sess, tstconn, cookie);
		debug_printf("Connected to chat room %s exchange %d\n", roomname, exchange);
		}
		break;
	default: /* huh? */
		debug_printf("got redirect for unknown service 0x%04x\n", serviceid);
		break;
	}

	va_end(ap);

	g_free(host);
	return 1;
}

int gaim_parse_oncoming(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	time_t time_idle;
	int type = 0;
	struct gaim_connection *gc = sess->aux_data;

	va_list ap;
	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	if (info->flags & AIM_FLAG_UNCONFIRMED)
		type |= UC_UNCONFIRMED;
	else if (info->flags & AIM_FLAG_ADMINISTRATOR)
		type |= UC_ADMIN;
	else if (info->flags & AIM_FLAG_AOL)
		type |= UC_AOL;
	else if (info->flags & AIM_FLAG_FREE)
		type |= UC_NORMAL;
	if (info->flags & AIM_FLAG_AWAY)
		type |= UC_UNAVAILABLE;

	if (info->idletime) {
		time(&time_idle);
		time_idle -= info->idletime*60;
	} else
		time_idle = 0;

	serv_got_update(gc, info->sn, 1, info->warnlevel/10, info->onlinesince,
			time_idle, type, info->capabilities);

	return 1;
}

int gaim_parse_offgoing(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	serv_got_update(gc, info->sn, 0, 0, 0, 0, 0, 0);

	return 1;
}

static void cancel_direct_im(gpointer w, struct ask_direct *d) {
	debug_printf("Freeing DirectIM prompts.\n");

	g_free(d->sn);
	g_free(d);
}

static void delete_direct_im(gpointer w, struct direct_im *d) {
	struct oscar_data *od = (struct oscar_data *)d->gc->proto_data;

	od->direct_ims = g_slist_remove(od->direct_ims, d);
	gdk_input_remove(d->watcher);
	aim_conn_kill(od->sess, &d->conn);
	g_free(d);
}

static void oscar_directim_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct direct_im *dim = data;
	struct gaim_connection *gc = dim->gc;
	struct oscar_data *od = gc->proto_data;
	char buf[256];

	if (!g_slist_find(connections, gc)) {
		g_free(dim);
		return;
	}

	if (source < 0) {
		g_free(dim);
		return;
	}

	aim_conn_completeconnect(od->sess, dim->conn);
	if (!(dim->cnv = find_conversation(dim->name))) dim->cnv = new_conversation(dim->name);
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), dim->name);
	write_to_conv(dim->cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	od->direct_ims = g_slist_append(od->direct_ims, dim);

	gtk_signal_connect(GTK_OBJECT(dim->cnv->window), "destroy",
			   GTK_SIGNAL_FUNC(delete_direct_im), dim);

	dim->watcher = gdk_input_add(dim->conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					oscar_callback, dim->conn);
}

static int accept_direct_im(gpointer w, struct ask_direct *d) {
	struct gaim_connection *gc = d->gc;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim;
	char *host; int port = FAIM_LOGIN_PORT;
	int i;

	debug_printf("Accepted DirectIM.\n");

	dim = find_direct_im(od, d->sn);
	if (dim) {
		cancel_direct_im(w, d); /* 40 */
		return TRUE;
	}
	dim = g_new0(struct direct_im, 1);
	dim->gc = d->gc;
	g_snprintf(dim->name, sizeof dim->name, "%s", d->sn);

	if ((dim->conn = aim_newconn(od->sess, AIM_CONN_TYPE_RENDEZVOUS, NULL)) == NULL) {
		g_free(dim);
		cancel_direct_im(w, d);
		return TRUE;
	}
	dim->conn->subtype = AIM_CONN_SUBTYPE_OFT_DIRECTIM;
	dim->conn->priv = d->priv;
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_directim_incoming, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMDISCONNECT,
				gaim_directim_disconnect, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_directim_typing, 0);

	for (i = 0; i < (int)strlen(d->priv->ip); i++) {
		if (d->priv->ip[i] == ':') {
			port = atoi(&(d->priv->ip[i+1]));
			break;
		}
	}
	host = g_strndup(d->priv->ip, i);
	dim->conn->status |= AIM_CONN_STATUS_INPROGRESS;
	dim->conn->fd = proxy_connect(host, port, oscar_directim_callback, dim);
	g_free(host);
	if (dim->conn->fd < 0) {
		aim_conn_kill(od->sess, &dim->conn);
		g_free(dim);
		cancel_direct_im(w, d);
		return TRUE;
	}

	cancel_direct_im(w, d);

	return TRUE;
}

/*
static void cancel_getfile(gpointer w, struct ask_getfile *g) {
	g_free(g->ip);
	g_free(g->cookie);
	g_free(g->sn);
	g_free(g);
}

static void cancel_getfile_file(GtkObject *obj, struct ask_getfile *g) {
	GtkWidget *w = gtk_object_get_user_data(obj);
	gtk_widget_destroy(w);
	cancel_getfile(w, g);
}

static void cancel_getfile_cancel(GtkObject *obj, struct ask_getfile *g) {
	GtkWidget *w = gtk_object_get_user_data(obj);
	gtk_widget_destroy(w);
}

static void interrupt_getfile(GtkObject *obj, struct getfile_transfer *gt) {
	struct gaim_connection *gc = gt->gc;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	gtk_widget_destroy(gt->window);
	gdk_input_remove(gt->gip);
	if (gt->gop > 0)
		gdk_input_remove(gt->gop);
	aim_conn_kill(od->sess, &gt->conn);
	od->getfiles = g_slist_remove(od->getfiles, gt);
	g_free(gt->receiver);
	g_free(gt->filename);
	fclose(gt->listing);
	g_free(gt);
}

static int gaim_getfile_filereq(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct getfile_transfer *gt;
	char buf[2048];
	GtkWidget *label;
	GtkWidget *button;

	va_list ap;
	struct aim_conn_t *oftconn;
	struct aim_fileheader_t *fh;
	char *cookie;

	va_start(ap, command);
	oftconn = va_arg(ap, struct aim_conn_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	cookie = va_arg(ap, char *);
	va_end(ap);

	gt = find_getfile_transfer(od, oftconn);

	if (gt->window)
		return 1;

	gt->window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(gt->window), _("Gaim - File Transfer"));
	gtk_widget_realize(gt->window);
	aol_icon(gt->window->window);

	g_snprintf(buf, sizeof buf, _("Sending %s to %s"), fh->name, gt->receiver);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gt->window)->vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gt->meter = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gt->window)->action_area), gt->meter, FALSE, FALSE, 5);
	gtk_widget_show(gt->meter);

	gt->label = gtk_label_new("0 %");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gt->window)->action_area), gt->label, FALSE, FALSE, 5);
	gtk_widget_show(gt->label);

	button = picture_button(gt->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gt->window)->action_area), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(interrupt_getfile), gt);

	gtk_widget_show(gt->window);

	return 1;
}

static void getfile_send_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct getfile_transfer *gt = (struct getfile_transfer *)data;
	int result;

	result = aim_getfile_send_chunk(gt->conn, gt->file, gt->fh, -1, 1024);
	gt->pos += result;
	if (result == 0) {
		gdk_input_remove(gt->gop); gt->gop = 0;
	} else if (result == -1) {
		do_error_dialog(_("Error in transfer"), "Gaim");
		gdk_input_remove(gt->gop); gt->gop = 0;
		interrupt_getfile(NULL, gt);
	}
}

static int gaim_getfile_filesend(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct getfile_transfer *gt;

	va_list ap;
	struct aim_conn_t *oftconn;
	struct aim_fileheader_t *fh;
	char *cookie;

	va_start(ap, command);
	oftconn = va_arg(ap, struct aim_conn_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	cookie = va_arg(ap, char *);
	va_end(ap);

	gt = find_getfile_transfer(od, oftconn);

	if (gt->gop > 0) {
		debug_printf("already have output watcher?\n");
		return 1;
	}

	if ((gt->file = fopen(gt->filename, "r")) == NULL) {
		interrupt_getfile(NULL, gt);
		return 1;
	}
	gt->pos = 0;
	gt->fh = g_memdup(fh, sizeof(struct aim_fileheader_t));
	fseek(gt->file, 0, SEEK_SET);

	gt->gop = gdk_input_add(gt->conn->fd, GDK_INPUT_WRITE, getfile_send_callback, gt);

	return 1;
}

static int gaim_getfile_complete(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct getfile_transfer *gt;

	va_list ap;
	struct aim_conn_t *conn;
	struct aim_fileheader_t *fh;

	va_start(ap, command);
	conn = va_arg(ap, struct aim_conn_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	gt = find_getfile_transfer(od, conn);

	gtk_widget_destroy(gt->window);
	gt->window = NULL;
	do_error_dialog(_("Transfer complete."), "Gaim");

	return 1;
}

static int gaim_getfile_disconnect(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct getfile_transfer *gt;

	va_list ap;
	struct aim_conn_t *conn;
	char *sn;

	va_start(ap, command);
	conn = va_arg(ap, struct aim_conn_t *);
	sn = va_arg(ap, char *);
	va_end(ap);

	gt = find_getfile_transfer(od, conn);
	od->getfiles = g_slist_remove(od->getfiles, gt);
	gdk_input_remove(gt->gip);
	if (gt->gop > 0)
		gdk_input_remove(gt->gop);
	g_free(gt->receiver);
	g_free(gt->filename);
	aim_conn_kill(sess, &conn);
	fclose(gt->listing);
	g_free(gt);

	debug_printf("getfile disconnect\n");

	return 1;
}

static void oscar_getfile_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct getfile_transfer *gf = data;
	struct gaim_connection *gc = gf->gc;
	struct oscar_data *od = gc->proto_data;

	gdk_input_remove(gf->gip);
	gf->gip = gdk_input_add(source, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, oscar_callback, gf->conn);

	aim_conn_addhandler(od->sess, gf->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEFILEREQ, gaim_getfile_filereq, 0);
	aim_conn_addhandler(od->sess, gf->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEFILESEND, gaim_getfile_filesend, 0);
	aim_conn_addhandler(od->sess, gf->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILECOMPLETE, gaim_getfile_complete, 0);
	aim_conn_addhandler(od->sess, gf->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_GETFILEDISCONNECT, gaim_getfile_disconnect, 0);
}

static void do_getfile(GtkObject *obj, struct ask_getfile *g) {
	GtkWidget *w = gtk_object_get_user_data(obj);
	char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(w));
	struct gaim_connection *gc = g->gc;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct getfile_transfer *gf;
	struct stat st;
	struct tm *ft;
	char tmppath[256];
	FILE *file;
	static int current = 0;
	struct aim_conn_t *newconn;

	if (file_is_dir(filename, w))
		return;

	if (stat(filename, &st) != 0) {
		gtk_widget_destroy(w);
		do_error_dialog(_("Error examining file"), _("GetFile Error"));
		cancel_getfile(w, g);
		return;
	}

	g_snprintf(tmppath, sizeof tmppath, "/%s/gaim%d%d", g_get_tmp_dir(), getpid(), current++);
	if ((file = fopen(tmppath, "w+")) == NULL) {
		gtk_widget_destroy(w);
		do_error_dialog(_("Could not open temporary file, aborting"), _("GetFile Error"));
		cancel_getfile(w, g);
		return;
	}

	gf = g_new0(struct getfile_transfer, 1);
	gf->gc = gc;
	gf->filename = g_strdup(filename);
	gf->listing = file;
	gf->receiver = g_strdup(g->sn);
	gf->size = st.st_size;

	ft = localtime(&st.st_ctime);
	fprintf(file, "%2d/%2d/%4d %2d:%2d %8ld ",
			ft->tm_mon + 1, ft->tm_mday, ft->tm_year + 1900,
			ft->tm_hour + 1, ft->tm_min + 1, (long)st.st_size);
	fprintf(file, "%s\r\n", g_basename(filename));
	rewind(file);

	aim_oft_registerlisting(od->sess, file, "");
	if ((newconn = aim_accepttransfer(od->sess, od->conn, g->sn, g->cookie, g->ip, file, AIM_CAPS_GETFILE)) == NULL) {
		od->sess->flags ^= AIM_SESS_FLAGS_NONBLOCKCONNECT;
		do_error_dialog(_("Error connecting for transfer"), _("GetFile Error"));
		g_free(gf->filename);
		fclose(file);
		g_free(gf);
		gtk_widget_destroy(w);
		return;
	}

	gtk_widget_destroy(w);

	od->getfiles = g_slist_append(od->getfiles, gf);
	gf->conn = newconn;
	gf->gip = gdk_input_add(newconn->fd, GDK_INPUT_WRITE, oscar_getfile_callback, gf);
}

static int accept_getfile(gpointer w, struct ask_getfile *g) {
	GtkWidget *window;
	window = gtk_file_selection_new(_("Gaim - Send File..."));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(window));
	gtk_object_set_user_data(GTK_OBJECT(window), window);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(cancel_getfile_file), g);
	gtk_object_set_user_data(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), window);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(do_getfile), g);
	gtk_object_set_user_data(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button), window);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_getfile_cancel), g);
	gtk_widget_show(window);

	return TRUE;
}
*/

#if USE_PIXBUF
static gboolean redraw_anim(gpointer data)
{
	int delay;
	struct icon_req *ir = data;
	GList *frames;
	GdkPixbufFrame *frame;
	GdkPixbuf *buf;
	GdkPixmap *pm; GdkBitmap *bm;
	GdkPixmap *src;
	GdkGC *gc;

	if (!ir->cnv || !g_list_find(conversations, ir->cnv)) {
		debug_printf("I think this is a bug.\n");
		return FALSE;
	}

	frames = gdk_pixbuf_animation_get_frames(ir->anim);
	frame = g_list_nth_data(frames, ir->curframe);
	buf = gdk_pixbuf_frame_get_pixbuf(frame);
	switch (gdk_pixbuf_frame_get_action(frame)) {
		case GDK_PIXBUF_FRAME_RETAIN:
			gdk_pixbuf_render_pixmap_and_mask(buf, &src, NULL, 0);
			gtk_pixmap_get(GTK_PIXMAP(ir->pix), &pm, &bm);
			gc = gdk_gc_new(pm);
			gdk_draw_pixmap(pm, gc, src, 0, 0,
					gdk_pixbuf_frame_get_x_offset(frame),
					gdk_pixbuf_frame_get_y_offset(frame),
					-1, -1);
			gdk_pixmap_unref(src);
			gtk_widget_queue_draw(ir->pix);
			gdk_gc_unref(gc);
			break;
		case GDK_PIXBUF_FRAME_DISPOSE:
			gdk_pixbuf_render_pixmap_and_mask(buf, &pm, &bm, 0);
			gtk_pixmap_set(GTK_PIXMAP(ir->pix), pm, bm);
			gdk_pixmap_unref(pm);
			if (bm)
				gdk_bitmap_unref(bm);
			break;
		case GDK_PIXBUF_FRAME_REVERT:
			frame = frames->data;
			buf = gdk_pixbuf_frame_get_pixbuf(frame);
			gdk_pixbuf_render_pixmap_and_mask(buf, &pm, &bm, 0);
			gtk_pixmap_set(GTK_PIXMAP(ir->pix), pm, bm);
			gdk_pixmap_unref(pm);
			if (bm)
				gdk_bitmap_unref(bm);
			break;
	}
	ir->curframe = (ir->curframe + 1) % g_list_length(frames);
	delay = MAX(gdk_pixbuf_frame_get_delay_time(frame), 13);
	ir->timer = gtk_timeout_add(delay * 10, redraw_anim, ir);
	return FALSE;
}
#endif

int gaim_parse_incoming_im(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	int channel;
	struct aim_userinfo_s *userinfo;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	channel = va_arg(ap, int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);

	/* channel 1: standard message */
	if (channel == 1) {
		char *tmp = g_malloc(BUF_LONG);
		struct aim_incomingim_ch1_args *args;

		args = va_arg(ap, struct aim_incomingim_ch1_args *);
		va_end(ap);

#if USE_PIXBUF
		if (args->icbmflags & AIM_IMFLAGS_HASICON) {
			struct oscar_data *od = gc->proto_data;
			struct icon_req *ir;
			GSList *h = od->hasicons;
			char *who = normalize(userinfo->sn);
			debug_printf("%s has an icon\n", userinfo->sn);
			while (h) {
				ir = h->data;
				if (!strcmp(ir->user, who))
					break;
				h = h->next;
			}
			if (!h) {
				ir = g_new0(struct icon_req, 1);
				ir->user = g_strdup(who);
				od->hasicons = g_slist_append(od->hasicons, ir);
			}
			if (args->iconstamp > ir->timestamp)
				ir->request = TRUE;
			ir->timestamp = args->iconstamp;
		}
#endif

		/*
		 * Quickly convert it to eight bit format, replacing 
		 * non-ASCII UNICODE characters with their equivelent 
		 * HTML entity.
		 */
		if (args->icbmflags & AIM_IMFLAGS_UNICODE) {
			int i;
			
			for (i = 0, tmp[0] = '\0'; i < args->msglen; i += 2) {
				unsigned short uni;
				
				uni = ((args->msg[i] & 0xff) << 8) | (args->msg[i+1] & 0xff);

				if ((uni < 128) || ((uni >= 160) && (uni <= 255))) { /* ISO 8859-1 */
					
					g_snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp), "%c", uni);
					
				} else { /* something else, do UNICODE entity */
					g_snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp), "&#%04x;", uni);
				}
			}
		} else
			g_snprintf(tmp, BUF_LONG, "%s", args->msg);

		serv_got_im(gc, userinfo->sn, tmp, args->icbmflags & AIM_IMFLAGS_AWAY, time(NULL));
		g_free(tmp);
	} else if (channel == 2) {
		struct aim_incomingim_ch2_args *args;
		args = va_arg(ap, struct aim_incomingim_ch2_args *);
		va_end(ap);
		if (args->reqclass & AIM_CAPS_CHAT) {
			char *name = extract_name(args->info.chat.roominfo.name);
			serv_got_chat_invite(gc,
					     name ? name : args->info.chat.roominfo.name,
					     args->info.chat.roominfo.exchange,
					     userinfo->sn,
					     args->info.chat.msg);
			if (name)
				g_free(name);
		} else if (args->reqclass & AIM_CAPS_SENDFILE) {
		} else if (args->reqclass & AIM_CAPS_GETFILE) {
			/*
			char *ip, *cookie;
			struct ask_getfile *g = g_new0(struct ask_getfile, 1);
			char buf[256];

			userinfo = va_arg(ap, struct aim_userinfo_s *);
			ip = va_arg(ap, char *);
			cookie = va_arg(ap, char *);
			va_end(ap);

			debug_printf("%s received getfile request from %s (%s), cookie = %s\n",
					gc->username, userinfo->sn, ip, cookie);

			g->gc = gc;
			g->sn = g_strdup(userinfo->sn);
			g->cookie = g_strdup(cookie);
			g->ip = g_strdup(ip);
			g_snprintf(buf, sizeof buf, "%s has just asked to get a file from %s.",
					userinfo->sn, gc->username);
			do_ask_dialog(buf, g, accept_getfile, cancel_getfile);
			*/
		} else if (args->reqclass & AIM_CAPS_VOICE) {
		} else if (args->reqclass & AIM_CAPS_BUDDYICON) {
#if USE_PIXBUF
			struct oscar_data *od = gc->proto_data;
			GSList *h = od->hasicons;
			struct icon_req *ir = NULL;
			char *who;
			struct conversation *c;

			GdkPixbufLoader *load;
			GList *frames;
			GdkPixbuf *buf;
			GdkPixmap *pm;
			GdkBitmap *bm;

			who = normalize(userinfo->sn);

			while (h) {
				ir = h->data;
				if (!strcmp(who, ir->user))
					break;
				h = h->next;

			}

			if (!h || ((c = find_conversation(userinfo->sn)) == NULL) || (c->gc != gc)) {
				debug_printf("got buddy icon for %s but didn't want it\n", userinfo->sn);
				return 1;
			}

			if (ir->pix && ir->cnv)
				gtk_container_remove(GTK_CONTAINER(ir->cnv->bbox), ir->pix);
			ir->pix = NULL;
			ir->cnv = NULL;
			if (ir->data)
				g_free(ir->data);
			if (ir->anim)
				gdk_pixbuf_animation_unref(ir->anim);
			ir->anim = NULL;
			if (ir->unanim)
				gdk_pixbuf_unref(ir->unanim);
			ir->unanim = NULL;
			if (ir->timer)
				gtk_timeout_remove(ir->timer);
			ir->timer = 0;

			ir->length = args->info.icon.length;

			if (!ir->length)
				return 1;

			ir->data = g_memdup(args->info.icon.icon, args->info.icon.length);

			load = gdk_pixbuf_loader_new();
			gdk_pixbuf_loader_write(load, ir->data, ir->length);
			ir->anim = gdk_pixbuf_loader_get_animation(load);

			if (ir->anim) {
				frames = gdk_pixbuf_animation_get_frames(ir->anim);
				buf = gdk_pixbuf_frame_get_pixbuf(frames->data);
				gdk_pixbuf_render_pixmap_and_mask(buf, &pm, &bm, 0);

				if (gdk_pixbuf_animation_get_num_frames(ir->anim) > 1) {
					int delay =
						MAX(gdk_pixbuf_frame_get_delay_time(frames->data), 13);
					ir->curframe = 1;
					ir->timer = gtk_timeout_add(delay * 10, redraw_anim, ir);
				}
			} else {
				ir->unanim = gdk_pixbuf_loader_get_pixbuf(load);
				if (!ir->unanim) {
					gdk_pixbuf_loader_close(load);
					return 1;
				}
				gdk_pixbuf_render_pixmap_and_mask(ir->unanim, &pm, &bm, 0);
			}

			ir->cnv = c;
			ir->pix = gtk_pixmap_new(pm, bm);
			gtk_box_pack_start(GTK_BOX(c->bbox), ir->pix, FALSE, FALSE, 5);
			if (ir->anim && (gdk_pixbuf_animation_get_num_frames(ir->anim) > 1))
				gtk_widget_set_usize(ir->pix, gdk_pixbuf_animation_get_width(ir->anim),
							gdk_pixbuf_animation_get_height(ir->anim));
			gtk_widget_show(ir->pix);
			gdk_pixmap_unref(pm);
			if (bm)
				gdk_bitmap_unref(bm);

			gdk_pixbuf_loader_close(load);

#endif
		} else if (args->reqclass & AIM_CAPS_IMIMAGE) {
			struct ask_direct *d = g_new0(struct ask_direct, 1);
			char buf[256];

			debug_printf("%s received direct im request from %s (%s)\n",
					gc->username, userinfo->sn, args->info.directim->ip);

			d->gc = gc;
			d->sn = g_strdup(userinfo->sn);
			d->priv = args->info.directim;
			g_snprintf(buf, sizeof buf, "%s has just asked to directly connect to %s.",
					userinfo->sn, gc->username);
			do_ask_dialog(buf, d, accept_direct_im, cancel_direct_im);
		} else {
			debug_printf("Unknown reqclass %d\n", args->reqclass);
		}
	}

	return 1;
}

int gaim_parse_misses(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short chan, nummissed, reason;
	struct aim_userinfo_s *userinfo;
	char buf[1024];

	va_start(ap, command);
	chan = (unsigned short)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	nummissed = (unsigned short)va_arg(ap, unsigned int);
	reason = (unsigned short)va_arg(ap, unsigned int);
	va_end(ap);

	switch(reason) {
		case 1:
			/* message too large */
			sprintf(buf, _("You missed a message from %s because it was too large."), userinfo->sn);
			do_error_dialog(buf, _("Gaim - Error"));
			plugin_event(event_error, (void *)961, 0, 0, 0);
			break;
		default:
			sprintf(buf, _("You missed a message from %s for unknown reasons."), userinfo->sn);
			do_error_dialog(buf, _("Gaim - Error"));
			plugin_event(event_error, (void *)970, 0, 0, 0);
			break;
	}

	return 1;
}

int gaim_parse_genericerr(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short reason;

	va_start(ap, command);
	reason = va_arg(ap, int);
	va_end(ap);

	debug_printf("snac threw error (reason 0x%04x: %s\n", reason,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown");

	return 1;
}

int gaim_parse_msgerr(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	char *destn;
	unsigned short reason;
	char buf[1024];

	va_start(ap, command);
	reason = (unsigned short)va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	sprintf(buf, _("Your message to %s did not get sent: %s"), destn,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : _("Reason unknown"));
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

int gaim_parse_locerr(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	char *destn;
	unsigned short reason;
	char buf[1024];

	va_start(ap, command);
	reason = (unsigned short)va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	sprintf(buf, _("User information for %s unavailable: %s"), destn,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : _("Reason unknown"));
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

static char *images(int flags) {
	static char buf[1024];
	g_snprintf(buf, sizeof(buf), "%s%s%s%s",
			(flags & AIM_FLAG_UNCONFIRMED) ? "<IMG SRC=\"dt_icon.gif\">" : "",
			(flags & AIM_FLAG_AOL) ? "<IMG SRC=\"aol_icon.gif\">" : "",
			(flags & AIM_FLAG_ADMINISTRATOR) ? "<IMG SRC=\"admin_icon.gif\">" : "",
			(flags & AIM_FLAG_FREE) ? "<IMG SRC=\"free_icon.gif\">" : "");
	return buf;
}

int gaim_parse_user_info(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	char *prof_enc = NULL, *prof = NULL;
	unsigned short infotype;
	char buf[BUF_LONG];
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *asc;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	prof_enc = va_arg(ap, char *);
	prof = va_arg(ap, char *);
	infotype = (unsigned short)va_arg(ap, unsigned int);
	va_end(ap);

	if (info->membersince)
		asc = g_strdup_printf("Member Since : <B>%s</B><BR>\n",
				asctime(localtime(&info->membersince)));
	else
		asc = g_strdup("");

	g_snprintf(buf, sizeof buf,
			_("Username : <B>%s</B>  %s <BR>\n"
			"%s"
			"Warning Level : <B>%d %%</B><BR>\n"
			"Online Since : <B>%s</B><BR>\n"
			"Idle Minutes : <B>%d</B>\n<BR>\n<HR><BR>\n"
			"%s"
			"<br><BODY BGCOLOR=WHITE><hr><I>Legend:</I><br><br>"
			"<IMG SRC=\"free_icon.gif\"> : Normal AIM User<br>"
			"<IMG SRC=\"aol_icon.gif\"> : AOL User <br>"
			"<IMG SRC=\"dt_icon.gif\"> : Trial AIM User <br>"
			"<IMG SRC=\"admin_icon.gif\"> : Administrator"),
			info->sn, images(info->flags),
			asc,
			info->warnlevel/10,
			asctime(localtime(&info->onlinesince)),
			info->idletime,
			(prof && strlen(prof)) ?
				(infotype == AIM_GETINFO_GENERALINFO ?
					prof :
					away_subs(prof, gc->username))
				:
				(infotype == AIM_GETINFO_GENERALINFO ?
					_("<i>No Information Provided</i>") :
					_("<i>User has no away message</i>")));

	g_show_info_text(away_subs(buf, gc->username));

	g_free(asc);

	return 1;
}

int gaim_parse_motd(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	char *msg;
	unsigned short id;
	va_list ap;
	char buildbuf[150];

	va_start(ap, command);
	id  = (unsigned short)va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	aim_getbuildstring(buildbuf, sizeof(buildbuf));

	debug_printf("MOTD: %s (%d)\n", msg, id);
	debug_printf("Gaim %s / Libfaim %s\n", VERSION, buildbuf);
	if (id != 4)
		do_error_dialog(_("Your connection may be lost."),
				_("AOL error"));

	return 1;
}

int gaim_chatnav_info(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short type;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;

	va_start(ap, command);
	type = (unsigned short)va_arg(ap, unsigned int);

	switch(type) {
		case 0x0002: {
			int maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i = 0;

			maxrooms = (unsigned char)va_arg(ap, unsigned int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);
			va_end(ap);

			debug_printf("chat info: Chat Rights:\n");
			debug_printf("chat info: \tMax Concurrent Rooms: %d\n", maxrooms);
			debug_printf("chat info: \tExchange List: (%d total)\n", exchangecount);
			while (i < exchangecount)
				debug_printf("chat info: \t\t%d\n", exchanges[i++].number);
			if (odata->create_exchange) {
				debug_printf("creating room %s\n", odata->create_name);
				aim_chatnav_createroom(sess, command->conn, odata->create_name,
						odata->create_exchange);
				odata->create_exchange = 0;
				g_free(odata->create_name);
				odata->create_name = NULL;
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			unsigned short instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
			unsigned char createperms;
			unsigned long createtime;

			fqcn = va_arg(ap, char *);
			instance = (unsigned short)va_arg(ap, unsigned int);
			exchange = (unsigned short)va_arg(ap, unsigned int);
			flags = (unsigned short)va_arg(ap, unsigned int);
			createtime = va_arg(ap, unsigned long);
			maxmsglen = (unsigned short)va_arg(ap, unsigned int);
			maxoccupancy = (unsigned short)va_arg(ap, unsigned int);
			createperms = (unsigned char)va_arg(ap, int);
			unknown = (unsigned short)va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);
			va_end(ap);

			debug_printf("created room: %s %d %d %d %lu %d %d %d %d %s %s\n",
					fqcn,
					exchange, instance, flags,
					createtime,
					maxmsglen, maxoccupancy, createperms, unknown,
					name, ck);
			aim_chat_join(odata->sess, odata->conn, exchange, ck);
			}
			break;
		default:
			va_end(ap);
			debug_printf("chatnav info: unknown type (%04x)\n", type);
			break;
	}
	return 1;
}

int gaim_chat_join(struct aim_session_t *sess,
		   struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, command->conn);
	if (!c)
		return 1;

	while (i < count)
		add_chat_buddy(c->cnv, info[i++].sn);

	return 1;
}

int gaim_chat_leave(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, command->conn);
	if (!c)
		return 1;

	while (i < count)
		remove_chat_buddy(c->cnv, info[i++].sn);

	return 1;
}

int gaim_chat_info_update(struct aim_session_t *sess,
			  struct command_rx_struct *command, ...) {
	debug_printf("inside chat_info_update\n");
	return 1;
}

int gaim_chat_incoming_msg(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_userinfo_s *info;
	char *msg;
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, command->conn);
	char *tmp;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	msg  = va_arg(ap, char *);

	tmp = g_malloc(BUF_LONG);
	g_snprintf(tmp, BUF_LONG, "%s", msg);
	serv_got_chat_in(gc, ccon->id, info->sn, 0, tmp, time((time_t)NULL));
	g_free(tmp);

	return 1;
}

 /*
  * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
  */
int gaim_parse_msgack(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short type;
	char *sn = NULL;

	va_start(ap, command);
	type = (unsigned short)va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Sent message to %s.\n", sn);

	return 1;
}

int gaim_parse_ratechange(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	static char *codes[5] = {"invalid",
				 "change",
				 "warning",
				 "limit",
				 "limit cleared"};
	va_list ap;
	int code;
	unsigned long rateclass, windowsize, clear, alert, limit, disconnect;
	unsigned long currentavg, maxavg;

	va_start(ap, command); 
	code = va_arg(ap, int);
	rateclass= va_arg(ap, int);
	windowsize = va_arg(ap, unsigned long);
	clear = va_arg(ap, unsigned long);
	alert = va_arg(ap, unsigned long);
	limit = va_arg(ap, unsigned long);
	disconnect = va_arg(ap, unsigned long);
	currentavg = va_arg(ap, unsigned long);
	maxavg = va_arg(ap, unsigned long);
	va_end(ap);

	debug_printf("rate %s (paramid 0x%04lx): curavg = %ld, maxavg = %ld, alert at %ld, "
		     "clear warning at %ld, limit at %ld, disconnect at %ld (window size = %ld)\n",
		     (code < 5) ? codes[code] : codes[0],
		     rateclass,
		     currentavg, maxavg,
		     alert, clear,
		     limit, disconnect,
		     windowsize);

	if (code == AIM_RATE_CODE_CHANGE) {
		if (currentavg >= clear)
			aim_conn_setlatency(command->conn, 0);
	} else if (code == AIM_RATE_CODE_WARNING) {
		aim_conn_setlatency(command->conn, windowsize/4);
	} else if (code == AIM_RATE_CODE_LIMIT) {
		aim_conn_setlatency(command->conn, windowsize/2);
	} else if (code == AIM_RATE_CODE_CLEARLIMIT) {
		aim_conn_setlatency(command->conn, 0);
	}

	return 1;
}

int gaim_parse_evilnotify(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	int newevil;
	struct aim_userinfo_s *userinfo;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	newevil = va_arg(ap, int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	serv_got_eviled(gc, (userinfo && userinfo->sn[0]) ? userinfo->sn : NULL, newevil / 10);

	return 1;
}

int gaim_rateresp(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	struct gaim_connection *gc = sess->aux_data;
	switch (command->conn->type) {
	case AIM_CONN_TYPE_BOS:
		aim_bos_ackrateresp(sess, command->conn);
		aim_bos_reqpersonalinfo(sess, command->conn);
		aim_bos_reqlocaterights(sess, command->conn);
		aim_bos_setprofile(sess, command->conn, gc->user->user_info, NULL, gaim_caps);
		aim_bos_reqbuddyrights(sess, command->conn);

		account_online(gc);
		serv_finish_login(gc);

		if (bud_list_cache_exists(gc))
			do_import(NULL, gc);

		debug_printf("buddy list loaded\n");

		aim_addicbmparam(sess, command->conn);
		aim_bos_reqicbmparaminfo(sess, command->conn);

		aim_bos_reqrights(sess, command->conn);
		aim_bos_setgroupperm(sess, command->conn, AIM_FLAG_ALLUSERS);
		aim_bos_setprivacyflags(sess, command->conn, AIM_PRIVFLAGS_ALLOWIDLE |
							     AIM_PRIVFLAGS_ALLOWMEMBERSINCE);

		break;
	case AIM_CONN_TYPE_AUTH:
		aim_bos_ackrateresp(sess, command->conn);
		aim_auth_clientready(sess, command->conn);
		debug_printf("connected to auth (admin)\n");
		break;
	default:
		debug_printf("got rate response for unhandled connection type %04x\n",
				command->conn->type);
		break;
	}

	return 1;
}

int gaim_reportinterval(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	if (command->data) {
		debug_printf("minimum report interval: %d (seconds?)\n", aimutil_get16(command->data+10));
	} else
		debug_printf("NULL minimum report interval!\n");
	return 1;
}

int gaim_parse_buddyrights(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short maxbuddies, maxwatchers;

	va_start(ap, command);
	maxbuddies = (unsigned short)va_arg(ap, unsigned int);
	maxwatchers = (unsigned short)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("buddy list rights: Max buddies = %d / Max watchers = %d\n", maxbuddies, maxwatchers);

	return 1;
}

int gaim_bosrights(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	unsigned short maxpermits, maxdenies;
	va_list ap;

	va_start(ap, command);
	maxpermits = (unsigned short)va_arg(ap, unsigned int);
	maxdenies = (unsigned short)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("BOS rights: Max permit = %d / Max deny = %d\n", maxpermits, maxdenies);

	aim_bos_clientready(sess, command->conn);

	aim_bos_reqservice(sess, command->conn, AIM_CONN_TYPE_CHATNAV);

	return 1;
}

int gaim_parse_searchreply(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	char *address, *SNs;
	int i, num;
	char *buf;
	int at = 0, len;

	va_start(ap, command);
	address = va_arg(ap, char *);
	num = va_arg(ap, int);
	SNs = va_arg(ap, char *);
	va_end(ap);

	len = num * (MAXSNLEN + 1) + 1024;
	buf = g_malloc(len);
	at += g_snprintf(buf + at, len - at, "<B>%s has the following screen names:</B><BR>", address);
	for (i = 0; i < num; i++)
		at += g_snprintf(buf + at, len - at, "%s<BR>", &SNs[i * (MAXSNLEN + 1)]);
	g_show_info_text(buf);
	g_free(buf);

	return 1;
}

int gaim_parse_searcherror(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	char *address;
	char buf[BUF_LONG];

	va_start(ap, command);
	address = va_arg(ap, char *);
	va_end(ap);

	g_snprintf(buf, sizeof(buf), "No results found for email address %s", address);
	do_error_dialog(buf, _("Error"));

	return 1;
}

int gaim_account_confirm(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	int status;
	va_list ap;
	char msg[256];
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	status = va_arg(ap, int); /* status code of confirmation request */
	va_end(ap);

	debug_printf("account confirmation returned status 0x%04x (%s)\n", status,
			status ? "email sent" : "unknown");
	if (status) {
		g_snprintf(msg, sizeof(msg), "You should receive an email asking to confirm %s.",
				gc->username);
		do_error_dialog(msg, "Confirm");
	}

	return 1;
}

int gaim_info_change(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	unsigned short change = 0;
	int perms, type, length, str;
	char *val;
	va_list ap;
	char buf[BUF_LONG];
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	perms = va_arg(ap, int);
	type = va_arg(ap, int);
	length = va_arg(ap, int);
	val = va_arg(ap, char *);
	str = va_arg(ap, int);
	va_end(ap);

	if (aimutil_get16(command->data+2) == 0x0005)
		change = 1;

	debug_printf("info%s: perms = %d, type = %x, length = %d, val = %s\n",
			change ? " change" : "", perms, type, length, str ? val : "(not string)");

	if ((type == 0x0011) && str) {
		g_snprintf(buf, sizeof(buf), "The email address for %s is %s", gc->username, val);
		do_error_dialog(buf, "Email");
	}

	return 1;
}

static void oscar_keepalive(struct gaim_connection *gc) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	aim_flap_nop(odata->sess, odata->conn);
}

static char *oscar_name() {
	return "Oscar";
}

static void oscar_send_im(struct gaim_connection *gc, char *name, char *message, int away) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim = find_direct_im(odata, name);
	if (dim) {
		aim_send_im_direct(odata->sess, dim->conn, message);
	} else {
		if (away)
			aim_send_im(odata->sess, odata->conn, name, AIM_IMFLAGS_AWAY, message);
		else {
			int flags = AIM_IMFLAGS_ACK;
#if USE_PIXBUF
			GSList *h = odata->hasicons;
			struct icon_req *ir;
			char *who = normalize(name);
			while (h) {
				ir = h->data;
				if (ir->request && !strcmp(who, ir->user))
					break;
				h = h->next;
			}
			if (h) {
				ir->request = FALSE;
				flags |= AIM_IMFLAGS_BUDDYREQ;
				debug_printf("sending buddy icon request with message\n");
			}
#endif
			aim_send_im(odata->sess, odata->conn, name, flags, message);
		}
	}
}

static void oscar_get_info(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_getinfo(odata->sess, odata->conn, name, AIM_GETINFO_GENERALINFO);
}

static void oscar_get_away_msg(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_getinfo(odata->sess, odata->conn, name, AIM_GETINFO_AWAYMESSAGE);
}

static void oscar_set_dir(struct gaim_connection *g, char *first, char *middle, char *last,
			  char *maiden, char *city, char *state, char *country, int web) {
	/* FIXME : some of these things are wrong, but i'm lazy */
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_setdirectoryinfo(odata->sess, odata->conn, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
}


static void oscar_set_idle(struct gaim_connection *g, int time) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_bos_setidle(odata->sess, odata->conn, time);
}

static void oscar_set_info(struct gaim_connection *g, char *info) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	char inforeal[1025], away[1025];
	g_snprintf(inforeal, sizeof(inforeal), "%s", info);
	if (g->away)
		g_snprintf(away, sizeof(away), "%s", g->away);
	if (strlen(info) > 1024)
		do_error_dialog("Maximum info length (1024) exceeded, truncating", "Info Too Long");
	aim_bos_setprofile(odata->sess, odata->conn, inforeal, g->away ? NULL : "", gaim_caps);
}

static void oscar_set_away(struct gaim_connection *g, char *state, char *message) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	char info[1025], away[1025];
	g_snprintf(info, sizeof(info), "%s", g->user->user_info);
	if (message)
		g_snprintf(away, sizeof(away), "%s", message);
	aim_bos_setprofile(odata->sess, odata->conn, NULL, message ? away : "", gaim_caps);
	if (g->away)
		g_free (g->away);
	g->away = NULL;
	if (message) {
		if (strlen(message) > 1024)
			do_error_dialog("Maximum away length (1024) exceeded, truncating",
					"Info Too Long");
		g->away = g_strdup (message);
	}
}

static void oscar_warn(struct gaim_connection *g, char *name, int anon) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_send_warning(odata->sess, odata->conn, name, anon);
}

static void oscar_dir_search(struct gaim_connection *g, char *first, char *middle, char *last,
			     char *maiden, char *city, char *state, char *country, char *email) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	if (strlen(email))
		aim_usersearch_address(odata->sess, odata->conn, email);
}

static void oscar_add_buddy(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_add_buddy(odata->sess, odata->conn, name);
}

static void oscar_add_buddies(struct gaim_connection *g, GList *buddies) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	char buf[MSG_LEN];
	int n = 0;
	while (buddies) {
		if (n > MSG_LEN - 18) {
			aim_bos_setbuddylist(odata->sess, odata->conn, buf);
			n = 0;
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, "%s&", (char *)buddies->data);
		buddies = buddies->next;
	}
	aim_bos_setbuddylist(odata->sess, odata->conn, buf);
}

static void oscar_remove_buddy(struct gaim_connection *g, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_remove_buddy(odata->sess, odata->conn, name);
}

static void oscar_join_chat(struct gaim_connection *g, int exchange, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	struct aim_conn_t *cur = NULL;
	if (!name) {
		if (!join_chat_entry || !join_chat_spin)
			return;
		name = gtk_entry_get_text(GTK_ENTRY(join_chat_entry));
		exchange = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(join_chat_spin));
		if (!name || !strlen(name))
			return;
	}
	debug_printf("Attempting to join chat room %s.\n", name);
	if ((cur = aim_getconn_type(odata->sess, AIM_CONN_TYPE_CHATNAV))) {
		debug_printf("chatnav exists, creating room\n");
		aim_chatnav_createroom(odata->sess, cur, name, exchange);
	} else {
		/* this gets tricky */
		debug_printf("chatnav does not exist, opening chatnav\n");
		odata->create_exchange = exchange;
		odata->create_name = g_strdup(name);
		aim_bos_reqservice(odata->sess, odata->conn, AIM_CONN_TYPE_CHATNAV);
	}
}

static void des_jc()
{
	join_chat_entry = NULL;
	join_chat_spin = NULL;
}

static void oscar_draw_join_chat(struct gaim_connection *gc, GtkWidget *fbox) {
	GtkWidget *label;
	GtkWidget *rowbox;
	GtkObject *adjust;

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);
	gtk_widget_show(rowbox);

	label = gtk_label_new(_("Join what group:"));
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(label), "destroy", GTK_SIGNAL_FUNC(des_jc), NULL);
	gtk_widget_show(label);

	join_chat_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rowbox), join_chat_entry, TRUE, TRUE, 0);
	gtk_widget_grab_focus(join_chat_entry);
	gtk_signal_connect(GTK_OBJECT(join_chat_entry), "activate", GTK_SIGNAL_FUNC(do_join_chat), NULL);
	gtk_widget_show(join_chat_entry);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);
	gtk_widget_show(rowbox);

	label = gtk_label_new(_("Exchange:"));
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	
	adjust = gtk_adjustment_new(4, 4, 20, 1, 10, 10);
	join_chat_spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(join_chat_spin, 50, -1);
	gtk_box_pack_start(GTK_BOX(rowbox), join_chat_spin, FALSE, FALSE, 0);
	gtk_widget_show(join_chat_spin);
}

static void oscar_chat_invite(struct gaim_connection *g, int id, char *message, char *name) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	struct chat_connection *ccon = find_oscar_chat(g, id);
	
	if (!ccon)
		return;
	
	aim_chat_invite(odata->sess, odata->conn, name, message ? message : "",
			ccon->exchange, ccon->name, 0x0);
}

static void oscar_chat_leave(struct gaim_connection *g, int id) {
	struct oscar_data *odata = g ? (struct oscar_data *)g->proto_data : NULL;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	struct chat_connection *c = NULL;
	int count = 0;

	while (bcs) {
		count++;
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}

	if (!b)
		return;

	debug_printf("Attempting to leave room %s (currently in %d rooms)\n", b->name, count);
	
	c = find_oscar_chat(g, b->id);
	if (c != NULL) {
		if (odata)
			odata->oscar_chats = g_slist_remove(odata->oscar_chats, c);
		if (c->inpa > 0)
			gdk_input_remove(c->inpa);
		if (g && odata->sess)
			aim_conn_kill(odata->sess, &c->conn);
		g_free(c->name);
		g_free(c->show);
		g_free(c);
	}
	/* we do this because with Oscar it doesn't tell us we left */
	serv_got_chat_left(g, b->id);
}

static void oscar_chat_send(struct gaim_connection *g, int id, char *message) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	struct chat_connection *c = NULL;
	char *buf;
	int i, j;

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return;

	bcs = odata->oscar_chats;
	while (bcs) {
		c = (struct chat_connection *)bcs->data;
		if (b == c->cnv)
			break;
		bcs = bcs->next;
		c = NULL;
	}
	if (!c)
		return;

	buf = g_malloc(strlen(message) * 4 + 1);
	for (i = 0, j = 0; i < strlen(message); i++) {
		if (message[i] == '\n') {
			buf[j++] = '<';
			buf[j++] = 'B';
			buf[j++] = 'R';
			buf[j++] = '>';
		} else {
			buf[j++] = message[i];
		}
	}
	buf[j] = '\0';
	aim_chat_send_im(odata->sess, c->conn, 0, buf, strlen(buf));
	g_free(buf);
}

static char **oscar_list_icon(int uc) {
	if (uc & UC_UNAVAILABLE)
		return (char **)away_icon_xpm;
	if (uc & UC_AOL)
		return (char **)aol_icon_xpm;
	if (uc & UC_NORMAL)
		return (char **)free_icon_xpm;
	if (uc & UC_ADMIN)
		return (char **)admin_icon_xpm;
	if (uc & UC_UNCONFIRMED)
		return (char **)dt_icon_xpm;
	return NULL;
}

static void oscar_info(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_info(gc, who);
}

static void oscar_away_msg(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_away_msg(gc, who);
}

static int gaim_directim_initiate(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct aim_directim_priv *priv;
	struct aim_conn_t *newconn;
	struct direct_im *dim;
	char buf[256];

	va_start(ap, command);
	newconn = va_arg(ap, struct aim_conn_t *);
	va_end(ap);

	priv = (struct aim_directim_priv *)newconn->priv;

	debug_printf("DirectIM: initiate success to %s\n", priv->sn);
	dim = find_direct_im(od, priv->sn);

	dim->cnv = find_conversation(priv->sn);
	if (!dim->cnv) dim->cnv = new_conversation(priv->sn);
	gtk_signal_connect(GTK_OBJECT(dim->cnv->window), "destroy",
			   GTK_SIGNAL_FUNC(delete_direct_im), dim);
	gdk_input_remove(dim->watcher);
	dim->conn = newconn;
	dim->watcher = gdk_input_add(dim->conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					oscar_callback, dim->conn);
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), priv->sn);
	write_to_conv(dim->cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_directim_incoming, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMDISCONNECT,
				gaim_directim_disconnect, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_directim_typing, 0);

	return 1;
}

static int gaim_directim_incoming(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	char *msg = NULL;
	struct aim_conn_t *conn;
	struct aim_directim_priv *priv;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, command);
	conn = va_arg(ap, struct aim_conn_t *);
	msg = va_arg(ap, char *);
	va_end(ap);

	if (!(priv = conn->priv)) {
		return -1;
	}

	debug_printf("Got DirectIM message from %s\n", priv->sn);

	serv_got_im(gc, priv->sn, msg, 0, time((time_t)NULL));

	return 1;
}

static int gaim_directim_disconnect(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_conn_t *conn;
	char *sn;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim;
	char buf[256];

	va_start(ap, command);
	conn = va_arg(ap, struct aim_conn_t *);
	sn = va_arg(ap, char *);
	va_end(ap);

	debug_printf("%s disconnected Direct IM.\n", sn);

	dim = find_direct_im(od, sn);
	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	gdk_input_remove(dim->watcher);
	gtk_signal_disconnect_by_data(GTK_OBJECT(dim->cnv->window), dim);

	g_snprintf(buf, sizeof buf, _("Direct IM with %s closed"), sn);
	if (dim->cnv)
		write_to_conv(dim->cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	aim_conn_kill(sess, &conn);

	return 1;
}

static int gaim_directim_typing(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_conn_t *conn;
	struct aim_directim_priv *priv;

	va_start(ap, command);
	conn = va_arg(ap, struct aim_conn_t *);
	va_end(ap);

	if (!(priv = conn->priv)) {
		return -1;
	}

	/* I had to leave this. It's just too funny. It reminds me of my sister. */
	debug_printf("ohmigod! %s has started typing (DirectIM). He's going to send you a message! *squeal*\n", priv->sn);

	return 1;
}

struct ask_do_dir_im {
	char *who;
	struct gaim_connection *gc;
};

static void oscar_cancel_direct_im(GtkObject *obj, struct ask_do_dir_im *data) {
	g_free(data);
}

static void oscar_direct_im(GtkObject *obj, struct ask_do_dir_im *data) {
	struct gaim_connection *gc = data->gc;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim;

	dim = find_direct_im(od, data->who);
	if (dim) {
		do_error_dialog("Direct IM request already pending.", "Unable");
		return;
	}
	dim = g_new0(struct direct_im, 1);
	dim->gc = gc;
	g_snprintf(dim->name, sizeof dim->name, "%s", data->who);

	dim->conn = aim_directim_initiate(od->sess, od->conn, NULL, data->who);
	if (dim->conn != NULL) {
		od->direct_ims = g_slist_append(od->direct_ims, dim);
		dim->watcher = gdk_input_add(dim->conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
						oscar_callback, dim->conn);
		aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINITIATE,
					gaim_directim_initiate, 0);
	} else {
		do_error_dialog(_("Unable to open Direct IM"), _("Error"));
		g_free(dim);
	}
}

static void oscar_ask_direct_im(GtkObject *m, gchar *who) {
	char buf[BUF_LONG];
	struct ask_do_dir_im *data = g_new0(struct ask_do_dir_im, 1);
	data->who = who;
	data->gc = gtk_object_get_user_data(m);
	g_snprintf(buf, sizeof(buf),  _("You have selected to open a Direct IM connection with %s."
					" Doing this will let them see your IP address, and may be"
					" a security risk. Do you wish to continue?"), who);
	do_ask_dialog(buf, data, oscar_direct_im, oscar_cancel_direct_im);
}

static void oscar_buddy_menu(GtkWidget *menu, struct gaim_connection *gc, char *who) {
	GtkWidget *button;
	char *n = g_strdup(normalize(gc->username));

	button = gtk_menu_item_new_with_label(_("Get Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(oscar_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Get Away Msg"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(oscar_away_msg), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	if (strcmp(n, normalize(who))) {
		button = gtk_menu_item_new_with_label(_("Direct IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate",
				   GTK_SIGNAL_FUNC(oscar_ask_direct_im), who);
		gtk_object_set_user_data(GTK_OBJECT(button), gc);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);
	}
	g_free(n);
}


/* weeee */
static void oscar_print_option(GtkEntry *entry, struct aim_user *user)
{
	int entrynum;

	entrynum = (int)gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_AUTH) {
		g_snprintf(user->proto_opt[USEROPT_AUTH],
			   sizeof(user->proto_opt[USEROPT_AUTH]), "%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_AUTHPORT) {
		g_snprintf(user->proto_opt[USEROPT_AUTHPORT],
			   sizeof(user->proto_opt[USEROPT_AUTHPORT]), "%s", gtk_entry_get_text(entry));
	}
}

static void oscar_user_opts(GtkWidget *book, struct aim_user *user)
{
	/* so here, we create the new notebook page */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, gtk_label_new("Oscar Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Auth Host:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_AUTH);
	gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(oscar_print_option), user);
	if (user->proto_opt[USEROPT_AUTH][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTH]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTH]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "login.oscar.aol.com");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Auth Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_AUTHPORT);
	gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(oscar_print_option), user);
	if (user->proto_opt[USEROPT_AUTHPORT][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTHPORT]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTHPORT]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "5190");

	gtk_widget_show(entry);
}

static void oscar_set_permit_deny(struct gaim_connection *gc) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	GSList *list;
	char buf[MAXMSGLEN];
	int at;

	switch(gc->permdeny) {
	case 1:
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, gc->username);
		break;
	case 2:
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, gc->username);
		break;
	case 3:
		list = gc->permit;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, buf);
		break;
	case 4:
		list = gc->deny;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, buf);
		break;
	default:
		break;
	}
}

static void oscar_add_permit(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 3) return;
	oscar_set_permit_deny(gc);
}

static void oscar_add_deny(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 4) return;
	oscar_set_permit_deny(gc);
}

static void oscar_rem_permit(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 3) return;
	oscar_set_permit_deny(gc);
}

static void oscar_rem_deny(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 4) return;
	oscar_set_permit_deny(gc);
}

static void oscar_draw_new_user(GtkWidget *box)
{
	GtkWidget *label;

	label = gtk_label_new(_("Unfortunately, currently Oscar only allows new user registration by "
				"going to http://aim.aol.com/aimnew/Aim/register.adp?promo=106723&pageset=Aim&client=no"
				". Clicking the Register button will open the URL for you."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
}

static void oscar_do_new_user()
{
	open_url(NULL, "http://aim.aol.com/aimnew/Aim/register.adp?promo=106723&pageset=Aim&client=no");
}

static GList *oscar_away_states()
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void oscar_do_action(struct gaim_connection *gc, char *act)
{
	struct oscar_data *od = gc->proto_data;
	struct aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (!strcmp(act, "Set User Info")) {
		show_set_info(gc);
	} else if (!strcmp(act, "Change Password")) {
		show_change_passwd(gc);
	} else if (!strcmp(act, "Confirm Account")) {
		if (!conn) {
			od->conf = TRUE;
			aim_bos_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		} else
			aim_auth_reqconfirm(od->sess, conn);
	} else if (!strcmp(act, "Change Email")) {
	} else if (!strcmp(act, "Display Current Registered Address")) {
		if (!conn) {
			od->reqemail = TRUE;
			aim_bos_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		} else
			aim_auth_getinfo(od->sess, conn, 0x11);
	} else if (!strcmp(act, "Search for Buddy by Email")) {
		show_find_email(gc);
	}
}

static GList *oscar_actions()
{
	GList *m = NULL;

	m = g_list_append(m, "Set User Info");
	m = g_list_append(m, NULL);
	m = g_list_append(m, "Change Password");
	m = g_list_append(m, "Confirm Account");
	/*
	m = g_list_append(m, "Change Email");
	*/
	m = g_list_append(m, "Display Current Registered Address");
	m = g_list_append(m, NULL);
	m = g_list_append(m, "Search for Buddy by Email");

	return m;
}

static void oscar_change_passwd(struct gaim_connection *gc, char *old, char *new)
{
	struct oscar_data *od = gc->proto_data;
	if (!aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH)) {
		od->chpass = TRUE;
		od->oldp = g_strdup(old);
		od->newp = g_strdup(new);
		aim_bos_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	} else {
		aim_auth_changepasswd(od->sess, aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH),
				new, old);
	}
}

static void oscar_insert_convo(struct gaim_connection *gc, struct conversation *c)
{
#if USE_PIXBUF
	struct oscar_data *od = gc->proto_data;
	GSList *h = od->hasicons;
	struct icon_req *ir = NULL;
	char *who = normalize(c->name);

	GdkPixbufLoader *load;
	GList *frames;
	GdkPixbuf *buf;
	GdkPixmap *pm;
	GdkBitmap *bm;

	while (h) {
		ir = h->data;
		if (!strcmp(who, ir->user))
			break;
		h = h->next;
	}
	if (!h || !ir->data)
		return;

	ir->cnv = c;

	load = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(load, ir->data, ir->length);
	ir->anim = gdk_pixbuf_loader_get_animation(load);

	if (ir->anim) {
		frames = gdk_pixbuf_animation_get_frames(ir->anim);
		buf = gdk_pixbuf_frame_get_pixbuf(frames->data);
		gdk_pixbuf_render_pixmap_and_mask(buf, &pm, &bm, 0);

		if (gdk_pixbuf_animation_get_num_frames(ir->anim) > 1) {
			int delay = MAX(gdk_pixbuf_frame_get_delay_time(frames->data), 13);
			ir->curframe = 1;
			ir->timer = gtk_timeout_add(delay * 10, redraw_anim, ir);
		}
	} else {
		ir->unanim = gdk_pixbuf_loader_get_pixbuf(load);
		if (!ir->unanim) {
			gdk_pixbuf_loader_close(load);
			return;
		}
		gdk_pixbuf_render_pixmap_and_mask(ir->unanim, &pm, &bm, 0);
	}

	ir->pix = gtk_pixmap_new(pm, bm);
	gtk_box_pack_start(GTK_BOX(c->bbox), ir->pix, FALSE, FALSE, 5);
	if (ir->anim && (gdk_pixbuf_animation_get_num_frames(ir->anim) > 1))
		gtk_widget_set_usize(ir->pix, gdk_pixbuf_animation_get_width(ir->anim),
					gdk_pixbuf_animation_get_height(ir->anim));
	gtk_widget_show(ir->pix);
	gdk_pixmap_unref(pm);
	if (bm)
		gdk_bitmap_unref(bm);

	gdk_pixbuf_loader_close(load);
#endif
}

static void oscar_remove_convo(struct gaim_connection *gc, struct conversation *c)
{
#if USE_PIXBUF
	struct oscar_data *od = gc->proto_data;
	GSList *h = od->hasicons;
	struct icon_req *ir = NULL;
	char *who = normalize(c->name);
	
	while (h) {
		ir = h->data;
		if (!strcmp(who, ir->user))
			break;
		h = h->next;
	}
	if (!h || !ir->data)
		return;
	
	if (ir->cnv && ir->pix) {
		gtk_container_remove(GTK_CONTAINER(ir->cnv->bbox), ir->pix);
		ir->pix = NULL;
		ir->cnv = NULL;
	}
	
	if (ir->anim) {
		gdk_pixbuf_animation_unref(ir->anim);
		ir->anim = NULL;
	} else if (ir->unanim) {
		gdk_pixbuf_unref(ir->unanim);
		ir->unanim = NULL;
	}
	
	ir->curframe = 0;
	
	if (ir->timer)
		gtk_timeout_remove(ir->timer);
	ir->timer = 0;
#endif
}

void oscar_init(struct prpl *ret) {
	ret->protocol = PROTO_OSCAR;
	ret->options = OPT_PROTO_HTML | OPT_PROTO_CORRECT_TIME;
	ret->name = oscar_name;
	ret->list_icon = oscar_list_icon;
	ret->away_states = oscar_away_states;
	ret->actions = oscar_actions;
	ret->do_action = oscar_do_action;
	ret->buddy_menu = oscar_buddy_menu;
	ret->user_opts = oscar_user_opts;
	ret->draw_new_user = oscar_draw_new_user;
	ret->do_new_user = oscar_do_new_user;
	ret->insert_convo = oscar_insert_convo;
	ret->remove_convo = oscar_remove_convo;
	ret->login = oscar_login;
	ret->close = oscar_close;
	ret->send_im = oscar_send_im;
	ret->set_info = oscar_set_info;
	ret->get_info = oscar_get_info;
	ret->set_away = oscar_set_away;
	ret->get_away_msg = oscar_get_away_msg;
	ret->set_dir = oscar_set_dir;
	ret->get_dir = NULL; /* Oscar really doesn't have this */
	ret->dir_search = oscar_dir_search;
	ret->set_idle = oscar_set_idle;
	ret->change_passwd = oscar_change_passwd;
	ret->add_buddy = oscar_add_buddy;
	ret->add_buddies = oscar_add_buddies;
	ret->remove_buddy = oscar_remove_buddy;
	ret->add_permit = oscar_add_permit;
	ret->add_deny = oscar_add_deny;
	ret->rem_permit = oscar_rem_permit;
	ret->rem_deny = oscar_rem_deny;
	ret->set_permit_deny = oscar_set_permit_deny;
	ret->warn = oscar_warn;
	ret->accept_chat = NULL; /* oscar doesn't have accept, it just joins */
	ret->join_chat = oscar_join_chat;
	ret->draw_join_chat = oscar_draw_join_chat;
	ret->chat_invite = oscar_chat_invite;
	ret->chat_leave = oscar_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = oscar_chat_send;
	ret->keepalive = oscar_keepalive;
}
