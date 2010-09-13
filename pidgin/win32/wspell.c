/*
 * pidgin
 *
 * File: wspell.c
 * Date: March, 2003
 * Description: Windows Pidgin gtkspell interface.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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

/* Intermediate function so that we can eat Enchant error popups when it doesn't find a DLL
 * This is fixed upstream, but not released */
GtkSpell*         (*wpidginspell_new_attach_proxy)              (GtkTextView *,
							     const gchar *,
							     GError **) = NULL;

/* GTKSPELL DUMMY FUNCS */
static GtkSpell* wgtkspell_new_attach(GtkTextView *view, const gchar *lang, GError **error) {
	GtkSpell *ret = NULL;
	if (wpidginspell_new_attach_proxy) {
		UINT old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
		ret = wpidginspell_new_attach_proxy(view, lang, error);
		SetErrorMode(old_error_mode);
	}
	return ret;
}
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

#define GTKSPELL_DLL "libgtkspell-0.dll"

static void load_gtkspell() {
	UINT old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
	gchar *tmp, *tmp2;

	const char *path = g_getenv("PATH");
	tmp = g_build_filename(wpurple_install_dir(), "spellcheck", NULL);
	tmp2 = g_strdup_printf("%s%s%s", tmp,
		(path ? G_SEARCHPATH_SEPARATOR_S : ""),
		(path ? path : ""));
	g_free(tmp);
	g_setenv("PATH", tmp2, TRUE);
	g_free(tmp2);

	tmp = g_build_filename(wpurple_install_dir(), "spellcheck", GTKSPELL_DLL, NULL);
	/* Suppress error popups */
	wpidginspell_new_attach_proxy = (void*) wpurple_find_and_loadproc(tmp, "gtkspell_new_attach" );
	if (wpidginspell_new_attach_proxy) {
		wpidginspell_get_from_text_view = (void*) wpurple_find_and_loadproc(tmp, "gtkspell_get_from_text_view");
		wpidginspell_detach = (void*) wpurple_find_and_loadproc(tmp, "gtkspell_detach");
		wpidginspell_set_language = (void*) wpurple_find_and_loadproc(tmp, "gtkspell_set_language");
		wpidginspell_recheck_all = (void*) wpurple_find_and_loadproc(tmp, "gtkspell_recheck_all");
	} else {
		purple_debug_warning("wspell", "Couldn't load gtkspell (%s) \n", tmp);
		/*wpidginspell_new_attach = wgtkspell_new_attach;*/
	}
	g_free(tmp);
	SetErrorMode(old_error_mode);
}

void winpidgin_spell_init() {
	load_gtkspell();
}
