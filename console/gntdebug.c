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
		/* XXX: This doesn't seem to always work */
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
		gnt_text_view_next_line(GNT_TEXT_VIEW(debug.tview));
		gnt_text_view_scroll(GNT_TEXT_VIEW(debug.tview), 0);

		g_signal_connect(G_OBJECT(debug.window), "key_pressed", G_CALLBACK(debug_window_kpress_cb), debug.tview);
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

void gg_debug_window_show()
{
	if (debug.window == NULL)
	{
		debug.window = gnt_vbox_new(FALSE);
		gnt_box_set_toplevel(GNT_BOX(debug.window), TRUE);
		gnt_box_set_title(GNT_BOX(debug.window), _("Debug Window"));

		debug.tview = gnt_text_view_new();
		gnt_box_add_widget(GNT_BOX(debug.window), debug.tview);

		g_signal_connect(G_OBJECT(debug.window), "destroy", G_CALLBACK(reset_debug_win), NULL);
	}

	gnt_widget_show(debug.window);
}

void gg_debug_init()
{
	if (gaim_debug_is_enabled())
		gg_debug_window_show();
}

void gg_debug_uninit()
{
}

