/*
 * winprefs.c
 *
 * copyright (c) 1998-2002, Herman Bloggs <hermanator12002@yahoo.com>
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
#include "gaim.h"
#include "gtkplugin.h"
#include "prefs.h"
#include "win32dep.h"

/*
 *  MACROS & DEFINES
 */
#define WINPREFS_PLUGIN_ID             "gaim-winprefs"
#define WINPREFS_VERSION                 1

/*
 *  LOCALS
 */
static const char *OPT_WINPREFS_AUTOSTART="/plugins/gtk/win32/winprefs/auto_start";

/*
 *  PROTOS
 */

/*
 *  CODE
 */

static GtkWidget *wgaim_button(const char *text, const char *pref, GtkWidget *page) {
        GtkWidget *button;
	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), gaim_prefs_get_bool(pref));
        gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
        return button;
}

static int open_run_key(PHKEY phKey, REGSAM samDesired) {
        /* First try current user key (for WinNT & Win2k +), fall back to local machine */
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, 
					 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
					 0,  samDesired,  phKey));
	else if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
					      0,  samDesired,  phKey));
	else {
		gaim_debug(3, WINPREFS_PLUGIN_ID, "open_run_key: Could not open key for writing value\n");
		return 0;
	}
	return 1;
}

static void set_winprefs_option(GtkWidget *w, const char *key) {
        gaim_prefs_set_bool(key, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
	if(key == OPT_WINPREFS_AUTOSTART) {
		HKEY hKey;

		if(!open_run_key(&hKey, KEY_SET_VALUE))
			return;
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
			char buffer[1024];
			DWORD size;

			if((size = GetModuleFileName(wgaim_hinstance(),
						     (LPBYTE)buffer,
						     sizeof(buffer)))==0) {
				gaim_debug(3, WINPREFS_PLUGIN_ID, "GetModuleFileName Error.. Could not set Gaim autostart.\n");
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
				gaim_debug(3, WINPREFS_PLUGIN_ID, "Could not set registry key value\n");
 		}
		else {
			if(ERROR_SUCCESS != RegDeleteValue(hKey, "Gaim"))
				gaim_debug(3, WINPREFS_PLUGIN_ID, "Could not delete registry key value\n");
		}
		RegCloseKey(hKey);
	}
}

/*
 *  EXPORTED FUNCTIONS
 */
static GtkWidget* get_config_frame(GaimPlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *button;
	GtkWidget *vbox;
	HKEY hKey;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	/* IM Convo trans options */
	vbox = gaim_gtk_make_frame (ret, _("Startup"));
	button = wgaim_button(_("_Start Gaim on Windows startup"), OPT_WINPREFS_AUTOSTART, vbox);
	/* Set initial value */
	if(open_run_key(&hKey, KEY_QUERY_VALUE)) {
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, "Gaim", 0, NULL, NULL, NULL)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		}
	}
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_winprefs_option), (void *)OPT_WINPREFS_AUTOSTART);

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
	WEBSITE,
	NULL,
	NULL,
	NULL,
	&ui_info,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
  gaim_prefs_add_none("/plugins/gtk/win32");
  gaim_prefs_add_none("/plugins/gtk/win32/winprefs");
  gaim_prefs_add_bool("/plugins/gtk/win32/winprefs/auto_start", FALSE);
}

GAIM_INIT_PLUGIN(winprefs, init_plugin, info);
