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
#include <ctype.h>
#include <gtk/gtk.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"

#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/tb_redo.xpm"
#include "pixmaps/tb_undo.xpm"
#include "pixmaps/tb_refresh.xpm"
#include "pixmaps/no_icon.xpm"

#define LOGIN_STEPS 5

GSList *connections;
int connecting_count = 0;

static GtkWidget *acctedit = NULL;
static GtkWidget *list = NULL;	/* the clist of names in the accteditor */

static GSList *mod_users = NULL;

static struct mod_user *find_mod_user(struct aim_user *a)
{
	GSList *m = mod_users;
	while (m) {
		struct mod_user *u = m->data;
		if (u->user == a)
			return u;
		m = m->next;
	}
	return NULL;
}

static void generate_protocol_options(struct mod_user *, GtkWidget *);

struct mod_usr_opt {
	struct mod_user *user;
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
	gc->keepalive = 0;
	gc->inpa = 0;
	gc->buddy_chats = NULL;
	gc->groups = NULL;
	gc->permit = NULL;
	gc->deny = NULL;

	connections = g_slist_append(connections, gc);

	user->gc = gc;
	gc->user = user;

	return gc;
}

struct meter_window {
		GtkWidget *window;
		GtkTable *table;
		gint rows;
		gint active_count;
	} *meter_win = NULL;

void destroy_gaim_conn(struct gaim_connection *gc)
{
	GSList *g = gc->groups;
	GSList *h;
	struct group *m;
	struct buddy *n;
	while (g) {
		m = (struct group *)g->data;
		g = g_slist_remove(g, m);
		h = m->members;
		while (h) {
			n = (struct buddy *)h->data;
			if (gc->prpl->buddy_free)
				gc->prpl->buddy_free(n);
			h = g_slist_remove(h, n);
			g_free(n);
		}
		g_free(m);
	}
	g = gc->permit;
	while (g) {
		g_free(g->data);
		g = g_slist_remove(g, g->data);
	}
	g = gc->deny;
	while (g) {
		g_free(g->data);
		g = g_slist_remove(g, g->data);
	}
	g_free(gc);

	if (!connections && mainwindow)
		gtk_widget_show(mainwindow);
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
		return p->name();
	else
		return "Unknown";
}


static void reorder_list(GtkCList *cl, int from, int to, void *p)
{
	struct aim_user *au;
	if (from == to)
		return; /* This shouldn't happen, but just in case */
	au = (struct aim_user*)g_slist_nth_data(aim_users, from);
	aim_users = g_slist_remove (aim_users, au);
	aim_users = g_slist_insert(aim_users, au, to);
	save_prefs();
}

void regenerate_user_list()
{
	char *titles[4];
	GSList *u = aim_users;
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
	gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_EXTENDED);
	gtk_clist_column_titles_passive(GTK_CLIST(list));
	
	gtk_container_add(GTK_CONTAINER(win), list);
	gtk_widget_show(list);

	regenerate_user_list();
	gtk_clist_set_reorderable (GTK_CLIST(list), TRUE);
	gtk_clist_set_use_drag_icons (GTK_CLIST(list), TRUE);
	gtk_signal_connect(GTK_OBJECT(list), "row-move", GTK_SIGNAL_FUNC(reorder_list), NULL);
		
	gtk_widget_show(win);
	return win;
}

static void delmod(GtkWidget *w, struct mod_user *u)
{
	mod_users = g_slist_remove(mod_users, u);
	g_free(u);
}

static void mod_opt(GtkWidget *b, struct mod_usr_opt *m)
{
	m->user->options = m->user->options ^ m->opt;
}

static void free_muo(GtkWidget *b, struct mod_usr_opt *m)
{
	g_free(m);
}

static GtkWidget *acct_button(const char *text, struct mod_user *u, int option, GtkWidget *box)
{
	GtkWidget *button;
	struct mod_usr_opt *muo = g_new0(struct mod_usr_opt, 1);
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (u->options & option));
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	muo->user = u;
	muo->opt = option;
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_opt), muo);
	gtk_signal_connect(GTK_OBJECT(button), "destroy", GTK_SIGNAL_FUNC(free_muo), muo);
	gtk_widget_show(button);
	return button;
}

static void ok_mod(GtkWidget *w, struct mod_user *u)
{
	GList *tmp;
	const char *txt;
	int i;
	struct aim_user *a;
	struct prpl *p;

	if (!u->user) {
		txt = gtk_entry_get_text(GTK_ENTRY(u->name));
		u->user = new_user(txt, u->protocol, u->options);
	}
	a = u->user;

	a->options = u->options;
	a->protocol = u->protocol;
	txt = gtk_entry_get_text(GTK_ENTRY(u->name));
	g_snprintf(a->username, sizeof(a->username), "%s", txt);
	txt = gtk_entry_get_text(GTK_ENTRY(u->alias));
	g_snprintf(a->alias, sizeof(a->alias), "%s", txt);
	txt = gtk_entry_get_text(GTK_ENTRY(u->pass));
	if (a->options & OPT_USR_REM_PASS)
		g_snprintf(a->password, sizeof(a->password), "%s", txt);
	else
		a->password[0] = '\0';

	i = gtk_clist_find_row_from_data(GTK_CLIST(list), a);
	gtk_clist_set_text(GTK_CLIST(list), i, 0, a->username);
	gtk_clist_set_text(GTK_CLIST(list), i, 2,
			   (a->options & OPT_USR_AUTO) ? "True" : "False");
	gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(a->protocol));

	tmp = u->opt_entries;
	while (tmp) {
		GtkEntry *entry = tmp->data;
		int pos = (int)gtk_object_get_user_data(GTK_OBJECT(entry));
		g_snprintf(a->proto_opt[pos], sizeof(a->proto_opt[pos]), "%s",
				   gtk_entry_get_text(entry));
		tmp = tmp->next;
	}
	if (u->opt_entries)
		g_list_free(u->opt_entries);
	u->opt_entries = NULL;

	g_snprintf(a->iconfile, sizeof(a->iconfile), "%s", u->iconfile);
	if (u->icondlg)
		gtk_widget_destroy(u->icondlg);
	u->icondlg = NULL;

	/*
	 * See if user registration is supported/required
	 */
	if((p = find_prpl(u->protocol)) == NULL) {
		/* TBD: error dialog here! (This should never happen, you know...) */
		fprintf(stderr, "dbg: couldn't find protocol for protocol number %d!\n", u->protocol);
		fflush(stderr);
	} else {
		if(p->register_user != NULL &&
		   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(u->register_user)) == TRUE) {

			p->register_user(a);
		}
	}

	save_prefs();

	gtk_widget_destroy(u->mod);
}

static void cancel_mod(GtkWidget *w, struct mod_user *u)
{
	if (u->opt_entries)
		g_list_free(u->opt_entries);
	u->opt_entries = NULL;
	if (u->icondlg)
		gtk_widget_destroy(u->icondlg);
	u->icondlg = NULL;
	gtk_widget_destroy(u->mod);
}

static void set_prot(GtkWidget *opt, int proto)
{
	struct mod_user *u = gtk_object_get_user_data(GTK_OBJECT(opt));
	struct prpl *p, *q;
	q = find_prpl(proto);
	if (u->protocol != proto) {
		int i;
		for (i = 0; i < 7; i++)
			u->proto_opt[i][0] = '\0';
		p = find_prpl(u->protocol);

		if (!(p->options & OPT_PROTO_NO_PASSWORD) && (q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(u->pwdbox);
			gtk_widget_hide(u->rempass);
		} else if ((p->options & OPT_PROTO_NO_PASSWORD) && !(q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_show(u->pwdbox);
			gtk_widget_show(u->rempass);
		}
		if (!(p->options & OPT_PROTO_MAIL_CHECK) && (q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_show(u->checkmail);
		} else if ((p->options & OPT_PROTO_MAIL_CHECK) && !(q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_hide(u->checkmail);
		}

		if (!(p->options & OPT_PROTO_BUDDY_ICON) && (q->options & OPT_PROTO_BUDDY_ICON)) {
			gtk_widget_show(u->iconsel);
		} else if ((p->options & OPT_PROTO_BUDDY_ICON) && !(q->options & OPT_PROTO_BUDDY_ICON)) {
			gtk_widget_hide(u->iconsel);
		}

		if ((q->options & OPT_PROTO_BUDDY_ICON) || (q->options & OPT_PROTO_MAIL_CHECK))
			gtk_widget_show(u->user_frame);
		else
			gtk_widget_hide(u->user_frame);

		u->protocol = proto;
		generate_protocol_options(u, u->main);
	}
}

static GtkWidget *make_protocol_menu(GtkWidget *box, struct mod_user *u)
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
		if (e->protocol == u->protocol)
			found = TRUE;
		if (!found)
			count++;
		if (e->name)
			opt = gtk_menu_item_new_with_label(e->name());
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

static void des_icon_sel(GtkWidget *w, struct mod_user *u)
{
	w = u->icondlg;
	if (u->icondlg)
		u->icondlg = NULL;
	if (w)
		gtk_widget_destroy(w);
}

static void set_icon(GtkWidget *w, struct mod_user *u)
{
	GtkWidget *sel = u->icondlg;
	const char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(sel));

	if (file_is_dir(file, sel))
		return;

	gtk_entry_set_text(GTK_ENTRY(u->iconentry), file);
	g_snprintf(u->iconfile, sizeof(u->iconfile), "%s", file);
	u->icondlg = NULL;

	gtk_widget_destroy(sel);
}

static void sel_icon_dlg(GtkWidget *w, struct mod_user *u)
{
	GtkWidget *dlg;
	char buf[256];

	if (u->icondlg) {
		gtk_widget_show(u->icondlg);
		return;
	}

	dlg = gtk_file_selection_new(_("Gaim - Load Buddy Icon"));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(dlg));
	if (u->iconfile) {
		char *tmp = g_dirname(u->iconfile);
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(dlg), tmp);
		g_free(tmp);
	} else {
		g_snprintf(buf, sizeof(buf), "%s/", g_get_home_dir());
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(dlg), buf);
	}

	gtk_signal_connect(GTK_OBJECT(dlg), "destroy", GTK_SIGNAL_FUNC(des_icon_sel), u);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(dlg)->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(des_icon_sel), u);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(dlg)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(set_icon), u);

	u->icondlg = dlg;

	gtk_widget_show(dlg);
}

static void reset_icon(GtkWidget *w, struct mod_user *u)
{
	u->iconfile[0] = 0;
	gtk_entry_set_text(GTK_ENTRY(u->iconentry), "");
}

static GtkWidget *build_icon_selection(struct mod_user *u, GtkWidget *box)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *name;
	GtkWidget *browse;
	GtkWidget *reset;

	if (u->user)
		g_snprintf(u->iconfile, sizeof(u->iconfile), "%s", u->user->iconfile);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Buddy Icon File:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name), u->iconfile);
	gtk_entry_set_editable(GTK_ENTRY(name), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), name, TRUE, TRUE, 5);
	gtk_widget_show(name);
	u->iconentry = name;

	browse = gtk_button_new_with_label(_("Browse"));
	gtk_signal_connect(GTK_OBJECT(browse), "clicked", GTK_SIGNAL_FUNC(sel_icon_dlg), u);
	gtk_box_pack_start(GTK_BOX(hbox), browse, FALSE, FALSE, 0);
	gtk_widget_show(browse);

	reset = gtk_button_new_with_label(_("Reset"));
	gtk_signal_connect(GTK_OBJECT(reset), "clicked", GTK_SIGNAL_FUNC(reset_icon), u);
	gtk_box_pack_start(GTK_BOX(hbox), reset, FALSE, FALSE, 0);
	gtk_widget_show(reset);

	return hbox;
}

static void generate_login_options(struct mod_user *u, GtkWidget *box)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;

	struct prpl *p;

	frame = gtk_frame_new("Login Options");
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Screenname:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	u->name = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), u->name, TRUE, TRUE, 0);

	u->pwdbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), u->pwdbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Password:"));
	gtk_box_pack_start(GTK_BOX(u->pwdbox), label, FALSE, FALSE, 0);

	u->pass = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(u->pwdbox), u->pass, TRUE, TRUE, 0);
	gtk_entry_set_visibility(GTK_ENTRY(u->pass), FALSE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Alias:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	u->alias = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), u->alias, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Protocol:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	make_protocol_menu(hbox, u);

	u->rempass = acct_button(_("Remember Password"), u, OPT_USR_REM_PASS, vbox);
	acct_button(_("Auto-Login"), u, OPT_USR_AUTO, vbox);

	gtk_widget_show_all(frame);

	if (u->user) {
		gtk_entry_set_text(GTK_ENTRY(u->name), u->user->username);
		gtk_entry_set_text(GTK_ENTRY(u->alias), u->user->alias);		
		gtk_entry_set_text(GTK_ENTRY(u->pass), u->user->password);
	}

	p = find_prpl(u->protocol);
	if (p && (p->options & OPT_PROTO_NO_PASSWORD)) {
		gtk_widget_hide(u->pwdbox);
		gtk_widget_hide(u->rempass);
	}
}

static void generate_user_options(struct mod_user *u, GtkWidget *box)
{
	/* This function will add the appropriate (depending on the current
	 * protocol) widgets to frame and return TRUE if there anything
	 * was added (meaning the frame should be shown)
	 * Eric will most likely change this (as he does all other submitted code)
	 * so that it will accept the vbox as an argument and create, add, and show
	 * the frame itself (like generate_protocol_options).  I'd do it myself, but I'm
	 * tired and I don't care. */
	/* Sean was right. I did do that. I told him I would. */

	GtkWidget *vbox;

	struct prpl *p = find_prpl(u->protocol);

	u->user_frame = gtk_frame_new("User Options");
	gtk_box_pack_start(GTK_BOX(box), u->user_frame, FALSE, FALSE, 0);
	gtk_widget_show(u->user_frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(u->user_frame), vbox);
	gtk_widget_show(vbox);

	u->checkmail = acct_button(_("New Mail Notifications"), u, OPT_USR_MAIL_CHECK, vbox);
	u->iconsel = build_icon_selection(u, vbox);

	if (!p) {
		gtk_widget_hide(u->user_frame);
		return;
	}

	if (!(p->options & OPT_PROTO_MAIL_CHECK))
		gtk_widget_hide(u->checkmail);
	if (!(p->options & OPT_PROTO_BUDDY_ICON))
		gtk_widget_hide(u->iconsel);

	if ((p->options & OPT_PROTO_BUDDY_ICON) || (p->options & OPT_PROTO_MAIL_CHECK))
		return;
	gtk_widget_hide(u->user_frame);
}

static void generate_protocol_options(struct mod_user *u, GtkWidget *box)
{
	struct prpl *p = find_prpl(u->protocol);

	GList *op, *tmp;

	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	char buf[256];

	if (u->proto_frame)
		gtk_widget_destroy(u->proto_frame);
	u->proto_frame = NULL;

	if (u->opt_entries) {
		g_list_free(u->opt_entries);
		u->opt_entries = NULL;
	}

	if (!p)
		return;

	if (!p->user_opts)
		return;

	tmp = op = p->user_opts();

	if (!op)
		return;

	g_snprintf(buf, sizeof(buf), "%s Options", p->name());
	u->proto_frame = gtk_frame_new(buf);
	gtk_box_pack_start(GTK_BOX(box), u->proto_frame, FALSE, FALSE, 0);
	gtk_widget_show(u->proto_frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(u->proto_frame), vbox);
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
		if (u->proto_opt[puo->pos][0]) {
			debug_printf("setting text %s\n", u->proto_opt[puo->pos]);
			gtk_entry_set_text(GTK_ENTRY(entry), u->proto_opt[puo->pos]);
		} else {
			gtk_entry_set_text(GTK_ENTRY(entry), puo->def);
		}
		gtk_widget_show(entry);

		u->opt_entries = g_list_append(u->opt_entries, entry);

		g_free(puo);
		op = op->next;
	}

	if(p->register_user != NULL) {
		u->register_user = gtk_check_button_new_with_label(_("Register with server"));
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(u->register_user), FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), u->register_user, FALSE, FALSE, 0);
		gtk_widget_show(u->register_user);
	}

	g_list_free(tmp);
}

static void show_acct_mod(struct aim_user *a)
{
	/* This is the fucking modify account dialog. I've fucking seperated it into
	 * three fucking frames:
	 * a fucking Login Options frame, a fucking User Options frame and a fucking
	 * Protcol Options frame. This fucking removes the two fucking tabs, which were
	 * quite fucking uneccessary. Fuck. */
				/* -- SeanEgan */
	GtkWidget *hbox;
	GtkWidget *button;

	struct mod_user *u = find_mod_user(a);

	if (!u) {
		u = g_new0(struct mod_user, 1);
		u->user = a;
		mod_users = g_slist_append(mod_users, u);

		if (a) {
			int i;
			u->options = a->options;
			if (find_prpl(a->protocol))
				u->protocol = a->protocol;
			else if (protocols)
				u->protocol = ((struct prpl *)protocols->data)->protocol;
			else
				u->protocol = -1;
			g_snprintf(u->iconfile, sizeof(u->iconfile), "%s", a->iconfile);
			for (i = 0; i < 7; i++)
				g_snprintf(u->proto_opt[i], sizeof(u->proto_opt[i]), "%s",
						a->proto_opt[i]);
		} else {
			u->options = OPT_USR_REM_PASS;
			if (find_prpl(DEFAULT_PROTO))
				u->protocol = DEFAULT_PROTO;
			else if (protocols)
				u->protocol = ((struct prpl *)protocols->data)->protocol;
			else
				u->protocol = -1;
		}
	} else {
		gtk_widget_show(u->mod);
		return;
	}

	u->mod = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(u->mod), "account", "Gaim");
	gtk_widget_realize(u->mod);
	gtk_window_set_title(GTK_WINDOW(u->mod), _("Gaim - Modify Account"));
	gtk_window_set_policy(GTK_WINDOW(u->mod), FALSE, TRUE, TRUE);	/* nothing odd here :) */
	gtk_signal_connect(GTK_OBJECT(u->mod), "destroy", GTK_SIGNAL_FUNC(delmod), u);

	u->main = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(u->main), 5);
	gtk_container_add(GTK_CONTAINER(u->mod), u->main);
	gtk_widget_show(u->main);

	generate_login_options(u, u->main);
	generate_user_options(u, u->main);
	generate_protocol_options(u, u->main);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(u->main), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = picture_button(u->mod, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cancel_mod), u);
	gtk_widget_show(button);

	button = picture_button(u->mod, _("OK"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(ok_mod), u);
	gtk_widget_show(button);

	gtk_widget_show(u->mod);
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
			u = g_slist_nth_data(aim_users, row);
			if (u)
				show_acct_mod(u);
		}
		l = l->next;
	}
}

struct pass_prompt {
	struct aim_user *u;
	GtkWidget *win;
	GtkWidget *entry;
};
static GSList *passes = NULL;

static struct pass_prompt *find_pass_prompt(struct aim_user *u)
{
	GSList *p = passes;
	while (p) {
		struct pass_prompt *r = p->data;
		if (r->u == u)
			return r;
		p = p->next;
	}
	return NULL;
}

static void pass_des(GtkWidget *w, struct pass_prompt *p)
{
	passes = g_slist_remove(passes, p);
	g_free(p);
}

static void pass_cancel(GtkWidget *w, struct pass_prompt *p)
{
	gtk_widget_destroy(p->win);
}

static void pass_signon(GtkWidget *w, struct pass_prompt *p)
{
	const char *txt = gtk_entry_get_text(GTK_ENTRY(p->entry));
	g_snprintf(p->u->password, sizeof(p->u->password), "%s", txt);
	serv_login(p->u);
	gtk_widget_destroy(p->win);
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
	struct pass_prompt *p = find_pass_prompt(u);

	if (p) {
		gtk_widget_show(p->win);
		return;
	}

	p = g_new0(struct pass_prompt, 1);
	p->u = u;
	passes = g_slist_append(passes, p);

	GAIM_DIALOG(p->win);
	gtk_window_set_wmclass(GTK_WINDOW(p->win), "password", "Gaim");
	gtk_container_border_width(GTK_CONTAINER(p->win), 5);
	gtk_signal_connect(GTK_OBJECT(p->win), "destroy", GTK_SIGNAL_FUNC(pass_des), p);
	gtk_widget_realize(p->win);

	frame = gtk_frame_new(_("Enter Password"));
	gtk_container_add(GTK_CONTAINER(p->win), frame);
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

	p->entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(p->entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), p->entry, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(p->entry), "activate", GTK_SIGNAL_FUNC(pass_signon), p);
	gtk_widget_grab_focus(p->entry);
	gtk_widget_show(p->entry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(p->win, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pass_cancel), p);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	button = picture_button(p->win, _("Signon"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pass_signon), p);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	gtk_widget_show(p->win);
}

static void acct_signin(GtkWidget *w, gpointer d)
{
	GList *l = GTK_CLIST(list)->selection;
	int row = -1;
	struct aim_user *u = NULL;
	struct prpl *p = NULL;
	while (l) {
		row = (int)l->data;
		u = g_slist_nth_data(aim_users, row);
		p = find_prpl(u->protocol);
		if (!u->gc && p && p->login) {
			struct prpl *p = find_prpl(u->protocol);
			if (p && !(p->options & OPT_PROTO_NO_PASSWORD) && !u->password[0]) {
				do_pass_dlg(u);
			} else {
				serv_login(u);
				gtk_clist_set_text(GTK_CLIST(list), row, 1, "Attempting");
			}
		} else if (u->gc) {
			u->gc->wants_to_die = TRUE;
			signoff(u->gc);
		} else {
			if (u->protocol == PROTO_TOC)
				do_error_dialog(_("TOC not found."), 
						_("You have attempted to login an IM account using the "
						 "TOC protocol.  Because this protocol is inferior to "
						 "OSCAR, it is now compiled as a plugin by default.  "
						 "To login, edit this account to use OSCAR or load the "
						  "TOC plugin."), GAIM_ERROR);
			else
				do_error_dialog(_("Protocol not found."), 
						_("You cannot log this account in; you do not have "
						  "the protocol it uses loaded, or the protocol does "
						  "not have a login function."), GAIM_ERROR);
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
	gtk_clist_remove(GTK_CLIST(list), g_slist_index(aim_users, u));
	aim_users = g_slist_remove(aim_users, u);
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
		u = g_slist_nth_data(aim_users, row);
		if (!u)
			return;

		g_snprintf(buf, sizeof(buf), _("Are you sure you want to delete %s?"), u->username);
		do_ask_dialog(buf, u, do_del_acct, NULL);
		l = l->next;
	}
}

static void sel_auto(gpointer w, gpointer d)
{
	GSList *l = aim_users;
	struct aim_user *u;
	int i = 0;		/* faster than doing g_list_index each time */
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
		gtk_window_present(GTK_WINDOW(acctedit));
		return;
	}

	acctedit = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(acctedit), _("Gaim - Account Editor"));
	gtk_window_set_wmclass(GTK_WINDOW(acctedit), "accounteditor", "Gaim");
	gtk_widget_realize(acctedit);
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

struct signon_meter {
	struct gaim_connection *gc;
	GtkWidget *button;
	GtkWidget *progress;
	GtkWidget *status;
};
static GSList *meters = NULL;

GtkWidget* create_meter_pixmap (GtkWidget *widget, struct gaim_connection *gc)
{
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;
	GtkWidget *pixmap;

	create_prpl_icon (widget, gc, &gdkpixmap, &mask);
			
	pixmap = gtk_pixmap_new (gdkpixmap, mask);
	gdk_pixmap_unref (gdkpixmap);
	gdk_bitmap_unref (mask);
	return pixmap;
}

static struct signon_meter *find_signon_meter(struct gaim_connection *gc)
{
	GSList *m = meters;
	while (m) {
		if (((struct signon_meter *)m->data)->gc == gc)
			return m->data;
		m = m->next;
	}
	return NULL;
}

void kill_meter(struct signon_meter *meter) {
	gtk_widget_set_sensitive (meter->button, FALSE);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(meter->progress), 1);
	gtk_statusbar_pop(GTK_STATUSBAR(meter->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(meter->status), 1, "Done.");
	meter_win->active_count--;
	if (meter_win->active_count == 0) {
		gtk_widget_destroy(meter_win->window);
		g_free (meter_win);
		meter_win = NULL;
	}
}

void account_online(struct gaim_connection *gc)
{
	int i;
	struct signon_meter *meter = find_signon_meter(gc);

	/* first we hide the login progress meter */
	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}

	/* then we do the buddy list stuff */
	if (mainwindow)
		gtk_widget_hide(mainwindow);

	show_buddy_list();

	update_privacy_connections();
	do_away_menu();
	do_proto_menu();
	redo_convo_menus();
	gaim_setup(gc);

	gc->user->connecting = FALSE;
	if (connecting_count) {
		connecting_count--;
	}
	debug_printf("connecting_count: %d\n", connecting_count);

	plugin_event(event_signon, gc);
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
		g_free(opt_away_arg);
		opt_away_arg = NULL;
	}

	/* everything for the account editor */
	if (!acctedit)
		return;
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), gc->user);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "Yes");
	gtk_clist_set_text(GTK_CLIST(list), i, 3, gc->prpl->name());

	return;
}

void account_offline(struct gaim_connection *gc)
{
	int i;
	struct signon_meter *meter = find_signon_meter(gc);
	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}
	gc->user->gc = NULL;	/* wasn't that awkward? */
	if (!acctedit)
		return;
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), gc->user);
	gtk_clist_set_text(GTK_CLIST(list), i, 1, "No");
}

void auto_login()
{
	GSList *u = aim_users;
	struct aim_user *a = NULL;

	while (u) {
		a = (struct aim_user *)u->data;
		if ((a->options & OPT_USR_AUTO) && (a->options & OPT_USR_REM_PASS)) {
			serv_login(a);
		}
		u = u->next;
	}
}

/*
 * d:)->-< 
 *
 * d:O-\-<
 * 
 * d:D-/-<
 *
 * d8D->-< DANCE!
 */

static void cancel_signon(GtkWidget *button, struct signon_meter *meter)
{
	meter->gc->wants_to_die = TRUE;
	signoff(meter->gc);
}

static gint meter_destroy(GtkWidget *window, GdkEvent *evt, struct signon_meter *meter)
{
	return TRUE;
}

static struct signon_meter *register_meter(struct gaim_connection *gc, GtkWidget *widget, GtkTable *table, gint *rows)
{
	GtkWidget *graphic;
	GtkWidget *label;
	GtkWidget *nest_vbox;
	GString *name_to_print;
	struct signon_meter *meter;
	
	name_to_print = g_string_new(gc->username);
	
	meter = g_new0(struct signon_meter, 1);
	
	(*rows)++;
	gtk_table_resize (table, *rows, 4);
	
	graphic = create_meter_pixmap( widget , gc);
	
	nest_vbox = gtk_vbox_new (FALSE, 0);
	
	name_to_print = g_string_prepend(name_to_print, "Signon: ");
	label = gtk_label_new (name_to_print->str);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
		
	meter->status = gtk_statusbar_new();
	gtk_widget_set_usize(meter->status, 250, 0);
	
	meter->progress = gtk_progress_bar_new ();
	
	meter->button = gtk_button_new_with_label ("Cancel");
	gtk_signal_connect (GTK_OBJECT (meter->button), "clicked", GTK_SIGNAL_FUNC (cancel_signon), meter);
		
	gtk_table_attach (GTK_TABLE (table), graphic, 0, 1, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), nest_vbox, 1, 2, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (label), FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (meter->status), FALSE, FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), meter->progress, 2, 3, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), meter->button, 3, 4, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);

	gtk_widget_show_all (GTK_WIDGET (meter_win->window));
	
	meter_win->active_count++;
	
	return meter;
}

static void loop_cancel () {
	GSList *m = meters;
	struct signon_meter *meter = NULL;
	
	while (m) {
		meter = (struct signon_meter *) (m->data);
		meter->gc->wants_to_die = TRUE;
		signoff((struct gaim_connection *) meter->gc);
		m = meters;
		}
	}

void set_login_progress(struct gaim_connection *gc, float howfar, char *message)
{
	struct signon_meter *meter = find_signon_meter(gc);

	if (mainwindow)
		gtk_widget_hide(mainwindow);

	if (!meter_win) {
		GtkWidget *cancel_button;
		GtkWidget *vbox;
				
		meter_win = g_new0(struct meter_window, 1);
		meter_win->rows=0;
			
		meter_win->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		GAIM_DIALOG(meter_win->window);
		gtk_window_set_policy(GTK_WINDOW(meter_win->window), 0, 0, 1);
		gtk_window_set_wmclass(GTK_WINDOW(meter_win->window), "signon", "Gaim");
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->window), 5);
		gtk_window_set_title (GTK_WINDOW (meter_win->window), "Gaim Account Signon");
		gtk_widget_realize(meter_win->window);
		
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (meter_win->window), GTK_WIDGET (vbox));

		meter_win->table = (GtkTable *) gtk_table_new (1 , 4, FALSE);
		gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (meter_win->table), FALSE, FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (meter_win->table), 5);
		gtk_table_set_row_spacings (GTK_TABLE (meter_win->table), 5);
		gtk_table_set_col_spacings (GTK_TABLE (meter_win->table), 10);
	
		cancel_button = gtk_button_new_with_label ("Cancel All");
    	gtk_signal_connect_object (GTK_OBJECT (cancel_button), "clicked", GTK_SIGNAL_FUNC (loop_cancel), NULL);
		gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (cancel_button), FALSE, FALSE, 0);
	
		gtk_signal_connect (GTK_OBJECT (meter_win->window), "delete_event", GTK_SIGNAL_FUNC (meter_destroy), NULL);
		}
	
	if (!meter) {
		char buf[256];

		meter = register_meter(gc, GTK_WIDGET (meter_win->window), GTK_TABLE (meter_win->table), (gint *)  &meter_win->rows);
		meter->gc = gc;
		meters = g_slist_append(meters, meter);

		g_snprintf(buf, sizeof(buf), "%s Signing On (using %s)", gc->username, gc->prpl->name());
	}

	gtk_progress_bar_update(GTK_PROGRESS_BAR(meter->progress), howfar / LOGIN_STEPS);
	gtk_statusbar_pop(GTK_STATUSBAR(meter->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(meter->status), 1, message);
}

struct kick_dlg {
	struct aim_user *user;
	GtkWidget *dlg;
};
static GSList *kicks = NULL;

static struct kick_dlg *find_kick_dlg(struct aim_user *u)
{
	GSList *k = kicks;
	while (k) {
		struct kick_dlg *d = k->data;
		if (d->user == u)
			return d;
		k = k->next;
	}
	return NULL;
}

static void set_kick_null(GtkObject *obj, struct kick_dlg *k)
{
	kicks = g_slist_remove(kicks, k);
	g_free(k);
}

/*
 * Common code for hide_login_progress(), and hide_login_progress_info()
 */
static void hide_login_progress_common(struct gaim_connection *gc,
				       char *details,
				       char *title,
				       char *prologue)
{
	char buf[2048];
	struct kick_dlg *k = find_kick_dlg(gc->user);
	struct signon_meter *meter = find_signon_meter(gc);
	sprintf(buf, _("%s\n%s: %s"), full_date(), prologue, details);
	if (k)
		gtk_widget_destroy(k->dlg);
	k = g_new0(struct kick_dlg, 1);
	k->user = gc->user;
	k->dlg = do_error_dialog(title, buf, GAIM_ERROR);
	kicks = g_slist_append(kicks, k);
	gtk_signal_connect(GTK_OBJECT(k->dlg), "destroy", GTK_SIGNAL_FUNC(set_kick_null), k);
	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}
}

void hide_login_progress(struct gaim_connection *gc, char *why)
{
	char buf[2048];

	sprintf(buf, _("%s was unable to sign on"), gc->username);
	hide_login_progress_common(gc, why, _("Signon Error"), buf);
}

/*
 * Like hide_login_progress(), but for informational, not error/warning,
 * messages.
 *
 */
void hide_login_progress_notice(struct gaim_connection *gc, char *why)
{
	hide_login_progress_common(gc, why, _("Notice"), gc->username);
}

/*
 * Like hide_login_progress(), but for non-signon error messages.
 *
 */
void hide_login_progress_error(struct gaim_connection *gc, char *why)
{
	char buf[2048];
	g_snprintf(buf, sizeof(buf), _("%s has been signed off"), gc->username);
	hide_login_progress_common(gc, why, _("Connection Error"), buf);
}

void signoff_all()
{
	GSList *c = connections;
	struct gaim_connection *g = NULL;

	while (c) {
		g = (struct gaim_connection *)c->data;
		g->wants_to_die = TRUE;
		signoff(g);
		c = connections;
	}
}

void signoff(struct gaim_connection *gc)
{
	/* UI stuff */
	convo_menu_remove(gc);
	remove_icon_data(gc);

	/* core stuff */
	/* remove this here so plugins get a sensible count of connections */
	connections = g_slist_remove(connections, gc);
	debug_printf("date: %s\n", full_date());
	plugin_event(event_signoff, gc);
	system_log(log_signoff, gc, NULL, OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);
	/* set this in case the plugin died before really connecting.
	   do it after calling the plugins so they can determine if
	   this user was ever on-line or not */
	if (gc->user->connecting) {
		gc->user->connecting = FALSE;
		if (connecting_count) {
			connecting_count--;
		}
	}
	debug_printf("connecting_count: %d\n", connecting_count);
	serv_close(gc);

	/* more UI stuff */
	redo_buddy_list();
	build_edit_tree();
	do_away_menu();
	do_proto_menu();
	redo_convo_menus();
	update_privacy_connections();

	/* in, out, shake it all about */
	if (connections)
		return;

	destroy_all_dialogs();
	destroy_buddy();

	show_login();
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
	aim_users = g_slist_append(aim_users, u);

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
