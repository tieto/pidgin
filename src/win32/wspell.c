/*
 *  wspell.c
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: March, 2003
 *  Description: Windows Gaim gtkspell interface.
 */
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtkspell/gtkspell.h>
#include "win32dep.h"

extern void debug_printf(char * fmt, ...);

/* GTKSPELL DUMMY FUNCS */
GtkSpell* wgtkspell_new_attach(GtkTextView *view,
			       const gchar *lang, 
			       GError **error) {return NULL;}
GtkSpell* wgtkspell_get_from_text_view(GtkTextView *view) {return NULL;}
void      wgtkspell_detach(GtkSpell *spell) {}
gboolean  wgtkspell_set_language(GtkSpell *spell,
				const gchar *lang,
				GError **error) {return 0;}
void      wgtkspell_recheck_all(GtkSpell *spell) {}

/* GTKSPELL PROTOS */
GtkSpell*         (*wgaim_gtkspell_new_attach)              (GtkTextView *,
							     const gchar *, 
							     GError **) = wgtkspell_new_attach;

GtkSpell*         (*wgaim_gtkspell_get_from_text_view)      (GtkTextView*) = wgtkspell_get_from_text_view;

void              (*wgaim_gtkspell_detach)                  (GtkSpell*) = wgtkspell_detach;

gboolean          (*wgaim_gtkspell_set_language)            (GtkSpell*,
							     const gchar*,
							     GError**) = wgtkspell_set_language;

void              (*wgaim_gtkspell_recheck_all)             (GtkSpell*) = wgtkspell_recheck_all;

static void load_gtkspell() {
	wgaim_gtkspell_new_attach = (void*)wgaim_find_and_loadproc("libgtkspell.dll", "gtkspell_new_attach" );
	wgaim_gtkspell_get_from_text_view = (void*)wgaim_find_and_loadproc("libgtkspell.dll", "gtkspell_get_from_text_view");
	wgaim_gtkspell_detach = (void*)wgaim_find_and_loadproc("libgtkspell.dll", "gtkspell_detach");
	wgaim_gtkspell_set_language = (void*)wgaim_find_and_loadproc("libgtkspell.dll", "gtkspell_set_language");
	wgaim_gtkspell_recheck_all = (void*)wgaim_find_and_loadproc("libgtkspell.dll", "gtkspell_recheck_all");
}

void wgaim_gtkspell_init() {
	    HKEY hKey;
	    char buffer[1024] = "";
	    DWORD size = sizeof(buffer);
	    DWORD type;

	    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					     "Software\\Aspell", 
					     0,  
					     KEY_QUERY_VALUE,  
					     &hKey)) {
		    /* Official aspell.net win32 installation or Gaim's aspell installation */
		    if(ERROR_SUCCESS == RegQueryValueEx(hKey, "Path", NULL, &type, (LPBYTE)buffer, &size) ||
		       ERROR_SUCCESS == RegQueryValueEx(hKey, "", NULL, &type, (LPBYTE)buffer, &size)) {
			    int mark = strlen(buffer);
			    strcat(buffer, "\\aspell-15.dll");
			    if(_access( buffer, 0 ) < 0)
				    debug_printf("Couldn't find aspell-15.dll\n");
			    else {
				    char* tmp=NULL;
				    buffer[mark] = '\0';
				    debug_printf("Found Aspell in %s\n", buffer);
				    /* Add path to Aspell dlls to PATH */
				    tmp = g_malloc0(strlen(getenv("PATH")) + strlen(buffer) + 7);
				    sprintf(tmp, "PATH=%s;%s", getenv("PATH"), buffer);
				    putenv(tmp);
				    g_free(tmp);
				    load_gtkspell();
			    }
		    }
		    else {
			    debug_printf("Couldn't find path for Aspell\n");
		    }
		    RegCloseKey(hKey);
	    }
}
