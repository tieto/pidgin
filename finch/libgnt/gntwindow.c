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

#include "gntstyle.h"
#include "gntwindow.h"

#include <string.h>

enum
{
	SIGS = 1,
};

static GntBoxClass *parent_class = NULL;

static void (*org_destroy)(GntWidget *widget);

static gboolean
show_menu(GntBindable *bind, GList *null)
{
	GntWindow *win = GNT_WINDOW(bind);
	if (win->menu) {
		gnt_screen_menu_show(win->menu);
		return TRUE;
	}
	return FALSE;
}

static void
gnt_window_destroy(GntWidget *widget)
{
	GntWindow *window = GNT_WINDOW(widget);
	if (window->menu)
		gnt_widget_destroy(GNT_WIDGET(window->menu));
	org_destroy(widget);
}

static void
gnt_window_class_init(GntWindowClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	GntWidgetClass *wid_class = GNT_WIDGET_CLASS(klass);
	parent_class = GNT_BOX_CLASS(klass);

	org_destroy = wid_class->destroy;
	wid_class->destroy = gnt_window_destroy;

	gnt_bindable_class_register_action(bindable, "show-menu", show_menu,
				GNT_KEY_CTRL_O, NULL);
	gnt_bindable_register_binding(bindable, "show-menu", GNT_KEY_F10, NULL);
	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), bindable);

	GNTDEBUG;
}

static void
gnt_window_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	GNTDEBUG;
}

/******************************************************************************
 * GntWindow API
 *****************************************************************************/
GType
gnt_window_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntWindowClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_window_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntWindow),
			0,						/* n_preallocs		*/
			gnt_window_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_BOX,
									  "GntWindow",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_window_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_WINDOW, NULL);

	return widget;
}

GntWidget *gnt_window_box_new(gboolean homo, gboolean vert)
{
	GntWidget *wid = gnt_window_new();
	GntBox *box = GNT_BOX(wid);

	box->homogeneous = homo;
	box->vertical = vert;
	box->alignment = vert ? GNT_ALIGN_LEFT : GNT_ALIGN_MID;

	return wid;
}

void gnt_window_set_menu(GntWindow *window, GntMenu *menu)
{
	/* If a menu already existed, then destroy that first. */
	if (window->menu)
		gnt_widget_destroy(GNT_WIDGET(window->menu));
	window->menu = menu;
}

