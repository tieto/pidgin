/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "plugin.h"
#include "prefs.h"
#include "prpl.h"
#include "status.h"
#include "util.h"
#include "request.h"

#if 0
/* XXX CORE/UI: Until we can get rid of the message queue stuff... */
#include "away.h"
#include "gaim.h"
#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtkprefs.h"
#include "gtkutils.h"

GtkWidget *imaway = NULL;
GtkWidget *awaymenu = NULL;
GtkWidget *awayqueue = NULL;
GtkListStore *awayqueuestore = NULL;
GtkWidget *awayqueuesw;

GSList *message_queue = NULL;
GSList *unread_message_queue = NULL;

GSList *away_messages = NULL;
struct away_message *awaymessage = NULL;

static void dequeue_message(GtkTreeIter *iter)
{
	gchar *name;
	GSList *templist;
	GaimConversation *cnv;
	gboolean orig_while_away;

	orig_while_away = gaim_prefs_get_bool("/core/sound/while_away");
	if (orig_while_away)
		gaim_prefs_set_bool("/core/sound/while_away", FALSE);

	gtk_tree_model_get(GTK_TREE_MODEL(awayqueuestore), iter, 0, &name, -1);

	gaim_debug(GAIM_DEBUG_INFO, "away", "Dequeueing messages from %s.\n",
			   name);

	templist = message_queue;

	while (templist) {
		struct queued_message *qm = templist->data;
		if (templist->data) {
			if (!gaim_utf8_strcasecmp(qm->name, name)) {
				GaimAccount *account = NULL;

				if (g_list_index(gaim_accounts_get_all(), qm->account) >= 0)
					account = qm->account;

				cnv = gaim_find_conversation_with_account(name, account);

				if (!cnv)
					cnv = gaim_conversation_new(GAIM_CONV_IM, account, qm->name);
				else
					gaim_conversation_set_account(cnv, account);

				gaim_conv_im_write(GAIM_CONV_IM(cnv), NULL, qm->message,
						qm->flags, qm->tm);
				g_free(qm->message);
				g_free(qm);
				templist = message_queue = g_slist_remove(message_queue, qm);

			} else {
				templist = templist->next;
			}
		}
	}

	g_free(name);
	/* In GTK 2.2, _store_remove actually returns whether iter is valid or not
	 * after the remove, but in GTK 2.0 it is a void function. */
	gtk_list_store_remove(awayqueuestore, iter);

	if (orig_while_away)
		gaim_prefs_set_bool("/core/sound/while_away", orig_while_away);
}

void purge_away_queue(GSList **queue)
{
	GSList *q = *queue;
	struct queued_message *qm;
	GaimConversation *cnv;
	GaimAccount *account;
	gboolean orig_while_away;

	orig_while_away = gaim_prefs_get_bool("/core/sound/while_away");
	if (orig_while_away)
		gaim_prefs_set_bool("/core/sound/while_away", FALSE);

	while (q) {
		qm = q->data;

		account = NULL;

		if (g_list_index(gaim_accounts_get_all(), qm->account) >= 0)
			account = qm->account;

		cnv = gaim_find_conversation_with_account(qm->name, account);

		if (!cnv)
			cnv = gaim_conversation_new(GAIM_CONV_IM, account, qm->name);
		else
			gaim_conversation_set_account(cnv, account);

		gaim_conv_im_write(GAIM_CONV_IM(cnv), NULL, qm->message, qm->flags, qm->tm);

		g_free(qm->message);
		g_free(qm);

		q->data = NULL;
		q = q->next;
	}

	g_slist_free(*queue);
	*queue = NULL;

	if (orig_while_away)
		gaim_prefs_set_bool("/core/sound/while_away", orig_while_away);
}

gint dequeue_cb(GtkWidget *treeview, GdkEventButton *event, gpointer data) {
	GtkTreeIter iter;
	GtkTreeSelection *select;

	if(!(event->type == GDK_2BUTTON_PRESS && event->button == 1))
		return FALSE; /* Double clicking on the list will dequeue that user's messages. */

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if(gtk_tree_selection_get_selected(select, NULL, &iter))
			dequeue_message(&iter);

	return FALSE;
}



void toggle_away_queue()
{
	if (!awayqueue || !awayqueuesw)
		return;

	if (gaim_prefs_get_bool("/gaim/gtk/away/queue_messages")) {
		gtk_widget_show(awayqueue);
		gtk_widget_show(awayqueuesw);
	} else {
		gtk_widget_hide(awayqueue);
		gtk_widget_hide(awayqueuesw);
		purge_away_queue(&message_queue);
	}
}

void do_im_back(GtkWidget *w, GtkWidget *x)
{
	if (imaway) {
		GtkWidget *tmp = imaway;

		purge_away_queue(&message_queue);

		imaway = NULL;
		gtk_widget_destroy(tmp);
		if (w != tmp)
			return;
	}

	awaymessage = NULL;
	awayqueue = NULL;
	awayqueuesw = NULL;
	if (awayqueuestore != NULL)
		g_object_unref(G_OBJECT(awayqueuestore));
	awayqueuestore = NULL;
	serv_set_away_all(NULL);
}


void do_away_message(GtkWidget *w, struct away_message *a)
{
	GtkWidget *back;
	GtkWidget *edit;
	GtkWidget *awaytext;
	GtkWidget *sw;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	char *buf;

	if (!a)
		return;

	if(imaway)
		gtk_widget_destroy(imaway);

	GAIM_DIALOG(imaway);
	gtk_window_set_role(GTK_WINDOW(imaway), "imaway");
	if (strlen(a->name))
		gtk_window_set_title(GTK_WINDOW(imaway), a->name);
	else
		gtk_window_set_title(GTK_WINDOW(imaway), _("Away!"));
	g_signal_connect(G_OBJECT(imaway), "destroy",
			G_CALLBACK(do_im_back), imaway);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(imaway), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
	gtk_widget_show(hbox);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_widget_set_size_request(sw, 245, 120);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	awaytext = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), awaytext);
	gaim_setup_imhtml(awaytext);
	gtk_widget_show(awaytext);
	buf = stylize(a->message, BUF_LONG);
	gtk_imhtml_append_text(GTK_IMHTML(awaytext), buf, GTK_IMHTML_NO_TITLE |
			GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	g_free(buf);
	gtk_imhtml_append_text(GTK_IMHTML(awaytext), "<BR>",
			GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_COMMENTS |
			GTK_IMHTML_NO_SCROLL);

	awayqueuesw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(awayqueuesw),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(awayqueuesw),
			GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), awayqueuesw, TRUE, TRUE, 0);

	awayqueuestore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	awayqueue = gtk_tree_view_new_with_model(GTK_TREE_MODEL(awayqueuestore));
	renderer = gtk_cell_renderer_text_new();

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(awayqueue), FALSE);
	column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
			"text", 0,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(awayqueue), column);
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 1,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(awayqueue), column);
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"text", 2,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(awayqueue), column);

	gtk_container_add(GTK_CONTAINER(awayqueuesw), awayqueue);

	g_signal_connect(G_OBJECT(awayqueue), "button_press_event", G_CALLBACK(dequeue_cb), NULL);


	if (gaim_prefs_get_bool("/gaim/gtk/away/queue_messages")) {
		gtk_widget_show(awayqueuesw);
		gtk_widget_show(awayqueue);
	}

	awaymessage = a;

	edit = gaim_pixbuf_button_from_stock(_("Edit This Message"), GTK_STOCK_CONVERT, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(hbox), edit, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(edit), "clicked", G_CALLBACK(create_away_mess), awaymessage);
	gtk_widget_show(edit);

	back = gaim_pixbuf_button_from_stock(_("I'm Back!"), GTK_STOCK_JUMP_TO, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(hbox), back, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(back), "clicked", G_CALLBACK(do_im_back), imaway);
	gtk_window_set_focus(GTK_WINDOW(imaway), back);
	gtk_widget_show(back);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(imaway);
	serv_set_away_all(awaymessage->message);
}

void do_rem_away_mess(gchar *name)
{
	struct away_message *a = NULL;
	struct away_message *default_away = NULL;
	const char *default_away_name;
	GSList *l;

	/* Lookup the away message based on the title */
	for (l = away_messages; l != NULL; l = l->next) {
		a = l->data;
		if (!strcmp(a->name, name))
			break;
	}
	g_free(name);

	if (l == NULL || a == NULL) {
		/* Could not find away message! */
		return;
	}

	default_away_name = gaim_prefs_get_string("/core/away/default_message");

	for(l = away_messages; l; l = l->next) {
		if(!strcmp(default_away_name, ((struct away_message *)l->data)->name)) {
			default_away = l->data;
			break;
		}
	}

	if(!default_away && away_messages)
		default_away = away_messages->data;

	away_messages = g_slist_remove(away_messages, a);
	g_free(a);
	do_away_menu();
	gaim_status_sync();
}

void rem_away_mess(GtkWidget *w, struct away_message *a)
{
	gchar *text;

	text = g_strdup_printf(_("Are you sure you want to remove the away message \"%s\"?"), a->name);

	gaim_request_action(NULL, NULL, _("Remove Away Message"), text,
						0, g_strdup(a->name), 2,
						_("Remove"), G_CALLBACK(do_rem_away_mess),
						_("Cancel"), G_CALLBACK(g_free));

	g_free(text);
}

static void set_gc_away(GObject *obj, GaimConnection *gc)
{
	struct away_message *awy = g_object_get_data(obj, "away_message");

	if (awy)
		serv_set_away(gc, GAIM_AWAY_CUSTOM, awy->message);
	else
		serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
}

static void set_gc_state(GObject *obj, GaimConnection *gc)
{
	char *awy = g_object_get_data(obj, "away_state");

	serv_set_away(gc, awy, NULL);
}

/* XXX This needs to be fixed, NOW! */
extern GtkListStore *prefs_away_store;
extern GtkWidget *prefs_away_menu;

void do_away_menu()
{
	GtkWidget *menuitem;
	GtkWidget *remmenu;
	GtkWidget *submenu, *submenu2;
	GtkWidget *remitem;
	GtkWidget *image;
	GdkPixbuf *pixbuf, *scale;
	GList *l;
	GSList *awy = away_messages;
	struct away_message *a;
	GList *con;
	GaimConnection *gc = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	int count = 0;

	if (prefs_away_store != NULL) {
		gtk_list_store_clear(prefs_away_store);
		while (awy) {
			GtkTreeIter iter;
			a = (struct away_message *)awy->data;
			gtk_list_store_append(prefs_away_store, &iter);
			gtk_list_store_set(prefs_away_store, &iter,
					0, a->name,
					1, a,
					-1);
			awy = g_slist_next(awy);
		}
	}

	if (awaymenu) {
		l = gtk_container_get_children(GTK_CONTAINER(awaymenu));

		while (l) {
			gtk_container_remove(GTK_CONTAINER(awaymenu), GTK_WIDGET(l->data));
			l = l->next;
		}

		g_list_free(l);

		remmenu = gtk_menu_new();

		menuitem = gtk_menu_item_new_with_label(_("New Away Message"));
		gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
		gtk_widget_show(menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(create_away_mess),
				   NULL);

		awy = away_messages;
		while (awy) {
			a = (struct away_message *)awy->data;

			remitem = gtk_menu_item_new_with_label(a->name);
			gtk_menu_shell_append(GTK_MENU_SHELL(remmenu), remitem);
			gtk_widget_show(remitem);
			g_signal_connect(G_OBJECT(remitem), "activate",
					   G_CALLBACK(rem_away_mess), a);

			awy = g_slist_next(awy);

		}

		menuitem = gtk_menu_item_new_with_label(_("Remove Away Message"));
		gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
		gtk_widget_show(menuitem);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), remmenu);
		gtk_widget_show(remmenu);

		gaim_separator(awaymenu);

		for (con = gaim_connections_get_all(); con != NULL; con = con->next) {
			gc = con->data;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (prpl_info->away_states != NULL && prpl_info->set_away != NULL)
				count++;
		}

		if (count == 0) {
		} else if (count == 1) {
			GList *msgs, *tmp;

			for (con = gaim_connections_get_all(); con != NULL; con = con->next) {
				gc = con->data;

				prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

				if (prpl_info->away_states && prpl_info->set_away)
					break;
			}

			tmp = msgs = prpl_info->away_states(gc);

			if ((g_list_length(msgs) == 1) && !strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
				awy = away_messages;

				while (awy) {
					a = (struct away_message *)awy->data;

					menuitem = gtk_menu_item_new_with_label(a->name);
					g_object_set_data(G_OBJECT(menuitem), "away_message", a);
					gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
					gtk_widget_show(menuitem);
					g_signal_connect(G_OBJECT(menuitem), "activate",
							   G_CALLBACK(do_away_message), a);

					awy = g_slist_next(awy);
				}
			} else
				while (msgs) {
					awy = away_messages;

					menuitem = gtk_menu_item_new_with_label(msgs->data);
					g_object_set_data(G_OBJECT(menuitem), "away_state", msgs->data);
					gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
					gtk_widget_show(menuitem);

					if (strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
						g_signal_connect(G_OBJECT(menuitem), "activate",
								   G_CALLBACK(set_gc_state), gc);
					} else {
						submenu = gtk_menu_new();
						gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
									  submenu);
						gtk_widget_show(submenu);

						while (awy) {
							a = (struct away_message *)awy->data;

							menuitem = gtk_menu_item_new_with_label(a->name);
							g_object_set_data(G_OBJECT(menuitem), "away_message",
									a);
							gtk_menu_shell_append(GTK_MENU_SHELL(submenu),
									menuitem);
							gtk_widget_show(menuitem);
							g_signal_connect(G_OBJECT(menuitem),
									   "activate",
									   G_CALLBACK
									   (do_away_message), a);

							awy = g_slist_next(awy);
						}
					}
					msgs = g_list_next(msgs);
				}

			g_list_free(tmp);
		}
		else {
			for (con = gaim_connections_get_all(); con != NULL; con = con->next) {
				GaimAccount *account;
				char buf[256];
				GList *msgs, *tmp;
				gc = con->data;

				prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

				if (!prpl_info->away_states || !prpl_info->set_away)
					continue;

				account = gaim_connection_get_account(gc);

				g_snprintf(buf, sizeof(buf), "%s (%s)",
						   gaim_account_get_username(account),
						   gaim_account_get_protocol_name(account));
				menuitem = gtk_image_menu_item_new_with_label(buf);

				pixbuf = create_prpl_icon(gc->account);
				if (pixbuf) {
					scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
					image = gtk_image_new_from_pixbuf(scale);
					g_object_unref(G_OBJECT(pixbuf));
					g_object_unref(G_OBJECT(scale));
					gtk_widget_show(image);
					gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
							image);
				}

				gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
				gtk_widget_show(menuitem);

				submenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
				gtk_widget_show(submenu);

				tmp = msgs = prpl_info->away_states(gc);

				if ((g_list_length(msgs) == 1) &&
				    (!strcmp(msgs->data, GAIM_AWAY_CUSTOM))) {
					menuitem = gtk_menu_item_new_with_label(_("Back"));
					gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
					gtk_widget_show(menuitem);
					g_signal_connect(G_OBJECT(menuitem), "activate",
							   G_CALLBACK(set_gc_away), gc);

					gaim_separator(submenu);

					awy = away_messages;

					while (awy) {
						a = (struct away_message *)awy->data;

						menuitem = gtk_menu_item_new_with_label(a->name);
						g_object_set_data(G_OBJECT(menuitem), "away_message", a);
						gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
						gtk_widget_show(menuitem);
						g_signal_connect(G_OBJECT(menuitem), "activate",
								   G_CALLBACK(set_gc_away), gc);

						awy = g_slist_next(awy);
					}
				} else
					while (msgs) {
						awy = away_messages;

						menuitem = gtk_menu_item_new_with_label(msgs->data);
						g_object_set_data(G_OBJECT(menuitem), "away_state",
									 msgs->data);
						gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
						gtk_widget_show(menuitem);

						if (strcmp(msgs->data, GAIM_AWAY_CUSTOM)) {
							g_signal_connect(G_OBJECT(menuitem),
									   "activate",
									   G_CALLBACK(set_gc_state),
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
								g_object_set_data(G_OBJECT(menuitem),
										"away_message", a);
								gtk_menu_shell_append(GTK_MENU_SHELL(submenu2),
										menuitem);
								gtk_widget_show(menuitem);
								g_signal_connect(G_OBJECT(menuitem),
										   "activate",
										   G_CALLBACK
										   (set_gc_away), gc);

								awy = g_slist_next(awy);
							}
						}
						msgs = g_list_next(msgs);
					}

				g_list_free(tmp);
			}

			menuitem = gtk_menu_item_new_with_label(_("Set All Away"));
			gtk_menu_shell_append(GTK_MENU_SHELL(awaymenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);

			awy = away_messages;

			while (awy) {
				a = (struct away_message *)awy->data;

				menuitem = gtk_menu_item_new_with_label(a->name);
				g_object_set_data(G_OBJECT(menuitem), "away_message", a);
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
				gtk_widget_show(menuitem);
				g_signal_connect(G_OBJECT(menuitem), "activate",
						   G_CALLBACK(do_away_message), a);

				awy = g_slist_next(awy);
			}
		}
	}

	if (prefs_away_menu) {
		l = gtk_container_get_children(GTK_CONTAINER(prefs_away_menu));
		while (l) {
			gtk_widget_destroy(GTK_WIDGET(l->data));
			l = l->next;
		}
		gtk_widget_hide(GTK_WIDGET(prefs_away_menu));
		default_away_menu_init(GTK_WIDGET(prefs_away_menu));
		gtk_widget_show(prefs_away_menu);
	}
}

/*------------------------------------------------------------------------*/
/*  The dialog for new away messages (from dialogs.c)                     */
/*------------------------------------------------------------------------*/
struct create_away {
	GtkWidget *window;
	GtkWidget *toolbar;
	GtkWidget *entry;
	GtkWidget *text;
	struct away_message *mess;
};

static void
away_mess_destroy(GtkWidget *widget, struct create_away *ca)
{
	gtk_widget_destroy(ca->window);
	g_free(ca);
}

static void
away_mess_destroy_ca(GtkWidget *widget, GdkEvent *event, struct create_away *ca)
{
	away_mess_destroy(NULL, ca);
}

static gint
sort_awaymsg_list(gconstpointer a, gconstpointer b)
{
	struct away_message *msg_a;
	struct away_message *msg_b;

	msg_a = (struct away_message *)a;
	msg_b = (struct away_message *)b;

	return (strcmp(msg_a->name, msg_b->name));
}

static struct
away_message *save_away_message(struct create_away *ca)
{
	struct away_message *am;
	gchar *away_message;

	if (!ca->mess)
		am = g_new0(struct away_message, 1);
	else {
		am = ca->mess;
	}

	g_snprintf(am->name, sizeof(am->name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	away_message = gtk_imhtml_get_markup(GTK_IMHTML(ca->text));

	g_snprintf(am->message, sizeof(am->message), "%s", away_message);
	g_free(away_message);

	if (!ca->mess)
		away_messages = g_slist_insert_sorted(away_messages, am, sort_awaymsg_list);

	do_away_menu(NULL);
	gaim_status_sync();

	return am;
}

int
check_away_mess(struct create_away *ca, int type)
{
	gchar *msg;
	if ((strlen(gtk_entry_get_text(GTK_ENTRY(ca->entry))) == 0) && (type == 1)) {
		/* We shouldn't allow a blank title */
		gaim_notify_error(NULL, NULL,
						  _("You cannot save an away message with a "
							"blank title"),
						  _("Please give the message a title, or choose "
							"\"Use\" to use without saving."));
		return 0;
	}

	msg = gtk_imhtml_get_text(GTK_IMHTML(ca->text), NULL, NULL);

	if ((type <= 1) && ((msg == NULL) || (*msg == '\0'))) {
		/* We shouldn't allow a blank message */
		gaim_notify_error(NULL, NULL,
						  _("You cannot create an empty away message"), NULL);
		return 0;
	}

	g_free(msg);

	return 1;
}

void
save_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;

	save_away_message(ca);

	away_mess_destroy(NULL, ca);
}

void
use_away_mess(GtkWidget *widget, struct create_away *ca)
{
	static struct away_message am;
	gchar *away_message;

	if (!check_away_mess(ca, 0))
		return;

	g_snprintf(am.name, sizeof(am.name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	away_message = gtk_imhtml_get_markup(GTK_IMHTML(ca->text));

	g_snprintf(am.message, sizeof(am.message), "%s", away_message);
	g_free(away_message);

	do_away_message(NULL, &am);

	away_mess_destroy(NULL, ca);
}

void
su_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;

	do_away_message(NULL, save_away_message(ca));

	away_mess_destroy(NULL, ca);
}

void
create_away_mess(GtkWidget *widget, void *dummy)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *button;
	GList *focus_chain = NULL;
	struct create_away *ca = g_new0(struct create_away, 1);

	/* Set up window */
	GAIM_DIALOG(ca->window);
	gtk_widget_set_size_request(ca->window, -1, 250);
	gtk_window_set_role(GTK_WINDOW(ca->window), "away_mess");
	gtk_window_set_title(GTK_WINDOW(ca->window), _("New away message"));
	g_signal_connect(G_OBJECT(ca->window), "delete_event",
			   G_CALLBACK(away_mess_destroy_ca), ca);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
	gtk_container_add(GTK_CONTAINER(ca->window), hbox);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	/* Away message title */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Away title: "));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	ca->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), ca->entry, TRUE, TRUE, 0);
	gaim_set_accessible_label (ca->entry, label);
	focus_chain = g_list_append(focus_chain, hbox);

	/* Toolbar */
	ca->toolbar = gtk_imhtmltoolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), ca->toolbar, FALSE, FALSE, 0);

	/* Away message text */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	ca->text = gtk_imhtml_new(NULL, NULL);
	gtk_imhtml_set_editable(GTK_IMHTML(ca->text), TRUE);
	gtk_imhtml_set_format_functions(GTK_IMHTML(ca->text), GTK_IMHTML_ALL ^ GTK_IMHTML_IMAGE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ca->text), GTK_WRAP_WORD_CHAR);

	gtk_imhtml_smiley_shortcuts(GTK_IMHTML(ca->text),
			gaim_prefs_get_bool("/gaim/gtk/conversations/smiley_shortcuts"));
	gtk_imhtml_html_shortcuts(GTK_IMHTML(ca->text),
			gaim_prefs_get_bool("/gaim/gtk/conversations/html_shortcuts"));
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(ca->text));
	gtk_imhtmltoolbar_attach(GTK_IMHTMLTOOLBAR(ca->toolbar), ca->text);
	gtk_imhtmltoolbar_associate_smileys(GTK_IMHTMLTOOLBAR(ca->toolbar), "default");
	gaim_setup_imhtml(ca->text);

	gtk_container_add(GTK_CONTAINER(sw), ca->text);
	focus_chain = g_list_append(focus_chain, sw);

	if (dummy) {
		/* If anything is passed here, it is an away_message pointer */
		struct away_message *amt = (struct away_message *) dummy ;
		gtk_entry_set_text(GTK_ENTRY(ca->entry), amt->name);
		gtk_imhtml_append_text_with_images(GTK_IMHTML(ca->text), amt->message, 0, NULL);
		ca->mess = amt;
	}

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("_Save"), GTK_STOCK_SAVE, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(save_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("Sa_ve & Use"), GTK_STOCK_OK, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(su_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("_Use"), GTK_STOCK_EXECUTE, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(use_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button_from_stock(_("_Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(away_mess_destroy), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	focus_chain = g_list_prepend(focus_chain, hbox);

	gtk_widget_show_all(ca->window);
	gtk_container_set_focus_chain(GTK_CONTAINER(vbox), focus_chain);
}
#endif /* 0 */
