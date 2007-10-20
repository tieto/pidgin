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

#include "gntmenu.h"
#include "gntmenuitem.h"

enum
{
	SIG_ACTIVATE,
	SIGS
};
static guint signals[SIGS] = { 0 };

static GObjectClass *parent_class = NULL;

static void
gnt_menuitem_destroy(GObject *obj)
{
	GntMenuItem *item = GNT_MENU_ITEM(obj);
	g_free(item->text);
	item->text = NULL;
	if (item->submenu)
		gnt_widget_destroy(GNT_WIDGET(item->submenu));
	g_free(item->priv.id);
	parent_class->dispose(obj);
}

static void
gnt_menuitem_class_init(GntMenuItemClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = gnt_menuitem_destroy;

	signals[SIG_ACTIVATE] =
		g_signal_new("activate",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
}

static void
gnt_menuitem_init(GTypeInstance *instance, gpointer klass)
{
}

/******************************************************************************
 * GntMenuItem API
 *****************************************************************************/
GType
gnt_menuitem_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntMenuItemClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_menuitem_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntMenuItem),
			0,						/* n_preallocs		*/
			gnt_menuitem_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntMenuItem",
									  &info, 0);
	}

	return type;
}

GntMenuItem *gnt_menuitem_new(const char *text)
{
	GObject *item = g_object_new(GNT_TYPE_MENU_ITEM, NULL);
	GntMenuItem *menuitem = GNT_MENU_ITEM(item);

	menuitem->text = g_strdup(text);

	return menuitem;
}

void gnt_menuitem_set_callback(GntMenuItem *item, GntMenuItemCallback callback, gpointer data)
{
	item->callback = callback;
	item->callbackdata = data;
}

void gnt_menuitem_set_submenu(GntMenuItem *item, GntMenu *menu)
{
	if (item->submenu)
		gnt_widget_destroy(GNT_WIDGET(item->submenu));
	item->submenu = menu;
}

GntMenu *gnt_menuitem_get_submenu(GntMenuItem *item)
{
	return item->submenu;
}

void gnt_menuitem_set_trigger(GntMenuItem *item, char trigger)
{
	item->priv.trigger = trigger;
}

char gnt_menuitem_get_trigger(GntMenuItem *item)
{
	return item->priv.trigger;
}

void gnt_menuitem_set_id(GntMenuItem *item, const char *id)
{
	g_free(item->priv.id);
	item->priv.id = g_strdup(id);
}

const char * gnt_menuitem_get_id(GntMenuItem *item)
{
	return item->priv.id;
}

gboolean gnt_menuitem_activate(GntMenuItem *item)
{
	g_signal_emit(item, signals[SIG_ACTIVATE], 0);
	if (item->callback) {
		item->callback(item, item->callbackdata);
		return TRUE;
	}
	return FALSE;
}

