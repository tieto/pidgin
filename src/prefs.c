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

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include "gaim.h"
#include "prpl.h"
#include "pixmaps/cancel.xpm"
#include "pixmaps/fontface2.xpm"
#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/bgcolor.xpm"
#include "pixmaps/fgcolor.xpm"
#include "pixmaps/save.xpm"
#include "proxy.h"

struct debug_window *dw = NULL;
static GtkWidget *prefs = NULL;

static GtkWidget *gaim_button(const char *, guint *, int, GtkWidget *);
static GtkWidget *blist_tab_radio(const char *, int, GtkWidget *, GtkWidget *);
static void prefs_build_general();
static void prefs_build_buddy();
static void prefs_build_convo();
static void prefs_build_sound();
static void prefs_build_away();
static void prefs_build_deny();
static gint handle_delete(GtkWidget *, GdkEvent *, void *);
static void delete_prefs(GtkWidget *, void *);
void set_default_away(GtkWidget *, gpointer);
static void set_font_option(GtkWidget *w, int option);

static GtkWidget *sounddialog = NULL;
static GtkWidget *prefdialog = NULL;
static GtkWidget *debugbutton = NULL;
static GtkWidget *tickerbutton = NULL;

extern GtkWidget *tickerwindow;
extern void BuddyTickerShow();

GtkWidget *prefs_away_list = NULL;
GtkWidget *prefs_away_menu = NULL;
GtkWidget *preftree = NULL;
GtkCTreeNode *general_node = NULL;
GtkCTreeNode *deny_node = NULL;
GtkWidget *prefs_proxy_frame = NULL;

static void destdeb(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(debugbutton);
	debugbutton = NULL;
}

static void desttkr(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(tickerbutton);
	tickerbutton = NULL;
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

static void general_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *mbox;
	GtkWidget *sep;
	GtkWidget *idle;
	GtkWidget *opt;
	GtkWidget *typingbutton;

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

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	frame = gtk_frame_new(_("Miscellaneous"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
	gtk_widget_show(frame);

	mbox = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), mbox);
	gtk_widget_show(mbox);

	gaim_button(_("Use borderless buttons"), &misc_options, OPT_MISC_COOL_LOOK, mbox);

	if (!tickerwindow && (misc_options & OPT_MISC_BUDDY_TICKER))
		misc_options ^= OPT_MISC_BUDDY_TICKER;
	tickerbutton = gaim_button(_("Show Buddy Ticker"), &misc_options, OPT_MISC_BUDDY_TICKER, mbox);
	gtk_signal_connect(GTK_OBJECT(tickerbutton), "destroy", GTK_SIGNAL_FUNC(desttkr), 0);

	if (!dw && (misc_options & OPT_MISC_DEBUG))
		misc_options ^= OPT_MISC_DEBUG;
	debugbutton = gaim_button(_("Show Debug Window"), &misc_options, OPT_MISC_DEBUG, mbox);
	gtk_signal_connect(GTK_OBJECT(debugbutton), "destroy", GTK_SIGNAL_FUNC(destdeb), 0);
	
	/* Preferences should be positive */
	typingbutton = gaim_button(_("Notify buddies that you are typing to them"), &misc_options,
				   OPT_MISC_STEALTH_TYPING, mbox);

	/* So we have to toggle it */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(typingbutton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(typingbutton)));
	misc_options ^= OPT_MISC_STEALTH_TYPING;
	
	frame = gtk_frame_new(_("Report Idle Times"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
	gtk_widget_show(frame);

	mbox = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), mbox);
	gtk_widget_show(mbox);

	idle = idle_radio(_("None"), IDLE_NONE, mbox, NULL);
	idle = idle_radio(_("Gaim Use"), IDLE_GAIM, mbox, idle);
#ifdef USE_SCREENSAVER
	idle = idle_radio(_("X Use"), IDLE_SCREENSAVER, mbox, idle);
#endif

	frame = gtk_frame_new(_("Logging"));
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 5);
	gtk_widget_show(frame);

	mbox = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), mbox);
	gtk_widget_show(mbox);

	gaim_button(_("Log all conversations"), &logging_options, OPT_LOG_ALL, mbox);
	gaim_button(_("Strip HTML from logs"), &logging_options, OPT_LOG_STRIP_HTML, mbox);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(mbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	gaim_button(_("Log when buddies sign on/sign off"), &logging_options, OPT_LOG_BUDDY_SIGNON,
		    mbox);
	gaim_button(_("Log when buddies become idle/un-idle"), &logging_options, OPT_LOG_BUDDY_IDLE,
		    mbox);
	gaim_button(_("Log when buddies go away/come back"), &logging_options, OPT_LOG_BUDDY_AWAY, mbox);
	gaim_button(_("Log your own signons/idleness/awayness"), &logging_options, OPT_LOG_MY_SIGNON,
		    mbox);
	gaim_button(_("Individual log file for each buddy's signons"), &logging_options,
		    OPT_LOG_INDIVIDUAL, mbox);

	frame = gtk_frame_new(_("Browser"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	opt = browser_radio(_("KFM"), BROWSER_KFM, vbox, NULL);
	opt = browser_radio(_("Opera"), BROWSER_OPERA, vbox, opt);
	opt = browser_radio(_("Netscape"), BROWSER_NETSCAPE, vbox, opt);

	new_window =
	    gaim_button(_("Pop up new window by default"), &misc_options, OPT_MISC_BROWSER_POPUP, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

#ifdef USE_GNOME
	opt = browser_radio(_("GNOME URL Handler"), BROWSER_GNOME, vbox, opt);
#endif /* USE_GNOME */
	opt = browser_radio(_("Galeon"), BROWSER_GALEON, vbox, opt);
	opt = browser_radio(_("Manual"), BROWSER_MANUAL, vbox, opt);

	browser_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), browser_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(browser_entry), web_command);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "focus_out_event",
			   GTK_SIGNAL_FUNC(manualentry_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "destroy", GTK_SIGNAL_FUNC(brentdes), NULL);
	gtk_widget_show(browser_entry);

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

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYTYPE 2
#define PROXYUSER 3
#define PROXYPASS 4

static void proxy_print_option(GtkEntry *entry, int entrynum)
{
	if (entrynum == PROXYHOST)
		g_snprintf(proxyhost, sizeof(proxyhost), "%s", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPORT)
		proxyport = atoi(gtk_entry_get_text(entry));
	else if (entrynum == PROXYUSER)
		g_snprintf(proxyuser, sizeof(proxyuser), "%s", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPASS)
		g_snprintf(proxypass, sizeof(proxypass), "%s", gtk_entry_get_text(entry));
	save_prefs();
}

static void proxy_print_optionrad(GtkRadioButton * entry, int entrynum)
{
	if (entrynum == PROXY_NONE)
		gtk_widget_set_sensitive(prefs_proxy_frame, FALSE);
	else
		gtk_widget_set_sensitive(prefs_proxy_frame, TRUE);

	proxytype = entrynum;
	save_prefs();
}

static void proxy_page()
{
	GtkWidget *parent;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *first, *opt;
	GtkWidget *frame;
	GtkWidget *table;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Proxy Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), vbox);
	gtk_widget_show(vbox);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	label = gtk_label_new(_("Not all protocols can use these proxy options. Please see the "
				"README file for details."));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	frame = gtk_frame_new(_("Proxy Type"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(frame), table);

	frame = gtk_frame_new(_("Proxy Server"));
	prefs_proxy_frame = frame;

	first = gtk_radio_button_new_with_label(NULL, _("No Proxy"));
	gtk_table_attach(GTK_TABLE(table), first, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	gtk_signal_connect(GTK_OBJECT(first), "clicked",
			   GTK_SIGNAL_FUNC(proxy_print_optionrad), (void *)PROXY_NONE);
	gtk_widget_show(first);

	if (proxytype == PROXY_NONE)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(first), TRUE);

	opt =
	    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)),
					    _("SOCKS 4"));
	gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked",
			   GTK_SIGNAL_FUNC(proxy_print_optionrad), (void *)PROXY_SOCKS4);
	gtk_widget_show(opt);
	if (proxytype == PROXY_SOCKS4)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	opt =
	    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)),
					    _("SOCKS 5"));
	gtk_table_attach(GTK_TABLE(table), opt, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked",
			   GTK_SIGNAL_FUNC(proxy_print_optionrad), (void *)PROXY_SOCKS5);
	gtk_widget_show(opt);
	if (proxytype == PROXY_SOCKS5)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	opt =
	    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)), _("HTTP"));
	gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked",
			   GTK_SIGNAL_FUNC(proxy_print_optionrad), (void *)PROXY_HTTP);
	gtk_widget_show(opt);
	if (proxytype == PROXY_HTTP)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);


	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 5);

	if (proxytype == PROXY_NONE)
		gtk_widget_set_sensitive(GTK_WIDGET(frame), FALSE);

	table = gtk_table_new(2, 4, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(frame), table);


	label = gtk_label_new(_("Host"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYHOST);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyhost);
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Port"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPORT);

	if (proxyport) {
		char buf[128];
		g_snprintf(buf, sizeof(buf), "%d", proxyport);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
	}
	gtk_widget_show(entry);

	label = gtk_label_new(_("User"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYUSER);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyuser);
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Password"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPASS);
	gtk_entry_set_text(GTK_ENTRY(entry), proxypass);
	gtk_widget_show(entry);

	gtk_widget_show(prefdialog);
}

static void buddy_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *sep;
	GtkWidget *opt;
	GtkWidget *button;
	GtkWidget *button2;

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

	frame = gtk_frame_new(_("Buddy List Window"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	/* "Place blist tabs  */
	vbox = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 5);
	gtk_widget_show(vbox);

	label = gtk_label_new(_("Tab Placement:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);	
	
	opt = blist_tab_radio(_("Top"), ~(OPT_BLIST_BOTTOM_TAB), vbox, NULL);
	opt = blist_tab_radio(_("Bottom"), OPT_BLIST_BOTTOM_TAB, vbox, opt);

	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	/* End of blist tab options */

	vbox = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	button = gaim_button(_("Hide IM/Info/Chat buttons"), &blist_options, OPT_BLIST_NO_BUTTONS, vbox);
#ifdef USE_APPLET
	gaim_button(_("Automatically show buddy list on sign on"), &blist_options,
		    OPT_BLIST_APP_BUDDY_SHOW, vbox);
#endif
	gaim_button(_("Save Window Size/Position"), &blist_options, OPT_BLIST_SAVED_WINDOWS, vbox);

	button2 =
	    gaim_button(_("Show pictures on buttons"), &blist_options, OPT_BLIST_SHOW_BUTTON_XPM, vbox);
	if (blist_options & OPT_BLIST_NO_BUTTONS)
		gtk_widget_set_sensitive(button2, FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), button2);

#ifdef USE_APPLET
	gaim_button(_("Display Buddy List near applet"), &blist_options, OPT_BLIST_NEAR_APPLET, vbox);
#endif
	
	frame = gtk_frame_new(_("Group Displays"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Hide groups with no online buddies"), &blist_options, OPT_BLIST_NO_MT_GRP, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Show numbers in groups"), &blist_options, OPT_BLIST_SHOW_GRPNUM, vbox);

	frame = gtk_frame_new(_("Buddy Displays"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Show buddy type icons"), &blist_options, OPT_BLIST_SHOW_PIXMAPS, vbox);
	gaim_button(_("Show warning levels"), &blist_options, OPT_BLIST_SHOW_WARN, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Show idle times"), &blist_options, OPT_BLIST_SHOW_IDLETIME, vbox);
	gaim_button(_("Grey idle buddies"), &blist_options, OPT_BLIST_GREY_IDLERS, vbox);

	gtk_widget_show(prefdialog);
}

static void convo_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Conversation Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	frame = gtk_frame_new(_("Keyboard Options"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Enter sends message"), &convo_options, OPT_CONVO_ENTER_SENDS, vbox);
	gaim_button(_("Control-Enter sends message"), &convo_options, OPT_CONVO_CTL_ENTER, vbox);
	gaim_button(_("Escape closes window"), &convo_options, OPT_CONVO_ESC_CAN_CLOSE, vbox);

	vbox = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Control-{B/I/U/S} inserts HTML tags"), &convo_options, OPT_CONVO_CTL_CHARS, vbox);
	gaim_button(_("Control-(number) inserts smileys"), &convo_options, OPT_CONVO_CTL_SMILEYS, vbox);
	gaim_button(_("F2 toggles timestamp display"), &convo_options, OPT_CONVO_F2_TOGGLES, vbox);

	frame = gtk_frame_new(_("Display and General Options"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Show graphical smileys"), &convo_options, OPT_CONVO_SHOW_SMILEY, vbox);
	gaim_button(_("Show timestamp on messages"), &convo_options, OPT_CONVO_SHOW_TIME, vbox);
	gaim_button(_("Show URLs as links"), &convo_options, OPT_CONVO_SEND_LINKS, vbox);
	gaim_button(_("Highlight misspelled words"), &convo_options, OPT_CONVO_CHECK_SPELLING, vbox);
	gaim_button(_("Sending messages removes away status"), &away_options, OPT_AWAY_BACK_ON_IM, vbox);
	gaim_button(_("Queue new messages when away"), &away_options, OPT_AWAY_QUEUE, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Ignore colors"), &convo_options, OPT_CONVO_IGNORE_COLOUR, vbox);
	gaim_button(_("Ignore font faces"), &convo_options, OPT_CONVO_IGNORE_FONTS, vbox);
	gaim_button(_("Ignore font sizes"), &convo_options, OPT_CONVO_IGNORE_SIZES, vbox);
	gaim_button(_("Ignore TiK Automated Messages"), &away_options, OPT_AWAY_TIK_HACK, vbox);
	gaim_button(_("Ignore new conversations when away"), &away_options, OPT_AWAY_DISCARD, vbox);

	gtk_widget_show(prefdialog);
}

static void set_buttons_opt(GtkWidget *w, int data)
{
	int mask;
	if (data & 0x1) {	/* set the first bit if we're affecting chat buttons */
		mask = (OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM);
		chat_options &= ~(mask);
		chat_options |= (data & mask);
		update_chat_button_pix();
	} else {
		mask = (OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM);
		im_options &= ~(mask);
		im_options |= (data & mask);
		update_im_button_pix();
	}

	save_prefs();
}

/* i like everclear */
static GtkWidget *am_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_buttons_opt), (void *)which);
	gtk_widget_show(opt);
	if (which & 1) {
		if (which == (OPT_CHAT_BUTTON_TEXT | 1)) {
			if ((chat_options & OPT_CHAT_BUTTON_TEXT) &&
			    !(chat_options & OPT_CHAT_BUTTON_XPM))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		} else if (which == (OPT_CHAT_BUTTON_XPM | 1)) {
			if (!(chat_options & OPT_CHAT_BUTTON_TEXT) &&
			    (chat_options & OPT_CHAT_BUTTON_XPM))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		} else {
			if (((chat_options & OPT_CHAT_BUTTON_TEXT) &&
			     (chat_options & OPT_CHAT_BUTTON_XPM)) ||
			    (!(chat_options & OPT_CHAT_BUTTON_TEXT) &&
			     !(chat_options & OPT_CHAT_BUTTON_XPM)))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		}
	} else {
		if (which == OPT_IM_BUTTON_TEXT) {
			if ((im_options & OPT_IM_BUTTON_TEXT) && !(im_options & OPT_IM_BUTTON_XPM))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		} else if (which == OPT_IM_BUTTON_XPM) {
			if (!(im_options & OPT_IM_BUTTON_TEXT) && (im_options & OPT_IM_BUTTON_XPM))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		} else {
			if (((im_options & OPT_IM_BUTTON_TEXT) &&
			     (im_options & OPT_IM_BUTTON_XPM)) ||
			    (!(im_options & OPT_IM_BUTTON_TEXT) && !(im_options & OPT_IM_BUTTON_XPM)))
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		}
	}

	return opt;
}

static void set_tab_opt(GtkWidget *w, int data)
{
	int mask;
	if (convo_options & OPT_CONVO_COMBINE) {
		/* through an amazing coincidence (this wasn't planned), we're able to do this,
		 * since the two sets of options end up having the same value. isn't that great. */
		mask = (OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB);
		chat_options &= ~(mask);
		chat_options |= (data & mask);

		mask = (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB);
		im_options &= ~(mask);
		im_options |= (data & mask);

		update_im_tabs();
	} else {
		if (data & 0x1) {	/* set the first bit if we're affecting chat buttons */
			mask = (OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB);
			chat_options &= ~(mask);
			chat_options |= (data & mask);
			update_chat_tabs();
		} else {
			mask = (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB);
			im_options &= ~(mask);
			im_options |= (data & mask);
			update_im_tabs();
		}
	}

	save_prefs();
}

static GtkWidget *tab_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_tab_opt), (void *)which);
	gtk_widget_show(opt);
	if (which & 1) {
		if ((chat_options & (OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB)) == (which ^ 1))
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		if (!(chat_options & OPT_CHAT_ONE_WINDOW))
			gtk_widget_set_sensitive(opt, FALSE);
	} else {
		if ((im_options & (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB)) == which)
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		if (!(im_options & OPT_IM_ONE_WINDOW))
			gtk_widget_set_sensitive(opt, FALSE);
	}

	return opt;
}

static void update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	int *value = gtk_object_get_user_data(GTK_OBJECT(spin));
	*value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
}

static void gaim_labeled_spin_button(GtkWidget *box, const gchar *title, int *val, int min, int max)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *spin;
	GtkObject *adjust;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(title);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	adjust = gtk_adjustment_new(*val, min, max, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_object_set_user_data(GTK_OBJECT(spin), val);
	gtk_widget_set_usize(spin, 50, -1);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(adjust), "value-changed",
			   GTK_SIGNAL_FUNC(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);
}

static gboolean current_is_im = FALSE;

static void not_im()
{
	current_is_im = FALSE;
}

static void im_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *opt;
	GtkWidget *sep;
	GtkWidget *button;
	GtkWidget *hbox2;
	GtkWidget *vbox3;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	current_is_im = TRUE;

	prefdialog = gtk_frame_new(_("IM Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);
	gtk_signal_connect(GTK_OBJECT(prefdialog), "destroy", GTK_SIGNAL_FUNC(not_im), NULL);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	frame = gtk_frame_new(_("IM Window"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 5);
	gtk_widget_show(vbox2);

	label = gtk_label_new(_("Show buttons as: "));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	opt = am_radio(_("Pictures And Text"), OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM, vbox2, NULL);
	opt = am_radio(_("Pictures"), OPT_IM_BUTTON_XPM, vbox2, opt);
	opt = am_radio(_("Text"), OPT_IM_BUTTON_TEXT, vbox2, opt);

	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	button =
	    gaim_button(_("Show all conversations in one tabbed window"), &im_options, OPT_IM_ONE_WINDOW,
			vbox2);
	opt = gaim_button(_("Show chats in the same tabbed window"), &convo_options, OPT_CONVO_COMBINE, vbox2);
	if (chat_options & OPT_CHAT_ONE_WINDOW) {
		if (!(im_options & OPT_IM_ONE_WINDOW))
			gtk_widget_set_sensitive(opt, FALSE);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	} else
		gtk_widget_set_sensitive(opt, FALSE);
	gaim_button(_("Raise windows on events"), &im_options, OPT_IM_POPUP, vbox2);
	gaim_button(_("Show logins in window"), &im_options, OPT_IM_LOGON, vbox2);
	gaim_button(_("Show aliases in tabs/titles"), &im_options, OPT_IM_ALIAS_TAB, vbox2);
	gaim_button(_("Hide window on send"), &im_options, OPT_IM_POPDOWN, vbox2);

	frame = gtk_frame_new(_("Window Sizes"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	gaim_labeled_spin_button(vbox, _("New window width:"), &conv_size.width, 25, 9999);
	gaim_labeled_spin_button(vbox, _("New window height:"), &conv_size.height, 25, 9999);
	gaim_labeled_spin_button(vbox, _("Entry widget height:"), &conv_size.entry_height, 25, 9999);

	frame = gtk_frame_new(_("Tab Placement"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	hbox2 = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, TRUE, TRUE, 5);
	gtk_widget_show(hbox2);

	vbox3 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 5);
	gtk_widget_show(vbox3);

	opt = tab_radio(_("Top"), 0, vbox3, NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	opt = tab_radio(_("Bottom"), OPT_IM_BR_TAB, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);

	vbox3 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 5);
	gtk_widget_show(vbox3);

	opt = tab_radio(_("Left"), OPT_IM_SIDE_TAB, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	opt = tab_radio(_("Right"), OPT_IM_SIDE_TAB | OPT_IM_BR_TAB, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);

#if USE_PIXBUF
	frame = gtk_frame_new(_("Buddy Icons"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	gaim_button(_("Hide Buddy Icons"), &im_options, OPT_IM_HIDE_ICONS, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);
	
	gaim_button(_("Disable Buddy Icon Animation"), &im_options, OPT_IM_NO_ANIMATION, vbox);
#endif

	gtk_widget_show(prefdialog);
}

static void chat_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *opt;
	GtkWidget *sep;
	GtkWidget *button;
	GtkWidget *hbox2;
	GtkWidget *vbox3;
	GtkWidget *tab;
	GtkWidget *old;

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

	frame = gtk_frame_new(_("Group Chat Window"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 5);
	gtk_widget_show(vbox2);

	label = gtk_label_new(_("Show buttons as: "));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	opt =
	    am_radio(_("Pictures And Text"), OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM | 1, vbox2,
		     NULL);
	opt = am_radio(_("Pictures"), OPT_CHAT_BUTTON_XPM | 1, vbox2, opt);
	opt = am_radio(_("Text"), OPT_CHAT_BUTTON_TEXT | 1, vbox2, opt);

	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	vbox2 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	button =
	    gaim_button(_("Show all chats in one tabbed window"), &chat_options, OPT_CHAT_ONE_WINDOW,
			vbox2);
	opt = gaim_button(_("Show conversations in the same tabbed window"), &convo_options, OPT_CONVO_COMBINE, vbox2);
	if (im_options & OPT_IM_ONE_WINDOW) {
		if (!(chat_options & OPT_CHAT_ONE_WINDOW))
			gtk_widget_set_sensitive(opt, FALSE);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	} else
		gtk_widget_set_sensitive(opt, FALSE);
	gaim_button(_("Raise windows on events"), &chat_options, OPT_CHAT_POPUP, vbox2);
	gaim_button(_("Show people joining/leaving in window"), &chat_options, OPT_CHAT_LOGON, vbox2);

	frame = gtk_frame_new(_("Window Sizes"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	gaim_labeled_spin_button(vbox, _("New window width:"), &buddy_chat_size.width, 25, 9999);
	gaim_labeled_spin_button(vbox, _("New window height:"), &buddy_chat_size.height, 25, 9999);
	gaim_labeled_spin_button(vbox, _("Entry widget height:"), &buddy_chat_size.entry_height, 25, 9999);

	frame = gtk_frame_new(_("Tab Placement"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	hbox2 = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, TRUE, TRUE, 5);
	gtk_widget_show(hbox2);

	vbox3 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 5);
	gtk_widget_show(vbox3);

	opt = tab_radio(_("Top"), 1, vbox3, NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	opt = tab_radio(_("Bottom"), OPT_CHAT_BR_TAB | 1, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);

	vbox3 = gtk_vbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 5);
	gtk_widget_show(vbox3);

	opt = tab_radio(_("Left"), OPT_CHAT_SIDE_TAB | 1, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);
	opt = tab_radio(_("Right"), OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB | 1, vbox3, opt);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), opt);

	frame = gtk_frame_new(_("Tab Completion"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	tab = gaim_button(_("Tab-Complete Nicks"), &chat_options, OPT_CHAT_TAB_COMPLETE, vbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	old = gaim_button(_("Old-Style Tab Completion"), &chat_options, OPT_CHAT_OLD_STYLE_TAB, vbox);
	if (!(chat_options & OPT_CHAT_TAB_COMPLETE))
		gtk_widget_set_sensitive(GTK_WIDGET(old), FALSE);
	gtk_signal_connect(GTK_OBJECT(tab), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), old);

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

static void font_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *select;
	GtkWidget *spin;
	GtkObject *adjust;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *table;

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

	frame = gtk_frame_new("Font Style");

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	button = gtk_check_button_new_with_label(_("Bold Text"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (font_options & OPT_FONT_BOLD));
	gtk_table_attach(GTK_TABLE(table), button, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(set_font_option), (int *)OPT_FONT_BOLD);

	button = gtk_check_button_new_with_label(_("Italic Text"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (font_options & OPT_FONT_ITALIC));
	gtk_table_attach(GTK_TABLE(table), button, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(set_font_option), (int *)OPT_FONT_ITALIC);

	button = gtk_check_button_new_with_label(_("Underline Text"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (font_options & OPT_FONT_UNDERLINE));
	gtk_table_attach(GTK_TABLE(table), button, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(set_font_option), (int *)OPT_FONT_UNDERLINE);

	button = gtk_check_button_new_with_label(_("Strike through Text"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (font_options & OPT_FONT_STRIKE));
	gtk_table_attach(GTK_TABLE(table), button, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(set_font_option), (int *)OPT_FONT_STRIKE);


	gtk_widget_show_all(table);
	gtk_widget_show(frame);

	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);

	/* ----------- */

	frame = gtk_frame_new("Font Color");
	fbox = gtk_vbox_new(FALSE, 5);

	gtk_container_add(GTK_CONTAINER(frame), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);

	gtk_widget_show(fbox);
	gtk_widget_show(frame);

	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 5);
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
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 5);
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

	/* ----------- */

	frame = gtk_frame_new("Font Face");
	fbox = gtk_vbox_new(FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 5);
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
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = gaim_button(_("Font Size for Text"), &font_options, OPT_FONT_SIZE, hbox);

	adjust = gtk_adjustment_new(fontsize, 1, 7, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(spin, 50, -1);
	gtk_object_set_user_data(GTK_OBJECT(spin), &fontsize);
	if (!(font_options & OPT_FONT_SIZE))
		gtk_widget_set_sensitive(GTK_WIDGET(spin), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), spin);
	+gtk_signal_connect(GTK_OBJECT(adjust), "value-changed", GTK_SIGNAL_FUNC(update_spin_value),
			    GTK_WIDGET(spin));
	gtk_widget_show(spin);

	gtk_container_add(GTK_CONTAINER(frame), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(fbox);
	gtk_widget_show(frame);

	gtk_widget_show(prefdialog);
}

static GtkWidget *sndent[NUM_SOUNDS];
static GtkWidget *sndcmd = NULL;
static char *last_sound_dir = NULL;

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
	const char *file;

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

	if (last_sound_dir)
		g_free(last_sound_dir);
	last_sound_dir = g_dirname(sound_file[snd]);
}

static void test_sound(GtkWidget *button, int snd)
{
	guint32 tmp_sound = sound_options;
	if (!(sound_options & OPT_SOUND_WHEN_AWAY))
		sound_options ^= OPT_SOUND_WHEN_AWAY;
	if (!(sound_options & sounds[snd].opt))
		sound_options ^= sounds[snd].opt;
	play_sound(snd);
	sound_options = tmp_sound;
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

		g_snprintf(buf, BUF_LEN - 1, "%s/", last_sound_dir ? last_sound_dir : g_get_home_dir());

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

static void sound_entry(GtkWidget *box, int snd)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *button;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gaim_button(sounds[snd].label, &sound_options, sounds[snd].opt, hbox);

	button = gtk_button_new_with_label(_("Test"));
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(test_sound), (void *)snd);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(reset_sound), (void *)snd);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Choose..."));
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(sel_sound), (void *)snd);
	gtk_widget_show(button);

	entry = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);

	if (sound_file[snd])
		gtk_entry_set_text(GTK_ENTRY(entry), sound_file[snd]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "(default)");

	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	sndent[snd] = entry;
	gtk_widget_show(entry);
}

static gint sound_cmd_yeah(GtkEntry *entry, GdkEvent *event, gpointer d)
{
	g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", gtk_entry_get_text(GTK_ENTRY(sndcmd)));
	save_prefs();
	return TRUE;
}

static void set_sound_driver(GtkWidget *w, int option)
{
	if (option == OPT_SOUND_CMD)
		gtk_widget_set_sensitive(sndcmd, TRUE);
	else
		gtk_widget_set_sensitive(sndcmd, FALSE);

	sound_options &= ~(OPT_SOUND_NORMAL | OPT_SOUND_BEEP |
			   OPT_SOUND_NAS | OPT_SOUND_ARTSC |
			   OPT_SOUND_ESD | OPT_SOUND_CMD);
	sound_options |= option;
	save_prefs();
}

static void sound_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *sep;
	GtkWidget *omenu;
	GtkWidget *menu;
	GtkWidget *opt;
	int i=1, driver=0, j;

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

	frame = gtk_frame_new(_("Options"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	gaim_button(_("No sounds when you log in"), &sound_options, OPT_SOUND_SILENT_SIGNON, vbox2);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, vbox2);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Sound method"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	omenu = gtk_option_menu_new();
	menu = gtk_menu_new();

	opt = gtk_menu_item_new_with_label("Console Beep");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_BEEP);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_BEEP) && !driver) driver = i;
	i++;

#ifdef ESD_SOUND
	opt = gtk_menu_item_new_with_label("ESD");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_ESD);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_ESD) && !driver) driver = i;
	i++;
#endif
#ifdef ARTSC_SOUND
	opt = gtk_menu_item_new_with_label("ArtsC");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_ARTSC);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_ARTSC) && !driver) driver = i;
	i++;
#endif
#ifdef NAS_SOUND
	opt = gtk_menu_item_new_with_label("NAS");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_NAS);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_NAS) && !driver) driver = i;
	i++;
#endif

	opt = gtk_menu_item_new_with_label("Internal");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_NORMAL);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_NORMAL) && !driver) driver = i;
	i++;

	opt = gtk_menu_item_new_with_label("Command");
	gtk_signal_connect(GTK_OBJECT(opt), "activate",
			   GTK_SIGNAL_FUNC(set_sound_driver), 
			   (gpointer)OPT_SOUND_CMD);
	gtk_widget_show(opt);
	gtk_menu_append(GTK_MENU(menu), opt);
	if ((sound_options & OPT_SOUND_CMD) && !driver) driver = i;
	i++;

	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), driver - 1);
	gtk_box_pack_start(GTK_BOX(hbox), omenu, FALSE, FALSE, 5);
	gtk_widget_show_all(omenu);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Sound command\n(%s for filename)"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sndcmd = gtk_entry_new();
	gtk_entry_set_editable(GTK_ENTRY(sndcmd), TRUE);
	gtk_entry_set_text(GTK_ENTRY(sndcmd), sound_cmd);
	gtk_box_pack_end(GTK_BOX(hbox), sndcmd, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(sndcmd), "focus_out_event", GTK_SIGNAL_FUNC(sound_cmd_yeah), NULL);
	gtk_widget_set_sensitive(sndcmd, (OPT_SOUND_CMD & sound_options));
	gtk_widget_show(sndcmd);

	frame = gtk_frame_new(_("Sound played when:"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	for (j=0; j < NUM_SOUNDS; j++) {
		/* no entry for the buddy pounce sound, it's configurable per-pounce */
		if (j == SND_POUNCE_DEFAULT)
			continue;

		/* seperators before SND_RECEIVE and SND_CHAT_JOIN */
		if ((j == SND_RECEIVE) || (j == SND_CHAT_JOIN)) {
			sep = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 5);
			gtk_widget_show(sep);
		}

		sound_entry(vbox, j);
	}

	gtk_widget_show(prefdialog);
}

static struct away_message *cur_message;
static GtkWidget *away_text;
static GtkWidget *make_away_button = NULL;

void away_list_clicked(GtkWidget *widget, struct away_message *a)
{
	gchar buffer[BUF_LONG];
	char *tmp;

	cur_message = a;

	/* Clear the Box */
	gtk_imhtml_clear(GTK_IMHTML(away_text));

	/* Fill the text box with new message */
	strcpy(buffer, a->message);
	tmp = stylize(buffer, BUF_LONG);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), tmp, -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), "<BR>", -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	g_free(tmp);
}

void remove_away_message(GtkWidget *widget, void *dummy)
{
	GList *i;
	struct away_message *a;

	i = GTK_LIST(prefs_away_list)->selection;

	if (!i)
		return;
	if (!i->next) {
		gtk_imhtml_clear(GTK_IMHTML(away_text));
	}
	a = gtk_object_get_user_data(GTK_OBJECT(i->data));
	rem_away_mess(NULL, a);
}

static void paldest(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(prefs_away_list);
	prefs_away_list = NULL;
	prefs_away_menu = NULL;
	make_away_button = NULL;
}

static void do_away_mess(GtkWidget *m, gpointer n)
{
	GList *i = GTK_LIST(prefs_away_list)->selection;
	if (i)
		do_away_message(NULL, gtk_object_get_user_data(GTK_OBJECT(i->data)));
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
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *button;
	GtkWidget *button2;
	GtkWidget *top;
	GtkWidget *bot;
	GtkWidget *sw;
	GtkWidget *sw2;
	GtkWidget *list_item;
	GtkWidget *sep;
	GtkObject *adjust;
	GtkWidget *spin;
	GSList *awy = away_messages;
	struct away_message *a;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Away Messages"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	frame = gtk_frame_new(_("Options"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	gaim_button(_("Ignore new conversations when away"), &away_options, OPT_AWAY_DISCARD, vbox2);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, vbox2);
	gaim_button(_("Sending messages removes away status"), &away_options, OPT_AWAY_BACK_ON_IM,
		    vbox2);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);
	gtk_widget_show(vbox2);

	button = gaim_button(_("Don't send auto-response"), &away_options, OPT_AWAY_NO_AUTO_RESP, vbox2);
	button2 = gaim_button(_("Only send auto-response when idle"), &away_options, OPT_AWAY_IDLE_RESP,
			      vbox2);
	if (away_options & OPT_AWAY_NO_AUTO_RESP)
		gtk_widget_set_sensitive(button2, FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), button2);
	gaim_button(_("Queue new messages when away"), &away_options, OPT_AWAY_QUEUE, vbox2);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gaim_labeled_spin_button(hbox, _("Time between sending auto-responses (in seconds):"),
			&away_resend, 1, 24 * 60 * 60);

	if (away_options & OPT_AWAY_NO_AUTO_RESP)
		gtk_widget_set_sensitive(hbox, FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), hbox);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gaim_button(_("Auto Away after"), &away_options, OPT_AWAY_AUTO, hbox);

	adjust = gtk_adjustment_new(auto_away, 1, 1440, 1, 10, 10);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(spin, 50, -1);
	gtk_object_set_user_data(GTK_OBJECT(spin), &auto_away);
	if (!(away_options & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(spin), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), spin);
	gtk_signal_connect(GTK_OBJECT(adjust), "value-changed", GTK_SIGNAL_FUNC(update_spin_value),
			   GTK_WIDGET(spin));
	gtk_widget_show(spin);

	label = gtk_label_new(_("minutes using"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	prefs_away_menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), prefs_away_menu, FALSE, FALSE, 0);
	default_away_menu_init(prefs_away_menu);
	if (!(away_options & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(prefs_away_menu), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),
			   prefs_away_menu);
	gtk_widget_show(prefs_away_menu);

	frame = gtk_frame_new(_("Messages"));
	gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Title"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	label = gtk_label_new(_("Message"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	top = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), top, TRUE, TRUE, 0);
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
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(top), sw2, TRUE, TRUE, 0);
	gtk_widget_show(sw2);

	away_text = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw2), away_text);
	GTK_LAYOUT(away_text)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(away_text)->vadjustment->step_increment = 10.0;
	gaim_setup_imhtml(away_text);
	gtk_widget_show(away_text);

	bot = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), bot, FALSE, FALSE, 5);
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
		char buffer[BUF_LONG];
		char *tmp;
		a = (struct away_message *)awy->data;
		g_snprintf(buffer, sizeof(buffer), "%s", a->message);
		tmp = stylize(buffer, BUF_LONG);
		gtk_imhtml_append_text(GTK_IMHTML(away_text), tmp, -1, GTK_IMHTML_NO_TITLE |
				       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
		gtk_imhtml_append_text(GTK_IMHTML(away_text), "<BR>", -1, GTK_IMHTML_NO_TITLE |
				       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
		g_free(tmp);
	}

	while (awy) {
		a = (struct away_message *)awy->data;
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(prefs_away_list), list_item);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_list_clicked),
				   a);
		gtk_object_set_user_data(GTK_OBJECT(list_item), a);
		gtk_widget_show(list_item);

		hbox = gtk_hbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(list_item), hbox);
		gtk_widget_show(hbox);

		label = gtk_label_new(a->name);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		awy = awy->next;
	}
	if (away_messages != NULL)
		gtk_list_select_item(GTK_LIST(prefs_away_list), 0);

	gtk_widget_show(prefdialog);
}

static GtkWidget *deny_type = NULL;
static GtkWidget *deny_conn_hbox = NULL;
static GtkWidget *deny_opt_menu = NULL;
static struct gaim_connection *current_deny_gc = NULL;
static gboolean current_is_deny = FALSE;
static GtkWidget *allow_list = NULL;
static GtkWidget *block_list = NULL;

static void set_deny_mode(GtkWidget *w, int data)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		return;
	debug_printf("setting deny mode %d\n", data);
	current_deny_gc->permdeny = data;
	serv_set_permit_deny(current_deny_gc);
	do_export(current_deny_gc);
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
	gtk_signal_connect(GTK_OBJECT(opt), "toggled", GTK_SIGNAL_FUNC(set_deny_mode), (void *)which);
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
	deny_type = NULL;
	deny_opt_menu = NULL;
	current_is_deny = FALSE;
	allow_list = NULL;
	block_list = NULL;
}

static void set_deny_type()
{
	GSList *bg = gtk_radio_button_group(GTK_RADIO_BUTTON(deny_type));

	switch (current_deny_gc->permdeny) {
	case 4:
		break;
	case 3:
		bg = bg->next->next;
		break;
	case 2:
		bg = bg->next;
		break;
	case 1:
		bg = bg->next->next->next;
		break;
	}

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(bg->data), TRUE);
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
	set_deny_type();
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
		g_snprintf(buf, sizeof buf, "%s (%s)", gc->username, gc->prpl->name());
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

	do_export(current_deny_gc);
}

static void deny_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *bbox;
	GtkWidget *button;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	current_deny_gc = connections->data;	/* this is safe because this screen will only be
						   available when there are connections */
	current_is_deny = TRUE;

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

	deny_type = deny_opt(_("Allow all users to contact me"), 1, vbox, NULL);
	deny_type = deny_opt(_("Allow only the users below"), 3, vbox, deny_type);

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

	deny_type = deny_opt(_("Deny all users"), 2, vbox, deny_type);
	deny_type = deny_opt(_("Block the users below"), 4, vbox, deny_type);

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

void update_connection_dependent_prefs()
{				/* what a crappy name */
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

static void try_me(GtkCTree *ctree, GtkCTreeNode *node)
{
	/* this is a hack */
	void (*func)();
	func = gtk_ctree_node_get_row_data(ctree, node);
	func();
}

void show_prefs()
{
	GtkWidget *vbox;
	GtkWidget *hpaned;
	/* GtkWidget *scroll; */
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
	gtk_widget_set_usize(prefs, 725, 620);
	gtk_signal_connect(GTK_OBJECT(prefs), "destroy", GTK_SIGNAL_FUNC(delete_prefs), NULL);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(prefs), vbox);
	gtk_widget_show(vbox);

	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show(hpaned);

	/*
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_paned_pack1(GTK_PANED(hpaned), scroll, FALSE, FALSE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_widget_set_usize(scroll, 125, -1);
	gtk_widget_show(scroll);
	*/

	preftree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style(GTK_CTREE(preftree), GTK_CTREE_LINES_SOLID);
	gtk_ctree_set_expander_style(GTK_CTREE(preftree), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_clist_set_reorderable(GTK_CLIST(preftree), FALSE);
	/* gtk_container_add(GTK_CONTAINER(scroll), preftree); */
	gtk_paned_pack1(GTK_PANED(hpaned), preftree, FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(preftree), "tree_select_row", GTK_SIGNAL_FUNC(try_me), NULL);
	gtk_widget_set_usize(preftree, 125, -1);
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

	/* general_page(); */

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
	if (misc_options & OPT_MISC_DEBUG) {
		misc_options ^= OPT_MISC_DEBUG;
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

	GAIM_DIALOG(dw->window);
	gtk_window_set_title(GTK_WINDOW(dw->window), _("Gaim debug output window"));
	gtk_window_set_wmclass(GTK_WINDOW(dw->window), "debug_out", "Gaim");
	gtk_signal_connect(GTK_OBJECT(dw->window), "delete_event", GTK_SIGNAL_FUNC(debug_delete), NULL);
	gtk_widget_realize(dw->window);
	aol_icon(dw->window->window);

	box = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dw->window), box);
	gtk_widget_show(box);

	dw->entry = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(dw->entry), TRUE);
	gtk_text_set_editable(GTK_TEXT(dw->entry), FALSE);
	gtk_container_add(GTK_CONTAINER(box), dw->entry);
	gtk_widget_set_usize(dw->entry, 500, 200);
	gtk_widget_show(dw->entry);

	scroll = gtk_vscrollbar_new(GTK_TEXT(dw->entry)->vadj);
	gtk_box_pack_start(GTK_BOX(box), scroll, FALSE, FALSE, 0);
	gtk_widget_show(scroll);

	gtk_widget_show(dw->window);
}

void show_debug()
{
	if ((misc_options & OPT_MISC_DEBUG)) {
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

	if (misc_options & OPT_MISC_DEBUG && dw) {
		GtkAdjustment *adj = GTK_TEXT(dw->entry)->vadj;
		gboolean scroll = (adj->value == adj->upper - adj->lower - adj->page_size);

		gtk_text_freeze(GTK_TEXT(dw->entry));
		gtk_text_insert(GTK_TEXT(dw->entry), NULL, NULL, NULL, s, -1);
		gtk_text_thaw(GTK_TEXT(dw->entry));

		if (scroll)
			gtk_adjustment_set_value(adj, adj->upper - adj->lower - adj->page_size);
	}
	if (opt_debug)
		g_print("%s", s);
	g_free(s);
}

static gint handle_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	save_prefs();

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

static void set_misc_option(GtkWidget *w, int option)
{
	misc_options ^= option;

	if (option == OPT_MISC_DEBUG)
		show_debug();

	if (option == OPT_MISC_BUDDY_TICKER)
		BuddyTickerShow();

	save_prefs();
}

static void set_logging_option(GtkWidget *w, int option)
{
	logging_options ^= option;

	if (option == OPT_LOG_ALL)
		update_log_convs();

	save_prefs();
}

static void set_blist_option(GtkWidget *w, int option)
{
	blist_options ^= option;

	save_prefs();

	if (!blist)
		return;

	if (option == OPT_BLIST_NO_BUTTONS)
		build_imchat_box(!(blist_options & OPT_BLIST_NO_BUTTONS));

	if (option == OPT_BLIST_SHOW_GRPNUM)
		update_num_groups();

	if (option == OPT_BLIST_NO_MT_GRP)
		toggle_show_empty_groups();

	if ((option == OPT_BLIST_SHOW_BUTTON_XPM) || (option == OPT_BLIST_NO_BUTTONS))
		update_button_pix();

	if (option == OPT_BLIST_SHOW_PIXMAPS)
		toggle_buddy_pixmaps();

	if ((option == OPT_BLIST_GREY_IDLERS) || (option == OPT_BLIST_SHOW_IDLETIME))
		update_idle_times();

}

static void set_convo_option(GtkWidget *w, int option)
{
	convo_options ^= option;

	if (option == OPT_CONVO_SHOW_SMILEY)
		toggle_smileys();

	if (option == OPT_CONVO_SHOW_TIME)
		toggle_timestamps();

	if (option == OPT_CONVO_CHECK_SPELLING)
		toggle_spellchk();

	if (option == OPT_CONVO_COMBINE) {
		/* (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB) == (OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB) */
		if (current_is_im) {
			int set = im_options & (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB);
			chat_options &= ~(OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB);
			chat_options |= set;
		} else {
			int set = chat_options & (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB);
			im_options &= ~(OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB);
			im_options |= set;
		}
		convo_tabize();
	}

	save_prefs();
}

static void set_im_option(GtkWidget *w, int option)
{
	im_options ^= option;

	if (option == OPT_IM_ONE_WINDOW)
		im_tabize();

	if (option == OPT_IM_HIDE_ICONS)
		set_hide_icons();

	if (option == OPT_IM_ALIAS_TAB)
		set_convo_titles();

	if (option == OPT_IM_NO_ANIMATION)
		set_anim();

	save_prefs();
}

static void set_chat_option(GtkWidget *w, int option)
{
	chat_options ^= option;

	if (option == OPT_CHAT_ONE_WINDOW)
		chat_tabize();

	save_prefs();
}

void set_sound_option(GtkWidget *w, int option)
{
	sound_options ^= option;

	save_prefs();
}

static void set_font_option(GtkWidget *w, int option)
{
	font_options ^= option;

	update_font_buttons();

	save_prefs();
}

static void set_away_option(GtkWidget *w, int option)
{
	away_options ^= option;

	if (option == OPT_AWAY_QUEUE)
		toggle_away_queue();

	save_prefs();
}

GtkWidget *gaim_button(const char *text, guint *options, int option, GtkWidget *page)
{
	GtkWidget *button;
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (*options & option));
	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	if (options == &misc_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_misc_option),
				   (int *)option);
	if (options == &logging_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_logging_option),
				   (int *)option);
	if (options == &blist_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_blist_option),
				   (int *)option);
	if (options == &convo_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_convo_option),
				   (int *)option);
	if (options == &im_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_im_option),
				   (int *)option);
	if (options == &chat_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_chat_option),
				   (int *)option);
	if (options == &font_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_font_option),
				   (int *)option);
	if (options == &sound_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_sound_option),
				   (int *)option);
	if (options == &away_options)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_away_option),
				   (int *)option);

	gtk_widget_show(button);

	return button;
}

static void blist_tab_opt(GtkWidget *widget, int option)
{
	/* Following general method of set_tab_opt()  */
	int mask;
	mask = (OPT_BLIST_BOTTOM_TAB);
	
	blist_options &= ~(mask);
	blist_options |= (mask & option);	
	
	set_blist_tab();
}

static GtkWidget *blist_tab_radio(const char *label, int which, GtkWidget *box,GtkWidget *set) 
{
        GtkWidget *new_opt;

        if(!set)
                new_opt = gtk_radio_button_new_with_label (NULL, label);        
	else
                new_opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)), label); 
        
	gtk_box_pack_start(GTK_BOX(box), new_opt, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(new_opt), "clicked", 
			GTK_SIGNAL_FUNC(blist_tab_opt), (void *)which);
	gtk_widget_show(new_opt);

        if ((blist_options & OPT_BLIST_BOTTOM_TAB) == (which & OPT_BLIST_BOTTOM_TAB))
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(new_opt), TRUE);

	return new_opt;        
}

void prefs_build_general()
{
	GtkCTreeNode *node;
	char *text[1];

	text[0] = _("General");
	general_node = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), general_node, general_page);

	text[0] = _("Proxy");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), general_node, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, proxy_page);

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
	GtkCTreeNode *parent, *node;
	char *text[1];

	text[0] = _("Conversations");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, convo_page);

	text[0] = _("IM Window");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, im_page);

	text[0] = _("Chat");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, chat_page);

	text[0] = _("Font Options");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
				     text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, font_page);
}

void prefs_build_sound()
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("Sounds");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
				       text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, sound_page);
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
