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

#include "gntmenuitemcheck.h"

static GntMenuItemClass *parent_class = NULL;

static void
gnt_menuitem_check_class_init(GntMenuItemCheckClass *klass)
{
	parent_class = GNT_MENU_ITEM_CLASS(klass);

	GNTDEBUG;
}

static void
gnt_menuitem_check_init(GTypeInstance *instance, gpointer class)
{
	GNTDEBUG;
}

/******************************************************************************
 * GntMenuItemCheck API
 *****************************************************************************/
GType
gnt_menuitem_check_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntMenuItemCheckClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_menuitem_check_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntMenuItemCheck),
			0,						/* n_preallocs		*/
			gnt_menuitem_check_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_MENU_ITEM,
									  "GntMenuItemCheck",
									  &info, 0);
	}

	return type;
}

GntMenuItem *gnt_menuitem_check_new(const char *text)
{
	GntMenuItem *item = g_object_new(GNT_TYPE_MENU_ITEM_CHECK, NULL);
	GntMenuItem *menuitem = GNT_MENU_ITEM(item);

	menuitem->text = g_strdup(text);
	return item;
}

gboolean gnt_menuitem_check_get_checked(GntMenuItemCheck *item)
{
		return item->checked;
}

void gnt_menuitem_check_set_checked(GntMenuItemCheck *item, gboolean set)
{
		item->checked = set;
}

