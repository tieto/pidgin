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
#include "gnome_applet_mgr.h"
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
static void prefs_build_general(GtkWidget *);
static void prefs_build_connect(GtkWidget *);
static void prefs_build_buddy(GtkWidget *);
static void prefs_build_convo(GtkWidget *);
static void prefs_build_sound(GtkWidget *);
static void prefs_build_away(GtkWidget *);
static void prefs_build_browser(GtkWidget *);
static gint handle_delete(GtkWidget *, GdkEvent *, void *);

static GtkWidget *prefdialog = NULL;
static GtkWidget *debugbutton = NULL;
static GtkWidget *prefrem = NULL;
GtkWidget *prefs_away_list = NULL;

static void destdeb(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(debugbutton);
	debugbutton = NULL;
}

static void remdes(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(prefrem);
	prefrem = NULL;
}

static void general_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *idle;
	
	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("General Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	prefrem = gaim_button(_("Remember password"), &general_options, OPT_GEN_REMEMBER_PASS, box);
	gtk_signal_connect(GTK_OBJECT(prefrem), "destroy", GTK_SIGNAL_FUNC(remdes), 0);
	gaim_button(_("Auto-login"), &general_options, OPT_GEN_AUTO_LOGIN, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Use borderless buttons (requires restart for some buttons)"), &display_options, OPT_DISP_COOL_LOOK, box);
	gaim_button(_("Show Buddy Ticker after restart"), &display_options, OPT_DISP_SHOW_BUDDYTICKER, box);
	if (!dw && (general_options & OPT_GEN_DEBUG))
		general_options = general_options ^ OPT_GEN_DEBUG;
	debugbutton = gaim_button(_("Show Debug Window"), &general_options, OPT_GEN_DEBUG, box);
	gtk_signal_connect_object(GTK_OBJECT(debugbutton), "clicked", GTK_SIGNAL_FUNC(show_debug), 0);
	gtk_signal_connect(GTK_OBJECT(debugbutton), "destroy", GTK_SIGNAL_FUNC(destdeb), 0);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	idle = gaim_button(_("Report Idle Times"), &report_idle, 1, box);
	gtk_signal_connect(GTK_OBJECT(idle), "clicked", set_option, &report_idle);

	gtk_widget_show(prefdialog);
}

static GtkWidget *aim_host_entry;
static GtkWidget *aim_port_entry;
static GtkWidget *login_host_entry;
static GtkWidget *login_port_entry;
static GtkWidget *proxy_host_entry;
static GtkWidget *proxy_port_entry;

static int connection_key_pressed(GtkWidget *w, GdkEvent *event, void *dummy)
{
	g_snprintf(aim_host, sizeof(aim_host), "%s", gtk_entry_get_text(GTK_ENTRY(aim_host_entry)));
	sscanf(gtk_entry_get_text(GTK_ENTRY(aim_port_entry)), "%d", &aim_port);
	g_snprintf(login_host, sizeof(login_host), "%s", gtk_entry_get_text(GTK_ENTRY(login_host_entry)));
	sscanf(gtk_entry_get_text(GTK_ENTRY(login_port_entry)), "%d", &login_port);
	if (proxy_type != PROXY_NONE) {
		g_snprintf(proxy_host, sizeof(proxy_host), "%s", gtk_entry_get_text(GTK_ENTRY(proxy_host_entry)));
		sscanf(gtk_entry_get_text(GTK_ENTRY(proxy_port_entry)), "%d", &proxy_port);
	}
	save_prefs();
	return TRUE;
}

static void set_connect(GtkWidget *w, int *data)
{
        proxy_type = (int)data;
        if (proxy_type != PROXY_NONE) {
                if (proxy_host_entry)
                        gtk_widget_set_sensitive(proxy_host_entry, TRUE);
                if (proxy_port_entry)
                        gtk_widget_set_sensitive(proxy_port_entry, TRUE);
        } else {
                if (proxy_host_entry)
                        gtk_widget_set_sensitive(proxy_host_entry, FALSE);
                if (proxy_port_entry)
                        gtk_widget_set_sensitive(proxy_port_entry, FALSE);
        }

        save_prefs();
}

static GtkWidget *connect_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)), label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_connect), (void *)which);
	gtk_widget_show(opt);
	if (proxy_type == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void connect_destroy(GtkWidget *n, gpointer d)
{
	proxy_host_entry = NULL;
	proxy_port_entry = NULL;
}

static void connect_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("TOC Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);
	gtk_signal_connect(GTK_OBJECT(prefdialog), "destroy", GTK_SIGNAL_FUNC(connect_destroy), 0);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("AOL has two protocols for connecting to AIM. One of them is Oscar and the other is TOC.\n\nTOC is a published protocol; AOL allows people to use the TOC protocol in their clients to connect. It is a simplified version of Oscar; it is capable of most tasks, but cannot perform all of the functions of Oscar. Because TOC is published, using TOC in gaim tends to be more stable and reliable.\n\nOscar is a proprietary protocol. AOL has not published any information about it. Gaim is able to use Oscar thanks to libfaim, which reverse-engineered the Oscar protocol and is able to emulate it. While libfaim has not decoded or implemented all of the functions of Oscar, it is still able to perform most functions TOC provides as well as several others. However, using Oscar in gaim tends to be less reliable, though more usable.\n\nChanging this option takes effect at signon time."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Use Oscar Protocol"), &general_options, OPT_GEN_USE_OSCAR, box);

	gtk_widget_show(prefdialog);
}

static void toc_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *hbox;
	GtkWidget *opt;
	char buffer[1024];

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("TOC Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);
	gtk_signal_connect(GTK_OBJECT(prefdialog), "destroy", GTK_SIGNAL_FUNC(connect_destroy), 0);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("TOC Host:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	aim_host_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), aim_host_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(aim_host_entry), aim_host);
	gtk_signal_connect(GTK_OBJECT(aim_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(aim_host_entry);

	label = gtk_label_new(_("Port:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	aim_port_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), aim_port_entry, FALSE, FALSE, 0);
	g_snprintf(buffer, sizeof(buffer), "%d", aim_port);
	gtk_entry_set_text(GTK_ENTRY(aim_port_entry), buffer);
	gtk_signal_connect(GTK_OBJECT(aim_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(aim_port_entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Login Host:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	login_host_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), login_host_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(login_host_entry), login_host);
	gtk_signal_connect(GTK_OBJECT(login_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(login_host_entry);

	label = gtk_label_new(_("Port:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	login_port_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), login_port_entry, FALSE, FALSE, 0);
	g_snprintf(buffer, sizeof(buffer), "%d", login_port);
	gtk_entry_set_text(GTK_ENTRY(login_port_entry), buffer);
	gtk_signal_connect(GTK_OBJECT(login_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(login_port_entry);

	opt = connect_radio(_("No Proxy"), PROXY_NONE, box, NULL);
	opt = connect_radio(_("HTTP Proxy"), PROXY_HTTP, box, opt);
	opt = connect_radio(_("Socks 4 Proxy"), PROXY_SOCKS4, box, opt);
	opt = connect_radio(_("Socks 5 Proxy"), PROXY_SOCKS5, box, opt);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Proxy Host:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	proxy_host_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), proxy_host_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(proxy_host_entry), proxy_host);
	gtk_signal_connect(GTK_OBJECT(proxy_host_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(proxy_host_entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Port:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	proxy_port_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), proxy_port_entry, FALSE, FALSE, 0);
	g_snprintf(buffer, sizeof(buffer), "%d", proxy_port);
	gtk_entry_set_text(GTK_ENTRY(proxy_port_entry), buffer);
	gtk_signal_connect(GTK_OBJECT(proxy_port_entry), "focus_out_event", GTK_SIGNAL_FUNC(connection_key_pressed), NULL);
	gtk_widget_show(proxy_port_entry);

	if (proxy_type != PROXY_NONE) {
		gtk_widget_set_sensitive(proxy_host_entry, TRUE);
		gtk_widget_set_sensitive(proxy_port_entry, TRUE);
	} else {
		gtk_widget_set_sensitive(proxy_host_entry, FALSE);
		gtk_widget_set_sensitive(proxy_port_entry, FALSE);
	}

	gtk_widget_show(prefdialog);
}

static void oscar_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Buddy List Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Send Keep-Alive Packet (6 bytes/minute)"), &general_options, OPT_GEN_KEEPALIVE, box);

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
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Show numbers in groups"), &display_options, OPT_DISP_SHOW_GRPNUM, box);
	gaim_button(_("Show idle times"), &display_options, OPT_DISP_SHOW_IDLETIME, box);
	gaim_button(_("Show buddy type icons"), &display_options, OPT_DISP_SHOW_PIXMAPS, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Hide IM/Info/Chat buttons"), &display_options, OPT_DISP_NO_BUTTONS, box);
	gaim_button(_("Show pictures on buttons"), &display_options, OPT_DISP_SHOW_BUTTON_XPM, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Save Window Size/Position"), &general_options, OPT_GEN_SAVED_WINDOWS, box);
#ifdef USE_APPLET
	gaim_button(_("Automatically show buddy list on sign on"), &general_options, OPT_GEN_APP_BUDDY_SHOW, box);
#endif

	gtk_widget_show(prefdialog);
}

static GtkWidget *permtree = NULL;

static void build_deny_tree()
{
	GtkWidget *ti;
        GtkWidget *sub;
        GList *plist = permit;
        GList *dlist = deny;

	if (!permtree) return;

        gtk_tree_clear_items(GTK_TREE(permtree), 0, -1);

        ti = gtk_tree_item_new_with_label(_("Permit"));
        sub = gtk_tree_new();
        gtk_widget_show(ti);
        gtk_widget_show(sub);
        gtk_tree_prepend(GTK_TREE(permtree), ti);
        gtk_tree_item_set_subtree(GTK_TREE_ITEM(ti), sub);
        gtk_tree_item_expand(GTK_TREE_ITEM(ti));
        
        while(plist) {
                ti = gtk_tree_item_new_with_label((char *)plist->data);
                gtk_widget_show(ti);
                gtk_tree_prepend(GTK_TREE(sub), ti);
                plist = plist->next;
        }


        ti = gtk_tree_item_new_with_label(_("Deny"));
        sub = gtk_tree_new();
        gtk_widget_show(ti);
        gtk_widget_show(sub);
        gtk_tree_prepend(GTK_TREE(permtree), ti);
        gtk_tree_item_set_subtree(GTK_TREE_ITEM(ti), sub);
        gtk_tree_item_expand(GTK_TREE_ITEM(ti));
        
        while(dlist) {
                ti = gtk_tree_item_new_with_label((char *)dlist->data);
                gtk_widget_show(ti);
                gtk_tree_prepend(GTK_TREE(sub), ti);
                dlist = dlist->next;
        }
}

static void do_del_perm(GtkWidget *w, GtkTree *ptree)
{
	GtkLabel *label, *plabel;
	GtkWidget *item, *pitem;
	char *c, *d;
	GList *i;
	
        GList *plist;
        GList *dlist;
	int level;

        plist = permit;
        dlist = deny;
        
	i = GTK_TREE_SELECTION(ptree);
	if (i) {
		item = GTK_WIDGET(i->data);
		gtk_tree_unselect_child(GTK_TREE(ptree), item);
		label = GTK_LABEL(GTK_BIN(item)->child);
		gtk_label_get(label, &c);
		level = GTK_TREE(item->parent)->level;
		if (level > 0) {
			pitem = GTK_WIDGET(GTK_TREE(item->parent)->tree_owner);
			plabel = GTK_LABEL(GTK_BIN(pitem)->child);
			gtk_label_get(plabel, &d);
                        if (!strcasecmp(d, _("Permit"))) {
                                while(plist) {
                                        if (!strcasecmp((char *)(plist->data), c)) {
                                                permit = g_list_remove(permit, plist->data);
                                                break;
                                        }

                                        plist = plist->next;
                                }

                        } else {
                                while(dlist) {
                                        if (!strcasecmp((char *)(dlist->data), c)) {
                                                deny = g_list_remove(deny, dlist->data);
                                                
                                                break;
                                        }
                                        dlist = dlist->next;
                                }

                        }

                        
                } else {
                        /* Can't delete groups here! :) */
                        return;
                }
                serv_set_permit_deny();
		gtk_tree_clear_items(GTK_TREE(ptree), 0, -1);
                build_permit_tree();
                serv_save_config();
	}
}


static void set_permit(GtkWidget *w, int *data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
		permdeny = (int)data;
	        if (blist) {
			do_export(0, 0);
			serv_save_config();
			/* we do this here because we can :) */
			serv_set_permit_deny();
		}
	}
}

static GtkWidget *deny_radio(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)), label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(set_permit), (void *)which);
	gtk_widget_show(opt);
	if (permdeny == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void permdest(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(permtree);
	permtree = NULL;
}

static void add_perm_callback(GtkWidget *widget, void *dummy)
{
	if (!blist)
		do_error_dialog(_("Please sign on before editing the permit/deny lists."),
				_("Please sign on"));
	else
	        show_add_perm(NULL);
}

static void deny_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *xbox;
	GtkWidget *opt;
	GtkWidget *button;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Permit/Deny List Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	label = gtk_label_new(_("The permit/deny configuration will change between users,\n"
				"and changes while you are signed off will not be saved."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 5);
	gtk_widget_show(vbox);

	opt = deny_radio(_("Allow Anyone"), PERMIT_ALL, vbox, NULL);
#if 0
	/* This doesn't work because TOC doesn't have a PERMIT_BUDDY setting
	 * and merging the two would be very difficult at best, most likely
	 * impossible. If we can guarantee only Oscar than this is easy */
	opt = deny_radio(_("Allow only users on Buddy List"), PERMIT_BUDDY, vbox, opt);
#endif
	opt = deny_radio(_("Allow only the users in \"Permit\""), PERMIT_SOME, vbox, opt);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 5);
	gtk_widget_show(vbox);

	opt = deny_radio(_("Block all users"), PERMIT_NONE, vbox, opt);
	opt = deny_radio(_("Block the users in \"Deny\""), DENY_SOME, vbox, opt);

	xbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), xbox, TRUE, TRUE, 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(xbox),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(xbox);

	permtree = gtk_tree_new();
	build_deny_tree();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(xbox), permtree);
	gtk_signal_connect(GTK_OBJECT(permtree), "destroy", GTK_SIGNAL_FUNC(permdest), 0);
	gtk_widget_show(permtree);

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(prefs, _("Add"), gnome_add_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(add_perm_callback), NULL);

	button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 10);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_del_perm), permtree);

	gtk_widget_show(prefdialog);
}

void build_permit_tree()
{
	if (permtree)
		deny_page();
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
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Enter sends message"), &general_options, OPT_GEN_ENTER_SENDS, box);
	gaim_button(_("Control-{B/I/U/S} inserts HTML tags"), &general_options, OPT_GEN_CTL_CHARS, box);
	gaim_button(_("Control-(number) inserts smileys"), &general_options, OPT_GEN_CTL_SMILEYS, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Show graphical smileys"), &display_options, OPT_DISP_SHOW_SMILEY, box);
	gaim_button(_("Show timestamp on messages"), &display_options, OPT_DISP_SHOW_TIME, box);
	gaim_button(_("Ignore incoming colors"), &display_options, OPT_DISP_IGNORE_COLOUR, box);
	gaim_button(_("Ignore white backgrounds"), &display_options, OPT_DISP_IGN_WHITE, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Log all conversations"), &general_options, OPT_GEN_LOG_ALL, box);
	gaim_button(_("Strip HTML from logs"), &general_options, OPT_GEN_STRIP_HTML, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Highlight misspelled words"), &general_options, OPT_GEN_CHECK_SPELLING, box);
	gaim_button(_("Show URLs as links"), &general_options, OPT_GEN_SEND_LINKS, box);
	gaim_button(_("Sending messages removes away status"), &general_options, OPT_GEN_BACK_ON_IM, box);

	gtk_widget_show(prefdialog);
}

static void im_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("IM Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Show logins in window"), &display_options, OPT_DISP_SHOW_LOGON, box);
	gaim_button(_("Show buttons with text"), &display_options, OPT_DISP_CONV_SHOW_TEXT, box);
	gaim_button(_("Show larger entry box on new windows"), &display_options, OPT_DISP_CONV_BIG_ENTRY, box);
	gaim_button(_("Raise windows on events"), &general_options, OPT_GEN_POPUP_WINDOWS, box);
	gaim_button(_("Ignore new conversations when away"), &general_options, OPT_GEN_DISCARD_WHEN_AWAY, box);
	gaim_button(_("Ignore TiK Automated Messages"), &general_options, OPT_GEN_TIK_HACK, box);

	gtk_widget_show(prefdialog);
}

static void chat_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Chat Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Show people joining/leaving in window"), &display_options, OPT_DISP_CHAT_LOGON, box);
	gaim_button(_("Show buttons with text"), &display_options, OPT_DISP_CHAT_SHOW_TEXT, box);
	gaim_button(_("Show larger entry box on new windows"), &display_options, OPT_DISP_CHAT_BIG_ENTRY, box);
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

	prefdialog = gtk_frame_new(_("Chat Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
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

static fgbgdes(GtkWidget *w, gpointer d)
{
	pref_fg_picture = NULL;
	pref_bg_picture = NULL;
}

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
	GtkWidget *sep;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *select;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Font Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
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
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color), pref_fg_picture);

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
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color), pref_bg_picture);

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
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

#ifdef USE_GNOME
	gaim_button(_("Sounds go through GNOME"), &sound_options, OPT_SOUND_THROUGH_GNOME, box);
#endif
	gaim_button(_("No sounds when you log in"), &sound_options, OPT_SOUND_SILENT_SIGNON, box);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, box);
	gaim_button(_("Beep instead of playing sound"), &sound_options, OPT_SOUND_BEEP, box);

	gtk_widget_show(prefdialog);
}

static void event_page()
{
	GtkWidget *parent;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *sep;

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Sound Events"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("All options take effect immediately unless otherwise noted."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	gaim_button(_("Sound when buddy logs in"), &sound_options, OPT_SOUND_LOGIN, box);
	gaim_button(_("Sound when buddy logs out"), &sound_options, OPT_SOUND_LOGOUT, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Sound when message is received"), &sound_options, OPT_SOUND_RECV, box);
	gaim_button(_("Sound when message is first received"), &sound_options, OPT_SOUND_FIRST_RCV, box);
	gaim_button(_("Sound when message is sent"), &sound_options, OPT_SOUND_SEND, box);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	gaim_button(_("Sound in chat rooms when people enter/leave"), &sound_options, OPT_SOUND_CHAT_JOIN, box);
	gaim_button(_("Sound in chat rooms when people talk"), &sound_options, OPT_SOUND_CHAT_SAY, box);

	gtk_widget_show(prefdialog);
}

static struct away_message *cur_message;
static char *edited_message;
static GtkWidget *away_text;

void away_list_clicked(GtkWidget *widget, struct away_message *a)
{
	gchar buffer[2048];
	guint text_len;
	
	cur_message = a;
	
	/* Get proper Length */
	text_len = gtk_text_get_length(GTK_TEXT(away_text));
	
	/* Clear the Box */
	gtk_text_set_point(GTK_TEXT(away_text), 0 );
	gtk_text_forward_delete (GTK_TEXT(away_text), text_len);

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

	if (!i->next) {
		int text_len = gtk_text_get_length(GTK_TEXT(away_text));
		gtk_text_set_point(GTK_TEXT(away_text), 0 );
		gtk_text_forward_delete (GTK_TEXT(away_text), text_len);
	}
	a = gtk_object_get_user_data(GTK_OBJECT(i->data));
	rem_away_mess(NULL, a);
}

static void paldest(GtkWidget *m, gpointer n)
{
	gtk_widget_destroy(prefs_away_list);
	prefs_away_list = NULL;
}

static void do_away_mess(GtkWidget *m, gpointer n)
{
	GList *i = GTK_LIST(prefs_away_list)->selection;
	if (i)
		do_away_message(NULL, gtk_object_get_user_data(GTK_OBJECT(i->data)));
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
	GList *awy = away_messages;
	struct away_message *a;
	char buffer[BUF_LONG];

	parent = prefdialog->parent;
	gtk_widget_destroy(prefdialog);

	prefdialog = gtk_frame_new(_("Away Messages"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefdialog), box);
	gtk_widget_show(box);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_set_usize(hbox, -1, 30);
	gtk_widget_show(hbox);

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
	gtk_box_pack_start(GTK_BOX(box), top, FALSE, TRUE, 0);
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
	
	button = picture_button(prefs, _("Make Away"), gnome_preferences_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_away_mess), NULL);
	gtk_box_pack_start(GTK_BOX(bot), button, TRUE, FALSE, 5);

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
                gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_list_clicked), a);
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

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	gaim_button(_("Ignore new conversations when away"), &general_options, OPT_GEN_DISCARD_WHEN_AWAY, hbox);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, hbox);

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
		opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)), label);
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
	prefs_away_list = NULL;

	prefdialog = gtk_frame_new(_("Browser Options"));
	gtk_container_add(GTK_CONTAINER(parent), prefdialog);

	box = gtk_vbox_new(FALSE, 5);
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
	opt = browser_radio(_("Internal HTML Widget (Quite likely a bad idea!)"), BROWSER_INTERNAL, box, opt);
	opt = browser_radio(_("Manual"), BROWSER_MANUAL, box, opt);

	browser_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), browser_entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(browser_entry), web_command);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "focus_out_event", GTK_SIGNAL_FUNC(manualentry_key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "destroy", GTK_SIGNAL_FUNC(brentdes), NULL);
	gtk_widget_show(browser_entry);

	new_window = gaim_button(_("Pop up new window by default"), &general_options, OPT_GEN_BROWSER_POPUP, box);

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

static void try_me(GtkCTree *ctree, GtkCTreeNode *node)
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
	GtkWidget *preftree;
	GtkWidget *container;
	GtkWidget *hbox;
	GtkWidget *close;

	if (prefs) {
		gtk_widget_show(prefs);
		return;
	}

	prefs = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(prefs);
	aol_icon(prefs->window);
	gtk_container_border_width(GTK_CONTAINER(prefs), 10);
	gtk_window_set_title(GTK_WINDOW(prefs), _("Gaim - Preferences"));
	gtk_widget_set_usize(prefs, 600, 550);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(prefs), vbox);
	gtk_widget_show(vbox);

	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 5);
	gtk_widget_show(hpaned);
	
       	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_paned_pack1(GTK_PANED(hpaned), scroll, FALSE, FALSE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_widget_set_usize(scroll, 125, -1);
	gtk_widget_show(scroll);

	preftree = gtk_ctree_new(1, 0);
	gtk_ctree_set_line_style (GTK_CTREE(preftree), GTK_CTREE_LINES_SOLID);
        gtk_ctree_set_expander_style(GTK_CTREE(preftree), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_clist_set_reorderable(GTK_CLIST(preftree), FALSE);
	gtk_container_add(GTK_CONTAINER(scroll), preftree);
	gtk_signal_connect(GTK_OBJECT(preftree), "tree_select_row", GTK_SIGNAL_FUNC(try_me), NULL);
	gtk_widget_show(preftree);

	prefs_build_general(preftree);
	prefs_build_connect(preftree);
	prefs_build_buddy(preftree);
	prefs_build_convo(preftree);
	prefs_build_sound(preftree);
	prefs_build_away(preftree);
	prefs_build_browser(preftree);

	container = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(container), 0);
	gtk_frame_set_shadow_type(GTK_FRAME(container), GTK_SHADOW_NONE);
	gtk_paned_pack2(GTK_PANED(hpaned), container, TRUE, TRUE);
	gtk_widget_show(container);

	prefdialog = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(container), prefdialog);
	gtk_widget_show(prefdialog);

	general_page();

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	close = picture_button(prefs, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), close, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(handle_delete), NULL);

	gtk_widget_show(prefs);
}

char debug_buff[BUF_LONG];

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

static void build_debug()
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

void show_debug(GtkObject *obj)
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

void debug_print(char *chars)
{
	if (general_options & OPT_GEN_DEBUG && dw)
		gtk_text_insert(GTK_TEXT(dw->entry), NULL, NULL, NULL, chars, strlen(chars));
#ifdef DEBUG
        printf("%s\n", chars);
#endif
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
	
        return FALSE;
}

void set_option(GtkWidget *w, int *option)
{
	*option = !(*option);
}

void set_general_option(GtkWidget *w, int *option)
{
	general_options = general_options ^ (int)option;

       	if ((int)option == OPT_GEN_LOG_ALL)
       		update_log_convs();

	if ((int)option == OPT_GEN_KEEPALIVE)
		update_keepalive(general_options & OPT_GEN_KEEPALIVE);

	if (prefrem)
		gtk_signal_handler_block_by_data(GTK_OBJECT(prefrem), (int *)OPT_GEN_REMEMBER_PASS);
	if (remember)
		gtk_signal_handler_block_by_data(GTK_OBJECT(remember), (int *)OPT_GEN_REMEMBER_PASS);
	if (prefrem)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefrem),
			(general_options & OPT_GEN_REMEMBER_PASS));
	if (remember)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(remember),
			(general_options & OPT_GEN_REMEMBER_PASS));
	if (prefrem)
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(prefrem), (int *)OPT_GEN_REMEMBER_PASS);
	if (remember)
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(remember), (int *)OPT_GEN_REMEMBER_PASS);

	save_prefs();
}

void set_display_option(GtkWidget *w, int *option)
{
        display_options = display_options ^ (int)option;

	if (blist) build_imchat_box(!(display_options & OPT_DISP_NO_BUTTONS));

	if (blist) update_button_pix();

	update_chat_button_pix();

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

void prefs_build_general(GtkWidget *preftree)
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("General");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, general_page);
}

void prefs_build_connect(GtkWidget *preftree)
{
	GtkCTreeNode *parent, *node;
	char *text[1];

	text[0] = _("Connection");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, connect_page);

	text[0] = _("TOC Options");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, toc_page);

	text[0] = _("Oscar Options");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, oscar_page);
}

void prefs_build_buddy(GtkWidget *preftree)
{
	GtkCTreeNode *parent, *node;
	char *text[1];

	text[0] = _("Buddy List");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, buddy_page);

	/* FIXME ! We shouldn't be showing this if we're not signed on */
	text[0] = _("Permit/Deny");
	node = gtk_ctree_insert_node(GTK_CTREE(preftree), parent, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), node, deny_page);
}

void prefs_build_convo(GtkWidget *preftree)
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

void prefs_build_sound(GtkWidget *preftree)
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

void prefs_build_away(GtkWidget *preftree)
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("Away Messages");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, away_page);
}

void prefs_build_browser(GtkWidget *preftree)
{
	GtkCTreeNode *parent;
	char *text[1];

	text[0] = _("Browser");
	parent = gtk_ctree_insert_node(GTK_CTREE(preftree), NULL, NULL,
					text, 5, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(preftree), parent, browser_page);
}
