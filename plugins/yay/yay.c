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
#include "config.h"
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
#include "yay.h"

#include "pixmaps/status-away.xpm"
#include "pixmaps/status-here.xpm"
#include "pixmaps/status-idle.xpm"

#include "pixmaps/cancel.xpm"

#define USEROPT_HTTPHOST 0
#define USEROPT_HTTPPORT 1

struct conn {
	int socket;
	int type;
	int inpa;
};

struct yahoo_data {
	struct yahoo_session *sess;
	int current_status;
	GHashTable *hash;
	GtkWidget *email_win;
	GtkWidget *email_label;
	char *active_id;
	GList *conns;
	gboolean logged_in;
};

static char *yahoo_name() {
	return "Yahoo";
}

char *name() {
	return "Yahoo";
}

char *description() {
	return "Allows gaim to use the Yahoo protocol";
}

static int yahoo_status(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	time_t tmptime;
	struct buddy *b;
	gboolean online;

	va_list ap;
	char *who;
	int status;
	char *msg;
	int in_pager, in_chat, in_game;

	va_start(ap, sess);
	who = va_arg(ap, char *);
	status = va_arg(ap, int);
	msg = va_arg(ap, char *);
	in_pager = va_arg(ap, int);
	in_chat = va_arg(ap, int);
	in_game = va_arg(ap, int);
	va_end(ap);

	online = in_pager || in_chat || in_game;

	b = find_buddy(gc, who);
	if (!b) return 0;
	if (!online)
		serv_got_update(gc, b->name, 0, 0, 0, 0, 0, 0);
	else {
		if (status == YAHOO_STATUS_AVAILABLE)
			serv_got_update(gc, b->name, 1, 0, 0, 0, UC_NORMAL, 0);
		else if (status == YAHOO_STATUS_IDLE) {
			time(&tmptime);
			serv_got_update(gc, b->name, 1, 0, 0, tmptime - 600,
					(status << 5) | UC_NORMAL, 0);
		} else
			serv_got_update(gc, b->name, 1, 0, 0, 0,
					(status << 5) | UC_UNAVAILABLE, 0);
		if (status == YAHOO_STATUS_CUSTOM) {
			gpointer val = g_hash_table_lookup(yd->hash, b->name);
			if (val)
				g_free(val);
			g_hash_table_insert(yd->hash, g_strdup(b->name), g_strdup(msg));
		}
	}

	return 1;
}

static int yahoo_message(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	char buf[BUF_LEN * 4];
	char *tmp, *c, *e;
	time_t tm;
	int at = 0;

	va_list ap;
	char *id, *nick, *msg;

	va_start(ap, sess);
	id = va_arg(ap, char *);
	nick = va_arg(ap, char *);
	tm = va_arg(ap, time_t);
	msg = va_arg(ap, char *);
	va_end(ap);

	if (msg)
		e = tmp = g_strdup(msg);
	else
		return 1;

	while ((c = strchr(e, '\033')) != NULL) {
		*c++ = '\0';
		at += g_snprintf(buf + at, sizeof(buf) - at, "%s", e);
		e = ++c;
		while (*e && (*e++ != 'm'));
	}

	if (*e)
		g_snprintf(buf + at, sizeof(buf) - at, "%s", e);

	g_free(tmp);

	serv_got_im(gc, nick, buf, 0, tm ? tm : time((time_t)NULL));

	return 1;
}

static int yahoo_bounce(struct yahoo_session *sess, ...) {
	do_error_dialog(_("Your message did not get sent."), _("Gaim - Error"));
	
	return 1;
}

static int yahoo_buddyadded(struct yahoo_session *sess, ...) {
	va_list ap;
	char *id;
	char *who;
	char *msg;
	char buf[2048];

	va_start(ap, sess);
	id = va_arg(ap, char *);
	who = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	g_snprintf(buf, sizeof(buf), _("%s has made %s their buddy%s%s"), who, id,
			msg ? ": " : "", msg ? msg : "");
	do_error_dialog(buf, _("Gaim - Buddy"));

	return 1;
}

static void des_win(GtkWidget *w, struct yahoo_data *yd) {
	gtk_widget_destroy(yd->email_win);
	if (yd->email_win == w)
		yd->email_win = NULL;
	yd->email_label = NULL;
}

static int yahoo_newmail(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	char buf[2048];

	va_list ap;
	int count;

	va_start(ap, sess);
	count = va_arg(ap, int);
	va_end(ap);

	if (count) {
		g_snprintf(buf, sizeof buf, "%s has %d new message%s on Yahoo Mail.",
				gc->username, count, count == 1 ? "" : "s");
		if (!yd->email_win) {
			GtkWidget *close;

			yd->email_win = gtk_dialog_new();
			gtk_window_set_policy(GTK_WINDOW(yd->email_win), 0, 0, 1);
			gtk_container_set_border_width(GTK_CONTAINER(yd->email_win), 5);
			gtk_window_set_title(GTK_WINDOW(yd->email_win), "New Mail");
			gtk_signal_connect(GTK_OBJECT(yd->email_win), "destroy",
					   GTK_SIGNAL_FUNC(des_win), yd);
			gtk_widget_realize(yd->email_win);
			aol_icon(yd->email_win->window);

			yd->email_label = gtk_label_new(buf);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(yd->email_win)->vbox),
				yd->email_label, 0, 0, 5);
			gtk_widget_show(yd->email_label);

			close = picture_button(yd->email_win, _("Close"), cancel_xpm);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(yd->email_win)->action_area),
					close, 0, 0, 5);
			gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(des_win), yd);

			gtk_widget_show(yd->email_win);
		}
		gtk_label_set_text(GTK_LABEL(yd->email_label), buf);
	} else if (yd->email_win)
		gtk_widget_destroy(yd->email_win);

	return 1;
}

static int yahoo_disconn(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	hide_login_progress(gc, "Disconnected");
	signoff(gc);
	return 1;
}

static int yahoo_authconnect(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;

	set_login_progress(gc, 2, "Connected to Auth");
	if (yahoo_send_login(sess, gc->username, gc->password) < 1) {
		hide_login_progress(gc, "Authorizer error");
		signoff(gc);
	}

	return 1;
}

static int yahoo_badpassword(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	hide_login_progress(gc, "Bad Password");
	signoff(gc);
	return 1;
}

static int yahoo_logincookie(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;

	set_login_progress(gc, 3, "Got login cookie");
	if (yahoo_major_connect(sess, NULL, 0) < 1) {
		hide_login_progress(gc, "Login error");
		signoff(gc);
	}

	return 1;
}

static int yahoo_mainconnect(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	struct yahoo_data *yd = gc->proto_data;
	GList *grps;

	set_login_progress(gc, 4, "Connected to service");
	if (yahoo_finish_logon(sess, YAHOO_STATUS_AVAILABLE) < 1) {
		hide_login_progress(gc, "Login error");
		signoff(gc);
	}

	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);

	grps = yd->sess->groups;
	while (grps) {
		struct yahoo_group *grp = grps->data;
		int i;
		
		for (i = 0; grp->buddies[i]; i++)
			add_buddy(gc, grp->name, grp->buddies[i], NULL);

		grps = grps->next;
	}

	return 1;
}

static int yahoo_online(struct yahoo_session *sess, ...) {
	struct gaim_connection *gc = sess->user_data;
	struct yahoo_data *yd = gc->proto_data;

	account_online(gc);
	serv_finish_login(gc);
	yd->active_id = g_strdup(gc->username);
	yd->logged_in = TRUE;

	return 1;
}

static void yahoo_pending(gpointer data, gint source, GdkInputCondition condition) {
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	yahoo_socket_handler(yd->sess, source, condition);
}

static void yahoo_notify(struct yahoo_session *sess, int socket, int type, int cont) {
	struct gaim_connection *gc = sess->user_data;
	struct yahoo_data *yd = gc->proto_data;

	if (cont) {
		struct conn *c = g_new0(struct conn, 1);
		c->socket = socket;
		c->type = type;
		c->inpa = gdk_input_add(socket, type, yahoo_pending, gc);
		yd->conns = g_list_append(yd->conns, c);
	} else {
		GList *c = yd->conns;
		while (c) {
			struct conn *m = c->data;
			if ((m->socket == socket) && (m->type == type)) {
				yd->conns = g_list_remove(yd->conns, m);
				gdk_input_remove(m->inpa);
				g_free(m);
				return;
			}
			c = g_list_next(c);
		}
	}
}

static void yahoo_debug(struct yahoo_session *sess, int level, const char *string) {
	debug_printf("Level %d: %s\n", level, string);
}

static void yahoo_login(struct aim_user *user) {
	struct gaim_connection *gc = new_gaim_conn(user);
	struct yahoo_data *yd = gc->proto_data = g_new0(struct yahoo_data, 1);

	yd->sess = yahoo_new();
	yd->sess->user_data = gc;
	yd->current_status = YAHOO_STATUS_AVAILABLE;
	yd->hash = g_hash_table_new(g_str_hash, g_str_equal);

	if (user->proto_opt[USEROPT_HTTPHOST][0]) {
		if (user->proto_opt[USEROPT_HTTPPORT][0])
			yahoo_set_proxy(yd->sess, YAHOO_PROXY_HTTP,
					user->proto_opt[USEROPT_HTTPHOST],
					atoi(user->proto_opt[USEROPT_HTTPPORT]));
		else
			yahoo_set_proxy(yd->sess, YAHOO_PROXY_HTTP,
					user->proto_opt[USEROPT_HTTPHOST], 8080);
	}

	set_login_progress(gc, 1, "Connecting");

	if (!yahoo_connect(yd->sess, NULL, 0)) {
		yahoo_delete(yd->sess);
		hide_login_progress(gc, "Connection problem");
		signoff(gc);
		return;
	}

	yahoo_add_handler(yd->sess, YAHOO_HANDLE_DISCONNECT, yahoo_disconn);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_AUTHCONNECT, yahoo_authconnect);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_BADPASSWORD, yahoo_badpassword);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_LOGINCOOKIE, yahoo_logincookie);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_MAINCONNECT, yahoo_mainconnect);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_ONLINE, yahoo_online);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_NEWMAIL, yahoo_newmail);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_MESSAGE, yahoo_message);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_BOUNCE, yahoo_bounce);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_STATUS, yahoo_status);
	yahoo_add_handler(yd->sess, YAHOO_HANDLE_BUDDYADDED, yahoo_buddyadded);
}

static gboolean yahoo_destroy_hash(gpointer key, gpointer val, gpointer data) {
	g_free(key);
	g_free(val);
	return TRUE;
}

static void yahoo_close(struct gaim_connection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	g_hash_table_foreach_remove(yd->hash, yahoo_destroy_hash, NULL);
	g_hash_table_destroy(yd->hash);
	yahoo_disconnect(yd->sess);
	yahoo_delete(yd->sess);
	g_free(yd);
}

static void yahoo_send_im(struct gaim_connection *gc, char *who, char *message, int away) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	if (away || !strlen(message)) return;

	yahoo_send_message(yd->sess, yd->active_id, who, message);
}

static void yahoo_set_away(struct gaim_connection *gc, char *state, char *msg) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	gc->away = NULL;

	if (msg) {
		yahoo_away(yd->sess, YAHOO_STATUS_CUSTOM, msg);
		yd->current_status = YAHOO_STATUS_CUSTOM;
		gc->away = "";
	} else if (state) {
		gc->away = "";
		if (!strcmp(state, "Available")) {
			yahoo_away(yd->sess, YAHOO_STATUS_AVAILABLE, msg);
			yd->current_status = YAHOO_STATUS_AVAILABLE;
		} else if (!strcmp(state, "Be Right Back")) {
			yahoo_away(yd->sess, YAHOO_STATUS_BRB, msg);
			yd->current_status = YAHOO_STATUS_BRB;
		} else if (!strcmp(state, "Busy")) {
			yahoo_away(yd->sess, YAHOO_STATUS_BUSY, msg);
			yd->current_status = YAHOO_STATUS_BUSY;
		} else if (!strcmp(state, "Not At Home")) {
			yahoo_away(yd->sess, YAHOO_STATUS_NOTATHOME, msg);
			yd->current_status = YAHOO_STATUS_NOTATHOME;
		} else if (!strcmp(state, "Not At Desk")) {
			yahoo_away(yd->sess, YAHOO_STATUS_NOTATDESK, msg);
			yd->current_status = YAHOO_STATUS_NOTATDESK;
		} else if (!strcmp(state, "Not In Office")) {
			yahoo_away(yd->sess, YAHOO_STATUS_NOTINOFFICE, msg);
			yd->current_status = YAHOO_STATUS_NOTINOFFICE;
		} else if (!strcmp(state, "On Phone")) {
			yahoo_away(yd->sess, YAHOO_STATUS_ONPHONE, msg);
			yd->current_status = YAHOO_STATUS_ONPHONE;
		} else if (!strcmp(state, "On Vacation")) {
			yahoo_away(yd->sess, YAHOO_STATUS_ONVACATION, msg);
			yd->current_status = YAHOO_STATUS_ONVACATION;
		} else if (!strcmp(state, "Out To Lunch")) {
			yahoo_away(yd->sess, YAHOO_STATUS_OUTTOLUNCH, msg);
			yd->current_status = YAHOO_STATUS_OUTTOLUNCH;
		} else if (!strcmp(state, "Stepped Out")) {
			yahoo_away(yd->sess, YAHOO_STATUS_STEPPEDOUT, msg);
			yd->current_status = YAHOO_STATUS_STEPPEDOUT;
		} else if (!strcmp(state, "Invisible")) {
			yahoo_away(yd->sess, YAHOO_STATUS_INVISIBLE, msg);
			yd->current_status = YAHOO_STATUS_INVISIBLE;
		} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
			if (gc->is_idle) {
				yahoo_away(yd->sess, YAHOO_STATUS_IDLE, NULL);
				yd->current_status = YAHOO_STATUS_IDLE;
			} else {
				yahoo_away(yd->sess, YAHOO_STATUS_AVAILABLE, NULL);
				yd->current_status = YAHOO_STATUS_AVAILABLE;
			}
			gc->away = NULL;
		}
	} else if (gc->is_idle) {
		yahoo_away(yd->sess, YAHOO_STATUS_IDLE, NULL);
		yd->current_status = YAHOO_STATUS_IDLE;
	} else {
		yahoo_away(yd->sess, YAHOO_STATUS_AVAILABLE, NULL);
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	}
}

static void yahoo_set_idle(struct gaim_connection *gc, int idle) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	if (idle && yd->current_status == YAHOO_STATUS_AVAILABLE) {
		yahoo_away(yd->sess, YAHOO_STATUS_IDLE, NULL);
		yd->current_status = YAHOO_STATUS_IDLE;
	} else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
		yahoo_back(yd->sess, YAHOO_STATUS_AVAILABLE, NULL);
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	}
}

static void yahoo_keepalive(struct gaim_connection *gc) {
	yahoo_ping(((struct yahoo_data *)gc->proto_data)->sess);
}

static void gyahoo_add_buddy(struct gaim_connection *gc, char *name) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_group *tmpgroup;
	struct group *g = find_group_by_buddy(gc, name);
	char *group = NULL;

	if (!yd->logged_in)
		return;

	if (g) {
		group = g->name;
	} else if (yd->sess && yd->sess->groups) {
		tmpgroup = yd->sess->groups->data;
		group = tmpgroup->name;
	} else {
		group = "Buddies";
	}

	if (group)
		yahoo_add_buddy(yd->sess, yd->active_id, group, name, "");
}

static void yahoo_add_buddies(struct gaim_connection *gc, GList *buddies) {
	while (buddies) {
		gyahoo_add_buddy(gc, buddies->data);
		buddies = buddies->next;
	}
}

static void gyahoo_remove_buddy(struct gaim_connection *gc, char *name) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct group *g = find_group_by_buddy(gc, name);
	char *group = NULL;

	if (g) {
		group = g->name;
	} else if (yd->sess && yd->sess->groups) {
		GList *x = yd->sess->groups;
		while (x) {
			struct yahoo_group *tmpgroup = x->data;
			char **bds = tmpgroup->buddies;
			while (*bds) {
				if (!strcmp(*bds, name))
					break;
				bds++;
			}
			if (*bds) {
				group = tmpgroup->name;
				break;
			}
			x = x->next;
		}
	} else {
		group = "Buddies";
	}

	if (group)
		yahoo_remove_buddy(yd->sess, yd->active_id, group, name, "");
}

static char **yahoo_list_icon(int uc) {
	if ((uc >> 5) == YAHOO_STATUS_IDLE)
		return status_idle_xpm;
	else if (uc == UC_NORMAL)
		return status_here_xpm;
	return status_away_xpm;
}

static char *yahoo_get_status_string(enum yahoo_status a) {
	switch (a) {
	case YAHOO_STATUS_BRB:
		return "Be Right Back";
	case YAHOO_STATUS_BUSY:
		return "Busy";
	case YAHOO_STATUS_NOTATHOME:
		return "Not At Home";
	case YAHOO_STATUS_NOTATDESK:
		return "Not At Desk";
	case YAHOO_STATUS_NOTINOFFICE:
		return "Not In Office";
	case YAHOO_STATUS_ONPHONE:
		return "On Phone";
	case YAHOO_STATUS_ONVACATION:
		return "On Vacation";
	case YAHOO_STATUS_OUTTOLUNCH:
		return "Out To Lunch";
	case YAHOO_STATUS_STEPPEDOUT:
		return "Stepped Out";
	default:
		return NULL;
	}
}

static void yahoo_buddy_menu(GtkWidget *menu, struct gaim_connection *gc, char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	GtkWidget *button;
	struct buddy *b = find_buddy(gc, who); /* this should never be null. if it is,
						  segfault and get the bug report. */
	char buf[1024];

	if (b->uc & UC_NORMAL)
		return;

	if ((b->uc >> 5) != YAHOO_STATUS_CUSTOM)
		g_snprintf(buf, sizeof buf, "Status: %s", yahoo_get_status_string(b->uc >> 5));
	else
		g_snprintf(buf, sizeof buf, "Custom Status: %s",
			   (char *)g_hash_table_lookup(yd->hash, b->name));
	button = gtk_menu_item_new_with_label(buf);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static GList *yahoo_away_states() {
	GList *m = NULL;

	m = g_list_append(m, "Available");
	m = g_list_append(m, "Be Right Back");
	m = g_list_append(m, "Busy");
	m = g_list_append(m, "Not At Home");
	m = g_list_append(m, "Not At Desk");
	m = g_list_append(m, "Not In Office");
	m = g_list_append(m, "On Phone");
	m = g_list_append(m, "On Vacation");
	m = g_list_append(m, "Out To Lunch");
	m = g_list_append(m, "Stepped Out");
	m = g_list_append(m, "Invisible");
	m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void yahoo_act_id(gpointer data, char *entry) {
	struct gaim_connection *gc = data;
	struct yahoo_data *yd = gc->proto_data;

	yahoo_activate_id(yd->sess, entry);
	if (yd->active_id)
		g_free(yd->active_id);
	yd->active_id = g_strdup(entry);
}

static void yahoo_do_action(struct gaim_connection *gc, char *act) {
	if (!strcmp(act, "Activate ID")) {
		do_prompt_dialog("Activate which ID:", gc, yahoo_act_id, NULL);
	}
}

static GList *yahoo_actions() {
	GList *m = NULL;

	m = g_list_append(m, "Activate ID");

	return m;
}

static void yahoo_print_option(GtkEntry *entry, struct aim_user *user) {
	int entrynum;

	entrynum = (int) gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_HTTPHOST) {
		g_snprintf(user->proto_opt[USEROPT_HTTPHOST],
				sizeof(user->proto_opt[USEROPT_HTTPHOST]),
				"%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_HTTPPORT) {
		g_snprintf(user->proto_opt[USEROPT_HTTPPORT],
				sizeof(user->proto_opt[USEROPT_HTTPPORT]),
				"%s", gtk_entry_get_text(entry));
	}
}

static void yahoo_user_opts(GtkWidget *book, struct aim_user *user) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
			gtk_label_new("Yahoo Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("HTTP Proxy Host:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_HTTPHOST);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(yahoo_print_option), user);
	if (user->proto_opt[USEROPT_HTTPHOST][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_HTTPHOST]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_HTTPHOST]);
	}
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("HTTP Proxy Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_HTTPPORT);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(yahoo_print_option), user);
	if (user->proto_opt[USEROPT_HTTPPORT][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_HTTPPORT]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_HTTPPORT]);
	}
	gtk_widget_show(entry);
}

static struct prpl *my_protocol = NULL;

void Yahoo_init(struct prpl *ret) {
	/* the NULL's aren't required but they're nice to have */
	ret->protocol = PROTO_YAHOO;
	ret->name = yahoo_name;
	ret->list_icon = yahoo_list_icon;
	ret->away_states = yahoo_away_states;
	ret->actions = yahoo_actions;
	ret->do_action = yahoo_do_action;
	ret->buddy_menu = yahoo_buddy_menu;
	ret->user_opts = yahoo_user_opts;
	ret->login = yahoo_login;
	ret->close = yahoo_close;
	ret->send_im = yahoo_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = yahoo_set_away;
	ret->get_away_msg = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = yahoo_set_idle;
	ret->change_passwd = NULL;
	ret->add_buddy = gyahoo_add_buddy;
	ret->add_buddies = yahoo_add_buddies;
	ret->remove_buddy = gyahoo_remove_buddy;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_permit = NULL;
	ret->rem_deny = NULL;
	ret->set_permit_deny = NULL;
	ret->warn = NULL;
	ret->accept_chat = NULL;
	ret->join_chat = NULL;
	ret->chat_invite = NULL;
	ret->chat_leave = NULL;
	ret->chat_whisper = NULL;
	ret->chat_send = NULL;
	ret->keepalive = yahoo_keepalive;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule *handle) {
	load_protocol(Yahoo_init, sizeof(struct prpl));
	yahoo_socket_notify = yahoo_notify;
	yahoo_print = yahoo_debug;
	return NULL;
}

void gaim_plugin_remove() {
	struct prpl *p = find_prpl(PROTO_YAHOO);
	if (p == my_protocol)
		unload_protocol(p);
}
