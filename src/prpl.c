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

#include "prpl.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/close.xpm"
#include "pixmaps/register.xpm"

extern void toc_init(struct prpl *);
extern void oscar_init(struct prpl *);

GSList *protocols = NULL;

static GtkWidget *regdialog = NULL;
static GtkWidget *regbox = NULL;
static struct prpl *regprpl = NULL;

struct prpl *find_prpl(int prot)
{
	GSList *e = protocols;
	struct prpl *r;

	while (e) {
		r = (struct prpl *)e->data;
		if (r->protocol == prot)
			return r;
		e = e->next;
	}

	return NULL;
}

static gint proto_compare(struct prpl *a, struct prpl *b)
{
	/* neg if a before b, 0 if equal, pos if a after b */
	return a->protocol - b->protocol;
}

void load_protocol(proto_init pi)
{
	struct prpl *p = g_new0(struct prpl, 1);
	struct prpl *old;
	pi(p);
	if ((old = find_prpl(p->protocol)) == NULL)
		unload_protocol(old);
	protocols = g_slist_insert_sorted(protocols, p, (GCompareFunc)proto_compare);
	if (regdialog)
		gtk_widget_destroy(regdialog);
}

void unload_protocol(struct prpl *p)
{
	GSList *c = connections;
	struct gaim_connection *g;
	while (c) {
		g = (struct gaim_connection *)c->data;
		if (g->prpl == p) {
			char buf[256];
			g_snprintf(buf, sizeof buf, _("%s was using %s, which got removed."
						      " %s is now offline."), g->username,
				   (*p->name)(), g->username);
			do_error_dialog(buf, _("Disconnect"));
			signoff(g);
			c = connections;
		} else
			c = c->next;
	}
	protocols = g_slist_remove(protocols, p);
	g_free(p);
}

void static_proto_init()
{
	load_protocol(toc_init);
	load_protocol(oscar_init);
}

static void des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

static void rem_win(GtkWidget *a, GtkWidget *b)
{
	void *d = gtk_object_get_user_data(GTK_OBJECT(a));
	if (d)
		gtk_signal_disconnect_by_data(GTK_OBJECT(b), d);
	gtk_widget_destroy(b);
}

void do_ask_dialog(const char *text, void *data, void *doit, void *dont)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(window), "accept", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Accept?"));
	gtk_widget_realize(window);
	aol_icon(window->window);
	if (dont)
		gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(dont), data);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(button), data);
	if (doit)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(doit), data);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(rem_win), window);

	gtk_widget_show_all(window);
}

static void delete_reg(gpointer a, gpointer b)
{
	GtkWidget *tmp = regdialog;
	if (regdialog) {
		regdialog = NULL;
		gtk_widget_destroy(tmp);
	}
}

static void reg_prpl(gpointer a, struct prpl *p)
{
	while (GTK_BOX(regbox)->children)
		gtk_container_remove(GTK_CONTAINER(regbox),
				     ((GtkBoxChild *)GTK_BOX(regbox)->children->data)->widget);
	regprpl = p;
	(*regprpl->draw_new_user)(regbox);
}

static void do_reg(gpointer a, gpointer b)
{
	if (regprpl->do_new_user)
		(*regprpl->do_new_user)();
}

void register_user(gpointer a, gpointer b)
{
	GSList *pr = protocols;
	struct prpl *p = NULL, *q;
	GtkWidget *box;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GtkWidget *button;

	if (regdialog)
		return;

	while (pr) {
		p = pr->data;
		if (p->draw_new_user)
			break;
		pr = pr->next;
		p = NULL;
	}
	if (p == NULL)
		/* this should never happen because I said so. Hi mom. */
		return;
	pr = protocols;

	regdialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(regdialog), "registration", "Gaim");
	gtk_container_set_border_width(GTK_CONTAINER(regdialog), 5);
	gtk_window_set_title(GTK_WINDOW(regdialog), _("Gaim - New User Registration"));
	gtk_signal_connect(GTK_OBJECT(regdialog), "destroy", GTK_SIGNAL_FUNC(delete_reg), NULL);
	gtk_widget_realize(regdialog);
	aol_icon(regdialog->window);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(regdialog), box);
	gtk_widget_show(box);

	frame = gtk_frame_new(_("New User Registration"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Register new user for"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 5);
	gtk_widget_show(optmenu);

	menu = gtk_menu_new();

	while (pr) {
		q = pr->data;
		if (q->draw_new_user) {
			opt = gtk_menu_item_new_with_label((*q->name)());
			gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(reg_prpl), q);
			gtk_menu_append(GTK_MENU(menu), opt);
			gtk_widget_show(opt);
		}
		pr = pr->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
	regprpl = p;

	regbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), regbox, FALSE, FALSE, 5);
	gtk_widget_show(regbox);

	(*regprpl->draw_new_user)(regbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(regdialog, _("Close"), close_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(delete_reg), NULL);

	button = picture_button(regdialog, _("Register"), register_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_reg), NULL);

	gtk_widget_show(regdialog);
}
