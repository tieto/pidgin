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
#define GAIM_PLUGINS
#include <windows.h>
#include <winreg.h>
#include <winerror.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>
#include "gaim.h"
#include "win32dep.h"

/*
 *  MACROS & DEFINES
 */
#define WINPREFS_VERSION 1

/* Plugin options */
#define OPT_WGAIM_AUTOSTART               0x00000001

/*
 *  LOCALS
 */
guint winprefs_options=0;

/*
 *  PROTOS
 */
extern GtkWidget *gaim_button(const char*, guint*, int, GtkWidget*);

/*
 *  CODE
 */

static void write_options(FILE *f) {
	fprintf(f, "options {\n");
	fprintf(f, "\twinprefs_options { %u }\n", winprefs_options);
	fprintf(f, "}\n");
}

static void save_winprefs_prefs() {
	FILE *f;
	char buf[1024];

	if (gaim_home_dir()) {
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S ".gaim" G_DIR_SEPARATOR_S "winprefsrc", gaim_home_dir());
	}
	else
		return;

	if ((f = fopen(buf, "w"))) {
		fprintf(f, "# winprefsrc v%d\n", WINPREFS_VERSION);
		write_options(f);
		fclose(f);
	}
	else
		debug_printf("Error opening wintransrc\n");
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
		debug_printf("open_run_key: Could not open key for writing value\n");
		return 0;
	}
	return 1;
}

static void set_winprefs_option(GtkWidget *w, int option) {
	winprefs_options ^= option;
	save_winprefs_prefs();

	if(option == OPT_WGAIM_AUTOSTART) {
		HKEY hKey;

		if(!open_run_key(&hKey, KEY_SET_VALUE))
			return;
		if(winprefs_options & OPT_WGAIM_AUTOSTART) {
			char buffer[1024];
			DWORD size;

			if((size = GetModuleFileName(wgaim_hinstance(),
						     (LPBYTE)buffer,
						     sizeof(buffer)))==0) {
				debug_printf("GetModuleFileName Error.. Could not set Gaim autostart.\n");
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
				debug_printf("Could not set registry key value\n");
 		}
		else {
			if(ERROR_SUCCESS != RegDeleteValue(hKey, "Gaim"))
				debug_printf("Could not delete registry key value\n");
		}
		RegCloseKey(hKey);
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *gaim_plugin_init(GModule *handle) {
	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
}

struct gaim_plugin_description desc; 

G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("WinGaim Options"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Options specific to Windows Gaim.")); 
	desc.authors = g_strdup("Herman Bloggs &lt;hermanator12002@yahoo.com&gt;");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

G_MODULE_EXPORT char *name() {
	return _("WinGaim Options");
}

G_MODULE_EXPORT char *description() {
	return _("Options specific to Windows Gaim.");
}

G_MODULE_EXPORT GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *ret;
	GtkWidget *button;
	GtkWidget *vbox;
	HKEY hKey;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	/* IM Convo trans options */
	vbox =  make_frame (ret, _("Startup"));
	button = gaim_button(_("_Start Gaim on Windows startup"), &winprefs_options, OPT_WGAIM_AUTOSTART, vbox);
	/* Set initial value */
	if(open_run_key(&hKey, KEY_QUERY_VALUE)) {
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, "Gaim", 0, NULL, NULL, NULL)) {
			winprefs_options ^= OPT_WGAIM_AUTOSTART;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		}
	}
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_winprefs_option), (int *)OPT_WGAIM_AUTOSTART);

	gtk_widget_show_all(ret);
	return ret;
}
