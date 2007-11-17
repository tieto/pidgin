/**
 * @file gntwindow.h Window API
 * @ingroup gnt
 */
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

#ifndef GNT_WINDOW_H
#define GNT_WINDOW_H

#include "gnt.h"
#include "gntbox.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenu.h"

#define GNT_TYPE_WINDOW				(gnt_window_get_gtype())
#define GNT_WINDOW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WINDOW, GntWindow))
#define GNT_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WINDOW, GntWindowClass))
#define GNT_IS_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WINDOW))
#define GNT_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WINDOW))
#define GNT_WINDOW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WINDOW, GntWindowClass))

#define GNT_WINDOW_FLAGS(obj)				(GNT_WINDOW(obj)->priv.flags)
#define GNT_WINDOW_SET_FLAGS(obj, flags)		(GNT_WINDOW_FLAGS(obj) |= flags)
#define GNT_WINDOW_UNSET_FLAGS(obj, flags)	(GNT_WINDOW_FLAGS(obj) &= ~(flags))

typedef struct _GntWindow			GntWindow;
typedef struct _GntWindowPriv		GntWindowPriv;
typedef struct _GntWindowClass		GntWindowClass;

typedef enum
{
	GNT_WINDOW_MAXIMIZE_X = 1 << 0,
	GNT_WINDOW_MAXIMIZE_Y = 1 << 1,
} GntWindowFlags;

struct _GntWindow
{
	GntBox parent;
	GntMenu *menu;
	GntWindowPriv *priv;
};

struct _GntWindowClass
{
	GntBoxClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return  GType for GntWindow.
 */
GType gnt_window_get_gtype(void);

#define gnt_vwindow_new(homo) gnt_window_box_new(homo, TRUE)
#define gnt_hwindow_new(homo) gnt_window_box_new(homo, FALSE)

/**
 * Create a new window.
 *
 * @return The newly created window.
 */
GntWidget * gnt_window_new(void);

/**
 * Create a new window.
 *
 * @param homo  @c TRUE if the widgets inside the window should have the same dimensions.
 * @param vert  @c TRUE if the widgets inside the window should be stacked vertically.
 *
 * @return  The newly created window.
 */
GntWidget * gnt_window_box_new(gboolean homo, gboolean vert);

/**
 * Set the menu for a window.
 *
 * @param window  The window.
 * @param menu    The menu for the window.
 */
void gnt_window_set_menu(GntWindow *window, GntMenu *menu);

/**
 * Return the id of a menuitem specified to a keystroke.
 *
 * @param window    The window.
 * @param key       The keystroke.
 *
 * @return The id of the menuitem bound to the keystroke, or @c NULL.
 *
 * @since 2.3.0
 */
const char * gnt_window_get_accel_item(GntWindow *window, const char *key);

/**
 * Maximize a window, either horizontally or vertically, or both.
 *
 * @param window    The window to maximize.
 * @param maximize  The maximization state of the window.
 *
 * @since 2.3.0
 */
void gnt_window_set_maximize(GntWindow *window, GntWindowFlags maximize);

/**
 * Get the maximization state of a window.
 *
 * @param window  The window.
 *
 * @return  The maximization state of the window.
 *
 * @since 2.3.0
 */
GntWindowFlags gnt_window_get_maximize(GntWindow *window);

void gnt_window_workspace_hiding(GntWindow *);
void gnt_window_workspace_showing(GntWindow *);

G_END_DECLS

#endif /* GNT_WINDOW_H */
