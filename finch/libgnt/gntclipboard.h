/*
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
/**
 * SECTION:gntclipboard
 * @section_id: libgnt-gntclipboard
 * @short_description: <filename>gntclipboard.h</filename>
 * @title: Clipboard
 */

#ifndef GNT_CLIPBOARD_H
#define GNT_CLIPBOARD_H

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>

#define GNT_TYPE_CLIPBOARD				(gnt_clipboard_get_type())
#define GNT_CLIPBOARD(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_CLIPBOARD, GntClipboard))
#define GNT_CLIPBOARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_CLIPBOARD, GntClipboardClass))
#define GNT_IS_CLIPBOARD(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_CLIPBOARD))
#define GNT_IS_CLIPBOARD_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_CLIPBOARD))
#define GNT_CLIPBOARD_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_CLIPBOARD, GntClipboardClass))

typedef struct _GntClipboard			GntClipboard;
typedef struct _GntClipboardClass		GntClipboardClass;

struct _GntClipboard
{
	GObject inherit;
	gchar *string;
};

struct _GntClipboardClass
{
	GObjectClass parent;

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_clipboard_get_gtype:
 *
 * Returns: GType for GntClipboard.
 */
GType gnt_clipboard_get_type(void);

/**
 * gnt_clipboard_get_string:
 * @clip:  The clipboard.
 *
 * Get the current text from the clipboard.
 *
 * Returns:  A copy of the string in the clipboard. The caller should free the
 *          returned value.
 */
gchar * gnt_clipboard_get_string(GntClipboard *clip);

/**
 * gnt_clipboard_set_string:
 * @clip:     The clipboard.
 * @string:   New string for the clipboard.
 *
 * Set the text in the clipboard.
 */
void gnt_clipboard_set_string(GntClipboard *clip, const gchar *string);

G_END_DECLS

#endif
