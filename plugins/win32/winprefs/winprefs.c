/*
 * gaim - WinGaim Options Plugin
 *
 * File: winprefs.c
 * Date: December 12, 2002
 * Description: Gaim Plugin interface
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
#include <windows.h>
#include <winreg.h>
#include <winerror.h>
#include <gdk/gdkwin32.h>

#include "internal.h"
#include "gtkinternal.h"

#include "prefs.h"
#include "debug.h"

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
static GaimPlugin *plugin_id = NULL;
static GtkAppBar *blist_ab = NULL;
static GtkWidget *blist = NULL;

/*
 *  PROTOS
 */
static void blist_create_cb();

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
        if(blist_ab && gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE)) {
                if(blist_ab->docked) {
                        gaim_prefs_set_int(OPT_WINPREFS_DBLIST_HEIGHT, blist_ab->undocked_height);
                        gaim_prefs_set_int(OPT_WINPREFS_DBLIST_SIDE, blist_ab->side);
                }
                gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKED, blist_ab->docked);
        }
        else
                gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKED, FALSE);
}

static void blist_set_dockable(gboolean val) {
        if(val) {
                if(!blist_ab && blist)
                        blist_ab = gtk_appbar_add(blist);
        }
        else {
                gtk_appbar_remove(blist_ab);
                blist_ab = NULL;
        }
}

static void gaim_quit_cb() {
        gaim_debug(GAIM_DEBUG_INFO, WINPREFS_PLUGIN_ID, "gaim_quit_cb: removing appbar\n");
        blist_save_state();
        blist_set_dockable(FALSE);
}

static gboolean blist_create_cb_remove(gpointer data) {
        gaim_signal_disconnect(plugin_id, event_signon, blist_create_cb);
        return FALSE;
}

static void blist_create_cb() {
        gaim_debug(GAIM_DEBUG_INFO, WINPREFS_PLUGIN_ID, "event_signon\n");

        blist = GAIM_GTK_BLIST(gaim_get_blist())->window;
        if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE)) {
                blist_set_dockable(TRUE);
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKED)) {
                        blist_ab->undocked_height = gaim_prefs_get_int(OPT_WINPREFS_DBLIST_HEIGHT);
                        gtk_appbar_dock(blist_ab,
                                        gaim_prefs_get_int(OPT_WINPREFS_DBLIST_SIDE));
                }
        }
        /* removing here will cause a crash when going to next cb
           in the gaim signal cb loop.. so process delayed. */
        g_idle_add(blist_create_cb_remove, NULL);
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
		gaim_debug(GAIM_DEBUG_ERROR, WINPREFS_PLUGIN_ID, "open_run_key: Could not open key for writing value\n");
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
                        gaim_debug(GAIM_DEBUG_ERROR, WINPREFS_PLUGIN_ID, "GetModuleFileName Error.. Could not set Gaim autostart.\n");
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
                        gaim_debug(GAIM_DEBUG_ERROR, WINPREFS_PLUGIN_ID, "Could not set registry key value\n");
        }
        else {
                if(ERROR_SUCCESS != RegDeleteValue(hKey, "Gaim"))
                        gaim_debug(GAIM_DEBUG_ERROR, WINPREFS_PLUGIN_ID, "Could not delete registry key value\n");
        }
        RegCloseKey(hKey);
}

static void winprefs_set_blist_dockable(GtkWidget *w) {
        gaim_prefs_set_bool(OPT_WINPREFS_DBLIST_DOCKABLE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)) ? blist_set_dockable(TRUE) : blist_set_dockable(FALSE);
}


/*
 *  EXPORTED FUNCTIONS
 */

gboolean plugin_load(GaimPlugin *plugin) {
        plugin_id = plugin;

        /* blist docking init */
        if(!blist && gaim_get_blist() && GAIM_GTK_BLIST(gaim_get_blist())) {
                blist = GAIM_GTK_BLIST(gaim_get_blist())->window;
                if(gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE))
                        blist_set_dockable(TRUE);
        }
        else
                gaim_signal_connect(plugin, event_signon, blist_create_cb, NULL);

        gaim_signal_connect(plugin, event_quit, gaim_quit_cb, NULL);

        return TRUE;
}

gboolean plugin_unload(GaimPlugin *plugin) {
        blist_set_dockable(FALSE);
        return TRUE;
}

static GtkWidget* get_config_frame(GaimPlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *button;
	GtkWidget *vbox;
	HKEY hKey;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = gaim_gtk_make_frame (ret, _("Startup"));

        /* Autostart */
	button = wgaim_button(_("_Start Gaim on Windows startup"), vbox);
	if(open_run_key(&hKey, KEY_QUERY_VALUE)) {
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, "Gaim", 0, NULL, NULL, NULL)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		}
	}
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_autostart), NULL);

        /* Dockable Blist */
        button = wgaim_button(_("_Dockable Buddy List"), vbox);
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(winprefs_set_blist_dockable), NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(OPT_WINPREFS_DBLIST_DOCKABLE));

	gtk_widget_show_all(ret);
	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info =
{
	2,
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
}

GAIM_INIT_PLUGIN(winprefs, init_plugin, info)
