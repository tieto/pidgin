/**
 * @file gntbutton.h Button API
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

#ifndef GNT_BUTTON_H
#define GNT_BUTTON_H

#include <glib.h>
#include <glib-object.h>
#include "gnt.h"
#include "gntwidget.h"

#define GNT_TYPE_BUTTON				(gnt_button_get_gtype())
#define GNT_BUTTON(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_BUTTON, GntButton))
#define GNT_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_BUTTON, GntButtonClass))
#define GNT_IS_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_BUTTON))
#define GNT_IS_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_BUTTON))
#define GNT_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_BUTTON, GntButtonClass))

typedef struct _GntButton			GntButton;
typedef struct _GntButtonPriv		GntButtonPriv;
typedef struct _GntButtonClass		GntButtonClass;

struct _GntButtonPriv
{
	char *text;
};

struct _GntButton
{
	GntWidget parent;

	GntButtonPriv *priv;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntButtonClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return  GType for Gntbutton
 */
GType gnt_button_get_gtype(void);

/**
 * Create a new button.
 *
 * @param text   The text for the button.
 *
 * @return  The newly created button.
 */
GntWidget * gnt_button_new(const char *text);

G_END_DECLS

#endif /* GNT_BUTTON_H */
