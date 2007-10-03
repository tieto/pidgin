/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include "gntclipboard.h"

enum {
	SIG_CLIPBOARD = 0,
	SIGS
};

static guint signals[SIGS] = { 0 };

static void
gnt_clipboard_class_init(GntClipboardClass *klass)
{
	signals[SIG_CLIPBOARD] = 
		g_signal_new("clipboard_changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);

}

/******************************************************************************
 * GntClipboard API
 *****************************************************************************/

void
gnt_clipboard_set_string(GntClipboard *clipboard, const gchar *string)
{
	g_free(clipboard->string);
	clipboard->string = g_strdup(string);
	g_signal_emit(clipboard, signals[SIG_CLIPBOARD], 0, clipboard->string);
}

gchar *
gnt_clipboard_get_string(GntClipboard *clipboard)
{
	return g_strdup(clipboard->string);
}

static void gnt_clipboard_init(GTypeInstance *instance, gpointer class) {
	GntClipboard *clipboard = GNT_CLIPBOARD(instance);
	clipboard->string = g_strdup("");
}

GType
gnt_clipboard_get_gtype(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(GntClipboardClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_clipboard_class_init,
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntClipboard),
			0,						/* n_preallocs		*/
			gnt_clipboard_init,		/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntClipboard",
									  &info, 0);
	}

	return type;
}
