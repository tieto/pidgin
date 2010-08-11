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

#include "gntline.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;

static void
gnt_line_draw(GntWidget *widget)
{
	GntLine *line = GNT_LINE(widget);
	if (line->vertical)
		mvwvline(widget->window, 1, 0, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL),
				widget->priv.height - 2);
	else
		mvwhline(widget->window, 0, 1, ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL),
				widget->priv.width - 2);
}

static void
gnt_line_size_request(GntWidget *widget)
{
	if (GNT_LINE(widget)->vertical)
	{
		widget->priv.width = 1;
		widget->priv.height = 5;
	}
	else
	{
		widget->priv.width = 5;
		widget->priv.height = 1;
	}
}

static void
gnt_line_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
}

static void
gnt_line_class_init(GntLineClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->draw = gnt_line_draw;
	parent_class->map = gnt_line_map;
	parent_class->size_request = gnt_line_size_request;

	GNTDEBUG;
}

static void
gnt_line_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW | GNT_WIDGET_NO_BORDER);
	widget->priv.minw = 1;
	widget->priv.minh = 1;
	GNTDEBUG;
}

/******************************************************************************
 * GntLine API
 *****************************************************************************/
GType
gnt_line_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntLineClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_line_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntLine),
			0,						/* n_preallocs		*/
			gnt_line_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntLine",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_line_new(gboolean vertical)
{
	GntWidget *widget = g_object_new(GNT_TYPE_LINE, NULL);
	GntLine *line = GNT_LINE(widget);

	line->vertical = vertical;

	if (vertical)
	{
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_Y);
	}
	else
	{
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X);
	}

	return widget;
}

