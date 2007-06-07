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

#ifndef GNT_MENU_ITEM_CHECK_H
#define GNT_MENU_ITEM_CHECK_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenuitem.h"

#define GNT_TYPE_MENU_ITEM_CHECK				(gnt_menuitem_check_get_gtype())
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
GType gnt_menuitem_check_get_gtype(void);

/**
 * 
 * @param text
 *
 * @return
 */
GntMenuItem * gnt_menuitem_check_new(const char *text);

/**
 * 
 * @param item
 *
 * @return
 */
gboolean gnt_menuitem_check_get_checked(GntMenuItemCheck *item);

/**
 * 
 * @param item
 * @param set
 */
void gnt_menuitem_check_set_checked(GntMenuItemCheck *item, gboolean set);

G_END_DECLS

#endif /* GNT_MENU_ITEM_CHECK_H */
