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
#include <stdarg.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "prpl.h"
#include "pixmaps/cancel.xpm"
#include "pixmaps/fontface2.xpm"
#include "pixmaps/refresh.xpm"
#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/bgcolor.xpm"
#include "pixmaps/fgcolor.xpm"
#include "pixmaps/save.xpm"

struct debug_window *dw = NULL;
static GtkWidget *prefs = NULL;

static GtkWidget *gaim_button(const char *, int *, int, GtkWidget *);
static void prefs_build_general();
static void prefs_build_buddy();
static void prefs_build_convo();
static void prefs_build_sound();
static void prefs_build_away();
static void prefs_build_deny();
static gint handle_delete(GtkWidget *, GdkEvent *, void *);
static void delete_prefs(GtkWidget *, void *);
void set_default_away(GtkWidget *, gpointer);

static GtkWidget *sounddialog = NULL;
static GtkWidget *prefdialog = NULL;
static GtkWidget *debugbutton = NULL;

GtkWidget *prefs_away_list = NULL;
GtkWidget *prefs_away_menu = NULL;
GtkWidget *preftree = NULL;
GtkCTreeNode *general_node = NULL;
GtkCTreeNode *deny_node = NULL;

static void destdeb(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(debugbutton);
	debugbutton = NULL;
}

static void set_idle(GtkWidget *w, int *data)
{
	report_idle = (int)data;
	save_prefs();
}

static GtkWidget *idle_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_idle), (void *)which);
	gtk_widget_show(opt);
	if (report_idle == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void general_page()
{
	GtkWidget *parent;
	GtkWidget *box, *box2;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *idle;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("General Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	/*
	   prefrem = gaim_button(_("Remember password"), &general_options, OPT_GEN_REMEMBER_PASS, box);
	   gtk_signal_connect(GTK_OBJECT(prefrem), "destroy", GTK_SIGNAL_FUNC(remdes), 0);
	   gaim_button(_("Auto-login"), &general_options, OPT_GEN_AUTO_LOGIN, box);

	   sep = gtk_hseparator_new();
	   gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	   gtk_widget_show(sep);
	 */

	gaim_button(_("Use borderless buttons (requires restart for some buttons)"), &display_options,
		    OPT_DISP_COOL_LOOK, box);
	gaim_button(_("Show Buddy Ticker after restart"), &display_options, OPT_DISP_SHOW_BUDDYTICKER,
		    box);
	if (!dw && (general_options & OPT_GEN_DEBUG))
		general_options = general_options ^ OPT_GEN_DEBUG;
	debugbutton = gaim_button(_("Show Debug Window"), &general_options, OPT_GEN_DEBUG, box);
	gtk_signal_connect_object(GTK_OBJECT(debugbutton), "clicked", GTK_SIGNAL_FUNC(show_debug), 0);
	gtk_signal_connect(GTK_OBJECT(debugbutton), "destroy", GTK_SIGNAL_FUNC(destdeb), 0);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	box2 = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 5);
	gtk_widget_show(box2);

	label = gtk_label_new(_("Report Idle Times:"));
	gtk_box_pack_start(GTK_BOX(box2), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	idle = idle_radio(_("None"), IDLE_NONE, box2, NULL);
	idle = idle_radio(_("GAIM Use"), IDLE_GAIM, box2, idle);
#ifdef USE_SCREENSAVER
	idle = idle_radio(_("X Use"), IDLE_SCREENSAVER, box2, idle);
#endif

	gtk_widget_show(prefdialog);
}

static GtkWidget *browser_entry = NULL;
static GtkWidget *new_window = NULL;

static void set_browser(GtkWidget *w, int *data)
{
	web_browser = (int)data;
	if (web_browser != BROWSER_MANUAL) {
		if (browser_entry)
			gtk_widget_set_sensitive(browser_entry, FALSE);
	} else {
		if (browser_entry)
			gtk_widget_set_sensitive(browser_entry, TRUE);
	}

	if (web_browser != BROWSER_NETSCAPE) {
		if (new_window)
			gtk_widget_set_sensitive(new_window, FALSE);
	} else {
		if (new_window)
			gtk_widget_set_sensitive(new_window, TRUE);
	}


	save_prefs();
}

static int manualentry_key_pressed(GtkWidget *w, GdkEvent *event, void *dummy)
{
	g_snprintf(web_command, sizeof(web_command), "%s", gtk_entry_get_text(GTK_ENTRY(browser_entry)));
	save_prefs();
	return TRUE;
}

static GtkWidget *browser_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_browser), (void *)which);
	gtk_widget_show(opt);
	if (web_browser == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void brentdes(GtkWidget *m, gpointer n)
{
	browser_entry = NULL;
	new_window = NULL;
}

static void browser_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *opt;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Browser Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	opt = browser_radio(_("Netscape"), BROWSER_NETSCAPE, box, NULL);
	opt = browser_radio(_("KFM"), BROWSER_KFM, box, opt);
#ifdef USE_GNOME
	opt = browser_radio(_("GNOME URL Handler"), BROWSER_GNOME, box, opt);
#endif /* USE_GNOME */
	opt = browser_radio(_("Manual"), BROWSER_MANUAL, box, opt);

	browser_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), browser_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(browser_entry), web_command);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "focus_out_event",
			   GTK_SIGNAL_FUNC(manualentry_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "destroy", GTK_SIGNAL_FUNC(brentdes), NULL);
	gtk_widget_show(browser_entry);

	new_window =
	    gaim_button(_("Pop up new window by default"), &general_options, OPT_GEN_BROWSER_POPUP, box);

	if (web_browser != BROWSER_MANUAL) {
		gtk_widget_set_sensitive(browser_entry, FALSE);
	} else {
		gtk_widget_set_sensitive(browser_entry, TRUE);
	}

	if (web_browser != BROWSER_NETSCAPE) {
		gtk_widget_set_sensitive(new_window, FALSE);
	} else {
		gtk_widget_set_sensitive(new_window, TRUE);
	}

	gtk_widget_show(prefdialog);
}

static void logging_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Logging Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Log all conversations"), &logging_options, OPT_LOG_ALL, box);
	gaim_button(_("Strip HTML from conversationlogs"), &logging_options, OPT_LOG_STRIP_HTML, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	label = gtk_label_new(_("Nothing below is implemented yet"));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Log when buddies sign on/sign off"), &logging_options, OPT_LOG_BUDDY_SIGNON, box);
	gaim_button(_("Log when buddies become idle/un-idle"), &logging_options, OPT_LOG_BUDDY_IDLE, box);
	gaim_button(_("Log when buddies go away/come back"), &logging_options, OPT_LOG_BUDDY_AWAY, box);
	gaim_button(_("Log your own signons/idleness/awayness (as set by options above)"), &logging_options, OPT_LOG_MY_SIGNON, box);
	gaim_button(_("Individual log file for each buddy's signons"), &logging_options, OPT_LOG_INDIVIDUAL, box);

	gtk_widget_show(prefdialog);
}

static void buddy_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Buddy List Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Save Window Size/Position"), &general_options, OPT_GEN_SAVED_WINDOWS, box);
#ifdef USE_APPLET
	gaim_button(_("Automatically show buddy list on sign on"), &general_options,
		    OPT_GEN_APP_BUDDY_SHOW, box);
	gaim_button(_("Display Buddy List near applet"), &general_options, OPT_GEN_NEAR_APPLET, box);
#endif

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Hide groups with no online buddies"), &display_options, OPT_DISP_NO_MT_GRP, box);
	gaim_button(_("Show numbers in groups"), &display_options, OPT_DISP_SHOW_GRPNUM, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Show buddy type icons"), &display_options, OPT_DISP_SHOW_PIXMAPS, box);
	gaim_button(_("Show warning levels"), &display_options, OPT_DISP_SHOW_WARN, box);
	gaim_button(_("Show idle times"), &display_options, OPT_DISP_SHOW_IDLETIME, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Hide IM/Info/Chat buttons"), &display_options, OPT_DISP_NO_BUTTONS, box);
	gaim_button(_("Show pictures on buttons"), &display_options, OPT_DISP_SHOW_BUTTON_XPM, box);

	gtk_widget_show(prefdialog);
}

static void convo_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Conversation Window Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Enter sends message"), &general_options, OPT_GEN_ENTER_SENDS, box);
	gaim_button(_("Control-Enter sends message"), &general_options, OPT_GEN_CTL_ENTER, box);
	gaim_button(_("Escape closes window"), &general_options, OPT_GEN_ESC_CAN_CLOSE, box);
	gaim_button(_("Control-{B/I/U/S} inserts HTML tags"), &general_options, OPT_GEN_CTL_CHARS, box);
	gaim_button(_("Control-(number) inserts smileys"), &general_options, OPT_GEN_CTL_SMILEYS, box);
	gaim_button(_("F2 toggles timestamp display"), &general_options, OPT_GEN_F2_TOGGLES, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Show graphical smileys"), &display_options, OPT_DISP_SHOW_SMILEY, box);
	gaim_button(_("Show timestamp on messages"), &display_options, OPT_DISP_SHOW_TIME, box);
	gaim_button(_("Ignore colors"), &display_options, OPT_DISP_IGNORE_COLOUR, box);
	gaim_button(_("Ignore font faces"), &display_options, OPT_DISP_IGNORE_FONTS, box);
	gaim_button(_("Ignore font sizes"), &display_options, OPT_DISP_IGNORE_SIZES, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Highlight misspelled words"), &general_options, OPT_GEN_CHECK_SPELLING, box);
	gaim_button(_("Show URLs as links"), &general_options, OPT_GEN_SEND_LINKS, box);
	gaim_button(_("Sending messages removes away status"), &general_options, OPT_GEN_BACK_ON_IM,
		    box);

	gtk_widget_show(prefdialog);
}

static void set_buttons_opt(GtkWidget *w, int data)
{
	int mask;
	if (data & 0x1)		/* set the first bit if we're affecting chat buttons */
		mask = (OPT_DISP_CHAT_BUTTON_TEXT | OPT_DISP_CHAT_BUTTON_XPM);
	else
		mask = (OPT_DISP_CONV_BUTTON_TEXT | OPT_DISP_CONV_BUTTON_XPM);
	display_options &= ~(mask);
	display_options |= (data & mask);

	if (data & 0x1)
		update_chat_button_pix();
	else
		update_im_button_pix();
}

static void im_buttons_menu_init(GtkWidget *omenu)
{
	GtkWidget *menu, *opt;
	int index;

	switch (display_options & (OPT_DISP_CONV_BUTTON_TEXT | OPT_DISP_CONV_BUTTON_XPM)) {
	case OPT_DISP_CONV_BUTTON_TEXT:
		index = 2;
		break;
	case OPT_DISP_CONV_BUTTON_XPM:
		index = 1;
		break;
	default:		/* both or neither */
		index = 0;
		break;
	}

	menu = gtk_menu_new();

	opt = gtk_menu_item_new_with_label(_("Pictures and Text"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)(OPT_DISP_CONV_BUTTON_TEXT | OPT_DISP_CONV_BUTTON_XPM));
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	opt = gtk_menu_item_new_with_label(_("Pictures Only"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)OPT_DISP_CONV_BUTTON_XPM);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	opt = gtk_menu_item_new_with_label(_("Text Only"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)OPT_DISP_CONV_BUTTON_TEXT);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	set_default_away(menu, (gpointer)g_slist_index(away_messages, default_away));

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(omenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), index);
}

static void im_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *opt;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("IM Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Show all conversations in one tabbed window"), &display_options, OPT_DISP_ONE_WINDOW, box);
	gaim_button(_("Show logins in window"), &display_options, OPT_DISP_SHOW_LOGON, box);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Show buttons as "));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	opt = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), opt, FALSE, FALSE, 5);
	im_buttons_menu_init(opt);
	gtk_widget_show(opt);

	gaim_button(_("Show larger entry box on new windows"), &display_options, OPT_DISP_CONV_BIG_ENTRY,
		    box);
	gaim_button(_("Raise windows on events"), &general_options, OPT_GEN_POPUP_WINDOWS, box);
	gaim_button(_("Ignore new conversations when away"), &general_options, OPT_GEN_DISCARD_WHEN_AWAY,
		    box);
	gaim_button(_("Ignore TiK Automated Messages"), &general_options, OPT_GEN_TIK_HACK, box);

	gtk_widget_show(prefdialog);
}

static void chat_buttons_menu_init(GtkWidget *omenu)
{
	GtkWidget *menu, *opt;
	int index;

	switch (display_options & (OPT_DISP_CHAT_BUTTON_TEXT | OPT_DISP_CHAT_BUTTON_XPM)) {
	case OPT_DISP_CHAT_BUTTON_TEXT:
		index = 2;
		break;
	case OPT_DISP_CHAT_BUTTON_XPM:
		index = 1;
		break;
	default:		/* both or neither */
		index = 0;
		break;
	}

	menu = gtk_menu_new();

	opt = gtk_menu_item_new_with_label(_("Pictures and Text"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)(OPT_DISP_CHAT_BUTTON_TEXT | OPT_DISP_CHAT_BUTTON_XPM | 1));
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	opt = gtk_menu_item_new_with_label(_("Pictures Only"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)(OPT_DISP_CHAT_BUTTON_XPM | 1));
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	opt = gtk_menu_item_new_with_label(_("Text Only"));
	gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_buttons_opt),
			   (void *)(OPT_DISP_CHAT_BUTTON_TEXT | 1));
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(omenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), index);
}

static void chat_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *opt;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Chat Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Show people joining/leaving in window"), &display_options, OPT_DISP_CHAT_LOGON,
		    box);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Show buttons as "));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	opt = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), opt, FALSE, FALSE, 5);
	chat_buttons_menu_init(opt);
	gtk_widget_show(opt);


	gaim_button(_("Show larger entry box on new windows"), &display_options, OPT_DISP_CHAT_BIG_ENTRY,
		    box);
	gaim_button(_("Raise windows on events"), &general_options, OPT_GEN_POPUP_CHAT, box);

	gtk_widget_show(prefdialog);
}

struct chat_page {
	GtkWidget *list1;
	GtkWidget *list2;
};

static struct chat_page *cp = NULL;

static void refresh_list(GtkWidget *w, gpointer *m)
{
	char *text = grab_url(NULL, "http://www.aol.com/community/chat/allchats.html");
	char *c;
	int len = strlen(text);
	GtkWidget *item;
	GList *items = GTK_LIST(cp->list1)->children;
	struct chat_room *cr;
	c = text;

	while (items) {
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

	while (c) {
		if (c - text > len - 30)
			break;	/* assume no chat rooms 30 from end, padding */
		if (!strncasecmp(AOL_SRCHSTR, c, strlen(AOL_SRCHSTR))) {
			char *t;
			int len = 0;
			int exchange;
			char *name = NULL;

			c += strlen(AOL_SRCHSTR);
			t = c;
			while (t) {
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

static void add_chat(GtkWidget *w, gpointer *m)
{
	GList *sel = GTK_LIST(cp->list1)->selection;
	struct chat_room *cr, *cr2;
	GList *crs = chat_rooms;
	GtkWidget *item;

	if (sel) {
		cr = (struct chat_room *)gtk_object_get_user_data(GTK_OBJECT(sel->data));
	} else
		return;

	while (crs) {
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

static void remove_chat(GtkWidget *w, gpointer *m)
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

	while (crs) {
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

static void room_page()
{
	GtkWidget *table;
	GtkWidget *rem_button, *add_button, *ref_button;
	GtkWidget *list1, *list2;
	GtkWidget *label;
	GtkWidget *sw1, *sw2;
	GtkWidget *item;
	GList *crs = chat_rooms;
	GList *items = NULL;
	struct chat_room *cr;

	GtkWidget *parent;
	GtkWidget *box;

	if (!cp)
		g_free(cp);
	cp = g_new0(struct chat_page, 1);

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Chat Rooms"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	table = gtk_table_new(4, 2, FALSE);
	gtk_widget_show(table);

	gtk_box_pack_start(GTK_BOX(box), table, TRUE, TRUE, 0);

	list1 = gtk_list_new();
	list2 = gtk_list_new();
	sw1 = gtk_scrolled_window_new(NULL, NULL);
	sw2 = gtk_scrolled_window_new(NULL, NULL);

	ref_button = picture_button(prefs, _("Refresh"), refresh_xpm);
	add_button = picture_button(prefs, _("Add"), gnome_add_xpm);
	rem_button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_widget_show(list1);
	gtk_widget_show(sw1);
	gtk_widget_show(list2);
	gtk_widget_show(sw2);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw1), list1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list2);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	cp->list1 = list1;
	cp->list2 = list2;

	gtk_signal_connect(GTK_OBJECT(ref_button), "clicked", GTK_SIGNAL_FUNC(refresh_list), cp);
	gtk_signal_connect(GTK_OBJECT(rem_button), "clicked", GTK_SIGNAL_FUNC(remove_chat), cp);
	gtk_signal_connect(GTK_OBJECT(add_button), "clicked", GTK_SIGNAL_FUNC(add_chat), cp);



	label = gtk_label_new(_("List of available chats"));
	gtk_widget_show(label);

	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), ref_button, 0, 1, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw1, 0, 1, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), add_button, 0, 1, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0);


	label = gtk_label_new(_("List of subscribed chats"));
	gtk_widget_show(label);

	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_table_attach(GTK_TABLE(table), sw2, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 5);
	gtk_table_attach(GTK_TABLE(table), rem_button, 1, 2, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0);


	item = gtk_list_item_new_with_label(_("Gaim Chat"));
	cr = g_new0(struct chat_room, 1);
	strcpy(cr->name, _("Gaim Chat"));
	cr->exchange = 4;
	gtk_object_set_user_data(GTK_OBJECT(item), cr);
	gtk_widget_show(item);
	gtk_list_append_items(GTK_LIST(list1), g_list_append(NULL, item));


	while (crs) {
		cr = (struct chat_room *)crs->data;
		item = gtk_list_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		gtk_widget_show(item);
		items = g_list_append(items, item);

		crs = crs->next;
	}

	gtk_list_append_items(GTK_LIST(list2), items);

	gtk_widget_show(prefdialog);
}

static GtkWidget *show_color_pref(GtkWidget *box, gboolean fgc)
{
	/* more stuff stolen from X-Chat */
	GtkWidget *swid;
	GdkColor c;
	GtkStyle *style;
	c.pixel = 0;
	if (fgc) {
		if (font_options & OPT_FONT_FGCOL) {
			c.red = fgcolor.red << 8;
			c.blue = fgcolor.blue << 8;
			c.green = fgcolor.green << 8;
		} else {
			c.red = 0;
			c.blue = 0;
			c.green = 0;
		}
	} else {
		if (font_options & OPT_FONT_BGCOL) {
			c.red = bgcolor.red << 8;
			c.blue = bgcolor.blue << 8;
			c.green = bgcolor.green << 8;
		} else {
			c.red = 0xffff;
			c.blue = 0xffff;
			c.green = 0xffff;
		}
	}

	style = gtk_style_new();
	style->bg[0] = c;

	swid = gtk_event_box_new();
	gtk_widget_set_style(GTK_WIDGET(swid), style);
	gtk_style_unref(style);
	gtk_widget_set_usize(GTK_WIDGET(swid), 40, -1);
	gtk_box_pack_start(GTK_BOX(box), swid, FALSE, FALSE, 5);
	gtk_widget_show(swid);
	return swid;
}

GtkWidget *pref_fg_picture = NULL;
GtkWidget *pref_bg_picture = NULL;

void update_color(GtkWidget *w, GtkWidget *pic)
{
	GdkColor c;
	GtkStyle *style;
	c.pixel = 0;
	if (pic == pref_fg_picture) {
		if (font_options & OPT_FONT_FGCOL) {
			c.red = fgcolor.red << 8;
			c.blue = fgcolor.blue << 8;
			c.green = fgcolor.green << 8;
		} else {
			c.red = 0;
			c.blue = 0;
			c.green = 0;
		}
	} else {
		if (font_options & OPT_FONT_BGCOL) {
			c.red = bgcolor.red << 8;
			c.blue = bgcolor.blue << 8;
			c.green = bgcolor.green << 8;
		} else {
			c.red = 0xffff;
			c.blue = 0xffff;
			c.green = 0xffff;
		}
	}

	style = gtk_style_new();
	style->bg[0] = c;
	gtk_widget_set_style(pic, style);
	gtk_style_unref(style);
}

static void set_font_size(GtkWidget *w, GtkWidget *spin)
{
	fontsize = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
}

static void font_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *select;
	GtkWidget *spin;
	GtkObject *adjust;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Font Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Bold Text"), &font_options, OPT_FONT_BOLD, box);
	gaim_button(_("Italics Text"), &font_options, OPT_FONT_ITALIC, box);
	gaim_button(_("Underlined Text"), &font_options, OPT_FONT_UNDERLINE, box);
	gaim_button(_("Strike Text"), &font_options, OPT_FONT_STRIKE, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	pref_fg_picture = show_color_pref(hbox, TRUE);
	button = gaim_button(_("Text Color"), &font_options, OPT_FONT_FGCOL, hbox);

	select = picture_button(prefs, _("Select"), fgcolor_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 5);
	if (!(font_options & OPT_FONT_FGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_fgcolor_dialog), NULL);
	gtk_widget_show(select);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_fg_picture);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	pref_bg_picture = show_color_pref(hbox, FALSE);
	button = gaim_button(_("Background Color"), &font_options, OPT_FONT_BGCOL, hbox);

	select = picture_button(prefs, _("Select"), bgcolor_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 5);
	if (!(font_options & OPT_FONT_BGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_bgcolor_dialog), NULL);
	gtk_widget_show(select);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_bg_picture);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = gaim_button(_("Font Face for Text"), &font_options, OPT_FONT_FACE, hbox);

	select = picture_button(prefs, _("Select"), fontface2_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	if (!(font_options & OPT_FONT_FACE))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_font_dialog), NULL);
	gtk_widget_show(select);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = gaim_button(_("Font Size for Text"), &font_options, OPT_FONT_SIZE, hbox);

	adjust = gtk_adjustment_new(fontsize, 1, 7, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(spin, 50, -1);
	if (!(font_options & OPT_FONT_SIZE))
		gtk_widget_set_sensitive(GTK_WIDGET(spin), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), spin);
	gtk_signal_connect(GTK_OBJECT(adjust), "value-changed", GTK_SIGNAL_FUNC(set_font_size),
			   GTK_WIDGET(spin));
	gtk_widget_show(spin);

	gtk_widget_show(prefdialog);
}

static void sound_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Sound Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("No sounds when you log in"), &sound_options, OPT_SOUND_SILENT_SIGNON, box);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, box);
	gaim_button(_("Beep instead of playing sound"), &sound_options, OPT_SOUND_BEEP, box);

	gtk_widget_show(prefdialog);
}

static GtkWidget *sndent[NUM_SOUNDS];
static GtkWidget *sndcmd = NULL;

void close_sounddialog(GtkWidget *w, GtkWidget *w2)
{

	GtkWidget *dest;

	if (!GTK_IS_WIDGET(w2))
		dest = w;
	else
		dest = w2;

	sounddialog = NULL;

	gtk_widget_destroy(dest);
}

void do_select_sound(GtkWidget *w, int snd)
{

	char *file;

	file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(sounddialog));

	/* If they type in a directory, change there */
	if (file_is_dir(file, sounddialog))
		return;

	/* Let's just be safe */
	if (sound_file[snd])
		free(sound_file[snd]);

	/* Set it -- and forget it */
	sound_file[snd] = g_strdup(file);

	save_prefs();

	/* Set our text entry */
	gtk_entry_set_text(GTK_ENTRY(sndent[snd]), sound_file[snd]);

	/* Close the window! It's getting cold in here! */
	close_sounddialog(NULL, sounddialog);
}

static void reset_sound(GtkWidget *button, int snd)
{

	/* This just resets a sound file back to default */
	sound_file[snd] = NULL;

	gtk_entry_set_text(GTK_ENTRY(sndent[snd]), "(default)");
}

static void sel_sound(GtkWidget *button, int snd)
{

	char *buf = g_malloc(BUF_LEN);

	if (!sounddialog) {
		sounddialog = gtk_file_selection_new(_("Gaim - Sound Configuration"));

		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(sounddialog));

		g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("HOME"));

		gtk_file_selection_set_filename(GTK_FILE_SELECTION(sounddialog), buf);

		gtk_signal_connect(GTK_OBJECT(sounddialog), "destroy",
				   GTK_SIGNAL_FUNC(close_sounddialog), sounddialog);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(sounddialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(do_select_sound), (int *)snd);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(sounddialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(close_sounddialog), sounddialog);
	}

	g_free(buf);
	gtk_widget_show(sounddialog);
	gdk_window_raise(sounddialog->window);
}

static void sound_entry(char *label, int opt, GtkWidget *box, int snd)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *button;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gaim_button(label, &sound_options, opt, hbox);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(reset_sound), (void *)snd);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Choose..."));
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(sel_sound), (void *)snd);
	gtk_widget_show(button);

	entry = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);

	if (sound_file[snd])
		gtk_entry_set_text(GTK_ENTRY(entry), sound_file[snd]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "(default)");

	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	sndent[snd] = entry;
	gtk_widget_show(entry);
}

static gint sound_cmd_yeah(GtkEntry * entry, GdkEvent *event, gpointer d)
{
	g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", gtk_entry_get_text(GTK_ENTRY(sndcmd)));
	save_prefs();
	return TRUE;
}

static void event_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *hbox;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Sound Events"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sound_entry(_("Sound when buddy logs in"), OPT_SOUND_LOGIN, box, BUDDY_ARRIVE);
	sound_entry(_("Sound when buddy logs out"), OPT_SOUND_LOGOUT, box, BUDDY_LEAVE);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	sound_entry(_("Sound when message is received"), OPT_SOUND_RECV, box, RECEIVE);
	sound_entry(_("Sound when message is first received"), OPT_SOUND_FIRST_RCV, box, FIRST_RECEIVE);
	sound_entry(_("Sound when message is sent"), OPT_SOUND_SEND, box, SEND);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	sound_entry(_("Sound in chat rooms when people enter"), OPT_SOUND_CHAT_JOIN, box, CHAT_JOIN);
	sound_entry(_("Sound in chat rooms when people leave"), OPT_SOUND_CHAT_PART, box, CHAT_LEAVE);
	sound_entry(_("Sound in chat rooms when you talk"), OPT_SOUND_CHAT_YOU_SAY, box, CHAT_YOU_SAY);
	sound_entry(_("Sound in chat rooms when others talk"), OPT_SOUND_CHAT_SAY, box, CHAT_SAY);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Command to play sound files (%s for filename; internal if empty)"));

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sndcmd = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(sndcmd), TRUE);
	gtk_entry_set_text(GTK_ENTRY(sndcmd), sound_cmd);
	gtk_box_pack_end(GTK_BOX(hbox), sndcmd, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(sndcmd), "focus_out_event", GTK_SIGNAL_FUNC(sound_cmd_yeah), NULL);
	gtk_widget_show(sndcmd);

	gtk_widget_show(prefdialog);
}

static struct away_message *cur_message;
static char *edited_message;
static GtkWidget *away_text;
static GtkWidget *make_away_button = NULL;;

void away_list_clicked(GtkWidget *widget, struct away_message *a)
{
	gchar buffer[2048];
	guint text_len;

	cur_message = a;

	/* Get proper Length */
	text_len = gtk_text_get_length(GTK_TEXT(away_text));

	/* Clear the Box */
	gtk_text_set_point(GTK_TEXT(away_text), 0);
	gtk_text_forward_delete(GTK_TEXT(away_text), text_len);

	/* Fill the text box with new message */
	strcpy(buffer, a->message);
	gtk_text_insert(GTK_TEXT(away_text), NULL, NULL, NULL, buffer, -1);
}

void save_away_message(GtkWidget *widget, void *dummy)
{
	/* grab the current message */
	edited_message = gtk_editable_get_chars(GTK_EDITABLE(away_text), 0, -1);
	strcpy(cur_message->message, edited_message);
	save_prefs();
}

void remove_away_message(GtkWidget *widget, void *dummy)
{
	GList *i;
	struct away_message *a;

	i = GTK_LIST(prefs_away_list)->selection;

	if (!i)
		return;
	if (!i->next) {
		int text_len = gtk_text_get_length(GTK_TEXT(away_text));
		gtk_text_set_point(GTK_TEXT(away_text), 0);
		gtk_text_forward_delete(GTK_TEXT(away_text), text_len);
	}
	a = gtk_object_get_user_data(GTK_OBJECT(i->data));
	rem_away_mess(NULL, a);
}

static void paldest(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(prefs_away_list);
	prefs_away_list = NULL;
	make_away_button = NULL;
}

static void do_away_mess(GtkWidget *m, gpointer n)
{
	GList *i = GTK_LIST(prefs_away_list)->selection;
	if (i)
		do_away_message(NULL, gtk_object_get_user_data(GTK_OBJECT(i->data)));
}

static void set_auto_away(GtkWidget *w, GtkWidget *spin)
{
	auto_away = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
}

void set_default_away(GtkWidget *w, gpointer i)
{
	int length = g_slist_length(away_messages);

	if (away_messages == NULL)
		default_away = NULL;
	else if ((int)i >= length)
		default_away = g_slist_nth_data(away_messages, length - 1);
	else
		default_away = g_slist_nth_data(away_messages, (int)i);
}

void default_away_menu_init(GtkWidget *omenu)
{
	GtkWidget *menu, *opt;
	int index = 0;
	GSList *awy = away_messages;
	struct away_message *a;

	menu = gtk_menu_new();

	while (awy) {
		a = (struct away_message *)awy->data;
		opt = gtk_menu_item_new_with_label(a->name);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_default_away),
				   (gpointer)index);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);

		awy = awy->next;
		index++;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(omenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), g_slist_index(away_messages, default_away));
}



static void away_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *top;
	GtkWidget *bot;
	GtkWidget *sw;
	GtkWidget *sw2;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *list_item;
	GtkWidget *sep;
	GtkObject *adjust;
	GtkWidget *spin;
	GSList *awy = away_messages;
	struct away_message *a;
	char buffer[BUF_LONG];

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Away Messages"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_widget_show(box);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Title"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	label = gtk_label_new(_("Message"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	top = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), top, TRUE, TRUE, 0);
	gtk_widget_show(top);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(top), sw, TRUE, TRUE, 0);
	gtk_widget_set_usize(sw, -1, 225);
	gtk_widget_show(sw);

	prefs_away_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), prefs_away_list);
	gtk_signal_connect(GTK_OBJECT(prefs_away_list), "destroy", GTK_SIGNAL_FUNC(paldest), 0);
	gtk_widget_show(prefs_away_list);

	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(top), sw2, TRUE, TRUE, 0);
	gtk_widget_show(sw2);

	away_text = gtk_text_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw2), away_text);
	gtk_text_set_word_wrap(GTK_TEXT(away_text), TRUE);
	gtk_text_set_editable(GTK_TEXT(away_text), FALSE);
	gtk_widget_show(away_text);

	bot = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), bot, FALSE, FALSE, 5);
	gtk_widget_show(bot);

	button = picture_button(prefs, _("Add"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), NULL);
	gtk_box_pack_start(GTK_BOX(bot), button, TRUE, FALSE, 5);

	button = picture_button(prefs, _("Edit"), save_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), button);
	gtk_box_pack_start(GTK_BOX(bot), button, TRUE, FALSE, 5);

	make_away_button = picture_button(prefs, _("Make Away"), gnome_preferences_xpm);
	gtk_signal_connect(GTK_OBJECT(make_away_button), "clicked", GTK_SIGNAL_FUNC(do_away_mess), NULL);
	gtk_box_pack_start(GTK_BOX(bot), make_away_button, TRUE, FALSE, 5);
	if (!connections)
		gtk_widget_set_sensitive(make_away_button, FALSE);

	button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(remove_away_message), NULL);
	gtk_box_pack_start(GTK_BOX(bot), button, TRUE, FALSE, 5);

	if (awy != NULL) {
		a = (struct away_message *)awy->data;
		g_snprintf(buffer, sizeof(buffer), "%s", a->message);
		gtk_text_insert(GTK_TEXT(away_text), NULL, NULL, NULL, buffer, -1);
	}

	while (awy) {
		a = (struct away_message *)awy->data;
		label = gtk_label_new(a->name);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_list_clicked),
				   a);
/*                gtk_signal_connect(GTK_OBJECT(list_item), "deselect", GTK_SIGNAL_FUNC(away_list_unclicked), a);*/
		gtk_object_set_user_data(GTK_OBJECT(list_item), a);

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(prefs_away_list), list_item);
		gtk_widget_show(list_item);

		awy = awy->next;
	}

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gaim_button(_("Ignore new conversations when away   "), &general_options,
		    OPT_GEN_DISCARD_WHEN_AWAY, hbox);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, hbox);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gaim_button(_("Auto Away after"), &general_options, OPT_GEN_AUTO_AWAY, hbox);

	adjust = gtk_adjustment_new(auto_away, 1, 1440, 1, 10, 10);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(spin, 50, -1);
	if (!(general_options & OPT_GEN_AUTO_AWAY))
		gtk_widget_set_sensitive(GTK_WIDGET(spin), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), spin);
	gtk_signal_connect(GTK_OBJECT(adjust), "value-changed", GTK_SIGNAL_FUNC(set_auto_away),
			   GTK_WIDGET(spin));
	gtk_widget_show(spin);

	label = gtk_label_new(_("minutes using"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	prefs_away_menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), prefs_away_menu, FALSE, FALSE, 0);
	default_away_menu_init(prefs_away_menu);
	if (!(general_options & OPT_GEN_AUTO_AWAY))
		gtk_widget_set_sensitive(GTK_WIDGET(prefs_away_menu), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),
			   prefs_away_menu);
	gtk_widget_show(prefs_away_menu);

	gtk_widget_show(prefdialog);
}

static GtkWidget *deny_conn_hbox = NULL;
static GtkWidget *deny_opt_menu = NULL;
static struct gaim_connection *current_deny_gc = NULL;
static gboolean current_is_deny = FALSE;
static GtkWidget *allow_list = NULL;
static GtkWidget *block_list = NULL;

static void set_deny_mode(GtkWidget *w, int data)
{
	current_deny_gc->permdeny = data;
	serv_set_permit_deny(current_deny_gc);
	do_export(NULL, 0);
}

static GtkWidget *deny_opt(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_deny_mode), (void *)which);
	gtk_widget_show(opt);
	if (current_deny_gc->permdeny == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void des_deny_opt(GtkWidget *d, gpointer e)
{
	gtk_widget_destroy(d);
	current_deny_gc = NULL;
	deny_conn_hbox = NULL;
	deny_opt_menu = NULL;
	current_is_deny = FALSE;
	allow_list = NULL;
	block_list = NULL;
}

void build_allow_list()
{
	GtkWidget *label;
	GtkWidget *list_item;
	GSList *p;

	if (!current_is_deny)
		return;

	p = current_deny_gc->permit;

	gtk_list_remove_items(GTK_LIST(allow_list), GTK_LIST(allow_list)->children);

	while (p) {
		label = gtk_label_new(p->data);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p->data);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(allow_list), list_item);
		gtk_widget_show(list_item);
		p = p->next;
	}
}

void build_block_list()
{
	GtkWidget *label;
	GtkWidget *list_item;
	GSList *d;

	if (!current_is_deny)
		return;

	d = current_deny_gc->deny;

	gtk_list_remove_items(GTK_LIST(block_list), GTK_LIST(block_list)->children);

	while (d) {
		label = gtk_label_new(d->data);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_object_set_user_data(GTK_OBJECT(list_item), d->data);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(block_list), list_item);
		gtk_widget_show(list_item);
		d = d->next;
	}
}

static void deny_gc_opt(GtkWidget *opt, struct gaim_connection *gc)
{
	current_deny_gc = gc;
	build_allow_list();
	build_block_list();
}

static void build_deny_menu()
{
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *c = connections;
	struct gaim_connection *gc;
	int count = 0;
	gboolean found = FALSE;
	char buf[2048];

	if (g_slist_length(connections) == 1) {
		gtk_widget_hide(deny_conn_hbox);
		return;
	} else
		gtk_widget_show(deny_conn_hbox);

	menu = gtk_menu_new();

	while (c) {
		gc = (struct gaim_connection *)c->data;
		c = c->next;
		if (!gc->prpl->set_permit_deny)
			continue;
		g_snprintf(buf, sizeof buf, "%s (%s)", gc->username, (*gc->prpl->name)());
		opt = gtk_menu_item_new_with_label(buf);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(deny_gc_opt), gc);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);
		if (gc == current_deny_gc)
			found = TRUE;
		else if (!found)
			count++;
	}

	if (!found) {
		current_deny_gc = connections->data;
		count = 0;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(deny_opt_menu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(deny_opt_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(deny_opt_menu), count);

	gtk_widget_show(menu);
	gtk_widget_show(deny_opt_menu);
}

static void pref_deny_add(GtkWidget *button, gboolean permit)
{
	show_add_perm(current_deny_gc, NULL, permit);
}

static void pref_deny_rem(GtkWidget *button, gboolean permit)
{
	GList *i;
	char *who;

	if (permit && !allow_list)
		return;
	if (!permit && !block_list)
		return;

	if (permit)
		i = GTK_LIST(allow_list)->selection;
	else
		i = GTK_LIST(block_list)->selection;

	if (!i)
		return;
	who = gtk_object_get_user_data(GTK_OBJECT(i->data));
	if (permit) {
		current_deny_gc->permit = g_slist_remove(current_deny_gc->permit, who);
		serv_rem_permit(current_deny_gc, who);
		build_allow_list();
	} else {
		current_deny_gc->deny = g_slist_remove(current_deny_gc->deny, who);
		serv_rem_deny(current_deny_gc, who);
		build_block_list();
	}

	do_export(0, 0);
}

static void deny_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *opt;
	GtkWidget *sw;
	GtkWidget *bbox;
	GtkWidget *button;

	current_deny_gc = connections->data;	/* this is safe because this screen will only be
						   available when there are connections */
	current_is_deny = TRUE;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Privacy Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	deny_conn_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), deny_conn_hbox, FALSE, FALSE, 0);
	gtk_widget_show(deny_conn_hbox);

	label = gtk_label_new(_("Set privacy for:"));
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	deny_opt_menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), deny_opt_menu, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(deny_opt_menu), "destroy", GTK_SIGNAL_FUNC(des_deny_opt), NULL);
	gtk_widget_show(deny_opt_menu);

	build_deny_menu();

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 5);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	opt = deny_opt(_("Allow all users to contact me"), 1, vbox, NULL);
	opt = deny_opt(_("Allow only the users below"), 3, vbox, opt);

	label = gtk_label_new(_("Allow List"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);
	gtk_widget_show(sw);

	allow_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), allow_list);
	gtk_widget_show(allow_list);

	build_allow_list();

	bbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_widget_show(bbox);

	button = picture_button(prefs, _("Add"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_add), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_rem), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	opt = deny_opt(_("Deny all users"), 2, vbox, opt);
	opt = deny_opt(_("Block the users below"), 4, vbox, opt);

	label = gtk_label_new(_("Block List"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);
	gtk_widget_show(sw);

	block_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), block_list);
	gtk_widget_show(block_list);

	build_block_list();

	bbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_widget_show(bbox);

	button = picture_button(prefs, _("Add"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_add), FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_rem), FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	gtk_widget_show(prefdialog);
}

void update_connection_dependent_prefs()	/* what a crappy name */
{
	gboolean needdeny = FALSE;
	GSList *c = connections;
	struct gaim_connection *gc = NULL;

	if (!prefs)
		return;

	while (c) {
		gc = c->data;
		if (gc->prpl->set_permit_deny)
			break;
		gc = NULL;
		c = c->next;
	}
	needdeny = (gc != NULL);

	if (!needdeny && deny_node) {
		if (current_is_deny)
			gtk_ctree_select(GTK_CTREE(preftree), general_node);
		gtk_ctree_remove_node(GTK_CTREE(preftree), deny_node);
		deny_node = NULL;
	} else if (deny_node && current_is_deny) {
		build_deny_menu();
		build_allow_list();
		build_block_list();
	} else if (needdeny && !deny_node) {
		prefs_build_deny();
	}

	if (make_away_button) {
		if (connections)
			gtk_widget_set_sensitive(make_away_button, TRUE);
		else
			gtk_widget_set_sensitive(make_away_button, FALSE);
	}
}

static void try_me(GtkCTree * ctree, GtkCTreeNode * node)
{
	/* this is a hack */
	void (*func)();
	func = gtk_ctree_node_get_row_data(ctree, node);
	(*func)();
}

void show_prefs()
{
	GtkWidget *vbox;
	GtkWidget *hpaned;
	GtkWidget *scroll;
	GtkWidget *container;
	GtkWidget *hbox;
	GtkWidget *close;

	if (prefs) {
		gtk_widget_show(prefs);
		return;
	}

	prefs = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(prefs), "preferences", "Gaim");
	gtk_widget_realize(prefs);
	aol_icon(prefs->window);
	gtk_window_set_title(GTK_WINDOW(prefs), _("Gaim - Preferences"));
	gtk_widget_set_usize(prefs, 690, 600);
	gtk_signal_connect(GTK_OBJECT(prefs), "destroy", GTK_SIGNAL_FUNC(delete_prefs), NULL);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(prefs), vbox);
	gtk_widget_show(vbox);

	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show(hpaned);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_paned_pack1(GTK_PANED(hpaned), scroll, FALSE, FALSE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_widget_set_usize(scroll, 125, -1);
	gtk_widget_show(scroll);

	preftree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style(GTK_CTREE(preftree), GTK_CTREE_LINES_SOLID);
	gtk_ctree_set_expander_style(GTK_CTREE(preftree), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_clist_set_reorderable(GTK_CLIST(preftree), FALSE);
	gtk_container_add(GTK_CONTAINER(scroll), preftree);
	gtk_signal_connect(GTK_OBJECT(preftree), "tree_select_row", GTK_SIGNAL_FUNC(try_me), NULL);
	gtk_widget_show(preftree);

	container = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(container), 0);
	gtk_frame_set_shadow_type(GTK_FRAME(container), GTK_SHADOW_NONE);
	gtk_paned_pack2(GTK_PANED(hpaned), container, TRUE, TRUE);
	gtk_widget_show(container);

	prefdialog = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(container), prefdialog);
	gtk_widget_show(prefdialog);

	prefs_build_general();
	prefs_build_buddy();
	prefs_build_convo();
	prefs_build_sound();
	prefs_build_away();
	prefs_build_deny();

	//general_page();

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	close = picture_button(prefs, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(handle_delete), NULL);

	gtk_widget_show(prefs);
}

static gint debug_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	if (debugbutton)
		gtk_button_clicked(GTK_BUTTON(debugbutton));
	if (general_options & OPT_GEN_DEBUG) {
		general_options = general_options ^ (int)OPT_GEN_DEBUG;
		save_prefs();
	}
	g_free(dw);
	dw = NULL;
	return FALSE;

}

static void build_debug()
{
	GtkWidget *scroll;
	GtkWidget *box;
	if (!dw)
		dw = g_new0(struct debug_window, 1);

	box = gtk_hbox_new(FALSE, 0);
	dw->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(dw->window), _("GAIM debug output window"));
	gtk_window_set_wmclass(GTK_WINDOW(dw->window), "debug_out", "Gaim");
	gtk_container_add(GTK_CONTAINER(dw->window), box);
	dw->entry = gtk_text_new(NULL, NULL);
	gtk_widget_set_usize(dw->entry, 500, 200);
	scroll = gtk_vscrollbar_new(GTK_TEXT(dw->entry)->vadj);
	gtk_box_pack_start(GTK_BOX(box), dw->entry, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(box), scroll, FALSE, FALSE, 0);
	gtk_widget_show(dw->entry);
	gtk_widget_show(scroll);
	gtk_widget_show(box);
	gtk_signal_connect(GTK_OBJECT(dw->window), "delete_event", GTK_SIGNAL_FUNC(debug_delete), NULL);
	gtk_widget_show(dw->window);
}

void show_debug(GtkObject * obj)
{
	if ((general_options & OPT_GEN_DEBUG)) {
		if (!dw || !dw->window)
			build_debug();
		gtk_widget_show(dw->window);
	} else {
		if (!dw)
			return;
		gtk_widget_destroy(dw->window);
		dw->window = NULL;
	}
}

void debug_printf(char *fmt, ...)
{
	va_list ap;
	gchar *s;

	va_start(ap, fmt);
	s = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	if (general_options & OPT_GEN_DEBUG && dw) {
		gtk_text_insert(GTK_TEXT(dw->entry), NULL, NULL, NULL, s, -1);
	}

#ifdef DEBUG
	g_print("%s", s);
#endif
	g_free(s);
}

static gint handle_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	save_prefs();

	if (cp)
		g_free(cp);
	cp = NULL;

	if (event == NULL)
		gtk_widget_destroy(prefs);
	prefs = NULL;
	prefdialog = NULL;
	debugbutton = NULL;
	prefs_away_menu = NULL;

	return FALSE;
}

static void delete_prefs(GtkWidget *w, void *data)
{
	if (prefs) {
		save_prefs();
		gtk_widget_destroy(prefs);
	}
	prefs = NULL;
	prefs_away_menu = NULL;
	deny_node = NULL;
	current_deny_gc = NULL;
}


void set_option(GtkWidget *w, int *option)
{
	*option = !(*option);
}

void set_general_option(GtkWidget *w, int *option)
{
	general_options = general_options ^ (int)option;

	if ((int)option == OPT_GEN_CHECK_SPELLING)
		toggle_spellchk();

	save_prefs();
}

void set_display_option(GtkWidget *w, int *option)
{
	display_options = display_options ^ (int)option;

	if (blist && ((int)option == OPT_DISP_NO_BUTTONS)) {
		build_imchat_box(!(display_options & OPT_DISP_NO_BUTTONS));
		update_button_pix();
	}

	if (blist && ((int)option == OPT_DISP_SHOW_GRPNUM))
		update_num_groups();

	if (blist && ((int)option == OPT_DISP_NO_MT_GRP))
		toggle_show_empty_groups();

	if (blist && ((int)option == OPT_DISP_SHOW_BUTTON_XPM))
		update_button_pix();

	if ((int)option == OPT_DISP_SHOW_SMILEY)
		toggle_smileys();

	if ((int)option == OPT_DISP_SHOW_TIME)
		toggle_timestamps();

	if ((int)option == OPT_DISP_ONE_WINDOW)
		tabize();

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

void set_logging_option(GtkWidget *w, int *option)
{
	logging_options = logging_options ^ (int)option;

	if ((int)option == OPT_LOG_ALL)
		update_log_convs();

	save_prefs();
}

GtkWidget *gaim_button(const char *text, int *options, int option, GtkWidget *page)
{
	GtkWidget *button;
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (*options & option));
	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	if (options == &font_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_font_option),
				   (int *)option);
	if (options == &sound_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_sound_option),
				   (int *)option);
	if (options == &display_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_display_option),
				   (int *)option);
	if (options == &general_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_general_option),
				   (int *)option);
	if (options == &logging_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_logging_option),
				   (int *)option);
	gtk_widget_show(button);

	return button;
}

void prefs_build_general()
{
	GtkCTreeNode *node;
	char *text[1];

	text[0] = _("General");
	general_node = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), general_node, general_page);

	text[0] = _("Browser");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), general_node, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, browser_page);

	text[0] = _("Logging");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), general_node, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, logging_page);

	gtk_ctree_select(GTK_CTREE(preftree), general_node);
}

void prefs_build_buddy()
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("Buddy List");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, buddy_page);
}

void prefs_build_convo()
{
	GtkCTreeNode *parent, *node, *node2;
	char *text[1];

	text[0] = _("Conversations");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, convo_page);

	text[0] = _("IM Window");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, im_page);

	text[0] = _("Chat Window");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, chat_page);

	text[0] = _("Chat Rooms");
	node2 = gtk_ctree_insert_node(GTK_CTREE(preftree), node, NULL,
				      text, 5, NULL, NULL, NULL, NULL, 1, 0);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node2, room_page);

	text[0] = _("Font Options");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, font_page);
}

void prefs_build_sound()
{
	GtkCTreeNode *parent, *node;
	char *text[1];

	text[0] = _("Sounds");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, sound_page);

	text[0] = _("Events");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, event_page);
}

void prefs_build_away()
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("Away Messages");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, away_page);
}

void prefs_build_deny()
{
	char *text[1];

	if (connections && !deny_node) {
		text[0] = _("Privacy");
		deny_node = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
						  text, 5, NULL, NULL, NULL, NULL, 0, 1);
		gtk_ctree_node_set_row_data(GTK_CTREE(preftree), deny_node, deny_page);
	}
}
