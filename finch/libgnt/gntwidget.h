/**
 * @file gntwidget.h Widget API
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GNT_WIDGET_H
#define GNT_WIDGET_H

#include <stdio.h>
#include <glib.h>
#include <ncurses.h>

#include "gntbindable.h"

#define GNT_TYPE_WIDGET				(gnt_widget_get_gtype())
#define GNT_WIDGET(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WIDGET, GntWidget))
#define GNT_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WIDGET, GntWidgetClass))
#define GNT_IS_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WIDGET))
#define GNT_IS_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WIDGET))
#define GNT_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WIDGET, GntWidgetClass))

#define GNT_WIDGET_FLAGS(obj)				(GNT_WIDGET(obj)->priv.flags)
#define GNT_WIDGET_SET_FLAGS(obj, flags)		(GNT_WIDGET_FLAGS(obj) |= flags)
#define GNT_WIDGET_UNSET_FLAGS(obj, flags)	(GNT_WIDGET_FLAGS(obj) &= ~(flags))
#define GNT_WIDGET_IS_FLAG_SET(obj, flags)	(GNT_WIDGET_FLAGS(obj) & (flags))

typedef struct _GntWidget			GntWidget;
typedef struct _GntWidgetPriv		GntWidgetPriv;
typedef struct _GntWidgetClass		GntWidgetClass;

typedef enum _GntWidgetFlags
{
	GNT_WIDGET_DESTROYING     = 1 << 0,
	GNT_WIDGET_CAN_TAKE_FOCUS = 1 << 1,
	GNT_WIDGET_MAPPED         = 1 << 2,
	/* XXX: Need to set the following two as properties, and setup a callback whenever these
	 * get chnaged. */
	GNT_WIDGET_NO_BORDER      = 1 << 3,
	GNT_WIDGET_NO_SHADOW      = 1 << 4,
	GNT_WIDGET_HAS_FOCUS      = 1 << 5,
	GNT_WIDGET_DRAWING        = 1 << 6,
	GNT_WIDGET_URGENT         = 1 << 7,
	GNT_WIDGET_GROW_X         = 1 << 8,
	GNT_WIDGET_GROW_Y         = 1 << 9,
	GNT_WIDGET_INVISIBLE      = 1 << 10,
	GNT_WIDGET_TRANSIENT      = 1 << 11,
	GNT_WIDGET_DISABLE_ACTIONS = 1 << 12,
} GntWidgetFlags;

/* XXX: This will probably move elsewhere */
typedef enum _GntMouseEvent
{
	GNT_LEFT_MOUSE_DOWN = 1,
	GNT_RIGHT_MOUSE_DOWN,
	GNT_MIDDLE_MOUSE_DOWN,
	GNT_MOUSE_UP,
	GNT_MOUSE_SCROLL_UP,
	GNT_MOUSE_SCROLL_DOWN
} GntMouseEvent;

/* XXX: I'll have to ask grim what he's using this for in guifications. */
typedef enum _GntParamFlags
{
	GNT_PARAM_SERIALIZABLE	= 1 << G_PARAM_USER_SHIFT
} GntParamFlags;

struct _GntWidgetPriv
{
	int x, y;
	int width, height;
	GntWidgetFlags flags;
	char *name;

	int minw, minh;    /* Minimum size for the widget */
};

struct _GntWidget
{
	GntBindable inherit;

	GntWidget *parent;

	GntWidgetPriv priv;
	WINDOW *window;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GntWidgetClass
{
	GntBindableClass parent;

	void (*map)(GntWidget *obj);
	void (*show)(GntWidget *obj);		/* This will call draw() and take focus (if it can take focus) */
	void (*destroy)(GntWidget *obj);
	void (*draw)(GntWidget *obj);		/* This will draw the widget */
	void (*hide)(GntWidget *obj);
	void (*expose)(GntWidget *widget, int x, int y, int width, int height);
	void (*gained_focus)(GntWidget *widget);
	void (*lost_focus)(GntWidget *widget);

	void (*size_request)(GntWidget *widget);
	gboolean (*confirm_size)(GntWidget *widget, int x, int y);
	void (*size_changed)(GntWidget *widget, int w, int h);
	void (*set_position)(GntWidget *widget, int x, int y);
	gboolean (*key_pressed)(GntWidget *widget, const char *key);
	void (*activate)(GntWidget *widget);
	gboolean (*clicked)(GntWidget *widget, GntMouseEvent event, int x, int y);

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
GType gnt_widget_get_gtype(void);

/**
 * 
 * @param widget
 */
void gnt_widget_destroy(GntWidget *widget);

/**
 * 
 * @param widget
 */
void gnt_widget_show(GntWidget *widget);

/**
 * 
 * @param widget
 */
void gnt_widget_draw(GntWidget *widget);

/**
 * 
 * @param widget
 * @param x
 * @param y
 * @param width
 * @param height
 */
void gnt_widget_expose(GntWidget *widget, int x, int y, int width, int height);

/**
 * 
 * @param widget
 */
void gnt_widget_hide(GntWidget *widget);

/**
 * 
 * @param widget
 * @param x
 * @param y
 */
void gnt_widget_get_position(GntWidget *widget, int *x, int *y);

/**
 * 
 * @param widget
 * @param x
 * @param y
 */
void gnt_widget_set_position(GntWidget *widget, int x, int y);

/**
 * 
 * @param widget
 */
void gnt_widget_size_request(GntWidget *widget);

/**
 * 
 * @param widget
 * @param width
 * @param height
 */
void gnt_widget_get_size(GntWidget *widget, int *width, int *height);

/**
 * 
 * @param widget
 * @param width
 * @param height
 *
 * @return
 */
gboolean gnt_widget_set_size(GntWidget *widget, int width, int height);

/**
 * 
 * @param widget
 * @param width
 * @param height
 *
 * @return
 */
gboolean gnt_widget_confirm_size(GntWidget *widget, int width, int height);

/**
 * 
 * @param widget
 * @param keys
 *
 * @return
 */
gboolean gnt_widget_key_pressed(GntWidget *widget, const char *keys);

/**
 * 
 * @param widget
 * @param event
 * @param x
 * @param y
 *
 * @return
 */
gboolean gnt_widget_clicked(GntWidget *widget, GntMouseEvent event, int x, int y);

/**
 * 
 * @param widget
 * @param set
 *
 * @return
 */
gboolean gnt_widget_set_focus(GntWidget *widget, gboolean set);

/**
 * 
 * @param widget
 */
void gnt_widget_activate(GntWidget *widget);

/**
 * 
 * @param widget
 * @param name
 */
void gnt_widget_set_name(GntWidget *widget, const char *name);

const char *gnt_widget_get_name(GntWidget *widget);

/* Widget-subclasses should call this from the draw-callback.
 * Applications should just call gnt_widget_draw instead of this. */
/**
 * 
 * @param widget
 */
void gnt_widget_queue_update(GntWidget *widget);

/**
 * 
 * @param widget
 * @param set
 */
void gnt_widget_set_take_focus(GntWidget *widget, gboolean set);

/**
 * 
 * @param widget
 * @param set
 */
void gnt_widget_set_visible(GntWidget *widget, gboolean set);

/**
 * 
 * @param widget
 *
 * @return
 */
gboolean gnt_widget_has_shadow(GntWidget *widget);

G_END_DECLS

#endif /* GNT_WIDGET_H */
