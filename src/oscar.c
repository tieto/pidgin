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
#include "gaim.h"
#include <aim.h>
#include "gnome_applet_mgr.h"

static int inpa = -1;
static int paspa = -1;
struct aim_session_t *gaim_sess;
struct aim_conn_t    *gaim_conn;
int gaim_caps = AIM_CAPS_CHAT | AIM_CAPS_SENDFILE | AIM_CAPS_GETFILE |
		AIM_CAPS_VOICE | AIM_CAPS_IMIMAGE | AIM_CAPS_BUDDYICON;
int USE_OSCAR = 0;

GList *oscar_chats = NULL;

struct chat_connection *find_oscar_chat(char *name) {
	GList *g = oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (!strcmp(name, c->name))
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static int gaim_parse_auth_resp  (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_auth_server_ready(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_server_ready     (struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_chat_server_ready(struct aim_session_t *, struct command_rx_struct *, ...);
static int gaim_handle_redirect  (struct aim_session_t *, struct command_rx_struct *, ...);
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

extern void auth_failed();

static void oscar_callback(gpointer data, gint source,
				GdkInputCondition condition) {
	struct aim_conn_t *conn = (struct aim_conn_t *)data;

	if (condition & GDK_INPUT_EXCEPTION) {
		signoff();
		hide_login_progress(_("Disconnected."));
		aim_logoff(gaim_sess);
		gdk_input_remove(inpa);
		auth_failed();
		return;
	}
	if (condition & GDK_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_RENDEZVOUS_OUT) {
			if (aim_handlerendconnect(gaim_sess, conn) < 0) {
				debug_print(_("connection error (rend)\n"));
			}
		} else {
			if (aim_get_command(gaim_sess, conn) >= 0) {
				aim_rxdispatch(gaim_sess);
			} else {
				debug_print(_("connection error!\n"));
				aim_conn_kill(gaim_sess, &conn);
				if (!aim_getconn_type(gaim_sess, AIM_CONN_TYPE_BOS)) {
					debug_print(_("major connection error\n"));
					signoff();
					hide_login_progress(_("Disconnected."));
					aim_logoff(gaim_sess);
					auth_failed();
					gdk_input_remove(inpa);
				}
			}
		}
	}
}

int oscar_login(char *username, char *password) {
	struct aim_session_t *sess;
	struct aim_conn_t *conn;
	struct client_info_s info = {"AOL Instant Messenger (TM), version 2.1.1187/WIN32", 4, 30, 3141, "us", "en", 0x0004, 0x0001, 0x055};
	struct aim_user *u;
	char buf[256];

	sprintf(debug_buff, _("Logging in %s\n"), username);
	debug_print(debug_buff);

	sess = g_new0(struct aim_session_t, 1);
	aim_session_init(sess);
	/* we need an immediate queue because we don't use a while-loop to
	 * see if things need to be sent. */
	sess->tx_enqueue = &aim_tx_enqueue__immediate;
	gaim_sess = sess;

	sprintf(buf, _("Looking up %s"), FAIM_LOGIN_SERVER);
	set_login_progress(1, buf);
	conn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, FAIM_LOGIN_SERVER);

	if (conn == NULL) {
		debug_print(_("internal connection error\n"));
#ifdef USE_APPLET
		setUserState(offline);
#endif
		set_state(STATE_OFFLINE);
		hide_login_progress(_("Unable to login to AIM"));
		return -1;
	} else if (conn->fd == -1) {
#ifdef USE_APPLET
		setUserState(offline);
#endif
		set_state(STATE_OFFLINE);
		if (conn->status & AIM_CONN_STATUS_RESOLVERR) {
			sprintf(debug_buff, _("couldn't resolve host\n"));
			debug_print(debug_buff);
			hide_login_progress(debug_buff);
		} else if (conn->status & AIM_CONN_STATUS_CONNERR) {
			sprintf(debug_buff, _("couldn't connect to host\n"));
			debug_print(debug_buff);
			hide_login_progress(debug_buff);
		}
		return -1;
	}
	g_snprintf(buf, sizeof(buf), _("Signon: %s"), username);
	set_login_progress(2, buf);

	aim_conn_addhandler(sess, conn, AIM_CB_FAM_SPECIAL,
				AIM_CB_SPECIAL_AUTHSUCCESS,
				gaim_parse_auth_resp, 0);
	aim_conn_addhandler(sess, conn, AIM_CB_FAM_GEN,
				AIM_CB_GEN_SERVERREADY,
				gaim_auth_server_ready, 0);
	aim_send_login(sess, conn, username, password, &info);

	inpa = gdk_input_add(conn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, conn);

	u = find_user(username);

	if (!u) {
		u = g_new0(struct aim_user, 1);
		g_snprintf(u->username, sizeof(u->username), DEFAULT_INFO);
		aim_users = g_list_append(aim_users, u);
	}
	current_user = u;
	g_snprintf(current_user->username, sizeof(current_user->username),
				"%s", username);
	g_snprintf(current_user->password, sizeof(current_user->password),
				"%s", password);
	save_prefs();

	debug_print(_("Password sent, waiting for response\n"));

	return 0;
}

void oscar_close() {
#ifdef USE_APPLET
	setUserState(offline);
#endif
	set_state(STATE_OFFLINE);
	if (inpa > 0)
		gdk_input_remove(inpa);
	inpa = -1;
	aim_logoff(gaim_sess);
	debug_print(_("Signed off.\n"));
}

int gaim_parse_auth_resp(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	struct aim_conn_t *bosconn = NULL;
	sprintf(debug_buff, "inside auth_resp (Screen name: %s)\n",
			sess->logininfo.screen_name);
	debug_print(debug_buff);

	if (sess->logininfo.errorcode) {
		switch (sess->logininfo.errorcode) {
		case 0x18:
			/* connecting too frequently */
			show_error_dialog("983\0\0");
			break;
		case 0x05:
			/* Incorrect nick/password */
			show_error_dialog("980\0\0");
			break;
		case 0x1c:
			/* client too old */
			show_error_dialog("981\0\0");
			break;
		}
		sprintf(debug_buff, "Login Error Code 0x%04x\n",
				sess->logininfo.errorcode);
		debug_print(debug_buff);
		sprintf(debug_buff, "Error URL: %s\n",
				sess->logininfo.errorurl);
		debug_print(debug_buff);
#ifdef USE_APPLET
		setUserState(offline);
#endif
		set_state(STATE_OFFLINE);
		hide_login_progress(_("Authentication Failed"));
		gdk_input_remove(inpa);
		aim_conn_kill(sess, &command->conn);
		auth_failed();
		return 0;
	}


	sprintf(debug_buff, "Email: %s\n", sess->logininfo.email);
	debug_print(debug_buff);
	sprintf(debug_buff, "Closing auth connection...\n");
	debug_print(debug_buff);
	gdk_input_remove(inpa);
	aim_conn_kill(sess, &command->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, sess->logininfo.BOSIP);
	if (bosconn == NULL) {
#ifdef USE_APPLET
		setUserState(offline);
#endif
		set_state(STATE_OFFLINE);
		hide_login_progress(_("Internal Error"));
		auth_failed();
		return -1;
	} else if (bosconn->status != 0) {
#ifdef USE_APPLET
		setUserState(offline);
#endif
		set_state(STATE_OFFLINE);
		hide_login_progress(_("Could Not Connect"));
		auth_failed();
		return -1;
	}

	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_SERVERREADY, gaim_server_ready, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATEINFO, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_STS, AIM_CB_STS_SETREPORTINTERVAL, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, gaim_parse_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, gaim_parse_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, gaim_parse_incoming_im, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, gaim_parse_ratechange, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, gaim_parse_user_info, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ACK, gaim_parse_msgack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_CTN, AIM_CB_CTN_DEFAULT, aim_parse_unknown, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_DEFAULT, aim_parse_unknown, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, gaim_parse_motd, 0);

	aim_auth_sendcookie(sess, bosconn, sess->logininfo.cookie);
	gaim_conn = bosconn;
	inpa = gdk_input_add(bosconn->fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
			oscar_callback, bosconn);
	set_login_progress(4, _("Connection established, cookie sent"));
	return 1;
}

gboolean change_password = FALSE;
char *old_password;
char *new_password;

int gaim_auth_server_ready(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	debug_print("Authorization server is ready.\n");
	aim_auth_clientready(sess, command->conn);
	if (change_password) {
		debug_print("Changing passwords...\n");
		aim_auth_changepasswd(sess, command->conn, old_password,
							   new_password);
		g_free(old_password);
		g_free(new_password);
		change_password = FALSE;
	}
	return 1;
}

int gaim_server_ready(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	switch (command->conn->type) {
	case AIM_CONN_TYPE_BOS:
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_bos_setprivacyflags(sess, command->conn, 0x00000003);
		aim_bos_reqservice(sess, command->conn, AIM_CONN_TYPE_ADS);
		aim_bos_setgroupperm(sess, command->conn, 0x1f);
		debug_print("done with BOS ServerReady\n");
		break;
	case AIM_CONN_TYPE_CHATNAV:
		debug_print("chatnav: got server ready\n");
		aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);
		aim_bos_reqrate(sess, command->conn);
		aim_bos_ackrateresp(sess, command->conn);
		aim_chatnav_clientready(sess, command->conn);
		aim_chatnav_reqrights(sess, command->conn);
		break;
	default: /* huh? */
		sprintf(debug_buff, "server ready: got unexpected connection type %04x\n", command->conn->type);
		debug_print(debug_buff);
		break;
	}
	return 1;
}

int gaim_chat_server_ready(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	static int id = 1;
	debug_print("chat: got server ready\n");
	aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_chat_join, 0);
	aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_chat_leave, 0);
	aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_chat_info_update, 0);
	aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_chat_incoming_msg, 0);
	aim_bos_reqrate(sess, command->conn);
	aim_bos_ackrateresp(sess, command->conn);
	aim_chat_clientready(sess, command->conn);
	serv_got_joined_chat(id++, aim_chat_getname(command->conn));
	return 1;
}

int gaim_handle_redirect(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	va_list ap;
	int serviceid;
	char *ip;
	char *cookie;

	va_start(ap, command);
	serviceid = va_arg(ap, int);
	ip        = va_arg(ap, char *);
	cookie    = va_arg(ap, char *);

	switch(serviceid) {
	case 0x0005: /* Ads */
		debug_print("Received Ads, finishing login\n");
		/* we'll take care of this in the parse_toc_buddy_config() below
		sprintf(buddies, "%s&", current_user->username);
		aim_bos_setbuddylist(sess, command->conn, buddies);
		*/
		aim_bos_setprofile(sess, command->conn, current_user->user_info,
					NULL, gaim_caps);
		aim_seticbmparam(sess, command->conn);
		aim_conn_setlatency(command->conn, 1);

#ifdef USE_APPLET
		make_buddy();
		if (general_options & OPT_GEN_APP_BUDDY_SHOW) {
			gnome_buddy_show();
			createOnlinePopup();
			set_applet_draw_open();
		} else {
			gnome_buddy_hide();
			set_applet_draw_closed();
		}
		setUserState(online);
		gtk_widget_hide(mainwindow);
#else
		gtk_widget_hide(mainwindow);
		show_buddy_list();
		refresh_buddy_window();
#endif
		serv_finish_login();
		gaim_setup();
		if (bud_list_cache_exists())
			do_import(NULL, 0);

		aim_bos_clientready(sess, command->conn);
		debug_print("Roger that, all systems go\n");
		aim_bos_reqservice(sess, command->conn, AIM_CONN_TYPE_CHATNAV);

		break;
	case 0x7: /* Authorizer */
		debug_print("Reconnecting with authorizor...\n");
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, ip);
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR)
			debug_print("unable to reconnect with authorizer\n");
		else {
			paspa = gdk_input_add(tstconn->fd,
					GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					oscar_callback, tstconn);
			aim_auth_sendcookie(sess, tstconn, cookie);
		}
		}
		break;
	case 0xd: /* ChatNav */
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, ip);
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR) {
			debug_print("unable to connect to chatnav server\n");
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_server_ready, 0);
		aim_auth_sendcookie(sess, tstconn, cookie);
		}
		debug_print("chatnav: connected\n");
		break;
	case 0xe: /* Chat */
		{
		struct aim_conn_t *tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, ip);
		char *roomname = va_arg(ap, char *);
		struct chat_connection *ccon;
		if (tstconn == NULL || tstconn->status >= AIM_CONN_STATUS_RESOLVERR) {
			debug_print("unable to connect to chat server\n");
			return 1;
		}
		sprintf(debug_buff, "Connected to chat room %s\n", roomname);
		debug_print(debug_buff);

		ccon = g_new0(struct chat_connection, 1);
		ccon->conn = tstconn;
		ccon->fd = tstconn->fd;
		ccon->name = g_strdup(roomname);
		
		ccon->inpa = gdk_input_add(tstconn->fd,
				GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
				oscar_callback, tstconn);

		oscar_chats = g_list_append(oscar_chats, ccon);
		
		aim_chat_attachname(tstconn, roomname);
		aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, gaim_chat_server_ready, 0);
		aim_auth_sendcookie(sess, tstconn, cookie);
		}
		break;
	default: /* huh? */
		sprintf(debug_buff, "got redirect for unknown service 0x%04x\n",
				serviceid);
		debug_print(debug_buff);
		break;
	}

	va_end(ap);

	return 1;
}

int gaim_parse_oncoming(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	time_t time_idle;
	int type = 0;

	va_list ap;
	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	if (info->class & AIM_CLASS_TRIAL)
		type |= UC_UNCONFIRMED;
	else if (info->class & AIM_CLASS_AOL)
		type |= UC_AOL;
	else if (info->class & AIM_CLASS_FREE)
		type |= UC_NORMAL;
	if (info->class & AIM_CLASS_AWAY)
		type |= UC_UNAVAILABLE;

	if (info->idletime) {
		time(&time_idle);
		time_idle -= info->idletime*60;
	} else
		time_idle = 0;

	serv_got_update(info->sn, 1, info->warnlevel, info->onlinesince,
			time_idle, type);

	return 1;
}

int gaim_parse_offgoing(struct aim_session_t *sess,
			struct command_rx_struct *command, ...) {
	char *sn;
	va_list ap;

	va_start(ap, command);
	sn = va_arg(ap, char *);
	va_end(ap);

	serv_got_update(sn, 0, 0, 0, 0, 0);

	return 1;
}

int gaim_parse_incoming_im(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	int channel;
	va_list ap;

	va_start(ap, command);
	channel = va_arg(ap, int);

	/* channel 1: standard message */
	if (channel == 1) {
		struct aim_userinfo_s *userinfo;
		char *msg = NULL;
		u_int icbmflags = 0;
		u_short flag1, flag2;

		userinfo  = va_arg(ap, struct aim_userinfo_s *);
		msg       = va_arg(ap, char *);
		icbmflags = va_arg(ap, u_int);
		flag1     = (u_short)va_arg(ap, u_int);
		flag2     = (u_short)va_arg(ap, u_int);
		va_end(ap);

		serv_got_im(userinfo->sn, msg, icbmflags & AIM_IMFLAGS_AWAY);
	} else if (channel == 2) {
		struct aim_userinfo_s *userinfo;
		int rendtype = va_arg(ap, int);
		if (rendtype & AIM_CAPS_CHAT) {
			char *msg, *encoding, *lang;
			struct aim_chat_roominfo *roominfo;

			userinfo = va_arg(ap, struct aim_userinfo_s *);
			roominfo = va_arg(ap, struct aim_chat_roominfo *);
			msg      = va_arg(ap, char *);
			encoding = va_arg(ap, char *);
			lang     = va_arg(ap, char *);
			va_end(ap);

			serv_got_chat_invite(roominfo->name,
					     roominfo->instance,
					     userinfo->sn,
					     msg);
		} else if (rendtype & AIM_CAPS_SENDFILE) {
			/* libfaim won't tell us that we got this just yet */
		} else if (rendtype & AIM_CAPS_GETFILE) {
			/* nor will it tell us this. but it's still there */
		} else if (rendtype & AIM_CAPS_VOICE) {
			/* this one libfaim tells us unuseful info about  */
		} else if (rendtype & AIM_CAPS_BUDDYICON) {
			/* bah */
		} else if (rendtype & AIM_CAPS_IMIMAGE) {
		} else {
			sprintf(debug_buff, "Unknown rendtype %d\n", rendtype);
			debug_print(debug_buff);
		}
	}

	return 1;
}

int gaim_parse_misses(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	u_short family;
	u_short subtype;

	family  = aimutil_get16(command->data+0);
	subtype = aimutil_get16(command->data+2);

	switch (family) {
	case 0x0001:
		if (subtype == 0x000a) {
			/* sending messages too fast */
			/* this also gets sent to us when our warning level
			 * changes, don't ask me why or how to interpret it */
			show_error_dialog("960\0someone");
		}
		break;
	case 0x0002:
		if (subtype == 0x0001) {
			/* unknown SNAC error */
			show_error_dialog("970\0\0");
		}
		break;
	case 0x0004:
		if (subtype == 0x0001) {
			/* user is not logged in */
			show_error_dialog("901\0User");
		} else if (subtype == 0x000a) {
			/* message has been dropped */
			show_error_dialog("903\0\0");
		}
		break;
	}

	return 1;
}

int gaim_parse_user_info(struct aim_session_t *sess,
			 struct command_rx_struct *command, ...) {
	struct aim_userinfo_s *info;
	char *prof_enc = NULL, *prof = NULL;
	u_short infotype;
	char buf[BUF_LONG];
	va_list ap;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	prof_enc = va_arg(ap, char *);
	prof = va_arg(ap, char *);
	infotype = (u_short)va_arg(ap, u_int);
	va_end(ap);

	if (prof == NULL || !strlen(prof)) {
		/* no info/away message */
		show_error_dialog("977\0\0");
		return 1;
	}

	snprintf(buf, sizeof buf, _("Username : <B>%s</B>\n<BR>"
				  "Warning Level : <B>%d %%</B>\n<BR>"
				  "Online Since : <B>%s</B><BR>"
				  "Idle Minutes : <B>%d</B>\n<BR><HR><BR>"
				  "%s\n"),
				  info->sn,
				  info->warnlevel,
				  asctime(localtime(&info->onlinesince)),
				  info->idletime,
				  prof);
	g_show_info_text(buf);

	return 1;
}

int gaim_parse_motd(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	char *msg;
	u_short id;
	va_list ap;

	va_start(ap, command);
	id  = (u_short)va_arg(ap, u_int);
	msg = va_arg(ap, char *);
	va_end(ap);

	sprintf(debug_buff, "MOTD: %s\n", msg);
	debug_print(debug_buff);
	sprintf(debug_buff, "Gaim %s / Libfaim %s\n",
			VERSION, aim_getbuildstring());
	debug_print(debug_buff);

	return 1;
}

int gaim_chatnav_info(struct aim_session_t *sess,
		      struct command_rx_struct *command, ...) {
	va_list ap;
	u_short type;

	va_start(ap, command);
	type = (u_short)va_arg(ap, u_int);

	switch(type) {
		case 0x0002: {
			int maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i = 0;

			maxrooms = (u_char)va_arg(ap, u_int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);
			va_end(ap);

			debug_print("chat info: Chat Rights:\n");
			sprintf(debug_buff, "chat info: \tMax Concurrent Rooms: %d\n", maxrooms);
			debug_print(debug_buff);
			sprintf(debug_buff, "chat info: \tExchange List: (%d total)\n", exchangecount);
			debug_print(debug_buff);
			while (i < exchangecount) {
				sprintf(debug_buff, "chat info: \t\t%x: %s (%s/%s)\n",
						exchanges[i].number,
						exchanges[i].name,
						exchanges[i].charset1,
						exchanges[i].lang1);
				debug_print(debug_buff);
				i++;
			}
			}
			aim_conn_kill(sess, &command->conn);
			break;
		default:
			va_end(ap);
			sprintf(debug_buff, "chatnav info: unknown type (%04x)\n", type);
			debug_print(debug_buff);
			break;
	}
	return 1;
}

int gaim_chat_join(struct aim_session_t *sess,
		   struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;

	GList *bcs = buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;	
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 1;
		
	while (i < count)
		add_chat_buddy(b, info[i++].sn);

	return 1;
}

int gaim_chat_leave(struct aim_session_t *sess,
		    struct command_rx_struct *command, ...) {
	va_list ap;
	int count, i = 0;
	struct aim_userinfo_s *info;

	GList *bcs = buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	count = va_arg(ap, int);
	info  = va_arg(ap, struct aim_userinfo_s *);
	va_end(ap);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;	
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 1;
		
	while (i < count)
		remove_chat_buddy(b, info[i++].sn);

	return 1;
}

int gaim_chat_info_update(struct aim_session_t *sess,
			  struct command_rx_struct *command, ...) {
	debug_print("inside chat_info_update\n");
	return 1;
}

int gaim_chat_incoming_msg(struct aim_session_t *sess,
			   struct command_rx_struct *command, ...) {
	va_list ap;
	struct aim_userinfo_s *info;
	char *msg;

	GList *bcs = buddy_chats;
	struct conversation *b = NULL;

	va_start(ap, command);
	info = va_arg(ap, struct aim_userinfo_s *);
	msg  = va_arg(ap, char *);

	while(bcs) {
		b = (struct conversation *)bcs->data;
		if (!strcasecmp(b->name, (char *)command->conn->priv))
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return 0;

	serv_got_chat_in(b->id, info->sn, 0, msg);

	return 1;
}

 /*
  * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
  */
int gaim_parse_msgack(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned short type;
	char *sn = NULL;

	ap = va_start(ap, command);
	type = va_arg(ap, unsigned short);
	sn = va_arg(ap, char *);
	va_end(ap);

	sprintf(debug_buff, "Sent message to %s.\n", sn);
	debug_print(debug_buff);

	return 1;
}

int gaim_parse_ratechange(struct aim_session_t *sess, struct command_rx_struct *command, ...) {
	va_list ap;
	unsigned long newrate;

	va_start(ap, command); 
	newrate = va_arg(ap, unsigned long);
	va_end(ap);

	sprintf(debug_buff, "ratechange: %lu\n", newrate);
	debug_print(debug_buff);

	return 1;
};
