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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "prpl.h"

static GList *chatentries = NULL;
static GtkWidget *joinchat = NULL;
static GtkWidget *jc_vbox = NULL;
static struct gaim_connection *joinchatgc;

static void
do_join_chat()
{
	if (joinchat) {
		GList *data = NULL;
		GList *tmp;
		int *ival;
		char *sval;

		for (tmp = chatentries; tmp != NULL; tmp = tmp->next) {
			if (gtk_object_get_user_data(tmp->data)) {
				ival = g_new0(int, 1);
				*ival = gtk_spin_button_get_value_as_int(tmp->data);
				data = g_list_append(data, ival);
			}
			else {
				sval = g_strdup(gtk_entry_get_text(tmp->data));
				data = g_list_append(data, sval);
			}
		}

		serv_join_chat(joinchatgc, data);

		for (tmp = data; tmp != NULL; tmp = tmp->next)
			g_free(tmp->data);

		g_list_free(data);

		gtk_widget_destroy(joinchat);

		if (chatentries)
			g_list_free(chatentries);

		chatentries = NULL;
		joinchat = NULL;
	}
}

static void
rebuild_jc()
{
	GList *list, *tmp;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	if (!joinchatgc)
		return;

	while (GTK_BOX(jc_vbox)->children)
		gtk_container_remove(GTK_CONTAINER(jc_vbox),
				     ((GtkBoxChild *)GTK_BOX(jc_vbox)->children->data)->widget);

	if (chatentries)
		g_list_free(chatentries);

	chatentries = NULL;

	list = joinchatgc->prpl->chat_info(joinchatgc);

	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		GtkWidget *label;
		GtkWidget *rowbox;

		pce = tmp->data;

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(jc_vbox), rowbox, TRUE, TRUE, 0);
		gtk_widget_show(rowbox);

		label = gtk_label_new(pce->label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		if (pce->is_int) {
			GtkObject *adjust;
			GtkWidget *spin;
			adjust = gtk_adjustment_new(pce->min, pce->min,
										pce->max, 1, 10, 10);
			spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			gtk_object_set_user_data(GTK_OBJECT(spin), (void *)1);
			chatentries = g_list_append(chatentries, spin);
			gtk_widget_set_usize(spin, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), spin, FALSE, FALSE, 0);
			gtk_widget_show(spin);
		}
		else {
			GtkWidget *entry;

			entry = gtk_entry_new();
			gtk_box_pack_end(GTK_BOX(rowbox), entry, FALSE, FALSE, 0);

			chatentries = g_list_append(chatentries, entry);

			if (pce->def)
				gtk_entry_set_text(GTK_ENTRY(entry), pce->def);

			if (focus) {
				gtk_widget_grab_focus(entry);
				focus = FALSE;
			}

			g_signal_connect(G_OBJECT(entry), "activate",
							 G_CALLBACK(do_join_chat), NULL);

			gtk_widget_show(entry);
		}

		g_free(pce);
	}

	g_list_free(list);
}

static void
joinchat_choose(GtkWidget *w, struct gaim_connection *g)
{
	if (joinchatgc == g)
		return;

	joinchatgc = g;

	rebuild_jc();
}

static void
create_joinchat_menu(GtkWidget *box)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *c;
	struct gaim_connection *g;
	char buf[2048];

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	joinchatgc = NULL;

	for (c = connections; c != NULL; c = c->next) {
		g = (struct gaim_connection *)c->data;

		if (!g->prpl->join_chat)
			continue;

		if (!joinchatgc)
			joinchatgc = g;

		g_snprintf(buf, sizeof(buf), "%s (%s)", g->username, g->prpl->name);
		opt = gtk_menu_item_new_with_label(buf);

		gtk_object_set_user_data(GTK_OBJECT(opt), g);

		g_signal_connect(G_OBJECT(opt), "activate",
						 G_CALLBACK(joinchat_choose), g);

		gtk_menu_append(GTK_MENU(menu), opt);
		gtk_widget_show(opt);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
}

static void
destroy_join_chat()
{
	if (joinchat)
		gtk_widget_destroy(joinchat);

	joinchat = NULL;
}

void
join_chat()
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *rowbox;
	GtkWidget *bbox;
	GtkWidget *join;
	GtkWidget *cancel;
	GtkWidget *label;
	GtkWidget *sep;
	GSList *c;
	struct gaim_connection *gc = NULL;

	for (c = connections; c != NULL; c = c->next) {
		gc = c->data;

		if (gc->prpl->join_chat)
			break;

		gc = NULL;
	}

	if (gc == NULL) {
		do_error_dialog(
			_("You are not currently signed on with any protocols that have "
			  "the ability to chat."), NULL, GAIM_ERROR);

		return;
	}

	if (!joinchat) {
		GAIM_DIALOG(joinchat);
		gtk_window_set_role(GTK_WINDOW(joinchat), "joinchat");
		gtk_window_set_policy(GTK_WINDOW(joinchat), FALSE, TRUE, TRUE);
		gtk_widget_realize(joinchat);
		g_signal_connect(G_OBJECT(joinchat), "delete_event",
				   G_CALLBACK(destroy_join_chat), joinchat);
		gtk_window_set_title(GTK_WINDOW(joinchat), _("Join Chat"));

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(joinchat), mainbox);

		frame = make_frame(mainbox, _("Buddy Chat"));

		fbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

#ifndef NO_MULTI
		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);

		label = gtk_label_new(_("Join Chat As:"));
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		create_joinchat_menu(rowbox);

		jc_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(fbox), jc_vbox);
		gtk_container_set_border_width(GTK_CONTAINER(jc_vbox), 0);

#else
		joinchatgc = connections->data;
#endif
		rebuild_jc();
		/* buttons */

		sep = gtk_hseparator_new();
		gtk_widget_show(sep);
		gtk_box_pack_start(GTK_BOX(mainbox), sep, FALSE, FALSE, 0);

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		/* Join button. */
		join = gaim_pixbuf_button_from_stock(_("Join"), GTK_STOCK_JUMP_TO,
							 GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(bbox), join, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(join), "clicked",
				 G_CALLBACK(do_join_chat), NULL);
		/* Cancel button. */
		cancel = gaim_pixbuf_button_from_stock(_("Cancel"), GTK_STOCK_CANCEL,
							 GAIM_BUTTON_HORIZONTAL);
		gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(cancel), "clicked",
				 G_CALLBACK(destroy_join_chat), joinchat);

	}

	gtk_widget_show_all(joinchat);
}
