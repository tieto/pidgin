/*
 * gaim
 *
 * File: wspell.c
 * Date: March, 2003
 * Description: Windows Gaim gtkspell interface.
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtkspell/gtkspell.h>
#include "debug.h"
#include "win32dep.h"

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

static char* lookup_aspell_path() {
	char *path = NULL;
	const char *tmp;
	HKEY reg_key;
	DWORD type;
	DWORD nbytes;
	gboolean found_reg_key;
	LPCTSTR subkey = NULL;

	if ((tmp = g_getenv("GAIM_ASPELL_DIR")))
		return g_strdup(tmp);

	if (G_WIN32_HAVE_WIDECHAR_API ()) {
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Aspell", 0,
					KEY_QUERY_VALUE,
					&reg_key) == ERROR_SUCCESS) {
			subkey = (LPCTSTR) L"Path";
			if (!(found_reg_key = RegQueryValueExW(reg_key,
							(WCHAR*) subkey, NULL,
							&type, NULL, &nbytes
							) == ERROR_SUCCESS)) {
				subkey = NULL;
				found_reg_key = (RegQueryValueExW(reg_key,
							(WCHAR*) subkey, NULL,
							&type, NULL, &nbytes
					     ) == ERROR_SUCCESS);
			}
			if (found_reg_key) {
				wchar_t *wc_temp = g_new (wchar_t, (nbytes + 1) / 2 + 1);
				RegQueryValueExW(reg_key, (WCHAR*) subkey,
						NULL, &type, (LPBYTE) wc_temp,
						&nbytes);
				wc_temp[nbytes / 2] = '\0';
				path = g_utf16_to_utf8(
						wc_temp, -1, NULL, NULL, NULL);
				g_free (wc_temp);
			}
		}
    	} else {
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Aspell", 0,
					KEY_QUERY_VALUE, &reg_key
					) == ERROR_SUCCESS) {
			subkey = "Path";
			if (!(found_reg_key = RegQueryValueExA(reg_key, subkey,
							NULL, &type, NULL,
							&nbytes
							) == ERROR_SUCCESS)) {
				subkey = NULL;
				found_reg_key = (RegQueryValueExA(reg_key,
							subkey, NULL, &type,
							NULL, &nbytes
							) == ERROR_SUCCESS);
			}
			if (found_reg_key) {
				char *cp_temp = g_malloc (nbytes + 1);
				RegQueryValueExA(reg_key, subkey, NULL, &type,
						cp_temp, &nbytes);
				cp_temp[nbytes] = '\0';
				path = g_locale_to_utf8(
						cp_temp, -1, NULL, NULL, NULL);
				g_free (cp_temp);
			}
		}
	}

	if (reg_key != NULL) {
		RegCloseKey(reg_key);
	}

	return path;
}

void wgaim_gtkspell_init() {
	char *aspell_path = lookup_aspell_path();

	if (aspell_path != NULL) {
		char *tmp = g_strconcat(aspell_path, "\\aspell-15.dll", NULL);
		if (g_file_test(tmp, G_FILE_TEST_EXISTS)) {
			const char *path = g_getenv("PATH");
			gaim_debug_info("wspell", "Found Aspell in %s\n", aspell_path);

			g_free(tmp);

			tmp = g_strdup_printf("%s%s%s", (path ? path : ""),
					(path ? G_SEARCHPATH_SEPARATOR_S : ""),
					aspell_path);

			g_setenv("PATH", tmp, TRUE);

			load_gtkspell();
		} else {
			gaim_debug_warning("wspell", "Couldn't find aspell-15.dll\n");
		}

		g_free(tmp);
		g_free(aspell_path);
	} else {
		gaim_debug_warning("wspell", "Couldn't find path for Aspell\n");
	}
}
