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
#include "../config.h"
#endif
#ifdef USE_APPLET
#include <gnome.h>
#include <applet-widget.h>
#include "gnome_applet_mgr.h"
#endif /* USE_APPLET */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "pixmaps/join.xpm"

GtkWidget *imaway=NULL;

GtkWidget *awaymenu = NULL;
struct away_message *awaymessage = NULL;

static void destroy_im_away()
{
	if (imaway)
		gtk_widget_destroy(imaway);
	imaway=NULL;
}

void do_im_back(GtkWidget *w, GtkWidget *x)
{
#ifdef USE_APPLET
	applet_widget_unregister_callback(APPLET_WIDGET(applet),"away");
	set_user_state(online);
	insert_applet_away();
#endif /* USE_APPLET */
	if (imaway) {
		gtk_widget_destroy(imaway);
		imaway=NULL;
        }

        serv_set_away(NULL);
	awaymessage = NULL;
	plugin_event(event_back, 0, 0, 0);
}

void do_away_message(GtkWidget *w, struct away_message *a)
{
	GtkWidget *back;
 	GtkWidget *awaytext;
        GtkWidget *vscrollbar;
	GtkWidget *bbox;
	GtkWidget *vbox;
        GtkWidget *topbox;
        char *buf2;
        char buf[BUF_LONG];
        GList *cnv = conversations;
        struct conversation *c;

	if (!blist)
		return;

#ifdef USE_APPLET
	remove_applet_away();
        applet_widget_register_callback(APPLET_WIDGET(applet),
                                        "away",
                                        _("Back"),
                                        (AppletCallbackFunc) do_im_back,
                                        NULL);				
	set_user_state(away);
#endif

	if (!imaway) {
		imaway = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_widget_realize(imaway);
		aol_icon(imaway->window);
		back = picture_button(imaway, _("I'm Back!"), join_xpm);
		bbox = gtk_hbox_new(TRUE, 10);
		topbox = gtk_hbox_new(FALSE, 5);
                vbox = gtk_vbox_new(FALSE, 5);

		awaytext = gtk_text_new(NULL, NULL);
		g_snprintf(buf, sizeof(buf), "%s", a->message);
                vscrollbar = gtk_vscrollbar_new(GTK_TEXT(awaytext)->vadj);
		gtk_widget_show(vscrollbar);
		gtk_widget_set_usize(awaytext, 225, 75);
                gtk_text_set_word_wrap(GTK_TEXT(awaytext), TRUE);
                gtk_widget_show(awaytext);
		gtk_text_freeze(GTK_TEXT(awaytext));
		gtk_text_insert(GTK_TEXT(awaytext), NULL, NULL, NULL, buf, -1);
		gtk_widget_show(awaytext);
                
		if (display_options & OPT_DISP_COOL_LOOK)
			gtk_button_set_relief(GTK_BUTTON(back), GTK_RELIEF_NONE);
		
		/* Put the buttons in the box */
		gtk_box_pack_start(GTK_BOX(bbox), back, TRUE, TRUE, 10);
		
		gtk_box_pack_start(GTK_BOX(topbox), awaytext, FALSE, FALSE, 5);
	
		/* And the boxes in the box */
		gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
		
		/* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(imaway), "destroy",
			   GTK_SIGNAL_FUNC(destroy_im_away), imaway);
		gtk_signal_connect(GTK_OBJECT(back), "clicked",
			   GTK_SIGNAL_FUNC(do_im_back), imaway);

		/* Finish up */
		gtk_widget_show(back);
		gtk_widget_show(topbox);
		gtk_widget_show(bbox);
		gtk_widget_show(vbox);
		if (strlen(a->name))
			gtk_window_set_title(GTK_WINDOW(imaway), a->name);
		else
                        gtk_window_set_title(GTK_WINDOW(imaway), _("Gaim - Away!"));
		gtk_window_set_focus(GTK_WINDOW(imaway), back);
		gtk_container_add(GTK_CONTAINER(imaway), vbox);
		awaymessage = a;

        } else {
		destroy_im_away();
		do_away_message(w, a);
		return;
	}

        /* New away message... Clear out the old sent_aways */
        while(cnv) {
                c = (struct conversation *)cnv->data;
                c->sent_away = 0;
                cnv = cnv->next;
        }


        buf2 = g_malloc(strlen(awaymessage->message)*4 + 1);
	strcpy(buf2, awaymessage->message);
        escape_text(buf2);
        serv_set_away(buf2);
        g_free(buf2);
	gtk_widget_show(imaway);
	plugin_event(event_away, 0, 0, 0);
}

void rem_away_mess(GtkWidget *w, struct away_message *a)
{
#ifdef USE_APPLET
	char *awayname;
	awayname = g_malloc(sizeof (*awayname) * (6 + strlen(a->name)));
	awayname[0] = '\0';
	strcat(awayname, "away/");
	strcat(awayname, a->name);
	applet_widget_unregister_callback(APPLET_WIDGET(applet), awayname);
#endif
        away_messages = g_list_remove(away_messages, a);
        g_free(a);
        do_away_menu();
        save_prefs();
}


void do_away_menu()
{
	GtkWidget *menuitem;
	static GtkWidget *remmenu;
	GtkWidget *remitem;
	GtkWidget *label;
	GtkWidget *sep;
	GList *l;
	GtkWidget *list_item;
        GList *awy = away_messages;
        struct away_message *a;

#ifdef USE_APPLET
	remove_applet_away();
	if (imaway)
		applet_widget_register_callback(APPLET_WIDGET(applet),
			"away",
			_("Back"),
			(AppletCallbackFunc)do_im_back,
			NULL);
	else
		insert_applet_away();
#endif

	if (prefs_away_list != NULL) {
                gtk_list_clear_items(GTK_LIST(prefs_away_list), 0, -1);
		while(awy) {
			a = (struct away_message *)awy->data;
			label = gtk_label_new(a->name);
			list_item = gtk_list_item_new();
			gtk_container_add(GTK_CONTAINER(list_item), label);
			gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_list_clicked), a);
/*			gtk_signal_connect(GTK_OBJECT(list_item), "deselect", GTK_SIGNAL_FUNC(away_list_unclicked), a);*/
			gtk_object_set_user_data(GTK_OBJECT(list_item), a);

			gtk_widget_show(label);
			gtk_container_add(GTK_CONTAINER(prefs_away_list), list_item);
			gtk_widget_show(list_item);

			awy = awy->next;
		}
		if (away_messages != NULL)
                        gtk_list_select_item(GTK_LIST(prefs_away_list), 0);
	}
	
	if (!awaymenu) return;
	l = gtk_container_children(GTK_CONTAINER(awaymenu));
	
	while(l) {
		gtk_widget_destroy(GTK_WIDGET(l->data));
		l = l->next;
	}


	remmenu = gtk_menu_new();
	
	

	menuitem = gtk_menu_item_new_with_label(_("New Away Message"));
	gtk_menu_append(GTK_MENU(awaymenu), menuitem);
	gtk_widget_show(menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(create_away_mess), NULL);


	awy = away_messages;
        while(awy) {
                a = (struct away_message *)awy->data;

		remitem = gtk_menu_item_new_with_label(a->name);
		gtk_menu_append(GTK_MENU(remmenu), remitem);
		gtk_widget_show(remitem);
		gtk_signal_connect(GTK_OBJECT(remitem), "activate", GTK_SIGNAL_FUNC(rem_away_mess), a);

		awy = awy->next;

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

	awy = away_messages;
	
	while(awy) {
                a = (struct away_message *)awy->data;
                
		menuitem = gtk_menu_item_new_with_label(a->name);
		gtk_menu_append(GTK_MENU(awaymenu), menuitem);
		gtk_widget_show(menuitem);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate", GTK_SIGNAL_FUNC(do_away_message), a);

		awy = awy->next;

	}

}






