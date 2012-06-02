/**
 * @file gntmenuutil.c GNT Menu Utility Functions
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
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

#include <internal.h>
#include "finch.h"

#include "gnt.h"
#include "gntmenu.h"
#include "gntmenuitem.h"
#include "gntmenuutil.h"

static void
context_menu_callback(GntMenuItem *item, gpointer data)
{
	PurpleMenuAction *action = data;
	if (action) {
		void (*callback)(gpointer, gpointer);
		callback = (void (*)(gpointer, gpointer))
			purple_menu_action_get_callback(action);
		if (callback) {
			gpointer ctx = g_object_get_data(G_OBJECT(item), "menuctx");
			callback(ctx, purple_menu_action_get_data(action));
		}
	}
}

void
gnt_append_menu_action(GntMenu *menu, PurpleMenuAction *action, gpointer ctx)
{
	GList *list;
	GntMenuItem *item;

	if (action == NULL)
		return;

	item = gnt_menuitem_new(purple_menu_action_get_label(action));
	if (purple_menu_action_get_callback(action)) {
		gnt_menuitem_set_callback(item, context_menu_callback, action);
		g_object_set_data(G_OBJECT(item), "menuctx", ctx);
	}
	gnt_menu_add_item(menu, item);

	list = purple_menu_action_get_children(action);

	if (list) {
		GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
		gnt_menuitem_set_submenu(item, GNT_MENU(sub));
		for (; list; list = g_list_delete_link(list, list))
			gnt_append_menu_action(GNT_MENU(sub), list->data, action);
		purple_menu_action_set_children(action, NULL);
	}

	g_signal_connect_swapped(G_OBJECT(menu), "destroy",
		G_CALLBACK(purple_menu_action_free), action);
}

