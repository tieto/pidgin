/*
 * pidgin
 *
 * File: wspell.c
 * Date: March, 2003
 * Description: Windows Purple gtkspell interface.
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
#include "wspell.h"

/* GTKSPELL DUMMY FUNCS */
static GtkSpell* wgtkspell_new_attach(GtkTextView *view, const gchar *lang, GError **error) {return NULL;}
static GtkSpell* wgtkspell_get_from_text_view(GtkTextView *view) {return NULL;}
static void wgtkspell_detach(GtkSpell *spell) {}
static gboolean wgtkspell_set_language(GtkSpell *spell, const gchar *lang, GError **error) {return FALSE;}
static void wgtkspell_recheck_all(GtkSpell *spell) {}

/* GTKSPELL PROTOS */
GtkSpell*         (*wpidginspell_new_attach)              (GtkTextView *,
							     const gchar *,
							     GError **) = wgtkspell_new_attach;

GtkSpell*         (*wpidginspell_get_from_text_view)      (GtkTextView*) = wgtkspell_get_from_text_view;

void              (*wpidginspell_detach)                  (GtkSpell*) = wgtkspell_detach;

gboolean          (*wpidginspell_set_language)            (GtkSpell*,
							     const gchar*,
							     GError**) = wgtkspell_set_language;

void              (*wpidginspell_recheck_all)             (GtkSpell*) = wgtkspell_recheck_all;

static void load_gtkspell() {
	wpidginspell_new_attach = (void*) wpurple_find_and_loadproc("libgtkspell.dll", "gtkspell_new_attach" );
	wpidginspell_get_from_text_view = (void*) wpurple_find_and_loadproc("libgtkspell.dll", "gtkspell_get_from_text_view");
	wpidginspell_detach = (void*) wpurple_find_and_loadproc("libgtkspell.dll", "gtkspell_detach");
	wpidginspell_set_language = (void*) wpurple_find_and_loadproc("libgtkspell.dll", "gtkspell_set_language");
	wpidginspell_recheck_all = (void*) wpurple_find_and_loadproc("libgtkspell.dll", "gtkspell_recheck_all");
}

static char* lookup_aspell_path() {
	const char *tmp;

	if ((tmp = g_getenv("PURPLE_ASPELL_DIR")))
		return g_strdup(tmp);

	return wpurple_read_reg_string(HKEY_LOCAL_MACHINE, "Software\\Aspell", "Path");
}

void winpidgin_spell_init() {
	char *aspell_path = lookup_aspell_path();

	if (aspell_path != NULL) {
		char *tmp = g_strconcat(aspell_path, "\\aspell-15.dll", NULL);
		if (g_file_test(tmp, G_FILE_TEST_EXISTS)) {
			const char *path = g_getenv("PATH");
			purple_debug_info("wspell", "Found Aspell in %s\n", aspell_path);

			g_free(tmp);

			tmp = g_strdup_printf("%s%s%s", (path ? path : ""),
					(path ? G_SEARCHPATH_SEPARATOR_S : ""),
					aspell_path);

			g_setenv("PATH", tmp, TRUE);

			load_gtkspell();
		} else {
			purple_debug_warning("wspell", "Couldn't find aspell-15.dll\n");
		}

		g_free(tmp);
		g_free(aspell_path);
	} else {
		purple_debug_warning("wspell", "Couldn't find path for Aspell\n");
	}
}
