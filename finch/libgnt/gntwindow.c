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

#include "gntinternal.h"
#include "gntstyle.h"
#include "gntwindow.h"

#include <string.h>

struct _GntWindowPriv
{
	GHashTable *accels;   /* key => menuitem-id */
	GntWindowFlags flags;
};

enum
{
	SIG_WORKSPACE_HIDE,
	SIG_WORKSPACE_SHOW,
	SIGS,
};

static guint signals[SIGS] = { 0 };

static GntBoxClass *parent_class = NULL;

static void (*org_destroy)(GntWidget *widget);

static gboolean
show_menu(GntBindable *bind, GList *null)
{
	GntWindow *win = GNT_WINDOW(bind);
	if (win->menu) {
		GntMenu *menu = win->menu;

		gnt_screen_menu_show(menu);
		if (menu->type == GNT_MENU_TOPLEVEL) {
			GntMenuItem *item;
			item = g_list_nth_data(menu->list, menu->selected);
			if (item && gnt_menuitem_get_submenu(item)) {
				gnt_widget_activate(GNT_WIDGET(menu));
			}
		}
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
	if (window->priv) {
		if (window->priv->accels)
			g_hash_table_destroy(window->priv->accels);
		g_free(window->priv);
	}
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

	signals[SIG_WORKSPACE_HIDE] =
		g_signal_new("workspace-hidden",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

	signals[SIG_WORKSPACE_SHOW] =
		g_signal_new("workspace-shown",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

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
	GntWindow *win = GNT_WINDOW(widget);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	win->priv = g_new0(GntWindowPriv, 1);
	win->priv->accels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
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

void
gnt_window_workspace_hiding(GntWindow *window)
{
	if (window->menu)
		gnt_widget_hide(GNT_WIDGET(window->menu));
	g_signal_emit(window, signals[SIG_WORKSPACE_HIDE], 0);
}

void
gnt_window_workspace_showing(GntWindow *window)
{
	g_signal_emit(window, signals[SIG_WORKSPACE_SHOW], 0);
}

void gnt_window_set_menu(GntWindow *window, GntMenu *menu)
{
	/* If a menu already existed, then destroy that first. */
	const char *name = gnt_widget_get_name(GNT_WIDGET(window));
	if (window->menu)
		gnt_widget_destroy(GNT_WIDGET(window->menu));
	window->menu = menu;
	if (name && window->priv) {
		if (!gnt_style_read_menu_accels(name, window->priv->accels)) {
			g_hash_table_destroy(window->priv->accels);
			window->priv->accels = NULL;
		}
	}
}

const char * gnt_window_get_accel_item(GntWindow *window, const char *key)
{
	if (window->priv->accels)
		return g_hash_table_lookup(window->priv->accels, key);
	return NULL;
}

void gnt_window_set_maximize(GntWindow *window, GntWindowFlags maximize)
{
	if (maximize & GNT_WINDOW_MAXIMIZE_X)
		window->priv->flags |= GNT_WINDOW_MAXIMIZE_X;
	else
		window->priv->flags &= ~GNT_WINDOW_MAXIMIZE_X;

	if (maximize & GNT_WINDOW_MAXIMIZE_Y)
		window->priv->flags |= GNT_WINDOW_MAXIMIZE_Y;
	else
		window->priv->flags &= ~GNT_WINDOW_MAXIMIZE_Y;
}

GntWindowFlags gnt_window_get_maximize(GntWindow *window)
{
	return (window->priv->flags & (GNT_WINDOW_MAXIMIZE_X | GNT_WINDOW_MAXIMIZE_Y));
}

