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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "prpl.h"
#include "gtkimhtml.h"
#include "pixmaps/join.xpm"

GtkWidget *imaway = NULL;

GtkWidget *awaymenu = NULL;
GtkWidget *clistqueue = NULL;
GtkWidget *clistqueuesw;

struct away_message *awaymessage = NULL;
struct away_message *default_away;
int auto_away;

static void destroy_im_away()
{
	if (imaway)
		gtk_widget_destroy(imaway);

	clistqueue = NULL;
	clistqueuesw = NULL;
	imaway = NULL;
}

void purge_away_queue(GSList *queue)
{
	struct conversation *cnv;

	gtk_clist_freeze(GTK_CLIST(clistqueue));
	gtk_clist_clear(GTK_CLIST(clistqueue));

	while (queue) {
		struct queued_message *qm = queue->data;

		cnv = find_conversation(qm->name);
		if (!cnv)
			cnv = new_conversation(qm->name);
		if (g_slist_index(connections, qm->gc) >= 0)
			set_convo_gc(cnv, qm->gc);
		write_to_conv(cnv, qm->message, qm->flags, NULL, qm->tm, qm->len);

		queue = g_slist_remove(queue, qm);

		g_free(qm->message);
		g_free(qm);
	}

	gtk_clist_thaw(GTK_CLIST(clistqueue));
}

void dequeue_by_buddy(GtkWidget *clist, gint row, gint column, GdkEventButton *event, gpointer data) {
	char *temp;
	char *name;
	GSList *templist;
	struct conversation *cnv;
	
	if(!(event->type == GDK_2BUTTON_PRESS && event->button == 1))
		return; /* Double clicking on the clist will unqueue that users messages. */
	
	gtk_clist_get_text(GTK_CLIST(clist), row, 0, &temp);
	name = g_strdup(temp);

	if (!name)
		return;
	debug_printf("Unqueueing messages from %s.\n", name);
	templist = message_queue;
	while (templist) {
		struct queued_message *qm = templist->data;
		if (templist->data) {
			if (!g_strcasecmp(qm->name, name)) {
				cnv = find_conversation(name);
				if (!cnv)
					cnv = new_conversation(qm->name);
				if (g_slist_index(connections, qm->gc) >= 0)
					set_convo_gc(cnv, qm->gc);
				
				write_to_conv(cnv, qm->message, qm->flags, NULL, qm->tm, qm->len);
				g_free(qm->message);
				g_free(qm);
				templist = message_queue = g_slist_remove(message_queue, qm);
				
			} else {
				templist = templist->next;
			}
		}
	}
	g_free(name);
	gtk_clist_remove(GTK_CLIST(clist), row);

}
	
	


void toggle_away_queue()
{
	if (!clistqueue || !clistqueuesw)
		return;

	if (away_options & OPT_AWAY_QUEUE) {
		gtk_widget_show(clistqueue);
		gtk_widget_show(clistqueuesw);
	} else {
		gtk_widget_hide(clistqueue);
		gtk_widget_hide(clistqueuesw);
		purge_away_queue(message_queue);
	}
}

void do_im_back(GtkWidget *w, GtkWidget *x)
{
	if (imaway) {
		GtkWidget *tmp = imaway;

		purge_away_queue(message_queue);

		imaway = NULL;
		gtk_widget_destroy(tmp);
		if (w != tmp)
			return;
	}

	while (away_time_queue) {
		struct queued_away_response *qar = away_time_queue->data;
		away_time_queue = g_slist_remove(away_time_queue, qar);
		g_free(qar);
	}

	awaymessage = NULL;
	clistqueue = NULL;
	clistqueuesw = NULL;
	serv_set_away_all(NULL);
}


void do_away_message(GtkWidget *w, struct away_message *a)
{
	GtkWidget *back;
	GtkWidget *awaytext;
	GtkWidget *sw;
	GtkWidget *vbox;
	char *buf2;
	char *buf;

	if (!blist)
		return;

	if (!a)
		return;

	if (!imaway) {
		GAIM_DIALOG(imaway);
		gtk_window_set_wmclass(GTK_WINDOW(imaway), "imaway", "Gaim");
		if (strlen(a->name))
			 gtk_window_set_title(GTK_WINDOW(imaway), a->name);
		else
			gtk_window_set_title(GTK_WINDOW(imaway), _("Gaim - Away!"));
		gtk_signal_connect(GTK_OBJECT(imaway), "destroy", GTK_SIGNAL_FUNC(do_im_back), imaway);
		gtk_widget_realize(imaway);

		vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(imaway), vbox);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
		gtk_widget_show(vbox);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
					       GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
		gtk_widget_set_usize(sw, 245, 120);
		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
		gtk_widget_show(sw);

		awaytext = gtk_imhtml_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(sw), awaytext);
		gaim_setup_imhtml(awaytext);
		gtk_widget_show(awaytext);
		buf = stylize(a->message, BUF_LONG);
		gtk_imhtml_append_text(GTK_IMHTML(awaytext), buf, -1, GTK_IMHTML_NO_TITLE |
				       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
		g_free(buf);
		gtk_imhtml_append_text(GTK_IMHTML(awaytext), "<BR>", -1, GTK_IMHTML_NO_TITLE |
				       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);

		clistqueuesw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clistqueuesw), GTK_POLICY_NEVER,
					       GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(vbox), clistqueuesw, TRUE, TRUE, 0);

		clistqueue = gtk_clist_new(2);
		gtk_clist_set_column_width(GTK_CLIST(clistqueue), 0, 100);
		gtk_widget_set_usize(GTK_WIDGET(clistqueue), -1, 50);
		gtk_container_add(GTK_CONTAINER(clistqueuesw), clistqueue);
		gtk_signal_connect(GTK_OBJECT(clistqueue), "select_row", GTK_SIGNAL_FUNC(dequeue_by_buddy), NULL);



		if (away_options & OPT_AWAY_QUEUE) {
			gtk_widget_show(clistqueuesw);
			gtk_widget_show(clistqueue);
		}

		back = picture_button(imaway, _("I'm Back!"), join_xpm);
		gtk_box_pack_start(GTK_BOX(vbox), back, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(back), "clicked", GTK_SIGNAL_FUNC(do_im_back), imaway);
		gtk_window_set_focus(GTK_WINDOW(imaway), back);

		awaymessage = a;

	} else {
		destroy_im_away();
		do_away_message(w, a);
		return;
	}

	/* New away message... Clear out the old sent_aways */
	while (away_time_queue) {
		struct queued_away_response *qar = away_time_queue->data;
		away_time_queue = g_slist_remove(away_time_queue, qar);
		g_free(qar);
	}

	gtk_widget_show(imaway);
	buf2 = g_malloc(strlen(awaymessage->message) * 4 + 1);
	strncpy_withhtml(buf2, awaymessage->message, strlen(awaymessage->message) * 4 + 1);
	serv_set_away_all(buf2);
	g_free(buf2);
}

void rem_away_mess(GtkWidget *w, struct away_message *a)
{
	int default_index;
	default_index = g_slist_index(away_messages, default_away);
	if (default_index == -1) {
		if (away_messages != NULL)
			default_away = away_messages->data;
		else
			default_away = NULL;
	}
	away_messages = g_slist_remove(away_messages, a);
	g_free(a);
	do_away_menu();
	save_prefs();
}

static void set_gc_away(GtkObject *obj, struct gaim_connection *gc)
{
	struct away_message *awy = gtk_object_get_user_data(obj);

	if (awy)
		serv_set_away(gc, GAIM_AWAY_CUSTOM, awy->message);
	else
		serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
}

static void set_gc_state(GtkObject *obj, struct gaim_connection *gc)
{
	char *awy = gtk_object_get_user_data(obj);

	serv_set_away(gc, awy, NULL);
}

void do_away_menu()
{
	GtkWidget *menuitem;
	GtkWidget *remmenu;
	GtkWidget *submenu, *submenu2;
	GtkWidget *remitem;
	GtkWidget *label;
	GtkWidget *sep;
	GList *l;
	GtkWidget *list_item;
	GSList *awy = away_messages;
	struct away_message *a;
	GSList *con = connections;
	struct gaim_connection *gc = NULL;
	int count = 0;

	if (prefs_away_list != NULL) {
		GtkWidget *hbox;
		gtk_list_clear_items(GTK_LIST(prefs_away_list), 0, -1);
		while (awy) {
			a = (struct away_message *)awy->data;
			list_item = gtk_list_item_new();
			gtk_container_add(GTK_CONTAINER(prefs_away_list), list_item);
			gtk_signal_connect(GTK_OBJECT(list_item), "select",
					   GTK_SIGNAL_FUNC(away_list_clicked), a);
			gtk_object_set_user_data(GTK_OBJECT(list_item), a);
			gtk_widget_show(list_item);

			hbox = gtk_hbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(list_item), hbox);
			gtk_widget_show(hbox);

			label = gtk_label_new(a->name);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
			gtk_widget_show(label);

			awy = g_slist_next(awy);
		}
		if (away_messages != NULL)
			gtk_list_select_item(GTK_LIST(prefs_away_list), 0);
	}

	if (awaymenu) {
		l = gtk_container_children(GTK_CONTAINER(awaymenu));

		while (l) {
			gtk_container_remove(GTK_CONTAINER(awaymenu), GTK_WIDGET(l->data));
			l = l->next;
		}


		remmenu = gtk_menu_new();

		menuitem = gtk_menu_item_new_with_label(_("New Away Message"));
		gtk_menu_append(GTK_MENU(awaymenu), menuitem);
		gtk_widget_show(menuitem);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(create_away_mess),
				   NULL);

		awy = away_messages;
		while (awy) {
			a = (struct away_message *)awy->data;

			remitem = gtk_menu_item_new_with_label(a->name);
			gtk_menu_append(GTK_MENU(remmenu), remitem);
			gtk_widget_show(remitem);
			gtk_signal_connect(GTK_OBJECT(remitem), "activate",
					   GTK_SIGNAL_FUNC(rem_away_mess), a);

			awy = g_slist_next(awy);

		}

		menuitem = gtk_menu_item_new_with_label(_("Remove Away Message"));
		gtk_menu_append(GTK_MENU(awaymenu), menuitem);
		gtk_widget_show(menuitem);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), remmenu);
		gtk_widget_show(remmenu);

		sep = gtk_hseparator_new();
		menuitem = gtk_menu_item_new();
		gtk_menu_append(GTK_MENU(awaymenu), menuitem);
		gtk_container_add(GTK_CONTAINER(menuitem), sep);
		gtk_widget_set_sensitive(menuitem, FALSE);
		gtk_widget_show(menuitem);
		gtk_widget_show(sep);

		while (con) {
			gc = con->data;
			if (gc->prpl->away_states &&gc->prpl->set_away)
				count++;
			con = g_slist_next(con);
		}
		con = connections;

		if (count == 0) {
		} else if (count == 1) {
			GList *msgs, *tmp;
			while (con) {
				gc = con->data;
				if (gc->prpl->away_states &&gc->prpl->set_away)
					break;
				con = g_slist_next(con);
			}

			tmp = msgs = gc->prpl->away_states(gc);

			if ((g_list_length(msgs) == 1) && !strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
				awy = away_messages;

				while (awy) {
					a = (struct away_message *)awy->data;

					menuitem = gtk_menu_item_new_with_label(a->name);
					gtk_object_set_user_data(GTK_OBJECT(menuitem), a);
					gtk_menu_append(GTK_MENU(awaymenu), menuitem);
					gtk_widget_show(menuitem);
					gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
							   GTK_SIGNAL_FUNC(do_away_message), a);

					awy = g_slist_next(awy);
				}
			} else
				while (msgs) {
					awy = away_messages;

					menuitem = gtk_menu_item_new_with_label(msgs->data);
					gtk_object_set_user_data(GTK_OBJECT(menuitem), msgs->data);
					gtk_menu_append(GTK_MENU(awaymenu), menuitem);
					gtk_widget_show(menuitem);

					if (strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
						gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
								   GTK_SIGNAL_FUNC(set_gc_state), gc);
					} else {
						submenu = gtk_menu_new();
						gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
									  submenu);
						gtk_widget_show(submenu);

						while (awy) {
							a = (struct away_message *)awy->data;

							menuitem = gtk_menu_item_new_with_label(a->name);
							gtk_object_set_user_data(GTK_OBJECT(menuitem),
										 a);
							gtk_menu_append(GTK_MENU(submenu), menuitem);
							gtk_widget_show(menuitem);
							gtk_signal_connect(GTK_OBJECT(menuitem),
									   "activate",
									   GTK_SIGNAL_FUNC
									   (do_away_message), a);

							awy = g_slist_next(awy);
						}
					}
					msgs = g_list_next(msgs);
				}

			g_list_free(tmp);
		} else {
			while (con) {
				char buf[256];
				GList *msgs, *tmp;
				gc = con->data;

				if (!gc->prpl->away_states ||!gc->prpl->set_away) {
					con = con->next;
					continue;
				}

				g_snprintf(buf, sizeof(buf), "%s (%s)",
					   gc->username, gc->prpl->name);
				menuitem = gtk_menu_item_new_with_label(buf);
				gtk_menu_append(GTK_MENU(awaymenu), menuitem);
				gtk_widget_show(menuitem);

				submenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
				gtk_widget_show(submenu);

				tmp = msgs = gc->prpl->away_states(gc);

				if ((g_list_length(msgs) == 1) &&
				    (!strcmp(msgs->data, GAIM_AWAY_CUSTOM))) {
					menuitem = gtk_menu_item_new_with_label(_("Back"));
					gtk_menu_append(GTK_MENU(submenu), menuitem);
					gtk_widget_show(menuitem);
					gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
							   GTK_SIGNAL_FUNC(set_gc_away), gc);

					sep = gtk_hseparator_new();
					menuitem = gtk_menu_item_new();
					gtk_menu_append(GTK_MENU(submenu), menuitem);
					gtk_container_add(GTK_CONTAINER(menuitem), sep);
					gtk_widget_set_sensitive(menuitem, FALSE);
					gtk_widget_show(menuitem);
					gtk_widget_show(sep);

					awy = away_messages;

					while (awy) {
						a = (struct away_message *)awy->data;

						menuitem = gtk_menu_item_new_with_label(a->name);
						gtk_object_set_user_data(GTK_OBJECT(menuitem), a);
						gtk_menu_append(GTK_MENU(submenu), menuitem);
						gtk_widget_show(menuitem);
						gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
								   GTK_SIGNAL_FUNC(set_gc_away), gc);

						awy = g_slist_next(awy);
					}
				} else
					while (msgs) {
						awy = away_messages;

						menuitem = gtk_menu_item_new_with_label(msgs->data);
						gtk_object_set_user_data(GTK_OBJECT(menuitem),
									 msgs->data);
						gtk_menu_append(GTK_MENU(submenu), menuitem);
						gtk_widget_show(menuitem);

						if (strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
							gtk_signal_connect(GTK_OBJECT(menuitem),
									   "activate",
									   GTK_SIGNAL_FUNC(set_gc_state),
									   gc);
						} else {
							submenu2 = gtk_menu_new();
							gtk_menu_item_set_submenu(GTK_MENU_ITEM
										  (menuitem), submenu2);
							gtk_widget_show(submenu2);

							while (awy) {
								a = (struct away_message *)awy->data;

								menuitem =
								    gtk_menu_item_new_with_label(a->
												 name);
								gtk_object_set_user_data(GTK_OBJECT
											 (menuitem), a);
								gtk_menu_append(GTK_MENU(submenu2),
										menuitem);
								gtk_widget_show(menuitem);
								gtk_signal_connect(GTK_OBJECT(menuitem),
										   "activate",
										   GTK_SIGNAL_FUNC
										   (set_gc_away), gc);

								awy = g_slist_next(awy);
							}
						}
						msgs = g_list_next(msgs);
					}

				g_list_free(tmp);

				con = g_slist_next(con);
			}

			menuitem = gtk_menu_item_new_with_label(_("Set All Away"));
			gtk_menu_append(GTK_MENU(awaymenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);

			awy = away_messages;

			while (awy) {
				a = (struct away_message *)awy->data;

				menuitem = gtk_menu_item_new_with_label(a->name);
				gtk_object_set_user_data(GTK_OBJECT(menuitem), a);
				gtk_menu_append(GTK_MENU(submenu), menuitem);
				gtk_widget_show(menuitem);
				gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
						   GTK_SIGNAL_FUNC(do_away_message), a);

				awy = g_slist_next(awy);
			}
		}
	}
	if (prefs_away_menu) {
		l = gtk_container_children(GTK_CONTAINER(prefs_away_menu));
		while (l) {
			gtk_widget_destroy(GTK_WIDGET(l->data));
			l = l->next;
		}
		gtk_widget_hide(GTK_WIDGET(prefs_away_menu));
		default_away_menu_init(GTK_WIDGET(prefs_away_menu));
		gtk_widget_show(prefs_away_menu);
	}

}
