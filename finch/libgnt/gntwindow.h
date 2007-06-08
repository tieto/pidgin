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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

struct _GntWindow
{
	GntBox parent;
	GntMenu *menu;
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
 * 
 *
 * @return
 */
GType gnt_window_get_gtype(void);

#define gnt_vwindow_new(homo) gnt_window_box_new(homo, TRUE)
#define gnt_hwindow_new(homo) gnt_window_box_new(homo, FALSE)

/**
 * 
 *
 * @return
 */
GntWidget * gnt_window_new(void);

/**
 * 
 * @param homo
 * @param vert
 *
 * @return
 */
GntWidget * gnt_window_box_new(gboolean homo, gboolean vert);

/**
 * 
 * @param window
 * @param menu
 */
void gnt_window_set_menu(GntWindow *window, GntMenu *menu);

void gnt_window_workspace_hiding(GntWindow *);
void gnt_window_workspace_showing(GntWindow *);

G_END_DECLS

#endif /* GNT_WINDOW_H */
