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
#include "prefs.h"
#include <gtk/gtk.h>

typedef struct
{
	GtkWidget *window;
	GtkWidget *text;

	gboolean timestamps;
	gboolean paused;

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

	gaim_prefs_set_bool("/gaim/gtk/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
__configure_cb(GtkWidget *w, GdkEventConfigure *event, DebugWindow *win)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		gaim_prefs_set_int("/gaim/gtk/debug/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/debug/height", event->height);
	}

	return FALSE;
}

static void
__clear_cb(GtkWidget *w, DebugWindow *win)
{
	gtk_imhtml_clear(GTK_IMHTML(win->text));
}

static void
__pause_cb(GtkWidget *w, DebugWindow *win)
{
	win->paused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static void
__timestamps_cb(GtkWidget *w, DebugWindow *win)
{
	win->timestamps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));

	gaim_prefs_set_bool("/gaim/gtk/debug/timestamps", win->timestamps);
						
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *sw;
	GtkWidget *button;
	int width, height;

	win = g_new0(DebugWindow, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/debug/width");
	height = gaim_prefs_get_int("/gaim/gtk/debug/height");

	GAIM_DIALOG(win->window);
	gtk_window_set_default_size(GTK_WINDOW(win->window), width, height);
	gtk_window_set_role(GTK_WINDOW(win->window), "debug");
	gtk_window_set_title(GTK_WINDOW(win->window), _("Debug Window"));

	g_signal_connect(G_OBJECT(win->window), "delete_event",
					 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win->window), "configure_event",
					 G_CALLBACK(__configure_cb), win);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win->window), vbox);

	if (gaim_prefs_get_bool("/gaim/gtk/debug/toolbar")) {
		/* Setup our top button bar thingie. */
		toolbar = gtk_toolbar_new();
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
								  GTK_ICON_SIZE_SMALL_TOOLBAR);

		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

#if 0
		/* Find button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_FIND,
								 NULL, NULL, NULL, NULL, -1);

		/* Save */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_SAVE,
								 NULL, NULL, NULL, NULL, -1);
#endif

		/* Clear button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_CLEAR,
								 NULL, NULL, G_CALLBACK(__clear_cb), win, -1);

		gtk_toolbar_insert_space(GTK_TOOLBAR(toolbar), -1);

		/* Pause */
		gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
								   GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
								   _("Pause"), NULL, NULL,
								   NULL, G_CALLBACK(__pause_cb), win);

		/* Timestamps */
		button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
											GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
											NULL, _("Timestamps"), NULL, NULL,
											NULL, G_CALLBACK(__timestamps_cb),
											win);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
				gaim_prefs_get_bool("/gaim/gtk/debug/timestamps"));
	}

	/* Now our scrolled window... */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);

	/* ... which has a gtkimhtml in it. */
	win->text = gtk_imhtml_new(NULL, NULL);

	gtk_container_add(GTK_CONTAINER(sw), win->text);

	/* Pack it in... Not like that, sicko. */
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	gtk_widget_show_all(win->window);

	return win;
}

static void
debug_enabled_cb(const char *name, GaimPrefType type, gpointer value,
				 gpointer data)
{
	if (debug_win == NULL)
		gaim_gtk_debug_window_show();
	else
		gaim_gtk_debug_window_hide();
}

void
gaim_gtk_debug_init(void)
{
	/* Debug window preferences. */
	gaim_prefs_add_none("/gaim/gtk/debug");
	gaim_prefs_add_bool("/gaim/gtk/debug/enabled", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/debug/timestamps", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/debug/toolbar", TRUE);
	gaim_prefs_add_int("/gaim/gtk/debug/width",  400);
	gaim_prefs_add_int("/gaim/gtk/debug/height", 150);

	gaim_prefs_connect_callback("/gaim/gtk/debug/enabled",
								debug_enabled_cb, NULL);
}

void
gaim_gtk_debug_window_show(void)
{
	if (debug_win == NULL)
		debug_win = debug_window_new();

	gtk_widget_show(debug_win->window);

	gaim_prefs_set_bool("/gaim/gtk/debug/enabled", TRUE);
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
	gchar *arg_s, *ts_s;
	gboolean timestamps;

	timestamps = gaim_prefs_get_bool("/gaim/gtk/debug/timestamps");

	arg_s = g_strdup_vprintf(format, args);

	if (category == NULL) {
		ts_s = g_strdup("");
	}
	else {
		/*
		 * If the category is not NULL, then do timestamps.
		 * This IS right. :)
		 */
		if (timestamps) {
			gchar mdate[64];
			time_t mtime = time(NULL);

			strftime(mdate, sizeof(mdate), "%H:%M:%S", localtime(&mtime));

			ts_s = g_strdup_printf("(%s) ", mdate);
		}
		else
			ts_s = g_strdup("");
	}

	if (gaim_prefs_get_bool("/gaim/gtk/debug/enabled") &&
		debug_win != NULL && !debug_win->paused) {

		gchar *esc_s, *cat_s, *s;

		if (category == NULL)
			cat_s = g_strdup("");
		else
			cat_s = g_strdup_printf("<b>%s:</b> ", category);

		esc_s = g_markup_escape_text(arg_s, -1);

		s = g_strdup_printf("<font color=\"%s\">%s%s%s</font>",
							debug_fg_colors[level], ts_s, cat_s, esc_s);

		g_free(esc_s);

		if (level == GAIM_DEBUG_FATAL) {
			gchar *temp = s;

			s = g_strdup_printf("<b>%s</b>", temp);
			g_free(temp);
		}

		g_free(cat_s);

		gtk_imhtml_append_text(GTK_IMHTML(debug_win->text), s, -1, 0);

		g_free(s);
	}

	if (opt_debug) {
		if (category == NULL)
			g_print("%s%s", ts_s, arg_s);
		else
			g_print("%s%s: %s", ts_s, category, arg_s);
	}

	g_free(ts_s);
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

