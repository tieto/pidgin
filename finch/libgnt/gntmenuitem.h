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
/**
 * SECTION:gntmenuitem
 * @section_id: libgnt-gntmenuitem
 * @short_description: <filename>gntmenuitem.h</filename>
 * @title: Menuitem
 */

#ifndef GNT_MENUITEM_H
#define GNT_MENUITEM_H

#include <glib.h>
#include <glib-object.h>

#define GNT_TYPE_MENU_ITEM				(gnt_menuitem_get_type())
#define GNT_MENU_ITEM(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENU_ITEM, GntMenuItem))
#define GNT_MENU_ITEM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENU_ITEM, GntMenuItemClass))
#define GNT_IS_MENU_ITEM(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENU_ITEM))
#define GNT_IS_MENU_ITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENU_ITEM))
#define GNT_MENU_ITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENU_ITEM, GntMenuItemClass))

#define GNT_MENU_ITEM_FLAGS(obj)				(GNT_MENU_ITEM(obj)->priv.flags)
#define GNT_MENU_ITEM_SET_FLAGS(obj, flags)		(GNT_MENU_ITEM_FLAGS(obj) |= flags)
#define GNT_MENU_ITEM_UNSET_FLAGS(obj, flags)	(GNT_MENU_ITEM_FLAGS(obj) &= ~(flags))

typedef struct _GntMenuItem			GntMenuItem;
typedef struct _GntMenuItemPriv		GntMenuItemPriv;
typedef struct _GntMenuItemClass		GntMenuItemClass;

#include "gntmenu.h"

struct _GntMenuItemPriv
{
	/* These will be used to determine the position of the submenu */
	int x;
	int y;
	char trigger;
	char *id;
};

typedef void (*GntMenuItemCallback)(GntMenuItem *item, gpointer data);

struct _GntMenuItem
{
	GObject parent;
	GntMenuItemPriv priv;

	char *text;

	/* A GntMenuItem can have a callback associated with it.
	 * The callback will be activated whenever the suer selects it and presses enter (or clicks).
	 * However, if the GntMenuItem has some child, then the callback and callbackdata will be ignored. */
	gpointer callbackdata;
	GntMenuItemCallback callback;

	GntMenu *submenu;
};

struct _GntMenuItemClass
{
	GObjectClass parent;

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_menuitem_get_type:
 *
 * Returns: GType for GntMenuItem.
 */
GType gnt_menuitem_get_type(void);

/**
 * gnt_menuitem_new:
 * @text:   Label for the menuitem.
 *
 * Create a new menuitem.
 *
 * Returns:  The newly created menuitem.
 */
GntMenuItem * gnt_menuitem_new(const char *text);

/**
 * gnt_menuitem_set_callback:
 * @item:       The menuitem.
 * @callback:   The callback function.
 * @data:       Data to send to the callback function.
 *
 * Set a callback function for a menuitem.
 */
void gnt_menuitem_set_callback(GntMenuItem *item, GntMenuItemCallback callback, gpointer data);

/**
 * gnt_menuitem_set_submenu:
 * @item:  The menuitem.
 * @menu:  The submenu.
 *
 * Set a submenu for a menuitem. A menuitem with a submenu cannot have a callback.
 */
void gnt_menuitem_set_submenu(GntMenuItem *item, GntMenu *menu);

/**
 * gnt_menuitem_get_submenu:
 * @item:   The menuitem.
 *
 * Get the submenu for a menuitem.
 *
 * Returns:  The submenu, or %NULL.
 *
 * Since: 2.3.0
 */
GntMenu *gnt_menuitem_get_submenu(GntMenuItem *item);

/**
 * gnt_menuitem_set_trigger:
 * @item:     The menuitem
 * @trigger:  The key that will trigger the item when the parent manu is visible
 *
 * Set a trigger key for the item.
 */
void gnt_menuitem_set_trigger(GntMenuItem *item, char trigger);

/**
 * gnt_menuitem_get_trigger:
 * @item:   The menuitem
 *
 * Get the trigger key for a menuitem.
 *
 * See gnt_menuitem_set_trigger().
 *
 * Returns: The trigger key for the menuitem.
 */
char gnt_menuitem_get_trigger(GntMenuItem *item);

/**
 * gnt_menuitem_set_id:
 * @item:   The menuitem.
 * @id:     The ID for the menuitem.
 *
 * Set an ID for the menuitem.
 *
 * Since: 2.3.0
 */
void gnt_menuitem_set_id(GntMenuItem *item, const char *id);

/**
 * gnt_menuitem_get_id:
 * @item:   The menuitem.
 *
 * Get the ID of the menuitem.
 *
 * Returns:  The ID for the menuitem.
 *
 * Since: 2.3.0
 */
const char * gnt_menuitem_get_id(GntMenuItem *item);

/**
 * gnt_menuitem_activate:
 * @item:   The menuitem.
 *
 * Activate a menuitem.
 * Activating the menuitem will first trigger the 'activate' signal for the
 * menuitem. Then the callback for the menuitem is triggered, if there is one.
 *
 * Returns:  Whether the callback for the menuitem was called.
 *
 * Since: 2.3.0
 */
gboolean gnt_menuitem_activate(GntMenuItem *item);

G_END_DECLS

#endif /* GNT_MENUITEM_H */
