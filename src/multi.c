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

#include <gtk/gtk.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#include "gnome_applet_mgr.h"
#include "aim.h"

#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/gnome_close.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"

GSList *connections;

static GtkWidget *acctedit = NULL;
static GtkWidget *list = NULL; /* the clist of names in the accteditor */
static GtkWidget *newmod = NULL; /* the dialog for creating a new account */
static struct aim_user tmpusr;

struct mod_usr_opt {
	struct aim_user *user;
	int opt;
};

struct gaim_connection *new_gaim_conn(int proto, char *username, char *password)
{
	struct gaim_connection *gc = g_new0(struct gaim_connection, 1);
	gc->protocol = proto;
	gc->prpl = find_prpl(proto);
	g_snprintf(gc->username, sizeof(gc->username), "%s", username);
	g_snprintf(gc->password, sizeof(gc->password), "%s", password);
	gc->keepalive = -1;
	gc->inpa = -1;
	gc->buddy_chats = NULL;

	connections = g_slist_append(connections, gc);

	return gc;
}

void destroy_gaim_conn(struct gaim_connection *gc)
{
	connections = g_slist_remove(connections, gc);
	g_free(gc);
	redo_convo_menus();
	if (!connections && mainwindow)
		gtk_widget_show(mainwindow);
}

struct gaim_connection *find_gaim_conn_by_name(char *name) {
	char *who = g_strdup(normalize(name));
	GSList *c = connections;
	struct gaim_connection *g = NULL;

	while (c) {
		g = (struct gaim_connection *)c->data;
		if (!strcmp(normalize(g->username), who)) {
			g_free(who);
			return g;
		}
		c = c->next;
	}

	g_free(who);
	return NULL;
}

static void delete_acctedit(GtkWidget *w, gpointer d)
{
	if (acctedit) {
		save_prefs();
		gtk_widget_destroy(acctedit);
	}
	acctedit = NULL;
	if (d == NULL && blist == NULL)
		exit(0);
}

static gint acctedit_close(GtkWidget *w, gpointer d)
{
	gtk_widget_destroy(acctedit);
	if (d == NULL && blist == NULL)
		exit(0);
	return FALSE;
}

static char *proto_name(int proto)
{
	struct prpl *p = find_prpl(proto);
	if (p && p->name)
		return (*p->name)();
	else
		return "Unknown";
}

static GtkWidget *generate_list()
{
	GtkWidget *win;
	char *titles[4] = {"Screenname", "Currently Online", "Auto-login", "Protocol"};
	GList *u = aim_users;
	struct aim_user *a;
	int i;

	win = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);

	list = gtk_clist_new_with_titles(4, titles);
	gtk_clist_set_column_width(GTK_CLIST(list), 0, 90);
	gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_passive(GTK_CLIST(list));
	gtk_container_add(GTK_CONTAINER(win), list);
	gtk_widget_show(list);

	while (u) {
		a = (struct aim_user *)u->data;
		titles[0] = a->username;
		titles[1] = find_gaim_conn_by_name(a->username) ? "Yes" : "No";
		titles[2] = (a->options & OPT_USR_AUTO) ? "True" : "False";
		titles[3] = proto_name(a->protocol);
		i = gtk_clist_append(GTK_CLIST(list), titles);
		gtk_clist_set_row_data(GTK_CLIST(list), i, a);
		u = u->next;
	}

	gtk_widget_show(win);
	return win;
}

static void delmod(GtkWidget *w, struct aim_user *u)
{
	gtk_widget_destroy(w);
	if (u) {
		u->mod = NULL;
	} else {
		newmod = NULL;
	}
}

static void mod_opt(GtkWidget *b, struct mod_usr_opt *m)
{
	if (m->user) {
		m->user->tmp_options = m->user->tmp_options ^ m->opt;
	} else {
		tmpusr.options = tmpusr.options ^ m->opt;
	}
}

static GtkWidget *acct_button(const char *text, struct aim_user *u, int option, GtkWidget *box)
{
	GtkWidget *button;
	struct mod_usr_opt *muo = g_new0(struct mod_usr_opt, 1);
	button = gtk_check_button_new_with_label(text);
	if (u) {
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (u->options & option));
	} else {
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (tmpusr.options & option));
	}
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	muo->user = u; muo->opt = option;
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_opt), muo);
	gtk_widget_show(button);
	return button;
}

static void ok_mod(GtkWidget *w, struct aim_user *u)
{
	char *txt;
	int i;
	if (u) {
		u->options = u->tmp_options;
		u->protocol = u->tmp_protocol;
		txt = gtk_entry_get_text(GTK_ENTRY(u->pass));
		if (u->options & OPT_USR_REM_PASS)
			g_snprintf(u->password, sizeof(u->password), "%s", txt);
		else
			u->password[0] = '\0';
		gtk_widget_destroy(u->mod);
		i = gtk_clist_find_row_from_data(GTK_CLIST(list), u);
		gtk_clist_set_text(GTK_CLIST(list), i, 2, (u->options & OPT_USR_AUTO) ? "True" : "False");
		gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(u->protocol));
	} else {
		char *titles[4];
		txt = gtk_entry_get_text(GTK_ENTRY(tmpusr.name));
		if (find_user(txt)) {
			/* PRPL: also need to check protocol. remember TOC and Oscar are both AIM */
			gtk_widget_destroy(newmod);
			return;
		}
		u = g_new0(struct aim_user, 1);
		g_snprintf(u->username, sizeof(u->username), "%s", txt);
		txt = gtk_entry_get_text(GTK_ENTRY(tmpusr.pass));
		g_snprintf(u->password, sizeof(u->password), "%s", txt);
		u->options = tmpusr.options;
		u->protocol = tmpusr.protocol;
		gtk_widget_destroy(newmod);
		titles[0] = u->username;
		titles[1] = find_gaim_conn_by_name(u->username) ? "Yes" : "No";
		titles[2] = (u->options & OPT_USR_AUTO) ? "True" : "False";
		titles[3] = proto_name(u->protocol);
		i = gtk_clist_append(GTK_CLIST(list), titles);
		gtk_clist_set_row_data(GTK_CLIST(list), i, u);
		aim_users = g_list_append(aim_users, u);
	}
	save_prefs();
}

static void cancel_mod(GtkWidget *w, struct aim_user *u)
{
	if (u) {
		gtk_widget_destroy(u->mod);
	} else {
		gtk_widget_destroy(newmod);
	}
}

static void set_prot(GtkWidget *opt, int proto)
{
	struct aim_user *u = gtk_object_get_user_data(GTK_OBJECT(opt));
	if (u) {
		u->tmp_protocol = proto;
	} else {
		tmpusr.protocol = proto;
	}
}

static GtkWidget *make_protocol_menu(GtkWidget *box, struct aim_user *u)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *p = protocols;
	struct prpl *e;

	/* PRPL: should we set some way to update these when new protocols get added? */
	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 5);
	gtk_widget_show(optmenu);

	menu = gtk_menu_new();

	while (p) {
		e = (struct prpl *)p->data;
		if (e->name)
			opt = gtk_menu_item_new_with_label((*e->name)());
		else
			opt = gtk_menu_item_new_with_label("Unknown");
		gtk_object_set_user_data(GTK_OBJECT(opt), u);
		gtk_signal_connect(GTK_OBJECT(opt), "activate",
				   GTK_SIGNAL_FUNC(set_prot), (void *)e->protocol);
		gtk_menu_append(GTK_MENU(menu), opt);
		gtk_widget_show(opt);
		p = p->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	if (u) {
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), u->protocol);
		u->tmp_protocol = u->protocol;
	} else {
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu),
				((struct prpl *)protocols->data)->protocol);
	}

	return optmenu;
}

static void show_acct_mod(struct aim_user *u)
{
	/* here we can have all the aim_user options, including ones not shown in the main acctedit
	 * window. this can keep the size of the acctedit window small and readable, and make this
	 * one the powerful editor. this is where things like name/password are edited, but can
	 * also have toggles (and even more complex options) like whether to autologin or whether
	 * to send keepalives or whatever. this would be the perfect place to specify which protocol
	 * to use. make sure to account for the possibility of protocol plugins. */
	GtkWidget *mod;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *name;
	GtkWidget *pass;
	GtkWidget *button;

	if (!u && newmod) {
		gtk_widget_show(newmod);
		return;
	}
	if (u && u->mod) {
		gtk_widget_show(u->mod);
		return;
	}

	mod = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(mod), "account", "Gaim");
	gtk_widget_realize(mod);
	aol_icon(mod->window);
	gtk_container_border_width(GTK_CONTAINER(mod), 10);
	gtk_window_set_title(GTK_WINDOW(mod), _("Gaim - Modify Account"));
	gtk_signal_connect(GTK_OBJECT(mod), "destroy",
			   GTK_SIGNAL_FUNC(delmod), u);

	frame = gtk_frame_new(_("Modify Account"));
	gtk_container_add(GTK_CONTAINER(mod), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Screenname:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	name = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name, FALSE, FALSE, 5);
	gtk_widget_show(name);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Password:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	pass = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), pass, FALSE, FALSE, 5);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);
	gtk_widget_show(pass);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	make_protocol_menu(hbox, u);

	acct_button(_("Remember Password"), u, OPT_USR_REM_PASS, vbox);
	acct_button(_("Auto-Login"), u, OPT_USR_AUTO, vbox);
	acct_button(_("Send KeepAlive packet (6 bytes/second)"), u, OPT_USR_KEEPALV, vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(mod, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cancel_mod), u);
	gtk_widget_show(button);

	button = picture_button(mod, _("OK"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(ok_mod), u);
	gtk_widget_show(button);

	if (u) {
		u->mod = mod;
		u->name = name;
		u->pass = pass;
		u->tmp_options = u->options;
		gtk_entry_set_text(GTK_ENTRY(name), u->username);
		gtk_entry_set_text(GTK_ENTRY(pass), u->password);
		gtk_entry_set_editable(GTK_ENTRY(name), FALSE);
	} else {
		newmod = mod;
		tmpusr.name = name;
		tmpusr.pass = pass;
	}

	gtk_widget_show(mod);
}

static void add_acct(GtkWidget *w, gpointer d)
{
	show_acct_mod(NULL);
}

static void mod_acct(GtkWidget *w, gpointer d)
{
	int row = -1;
	char *name;
	struct aim_user *u;
	if (GTK_CLIST(list)->selection)
		row = (int)GTK_CLIST(list)->selection->data;
	if (row != -1) {
		gtk_clist_get_text(GTK_CLIST(list), row, 0, &name);
		u = find_user(name);
		if (u)
			show_acct_mod(u);
	}
}

static void pass_des(GtkWidget *w, struct aim_user *u)
{
	gtk_widget_destroy(w);
	u->passprmt = NULL;
}

static void pass_cancel(GtkWidget *w, struct aim_user *u)
{
	gtk_widget_destroy(u->passprmt);
	u->passprmt = NULL;
}

static void pass_signon(GtkWidget *w, struct aim_user *u)
{
	char *txt = gtk_entry_get_text(GTK_ENTRY(u->passentry));
	g_snprintf(u->password, sizeof(u->password), "%s", txt);
#ifdef USE_APPLET
	set_user_state(signing_on);
#endif
	gtk_widget_destroy(u->passprmt);
	u->passprmt = NULL;
	serv_login(u);
}

static void do_pass_dlg(struct aim_user *u)
{
	/* we can safely assume that u is not NULL */
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	char buf[96];
	GtkWidget *label;
	GtkWidget *button;

	if (u->passprmt) { gtk_widget_show(u->passprmt); return; }
	u->passprmt = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(u->passprmt), "password", "Gaim");
	gtk_container_border_width(GTK_CONTAINER(u->passprmt), 5);
	gtk_signal_connect(GTK_OBJECT(u->passprmt), "destroy", GTK_SIGNAL_FUNC(pass_des), u);
	gtk_widget_realize(u->passprmt);
	aol_icon(u->passprmt->window);

	frame = gtk_frame_new(_("Enter Password"));
	gtk_container_add(GTK_CONTAINER(u->passprmt), frame);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	g_snprintf(buf, sizeof(buf), "Password for %s:", u->username);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	u->passentry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(u->passentry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), u->passentry, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(u->passentry), "activate", GTK_SIGNAL_FUNC(pass_signon), u);
	gtk_widget_grab_focus(u->passentry);
	gtk_widget_show(u->passentry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(u->passprmt, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pass_cancel), u);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	button = picture_button(u->passprmt, _("Signon"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pass_signon), u);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	gtk_widget_show(u->passprmt);
}

static void acct_signin(GtkWidget *w, gpointer d)
{
	int row = -1;
	char *name;
	struct aim_user *u;
	struct gaim_connection *gc;
	if (GTK_CLIST(list)->selection)
		row = (int)GTK_CLIST(list)->selection->data;
	if (row != -1) {
		gtk_clist_get_text(GTK_CLIST(list), row, 0, &name);
		u = find_user(name);
		gc = find_gaim_conn_by_name(name);
		if (!gc) {
			if (!u->password[0]) {
				do_pass_dlg(u);
			} else {
#ifdef USE_APPLET
				set_user_state(signing_on);
#endif /* USE_APPLET */
				serv_login(u);
			}
		} else {
			signoff(gc);
		}
	}
}
		
static void del_acct(GtkWidget *w, gpointer d)
{
	int row = -1;
	char *name;
	struct aim_user *u;
	if (GTK_CLIST(list)->selection)
		row = (int)GTK_CLIST(list)->selection->data;
	if (row != -1) {
		gtk_clist_get_text(GTK_CLIST(list), row, 0, &name);
		u = find_user(name);
		if (u) {
			aim_users = g_list_remove(aim_users, u);
			save_prefs();
		}
		gtk_clist_remove(GTK_CLIST(list), row);
	}
}

void account_editor(GtkWidget *w, GtkWidget *W)
{
	/* please kill me */
	GtkWidget *frame;
	GtkWidget *box;
	GtkWidget *list;
	GtkWidget *hbox;
	GtkWidget *button; /* used for many things */

	if (acctedit) { gtk_widget_show(acctedit); return; }

	acctedit = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(acctedit), _("Gaim - Account Editor"));
	gtk_window_set_wmclass(GTK_WINDOW(acctedit), "accounteditor", "Gaim");
	gtk_widget_realize(acctedit);
	aol_icon(acctedit->window);
	gtk_container_border_width(GTK_CONTAINER(acctedit), 10);
	gtk_widget_set_usize(acctedit, -1, 200);
	gtk_signal_connect(GTK_OBJECT(acctedit), "destroy",
			   GTK_SIGNAL_FUNC(delete_acctedit), W);

	frame = gtk_frame_new(_("Account Editor"));
	gtk_container_add(GTK_CONTAINER(acctedit), frame);
	gtk_widget_show(frame);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_widget_show(box);

	list = generate_list();
	gtk_box_pack_start(GTK_BOX(box), list, TRUE, TRUE, 5);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(acctedit, _("Add"), gnome_add_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(add_acct), NULL);

	button = picture_button(acctedit, _("Modify"), gnome_preferences_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_acct), NULL);

	button = picture_button(acctedit, _("Sign On/Off"), join_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(acct_signin), NULL);

	button = picture_button(acctedit, _("Delete"), gnome_remove_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(del_acct), NULL);

	button = picture_button(acctedit, _("Close"), gnome_close_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(acctedit_close), W);

	gtk_widget_show(acctedit);
}

void account_online(struct gaim_connection *gc)
{
	struct aim_user *u;
	int i;

	/* first we hide the login progress meter */
	if (gc->meter)
		gtk_widget_destroy(gc->meter);
	gc->meter = NULL;
	
	/* then we do the buddy list stuff */
	if (mainwindow)
		gtk_widget_hide(mainwindow);
	show_buddy_list();
	
#ifdef USE_APPLET
	if (general_options & OPT_GEN_APP_BUDDY_SHOW) {
		refresh_buddy_window();
		createOnlinePopup();
		applet_buddy_show = TRUE;
	} else {
		gtk_widget_hide(blist);
		applet_buddy_show = FALSE;
	}
	set_user_state(online);
#else
	refresh_buddy_window();
#endif
	setup_buddy_chats();

	redo_convo_menus();
	gaim_setup(gc);

	plugin_event(event_signon, gc, 0, 0, 0);

	/* everything for the account editor */
	if (!acctedit) return;
	u = find_user(gc->username);
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), u);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "Yes");
	gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(gc->protocol));

	return;
}

void account_offline(struct gaim_connection *gc)
{
	struct aim_user *u;
	int i;
	if (!acctedit) return;
	u = find_user(gc->username);
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), u);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "No");
	redo_convo_menus();
}

void auto_login()
{
	GList *u = aim_users;
	struct aim_user *a = NULL;

	while (u) {
		a = (struct aim_user *)u->data;
		if ((a->options & OPT_USR_AUTO) && (a->options & OPT_USR_REM_PASS)) {
#ifdef USE_APPLET
			set_user_state(signing_on);
#endif /* USE_APPLET */
			serv_login(a);
		}
		u = u->next;
	}
}

static void meter_destroy(GtkWidget *meter, struct gaim_connection *gc) {
	gtk_widget_destroy(meter);
	gc->meter = NULL;
}

void set_login_progress(struct gaim_connection *gc, float howfar, char *message) {
	if (mainwindow)
		gtk_widget_hide(mainwindow);

	if (!gc->meter) {
		GtkWidget *box, *label;
		char buf[256];

		gc->meter = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_policy(GTK_WINDOW(gc->meter), 0, 0, 1);
		gtk_window_set_wmclass(GTK_WINDOW(gc->meter), "signon", "Gaim");
		gtk_container_set_border_width(GTK_CONTAINER(gc->meter), 5);
		g_snprintf(buf, sizeof(buf), "%s Signing On", gc->username);
		gtk_window_set_title(GTK_WINDOW(gc->meter), buf);
		gtk_signal_connect(GTK_OBJECT(gc->meter), "destroy",
				   GTK_SIGNAL_FUNC(meter_destroy), gc);
		gtk_widget_realize(gc->meter);
		aol_icon(gc->meter->window);

		box = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(gc->meter), box);
		gtk_widget_show(box);

		label = gtk_label_new(buf);
		gtk_box_pack_start(GTK_BOX(box), label, 0, 0, 5);
		gtk_widget_show(label);

		gc->progress = gtk_progress_bar_new();
		gtk_widget_set_usize(gc->progress, 150, 0);
		gtk_box_pack_start(GTK_BOX(box), gc->progress, 0, 0, 5);
		gtk_widget_show(gc->progress);

		gc->status = gtk_statusbar_new();
		gtk_widget_set_usize(gc->status, 150, 0);
		gtk_box_pack_start(GTK_BOX(box), gc->status, 0, 0, 5);
		gtk_widget_show(gc->status);

		gtk_widget_show(gc->meter);
	}

	gtk_progress_bar_update(GTK_PROGRESS_BAR(gc->progress), howfar / 5);
	gtk_statusbar_pop(GTK_STATUSBAR(gc->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(gc->status), 1, message);
}

void hide_login_progress(struct gaim_connection *gc, char *why)
{
	char buf[2048];
	sprintf(buf, _("%s was unable to sign on: %s"), gc->username, why);
	do_error_dialog(buf, _("Signon Error"));
	if (gc->meter)
		gtk_widget_destroy(gc->meter);
	gc->meter = NULL;
}
