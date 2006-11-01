/**
 * @file gntdebug.c GNT Debug API
 * @ingroup gntui
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
#include <gnt.h>
#include <gntbox.h>
#include <gnttextview.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntline.h>

#include "gntdebug.h"
#include "gntgaim.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#define PREF_ROOT "/gaim/gnt/debug"

static struct
{
	GntWidget *window;
	GntWidget *tview;
	gboolean paused;
	gboolean timestamps;
} debug;

static gboolean
debug_window_kpress_cb(GntWidget *wid, const char *key, GntTextView *view)
{
	if (key[0] == 27)
	{
		if (strcmp(key, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(view, 1);
		else if (strcmp(key, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(view, -1);
		else if (strcmp(key, GNT_KEY_PGDOWN) == 0)
			gnt_text_view_scroll(view, wid->priv.height - 2);
		else if (strcmp(key, GNT_KEY_PGUP) == 0)
			gnt_text_view_scroll(view, -(wid->priv.height - 2));
		else
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

static void
gg_debug_print(GaimDebugLevel level, const char *category,
		const char *args)
{
	if (debug.window && !debug.paused)
	{
		int pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(debug.tview));
		GntTextFormatFlags flag = GNT_TEXT_FLAG_NORMAL;

		if (debug.timestamps) {
			const char *mdate;
			time_t mtime = time(NULL);
			mdate = gaim_utf8_strftime("%H:%M:%S ", localtime(&mtime));
			gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
					mdate, flag);
		}

		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
				category, GNT_TEXT_FLAG_BOLD);
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(debug.tview),
				": ", GNT_TEXT_FLAG_BOLD);

		switch (level)
		{
			case GAIM_DEBUG_WARNING:
				flag |= GNT_TEXT_FLAG_UNDERLINE;
			case GAIM_DEBUG_ERROR:
			case GAIM_DEBUG_FATAL:
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

static GaimDebugUiOps uiops =
{
	gg_debug_print,
};

GaimDebugUiOps *gg_debug_get_ui_ops()
{
	return &uiops;
}

static void
reset_debug_win(GntWidget *w, gpointer null)
{
	debug.window = debug.tview = NULL;
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

static void
toggle_timestamps(GntWidget *w, gpointer n)
{
	debug.timestamps = !debug.timestamps;
	gaim_prefs_set_bool("/core/debug/timestamps", debug.timestamps);
}

/* Xerox */
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
		gaim_debug_warning("gntdebug",
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

	g_free(new_domain);
}

static void
size_changed_cb(GntWidget *widget, int oldw, int oldh)
{
	int w, h;
	gnt_widget_get_size(widget, &w, &h);
	gaim_prefs_set_int(PREF_ROOT "/size/width", w);
	gaim_prefs_set_int(PREF_ROOT "/size/height", h);
}

void gg_debug_window_show()
{
	debug.paused = FALSE;
	debug.timestamps = gaim_prefs_get_bool("/core/debug/timestamps");
	if (debug.window == NULL)
	{
		GntWidget *wid, *box;
		debug.window = gnt_vbox_new(FALSE);
		gnt_box_set_toplevel(GNT_BOX(debug.window), TRUE);
		gnt_box_set_title(GNT_BOX(debug.window), _("Debug Window"));
		gnt_box_set_pad(GNT_BOX(debug.window), 0);
		gnt_box_set_alignment(GNT_BOX(debug.window), GNT_ALIGN_MID);

		debug.tview = gnt_text_view_new();
		gnt_box_add_widget(GNT_BOX(debug.window), debug.tview);
		gnt_widget_set_size(debug.tview,
				gaim_prefs_get_int(PREF_ROOT "/size/width"),
				gaim_prefs_get_int(PREF_ROOT "/size/height"));
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

		wid = gnt_check_box_new(_("Pause"));
		g_signal_connect(G_OBJECT(wid), "toggled", G_CALLBACK(toggle_pause), NULL);
		GNT_WIDGET_SET_FLAGS(wid, GNT_WIDGET_GROW_Y);
		gnt_box_add_widget(GNT_BOX(box), wid);

		wid = gnt_check_box_new(_("Timestamps"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(wid), debug.timestamps);
		g_signal_connect(G_OBJECT(wid), "toggled", G_CALLBACK(toggle_timestamps), NULL);
		GNT_WIDGET_SET_FLAGS(wid, GNT_WIDGET_GROW_Y);
		gnt_box_add_widget(GNT_BOX(box), wid);

		gnt_box_add_widget(GNT_BOX(debug.window), box);
		GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_GROW_Y);

		gnt_widget_set_name(debug.window, "debug-window");

		g_signal_connect(G_OBJECT(debug.window), "destroy", G_CALLBACK(reset_debug_win), NULL);
		g_signal_connect(G_OBJECT(debug.window), "key_pressed", G_CALLBACK(debug_window_kpress_cb), debug.tview);
	}

	gnt_widget_show(debug.window);
}

static gboolean
start_with_debugwin(gpointer null)
{
	gg_debug_window_show();
	return FALSE;
}

void gg_debug_init()
{
/* Xerox */
#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  gaim_glib_log_handler, NULL)

	/* Register the glib log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");

	g_set_print_handler(print_stderr);   /* Redirect the debug messages to stderr */

	gaim_prefs_add_none(PREF_ROOT);
	gaim_prefs_add_none(PREF_ROOT "/size");
	gaim_prefs_add_int(PREF_ROOT "/size/width", 60);
	gaim_prefs_add_int(PREF_ROOT "/size/height", 15);

	if (gaim_debug_is_enabled())
		g_timeout_add(0, start_with_debugwin, NULL);
}

void gg_debug_uninit()
{
}

