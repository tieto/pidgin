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

#include <string.h>
#include <gtk/gtk.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#ifdef USE_APPLET
#include "applet.h"
#endif

#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/tb_redo.xpm"
#include "pixmaps/tb_undo.xpm"
#include "pixmaps/tb_refresh.xpm"

#define LOGIN_STEPS 5

GSList *connections;

static GtkWidget *acctedit = NULL;
static GtkWidget *list = NULL;	/* the clist of names in the accteditor */
static GtkWidget *newmod = NULL;	/* the dialog for creating a new account */
static GtkWidget *newmain = NULL;	/* the notebook that holds options */
static struct aim_user tmpusr = { "", "", "", OPT_USR_REM_PASS, DEFAULT_PROTO,
		{ "", "", "", "", "", "", "" }, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, OPT_USR_REM_PASS, DEFAULT_PROTO, NULL, NULL, NULL };

static void generate_prpl_options(struct aim_user *, GtkWidget *);

struct mod_usr_opt {
	struct aim_user *user;
	int opt;
};

struct gaim_connection *new_gaim_conn(struct aim_user *user)
{
	struct gaim_connection *gc = g_new0(struct gaim_connection, 1);
	gc->edittype = EDIT_GC;
	gc->protocol = user->protocol;
	gc->prpl = find_prpl(user->protocol);
	g_snprintf(gc->username, sizeof(gc->username), "%s", user->username);
	g_snprintf(gc->password, sizeof(gc->password), "%s", user->password);
	gc->options = user->options;
	gc->keepalive = 0;
	gc->inpa = -1;
	gc->buddy_chats = NULL;
	gc->groups = NULL;
	gc->permit = NULL;
	gc->deny = NULL;

	connections = g_slist_append(connections, gc);

	user->gc = gc;
	gc->user = user;

	return gc;
}

void destroy_gaim_conn(struct gaim_connection *gc)
{
	GSList *g = gc->groups;
	GSList *h;
	struct group *m;
	struct buddy *n;
	connections = g_slist_remove(connections, gc);
	while (g) {
		m = (struct group *)g->data;
		g = g_slist_remove(g, m);
		h = m->members;
		while (h) {
			n = (struct buddy *)h->data;
			h = g_slist_remove(h, n);
			g_free(n);
		}
		g_free(m);
	}
	g_free(gc);
#ifndef USE_APPLET
	if (!connections && mainwindow)
		gtk_widget_show(mainwindow);
#endif
}

struct gaim_connection *find_gaim_conn_by_name(char *name)
{
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
	list = NULL;
	if (!d && !blist && !mainwindow && !connections)
		gtk_main_quit();
}

static gint acctedit_close(GtkWidget *w, gpointer d)
{
	gtk_widget_destroy(acctedit);
	if (!d && !blist && !mainwindow && !connections)
		gtk_main_quit();
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

void regenerate_user_list()
{
	char *titles[4];
	GList *u = aim_users;
	struct aim_user *a;
	int i;

	if (!acctedit)
		return;

	gtk_clist_clear(GTK_CLIST(list));

	while (u) {
		a = (struct aim_user *)u->data;
		titles[0] = a->username;
		titles[1] = a->gc ? "Yes" : "No";
		titles[2] = (a->options & OPT_USR_AUTO) ? "True" : "False";
		titles[3] = proto_name(a->protocol);
		i = gtk_clist_append(GTK_CLIST(list), titles);
		gtk_clist_set_row_data(GTK_CLIST(list), i, a);
		u = u->next;
	}
}

static GtkWidget *generate_list()
{
	GtkWidget *win;
	char *titles[4] = { "Screenname", "Currently Online", "Auto-login", "Protocol" };

	win = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win), GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);

	list = gtk_clist_new_with_titles(4, titles);
	gtk_clist_set_column_width(GTK_CLIST(list), 0, 90);
	gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_MULTIPLE);
	gtk_clist_column_titles_passive(GTK_CLIST(list));
	gtk_container_add(GTK_CONTAINER(win), list);
	gtk_widget_show(list);

	regenerate_user_list();

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

static void free_muo(GtkWidget *b, struct mod_usr_opt *m)
{
	g_free(m);
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
	muo->user = u;
	muo->opt = option;
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_opt), muo);
	gtk_signal_connect(GTK_OBJECT(button), "destroy", GTK_SIGNAL_FUNC(free_muo), muo);
	gtk_widget_show(button);
	return button;
}

static void ok_mod(GtkWidget *w, struct aim_user *u)
{
	GList *tmp;
	const char *txt;
	int i;

	if (u) {
		u->options = u->tmp_options;
		u->protocol = u->tmp_protocol;
		txt = gtk_entry_get_text(GTK_ENTRY(u->pass));
		if (u->options & OPT_USR_REM_PASS)
			g_snprintf(u->password, sizeof(u->password), "%s", txt);
		else
			u->password[0] = '\0';
		i = gtk_clist_find_row_from_data(GTK_CLIST(list), u);
		gtk_clist_set_text(GTK_CLIST(list), i, 2,
				   (u->options & OPT_USR_AUTO) ? "True" : "False");
		gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(u->protocol));

		tmp = u->opt_entries;
		while (tmp) {
			GtkEntry *entry = tmp->data;
			int pos = (int)gtk_object_get_user_data(GTK_OBJECT(entry));
			g_snprintf(u->proto_opt[pos], sizeof(u->proto_opt[pos]), "%s",
						gtk_entry_get_text(entry));
			tmp = tmp->next;
		}
		if (u->opt_entries)
			g_list_free(u->opt_entries);
		u->opt_entries = NULL;

		gtk_widget_destroy(u->mod);
	} else {
		txt = gtk_entry_get_text(GTK_ENTRY(tmpusr.name));
		u = new_user(txt, tmpusr.protocol, tmpusr.options);

		txt = gtk_entry_get_text(GTK_ENTRY(tmpusr.pass));
		g_snprintf(u->password, sizeof(u->password), "%s", txt);

		tmp = tmpusr.opt_entries;
		while (tmp) {
			GtkEntry *entry = tmp->data;
			int pos = (int)gtk_object_get_user_data(GTK_OBJECT(entry));
			g_snprintf(u->proto_opt[pos], sizeof(u->proto_opt[pos]), "%s",
						gtk_entry_get_text(entry));
			tmp = tmp->next;
		}
		if (tmpusr.opt_entries)
			g_list_free(tmpusr.opt_entries);
		tmpusr.opt_entries = NULL;

		gtk_widget_destroy(newmod);
	}
	save_prefs();
}

static void cancel_mod(GtkWidget *w, struct aim_user *u)
{
	if (u) {
		if (u->opt_entries)
			g_list_free(u->opt_entries);
		u->opt_entries = NULL;
		gtk_widget_destroy(u->mod);
	} else {
		if (tmpusr.opt_entries)
			g_list_free(tmpusr.opt_entries);
		tmpusr.opt_entries = NULL;
		gtk_widget_destroy(newmod);
	}
}

static void set_prot(GtkWidget *opt, int proto)
{
	struct aim_user *u = gtk_object_get_user_data(GTK_OBJECT(opt));
	struct prpl *p, *q;
	q = find_prpl(proto);
	if (u && (u->tmp_protocol != proto)) {
		int i;
		for (i = 0; i < 7; i++)
			u->proto_opt[i][0] = '\0';
		p = find_prpl(u->tmp_protocol);
		if (!(p->options & OPT_PROTO_NO_PASSWORD) &&
		     (q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(u->pwdbox);
			gtk_widget_hide(u->rempass);
		} else if ((p->options & OPT_PROTO_NO_PASSWORD) &&
			  !(q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_show(u->pwdbox);
			gtk_widget_show(u->rempass);
		}
		if (!(p->options & OPT_PROTO_MAIL_CHECK) &&
		     (q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_show(u->checkmail);
		} else if ((p->options & OPT_PROTO_MAIL_CHECK) &&
			  !(q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_hide(u->checkmail);
		}
		u->tmp_protocol = proto;
		generate_prpl_options(u, u->main);
	} else if (!u && (tmpusr.tmp_protocol != proto)) {
		int i;
		for (i = 0; i < 7; i++)
			tmpusr.proto_opt[i][0] = '\0';
		p = find_prpl(tmpusr.tmp_protocol);
		if (!(p->options & OPT_PROTO_NO_PASSWORD) &&
		     (q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(tmpusr.pwdbox);
			gtk_widget_hide(tmpusr.rempass);
		} else if ((p->options & OPT_PROTO_NO_PASSWORD) &&
			  !(q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_show(tmpusr.pwdbox);
			gtk_widget_show(tmpusr.rempass);
		}
		if (!(p->options & OPT_PROTO_MAIL_CHECK) &&
		     (q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_show(tmpusr.checkmail);
		} else if ((p->options & OPT_PROTO_MAIL_CHECK) &&
			  !(q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_hide(tmpusr.checkmail);
		}
		tmpusr.tmp_protocol = tmpusr.protocol = proto;
		generate_prpl_options(NULL, newmain);
	}
}

static GtkWidget *make_protocol_menu(GtkWidget *box, struct aim_user *u)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *p = protocols;
	struct prpl *e;
	int count = 0;
	gboolean found = FALSE;

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 5);
	gtk_widget_show(optmenu);

	menu = gtk_menu_new();

	while (p) {
		e = (struct prpl *)p->data;
		if (u) {
			if (e->protocol == u->tmp_protocol)
				found = TRUE;
			if (!found)
				count++;
		} else {
			if (e->protocol == tmpusr.tmp_protocol)
				found = TRUE;
			if (!found)
				count++;
		}
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
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), count);

	return optmenu;
}

static void generate_general_options(struct aim_user *u, GtkWidget *book)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *pwdbox;
	GtkWidget *label;
	GtkWidget *name;
	GtkWidget *pass;
	GtkWidget *rempass;
	GtkWidget *checkmail;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, gtk_label_new(_("General Options")));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Screenname:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	name = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name, TRUE, TRUE, 0);

	pwdbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), pwdbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Password:"));
	gtk_box_pack_start(GTK_BOX(pwdbox), label, FALSE, FALSE, 0);

	pass = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(pwdbox), pass, TRUE, TRUE, 0);
	gtk_entry_set_visibility(GTK_ENTRY(pass), FALSE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Protocol:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	make_protocol_menu(hbox, u);

	rempass = acct_button(_("Remember Password"), u, OPT_USR_REM_PASS, vbox);
	acct_button(_("Auto-Login"), u, OPT_USR_AUTO, vbox);
	/*acct_button(_("Send KeepAlive packet (6 bytes/second)"), u, OPT_USR_KEEPALV, vbox);*/
	checkmail = acct_button(_("New Mail Notifications"), u, OPT_USR_MAIL_CHECK, vbox);

	gtk_widget_show_all(vbox);

	if (u) {
		u->name = name;
		u->pwdbox = pwdbox;
		u->pass = pass;
		u->rempass = rempass;
		u->checkmail = checkmail;
		gtk_entry_set_text(GTK_ENTRY(name), u->username);
		gtk_entry_set_text(GTK_ENTRY(pass), u->password);
		gtk_entry_set_editable(GTK_ENTRY(name), FALSE);
	} else {
		tmpusr.name = name;
		tmpusr.pwdbox = pwdbox;
		tmpusr.pass = pass;
		tmpusr.rempass = rempass;
		tmpusr.checkmail = checkmail;
	}
}

static void generate_prpl_options(struct aim_user *u, GtkWidget *book)
{
	struct prpl *p;

	if (u)
		p = find_prpl(u->tmp_protocol);
	else
		p = find_prpl(tmpusr.protocol);

	/* page 0 is general, keep it. page 1 is options for our
	 * particular protocol, so clear it out and make a new one. */

	gtk_notebook_remove_page(GTK_NOTEBOOK(book), 1);

	if (!p)
		return;

	if (u && u->opt_entries) {
		g_list_free(u->opt_entries);
		u->opt_entries = NULL;
	} else if (!u && tmpusr.opt_entries) {
		g_list_free(tmpusr.opt_entries);
		tmpusr.opt_entries = NULL;
	}

	if (p->user_opts) {
		GList *op = (*p->user_opts)();
		GList *tmp = op;

		GtkWidget *vbox;
		GtkWidget *hbox;
		GtkWidget *label;
		GtkWidget *entry;

		char buf[256];

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		g_snprintf(buf, sizeof(buf), "%s Options", (*p->name)());
		gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, gtk_label_new(buf));
		gtk_widget_show(vbox);

		while (op) {
			struct proto_user_opt *puo = op->data;

			hbox = gtk_hbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
			gtk_widget_show(hbox);

			label = gtk_label_new(puo->label);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_widget_show(label);

			entry = gtk_entry_new();
			gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
			gtk_object_set_user_data(GTK_OBJECT(entry), (void *)puo->pos);
			if (u && u->proto_opt[puo->pos][0]) {
				debug_printf("setting text %s\n", u->proto_opt[puo->pos]);
				gtk_entry_set_text(GTK_ENTRY(entry), u->proto_opt[puo->pos]);
			} else {
				gtk_entry_set_text(GTK_ENTRY(entry), puo->def);
			}
			gtk_widget_show(entry);

			if (u)
				u->opt_entries = g_list_append(u->opt_entries, entry);
			else
				tmpusr.opt_entries = g_list_append(tmpusr.opt_entries, entry);

			g_free(puo);
			op = op->next;
		}
		g_list_free(tmp);
	}
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
	GtkWidget *box;
	GtkWidget *book;
	GtkWidget *hbox;
	GtkWidget *button;

	struct prpl *p;

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
	gtk_window_set_title(GTK_WINDOW(mod), _("Gaim - Modify Account"));
	gtk_window_set_policy(GTK_WINDOW(mod), FALSE, TRUE, TRUE);	/* nothing odd here :) */
	gtk_signal_connect(GTK_OBJECT(mod), "destroy", GTK_SIGNAL_FUNC(delmod), u);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(mod), 5);
	gtk_container_add(GTK_CONTAINER(mod), box);

	book = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(box), book, FALSE, FALSE, 0);

	if (u) {
		if (find_prpl(u->protocol))
			u->tmp_protocol = u->protocol;
		else if (protocols)
			u->tmp_protocol = ((struct prpl *)protocols->data)->protocol;
		else
			u->tmp_protocol = -1;
	} else {
		if (find_prpl(tmpusr.protocol))
			tmpusr.tmp_protocol = tmpusr.protocol;
		else if (protocols)
			tmpusr.tmp_protocol = ((struct prpl *)protocols->data)->protocol;
		else
			tmpusr.tmp_protocol = -1;
	}
	generate_general_options(u, book);
	generate_prpl_options(u, book);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	button = picture_button(mod, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cancel_mod), u);

	button = picture_button(mod, _("OK"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(ok_mod), u);

	if (u) {
		u->mod = mod;
		u->main = book;	/* sorry, i think i broke the joke :) */
		u->tmp_options = u->options;
	} else {
		newmod = mod;
		newmain = book;
	}

	gtk_widget_show_all(mod);
	
	if (u) {
		p = find_prpl(u->tmp_protocol);
		if (p && (p->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(u->pwdbox);
			gtk_widget_hide(u->rempass);
		}
		if (p && (!(p->options & OPT_PROTO_MAIL_CHECK)))
			gtk_widget_hide(u->checkmail);
	} else {
		p = find_prpl(tmpusr.tmp_protocol);
		if (p && (p->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(tmpusr.pwdbox);
			gtk_widget_hide(tmpusr.rempass);
		}
		if (p && (!(p->options & OPT_PROTO_MAIL_CHECK)))
			gtk_widget_hide(tmpusr.checkmail);
	}
}

static void add_acct(GtkWidget *w, gpointer d)
{
	show_acct_mod(NULL);
}

static void mod_acct(GtkWidget *w, gpointer d)
{
	GList *l = GTK_CLIST(list)->selection;
	int row = -1;
	struct aim_user *u;
	while (l) {
		row = (int)l->data;
		if (row != -1) {
			u = g_list_nth_data(aim_users, row);
			if (u)
				show_acct_mod(u);
		}
		l = l->next;
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
	const char *txt = gtk_entry_get_text(GTK_ENTRY(u->passentry));
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

	if (u->passprmt) {
		gtk_widget_show(u->passprmt);
		return;
	}
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
	GList *l = GTK_CLIST(list)->selection;
	int row = -1;
	struct aim_user *u;
	while (l) {
		row = (int)l->data;
		u = g_list_nth_data(aim_users, row);
		if (!u->gc) {
			struct prpl *p = find_prpl(u->protocol);
			if (p && !(p->options & OPT_PROTO_NO_PASSWORD) && !u->password[0]) {
				do_pass_dlg(u);
			} else {
#ifdef USE_APPLET
				set_user_state(signing_on);
#endif /* USE_APPLET */
				gtk_clist_set_text(GTK_CLIST(list), row, 1, "Attempting");
				serv_login(u);
			}
		} else {
			u->gc->wants_to_die = TRUE;
			signoff(u->gc);
		}
		l = l->next;
	}
}

static void do_del_acct(gpointer w, struct aim_user *u)
{
	if (u->gc) {
		u->gc->wants_to_die = TRUE;
		signoff(u->gc);
	}
	gtk_clist_remove(GTK_CLIST(list), g_list_index(aim_users, u));
	aim_users = g_list_remove(aim_users, u);
	save_prefs();
}

static void del_acct(GtkWidget *w, gpointer d)
{
	GList *l = GTK_CLIST(list)->selection;
	char buf[8192];
	int row = -1;
	struct aim_user *u;
	while (l) {
		row = (int)l->data;
		u = g_list_nth_data(aim_users, row);
		if (!u)
			return;

		g_snprintf(buf, sizeof(buf), _("Are you sure you want to delete %s?"), u->username);
		do_ask_dialog(buf, u, do_del_acct, NULL);
		l = l->next;
	}
}

static void sel_auto(gpointer w, gpointer d)
{
	GList *l = aim_users;
	struct aim_user *u;
	int i = 0; /* faster than doing g_list_index each time */
	while (l) {
		u = l->data;
		l = l->next;
		if (u->options & OPT_USR_AUTO)
			gtk_clist_select_row(GTK_CLIST(list), i, -1);
		else
			gtk_clist_unselect_row(GTK_CLIST(list), i, -1);
		i++;
	}
}

void account_editor(GtkWidget *w, GtkWidget *W)
{
	/* please kill me */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *sw;
	GtkWidget *button;	/* used for many things */

	if (acctedit) {
		gtk_widget_show(acctedit);
		return;
	}

	acctedit = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(acctedit), _("Gaim - Account Editor"));
	gtk_window_set_wmclass(GTK_WINDOW(acctedit), "accounteditor", "Gaim");
	gtk_widget_realize(acctedit);
	aol_icon(acctedit->window);
	gtk_widget_set_usize(acctedit, -1, 200);
	gtk_signal_connect(GTK_OBJECT(acctedit), "destroy", GTK_SIGNAL_FUNC(delete_acctedit), W);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(acctedit), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	sw = generate_list();

	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);

	button = picture_button2(acctedit, _("Select All"), tb_refresh_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_clist_select_all), GTK_OBJECT(list));

	button = picture_button2(acctedit, _("Select Autos"), tb_redo_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(sel_auto), NULL);

	button = picture_button2(acctedit, _("Select None"), tb_undo_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_clist_unselect_all), GTK_OBJECT(list));

	gtk_box_pack_start(GTK_BOX(hbox), sw, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(acctedit, _("Add"), gnome_add_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(add_acct), NULL);

	button = picture_button(acctedit, _("Modify"), gnome_preferences_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_acct), NULL);

	button = picture_button(acctedit, _("Sign On/Off"), join_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(acct_signin), NULL);

	button = picture_button(acctedit, _("Delete"), gnome_remove_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(del_acct), NULL);

	button = picture_button(acctedit, _("Close"), cancel_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(acctedit_close), W);

	gtk_widget_show_all(acctedit);
}

void account_online(struct gaim_connection *gc)
{
	int i;

	/* first we hide the login progress meter */
	if (gc->meter)
		gtk_widget_destroy(gc->meter);
	gc->meter = NULL;

	/* then we do the buddy list stuff */
	if (mainwindow)
		gtk_widget_hide(mainwindow);

#ifdef USE_APPLET
	if (blist_options & OPT_BLIST_APP_BUDDY_SHOW) {
		show_buddy_list();
		refresh_buddy_window();
		createOnlinePopup();
		applet_buddy_show = TRUE;
	} else if (!blist) {
		show_buddy_list();
		build_edit_tree();
		gtk_widget_hide(blist);
		applet_buddy_show = FALSE;
	} else {
		build_edit_tree();
	}
	set_user_state(online);
#else
	show_buddy_list();
	refresh_buddy_window();
#endif

	update_connection_dependent_prefs();
	do_away_menu();
	do_proto_menu();
	redo_convo_menus();
	gaim_setup(gc);

	plugin_event(event_signon, gc, 0, 0, 0);
	system_log(log_signon, gc, NULL, OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);

	/* away option given? */
	if (opt_away) {
		away_on_login(opt_away_arg);
		/* don't do it again */
		opt_away = 0;
	} else if (awaymessage) {
		serv_set_away(gc, GAIM_AWAY_CUSTOM, awaymessage->message);
	}
	if (opt_away_arg != NULL) {
		g_free (opt_away_arg);
		opt_away_arg = NULL;
	}

	/* everything for the account editor */
	if (!acctedit)
		return;
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), gc->user);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "Yes");
	gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(gc->protocol));

	return;
}

void account_offline(struct gaim_connection *gc)
{
	int i;
	if (gc->meter)
		gtk_widget_destroy(gc->meter);
	gc->meter = NULL;
	gc->user->gc = NULL;	/* wasn't that awkward? */
	if (!acctedit)
		return;
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), gc->user);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "No");
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

static void cancel_signon(GtkWidget *button, struct gaim_connection *gc)
{
	gc->wants_to_die = TRUE;
	signoff(gc);
}

static gint meter_destroy(GtkWidget *meter, GdkEvent *evt, struct gaim_connection *gc)
{
	return TRUE;
}

void set_login_progress(struct gaim_connection *gc, float howfar, char *message)
{
	if (mainwindow)
		gtk_widget_hide(mainwindow);

	if (!gc->meter) {
		GtkWidget *box, *label, *button;
		char buf[256];

		gc->meter = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_policy(GTK_WINDOW(gc->meter), 0, 0, 1);
		gtk_window_set_wmclass(GTK_WINDOW(gc->meter), "signon", "Gaim");
		gtk_container_set_border_width(GTK_CONTAINER(gc->meter), 5);
		g_snprintf(buf, sizeof(buf), "%s Signing On", gc->username);
		gtk_window_set_title(GTK_WINDOW(gc->meter), buf);
		gtk_signal_connect(GTK_OBJECT(gc->meter), "delete_event",
				   GTK_SIGNAL_FUNC(meter_destroy), gc);
		gtk_widget_realize(gc->meter);
		aol_icon(gc->meter->window);

		box = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(gc->meter), box);
		gtk_widget_show(box);

		label = gtk_label_new(buf);
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		gc->progress = gtk_progress_bar_new();
		gtk_widget_set_usize(gc->progress, 150, 0);
		gtk_box_pack_start(GTK_BOX(box), gc->progress, FALSE, FALSE, 0);
		gtk_widget_show(gc->progress);

		gc->status = gtk_statusbar_new();
		gtk_widget_set_usize(gc->status, 150, 0);
		gtk_box_pack_start(GTK_BOX(box), gc->status, FALSE, FALSE, 0);
		gtk_widget_show(gc->status);

		button = gtk_button_new_with_label(_("Cancel"));
		gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cancel_signon), gc);
		gtk_widget_show(button);

		gtk_widget_show(gc->meter);
	}

	gtk_progress_bar_update(GTK_PROGRESS_BAR(gc->progress), howfar / LOGIN_STEPS);
	gtk_statusbar_pop(GTK_STATUSBAR(gc->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(gc->status), 1, message);
}

static void set_kick_null(GtkObject *obj, struct aim_user *u)
{
	u->kick_dlg = NULL;
}

void hide_login_progress(struct gaim_connection *gc, char *why)
{
	char buf[2048];
	sprintf(buf, _("%s\n%s was unable to sign on: %s"), full_date(), gc->username, why);
	if (gc->user->kick_dlg)
		gtk_widget_destroy(gc->user->kick_dlg);
	gc->user->kick_dlg = do_error_dialog(buf, _("Signon Error"));
	gtk_signal_connect(GTK_OBJECT(gc->user->kick_dlg), "destroy",
			   GTK_SIGNAL_FUNC(set_kick_null), gc->user);
	if (gc->meter)
		gtk_widget_destroy(gc->meter);
	gc->meter = NULL;
}

struct aim_user *new_user(const char *name, int proto, int opts)
{
	char *titles[4];
	int i;

	struct aim_user *u = g_new0(struct aim_user, 1);
	g_snprintf(u->username, sizeof(u->username), "%s", name);
	g_snprintf(u->user_info, sizeof(u->user_info), "%s", DEFAULT_INFO);
	u->protocol = proto;
	u->options = opts;
	aim_users = g_list_append(aim_users, u);

	if (list) {
		titles[0] = u->username;
		titles[1] = u->gc ? "Yes" : "No";
		titles[2] = (u->options & OPT_USR_AUTO) ? "True" : "False";
		titles[3] = proto_name(u->protocol);
		i = gtk_clist_append(GTK_CLIST(list), titles);
		gtk_clist_set_row_data(GTK_CLIST(list), i, u);
	}

	return u;
}
