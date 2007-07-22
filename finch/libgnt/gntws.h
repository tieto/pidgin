/**
 * @file gntws.h Workspace API
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

#ifndef GNTWS_H
#define GNTWS_H

#include "gntwidget.h"

#include <panel.h>

#define GNT_TYPE_WS				(gnt_ws_get_gtype())
#define GNT_WS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WS, GntWS))
#define GNT_IS_WS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WS))
#define GNT_IS_WS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WS))
#define GNT_WS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WS, GntWSClass))

typedef struct _GntWS GntWS;

struct _GntWS
{
	GntBindable inherit;
	gchar *name;
	GList *list;
	GList *ordered;
	gpointer ui_data;
	
	void *res1;
	void *res2;
	void *res3;
	void *res4;
};

typedef struct _GntWSClass GntWSClass;

struct _GntWSClass
{
	GntBindableClass parent;

	void (*draw_taskbar)(GntWS *ws, gboolean );

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
	void (*res4)(void);
};

G_BEGIN_DECLS

GType gnt_ws_get_gtype(void);

GntWS *gnt_ws_new(const char *name);
void gnt_ws_set_name(GntWS *ws, const gchar *name);
void gnt_ws_add_widget(GntWS *ws, GntWidget *widget);
void gnt_ws_remove_widget(GntWS *ws, GntWidget *widget);
void gnt_ws_widget_hide(GntWidget *widget, GHashTable *nodes);
void gnt_ws_widget_show(GntWidget *widget, GHashTable *nodes);
void gnt_ws_draw_taskbar(GntWS *ws, gboolean reposition);
void gnt_ws_hide(GntWS *ws, GHashTable *table);
void gnt_ws_show(GntWS *ws, GHashTable *table);

const char * gnt_ws_get_name(GntWS *ws);

#endif
