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
#include <config.h>
#endif


#include <netdb.h>
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
		       AIM_CAPS_BUDDYICON |
		       AIM_CAPS_IMIMAGE;
static fu8_t gaim_features[] = {0x01, 0x01, 0x01, 0x02};

struct oscar_data {
	aim_session_t *sess;
	aim_conn_t *conn;

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
	GSList *hasicons;

        gboolean killme;
};

struct chat_connection {
	char *name;
	char *show; /* AOL did something funny to us */
	fu16_t exchange; /* XXX should have instance here too */
	int fd; /* this is redundant since we have the conn below */
	aim_conn_t *conn;
	int inpa;
	int id;
	struct gaim_connection *gc; /* i hate this. */
	struct conversation *cnv; /* bah. */
	int maxlen;
	int maxvis;
};

struct direct_im {
	struct gaim_connection *gc;
	char name[80];
	int watcher;
	aim_conn_t *conn;
};

struct ask_direct {
	struct gaim_connection *gc;
	char *sn;
	char ip[64];
	fu8_t cookie[8];
};

struct icon_req {
	char *user;
	time_t timestamp;
	unsigned long length;
	unsigned long checksum;
	gboolean request;
};

static struct direct_im *find_direct_im(struct oscar_data *od, const char *who) {
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
							aim_conn_t *conn) {
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

static int gaim_parse_auth_resp  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_login      (aim_session_t *, aim_frame_t *, ...);
static int gaim_handle_redirect  (aim_session_t *, aim_frame_t *, ...);
static int gaim_info_change      (aim_session_t *, aim_frame_t *, ...);
static int gaim_account_confirm  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_oncoming   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_offgoing   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_incoming_im(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_misses     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_user_info  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_motd       (aim_session_t *, aim_frame_t *, ...);
static int gaim_chatnav_info     (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_join        (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_leave       (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_info_update (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_incoming_msg(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgack     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_ratechange (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_evilnotify (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searcherror(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searchreply(aim_session_t *, aim_frame_t *, ...);
static int gaim_bosrights        (aim_session_t *, aim_frame_t *, ...);
static int rateresp_bos     (aim_session_t *, aim_frame_t *, ...);
static int rateresp_auth    (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_buddyrights(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_locerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_icbm_param_info  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_genericerr (aim_session_t *, aim_frame_t *, ...);
static int gaim_memrequest       (aim_session_t *,  aim_frame_t*, ...);
static int server_ready_bos      (aim_session_t *,  aim_frame_t*, ...);

static int gaim_directim_initiate  (aim_session_t *, aim_frame_t *, ...);
static int gaim_directim_incoming  (aim_session_t *, aim_frame_t *, ...);
static int gaim_directim_typing    (aim_session_t *, aim_frame_t *, ...);

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

static void gaim_directim_disconnect(aim_session_t *sess, aim_conn_t *conn) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct conversation *cnv;
	struct direct_im *dim;
	char *sn;
	char buf[256];

	sn = g_strdup(aim_directim_getsn(conn));

	debug_printf("%s disconnected Direct IM.\n", sn);

	dim = find_direct_im(od, sn);
	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	gaim_input_remove(dim->watcher);

	g_snprintf(buf, sizeof buf, _("Direct IM with %s closed"), sn);
	if ((cnv = find_conversation(sn)))
		write_to_conv(cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	g_free(dim); /* I guess? I don't see it anywhere else... -- mid */
	g_free(sn);

	return;
}

static void oscar_callback(gpointer data, gint source,
				GaimInputCondition condition) {
	aim_conn_t *conn = (aim_conn_t *)data;
	aim_session_t *sess = aim_conn_getsess(conn);
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

	if (condition & GAIM_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) {
			debug_printf("got information on rendezvous\n");
			if (aim_handlerendconnect(odata->sess, conn) < 0) {
				debug_printf(_("connection error (rend)\n"));
				aim_conn_kill(odata->sess, &conn);
			}
		} else {
			if (aim_get_command(odata->sess, conn) >= 0) {
				aim_rxdispatch(odata->sess);
                                if (odata->killme)
                                        signoff(gc);
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
						gaim_input_remove(c->inpa);
					c->inpa = 0;
					c->fd = -1;
					aim_conn_kill(odata->sess, &conn);
					sprintf(buf, _("You have been disconnected from chat room %s."), c->name);
					do_error_dialog(buf, _("Chat Error!"));
				} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
					if (odata->cnpa > 0)
						gaim_input_remove(odata->cnpa);
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
						gaim_input_remove(odata->paspa);
					odata->paspa = 0;
					debug_printf("removing authconn input watcher\n");
					aim_conn_kill(odata->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
					if (conn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM)
						gaim_directim_disconnect(odata->sess, conn);
					else {
						debug_printf("No handler for rendezvous disconnect (%d).\n",
								source);
					}
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

static void oscar_debug(aim_session_t *sess, int level, const char *format, va_list va) {
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

static void oscar_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	aim_session_t *sess;
	aim_conn_t *conn;

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
	gc->inpa = gaim_input_add(conn->fd, GAIM_INPUT_READ,
			oscar_callback, conn);
	debug_printf(_("Password sent, waiting for response\n"));
}

static void oscar_login(struct aim_user *user) {
	aim_session_t *sess;
	aim_conn_t *conn;
	char buf[256];
	struct gaim_connection *gc = new_gaim_conn(user);
	struct oscar_data *odata = gc->proto_data = g_new0(struct oscar_data, 1);
	odata->create_exchange = 0;

	debug_printf(_("Logging in %s\n"), user->username);

	sess = g_new0(aim_session_t, 1);

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
			gaim_input_remove(n->inpa);
		g_free(n->name);
		g_free(n->show);
		odata->oscar_chats = g_slist_remove(odata->oscar_chats, n);
		g_free(n);
	}
	while (odata->direct_ims) {
		struct direct_im *n = odata->direct_ims->data;
		if (n->watcher > 0)
			gaim_input_remove(n->watcher);
		odata->direct_ims = g_slist_remove(odata->direct_ims, n);
		g_free(n);
	}
	while (odata->hasicons) {
		struct icon_req *n = odata->hasicons->data;
		g_free(n->user);
		odata->hasicons = g_slist_remove(odata->hasicons, n);
		g_free(n);
	}
	if (gc->inpa > 0)
		gaim_input_remove(gc->inpa);
	if (odata->cnpa > 0)
		gaim_input_remove(odata->cnpa);
	if (odata->paspa > 0)
		gaim_input_remove(odata->paspa);
	aim_session_kill(odata->sess);
	g_free(odata->sess);
	odata->sess = NULL;
	g_free(gc->proto_data);
	gc->proto_data = NULL;
	debug_printf(_("Signed off.\n"));
}

static void oscar_bos_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	aim_session_t *sess;
	aim_conn_t *bosconn;

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
	gc->inpa = gaim_input_add(bosconn->fd, GAIM_INPUT_READ,
			oscar_callback, bosconn);
	set_login_progress(gc, 4, _("Connection established, cookie sent"));
}

static int gaim_parse_auth_resp(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	aim_conn_t *bosconn = NULL;
	char *sn = NULL, *bosip = NULL, *errurl = NULL, *email = NULL;
	fu8_t *cookie = NULL;
	int errorcode = 0, regstatus = 0;
	int latestbuild = 0, latestbetabuild = 0;
	char *latestrelease = NULL, *latestbeta = NULL;
	char *latestreleaseurl = NULL, *latestbetaurl = NULL;
	char *latestreleaseinfo = NULL, *latestbetainfo = NULL;
	int i; char *host; int port;
	struct aim_user *user;

	struct gaim_connection *gc = sess->aux_data;
        struct oscar_data *od = gc->proto_data;
	user = gc->user;
	port = user->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(user->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, fr);
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
		case 0x05:
			/* Incorrect nick/password */
			hide_login_progress(gc, _("Incorrect nickname or password."));
			plugin_event(event_error, (void *)980, 0, 0, 0);
			break;
		case 0x11:
			/* Suspended account */
			hide_login_progress(gc, _("Your account is currently suspended."));
			break;
		case 0x18:
			/* connecting too frequently */
			hide_login_progress(gc, _("You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer."));
			plugin_event(event_error, (void *)983, 0, 0, 0);
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
		od->killme = TRUE;
		return 1;
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
	aim_conn_kill(sess, &fr->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, NULL);
	if (bosconn == NULL) {
		hide_login_progress(gc, _("Internal Error"));
		od->killme = TRUE;
		return 0;
	}

	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, gaim_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0007, rateresp_bos, 0); /* rate info */
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, server_ready_bos, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
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
	aim_conn_addhandler(sess, bosconn, 0x0004, 0x0005, gaim_icbm_param_info, 0);
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
		od->killme = TRUE;
		return 0;
	}
	aim_auth_sendcookie(sess, bosconn, cookie);
	gaim_input_remove(gc->inpa);
	return 1;
}

struct pieceofcrap {
	struct gaim_connection *gc;
	unsigned long offset;
	unsigned long len;
	char *modname;
	int fd;
	aim_conn_t *conn;
	unsigned int inpa;
};

static void damn_you(gpointer data, gint source, GaimInputCondition c)
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
		gaim_input_remove(pos->inpa);
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
	gaim_input_remove(pos->inpa);
	close(pos->fd);
	aim_sendmemblock(od->sess, pos->conn, 0, 16, m, AIM_SENDMEMBLOCK_FLAG_ISHASH);
	g_free(pos);
}

static void straight_to_hell(gpointer data, gint source, GaimInputCondition cond) {
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
	pos->inpa = gaim_input_add(pos->fd, GAIM_INPUT_READ, damn_you, pos);
	return;
}

/* size of icbmui.ocm, the largest module in AIM 3.5 */
#define AIM_MAX_FILE_SIZE 98304

int gaim_memrequest(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct pieceofcrap *pos;
	fu32_t offset, len;
	char *modname;
	int fd;

	va_start(ap, fr);
	offset = (fu32_t)va_arg(ap, unsigned long);
	len = (fu32_t)va_arg(ap, unsigned long);
	modname = va_arg(ap, char *);
	va_end(ap);

	debug_printf("offset: %d, len: %d, file: %s\n", offset, len, modname ? modname : "aim.exe");
	if (len == 0) {
		debug_printf("len is 0, hashing NULL\n");
		aim_sendmemblock(sess, fr->conn, offset, len, NULL,
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
	pos->conn = fr->conn;

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

static int gaim_parse_login(aim_session_t *sess, aim_frame_t *fr, ...) {
#if 0
	struct client_info_s info = {"gaim", 4, 1, 2010, "us", "en", 0x0004, 0x0000, 0x04b};
#else
	struct client_info_s info = AIM_CLIENTINFO_KNOWNGOOD;
#endif
	char *key;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(sess, fr->conn, gc->username, gc->password, &info, key);

	return 1;
}

static int server_ready_auth(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

	aim_auth_setversions(sess, fr->conn);
	aim_bos_reqrate(sess, fr->conn);
	debug_printf("done with AUTH ServerReady\n");
	if (od->chpass) {
		debug_printf("changing password\n");
		aim_auth_changepasswd(sess, fr->conn, od->newp, od->oldp);
		g_free(od->oldp);
		g_free(od->newp);
		od->chpass = FALSE;
	}
	if (od->conf) {
		debug_printf("confirming account\n");
		aim_auth_reqconfirm(sess, fr->conn);
		od->conf = FALSE;
	}
	if (od->reqemail) {
		debug_printf("requesting email\n");
		aim_auth_getinfo(sess, fr->conn, 0x0011);
		od->reqemail = FALSE;
	}

	return 1;
}

static int server_ready_bos(aim_session_t *sess, aim_frame_t *fr, ...) {
	aim_setversions(sess, fr->conn);
	aim_bos_reqrate(sess, fr->conn); /* request rate info */
	debug_printf("done with BOS ServerReady\n");

	return 1;
}

static int server_ready_chatnav(aim_session_t *sess, aim_frame_t *fr, ...) {
	debug_printf("chatnav: got server ready\n");
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);
	aim_bos_reqrate(sess, fr->conn);
	aim_bos_ackrateresp(sess, fr->conn);
	aim_chatnav_clientready(sess, fr->conn);
	aim_chatnav_reqrights(sess, fr->conn);

	return 1;
}

static int server_ready_chat(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *chatcon;
	static int id = 1;

	debug_printf("chat: got server ready\n");
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_chat_join, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_chat_leave, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_chat_info_update, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_chat_incoming_msg, 0);
	aim_bos_reqrate(sess, fr->conn);
	aim_bos_ackrateresp(sess, fr->conn);
	aim_chat_clientready(sess, fr->conn);
	chatcon = find_oscar_chat_by_conn(gc, fr->conn);
	chatcon->id = id;
	chatcon->cnv = serv_got_joined_chat(gc, id++, chatcon->show);

	return 1;
}

static void oscar_chatnav_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	aim_session_t *sess;
	aim_conn_t *tstconn;

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
	odata->cnpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ,
					oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_auth_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *odata;
	aim_session_t *sess;
	aim_conn_t *tstconn;

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
	odata->paspa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ,
				oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_chat_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct chat_connection *ccon = data;
	struct gaim_connection *gc = ccon->gc;
	struct oscar_data *odata;
	aim_session_t *sess;
	aim_conn_t *tstconn;

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
	ccon->inpa = gaim_input_add(tstconn->fd,
			GAIM_INPUT_READ,
			oscar_callback, tstconn);
	odata->oscar_chats = g_slist_append(odata->oscar_chats, ccon);
	aim_chat_attachname(tstconn, ccon->name);
}

/* Hrmph. I don't know how to make this look better. --mid */
static int gaim_handle_redirect(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t serviceid;
	char *ip;
	fu8_t *cookie;
	struct gaim_connection *gc = sess->aux_data;
	struct aim_user *user = gc->user;
	aim_conn_t *tstconn;
	int i;
	char *host;
	int port;

	port = user->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(user->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, fr);
	serviceid = (fu16_t)va_arg(ap, unsigned int);
	ip = va_arg(ap, char *);
	cookie = (fu8_t *)va_arg(ap, unsigned char *);

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
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, server_ready_auth, 0);
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0007, rateresp_auth, 0);
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
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, server_ready_chatnav, 0);

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
		char *roomname;
		fu16_t exchange;
		struct chat_connection *ccon;

		roomname = va_arg(ap, char *);
		exchange = (fu16_t)va_arg(ap, unsigned int);

		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to connect to chat server\n");
			g_free(host);
			return 1;
		}

		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, server_ready_chat, 0);
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

static int gaim_parse_oncoming(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_userinfo_s *info;
	time_t time_idle;
	int type = 0;
	struct gaim_connection *gc = sess->aux_data;
	char *tmp;

	va_list ap;
	va_start(ap, fr);
	info = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	if (info->flags & AIM_FLAG_UNCONFIRMED)
		type |= UC_UNCONFIRMED;
	if (info->flags & AIM_FLAG_ADMINISTRATOR)
		type |= UC_ADMIN;
	if (info->flags & AIM_FLAG_AOL)
		type |= UC_AOL;
	if (info->flags & AIM_FLAG_FREE)
		type |= UC_NORMAL;
	if (info->flags & AIM_FLAG_AWAY)
		type |= UC_UNAVAILABLE;

	if (info->idletime) {
		time(&time_idle);
		time_idle -= info->idletime*60;
	} else
		time_idle = 0;

	tmp = g_strdup(normalize(gc->username));
	if (!strcmp(tmp, normalize(info->sn)))
		g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", info->sn);
	g_free(tmp);

	serv_got_update(gc, info->sn, 1, info->warnlevel/10, info->onlinesince,
			time_idle, type, info->capabilities);

	return 1;
}

static int gaim_parse_offgoing(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_userinfo_s *info;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
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

static void oscar_directim_callback(gpointer data, gint source, GaimInputCondition condition) {
	struct direct_im *dim = data;
	struct gaim_connection *gc = dim->gc;
	struct oscar_data *od = gc->proto_data;
	struct conversation *cnv;
	char buf[256];

	if (!g_slist_find(connections, gc)) {
		g_free(dim);
		return;
	}

	if (source < 0) {
		g_free(dim);
		return;
	}

	if (dim->conn->fd != source)
		dim->conn->fd = source;

	aim_conn_completeconnect(od->sess, dim->conn);
	if (!(cnv = find_conversation(dim->name))) 
		cnv = new_conversation(dim->name);
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), dim->name);
	write_to_conv(cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	od->direct_ims = g_slist_append(od->direct_ims, dim);

	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
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

	dim->conn = aim_directim_connect(od->sess, d->sn, NULL, d->cookie);
	if (!dim->conn) {
		g_free(dim);
		cancel_direct_im(w, d);
		return TRUE;
	}

	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_directim_incoming, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_directim_typing, 0);

	for (i = 0; i < (int)strlen(d->ip); i++) {
		if (d->ip[i] == ':') {
			port = atoi(&(d->ip[i+1]));
			break;
		}
	}
	host = g_strndup(d->ip, i);
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

static int incomingim_chan1(aim_session_t *sess, aim_conn_t *conn, struct aim_userinfo_s *userinfo, struct aim_incomingim_ch1_args *args) {
	char *tmp = g_malloc(BUF_LONG);
	struct gaim_connection *gc = sess->aux_data;
	int flags = 0;

	if (args->icbmflags & AIM_IMFLAGS_AWAY)
		flags |= IM_FLAG_AWAY;

	if (args->icbmflags & AIM_IMFLAGS_HASICON) {
		struct oscar_data *od = gc->proto_data;
		struct icon_req *ir = NULL;
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
		if ((args->iconlen != ir->length) ||
		    (args->iconsum != ir->checksum) ||
		    (args->iconstamp != ir->timestamp))
			ir->request = TRUE;
		ir->length = args->iconlen;
		ir->checksum = args->iconsum;
		ir->timestamp = args->iconstamp;
	}

	if (gc->user->iconfile && (args->icbmflags & AIM_IMFLAGS_BUDDYREQ)) {
		FILE *file;
		struct stat st;

		if (!stat(gc->user->iconfile, &st)) {
			char *buf = g_malloc(st.st_size);
			file = fopen(gc->user->iconfile, "r");
			if (file) {
				fread(buf, 1, st.st_size, file);
				aim_send_icon(sess, conn, userinfo->sn, buf, st.st_size,
					      st.st_mtime, aim_iconsum(buf, st.st_size));
				fclose(file);
			}
			g_free(buf);
		}
	}

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
				
				g_snprintf(tmp+strlen(tmp), BUF_LONG-strlen(tmp), "%c", uni);
				
			} else { /* something else, do UNICODE entity */
				g_snprintf(tmp+strlen(tmp), BUF_LONG-strlen(tmp), "&#%04x;", uni);
			}
		}
	} else
		g_snprintf(tmp, BUF_LONG, "%s", args->msg);

	serv_got_im(gc, userinfo->sn, tmp, flags, time(NULL));
	g_free(tmp);

	return 1;
}

static int incomingim_chan2(aim_session_t *sess, aim_conn_t *conn, struct aim_userinfo_s *userinfo, struct aim_incomingim_ch2_args *args) {
	struct gaim_connection *gc = sess->aux_data;

	if (args->reqclass & AIM_CAPS_CHAT) {
		char *name = extract_name(args->info.chat.roominfo.name);
		int *exch = g_new0(int, 1);
		GList *m = NULL;
		m = g_list_append(m, g_strdup(name ? name : args->info.chat.roominfo.name));
		*exch = args->info.chat.roominfo.exchange;
		m = g_list_append(m, exch);
		serv_got_chat_invite(gc,
				     name ? name : args->info.chat.roominfo.name,
				     userinfo->sn,
				     args->info.chat.msg,
				     m);
		if (name)
			g_free(name);
	} else if (args->reqclass & AIM_CAPS_SENDFILE) {
	} else if (args->reqclass & AIM_CAPS_GETFILE) {
	} else if (args->reqclass & AIM_CAPS_VOICE) {
	} else if (args->reqclass & AIM_CAPS_BUDDYICON) {
		set_icon_data(gc, normalize(userinfo->sn), args->info.icon.icon,
				args->info.icon.length);
	} else if (args->reqclass & AIM_CAPS_IMIMAGE) {
		struct ask_direct *d = g_new0(struct ask_direct, 1);
		char buf[256];

		debug_printf("%s received direct im request from %s (%s)\n",
				gc->username, userinfo->sn, args->info.imimage.ip);

		d->gc = gc;
		d->sn = g_strdup(userinfo->sn);
		strncpy(d->ip, args->info.imimage.ip, sizeof(d->ip));
		memcpy(d->cookie, args->cookie, 8);
		g_snprintf(buf, sizeof buf, "%s has just asked to directly connect to %s.",
				userinfo->sn, gc->username);
		do_ask_dialog(buf, d, accept_direct_im, cancel_direct_im);
	} else {
		debug_printf("Unknown reqclass %d\n", args->reqclass);
	}

	return 1;
}


static int gaim_parse_incoming_im(aim_session_t *sess, aim_frame_t *fr, ...) {
	int channel, ret = 0;
	struct aim_userinfo_s *userinfo;
	va_list ap;

	va_start(ap, fr);
	channel = va_arg(ap, int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);

	/* channel 1: standard message */
	if (channel == 1) {
		struct aim_incomingim_ch1_args *args;

		args = va_arg(ap, struct aim_incomingim_ch1_args *);

		ret = incomingim_chan1(sess, fr->conn, userinfo, args);

	} else if (channel == 2) {
		struct aim_incomingim_ch2_args *args;

		args = va_arg(ap, struct aim_incomingim_ch2_args *);

		ret = incomingim_chan2(sess, fr->conn, userinfo, args);
	}

	va_end(ap);

	return ret;
}

static int gaim_parse_misses(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t chan, nummissed, reason;
	struct aim_userinfo_s *userinfo;
	char buf[1024];

	va_start(ap, fr);
	chan = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	nummissed = (fu16_t)va_arg(ap, unsigned int);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	switch(reason) {
		case 0:
			/* Invalid (0) */
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s because %s invalid."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   nummissed == 1 ? "it was" : "they were",
				   userinfo->sn);
			break;
		case 1:
			/* Message too large */
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s because %s too large."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   nummissed == 1 ? "it was" : "they were",
				   userinfo->sn);
			break;
		case 2:
			/* Rate exceeded */
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s because the rate limit has been exceeded."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   userinfo->sn);
			break;
		case 3:
			/* Evil Sender */
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s because they are too evil."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   userinfo->sn);
			break;
		case 4:
			/* Evil Receiver */
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s because you are too evil."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   userinfo->sn);
			break;
		default:
			g_snprintf(buf,
				   sizeof(buf),
				   _("You missed %d message%s from %s for unknown reasons."),
				   nummissed,
				   nummissed == 1 ? "" : "s",
				   userinfo->sn);
			break;
	}
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

static int gaim_parse_genericerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t reason;

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("snac threw error (reason 0x%04x: %s\n", reason,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown");

	return 1;
}

static int gaim_parse_msgerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *destn;
	fu16_t reason;
	char buf[1024];

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	sprintf(buf, _("Your message to %s did not get sent: %s"), destn,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : _("Reason unknown"));
	do_error_dialog(buf, _("Gaim - Error"));

	return 1;
}

static int gaim_parse_locerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *destn;
	fu16_t reason;
	char buf[1024];

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
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

static int gaim_parse_user_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_userinfo_s *info;
	char *prof_enc = NULL, *prof = NULL;
	fu16_t infotype;
	char buf[BUF_LONG];
	char legend[BUF_LONG];
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *asc;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_userinfo_s *);
	prof_enc = va_arg(ap, char *);
	prof = va_arg(ap, char *);
	infotype = (fu16_t)va_arg(ap, unsigned int);
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
			"Idle Minutes : <B>%d</B>\n<BR>\n<HR><BR>\n"),
			info->sn, images(info->flags),
			asc,
			info->warnlevel/10,
			asctime(localtime(&info->onlinesince)),
			info->idletime);
	g_snprintf(legend, sizeof legend,
			_("<br><BODY BGCOLOR=WHITE><hr><I>Legend:</I><br><br>"
			"<IMG SRC=\"free_icon.gif\"> : Normal AIM User<br>"
			"<IMG SRC=\"aol_icon.gif\"> : AOL User <br>"
			"<IMG SRC=\"dt_icon.gif\"> : Trial AIM User <br>"
			"<IMG SRC=\"admin_icon.gif\"> : Administrator"));
	g_show_info_text(buf,
			 (prof && strlen(prof)) ?
				away_subs(prof, gc->username)
				:
				(infotype == AIM_GETINFO_GENERALINFO ?
					_("<i>No Information Provided</i>") :
					_("<i>User has no away message</i>")),
			 legend,
			 NULL);

	g_free(asc);

	return 1;
}

static int gaim_parse_motd(aim_session_t *sess, aim_frame_t *fr, ...) {
	char *msg;
	fu16_t id;
	va_list ap;
	char buildbuf[150];

	va_start(ap, fr);
	id  = (fu16_t)va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	aim_getbuildstring(buildbuf, sizeof(buildbuf));

	debug_printf("MOTD: %s (%d)\n", msg ? msg : "Unknown", id);
	debug_printf("Gaim %s / libfaim %s\n", VERSION, buildbuf);
	if (id < 4)
		do_error_dialog(_("Your connection may be lost."),
				_("AOL error"));

	return 1;
}

static int gaim_chatnav_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;

	va_start(ap, fr);
	type = (fu16_t)va_arg(ap, unsigned int);

	switch(type) {
		case 0x0002: {
			fu8_t maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i;

			maxrooms = (fu8_t)va_arg(ap, unsigned int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);
			va_end(ap);

			debug_printf("chat info: Chat Rights:\n");
			debug_printf("chat info: \tMax Concurrent Rooms: %d\n", maxrooms);
			debug_printf("chat info: \tExchange List: (%d total)\n", exchangecount);
			for (i = 0; i < exchangecount; i++)
				debug_printf("chat info: \t\t%d\n", exchanges[i].number);
			if (odata->create_exchange) {
				debug_printf("creating room %s\n", odata->create_name);
				aim_chatnav_createroom(sess, fr->conn, odata->create_name,
						odata->create_exchange);
				odata->create_exchange = 0;
				g_free(odata->create_name);
				odata->create_name = NULL;
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			fu16_t instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
			fu8_t createperms;
			fu32_t createtime;

			fqcn = va_arg(ap, char *);
			instance = (fu16_t)va_arg(ap, unsigned int);
			exchange = (fu16_t)va_arg(ap, unsigned int);
			flags = (fu16_t)va_arg(ap, unsigned int);
			createtime = va_arg(ap, fu32_t);
			maxmsglen = (fu16_t)va_arg(ap, unsigned int);
			maxoccupancy = (fu16_t)va_arg(ap, unsigned int);
			createperms = (fu8_t)va_arg(ap, int);
			unknown = (fu16_t)va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);
			va_end(ap);

			debug_printf("created room: %s %d %d %d %lu %d %d %d %d %s %s\n",
					fqcn,
					exchange, instance, flags,
					createtime,
					maxmsglen, maxoccupancy, createperms, unknown,
					name, ck);
			aim_chat_join(odata->sess, odata->conn, exchange, ck, instance);
			}
			break;
		default:
			va_end(ap);
			debug_printf("chatnav info: unknown type (%04x)\n", type);
			break;
	}
	return 1;
}

static int gaim_chat_join(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		add_chat_buddy(c->cnv, info[i].sn);

	return 1;
}

static int gaim_chat_leave(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	struct aim_userinfo_s *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		remove_chat_buddy(c->cnv, info[i].sn);

	return 1;
}

static int gaim_chat_info_update(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct aim_userinfo_s *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	fu16_t unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	fu32_t creationtime;
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount= va_arg(ap, int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (fu16_t)va_arg(ap, int);
	creationtime = (fu32_t)va_arg(ap, unsigned long);
	maxmsglen = (fu16_t)va_arg(ap, int);
	unknown_d2 = (fu16_t)va_arg(ap, int);
	unknown_d5 = (fu16_t)va_arg(ap, int);
	maxvisiblemsglen = (fu16_t)va_arg(ap, int);
	va_end(ap);

	debug_printf("inside chat_info_update (maxmsglen = %d, maxvislen = %d)\n",
			maxmsglen, maxvisiblemsglen);

	ccon->maxlen = maxmsglen;
	ccon->maxvis = maxvisiblemsglen;

	return 1;
}

static int gaim_chat_incoming_msg(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct aim_userinfo_s *info;
	char *msg;
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);
	char *tmp;

	va_start(ap, fr);
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
static int gaim_parse_msgack(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	char *sn;

	va_start(ap, fr);
	type = (fu16_t)va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Sent message to %s.\n", sn);

	return 1;
}

static int gaim_parse_ratechange(aim_session_t *sess, aim_frame_t *fr, ...) {
	static const char *codes[5] = {
		"invalid",
		 "change",
		 "warning",
		 "limit",
		 "limit cleared",
	};
	va_list ap;
	fu16_t code, rateclass;
	fu32_t windowsize, clear, alert, limit, disconnect, currentavg, maxavg;

	va_start(ap, fr); 
	code = (fu16_t)va_arg(ap, unsigned int);
	rateclass= (fu16_t)va_arg(ap, unsigned int);
	windowsize = (fu32_t)va_arg(ap, unsigned long);
	clear = (fu32_t)va_arg(ap, unsigned long);
	alert = (fu32_t)va_arg(ap, unsigned long);
	limit = (fu32_t)va_arg(ap, unsigned long);
	disconnect = (fu32_t)va_arg(ap, unsigned long);
	currentavg = (fu32_t)va_arg(ap, unsigned long);
	maxavg = (fu32_t)va_arg(ap, unsigned long);
	va_end(ap);

	debug_printf("rate %s (paramid 0x%04lx): curavg = %ld, maxavg = %ld, alert at %ld, "
		     "clear warning at %ld, limit at %ld, disconnect at %ld (window size = %ld)\n",
		     (code < 5) ? codes[code] : codes[0],
		     rateclass,
		     currentavg, maxavg,
		     alert, clear,
		     limit, disconnect,
		     windowsize);

	/* XXX fix these values */
	if (code == AIM_RATE_CODE_CHANGE) {
		if (currentavg >= clear)
			aim_conn_setlatency(fr->conn, 0);
	} else if (code == AIM_RATE_CODE_WARNING) {
		aim_conn_setlatency(fr->conn, windowsize/4);
	} else if (code == AIM_RATE_CODE_LIMIT) {
		aim_conn_setlatency(fr->conn, windowsize/2);
	} else if (code == AIM_RATE_CODE_CLEARLIMIT) {
		aim_conn_setlatency(fr->conn, 0);
	}

	return 1;
}

static int gaim_parse_evilnotify(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t newevil;
	struct aim_userinfo_s *userinfo;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	newevil = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	serv_got_eviled(gc, (userinfo && userinfo->sn[0]) ? userinfo->sn : NULL, newevil / 10);

	return 1;
}

static int rateresp_bos(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;

	aim_bos_ackrateresp(sess, fr->conn);
	aim_bos_reqpersonalinfo(sess, fr->conn);
	aim_bos_reqlocaterights(sess, fr->conn);
	aim_bos_setprofile(sess, fr->conn, gc->user->user_info, NULL, gaim_caps);
	aim_bos_reqbuddyrights(sess, fr->conn);

	account_online(gc);
	serv_finish_login(gc);

	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);

	debug_printf("buddy list loaded\n");

	aim_reqicbmparams(sess, fr->conn);

	aim_bos_reqrights(sess, fr->conn);
	aim_bos_setgroupperm(sess, fr->conn, AIM_FLAG_ALLUSERS);
	aim_bos_setprivacyflags(sess, fr->conn, AIM_PRIVFLAGS_ALLOWIDLE |
						     AIM_PRIVFLAGS_ALLOWMEMBERSINCE);

	return 1;
}

static int rateresp_auth(aim_session_t *sess, aim_frame_t *fr, ...) {
	aim_bos_ackrateresp(sess, fr->conn);
	aim_auth_clientready(sess, fr->conn);
	debug_printf("connected to auth (admin)\n");

	return 1;
}

static int gaim_icbm_param_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_icbmparameters *params;
	va_list ap;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	debug_printf("ICBM Parameters: maxchannel = %d, default flags = 0x%08lx, max msg len = %d, "
			"max sender evil = %f, max receiver evil = %f, min msg interval = %ld\n",
			params->maxchan, params->flags, params->maxmsglen,
			((float)params->maxsenderwarn)/10.0, ((float)params->maxrecverwarn)/10.0,
			params->minmsginterval);

	/* Maybe senderwarn and recverwarn should be user preferences... */
	params->maxmsglen = 8000;
	params->minmsginterval = 0;

	aim_seticbmparam(sess, fr->conn, params);

	return 1;
}

/* XXX this is frivelous... do you really want to know this info? */
static int gaim_parse_buddyrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t maxbuddies, maxwatchers;

	va_start(ap, fr);
	maxbuddies = (fu16_t)va_arg(ap, unsigned int);
	maxwatchers = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("buddy list rights: Max buddies = %d / Max watchers = %d\n", maxbuddies, maxwatchers);

	return 1;
}

static int gaim_bosrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t maxpermits, maxdenies;
	va_list ap;

	va_start(ap, fr);
	maxpermits = (fu16_t)va_arg(ap, unsigned int);
	maxdenies = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("BOS rights: Max permit = %d / Max deny = %d\n", maxpermits, maxdenies);

	aim_bos_clientready(sess, fr->conn);

	aim_bos_reqservice(sess, fr->conn, AIM_CONN_TYPE_CHATNAV);

	return 1;
}

static int gaim_parse_searchreply(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *address, *SNs;
	int i, num;
	char *buf;
	int at = 0, len;

	va_start(ap, fr);
	address = va_arg(ap, char *);
	num = va_arg(ap, int);
	SNs = va_arg(ap, char *);
	va_end(ap);

	len = num * (MAXSNLEN + 1) + 1024;
	buf = g_malloc(len);
	at += g_snprintf(buf + at, len - at, "<B>%s has the following screen names:</B><BR>", address);
	for (i = 0; i < num; i++)
		at += g_snprintf(buf + at, len - at, "%s<BR>", &SNs[i * (MAXSNLEN + 1)]);
	g_show_info_text(buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_parse_searcherror(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *address;
	char buf[BUF_LONG];

	va_start(ap, fr);
	address = va_arg(ap, char *);
	va_end(ap);

	g_snprintf(buf, sizeof(buf), "No results found for email address %s", address);
	do_error_dialog(buf, _("Error"));

	return 1;
}

static int gaim_account_confirm(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t status;
	va_list ap;
	char msg[256];
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	status = (fu16_t)va_arg(ap, unsigned int); /* status code of confirmation request */
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

static int gaim_info_change(aim_session_t *sess, aim_frame_t *fr, ...) {
	int change, str;
	fu16_t perms, type, length;
	char *val;
	va_list ap;
	char buf[BUF_LONG];
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	change = va_arg(ap, int);
	perms = (fu16_t)va_arg(ap, unsigned int);
	type = (fu16_t)va_arg(ap, unsigned int);
	length = (fu16_t)va_arg(ap, unsigned int);
	val = va_arg(ap, char *);
	str = va_arg(ap, int);
	va_end(ap);

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

static int oscar_send_im(struct gaim_connection *gc, char *name, char *message, int imflags) {
	struct oscar_data *odata = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim = find_direct_im(odata, name);
	int ret = 0;
	if (dim) {
		ret = aim_send_im_direct(odata->sess, dim->conn, message);
	} else {
		if (imflags & IM_FLAG_AWAY)
			ret = aim_send_im(odata->sess, odata->conn, name, AIM_IMFLAGS_AWAY, message);
		else {
			struct aim_sendimext_args args;
			GSList *h = odata->hasicons;
			struct icon_req *ir = NULL;
			char *who = normalize(name);
			struct stat st;

			args.flags = AIM_IMFLAGS_ACK | AIM_IMFLAGS_CUSTOMFEATURES;

			args.features = gaim_features;
			args.featureslen = sizeof(gaim_features);

			while (h) {
				ir = h->data;
				if (ir->request && !strcmp(who, ir->user))
					break;
				h = h->next;
			}
			if (h) {
				ir->request = FALSE;
				args.flags |= AIM_IMFLAGS_BUDDYREQ;
				debug_printf("sending buddy icon request with message\n");
			}

			if (gc->user->iconfile && !stat(gc->user->iconfile, &st)) {
				FILE *file = fopen(gc->user->iconfile, "r");
				if (file) {
					char *buf = g_malloc(st.st_size);
					fread(buf, 1, st.st_size, file);

					args.iconlen   = st.st_size;
					args.iconsum   = aim_iconsum(buf, st.st_size);
					args.iconstamp = st.st_mtime;

					args.flags |= AIM_IMFLAGS_HASICON;

					fclose(file);
					g_free(buf);
				}
			}

			args.destsn = name;
			args.msg    = message;
			args.msglen = strlen(message);

			ret = aim_send_im_ext(odata->sess, odata->conn, &args);
		}
	}
	if (ret >= 0)
		return 1;
	return ret;
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
	aim_send_warning(odata->sess, odata->conn, name, anon ? AIM_WARN_ANON : 0);
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

static GList *oscar_chat_info(struct gaim_connection *gc) {
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Join what group:");
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Exchange:");
	pce->is_int = TRUE;
	pce->min = 4;
	pce->max = 20;
	m = g_list_append(m, pce);

	return m;
}

static void oscar_join_chat(struct gaim_connection *g, GList *data) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	aim_conn_t *cur;
	char *name;
	int *exchange;

	if (!data || !data->next)
		return;

	name = data->data;
	exchange = data->next->data;

	debug_printf("Attempting to join chat room %s.\n", name);
	if ((cur = aim_getconn_type(odata->sess, AIM_CONN_TYPE_CHATNAV))) {
		debug_printf("chatnav exists, creating room\n");
		aim_chatnav_createroom(odata->sess, cur, name, *exchange);
	} else {
		/* this gets tricky */
		debug_printf("chatnav does not exist, opening chatnav\n");
		odata->create_exchange = *exchange;
		odata->create_name = g_strdup(name);
		aim_bos_reqservice(odata->sess, odata->conn, AIM_CONN_TYPE_CHATNAV);
	}
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
			gaim_input_remove(c->inpa);
		if (g && odata->sess)
			aim_conn_kill(odata->sess, &c->conn);
		g_free(c->name);
		g_free(c->show);
		g_free(c);
	}
	/* we do this because with Oscar it doesn't tell us we left */
	serv_got_chat_left(g, b->id);
}

static int oscar_chat_send(struct gaim_connection *g, int id, char *message) {
	struct oscar_data *odata = (struct oscar_data *)g->proto_data;
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	struct chat_connection *c = NULL;
	char *buf, *buf2;
	int i, j;

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return -EINVAL;

	bcs = odata->oscar_chats;
	while (bcs) {
		c = (struct chat_connection *)bcs->data;
		if (b == c->cnv)
			break;
		bcs = bcs->next;
		c = NULL;
	}
	if (!c)
		return -EINVAL;

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

	if (strlen(buf) > c->maxlen)
		return -E2BIG;

	buf2 = strip_html(buf);
	if (strlen(buf2) > c->maxvis) {
		g_free(buf2);
		return -E2BIG;
	}
	g_free(buf2);

	aim_chat_send_im(odata->sess, c->conn, 0, buf, strlen(buf));
	g_free(buf);
	return 0;
}

static char **oscar_list_icon(int uc) {
	if (uc & UC_UNAVAILABLE)
		return (char **)away_icon_xpm;
	if (uc & UC_AOL)
		return (char **)aol_icon_xpm;
	if (uc & UC_ADMIN)
		return (char **)admin_icon_xpm;
	if (uc & UC_UNCONFIRMED)
		return (char **)dt_icon_xpm;
	if (uc & UC_NORMAL)
		return (char **)free_icon_xpm;
	return NULL;
}

static int gaim_directim_initiate(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	aim_conn_t *newconn, *listenerconn;
	struct conversation *cnv;
	struct direct_im *dim;
	char buf[256];
	char *sn;

	va_start(ap, fr);
	newconn = va_arg(ap, aim_conn_t *);
	listenerconn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	aim_conn_close(listenerconn);
	aim_conn_kill(sess, &listenerconn);

	sn = g_strdup(aim_directim_getsn(newconn));

	debug_printf("DirectIM: initiate success to %s\n", sn);
	dim = find_direct_im(od, sn);

	if (!(cnv = find_conversation(sn)))
		cnv = new_conversation(sn);
	gaim_input_remove(dim->watcher);
	dim->conn = newconn;
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
					oscar_callback, dim->conn);
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), sn);
	g_free(sn);
	write_to_conv(cnv, buf, WFLAG_SYSTEM, NULL, time((time_t)NULL));

	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_directim_incoming, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_directim_typing, 0);

	return 1;
}

static int gaim_directim_incoming(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *msg, *sn;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Got DirectIM message from %s\n", sn);

	serv_got_im(gc, sn, msg, 0, time((time_t)NULL));

	return 1;
}

static int gaim_directim_typing(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *sn;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	va_end(ap);

	/* I had to leave this. It's just too funny. It reminds me of my sister. */
	debug_printf("ohmigod! %s has started typing (DirectIM). He's going to send you a message! *squeal*\n", sn);

	return 1;
}

struct ask_do_dir_im {
	char *who;
	struct gaim_connection *gc;
};

static void oscar_cancel_direct_im(gpointer obj, struct ask_do_dir_im *data) {
	g_free(data);
}

static void oscar_direct_im(gpointer obj, struct ask_do_dir_im *data) {
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

	dim->conn = aim_directim_initiate(od->sess, od->conn, data->who);
	if (dim->conn != NULL) {
		od->direct_ims = g_slist_append(od->direct_ims, dim);
		dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
						oscar_callback, dim->conn);
		aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINITIATE,
					gaim_directim_initiate, 0);
	} else {
		do_error_dialog(_("Unable to open Direct IM"), _("Error"));
		g_free(dim);
	}
}

static void oscar_ask_direct_im(struct gaim_connection *gc, gchar *who) {
	char buf[BUF_LONG];
	struct ask_do_dir_im *data = g_new0(struct ask_do_dir_im, 1);
	data->who = who;
	data->gc = gc;
	g_snprintf(buf, sizeof(buf),  _("You have selected to open a Direct IM connection with %s."
					" Doing this will let them see your IP address, and may be"
					" a security risk. Do you wish to continue?"), who);
	do_ask_dialog(buf, data, oscar_direct_im, oscar_cancel_direct_im);
}

static GList *oscar_buddy_menu(struct gaim_connection *gc, char *who) {
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	char *n = g_strdup(normalize(gc->username));

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Info");
	pbm->callback = oscar_get_info;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Away Msg");
	pbm->callback = oscar_get_away_msg;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	if (strcmp(n, normalize(who))) {
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Direct IM");
		pbm->callback = oscar_ask_direct_im;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
	}
	g_free(n);

	return m;
}

static GList *oscar_user_opts()
{
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Auth Host:";
	puo->def = "login.oscar.aol.com";
	puo->pos = USEROPT_AUTH;
	m = g_list_append(m, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Auth Port:";
	puo->def = "5190";
	puo->pos = USEROPT_AUTHPORT;
	m = g_list_append(m, puo);

	return m;
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

static GList *oscar_away_states()
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void oscar_do_action(struct gaim_connection *gc, char *act)
{
	struct oscar_data *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

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

static void oscar_convo_closed(struct gaim_connection *gc, char *who)
{
	struct oscar_data *od = gc->proto_data;
	struct direct_im *dim = find_direct_im(od, who);

	if (!dim)
		return;

	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	gaim_input_remove(dim->watcher);
	aim_conn_kill(od->sess, &dim->conn);
	g_free(dim);
}

static struct prpl *my_protocol = NULL;

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
	ret->chat_info = oscar_chat_info;
	ret->join_chat = oscar_join_chat;
	ret->chat_invite = oscar_chat_invite;
	ret->chat_leave = oscar_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = oscar_chat_send;
	ret->keepalive = oscar_keepalive;
	ret->convo_closed = oscar_convo_closed;

	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(oscar_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_OSCAR);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "Oscar";
}

char *description()
{
	return PRPL_DESC("Oscar");
}

#endif
