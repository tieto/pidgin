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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#include "conversation.h"
#include "gtklist.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define LOGIN_STEPS 5

GSList *connections;
int connecting_count = 0;

static GtkWidget *acctedit = NULL;
static GtkWidget *treeview = NULL; /* the treeview of names in the accteditor */
static GtkListStore *model = NULL;

static GSList *mod_accounts = NULL;

enum
{
	COLUMN_SCREENNAME,
	COLUMN_ONLINE,
	COLUMN_AUTOLOGIN,
	COLUMN_PROTOCOL,
	COLUMN_DATA,
	NUM_COLUMNS
};

struct mod_account {
	struct gaim_account *account;

	/* these are temporary */
	int options;
	int protocol;
	char proto_opt[7][256];

	/* stuff for modify window */
	GtkWidget *mod;
	GtkWidget *main;
	GtkWidget *name;
	GtkWidget *alias;
	GtkWidget *pwdbox;
	GtkWidget *pass;
	GtkWidget *rempass;
	GtkWidget *user_frame;
	GtkWidget *proto_frame;
	GtkSizeGroup *sg;
	GList *opt_entries;

	/* stuff for icon selection */
	char iconfile[256];
	GtkWidget *iconsel;
	GtkWidget *iconentry;
	GtkWidget *icondlg;

	/* stuff for mail check prompt */
	GtkWidget *checkmail;

	/* stuff for register with server */
	GtkWidget *register_user;

	/* stuff for proxy options */
	GtkWidget *proxy_frame;
	GtkWidget *proxy_host_box;
	GtkWidget *proxytype_menu;
	GtkWidget *proxyhost_entry;
	GtkWidget *proxyport_entry;
	GtkWidget *proxyuser_entry;
	GtkWidget *proxypass_entry;
};


struct mod_account_opt {
	struct mod_account *ma;
	int opt;
};

static void acct_signin(GtkCellRendererToggle *cell, gchar *path_str,
						gpointer d);
static void acct_autologin(GtkCellRendererToggle *cell, gchar *path_str,
						   gpointer d);

static struct mod_account *mod_account_find(struct gaim_account *a)
{
	GSList *m = mod_accounts;
	while (m) {
		struct mod_account *ma = m->data;
		if (ma->account == a)
			return ma;
		m = m->next;
	}
	return NULL;
}

static void generate_protocol_options(struct mod_account *, GtkWidget *);


struct gaim_connection *new_gaim_conn(struct gaim_account *account)
{
	struct gaim_connection *gc = g_new0(struct gaim_connection, 1);
	gc->edittype = EDIT_GC;
	gc->protocol = account->protocol;
	gc->prpl = find_prpl(account->protocol);
	g_snprintf(gc->username, sizeof(gc->username), "%s", account->username);
	g_snprintf(gc->password, sizeof(gc->password), "%s", account->password);
	gc->keepalive = 0;
	gc->inpa = 0;
	gc->buddy_chats = NULL;
	gc->away = NULL;
	gc->away_state = NULL;

	connections = g_slist_append(connections, gc);

	account->gc = gc;
	gc->account = account;

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
	GaimBlistNode *gnode,*bnode;
	struct group *m;
	struct buddy *n;
	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		m = (struct group *)gnode;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			n = (struct buddy *)bnode;
			if(n->account == gc->account) {
				n->present = 0;
			}
		}
	}
	g_free(gc->away);
	g_free(gc->away_state);
	g_free(gc);

	if (!connections && mainwindow)
		gtk_widget_show(mainwindow);
}

static void quit_acctedit(gpointer d)
{
	if (acctedit) {
		save_prefs();
		gtk_widget_destroy(acctedit);
		acctedit = NULL;
	}

	treeview = NULL;

	if (!d && !GAIM_GTK_BLIST(gaim_get_blist())->window &&
		!mainwindow && !connections) {

		do_quit();
	}
}

static void on_delete_acctedit(GtkWidget *w, GdkEvent *ev, gpointer d)
{
	quit_acctedit(d);
}

static void on_close_acctedit(GtkButton *button, gpointer d)
{
	quit_acctedit(d);
}

static char *proto_name(int proto)
{
	struct prpl *p = find_prpl(proto);
	if (p && p->name)
		return p->name;
	else
		return "Unknown";
}

void regenerate_user_list()
{
	GSList *accounts = gaim_accounts;
	struct gaim_account *a;
	GtkTreeIter iter;

	if (!acctedit)
		return;

	gtk_list_store_clear(model);

	while (accounts) {
		a = (struct gaim_account *)accounts->data;

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter,
						   COLUMN_SCREENNAME, a->username,
						   COLUMN_ONLINE, (a->gc ? TRUE : FALSE),
						   COLUMN_AUTOLOGIN, (a->options & OPT_ACCT_AUTO),
						   COLUMN_PROTOCOL, proto_name(a->protocol),
						   COLUMN_DATA, a,
						   -1);
		accounts = accounts->next;
	}
}

static gboolean get_iter_from_data(GtkTreeView *treeview,
								   struct gaim_account *a, GtkTreeIter *iter)
{
	return gtk_tree_model_iter_nth_child(gtk_tree_view_get_model(treeview),
										 iter, NULL,
										 g_slist_index(gaim_accounts, a));
}

static void add_columns(GtkWidget *treeview)
{
	GtkCellRenderer *renderer;
	/* GtkTreeViewColumn *column; */

	/* Screennames */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Screenname"),
												renderer,
												"text", COLUMN_SCREENNAME,
												NULL);

	/* Online? */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(acct_signin), model);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Online"),
												renderer,
												"active", COLUMN_ONLINE,
												NULL);

	/* Auto-login? */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(acct_autologin), model);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Auto-login"),
												renderer,
												"active", COLUMN_AUTOLOGIN,
												NULL);

	/* Protocol */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Protocol"),
												renderer,
												"text", COLUMN_PROTOCOL,
												NULL);

	/* Data */
	/*
	column = gtk_tree_view_column_new();
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);
	gtk_tree_view_column_set_visible(column, FALSE);
	*/
}

static GtkWidget *generate_list()
{
	GtkWidget *win;

	win = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(win),
										GTK_SHADOW_IN);

	/* Create the list model. */
	model = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_selection_set_mode(
		gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
		GTK_SELECTION_MULTIPLE);

	add_columns(treeview);

	gtk_container_add(GTK_CONTAINER(win), treeview);
	gtk_widget_show(treeview);

	regenerate_user_list();
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW(treeview), TRUE);
	gtk_widget_show(win);
	return win;
}

static void delmod(GtkWidget *w, struct mod_account *ma)
{
	mod_accounts = g_slist_remove(mod_accounts, ma);
	g_free(ma);
}

static void mod_opt(GtkWidget *b, struct mod_account_opt *mao)
{
	mao->ma->options = mao->ma->options ^ mao->opt;
}

static void free_mao(GtkWidget *b, struct mod_account_opt *mao)
{
	g_free(mao);
}

static GtkWidget *acct_button(const char *text, struct mod_account *ma, int option, GtkWidget *box)
{
	GtkWidget *button;
	struct mod_account_opt *mao = g_new0(struct mod_account_opt, 1);
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (ma->options & option));
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	mao->ma = ma;
	mao->opt = option;
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(mod_opt), mao);
	g_signal_connect(GTK_OBJECT(button), "destroy", G_CALLBACK(free_mao), mao);
	gtk_widget_show(button);
	return button;
}

static void ok_mod(GtkWidget *w, struct mod_account *ma)
{
	GList *tmp;
	const char *txt;
	struct gaim_account *a;
	struct prpl *p;
	GtkTreeIter iter;
	int proxytype;

	if (!ma->account) {
		txt = gtk_entry_get_text(GTK_ENTRY(ma->name));
		ma->account = gaim_account_new(txt, ma->protocol, ma->options);
	}
	a = ma->account;

	a->options = ma->options;
	a->protocol = ma->protocol;
	txt = gtk_entry_get_text(GTK_ENTRY(ma->name));
	g_snprintf(a->username, sizeof(a->username), "%s", txt);
	txt = gtk_entry_get_text(GTK_ENTRY(ma->alias));
	g_snprintf(a->alias, sizeof(a->alias), "%s", txt);
	txt = gtk_entry_get_text(GTK_ENTRY(ma->pass));
	if (a->options & OPT_ACCT_REM_PASS)
		g_snprintf(a->password, sizeof(a->password), "%s", txt);
	else
		a->password[0] = '\0';

	if (get_iter_from_data(GTK_TREE_VIEW(treeview), a, &iter)) {
		gtk_list_store_set(model, &iter,
						   COLUMN_SCREENNAME, a->username,
						   COLUMN_AUTOLOGIN, (a->options & OPT_ACCT_AUTO),
						   COLUMN_PROTOCOL, proto_name(a->protocol),
						   -1);
	}

#if 0
	i = gtk_clist_find_row_from_data(GTK_CLIST(list), a);
	gtk_clist_set_text(GTK_CLIST(list), i, 0, a->username);
	gtk_clist_set_text(GTK_CLIST(list), i, 2,
			   (a->options & OPT_ACCT_AUTO) ? "True" : "False");
	gtk_clist_set_text(GTK_CLIST(list), i, 3, proto_name(a->protocol));
#endif

	tmp = ma->opt_entries;
	while (tmp) {
		GtkEntry *entry = tmp->data;
		int pos = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "position"));
		g_snprintf(a->proto_opt[pos], sizeof(a->proto_opt[pos]), "%s",
				   gtk_entry_get_text(entry));
		tmp = tmp->next;
	}
	if (ma->opt_entries)
		g_list_free(ma->opt_entries);
	ma->opt_entries = NULL;

	g_snprintf(a->iconfile, sizeof(a->iconfile), "%s", ma->iconfile);
	if (ma->icondlg)
		gtk_widget_destroy(ma->icondlg);
	ma->icondlg = NULL;

	if(ma->account->gpi)
		g_free(ma->account->gpi);
	ma->account->gpi = NULL;

	proxytype = GPOINTER_TO_INT(g_object_get_data(
				G_OBJECT(gtk_menu_get_active(GTK_MENU(ma->proxytype_menu))),
				"proxytype"));

	if(proxytype != PROXY_USE_GLOBAL) {
		struct gaim_proxy_info *gpi = g_new0(struct gaim_proxy_info, 1);
		gpi->proxytype = proxytype;
		g_snprintf(gpi->proxyhost, sizeof(gpi->proxyhost), "%s", gtk_entry_get_text(GTK_ENTRY(ma->proxyhost_entry)));
		gpi->proxyport = atoi(gtk_entry_get_text(GTK_ENTRY(ma->proxyport_entry)));
		g_snprintf(gpi->proxyuser, sizeof(gpi->proxyuser), "%s", gtk_entry_get_text(GTK_ENTRY(ma->proxyuser_entry)));
		g_snprintf(gpi->proxypass, sizeof(gpi->proxypass), "%s", gtk_entry_get_text(GTK_ENTRY(ma->proxypass_entry)));

		ma->account->gpi = gpi;
	}

	/*
	 * See if user registration is supported/required
	 */
	if((p = find_prpl(ma->protocol)) == NULL) {
		/* TBD: error dialog here! (This should never happen, you know...) */
		fprintf(stderr, "dbg: couldn't find protocol for protocol number %d!\n", ma->protocol);
		fflush(stderr);
	} else {
		if(p->register_user != NULL &&
		   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ma->register_user)) == TRUE) {
			ref_protocol(p);
			p->register_user(a);
			/* we don't unref the protocol because register user has callbacks
			 * that need to get called first, then they will unref the protocol
			 * appropriately */
		}
	}

	save_prefs();

	gtk_widget_destroy(ma->mod);
}

static void cancel_mod(GtkWidget *w, struct mod_account *ma)
{
	if (ma->opt_entries)
		g_list_free(ma->opt_entries);
	ma->opt_entries = NULL;
	if (ma->icondlg)
		gtk_widget_destroy(ma->icondlg);
	ma->icondlg = NULL;
	gtk_widget_destroy(ma->mod);
}

static void set_prot(GtkWidget *opt, int proto)
{
	struct mod_account *ma = g_object_get_data(G_OBJECT(opt), "mod_account");
	struct prpl *p, *q;
	q = find_prpl(proto);
	if (ma->protocol != proto) {
		int i;
		for (i = 0; i < 7; i++)
			ma->proto_opt[i][0] = '\0';
		p = find_prpl(ma->protocol);

		if (!(p->options & OPT_PROTO_NO_PASSWORD) && (q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_hide(ma->pwdbox);
			gtk_widget_hide(ma->rempass);
		} else if ((p->options & OPT_PROTO_NO_PASSWORD) && !(q->options & OPT_PROTO_NO_PASSWORD)) {
			gtk_widget_show(ma->pwdbox);
			gtk_widget_show(ma->rempass);
		}
		if (!(p->options & OPT_PROTO_MAIL_CHECK) && (q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_show(ma->checkmail);
		} else if ((p->options & OPT_PROTO_MAIL_CHECK) && !(q->options & OPT_PROTO_MAIL_CHECK)) {
			gtk_widget_hide(ma->checkmail);
		}

		if (!(p->options & OPT_PROTO_BUDDY_ICON) && (q->options & OPT_PROTO_BUDDY_ICON)) {
			gtk_widget_show(ma->iconsel);
		} else if ((p->options & OPT_PROTO_BUDDY_ICON) && !(q->options & OPT_PROTO_BUDDY_ICON)) {
			gtk_widget_hide(ma->iconsel);
		}

		if ((q->options & OPT_PROTO_BUDDY_ICON) || (q->options & OPT_PROTO_MAIL_CHECK))
			gtk_widget_show(ma->user_frame);
		else
			gtk_widget_hide(ma->user_frame);

		ma->protocol = proto;
		generate_protocol_options(ma, ma->main);
	}
}

static GtkWidget *make_protocol_menu(GtkWidget *box, struct mod_account *ma)
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
		if (e->protocol == ma->protocol)
			found = TRUE;
		if (!found)
			count++;
		if (e->name)
			opt = gtk_menu_item_new_with_label(e->name);
		else
			opt = gtk_menu_item_new_with_label("Unknown");
		g_object_set_data(G_OBJECT(opt), "mod_account", ma);
		g_signal_connect(GTK_OBJECT(opt), "activate",
				   G_CALLBACK(set_prot), (void *)e->protocol);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
		gtk_widget_show(opt);
		p = p->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), count);

	return optmenu;
}

static void des_icon_sel(GtkWidget *w, struct mod_account *ma)
{
	w = ma->icondlg;
	if (ma->icondlg)
		ma->icondlg = NULL;
	if (w)
		gtk_widget_destroy(w);
}

static void set_icon(GtkWidget *w, struct mod_account *ma)
{
	GtkWidget *sel = ma->icondlg;
	const char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(sel));

	if (file_is_dir(file, sel))
		return;

	gtk_entry_set_text(GTK_ENTRY(ma->iconentry), file);
	g_snprintf(ma->iconfile, sizeof(ma->iconfile), "%s", file);
	ma->icondlg = NULL;

	gtk_widget_destroy(sel);
}

static void sel_icon_dlg(GtkWidget *w, struct mod_account *ma)
{
	GtkWidget *dlg;
	char buf[256];

	if (ma->icondlg) {
		gtk_widget_show(ma->icondlg);
		return;
	}

	dlg = gtk_file_selection_new(_("Load Buddy Icon"));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(dlg));
	if (ma->iconfile) {
		char *tmp = g_path_get_dirname(ma->iconfile);
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S, tmp);
		g_free(tmp);
	} else {
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S, gaim_home_dir());
	}
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(dlg), buf);

	g_signal_connect(GTK_OBJECT(dlg), "destroy", G_CALLBACK(des_icon_sel), ma);
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(dlg)->cancel_button), "clicked",
			   G_CALLBACK(des_icon_sel), ma);
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(dlg)->ok_button), "clicked",
			   G_CALLBACK(set_icon), ma);

	ma->icondlg = dlg;

	gtk_widget_show(dlg);
}

static void reset_icon(GtkWidget *w, struct mod_account *ma)
{
	ma->iconfile[0] = 0;
	gtk_entry_set_text(GTK_ENTRY(ma->iconentry), "");
}

static GtkWidget *build_icon_selection(struct mod_account *ma, GtkWidget *box)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *name;
	GtkWidget *browse;
	GtkWidget *reset;

	if (ma->account)
		g_snprintf(ma->iconfile, sizeof(ma->iconfile), "%s", ma->account->iconfile);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Buddy Icon File:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name), ma->iconfile);
	gtk_editable_set_editable(GTK_EDITABLE(name), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), name, TRUE, TRUE, 5);
	gtk_widget_show(name);
	ma->iconentry = name;

	browse = gtk_button_new_with_label(_("Browse"));
	g_signal_connect(GTK_OBJECT(browse), "clicked", G_CALLBACK(sel_icon_dlg), ma);
	gtk_box_pack_start(GTK_BOX(hbox), browse, FALSE, FALSE, 0);
	gtk_widget_show(browse);

	reset = gtk_button_new_with_label(_("Reset"));
	g_signal_connect(GTK_OBJECT(reset), "clicked", G_CALLBACK(reset_icon), ma);
	gtk_box_pack_start(GTK_BOX(hbox), reset, FALSE, FALSE, 0);
	gtk_widget_show(reset);

	return hbox;
}

static void generate_login_options(struct mod_account *ma, GtkWidget *box)
{
	GtkWidget *frame, *frame_parent;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;

	struct prpl *p;

	frame = make_frame(box, _("Login Options"));
	frame_parent = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Screenname:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	ma->name = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), ma->name, TRUE, TRUE, 0);

	ma->pwdbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), ma->pwdbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Password:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(ma->pwdbox), label, FALSE, FALSE, 0);

	ma->pass = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(ma->pwdbox), ma->pass, TRUE, TRUE, 0);
	gtk_entry_set_visibility(GTK_ENTRY(ma->pass), FALSE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Alias:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	ma->alias = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), ma->alias, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Protocol:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	make_protocol_menu(hbox, ma);

	ma->rempass = acct_button(_("Remember Password"), ma, OPT_ACCT_REM_PASS, vbox);
	acct_button(_("Auto-Login"), ma, OPT_ACCT_AUTO, vbox);

	gtk_widget_show_all(frame_parent);

	if (ma->account) {
		gtk_entry_set_text(GTK_ENTRY(ma->name), ma->account->username);
		gtk_entry_set_text(GTK_ENTRY(ma->alias), ma->account->alias);
		gtk_entry_set_text(GTK_ENTRY(ma->pass), ma->account->password);
	}

	p = find_prpl(ma->protocol);
	if (p && (p->options & OPT_PROTO_NO_PASSWORD)) {
		gtk_widget_hide(ma->pwdbox);
		gtk_widget_hide(ma->rempass);
	}
}

static void generate_user_options(struct mod_account *ma, GtkWidget *box)
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
	GtkWidget *frame;

	struct prpl *p = find_prpl(ma->protocol);

	frame = make_frame(box, _("User Options"));
	ma->user_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));
	gtk_widget_show_all(ma->user_frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	ma->checkmail = acct_button(_("New Mail Notifications"), ma, OPT_ACCT_MAIL_CHECK, vbox);
	ma->iconsel = build_icon_selection(ma, vbox);

	if (!p) {
		gtk_widget_hide(ma->user_frame);
		return;
	}

	if (!(p->options & OPT_PROTO_MAIL_CHECK))
		gtk_widget_hide(ma->checkmail);
	if (!(p->options & OPT_PROTO_BUDDY_ICON))
		gtk_widget_hide(ma->iconsel);

	if ((p->options & OPT_PROTO_BUDDY_ICON) || (p->options & OPT_PROTO_MAIL_CHECK))
		return;
	gtk_widget_hide(ma->user_frame);
}

static void generate_protocol_options(struct mod_account *ma, GtkWidget *box)
{
	struct prpl *p = find_prpl(ma->protocol);

	GList *op, *tmp;

	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *frame;

	char buf[256];

	if (ma->proto_frame)
		gtk_widget_destroy(ma->proto_frame);
	ma->proto_frame = NULL;

	if (ma->opt_entries) {
		g_list_free(ma->opt_entries);
		ma->opt_entries = NULL;
	}

	if (!p)
		return;

	if (!p->user_opts)
		return;

	tmp = op = p->user_opts;

	if (!op)
		return;

	g_snprintf(buf, sizeof(buf), _("%s Options"), p->name);
	frame = make_frame(box, buf);

	/* BLEH */
	ma->proto_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));
	gtk_widget_show_all(ma->proto_frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	while (op) {
		struct proto_user_opt *puo = op->data;

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		label = gtk_label_new(puo->label);
		gtk_size_group_add_widget(ma->sg, label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		entry = gtk_entry_new();
		gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(entry), "position", GINT_TO_POINTER(puo->pos));
		if (ma->proto_opt[puo->pos][0]) {
			debug_printf("setting text %s\n", ma->proto_opt[puo->pos]);
			gtk_entry_set_text(GTK_ENTRY(entry), ma->proto_opt[puo->pos]);
		} else {
			gtk_entry_set_text(GTK_ENTRY(entry), puo->def);
		}
		gtk_widget_show(entry);

		ma->opt_entries = g_list_append(ma->opt_entries, entry);

		op = op->next;
	}

	if(p->register_user != NULL) {
		ma->register_user = gtk_check_button_new_with_label(_("Register with server"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ma->register_user), FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), ma->register_user, FALSE, FALSE, 0);
		gtk_widget_show(ma->register_user);
	}

}

static void proxy_dropdown_set(GObject *w, struct mod_account *ma) {
	int opt = GPOINTER_TO_INT(g_object_get_data(w, "proxytype"));
	if(opt == PROXY_NONE || opt == PROXY_USE_GLOBAL)
		gtk_widget_hide_all(ma->proxy_host_box);
	else {
		gtk_widget_show_all(ma->proxy_host_box);
	}
}

static void generate_proxy_options(struct mod_account *ma, GtkWidget *box) {
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *menu;
	GtkWidget *dropdown;
	GtkWidget *opt;
	GtkWidget *entry;
	GtkWidget *vbox2;

	struct gaim_proxy_info *gpi = NULL;

	if(ma->account)
		gpi = ma->account->gpi;

	frame = make_frame(box, _("Proxy Options"));
	ma->proxy_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));
	gtk_widget_show_all(ma->proxy_frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* make the dropdown w/o the benefit of the easy helper funcs in prefs.c */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("Proxy _Type"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();

	opt = gtk_menu_item_new_with_label(_("Use Global Proxy Settings"));
	g_object_set_data(G_OBJECT(opt), "proxytype", GINT_TO_POINTER(PROXY_USE_GLOBAL));
	g_signal_connect(G_OBJECT(opt), "activate",
			G_CALLBACK(proxy_dropdown_set), ma);
	gtk_widget_show(opt);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	if(!gpi)
		gtk_menu_set_active(GTK_MENU(menu), 0);

	opt = gtk_menu_item_new_with_label(_("No Proxy"));
	g_object_set_data(G_OBJECT(opt), "proxytype", GINT_TO_POINTER(PROXY_NONE));
	g_signal_connect(G_OBJECT(opt), "activate",
			G_CALLBACK(proxy_dropdown_set), ma);
	gtk_widget_show(opt);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	if(gpi && gpi->proxytype == PROXY_NONE)
		gtk_menu_set_active(GTK_MENU(menu), 1);

	opt = gtk_menu_item_new_with_label("SOCKS 4");
	g_object_set_data(G_OBJECT(opt), "proxytype", GINT_TO_POINTER(PROXY_SOCKS4));
	g_signal_connect(G_OBJECT(opt), "activate",
			G_CALLBACK(proxy_dropdown_set), ma);
	gtk_widget_show(opt);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	if(gpi && gpi->proxytype == PROXY_SOCKS4)
		gtk_menu_set_active(GTK_MENU(menu), 2);

	opt = gtk_menu_item_new_with_label("SOCKS 5");
	g_object_set_data(G_OBJECT(opt), "proxytype", GINT_TO_POINTER(PROXY_SOCKS5));
	g_signal_connect(G_OBJECT(opt), "activate",
			G_CALLBACK(proxy_dropdown_set), ma);
	gtk_widget_show(opt);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	if(gpi && gpi->proxytype == PROXY_SOCKS5)
		gtk_menu_set_active(GTK_MENU(menu), 3);

	opt = gtk_menu_item_new_with_label("HTTP");
	g_object_set_data(G_OBJECT(opt), "proxytype", GINT_TO_POINTER(PROXY_HTTP));
	g_signal_connect(G_OBJECT(opt), "activate",
			G_CALLBACK(proxy_dropdown_set), ma);
	gtk_widget_show(opt);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	if(gpi && gpi->proxytype == PROXY_HTTP)
		gtk_menu_set_active(GTK_MENU(menu), 4);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);
	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);
	gtk_widget_show(dropdown);

	ma->proxytype_menu = menu;


	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), vbox2);
	gtk_widget_show(vbox2);
	ma->proxy_host_box = vbox2;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Host:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), gpi ? gpi->proxyhost : "");
	gtk_widget_show(entry);
	ma->proxyhost_entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("Port:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	if(gpi && gpi->proxyport) {
		char buf[128];
		g_snprintf(buf, sizeof(buf), "%d", gpi->proxyport);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
	}
	gtk_widget_show(entry);
	ma->proxyport_entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_User:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), gpi ? gpi->proxyuser : "");
	gtk_widget_show(entry);
	ma->proxyuser_entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("Pa_ssword:"));
	gtk_size_group_add_widget(ma->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), gpi ? gpi->proxypass : "");
	gtk_widget_show(entry);
	ma->proxypass_entry = entry;

	if(gpi == NULL || gpi->proxytype == PROXY_NONE)
		gtk_widget_hide_all(vbox2);
}

static void show_acct_mod(struct gaim_account *a)
{
	/* This is the fucking modify account dialog. I've fucking seperated it into
	 * three fucking frames:
	 * a fucking Login Options frame, a fucking User Options frame and a fucking
	 * Protcol Options frame. This fucking removes the two fucking tabs, which 
	 * were quite fucking uneccessary. Fuck. */
				/* -- SeanEgan */
				/* YEAH!! -- ChipX86 */
	GtkWidget *hbox, *vbox;
	GtkWidget *button;
	GtkWidget *sep;
	GtkSizeGroup *button_sg;

	struct mod_account *ma = mod_account_find(a);

	if (!ma) {
		ma = g_new0(struct mod_account, 1);
		ma->account = a;
		mod_accounts = g_slist_append(mod_accounts, ma);

		if (a) {
			int i;
			ma->options = a->options;
			if (find_prpl(a->protocol))
				ma->protocol = a->protocol;
			else if (protocols)
				ma->protocol = ((struct prpl *)protocols->data)->protocol;
			else
				ma->protocol = -1;
			g_snprintf(ma->iconfile, sizeof(ma->iconfile), "%s", a->iconfile);
			for (i = 0; i < 7; i++)
				g_snprintf(ma->proto_opt[i], sizeof(ma->proto_opt[i]), "%s",
						a->proto_opt[i]);
		} else {
			ma->options = OPT_ACCT_REM_PASS;
			if (find_prpl(DEFAULT_PROTO))
				ma->protocol = DEFAULT_PROTO;
			else if (protocols)
				ma->protocol = ((struct prpl *)protocols->data)->protocol;
			else
				ma->protocol = -1;
		}
	} else {
		gtk_widget_show(ma->mod);
		return;
	}

	ma->mod = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(ma->mod), "account");
	gtk_widget_realize(ma->mod);
	gtk_window_set_title(GTK_WINDOW(ma->mod), _("Modify Account"));
	gtk_window_set_resizable(GTK_WINDOW(ma->mod), FALSE);	/* nothing odd here :) */
	g_signal_connect(GTK_OBJECT(ma->mod), "destroy", G_CALLBACK(delmod), ma);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(ma->mod), vbox);
	gtk_widget_show(vbox);

	ma->main = gtk_vbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(ma->main), 6);
	gtk_box_pack_start(GTK_BOX(vbox), ma->main, FALSE, FALSE, 0);
	gtk_widget_show(ma->main);

	ma->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	generate_login_options(ma, ma->main);
	generate_user_options(ma, ma->main);
	generate_protocol_options(ma, ma->main);
	generate_proxy_options(ma, ma->main);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button_sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_size_group_add_widget(button_sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(ok_mod), ma);

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_size_group_add_widget(button_sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(cancel_mod), ma);

	sep = gtk_hseparator_new();
	gtk_box_pack_end (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	gtk_widget_show_all(hbox);
	gtk_widget_show(ma->mod);
}

static void add_acct(GtkWidget *w, gpointer d)
{
	show_acct_mod(NULL);
}

static void mod_acct_func(GtkTreeModel *model, GtkTreePath *path,
						  GtkTreeIter *iter, gpointer data)
{
	struct gaim_account *a;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &a, -1);

	if (a != NULL)
		show_acct_mod(a);
}

static void mod_acct(GtkWidget *w, gpointer d)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	gtk_tree_selection_selected_foreach(selection, mod_acct_func, NULL);
}

struct pass_prompt {
	struct gaim_account *account;
	GtkWidget *win;
	GtkWidget *entry;
};
static GSList *passes = NULL;

static struct pass_prompt *find_pass_prompt(struct gaim_account *account)
{
	GSList *p = passes;
	while (p) {
		struct pass_prompt *r = p->data;
		if (r->account == account)
			return r;
		p = p->next;
	}
	return NULL;
}

static void pass_callback(GtkDialog *d, gint resp, struct pass_prompt *p)
{
	if (resp == GTK_RESPONSE_YES) {	
		const char *txt = gtk_entry_get_text(GTK_ENTRY(p->entry));
		g_snprintf(p->account->password, sizeof(p->account->password), "%s", txt);
		serv_login(p->account);
	}
	passes = g_slist_remove(passes, p);
	gtk_widget_destroy(p->win);
	g_free(p);
}

static void do_pass_dlg(struct gaim_account *account)
{
	/* we can safely assume that u is not NULL */
	struct pass_prompt *p = find_pass_prompt(account);
	GtkWidget *label;
	GtkWidget *hbox, *vbox;
	char *labeltext=NULL;
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_auth.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);
	g_free(filename);


	if (p) {
		gtk_widget_show(p->win);
		return;
	}

	p = g_new0(struct pass_prompt, 1);
	p->account = account;
	passes = g_slist_append(passes, p);

	p->win = gtk_dialog_new_with_buttons("", NULL, 0, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						_("_Login"), GTK_RESPONSE_YES, NULL);

	gtk_dialog_set_default_response (GTK_DIALOG(p->win), GTK_RESPONSE_YES);
	g_signal_connect(G_OBJECT(p->win), "response", G_CALLBACK(pass_callback), p);

	gtk_container_set_border_width (GTK_CONTAINER(p->win), 6);
	gtk_window_set_resizable(GTK_WINDOW(p->win), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(p->win), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(p->win)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(p->win)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(p->win)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);	

	labeltext = g_strdup_printf(_("Please enter your password for %s.\n\n"),
			account->username);
	label = gtk_label_new(labeltext);
	g_free(labeltext);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	label = gtk_label_new_with_mnemonic(_("_Password"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	p->entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(p->entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), p->entry, FALSE, FALSE, 5);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), p->entry);
	gtk_widget_grab_focus(p->entry);

	gtk_widget_show_all(p->win);
}

static void acct_signin(GtkCellRendererToggle *cell, gchar *path_str,
						gpointer d)
{
	GtkTreeModel *model = (GtkTreeModel *)d;
	GtkTreeIter iter;

	struct gaim_account *account = NULL;
	struct prpl *p = NULL;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, COLUMN_DATA, &account, -1);

	p = find_prpl(account->protocol);
	if (!account->gc && p && p->login) {
		struct prpl *p = find_prpl(account->protocol);
		if (p && !(p->options & OPT_PROTO_NO_PASSWORD) &&
			!(p->options & OPT_PROTO_PASSWORD_OPTIONAL) && !account->password[0]) {
			do_pass_dlg(account);
		} else {
			serv_login(account);
		}
	} else if (account->gc) {
		account->gc->wants_to_die = TRUE;
		signoff(account->gc);
	} else {
		if (account->protocol == PROTO_TOC)
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
}

static void acct_autologin(GtkCellRendererToggle *cell, gchar *path_str,
						   gpointer d)
{
	GtkTreeModel *model = (GtkTreeModel *)d;
	GtkTreeIter iter;

	struct gaim_account *account = NULL;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, COLUMN_DATA, &account, -1);

	account->options ^= OPT_ACCT_AUTO;

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   COLUMN_AUTOLOGIN, (account->options & OPT_ACCT_AUTO), -1);

	save_prefs();
}

static void do_del_acct(struct gaim_account *account)
{
	GtkTreeIter iter;
	GaimBlistNode *gnode,*bnode;

	if (account->gc) {
		account->gc->wants_to_die = TRUE;
		signoff(account->gc);
	}

	if (get_iter_from_data(GTK_TREE_VIEW(treeview), account, &iter)) {
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}


	/* remove the buddies for the account we just destroyed */
	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		struct group *g = (struct group *)gnode;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			struct buddy *b = (struct buddy *)bnode;
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			if(b->account == account) {
				gaim_blist_remove_buddy(b);
			}
		}
		if(!gnode->child) {
			gaim_blist_remove_group(g);
		}
	}

	gaim_accounts = g_slist_remove(gaim_accounts, account);

	gaim_blist_save();

	save_prefs();
}

static void del_acct_func(GtkTreeModel *model, GtkTreePath *path,
						  GtkTreeIter *iter, gpointer data)
{
	struct gaim_account *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL) {
		char buf[8192];

		g_snprintf(buf, sizeof(buf),
				   _("Are you sure you want to delete %s?"), account->username);
		do_ask_dialog(buf, NULL, account, _("Delete"), do_del_acct, _("Cancel"), NULL, NULL, FALSE);
	}
}

static void del_acct(GtkWidget *w, gpointer d)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	gtk_tree_selection_selected_foreach(selection, del_acct_func, NULL);
}

void account_editor(GtkWidget *w, GtkWidget *W)
{
	/* please kill me */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *sw;
	GtkWidget *button;	/* used for many things */
	GtkWidget *sep;
	GtkSizeGroup *sg;

	if (acctedit) {
		gtk_window_present(GTK_WINDOW(acctedit));
		return;
	}
	
	acctedit = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(acctedit), _("Account Editor"));
	gtk_window_set_role(GTK_WINDOW(acctedit), "accounteditor");
	gtk_widget_realize(acctedit);
	gtk_widget_set_size_request(acctedit, -1, 250);
	gtk_window_set_default_size(GTK_WINDOW(acctedit), 550, 250);
	g_signal_connect(GTK_OBJECT(acctedit), "delete_event", G_CALLBACK(on_delete_acctedit), W);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(acctedit), vbox);

	sw = generate_list();
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 6);

	gtk_box_pack_start(GTK_BOX(hbox), sw, TRUE, TRUE, 0);

#if 0
	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_button_set_use_stock(GTK_BUTTON(button), FALSE);
	gtk_button_set_label(GTK_BUTTON(button), "Select All");
//	button = picture_button2(acctedit, _("Select All"), tb_refresh_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	g_signal_connect_swapped(GTK_OBJECT(button), "clicked",
				  G_CALLBACK(gtk_clist_select_all), GTK_OBJECT(list));

	button = gtk_button_new_from_stock(GTK_STOCK_REDO);
	gtk_button_set_use_stock(GTK_BUTTON(button), FALSE);
//	gtk_button_set_label(button, "Select Autos");
//	button = picture_button2(acctedit, _("Select Autos"), tb_redo_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(sel_auto), NULL);

	button = gtk_button_new_from_stock(GTK_STOCK_UNDO);
	gtk_button_set_use_stock(GTK_BUTTON(button), FALSE);
	gtk_button_set_label(GTK_BUTTON(button), "Select None");
//	button = picture_button2(acctedit, _("Select None"), tb_undo_xpm, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 0);
	g_signal_connect_swapped(GTK_OBJECT(button), "clicked",
				  G_CALLBACK(gtk_clist_unselect_all), GTK_OBJECT(list));

#endif

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(on_close_acctedit), W);

	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(del_acct), NULL);

	button = gaim_pixbuf_button_from_stock(_("_Modify"), GTK_STOCK_PREFERENCES,
										   GAIM_BUTTON_HORIZONTAL);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(mod_acct), NULL);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(add_acct), NULL);

	gtk_widget_show_all(acctedit);
}

struct signon_meter {
	struct gaim_connection *gc;
	GtkWidget *button;
	GtkWidget *progress;
	GtkWidget *status;
};
static GSList *meters = NULL;

GtkWidget* create_meter_pixmap (struct gaim_connection *gc)
{
	GdkPixbuf *pb = create_prpl_icon(gc->account);
	GdkPixbuf *scale = gdk_pixbuf_scale_simple(pb, 30,30,GDK_INTERP_BILINEAR);
	GtkWidget *image =
		gtk_image_new_from_pixbuf(scale);
	g_object_unref(G_OBJECT(pb));
	g_object_unref(G_OBJECT(scale));
	return image;
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
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress), 1);
	gtk_statusbar_pop(GTK_STATUSBAR(meter->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(meter->status), 1, _("Done."));
	meter_win->active_count--;
	if (meter_win->active_count == 0) {
		gtk_widget_destroy(meter_win->window);
		g_free (meter_win);
		meter_win = NULL;
	}
}

void account_online(struct gaim_connection *gc)
{
	struct signon_meter *meter = find_signon_meter(gc);
	GList *wins;
	GtkTreeIter iter;
	GaimBlistNode *gnode,*bnode;
	GList *add_buds=NULL;
	GList *l;

	/* Set the time the account came online */
	time(&gc->login_time);

	/* first we hide the login progress meter */
	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}

	/* then we do the buddy list stuff */
	if (mainwindow)
		gtk_widget_hide(mainwindow);

	gaim_blist_show();

	update_privacy_connections();
	do_away_menu();
	do_proto_menu();

	/*
	 * XXX This is a hack! Remove this and replace it with a better event
	 *     notification system.
	 */
	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		struct gaim_window *win = (struct gaim_window *)wins->data;
		gaim_conversation_update(gaim_window_get_conversation_at(win, 0),
								 GAIM_CONV_ACCOUNT_ONLINE);
	}

	gaim_setup(gc);

	gc->account->connecting = FALSE;
	connecting_count--;
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

	/* let the prpl know what buddies we pulled out of the local list */
	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			struct buddy *b = (struct buddy *)bnode;
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			if(b->account == gc->account) {
				add_buds = g_list_append(add_buds, b->name);
			}
		}
	}

	if(add_buds) {
		serv_add_buddies(gc, add_buds);
		g_list_free(add_buds);
	}

	serv_set_permit_deny(gc);

	/* everything for the account editor */
	if (!acctedit)
		return;

	if (get_iter_from_data(GTK_TREE_VIEW(treeview), gc->account, &iter)) {
		gtk_list_store_set(model, &iter,
						   COLUMN_ONLINE, TRUE,
						   COLUMN_PROTOCOL, gc->prpl->name,
						   -1);
	}

	/* Update the conversation windows that use this account. */
	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		struct gaim_conversation *conv = (struct gaim_conversation *)l->data;

		if (gaim_conversation_get_account(conv) == gc->account) {
			gaim_conversation_update(conv, GAIM_CONV_UPDATE_ACCOUNT);
		}
	}
}

void account_offline(struct gaim_connection *gc)
{
	struct signon_meter *meter = find_signon_meter(gc);
	GtkTreeIter iter;
	GList *l;

	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}
	debug_printf("Disconnecting. user = %p, gc = %p (%p)\n",
				 gc->account, gc->account->gc, gc);
	gc->account->gc = NULL;	/* wasn't that awkward? */

	if (!acctedit)
		return;

	if (get_iter_from_data(GTK_TREE_VIEW(treeview), gc->account, &iter)) {
		gtk_list_store_set(model, &iter, COLUMN_ONLINE, FALSE, -1);
	}

	/* Update the conversation windows that use this account. */
	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		struct gaim_conversation *conv = (struct gaim_conversation *)l->data;

		if (gaim_conversation_get_account(conv) == gc->account) {
			gaim_conversation_update(conv, GAIM_CONV_UPDATE_ACCOUNT);
		}
	}
}

void auto_login()
{
	GSList *accts = gaim_accounts;
	struct gaim_account *a = NULL;

	while (accts) {
		a = (struct gaim_account *)accts->data;
		if ((a->options & OPT_ACCT_AUTO) && (a->options & OPT_ACCT_REM_PASS)) {
			serv_login(a);
		}
		accts = accts->next;
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

	graphic = create_meter_pixmap(gc);

	nest_vbox = gtk_vbox_new (FALSE, 0);

	g_string_prepend(name_to_print, _("Signon: "));
	label = gtk_label_new (name_to_print->str);
	g_string_free(name_to_print, TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	meter->status = gtk_statusbar_new();
	gtk_widget_set_size_request(meter->status, 250, -1);

	meter->progress = gtk_progress_bar_new ();

	meter->button = gtk_button_new_with_label (_("Cancel"));
	g_signal_connect (GTK_OBJECT (meter->button), "clicked", G_CALLBACK (cancel_signon), meter);

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
		gtk_window_set_resizable(GTK_WINDOW(meter_win->window), FALSE);
		gtk_window_set_role(GTK_WINDOW(meter_win->window), "signon");
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->window), 5);
		gtk_window_set_title (GTK_WINDOW (meter_win->window), _("Signon"));
		gtk_widget_realize(meter_win->window);

		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (meter_win->window), GTK_WIDGET (vbox));

		meter_win->table = (GtkTable *) gtk_table_new (1 , 4, FALSE);
		gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (meter_win->table), FALSE, FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (meter_win->table), 5);
		gtk_table_set_row_spacings (GTK_TABLE (meter_win->table), 5);
		gtk_table_set_col_spacings (GTK_TABLE (meter_win->table), 10);
	
		cancel_button = gtk_button_new_with_label (_("Cancel All"));
		g_signal_connect_swapped (GTK_OBJECT (cancel_button), "clicked", G_CALLBACK (loop_cancel), NULL);
		gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (cancel_button), FALSE, FALSE, 0);
	
		g_signal_connect (GTK_OBJECT (meter_win->window), "delete_event", G_CALLBACK (meter_destroy), NULL);
		}
	
	if (!meter) {
		char buf[256];

		meter = register_meter(gc, GTK_WIDGET (meter_win->window), GTK_TABLE (meter_win->table), (gint *)  &meter_win->rows);
		meter->gc = gc;
		meters = g_slist_append(meters, meter);

		g_snprintf(buf, sizeof(buf), "%s Signing On (using %s)", gc->username, gc->prpl->name);
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress), howfar / LOGIN_STEPS);
	gtk_statusbar_pop(GTK_STATUSBAR(meter->status), 1);
	gtk_statusbar_push(GTK_STATUSBAR(meter->status), 1, message);
}

struct kick_dlg {
	struct gaim_account *account;
	GtkWidget *dlg;
};
static GSList *kicks = NULL;

static struct kick_dlg *find_kick_dlg(struct gaim_account *account)
{
	GSList *k = kicks;
	while (k) {
		struct kick_dlg *d = k->data;
		if (d->account == account)
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
	gchar *buf;
	struct kick_dlg *k = find_kick_dlg(gc->account);
	struct signon_meter *meter = find_signon_meter(gc);
	buf = g_strdup_printf(_("%s\n%s: %s"), full_date(), prologue, details);
	if (k)
		gtk_widget_destroy(k->dlg);
	k = g_new0(struct kick_dlg, 1);
	k->account = gc->account;
	k->dlg = do_error_dialog(title, buf, GAIM_ERROR);
	kicks = g_slist_append(kicks, k);
	g_signal_connect(GTK_OBJECT(k->dlg), "destroy", G_CALLBACK(set_kick_null), k);
	if (meter) {
		kill_meter(meter);
		meters = g_slist_remove(meters, meter);
		g_free(meter);
	}
	g_free(buf);
}

void hide_login_progress(struct gaim_connection *gc, char *why)
{
	gchar *buf;

	plugin_event(event_error, gc, why);
	buf = g_strdup_printf(_("%s was unable to sign on"), gc->username);
	hide_login_progress_common(gc, why, _("Signon Error"), buf);
	g_free(buf);
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

	plugin_event(event_error, gc, why);
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
	GList *wins;

	/* UI stuff */
	/* CONV XXX
	convo_menu_remove(gc);
	remove_icon_data(gc);
       	*/

	gaim_blist_remove_account(gc->account);

	/* core stuff */
	/* remove this here so plugins get a sensible count of connections */
	connections = g_slist_remove(connections, gc);
	debug_printf("date: %s\n", full_date());
	plugin_event(event_signoff, gc);
	system_log(log_signoff, gc, NULL, OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);
	/* set this in case the plugin died before really connecting.
	   do it after calling the plugins so they can determine if
	   this user was ever on-line or not */
	if (gc->account->connecting) {
		gc->account->connecting = FALSE;
		connecting_count--;
	}
	debug_printf("connecting_count: %d\n", connecting_count);
	serv_close(gc);

	/* more UI stuff */
	do_away_menu();
	do_proto_menu();

	/*
	 * XXX This is a hack! Remove this and replace it with a better event
	 *     notification system.
	 */
	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		struct gaim_window *win = (struct gaim_window *)wins->data;
		gaim_conversation_update(gaim_window_get_conversation_at(win, 0),
								 GAIM_CONV_ACCOUNT_OFFLINE);
	}

	update_privacy_connections();

	/* in, out, shake it all about */
	if (connections)
		return;

	destroy_all_dialogs();
	gaim_blist_destroy();

	show_login();
}

struct gaim_account *gaim_account_new(const char *name, int proto, int opts)
{
	struct gaim_account *account = g_new0(struct gaim_account, 1);
	g_snprintf(account->username, sizeof(account->username), "%s", name);
	g_snprintf(account->user_info, sizeof(account->user_info), "%s", DEFAULT_INFO);
	account->protocol = proto;
	account->options = opts;
	account->permit = NULL;
	account->deny = NULL;
	gaim_accounts = g_slist_append(gaim_accounts, account);

	if (treeview) {
		GtkTreeIter iter;

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter,
						   COLUMN_SCREENNAME, account->username,
						   COLUMN_ONLINE, (account->gc ? TRUE : FALSE),
						   COLUMN_AUTOLOGIN, (account->options & OPT_ACCT_AUTO),
						   COLUMN_PROTOCOL, proto_name(account->protocol),
						   COLUMN_DATA, account,
						   -1);
	}

	return account;
}
