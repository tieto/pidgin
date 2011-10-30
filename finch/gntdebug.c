/**
 * @file gntdebug.c GNT Debug API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <internal.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntentry.h>
#include <gntfilesel.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttextview.h>

#include "gntdebug.h"
#include "finch.h"
#include "notify.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#define PREF_ROOT "/finch/debug"

static gboolean
handle_fprintf_stderr_cb(GIOChannel *source, GIOCondition cond, gpointer null)
{
	gssize size;
	char message[1024];

	size = read(g_io_channel_unix_get_fd(source), message, sizeof(message) - 1);
	if (size <= 0) {
		/* Something bad probably happened elsewhere ... let's ignore */
	} else {
		message[size] = '\0';
		g_log("stderr", G_LOG_LEVEL_WARNING, "%s", message);
	}

	return TRUE;
}

static void
handle_fprintf_stderr(gboolean stop)
{
	GIOChannel *stderrch;
	static int readhandle = -1;
	int pipes[2];

	if (stop) {
		if (readhandle >= 0) {
			g_source_remove(readhandle);
			readhandle = -1;
		}
		return;
	}
	if (pipe(pipes)) {
		readhandle = -1;
		return;
	};
	dup2(pipes[1], STDERR_FILENO);

	stderrch = g_io_channel_unix_new(pipes[0]);
	g_io_channel_set_close_on_unref(stderrch, TRUE);
	readhandle = g_io_add_watch_full(stderrch, G_PRIORITY_HIGH,
			G_IO_IN | G_IO_ERR | G_IO_PRI,
			handle_fprintf_stderr_cb, NULL, NULL);
	g_io_channel_unref(stderrch);
}

static struct
{
	GntWidget *window;
	GntWidget *tview;
	GntWidget *search;
	gboolean paused;
} debug;

static gboolean
match_string(const char *category, const char *args)
{
	const char *str = gnt_entry_get_text(GNT_ENTRY(debug.search));
	if (!str || !*str)
		return TRUE;
	if (g_strrstr(category, str) != NULL)
		return TRUE;
	if (g_strrstr(args, str) != NULL)
		return TRUE;
	return FALSE;
}

static void
finch_debug_print(PurpleDebugLevel level, const char *category,
		const char *args)
{
	if (debug.window && !debug.paused && match_string(category, args))
	{
		int pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(debug.tview));
		GntTextFormatFlags flag = GNT_TEXT_FLAG_NORMAL;
		const char *mdate;
		time_t mtime = time(NULL);
		mdate = purple_utf8_strftime("%H:%M:%S ", localtime(&mtime));
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
				mdate, flag);

		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
				category, GNT_TEXT_FLAG_BOLD);
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
				": ", GNT_TEXT_FLAG_BOLD);

		switch (level)
		{
			case PURPLE_DEBUG_WARNING:
				flag |= GNT_TEXT_FLAG_UNDERLINE;
			case PURPLE_DEBUG_ERROR:
			case PURPLE_DEBUG_FATAL:
				flag |= GNT_TEXT_FLAG_BOLD;
				break;
			default:
				break;
		}

		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview), args, flag);
		if (pos <= 1)
			gnt_text_view_scroll(GNT_TEXT_VIEW(debug.tview), 0);
	}
}

static gboolean
finch_debug_is_enabled(PurpleDebugLevel level, const char *category)
{
	return debug.window && !debug.paused;
}

static PurpleDebugUiOps uiops =
{
	finch_debug_print,
	finch_debug_is_enabled,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleDebugUiOps *finch_debug_get_ui_ops()
{
	return &uiops;
}

static void
reset_debug_win(GntWidget *w, gpointer null)
{
	debug.window = debug.tview = debug.search = NULL;
}

static void
clear_debug_win(GntWidget *w, GntTextView *tv)
{
	gnt_text_view_clear(tv);
}

static void
print_stderr(const char *string)
{
	g_printerr("%s", string);
}

static void
toggle_pause(GntWidget *w, gpointer n)
{
	debug.paused = !debug.paused;
}

/* Xerox */
static void
purple_glib_log_handler(const gchar *domain, GLogLevelFlags flags,
					  const gchar *msg, gpointer user_data)
{
	PurpleDebugLevel level;
	char *new_msg = NULL;
	char *new_domain = NULL;

	if ((flags & G_LOG_LEVEL_ERROR) == G_LOG_LEVEL_ERROR)
		level = PURPLE_DEBUG_ERROR;
	else if ((flags & G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL)
		level = PURPLE_DEBUG_FATAL;
	else if ((flags & G_LOG_LEVEL_WARNING) == G_LOG_LEVEL_WARNING)
		level = PURPLE_DEBUG_WARNING;
	else if ((flags & G_LOG_LEVEL_MESSAGE) == G_LOG_LEVEL_MESSAGE)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_INFO) == G_LOG_LEVEL_INFO)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_DEBUG) == G_LOG_LEVEL_DEBUG)
		level = PURPLE_DEBUG_MISC;
	else
	{
		purple_debug_warning("gntdebug",
				   "Unknown glib logging level in %d\n", flags);

		level = PURPLE_DEBUG_MISC; /* This will never happen. */
	}

	if (msg != NULL)
		new_msg = purple_utf8_try_convert(msg);

	if (domain != NULL)
		new_domain = purple_utf8_try_convert(domain);

	if (new_msg != NULL)
	{
		purple_debug(level, (new_domain != NULL ? new_domain : "g_log"),
				   "%s\n", new_msg);

		g_free(new_msg);
	}

	g_free(new_domain);
}

static void
size_changed_cb(GntWidget *widget, int oldw, int oldh)
{
	int w, h;
	gnt_widget_get_size(widget, &w, &h);
	purple_prefs_set_int(PREF_ROOT "/size/width", w);
	purple_prefs_set_int(PREF_ROOT "/size/height", h);
}

static gboolean
for_real(gpointer entry)
{
	purple_prefs_set_string(PREF_ROOT "/filter", gnt_entry_get_text(entry));
	return FALSE;
}

static void
update_filter_string(GntEntry *entry, gpointer null)
{
	int id = g_timeout_add(1000, for_real, entry);
	g_object_set_data_full(G_OBJECT(entry), "update-filter", GINT_TO_POINTER(id),
					(GDestroyNotify)g_source_remove);
}

static void
file_save(GntFileSel *fs, const char *path, const char *file, GntTextView *tv)
{
	FILE *fp;

	if ((fp = g_fopen(path, "w+")) == NULL) {
		purple_notify_error(NULL, NULL, _("Unable to open file."), NULL);
		return;
	}

	fprintf(fp, "Finch Debug Log : %s\n", purple_date_format_full(NULL));
	fprintf(fp, "%s", tv->string->str);
	fclose(fp);
	gnt_widget_destroy(GNT_WIDGET(fs));
}

static void
file_cancel(GntWidget *w, GntFileSel *fs)
{
	gnt_widget_destroy(GNT_WIDGET(fs));
}

static void
save_debug_win(GntWidget *w, GntTextView *tv)
{
	GntWidget *window = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(window);
	gnt_file_sel_set_current_location(sel, purple_home_dir());
	gnt_file_sel_set_suggested_filename(sel, "debug.txt");
	g_signal_connect(G_OBJECT(sel), "file_selected", G_CALLBACK(file_save), tv);
	g_signal_connect(G_OBJECT(sel->cancel), "activate", G_CALLBACK(file_cancel), sel);
	gnt_widget_show(window);
}

void finch_debug_window_show()
{
	GntWidget *wid, *box, *label;

	debug.paused = FALSE;
	if (debug.window) {
		gnt_window_present(debug.window);
		return;
	}

	debug.window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(debug.window), TRUE);
	gnt_box_set_title(GNT_BOX(debug.window), _("Debug Window"));
	gnt_box_set_pad(GNT_BOX(debug.window), 0);
	gnt_box_set_alignment(GNT_BOX(debug.window), GNT_ALIGN_MID);

	debug.tview = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(debug.window), debug.tview);
	gnt_widget_set_size(debug.tview,
			purple_prefs_get_int(PREF_ROOT "/size/width"),
			purple_prefs_get_int(PREF_ROOT "/size/height"));
	g_signal_connect(G_OBJECT(debug.tview), "size_changed", G_CALLBACK(size_changed_cb), NULL);

	gnt_box_add_widget(GNT_BOX(debug.window), gnt_line_new(FALSE));

	box = gnt_hbox_new(FALSE);
	gnt_box_set_alignment(GNT_BOX(box), GNT_ALIGN_MID);
	gnt_box_set_fill(GNT_BOX(box), FALSE);

	/* XXX: Setting the GROW_Y for the following widgets don't make sense. But right now
	 * it's necessary to make the width of the debug window resizable ... like I said,
	 * it doesn't make sense. The bug is likely in the packing in gntbox.c.
	 */
	wid = gnt_button_new(_("Clear"));
	g_signal_connect(G_OBJECT(wid), "activate", G_CALLBACK(clear_debug_win), debug.tview);
	GNT_WIDGET_SET_FLAGS(wid, GNT_WIDGET_GROW_Y);
	gnt_box_add_widget(GNT_BOX(box), wid);

	wid = gnt_button_new(_("Save"));
	g_signal_connect(G_OBJECT(wid), "activate", G_CALLBACK(save_debug_win), debug.tview);
	GNT_WIDGET_SET_FLAGS(wid, GNT_WIDGET_GROW_Y);
	gnt_box_add_widget(GNT_BOX(box), wid);

	debug.search = gnt_entry_new(purple_prefs_get_string(PREF_ROOT "/filter"));
	label = gnt_label_new(_("Filter:"));
	GNT_WIDGET_UNSET_FLAGS(label, GNT_WIDGET_GROW_X);
	gnt_box_add_widget(GNT_BOX(box), label);
	gnt_box_add_widget(GNT_BOX(box), debug.search);
	g_signal_connect(G_OBJECT(debug.search), "text_changed", G_CALLBACK(update_filter_string), NULL);

	wid = gnt_check_box_new(_("Pause"));
	g_signal_connect(G_OBJECT(wid), "toggled", G_CALLBACK(toggle_pause), NULL);
	GNT_WIDGET_SET_FLAGS(wid, GNT_WIDGET_GROW_Y);
	gnt_box_add_widget(GNT_BOX(box), wid);

	gnt_box_add_widget(GNT_BOX(debug.window), box);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_GROW_Y);

	gnt_widget_set_name(debug.window, "debug-window");

	g_signal_connect(G_OBJECT(debug.window), "destroy", G_CALLBACK(reset_debug_win), NULL);
	gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(debug.tview), debug.window);
	gnt_text_view_attach_pager_widget(GNT_TEXT_VIEW(debug.tview), debug.window);

	gnt_widget_show(debug.window);
}

static gboolean
start_with_debugwin(gpointer null)
{
	finch_debug_window_show();
	return FALSE;
}

void finch_debug_init()
{
/* Xerox */
#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  purple_glib_log_handler, NULL)

	/* Register the glib log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");
	REGISTER_G_LOG_HANDLER("Gnt");
#ifdef USE_GSTREAMER
	REGISTER_G_LOG_HANDLER("GStreamer");
#endif
	REGISTER_G_LOG_HANDLER("stderr");

	g_set_print_handler(print_stderr);   /* Redirect the debug messages to stderr */
	if (!purple_debug_is_enabled())
		handle_fprintf_stderr(FALSE);

	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_string(PREF_ROOT "/filter", "");
	purple_prefs_add_none(PREF_ROOT "/size");
	purple_prefs_add_int(PREF_ROOT "/size/width", 60);
	purple_prefs_add_int(PREF_ROOT "/size/height", 15);

	if (purple_debug_is_enabled())
		g_timeout_add(0, start_with_debugwin, NULL);
}

void finch_debug_uninit()
{
	handle_fprintf_stderr(TRUE);
}

