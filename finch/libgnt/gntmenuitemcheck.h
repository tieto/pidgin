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
 * SECTION:gntmenuitemcheck
 * @section_id: libgnt-gntmenuitemcheck
 * @short_description: <filename>gntmenuitemcheck.h</filename>
 * @title: Check Menuitem
 */

#ifndef GNT_MENU_ITEM_CHECK_H
#define GNT_MENU_ITEM_CHECK_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenuitem.h"

#define GNT_TYPE_MENU_ITEM_CHECK				(gnt_menuitem_check_get_type())
#define GNT_MENU_ITEM_CHECK(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheck))
#define GNT_MENU_ITEM_CHECK_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheckClass))
#define GNT_IS_MENU_ITEM_CHECK(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENU_ITEM_CHECK))
#define GNT_IS_MENU_ITEM_CHECK_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENU_ITEM_CHECK))
#define GNT_MENU_ITEM_CHECK_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheckClass))

#define GNT_MENU_ITEM_CHECK_FLAGS(obj)				(GNT_MENU_ITEM_CHECK(obj)->priv.flags)
#define GNT_MENU_ITEM_CHECK_SET_FLAGS(obj, flags)		(GNT_MENU_ITEM_CHECK_FLAGS(obj) |= flags)
#define GNT_MENU_ITEM_CHECK_UNSET_FLAGS(obj, flags)	(GNT_MENU_ITEM_CHECK_FLAGS(obj) &= ~(flags))

typedef struct _GntMenuItemCheck			GntMenuItemCheck;
typedef struct _GntMenuItemCheckPriv		GntMenuItemCheckPriv;
typedef struct _GntMenuItemCheckClass		GntMenuItemCheckClass;

struct _GntMenuItemCheck
{
	GntMenuItem parent;
	gboolean checked;
};

struct _GntMenuItemCheckClass
{
	GntMenuItemClass parent;

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_menuitem_check_get_type:
 *
 * Returns: GType for GntMenuItemCheck.
 */
GType gnt_menuitem_check_get_type(void);

/**
 * gnt_menuitem_check_new:
 * @text:  The text for the menuitem.
 *
 * Create a new menuitem.
 *
 * Returns:  The newly created menuitem.
 */
GntMenuItem * gnt_menuitem_check_new(const char *text);

/**
 * gnt_menuitem_check_get_checked:
 * @item:  The menuitem.
 *
 * Check whether the menuitem is checked or not.
 *
 * Returns: %TRUE if the item is checked, %FALSE otherwise.
 */
gboolean gnt_menuitem_check_get_checked(GntMenuItemCheck *item);

/**
 * gnt_menuitem_check_set_checked:
 * @item:  The menuitem.
 * @set:   %TRUE if the item should be checked, %FALSE otherwise.
 *
 * Set whether the menuitem is checked or not.
 */
void gnt_menuitem_check_set_checked(GntMenuItemCheck *item, gboolean set);

G_END_DECLS

#endif /* GNT_MENU_ITEM_CHECK_H */
