/**
 * @file gtkdebug.c GTK+ Debug API
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "internal.h"
#include "gtkgaim.h"

#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "util.h"

#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "gtkstock.h"

typedef struct
{
	GtkWidget *window;
	GtkWidget *text;
	GtkWidget *find;

	gboolean timestamps;
	gboolean paused;

} DebugWindow;

static char debug_fg_colors[][8] = {
	"#000000",    /**< All debug levels. */
	"#666666",    /**< Misc.             */
	"#000000",    /**< Information.      */
	"#660000",    /**< Warnings.         */
	"#FF0000",    /**< Errors.           */
	"#FF0000",    /**< Fatal errors.     */
};

static DebugWindow *debug_win = NULL;

struct _find {
	DebugWindow *window;
	GtkWidget *entry;
};

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	gaim_prefs_disconnect_by_handle(gaim_gtk_debug_get_handle());

	/* If the "Save Log" dialog is open then close it */
	gaim_request_close_with_handle(debug_win);

	g_free(debug_win);
	debug_win = NULL;

	gaim_prefs_set_bool("/gaim/gtk/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, DebugWindow *win)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		gaim_prefs_set_int("/gaim/gtk/debug/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/debug/height", event->height);
	}

	return FALSE;
}

static void
do_find_cb(GtkWidget *widget, gint response, struct _find *f)
{
	switch (response) {
	case GTK_RESPONSE_OK:
	    gtk_imhtml_search_find(GTK_IMHTML(f->window->text),
							   gtk_entry_get_text(GTK_ENTRY(f->entry)));
		break;

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CLOSE:
		gtk_imhtml_search_clear(GTK_IMHTML(f->window->text));
		gtk_widget_destroy(f->window->find);
		f->window->find = NULL;
		g_free(f);
		break;
	}
}

static void
find_cb(GtkWidget *w, DebugWindow *win)
{
	GtkWidget *hbox, *img, *label;
	struct _find *f;

	if(win->find)
	{
		gtk_window_present(GTK_WINDOW(win->find));
		return;
	}

	f = g_malloc(sizeof(struct _find));
	f->window = win;
	win->find = gtk_dialog_new_with_buttons(_("Find"),
					GTK_WINDOW(win->window), GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					GTK_STOCK_FIND, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(win->find),
					 GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(win->find), "response",
					G_CALLBACK(do_find_cb), f);

	gtk_container_set_border_width(GTK_CONTAINER(win->find), 6);
	gtk_window_set_resizable(GTK_WINDOW(win->find), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(win->find), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(win->find)->vbox), 12);
	gtk_container_set_border_width(
		GTK_CONTAINER(GTK_DIALOG(win->find)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(win->find)->vbox),
					  hbox);
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(win->find),
									  GTK_RESPONSE_OK, FALSE);

	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Search for:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	f->entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(f->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(f->entry));
	g_signal_connect(G_OBJECT(f->entry), "changed",
					 G_CALLBACK(gaim_gtk_set_sensitive_if_input),
					 win->find);
	gtk_box_pack_start(GTK_BOX(hbox), f->entry, FALSE, FALSE, 0);

	gtk_widget_show_all(win->find);
	gtk_widget_grab_focus(f->entry);
}

static void
save_writefile_cb(void *user_data, const char *filename)
{
	DebugWindow *win = (DebugWindow *)user_data;
	FILE *fp;
	char *tmp;

	if ((fp = fopen(filename, "w+")) == NULL) {
		gaim_notify_error(win, NULL, _("Unable to open file."), NULL);
		return;
	}

	tmp = gtk_imhtml_get_text(GTK_IMHTML(win->text), NULL, NULL);
	fprintf(fp, "Gaim Debug log : %s\n", gaim_date_full());
	fprintf(fp, "%s", tmp);
	g_free(tmp);

	fclose(fp);
}

static void
save_cb(GtkWidget *w, DebugWindow *win)
{
	gaim_request_file(win, _("Save Debug Log"), "gaim-debug.log", TRUE,
					  G_CALLBACK(save_writefile_cb), NULL, win);
}

static void
clear_cb(GtkWidget *w, DebugWindow *win)
{
	gtk_imhtml_clear(GTK_IMHTML(win->text));
}

static void
pause_cb(GtkWidget *w, DebugWindow *win)
{
	win->paused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static void
timestamps_cb(GtkWidget *w, DebugWindow *win)
{
	win->timestamps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));

	gaim_prefs_set_bool("/core/debug/timestamps", win->timestamps);
}

static void
timestamps_pref_cb(const char *name, GaimPrefType type, gpointer value,
				   gpointer data)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), GPOINTER_TO_INT(value));
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *image;
	int width, height;

	win = g_new0(DebugWindow, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/debug/width");
	height = gaim_prefs_get_int("/gaim/gtk/debug/height");

	GAIM_DIALOG(win->window);
	gaim_debug_info("gtkdebug", "Setting dimensions to %d, %d\n",
			   width, height);

	gtk_window_set_default_size(GTK_WINDOW(win->window), width, height);
	gtk_window_set_role(GTK_WINDOW(win->window), "debug");
	gtk_window_set_title(GTK_WINDOW(win->window), _("Debug Window"));

	g_signal_connect(G_OBJECT(win->window), "delete_event",
					 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win->window), "configure_event",
					 G_CALLBACK(configure_cb), win);

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

		/* Find button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_FIND,
								 NULL, NULL, G_CALLBACK(find_cb), win, -1);

		/* Save */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_SAVE,
								 NULL, NULL, G_CALLBACK(save_cb), win, -1);

		/* Clear button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_CLEAR,
								 NULL, NULL, G_CALLBACK(clear_cb), win, -1);

		gtk_toolbar_insert_space(GTK_TOOLBAR(toolbar), -1);

		/* Pause */
		image = gtk_image_new_from_stock(GAIM_STOCK_PAUSE, GTK_ICON_SIZE_MENU);
		button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
											GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
											NULL, _("Pause"), NULL, NULL,
											image, G_CALLBACK(pause_cb), win);

		/* Timestamps */
		button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
											GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
											NULL, _("Timestamps"), NULL, NULL,
											NULL, G_CALLBACK(timestamps_cb),
											win);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
						gaim_prefs_get_bool("/core/debug/timestamps"));

		gaim_prefs_connect_callback(gaim_gtk_debug_get_handle(), "/core/debug/timestamps",
									timestamps_pref_cb, button);
	}

	/* Add the gtkimhtml */
	frame = gaim_gtk_create_imhtml(FALSE, &win->text, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	gtk_widget_show_all(win->window);

	return win;
}

static void
debug_enabled_cb(const char *name, GaimPrefType type, gpointer value,
				 gpointer data)
{
	if (value)
		gaim_gtk_debug_window_show();
	else
		gaim_gtk_debug_window_hide();
}

static void
gaim_glib_log_handler(const gchar *domain, GLogLevelFlags flags,
					  const gchar *msg, gpointer user_data)
{
	GaimDebugLevel level;
	char *new_msg = NULL;
	char *new_domain = NULL;

	if ((flags & G_LOG_LEVEL_ERROR) == G_LOG_LEVEL_ERROR)
		level = GAIM_DEBUG_ERROR;
	else if ((flags & G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL)
		level = GAIM_DEBUG_FATAL;
	else if ((flags & G_LOG_LEVEL_WARNING) == G_LOG_LEVEL_WARNING)
		level = GAIM_DEBUG_WARNING;
	else if ((flags & G_LOG_LEVEL_MESSAGE) == G_LOG_LEVEL_MESSAGE)
		level = GAIM_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_INFO) == G_LOG_LEVEL_INFO)
		level = GAIM_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_DEBUG) == G_LOG_LEVEL_DEBUG)
		level = GAIM_DEBUG_MISC;
	else
	{
		gaim_debug_warning("gtkdebug",
				   "Unknown glib logging level in %d\n", flags);

		level = GAIM_DEBUG_MISC; /* This will never happen. */
	}

	if (msg != NULL)
		new_msg = gaim_utf8_try_convert(msg);

	if (domain != NULL)
		new_domain = gaim_utf8_try_convert(domain);

	if (new_msg != NULL)
	{
		gaim_debug(level, (new_domain != NULL ? new_domain : "g_log"),
				   "%s\n", new_msg);

		g_free(new_msg);
	}

	if (new_domain != NULL)
		g_free(new_domain);
}

#ifdef _WIN32
static void
gaim_glib_dummy_print_handler(const gchar *string)
{
}
#endif

void
gaim_gtk_debug_init(void)
{
	/* Debug window preferences. */
	/*
	 * NOTE: This must be set before prefs are loaded, and the callbacks
	 *       set after they are loaded, since prefs sets the enabled
	 *       preference here and that loads the window, which calls the
	 *       configure event, which overrides the width and height! :P
	 */

	gaim_prefs_add_none("/gaim/gtk/debug");

	/* Controls printing to the debug window */
	gaim_prefs_add_bool("/gaim/gtk/debug/enabled", FALSE);

	gaim_prefs_add_bool("/gaim/gtk/debug/toolbar", TRUE);
	gaim_prefs_add_int("/gaim/gtk/debug/width",  450);
	gaim_prefs_add_int("/gaim/gtk/debug/height", 250);

	gaim_prefs_connect_callback(NULL, "/gaim/gtk/debug/enabled",
								debug_enabled_cb, NULL);

#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  gaim_glib_log_handler, NULL)

	/* Register the glib/gtk log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("Gdk");
	REGISTER_G_LOG_HANDLER("Gtk");
	REGISTER_G_LOG_HANDLER("GdkPixbuf");
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");

#ifdef _WIN32
	if (!opt_debug)
		g_set_print_handler(gaim_glib_dummy_print_handler);
#endif
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
	gboolean timestamps;
	gchar *arg_s, *ts_s;
	gchar *esc_s, *cat_s, *tmp, *s;

	if (!gaim_prefs_get_bool("/gaim/gtk/debug/enabled") ||
					(debug_win == NULL) || debug_win->paused) {
		return;
	}

	timestamps = gaim_prefs_get_bool("/core/debug/timestamps");

	arg_s = g_strdup_vprintf(format, args);

	/*
 	 * For some reason we only print the timestamp if category is
 	 * not NULL.  Why the hell do we do that?  --Mark
	 */
	if ((category != NULL) && (timestamps)) {
		gchar mdate[64];

		time_t mtime = time(NULL);
		strftime(mdate, sizeof(mdate), "%H:%M:%S", localtime(&mtime));
		ts_s = g_strdup_printf("(%s) ", mdate);
	} else {
		ts_s = g_strdup("");
	}

	if (category == NULL)
		cat_s = g_strdup("");
	else
		cat_s = g_strdup_printf("<b>%s:</b> ", category);

	esc_s = g_markup_escape_text(arg_s, -1);

	g_free(arg_s);

	s = g_strdup_printf("<font color=\"%s\">%s%s%s</font>",
						debug_fg_colors[level], ts_s, cat_s, esc_s);

	g_free(ts_s);
	g_free(cat_s);
	g_free(esc_s);

	tmp = gaim_utf8_try_convert(s);
	g_free(s);
	s = tmp;

	if (level == GAIM_DEBUG_FATAL) {
		tmp = g_strdup_printf("<b>%s</b>", s);
		g_free(s);
		s = tmp;
	}

	gtk_imhtml_append_text(GTK_IMHTML(debug_win->text), s, 0);

	g_free(s);
}

static GaimDebugUiOps ops =
{
	gaim_gtk_debug_print
};

GaimDebugUiOps *
gaim_gtk_debug_get_ui_ops(void)
{
	return &ops;
}

void *
gaim_gtk_debug_get_handle() {
	static int handle;

	return &handle;
}

