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
#include "internal.h"

#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "server.h"

#include "gtkimhtml.h"
#include "gtkutils.h"
#include "ui.h"

#ifdef _WIN32
# include "wspell.h"
#endif

static GList *chatentries = NULL;
static GtkWidget *joinchat = NULL;
static GtkWidget *jc_vbox = NULL;
static GaimConnection *joinchatgc;

static void
do_join_chat()
{
	if (joinchat) {
		GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, g_free);
		GList *tmp;

		for (tmp = chatentries; tmp != NULL; tmp = tmp->next) {
			if (g_object_get_data(tmp->data, "is_spin")) {
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup_printf("%d",
							gtk_spin_button_get_value_as_int(tmp->data)));
			}
			else {
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup(gtk_entry_get_text(tmp->data)));
			}
		}

		serv_join_chat(joinchatgc, components);

		g_hash_table_destroy(components);

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

	list = GAIM_PLUGIN_PROTOCOL_INFO(joinchatgc->prpl)->chat_info(joinchatgc);

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
			g_object_set_data(G_OBJECT(spin), "is_spin", GINT_TO_POINTER(TRUE));
			g_object_set_data(G_OBJECT(spin), "identifier", pce->identifier);
			chatentries = g_list_append(chatentries, spin);
			gtk_widget_set_size_request(spin, 50, -1);
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
			g_object_set_data(G_OBJECT(entry), "identifier", pce->identifier);

			gtk_widget_show(entry);
		}

		g_free(pce);
	}

	g_list_free(list);
}

static void
joinchat_choose(GtkWidget *w, GaimConnection *g)
{
	if (joinchatgc == g)
		return;

	if(joinchatgc->account->protocol == g->account->protocol) {
		joinchatgc = g;
	} else {
		joinchatgc = g;
		rebuild_jc();
	}
}

static void
create_joinchat_menu(GtkWidget *box)
{
	GaimAccount *account;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GtkSizeGroup *sg;
	GList *c;
	GaimConnection *g;
	const char *proto_name;
	char *filename;
	char buf[2048];

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	joinchatgc = NULL;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		g = (GaimConnection *)c->data;

		plugin = g->prpl;

		if (plugin == NULL)
			continue;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		if (prpl_info == NULL || prpl_info->join_chat == NULL)
			continue;

		if (!joinchatgc)
			joinchatgc = g;

		account = gaim_connection_get_account(g);

		opt = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(opt), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		if (prpl_info != NULL) {
			proto_name = prpl_info->list_icon(NULL, NULL);
			g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
										"default", buf, NULL);
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (pixbuf != NULL) {
				/* Scale and insert the image */
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
												GDK_INTERP_BILINEAR);
				image = gtk_image_new_from_pixbuf(scale);

				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
			}
			else
				image = gtk_image_new();
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		g_snprintf(buf, sizeof(buf), "%s (%s)",
				   gaim_account_get_username(account), plugin->info->name);

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(opt), "gaim_connection", g);

		g_signal_connect(G_OBJECT(opt), "activate",
						 G_CALLBACK(joinchat_choose), g);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
		gtk_widget_show(opt);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);

	g_object_unref(sg);
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
	GList *c;
	GaimConnection *gc = NULL;

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat)
			break;

		gc = NULL;
	}

	if (gc == NULL) {
		gaim_notify_error(NULL, NULL,
						  _("You are not currently signed on with any "
							"protocols that have the ability to chat."),
						  NULL);

		return;
	}

	if (!joinchat) {
		GAIM_DIALOG(joinchat);
		gtk_window_set_role(GTK_WINDOW(joinchat), "joinchat");
		gtk_window_set_resizable(GTK_WINDOW(joinchat), TRUE);
		gtk_widget_realize(joinchat);
		g_signal_connect(G_OBJECT(joinchat), "delete_event",
				   G_CALLBACK(destroy_join_chat), joinchat);
		gtk_window_set_title(GTK_WINDOW(joinchat), _("Join Chat"));

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(joinchat), mainbox);

		frame = gaim_gtk_make_frame(mainbox, _("Buddy Chat"));

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
		joinchatgc = gaim_connections_get_all()->data;
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
