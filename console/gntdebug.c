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

#include "gntdebug.h"
#include "gntgaim.h"

#include <stdio.h>
#include <string.h>

static struct
{
	GntWidget *window;
	GntWidget *tview;
} debug;

static gboolean
debug_window_kpress_cb(GntWidget *wid, const char *key, GntTextView *view)
{
	if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(view, 1);
		else if (strcmp(key+1, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(view, -1);
		else if (strcmp(key+1, GNT_KEY_PGDOWN) == 0)
			gnt_text_view_scroll(view, wid->priv.height - 2);
		else if (strcmp(key+1, GNT_KEY_PGUP) == 0)
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
	if (debug.window)
	{
		GntTextFormatFlags flag = GNT_TEXT_FLAG_NORMAL;

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
print_stderr(const char *string)
{
	g_printerr("%s", string);
}

void gg_debug_window_show()
{
	if (debug.window == NULL)
	{
		debug.window = gnt_vbox_new(FALSE);
		gnt_box_set_toplevel(GNT_BOX(debug.window), TRUE);
		gnt_box_set_title(GNT_BOX(debug.window), _("Debug Window"));

		debug.tview = gnt_text_view_new();
		gnt_box_add_widget(GNT_BOX(debug.window), debug.tview);

		/* XXX: Add checkboxes/buttons for Clear, Pause, Timestamps */

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
	g_set_print_handler(print_stderr);   /* Redirect the debug messages to stderr */
	if (gaim_debug_is_enabled())
		g_timeout_add(0, start_with_debugwin, NULL);
}

void gg_debug_uninit()
{
}

