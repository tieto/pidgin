/*
 * gaim - WinGaim Options Plugin
 *
 * File: winprefs.c
 * Date: December 12, 2002
 *
 * copyright (c) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 59 temple place, suite 330, boston, ma  02111-1307  usa
 *
 */
#include <gdk/gdkwin32.h>

#include "internal.h"

#include "core.h"
#include "prefs.h"
#include "debug.h"
#include "signals.h"
#include "version.h"

#include "gtkplugin.h"
#include "gtkutils.h"
#include "gtkblist.h"
#include "gtkappbar.h"

/*
 *  MACROS & DEFINES
 */
#define WINPREFS_PLUGIN_ID               "gtk-win-prefs"

/*
 *  LOCALS
 */
static const char *OPT_WINPREFS_DBLIST_DOCKABLE =      "/plugins/gtk/win32/winprefs/dblist_dockable";
static const char *OPT_WINPREFS_DBLIST_DOCKED =        "/plugins/gtk/win32/winprefs/dblist_docked";
static const char *OPT_WINPREFS_DBLIST_HEIGHT =        "/plugins/gtk/win32/winprefs/dblist_height";
static const char *OPT_WINPREFS_DBLIST_SIDE =          "/plugins/gtk/win32/winprefs/dblist_side";
static const char *OPT_WINPREFS_DBLIST_ON_TOP =        "/plugins/gtk/win32/winprefs/dblist_on_top";
static const char *OPT_WINPREFS_BLIST_ON_TOP =         "/plugins/gtk/win32/winprefs/blist_on_top";
static const char *OPT_WINPREFS_IM_BLINK =             "/plugins/gtk/win32/winprefs/im_blink";

static GaimPlugin *plugin_id = NULL;
static GtkAppBar *blist_ab = NULL;
static GtkWidget *blist = NULL;

/*
 *  PROTOS
 */
static void blist_create_cb(GaimBuddyList *blist, void *data);

/*
 *  CODE
 */

/* UTIL */

static GtkWidget *wgaim_button(const char *text, GtkWidget *page) {
        GtkWidget *button;
	button = gtk_check_button_new_with_mnemonic(text);
        gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
        return button;
}

/* BLIST DOCKING */

static void blist_save_state() {
        if(blist_ab) {
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE) && blist_ab->docked) {
                        gaim_prefs_set_int(OPT_WINPREFS_DBLIST_HEIGHT, blist_ab->undocked_height);
                        gaim_prefs_set_int(OPT_WINPREFS_DBLIST_SIDE, blist_ab->side);
                        gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKED, blist_ab->docked);
                }
                else
                        gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKED, FALSE);
        }
}

static void blist_set_ontop(gboolean val) {
        if(!blist)
                return;
        if(val)
                SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        else
                SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

static void blist_dock_cb(gboolean val) {
        if(val) {
                gaim_debug_info(WINPREFS_PLUGIN_ID, "Blist Docking..\n");
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP))
                        blist_set_ontop(TRUE);
        }
        else {
                gaim_debug_info(WINPREFS_PLUGIN_ID, "Blist Undocking..\n");
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP) &&
                   !gaim_prefs_get_bool(OPT_WINPREFS_BLIST_ON_TOP))
                        blist_set_ontop(FALSE);
        }
}

static void blist_set_dockable(gboolean val) {
        if(val) {
                if(!blist_ab && blist) {
                        blist_ab = gtk_appbar_add(blist);
                        gtk_appbar_add_dock_cb(blist_ab, blist_dock_cb);
                }
        }
        else {
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP) &&
                   !gaim_prefs_get_bool(OPT_WINPREFS_BLIST_ON_TOP))
                        blist_set_ontop(FALSE);
                gtk_appbar_remove(blist_ab);
                blist_ab = NULL;
        }
}

/* PLUGIN CALLBACKS */

/* We need this because the blist destroy cb won't be called before the
   plugin is unloaded, when quitting */
static void gaim_quit_cb() {
        gaim_debug_info(WINPREFS_PLUGIN_ID, "gaim_quit_cb: removing appbar\n");
        blist_save_state();
        blist_set_dockable(FALSE);
}

static void blist_create_cb(GaimBuddyList *gaim_blist, void *data) {
	gaim_debug_info(WINPREFS_PLUGIN_ID, "buddy list created\n");

	blist = GAIM_GTK_BLIST(gaim_blist)->window;

        if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE)) {
                blist_set_dockable(TRUE);
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKED)) {
                        blist_ab->undocked_height = gaim_prefs_get_int(OPT_WINPREFS_DBLIST_HEIGHT);
                        gtk_appbar_dock(blist_ab,
                                        gaim_prefs_get_int(OPT_WINPREFS_DBLIST_SIDE));
                        if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP))
                                blist_set_ontop(TRUE);
                }
        }
        if(gaim_prefs_get_bool(OPT_WINPREFS_BLIST_ON_TOP)) {
                blist_set_ontop(TRUE);
        }
}

/* AUTOSTART */

static int open_run_key(PHKEY phKey, REGSAM samDesired) {
        /* First try current user key (for WinNT & Win2k +), fall back to local machine */
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, 
					 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
					 0,  samDesired,  phKey));
	else if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
					      0,  samDesired,  phKey));
	else {
		gaim_debug_error(WINPREFS_PLUGIN_ID, "open_run_key: Could not open key for writing value\n");
		return 0;
	}
	return 1;
}

/* WIN PREFS GENERAL */

static void winprefs_set_autostart(GtkWidget *w) {
        HKEY hKey;

        if(!open_run_key(&hKey, KEY_SET_VALUE))
                return;
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
                char buffer[1024];
                DWORD size;
                
                if((size = GetModuleFileName(wgaim_hinstance(),
                                             (LPBYTE)buffer,
                                             sizeof(buffer)))==0) {
                        gaim_debug_error(WINPREFS_PLUGIN_ID, "GetModuleFileName Error.. Could not set Gaim autostart.\n");
                        RegCloseKey(hKey);
                        return;
                }
                /* Now set value of new key */
                if(ERROR_SUCCESS != RegSetValueEx(hKey,
                                                  "Gaim",
                                                  0,
                                                  REG_SZ,
                                                  buffer,
                                                  size))
                        gaim_debug_error(WINPREFS_PLUGIN_ID, "Could not set registry key value\n");
        }
        else {
                if(ERROR_SUCCESS != RegDeleteValue(hKey, "Gaim"))
                        gaim_debug_error(WINPREFS_PLUGIN_ID, "Could not delete registry key value\n");
        }
        RegCloseKey(hKey);
}

static void winprefs_set_blist_dockable(GtkWidget *w, GtkWidget *w1) {
        gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKABLE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
                blist_set_dockable(TRUE);
                gtk_widget_set_sensitive(w1, TRUE);
        }
        else {
                blist_set_dockable(FALSE);
                gtk_widget_set_sensitive(w1, FALSE);
        }
}

static void winprefs_set_blist_ontop(GtkWidget *w) {
        gaim_prefs_set_bool(OPT_WINPREFS_BLIST_ON_TOP, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
                blist_set_ontop(TRUE);
        }
        else {
                if(!(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE) &&
                     gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP) &&
                     (blist_ab && blist_ab->docked)))
                        blist_set_ontop(FALSE);
        }
}

static void winprefs_set_dblist_ontop(GtkWidget *w) {
        gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_ON_TOP, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
        if(blist && blist_ab) {
                if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
                        if(blist_ab->docked)
                                blist_set_ontop(TRUE);
                }
                else if(!gaim_prefs_get_bool(OPT_WINPREFS_BLIST_ON_TOP))
                        blist_set_ontop(FALSE);
        }
}

static void winprefs_set_im_blink(GtkWidget *w) {
        gaim_prefs_set_bool(OPT_WINPREFS_IM_BLINK, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
        wgaim_conv_im_blink_state(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

/*
 *  EXPORTED FUNCTIONS
 */

gboolean plugin_load(GaimPlugin *plugin) {
        plugin_id = plugin;

        /* blist docking init */
	if (gaim_get_blist() && GAIM_GTK_BLIST(gaim_get_blist()) && GAIM_GTK_BLIST(gaim_get_blist())->window) {
		blist_create_cb(gaim_get_blist(), NULL);
	} else {
		gaim_signal_connect(gaim_gtk_blist_get_handle(), "gtkblist-created", plugin_id, GAIM_CALLBACK(blist_create_cb), NULL);
	}

        wgaim_conv_im_blink_state(gaim_prefs_get_bool(OPT_WINPREFS_IM_BLINK));

        gaim_signal_connect((void*)gaim_get_core(), "quitting", plugin, GAIM_CALLBACK(gaim_quit_cb), NULL);

        return TRUE;
}

gboolean plugin_unload(GaimPlugin *plugin) {
        blist_set_dockable(FALSE);
        wgaim_conv_im_blink_state(TRUE);
        return TRUE;
}

static GtkWidget* get_config_frame(GaimPlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *button;
        GtkWidget *dbutton;
	GtkWidget *vbox;
        char* gtk_version = NULL;
        HKEY hKey = HKEY_CURRENT_USER;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

        while(hKey) {
                char version[25];
                DWORD vlen = 25;
                if(wgaim_read_reg_string(hKey, "SOFTWARE\\GTK\\2.0", "Version", (LPBYTE)&version, &vlen)) {
                        char revision[25];
                        DWORD rlen = 25;
                        gboolean gotrev = FALSE;
                        if(wgaim_read_reg_string(hKey, "SOFTWARE\\GTK\\2.0", "Revision", (LPBYTE)&revision, &rlen)) {
                                revision[0] = g_ascii_toupper(revision[0]);
                                revision[1] = '\0';
                                gotrev = TRUE;
                        }
                        gtk_version = g_strdup_printf("%s%s%s", 
                                                      version, 
                                                      gotrev?" Revision ":"", 
                                                      gotrev?revision:"");
                        hKey = 0;
                }
                if(hKey == HKEY_CURRENT_USER)
                        hKey = HKEY_LOCAL_MACHINE;
                else if(hKey == HKEY_LOCAL_MACHINE)
                        hKey = 0;
        }

        /* Display Installed GTK+ Runtime Version */
        if(gtk_version) {
                GtkWidget *label;
                vbox = gaim_gtk_make_frame(ret, _("GTK+ Runtime Version"));
                label = gtk_label_new(gtk_version);
                gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
                gtk_widget_show(label);
                g_free(gtk_version);
        }

        /* Autostart */
	vbox = gaim_gtk_make_frame (ret, _("Startup"));
	button = wgaim_button(_("_Start Gaim on Windows startup"), vbox);
	if(open_run_key(&hKey, KEY_QUERY_VALUE)) {
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, "Gaim", 0, NULL, NULL, NULL)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		}
	}
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_autostart), NULL);

        /* Buddy List */
	vbox = gaim_gtk_make_frame (ret, _("Buddy List"));
        button = wgaim_button(_("_Dockable Buddy List"), vbox);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE));

        /* Docked Blist On Top */
        dbutton = wgaim_button(_("Docked _Buddy List is always on top"), vbox);
        gtk_signal_connect(GTK_OBJECT(dbutton), "clicked", GTK_SIGNAL_FUNC(winprefs_set_dblist_ontop), NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dbutton), gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_ON_TOP));
        if(!gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE))
                gtk_widget_set_sensitive(GTK_WIDGET(dbutton), FALSE);

        /* Connect cb for Dockable option.. passing dblist on top button */
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_blist_dockable), dbutton);

        /* Blist On Top */
        button = wgaim_button(_("_Keep Buddy List window on top"), vbox);
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_blist_ontop), NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(OPT_WINPREFS_BLIST_ON_TOP));

        /* Conversations */
	vbox = gaim_gtk_make_frame (ret, _("Conversations"));
        button = wgaim_button(_("_Flash Window when messages are received"), vbox);
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_im_blink), NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(OPT_WINPREFS_IM_BLINK));

	gtk_widget_show_all(ret);
	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	WINPREFS_PLUGIN_ID,
	N_("WinGaim Options"),
	VERSION,
	N_("Options specific to Windows Gaim."),
	N_("Options specific to Windows Gaim."),
	"Herman Bloggs <hermanator12002@yahoo.com>",
	GAIM_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
        gaim_prefs_add_none("/plugins/gtk/win32");
        gaim_prefs_add_none("/plugins/gtk/win32/winprefs");
        gaim_prefs_add_bool(OPT_WINPREFS_DBLIST_DOCKABLE, FALSE);
        gaim_prefs_add_bool(OPT_WINPREFS_DBLIST_DOCKED, FALSE);
        gaim_prefs_add_int(OPT_WINPREFS_DBLIST_HEIGHT, 0);
        gaim_prefs_add_int(OPT_WINPREFS_DBLIST_SIDE, 0);
        gaim_prefs_add_bool(OPT_WINPREFS_DBLIST_ON_TOP, FALSE);
        gaim_prefs_add_bool(OPT_WINPREFS_BLIST_ON_TOP, FALSE);
        gaim_prefs_add_bool(OPT_WINPREFS_IM_BLINK, TRUE);
}

GAIM_INIT_PLUGIN(winprefs, init_plugin, info)
