/**
 * @file gtkdebug.c GTK+ Debug API
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
 */
#include "gtkdebug.h"
#include "gaim.h"
#include "gtkimhtml.h"
#include <gtk/gtk.h>

typedef struct
{
	GtkWidget *window;
	GtkWidget *entry;

} DebugWindow;

static char debug_fg_colors[][8] = {
	"#000000",    /**< All debug levels. */
	"#666666",    /**< Blather.          */
	"#000000",    /**< Information.      */
	"#660000",    /**< Warnings.         */
	"#FF0000",    /**< Errors.           */
	"#FF0000",    /**< Fatal errors.     */
};

static DebugWindow *debug_win = NULL;

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	g_free(debug_win);
	debug_win = NULL;

	if (misc_options & OPT_MISC_DEBUG)
		misc_options ^= OPT_MISC_DEBUG;

	save_prefs();

	return FALSE;
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *sw;

	win = g_new0(DebugWindow, 1);

	GAIM_DIALOG(win->window);
	gtk_window_set_default_size(GTK_WINDOW(win->window), 500, 200);
	gtk_window_set_role(GTK_WINDOW(win->window), "debug");
	gtk_window_set_title(GTK_WINDOW(win->window), _("Debug Window"));

	g_signal_connect(G_OBJECT(win->window), "delete_event",
					 G_CALLBACK(debug_window_destroy), NULL);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	win->entry = gtk_imhtml_new(NULL, NULL);

	gtk_container_add(GTK_CONTAINER(sw), win->entry);
	gtk_container_add(GTK_CONTAINER(win->window), sw);
	gtk_widget_show_all(win->window);
	
	return win;
}

void
gaim_gtk_debug_window_show(void)
{
	if (debug_win == NULL)
		debug_win = debug_window_new();

	gtk_widget_show(debug_win->window);
}

void
gaim_gtk_debug_window_hide(void)
{
	if (debug_win != NULL) {
		gtk_widget_destroy(debug_win->window);
		debug_window_destroy(NULL, NULL, NULL);
	}
}

static void
gaim_gtk_debug_print(GaimDebugLevel level, const char *category,
					 const char *format, va_list args)
{
	va_list ap;
	gchar *esc_s, *arg_s, *cat_s, *s;

	arg_s = g_strdup_vprintf(format, args);

	if ((misc_options & OPT_MISC_DEBUG) && debug_win != NULL) {
		if (category == NULL)
			cat_s = g_strdup("");
		else
			cat_s = g_strdup_printf("<b>%s:</b> ", category);

		esc_s = g_markup_escape_text(arg_s, -1);

		s = g_strdup_printf("<font color=\"%s\">%s%s</font>",
							debug_fg_colors[level], cat_s, esc_s);

		g_free(esc_s);

		if (level == GAIM_DEBUG_FATAL) {
			gchar *temp = s;

			s = g_strdup_printf("<b>%s</b>", temp);
			g_free(temp);
		}

		g_free(cat_s);

		gtk_imhtml_append_text(GTK_IMHTML(debug_win->entry), s, -1, 0);

		g_free(s);
	}

	if (opt_debug) {
		if (category == NULL)
			g_print("%s", arg_s);
		else
			g_print("%s: %s", category, arg_s);
	}

	g_free(arg_s);
}

static GaimDebugUiOps ops =
{
	gaim_gtk_debug_print
};

GaimDebugUiOps *
gaim_get_gtk_debug_ui_ops(void)
{
	return &ops;
}

