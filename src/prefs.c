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
#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "proxy.h"

struct prefs_data *pd = NULL;
struct debug_window *dw = NULL;

GtkWidget *debugbutton;

struct chat_page {
	GtkWidget *list1;
	GtkWidget *list2;
};


char debug_buff[BUF_LONG];

void do_chat_page(GtkWidget *page);

void list_clicked( GtkWidget *widget, struct away_message *a);
void list_unclicked( GtkWidget *widget, struct away_message *a);

void show_debug(GtkObject *);

void remove_away_message(GtkWidget *widget, void *dummy)
{
	GList *i;
	struct away_message *a;

	i = GTK_LIST(pd->away_list)->selection;

	a = gtk_object_get_user_data(GTK_OBJECT(i->data));

	rem_away_mess(NULL, a);
}

void away_list_clicked( GtkWidget *widget, struct away_message *a)
{
	gchar buffer[2048];
	guint text_len;

	pd->cur_message = a;

	/* Get proper Length */
	text_len = gtk_text_get_length(GTK_TEXT(pd->away_text));
	pd->edited_message = gtk_editable_get_chars(GTK_EDITABLE(pd->away_text), 0, text_len);

	/* Clear the Box */
	gtk_text_set_point(GTK_TEXT(pd->away_text), 0 );
	gtk_text_forward_delete (GTK_TEXT(pd->away_text), text_len); 

	/* Fill the text box with new message */
	strcpy(buffer, a->message);
	gtk_text_insert(GTK_TEXT(pd->away_text), NULL, NULL, NULL, buffer, -1);


}

void away_list_unclicked( GtkWidget *widget, struct away_message *a)
{
        if (pd == NULL)
                return;
	strcpy(a->message, pd->edited_message);
	save_prefs();
}

void set_option(GtkWidget *w, int *option)
{
	*option = !(*option);
}

void set_display_option(GtkWidget *w, int *option)
{
        display_options = display_options ^ (int)option;

	if (blist) update_button_pix();

#ifdef USE_APPLET
	update_pixmaps();
#endif

	save_prefs();
}

void set_sound_option(GtkWidget *w, int *option)
{
	sound_options = sound_options ^ (int)option;
	save_prefs();
}

void set_font_option(GtkWidget *w, int *option)
{
	font_options = font_options ^ (int)option;

	update_font_buttons();	

	save_prefs();
}

void set_general_option(GtkWidget *w, int *option)
{
	general_options = general_options ^ (int)option;

       	if ((int)option == OPT_GEN_SHOW_LAGMETER)
       		update_lagometer(-1);
       	if ((int)option == OPT_GEN_LOG_ALL)
       		update_log_convs();
	save_prefs();

	/*
        if (data == &show_grp_nums)
		update_num_groups();
	if (data == &showidle || data == &showpix)
		update_show_idlepix();
	if (data == &button_pix && blist)
                update_button_pix();
        if (data == &transparent)
                update_transparency();
          */
        
}


static gint debug_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	if (debugbutton)
		gtk_button_clicked(GTK_BUTTON(debugbutton));
	if (general_options & OPT_GEN_DEBUG)
	{
		general_options = general_options ^ (int)OPT_GEN_DEBUG;
		save_prefs();
	}
	g_free(dw);
	dw=NULL;
	return FALSE;

}

static gint handle_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	guint text_len;
	struct away_message *a;


        if (pd->cur_message) {
        
		a = pd->cur_message;


		/* Get proper Length and grab data */
		text_len = gtk_text_get_length(GTK_TEXT(pd->away_text));
		pd->edited_message = gtk_editable_get_chars(GTK_EDITABLE(pd->away_text), 0, text_len);

		/* Store the data for later use */
		strcpy(a->message, pd->edited_message);

	}
	
	save_prefs();

	if (event == NULL)
	{
		gtk_widget_destroy(pd->window);
		debugbutton=NULL;
	}
	g_free(pd);
	pd = NULL;

	
        return FALSE;
}

static int
manualentry_key_pressed(GtkWidget *w, GdkEvent *event, void *dummy)
{
        g_snprintf(web_command, sizeof(web_command), "%s", gtk_entry_get_text(GTK_ENTRY(pd->browser_entry)));
        save_prefs();
	return TRUE;
}

static int
connection_key_pressed(GtkWidget *w, GdkEvent *event, void *dummy)
{
	g_snprintf(aim_host, sizeof(aim_host), "%s", gtk_entry_get_text(GTK_ENTRY(pd->aim_host_entry)));
	sscanf(gtk_entry_get_text(GTK_ENTRY(pd->aim_port_entry)), "%d", &aim_port);
	if (proxy_type != PROXY_NONE) {
		g_snprintf(proxy_host, sizeof(proxy_host), "%s", gtk_entry_get_text(GTK_ENTRY(pd->proxy_host_entry)));
		sscanf(gtk_entry_get_text(GTK_ENTRY(pd->proxy_port_entry)), "%d", &proxy_port);
	}

	g_snprintf(login_host, sizeof(login_host), "%s", gtk_entry_get_text(GTK_ENTRY(pd->login_host_entry)));
	sscanf(gtk_entry_get_text(GTK_ENTRY(pd->login_port_entry)), "%d", &login_port);	
	save_prefs();
	return TRUE;
}



                   
static void set_browser(GtkWidget *w, int *data)
{
	web_browser = (int)data;
        if (web_browser != BROWSER_MANUAL) {
                if (pd->browser_entry)
                        gtk_widget_set_sensitive(pd->browser_entry, FALSE);
        } else {
                if (pd->browser_entry)
                        gtk_widget_set_sensitive(pd->browser_entry, TRUE);
        }
        
        if (web_browser != BROWSER_NETSCAPE) {
                if (pd->nwbutton)
                        gtk_widget_set_sensitive(pd->nwbutton, FALSE);
        } else {
                if (pd->nwbutton)
                        gtk_widget_set_sensitive(pd->nwbutton, TRUE);
        }
        

        save_prefs();
}

static void set_connect(GtkWidget *w, int *data)
{
	proxy_type = (int)data;
	if (proxy_type != PROXY_NONE) {
                if (pd->proxy_host_entry)
                        gtk_widget_set_sensitive(pd->proxy_host_entry, TRUE);
		if (pd->proxy_port_entry)
			gtk_widget_set_sensitive(pd->proxy_port_entry, TRUE);
	} else {
                if (pd->proxy_host_entry)
                        gtk_widget_set_sensitive(pd->proxy_host_entry, FALSE);
		if (pd->proxy_port_entry)
			gtk_widget_set_sensitive(pd->proxy_port_entry, FALSE);
	}
        
        save_prefs();
}

static void set_idle(GtkWidget *w, int *data)
{
	report_idle = (int)data;
        save_prefs();
}


GtkWidget *gaim_button(const char *text, int *options, int option, GtkWidget *page)
{
	GtkWidget *button;
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (*options & option));
	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	if (options == &font_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_font_option), (int *)option);

	if (options == &sound_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_sound_option), (int *)option);
	if (options == &display_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_display_option), (int *)option);

	if (options == &general_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_general_option), (int *)option);
	gtk_widget_show(button);

	return button;
}


void build_prefs()
{
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *hbox;
        GtkWidget *hbox2;
        GtkWidget *idlebox;
        GtkWidget *idleframe;
        GtkWidget *genbox;
	GtkWidget *fontbox;
	GtkWidget *fontframe;
	GtkWidget *appbox;
	GtkWidget *away_topbox;
	GtkWidget *away_botbox;
	GtkWidget *add_away;
	GtkWidget *remove_away;
	GtkWidget *close;
	GtkWidget *notebook;
	GtkWidget *sound_page;
	/* GtkWidget *debug_page; */
	GtkWidget *general_page;
	GtkWidget *appearance_page;
	GtkWidget *chat_page;
        GtkWidget *browser_page;
#ifndef USE_OSCAR /* sorry, since we don't control the comm we can't set
		     the connection */
        GtkWidget *connection_page;
#endif
#ifdef USE_APPLET
	GtkWidget *applet_page;
	GtkWidget *appletbox;
#endif
        GtkWidget *label;
        GtkWidget *browseropt;
        GtkWidget *connectopt;
        GtkWidget *idleopt;
	        
        GList *awy = away_messages;
        struct away_message *a;
        GtkWidget *sw;
	GtkWidget *sw2;
	GtkWidget *away_page;
	GtkWidget *select_font;
	GtkWidget *font_face_for_text;
	
	GtkWidget *list_item;

	gchar buffer[64];

	if (!pd)
                pd = g_new0(struct prefs_data, 1);

	pd->window = gtk_window_new(GTK_WINDOW_DIALOG);
        gtk_widget_realize(pd->window);
	aol_icon(pd->window->window);
	gtk_container_border_width(GTK_CONTAINER(pd->window), 10);
	gtk_window_set_title(GTK_WINDOW(pd->window), _("Gaim - Preferences"));
	
        vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(pd->window), vbox);

	/* Notebooks */
	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 5);


	/* General page */
	general_page = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("General"));
	gtk_widget_show(label);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_page, label);

        genbox = gtk_vbox_new(FALSE, 5);
        idleframe = gtk_frame_new(_("Idle"));
        idlebox = gtk_vbox_new(FALSE, 5);

        gtk_box_pack_start(GTK_BOX(general_page), genbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(general_page), idleframe, TRUE, TRUE, 5);
        gtk_container_add(GTK_CONTAINER(idleframe), idlebox);

        
	gaim_button(_("Enter sends message"), &general_options, OPT_GEN_ENTER_SENDS, genbox);
	gaim_button(_("Auto-login"), &general_options, OPT_GEN_AUTO_LOGIN, genbox);
	gaim_button(_("Log All Conversations"), &general_options, OPT_GEN_LOG_ALL, genbox);
	gaim_button(_("Strip HTML from log files"), &general_options, OPT_GEN_STRIP_HTML, genbox);
	gaim_button(_("Raise windows when message recieved"), &general_options, OPT_GEN_POPUP_WINDOWS, genbox);
	gaim_button(_("Raise chat windows when people speak"), &general_options, OPT_GEN_POPUP_CHAT, genbox);
        gaim_button(_("Send URLs as links"), &general_options, OPT_GEN_SEND_LINKS, genbox);
	gaim_button(_("Show Lag-O-Meter"), &general_options, OPT_GEN_SHOW_LAGMETER, genbox);
        gaim_button(_("Save some window size/positions"), &general_options, OPT_GEN_SAVED_WINDOWS, genbox);
        gaim_button(_("Ignore new conversations when away"), &general_options, OPT_GEN_DISCARD_WHEN_AWAY, genbox);
	gaim_button(_("Automagically highlight misspelled words"), &general_options, OPT_GEN_CHECK_SPELLING, genbox);
	gaim_button(_("Sending messages removes away status"), &general_options, OPT_GEN_BACK_ON_IM, genbox);
	if (!dw && (general_options & OPT_GEN_DEBUG))
		general_options = general_options ^ OPT_GEN_DEBUG;
        debugbutton = gaim_button(_("Enable debug mode"), &general_options, OPT_GEN_DEBUG, genbox);


        idleopt = gtk_radio_button_new_with_label(NULL, _("No Idle"));
        gtk_box_pack_start(GTK_BOX(idlebox), idleopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(idleopt), "clicked", GTK_SIGNAL_FUNC(set_idle), (void *)IDLE_NONE);
	gtk_widget_show(idleopt);
        if (report_idle == IDLE_NONE)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(idleopt), TRUE);

        idleopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(idleopt)), _("GAIM Use"));
        gtk_box_pack_start(GTK_BOX(idlebox), idleopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(idleopt), "clicked", GTK_SIGNAL_FUNC(set_idle), (void *)IDLE_GAIM);
	gtk_widget_show(idleopt);
        if (report_idle == IDLE_GAIM)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(idleopt), TRUE);

/*        idleopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(idleopt)), "X Use");
        gtk_box_pack_start(GTK_BOX(idlebox), idleopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(idleopt), "clicked", GTK_SIGNAL_FUNC(set_idle), (void *)IDLE_SYSTEM);
	gtk_widget_show(idleopt);
        if (report_idle == IDLE_SYSTEM)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(idleopt), TRUE);
*/
        
        gtk_widget_show(general_page);
        gtk_widget_show(genbox);
        gtk_widget_show(idlebox);
        gtk_widget_show(idleframe);


        gtk_signal_connect_object( GTK_OBJECT(debugbutton), "clicked", GTK_SIGNAL_FUNC(show_debug), NULL);


	/* Applet */
#ifdef USE_APPLET

	applet_page = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("Applet"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), applet_page, label);

        appletbox = gtk_vbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(applet_page), appletbox, TRUE, TRUE, 5);

	gaim_button(_("Automatically Show Buddy List"), &general_options, OPT_GEN_APP_BUDDY_SHOW, appletbox);
	gaim_button(_("Sounds go through GNOME"), &sound_options, OPT_SOUND_THROUGH_GNOME, appletbox);
	gaim_button(_("Buddy list displays near the applet"), &general_options, OPT_GEN_NEAR_APPLET, appletbox);

	gtk_widget_show(appletbox);
	gtk_widget_show(applet_page);

#endif
	

        /* Connection */
        
#ifndef USE_OSCAR
        connection_page = gtk_vbox_new(FALSE, 0);
        label = gtk_label_new(_("Connection"));
        gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), connection_page, label);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("TOC Host:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
        pd->aim_host_entry = gtk_entry_new();
        gtk_widget_show(pd->aim_host_entry);
	gtk_box_pack_start(GTK_BOX(hbox), pd->aim_host_entry, FALSE, FALSE, 0);

	label = gtk_label_new(_("Port:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
        pd->aim_port_entry = gtk_entry_new();
        gtk_widget_show(pd->aim_port_entry);
	gtk_box_pack_start(GTK_BOX(hbox), pd->aim_port_entry, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	
	gtk_box_pack_start(GTK_BOX(connection_page), hbox, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(pd->aim_host_entry), aim_host);

	g_snprintf(buffer, sizeof(buffer), "%d", aim_port);
	gtk_entry_set_text(GTK_ENTRY(pd->aim_port_entry), buffer);
        
        hbox2 = gtk_hbox_new(FALSE, 0);
        label = gtk_label_new(_("Login Host:"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 5);
        pd->login_host_entry = gtk_entry_new();
        gtk_widget_show(pd->login_host_entry);
        gtk_box_pack_start(GTK_BOX(hbox2), pd->login_host_entry, FALSE, FALSE, 0);

        label = gtk_label_new(_("Port:"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 5);
        pd->login_port_entry = gtk_entry_new();
        gtk_widget_show(pd->login_port_entry);
        gtk_box_pack_start(GTK_BOX(hbox2), pd->login_port_entry, FALSE, FALSE, 0);
        gtk_widget_show(hbox2);

        gtk_box_pack_start(GTK_BOX(connection_page), hbox2, FALSE, FALSE, 0);
        gtk_entry_set_text(GTK_ENTRY(pd->login_host_entry), login_host);

        g_snprintf(buffer, sizeof(buffer), "%d", login_port);
        gtk_entry_set_text(GTK_ENTRY(pd->login_port_entry), buffer);

        connectopt = gtk_radio_button_new_with_label(NULL, _("No Proxy"));
        gtk_box_pack_start(GTK_BOX(connection_page), connectopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(connectopt), "clicked", GTK_SIGNAL_FUNC(set_connect), (void *)PROXY_NONE);
	gtk_widget_show(connectopt);
        if (proxy_type == PROXY_NONE)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(connectopt), TRUE);

        connectopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(connectopt)), _("HTTP Proxy"));
        gtk_box_pack_start(GTK_BOX(connection_page), connectopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(connectopt), "clicked", GTK_SIGNAL_FUNC(set_connect), (void *)PROXY_HTTP);
	gtk_widget_show(connectopt);
	if (proxy_type == PROXY_HTTP)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(connectopt), TRUE);


        connectopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(connectopt)), _("SOCKS v4 Proxy"));
        gtk_box_pack_start(GTK_BOX(connection_page), connectopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(connectopt), "clicked", GTK_SIGNAL_FUNC(set_connect), (void *)PROXY_SOCKS4);
	gtk_widget_show(connectopt);
	if (proxy_type == PROXY_SOCKS4)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(connectopt), TRUE);


        connectopt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(connectopt)), _("SOCKS v5 Proxy (DOES NOT WORK!)"));
        gtk_box_pack_start(GTK_BOX(connection_page), connectopt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(connectopt), "clicked", GTK_SIGNAL_FUNC(set_connect), (void *)PROXY_SOCKS5);
	gtk_widget_show(connectopt);
	if (proxy_type == PROXY_SOCKS5)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(connectopt), TRUE);


	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Proxy Host:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
        pd->proxy_host_entry = gtk_entry_new();
        gtk_widget_show(pd->proxy_host_entry);
	gtk_box_pack_start(GTK_BOX(hbox), pd->proxy_host_entry, FALSE, FALSE, 0);

	label = gtk_label_new(_("Port:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
        pd->proxy_port_entry = gtk_entry_new();
        gtk_widget_show(pd->proxy_port_entry);
	gtk_box_pack_start(GTK_BOX(hbox), pd->proxy_port_entry, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	
	gtk_box_pack_start(GTK_BOX(connection_page), hbox, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(pd->proxy_host_entry), proxy_host);

	g_snprintf(buffer, sizeof(buffer), "%d", proxy_port);
	gtk_entry_set_text(GTK_ENTRY(pd->proxy_port_entry), buffer);


	gtk_widget_show(connection_page);


	if (proxy_type != PROXY_NONE) {
                if (pd->proxy_host_entry)
                        gtk_widget_set_sensitive(pd->proxy_host_entry, TRUE);
		if (pd->proxy_port_entry)
			gtk_widget_set_sensitive(pd->proxy_port_entry, TRUE);
	} else {
                if (pd->proxy_host_entry)
                        gtk_widget_set_sensitive(pd->proxy_host_entry, FALSE);
		if (pd->proxy_port_entry)
			gtk_widget_set_sensitive(pd->proxy_port_entry, FALSE);
	}

	

	gtk_signal_connect(GTK_OBJECT(pd->aim_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
        gtk_signal_connect(GTK_OBJECT(pd->aim_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(pd->login_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(pd->login_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(pd->proxy_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
        gtk_signal_connect(GTK_OBJECT(pd->proxy_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);


#endif /* USE_OSCAR */
	
	/* Away */
	
        a = awaymessage;
        pd->cur_message = NULL;
        pd->nwbutton = NULL;
        pd->browser_entry = NULL;
        
	away_page = gtk_vbox_new(FALSE, 0);
	away_topbox = gtk_hbox_new(FALSE, 0);
	away_botbox = gtk_hbox_new(FALSE, 0);

	label = gtk_label_new(_("Away"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), away_page, label);
	gtk_widget_show(away_page);

	sw2 = gtk_scrolled_window_new(NULL, NULL);
    	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_show(sw2);

	pd->away_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), pd->away_list);
	gtk_box_pack_start(GTK_BOX(away_topbox), sw2, TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
    	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_show(sw);
    
	pd->away_text = gtk_text_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), pd->away_text);
	gtk_box_pack_start(GTK_BOX(away_topbox), sw, TRUE, TRUE, 0);
	gtk_text_set_word_wrap(GTK_TEXT(pd->away_text), TRUE);
	gtk_text_set_editable(GTK_TEXT(pd->away_text), TRUE );

	add_away = gtk_button_new_with_label(_("Create Message"));
	gtk_signal_connect(GTK_OBJECT(add_away), "clicked", GTK_SIGNAL_FUNC(create_away_mess), NULL);
	gtk_box_pack_start(GTK_BOX(away_botbox), add_away, TRUE, FALSE, 5);

	remove_away = gtk_button_new_with_label(_("Remove Message"));
	gtk_signal_connect(GTK_OBJECT(remove_away), "clicked", GTK_SIGNAL_FUNC(remove_away_message), NULL);
	gtk_box_pack_start(GTK_BOX(away_botbox), remove_away, TRUE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(away_page), away_topbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(away_page), away_botbox, FALSE, FALSE, 0);

	gtk_widget_show(add_away);
	gtk_widget_show(remove_away);
	gtk_widget_show(pd->away_list);
	gtk_widget_show(pd->away_text);
	gtk_widget_show(away_topbox);
	gtk_widget_show(away_botbox);
    
	if (awy != NULL) {
		a = (struct away_message *)awy->data;
		g_snprintf(buffer, sizeof(buffer), "%s", a->message);
		gtk_text_insert(GTK_TEXT(pd->away_text), NULL, NULL, NULL, buffer, -1);
	}

        while(awy) {
		a = (struct away_message *)awy->data;
		label = gtk_label_new(a->name);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_list_clicked), a);
		gtk_signal_connect(GTK_OBJECT(list_item), "deselect", GTK_SIGNAL_FUNC(away_list_unclicked), a);
		gtk_object_set_user_data(GTK_OBJECT(list_item), a);
               
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(pd->away_list), list_item);
		gtk_widget_show(list_item);
	
		awy = awy->next;

	}
       
	/* Sound */
	sound_page = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("Sounds"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sound_page, label);
        gaim_button(_("Sound when buddy logs in"), &sound_options, OPT_SOUND_LOGIN, sound_page);
	gaim_button(_("Sound when buddy logs out"), &sound_options, OPT_SOUND_LOGOUT, sound_page);
        gaim_button(_("Sound when message is received"), &sound_options, OPT_SOUND_RECV, sound_page);
	gaim_button(_("Sound when message is sent"), &sound_options, OPT_SOUND_SEND, sound_page);
        gaim_button(_("Sound when first message is received"), &sound_options, OPT_SOUND_FIRST_RCV, sound_page);
        gaim_button(_("Sound when message is received if away"), &sound_options, OPT_SOUND_WHEN_AWAY, sound_page);
	gaim_button(_("No sound for buddies signed on when you log in"), &sound_options, OPT_SOUND_SILENT_SIGNON, sound_page);
	gaim_button(_("Sounds in chat rooms when people enter/leave"), &sound_options, OPT_SOUND_CHAT_JOIN, sound_page);
	gaim_button(_("Sounds in chat rooms when people talk"), &sound_options, OPT_SOUND_CHAT_SAY, sound_page);
        gtk_widget_show(sound_page);


        /* Browser */
        browser_page = gtk_vbox_new(FALSE, 0);

        label = gtk_label_new(_("Browser"));
        gtk_widget_show(label);
        

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), browser_page, label);
        browseropt = gtk_radio_button_new_with_label(NULL, _("Netscape"));
        gtk_box_pack_start(GTK_BOX(browser_page), browseropt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(browseropt), "clicked", GTK_SIGNAL_FUNC(set_browser), (void *)BROWSER_NETSCAPE);
	gtk_widget_show(browseropt);
        if (web_browser == BROWSER_NETSCAPE)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(browseropt), TRUE);

        browseropt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(browseropt)), _("KFM (The KDE browser)"));
        gtk_box_pack_start(GTK_BOX(browser_page), browseropt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(browseropt), "clicked", GTK_SIGNAL_FUNC(set_browser), (void *)BROWSER_KFM);
	gtk_widget_show(browseropt);
	if (web_browser == BROWSER_KFM)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(browseropt), TRUE);


        browseropt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(browseropt)), _("Internal HTML widget (Quite likely a bad idea!)"));
        gtk_box_pack_start(GTK_BOX(browser_page), browseropt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(browseropt), "clicked", GTK_SIGNAL_FUNC(set_browser), (void *)BROWSER_INTERNAL);
	gtk_widget_show(browseropt);
	if (web_browser == BROWSER_INTERNAL)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(browseropt), TRUE);

        
        browseropt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(browseropt)), _("Manual"));
        gtk_box_pack_start(GTK_BOX(browser_page), browseropt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(browseropt), "clicked", GTK_SIGNAL_FUNC(set_browser), (void *)BROWSER_MANUAL);
        gtk_widget_show(browseropt);
	if (web_browser == BROWSER_MANUAL)
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(browseropt), TRUE);


        pd->browser_entry = gtk_entry_new();
        gtk_widget_show(pd->browser_entry);

        gtk_box_pack_start(GTK_BOX(browser_page), pd->browser_entry, FALSE, FALSE, 0);
        gtk_entry_set_text(GTK_ENTRY(pd->browser_entry), web_command);

	pd->nwbutton = gaim_button(_("Pop up new window by default"), &general_options, OPT_GEN_BROWSER_POPUP, browser_page);
	gtk_widget_show(browser_page);

        gtk_signal_connect(GTK_OBJECT(pd->browser_entry), "focus_out_event", GTK_SIGNAL_FUNC(manualentry_key_pressed), NULL);

        

        if (web_browser != BROWSER_MANUAL) {
                gtk_widget_set_sensitive(pd->browser_entry, FALSE);
        } else {
                gtk_widget_set_sensitive(pd->browser_entry, TRUE);
        }
        
        if (web_browser != BROWSER_NETSCAPE) {
                gtk_widget_set_sensitive(pd->nwbutton, FALSE);
        } else {
                gtk_widget_set_sensitive(pd->nwbutton, TRUE);
        }
        



	/* Appearance */
	appearance_page = gtk_hbox_new(FALSE, 0);
        label = gtk_label_new(_("Appearance"));
        gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), appearance_page, label);
	appbox = gtk_vbox_new(FALSE, 5);
	fontframe = gtk_frame_new(_("Font Properties"));
	fontbox = gtk_vbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(appearance_page), appbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(appearance_page), fontframe, TRUE, TRUE, 5);
	gtk_container_add(GTK_CONTAINER(fontframe), fontbox);

	gaim_button(_("Show time on messages"), &display_options, OPT_DISP_SHOW_TIME, appbox);
	gaim_button(_("Show numbers in groups"), &display_options, OPT_DISP_SHOW_GRPNUM, appbox );
	gaim_button(_("Show buddy-type pixmaps"), &display_options, OPT_DISP_SHOW_PIXMAPS, appbox );
	gaim_button(_("Show idle times"), &display_options, OPT_DISP_SHOW_IDLETIME, appbox );
	gaim_button(_("Show button pixmaps"), &display_options, OPT_DISP_SHOW_BUTTON_XPM, appbox );
	gaim_button(_("Ignore incoming colours"), &display_options, OPT_DISP_IGNORE_COLOUR, appbox );
#if 0
	gaim_button("Transparent text window (experimental)", &transparent, appbox );
#endif
	gaim_button(_("Show logon/logoffs in conversation windows"), &display_options, OPT_DISP_SHOW_LOGON, appbox );
	gaim_button(_("Use devil icons"), &display_options, OPT_DISP_DEVIL_PIXMAPS, appbox );
	gaim_button(_("Show graphical smileys"), &display_options, OPT_DISP_SHOW_SMILEY, appbox );
	
	
	gaim_button(_("Bold Text"), &font_options, OPT_FONT_BOLD, fontbox);
	gaim_button(_("Italics Text"), &font_options, OPT_FONT_ITALIC, fontbox);
	gaim_button(_("Underlined Text"), &font_options, OPT_FONT_UNDERLINE, fontbox);
	gaim_button(_("Strike Text"), &font_options, OPT_FONT_STRIKE, fontbox);
	font_face_for_text = gaim_button(_("Font Face for Text"), &font_options, OPT_FONT_FACE, fontbox);
		
	select_font = gtk_button_new_with_label(_("Select Font"));
	gtk_box_pack_start(GTK_BOX(fontbox), select_font, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(select_font), "clicked", GTK_SIGNAL_FUNC(show_font_dialog), NULL);
	if (!(font_options & OPT_FONT_FACE))
	    gtk_widget_set_sensitive(GTK_WIDGET(select_font), FALSE);
	gtk_widget_show(select_font);
	gtk_signal_connect(GTK_OBJECT(font_face_for_text), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select_font);
	
	gtk_widget_show(appearance_page);
	gtk_widget_show(fontbox);
	gtk_widget_show(fontframe);
	gtk_widget_show(appbox);	


	/* Buddy Chats */
	chat_page = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("Buddy Chats"));

	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), chat_page, label);
	
	do_chat_page(chat_page);
	gtk_widget_show(chat_page);
	
	bbox = gtk_hbox_new(FALSE, 5);
	close = gtk_button_new_with_label(_("Close"));
	
	/* Pack the button(s) in the button box */
	gtk_box_pack_end(GTK_BOX(bbox), close, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox),bbox, FALSE, FALSE, 5);

	gtk_widget_show(notebook);
        gtk_widget_show(close);

	gtk_widget_show(bbox);
	gtk_widget_show(vbox);

	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(handle_delete), NULL);
        gtk_signal_connect(GTK_OBJECT(pd->window),"delete_event", GTK_SIGNAL_FUNC(handle_delete), NULL);

}

void show_prefs()
{
	if (!pd || !pd->window)
		build_prefs();
	gtk_widget_show(pd->window);
}
void add_chat(GtkWidget *w, struct chat_page *cp)
{
	GList *sel = GTK_LIST(cp->list1)->selection;
	struct chat_room *cr, *cr2;
	GList *crs = chat_rooms;
        GtkWidget *item;

	if (sel) {
		cr = (struct chat_room *)gtk_object_get_user_data(GTK_OBJECT(sel->data));
	} else
		return;

	while(crs) {
		cr2 = (struct chat_room *)crs->data;
		if (!strcasecmp(cr->name, cr2->name))
                        return;
                crs = crs->next;
	}
	item = gtk_list_item_new_with_label(cr->name);
	cr2 = g_new0(struct chat_room, 1);
	strcpy(cr2->name, cr->name);
	cr2->exchange = cr->exchange;
	gtk_object_set_user_data(GTK_OBJECT(item), cr2);
	gtk_widget_show(item);
	sel = g_list_append(NULL, item);
	gtk_list_append_items(GTK_LIST(cp->list2), sel);
	chat_rooms = g_list_append(chat_rooms, cr2);

	setup_buddy_chats();
	save_prefs();
	

}

void remove_chat(GtkWidget *w, struct chat_page *cp)
{
	GList *sel = GTK_LIST(cp->list2)->selection;
	struct chat_room *cr;
	GList *crs;
	GtkWidget *item;
	
	if (sel) {
		item = (GtkWidget *)sel->data;
		cr = (struct chat_room *)gtk_object_get_user_data(GTK_OBJECT(item));
	} else
		return;

	chat_rooms = g_list_remove(chat_rooms, cr);


	gtk_list_clear_items(GTK_LIST(cp->list2), 0, -1);

	if (g_list_length(chat_rooms) == 0)
		chat_rooms = NULL;

	crs = chat_rooms;

	while(crs) {
		cr = (struct chat_room *)crs->data;
		item = gtk_list_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		gtk_widget_show(item);
		gtk_list_append_items(GTK_LIST(cp->list2), g_list_append(NULL, item));

		
		crs = crs->next;
	}

	setup_buddy_chats();
        save_prefs();
}

void refresh_list(GtkWidget *w, struct chat_page *cp)
{
	char *text = grab_url("http://www.aol.com/community/chat/allchats.html");
	char *c;
	int len = strlen(text);
	GtkWidget *item;
	GList *items = GTK_LIST(cp->list1)->children;
	struct chat_room *cr;
	c = text;

	while(items) {
		g_free(gtk_object_get_user_data(GTK_OBJECT(items->data)));
		items = items->next;
	}

	items = NULL;

        gtk_list_clear_items(GTK_LIST(cp->list1), 0, -1);
	
	item = gtk_list_item_new_with_label(_("Gaim Chat"));
	cr = g_new0(struct chat_room, 1);
	strcpy(cr->name, _("Gaim Chat"));
	cr->exchange = 4;
	gtk_object_set_user_data(GTK_OBJECT(item), cr);
	gtk_widget_show(item);

        items = g_list_append(NULL, item);
	
	while(c) {
		if (c - text > len - 30)
			break; /* assume no chat rooms 30 from end, padding */
		if (!strncasecmp(AOL_SRCHSTR, c, strlen(AOL_SRCHSTR))) {
			char *t;
			int len=0;
			int exchange;
			char *name = NULL;
			
			c += strlen(AOL_SRCHSTR);
                        t = c;
			while(t) {
				len++;
				name = g_realloc(name, len);
				if (*t == '+')
					name[len - 1] = ' ';
				else if (*t == '&') {
					name[len - 1] = 0;
					sscanf(t, "&Exchange=%d", &exchange);
					c = t + strlen("&Exchange=x");
					break;
				} else
					name[len - 1] = *t;
				t++;
			}
			cr = g_new0(struct chat_room, 1);
			strcpy(cr->name, name);
			cr->exchange = exchange;
			item = gtk_list_item_new_with_label(name);
			gtk_widget_show(item);
			items = g_list_append(items, item);
			gtk_object_set_user_data(GTK_OBJECT(item), cr);
                        g_free(name);
		}
		c++;
	}
	gtk_list_append_items(GTK_LIST(cp->list1), items);
	g_free(text);
}



void do_chat_page(GtkWidget *page)
{
	GtkWidget *table;
	GtkWidget *rem_button, *add_button, *ref_button;
	GtkWidget *list1, *list2;
	GtkWidget *label;
	GtkWidget *sw1, *sw2;
	GtkWidget *item;
	struct chat_page *cp = g_new0(struct chat_page, 1);
	GList *crs = chat_rooms;
	GList *items = NULL;
	struct chat_room *cr;

	table = gtk_table_new(4, 2, FALSE);
	gtk_widget_show(table);
	

	gtk_box_pack_start(GTK_BOX(page), table, TRUE, TRUE, 0);


	list1 = gtk_list_new();
	list2 = gtk_list_new();
	sw1 = gtk_scrolled_window_new(NULL, NULL);
	sw2 = gtk_scrolled_window_new(NULL, NULL);
	ref_button = gtk_button_new_with_label(_("Refresh"));
	add_button = gtk_button_new_with_label(_("Add"));
	rem_button = gtk_button_new_with_label(_("Remove"));
	gtk_widget_show(list1);
	gtk_widget_show(sw1);
	gtk_widget_show(list2);
	gtk_widget_show(sw2);
	gtk_widget_show(ref_button);
	gtk_widget_show(add_button);
	gtk_widget_show(rem_button);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw1), list1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list2);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
				       GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);

	cp->list1 = list1;
	cp->list2 = list2;

	gtk_signal_connect(GTK_OBJECT(ref_button), "clicked",
			   GTK_SIGNAL_FUNC(refresh_list), cp);
	gtk_signal_connect(GTK_OBJECT(rem_button), "clicked",
			   GTK_SIGNAL_FUNC(remove_chat), cp);
	gtk_signal_connect(GTK_OBJECT(add_button), "clicked",
			   GTK_SIGNAL_FUNC(add_chat), cp);


	
	label = gtk_label_new(_("List of available chats"));
	gtk_widget_show(label);

	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			 GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), ref_button, 0, 1, 1, 2,
			 GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw1, 0, 1, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			 5, 5);
	gtk_table_attach(GTK_TABLE(table), add_button, 0, 1, 3, 4,
			 GTK_SHRINK, GTK_SHRINK, 0, 0);

	
	label = gtk_label_new(_("List of subscribed chats"));
	gtk_widget_show(label);
	
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
			 GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw2, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			 5, 5);
	gtk_table_attach(GTK_TABLE(table), rem_button, 1, 2, 3, 4,
			 GTK_SHRINK, GTK_SHRINK, 0, 0);


	item = gtk_list_item_new_with_label(_("Gaim Chat"));
	cr = g_new0(struct chat_room, 1);
	strcpy(cr->name, _("Gaim Chat"));
	cr->exchange = 4;
	gtk_object_set_user_data(GTK_OBJECT(item), cr);
	gtk_widget_show(item);
        gtk_list_append_items(GTK_LIST(list1), g_list_append(NULL, item));

	
	while(crs) {
		cr = (struct chat_room *)crs->data;
		item = gtk_list_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		gtk_widget_show(item);
		items = g_list_append(items, item);

		crs = crs->next;
	}

	gtk_list_append_items(GTK_LIST(list2), items);
}


	


void debug_print(char *chars)
{
	if(general_options & OPT_GEN_DEBUG && dw)
            gtk_text_insert(GTK_TEXT(dw->entry),NULL, NULL, NULL, chars, strlen(chars));
#ifdef DEBUG
        printf("%s\n", chars);
#endif
}


void build_debug()
{
	GtkWidget *scroll;
	GtkWidget *box;
	if (!dw)
		dw = g_new0(struct debug_window, 1);

	box = gtk_hbox_new(FALSE,0);
	dw->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(dw->window), _("GAIM debug output window"));
	gtk_container_add(GTK_CONTAINER(dw->window), box);
	dw->entry = gtk_text_new(NULL,NULL);
	gtk_widget_set_usize(dw->entry, 500, 200);
	scroll = gtk_vscrollbar_new(GTK_TEXT(dw->entry)->vadj);
	gtk_box_pack_start(GTK_BOX(box), dw->entry, TRUE,TRUE,0);
	gtk_box_pack_end(GTK_BOX(box), scroll,FALSE,FALSE,0);
	gtk_widget_show(dw->entry);
	gtk_widget_show(scroll);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(dw->window),"delete_event", GTK_SIGNAL_FUNC(debug_delete), NULL);
	gtk_widget_show(dw->window);
}



void show_debug(GtkObject * object)
{
	if((general_options & OPT_GEN_DEBUG)) {
                if(!dw || !dw->window)
                        build_debug();
                gtk_widget_show(dw->window);
        } else {
		if (!dw) return;
                gtk_widget_destroy(dw->window);
                dw->window = NULL;
	}
}

