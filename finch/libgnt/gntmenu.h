/**
 * @file gntmenu.h Menu API
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_MENU_H
#define GNT_MENU_H

#include "gnttree.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_MENU				(gnt_menu_get_gtype())
#define GNT_MENU(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENU, GntMenu))
#define GNT_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENU, GntMenuClass))
#define GNT_IS_MENU(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENU))
#define GNT_IS_MENU_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENU))
#define GNT_MENU_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENU, GntMenuClass))

#define GNT_MENU_FLAGS(obj)				(GNT_MENU(obj)->priv.flags)
#define GNT_MENU_SET_FLAGS(obj, flags)		(GNT_MENU_FLAGS(obj) |= flags)
#define GNT_MENU_UNSET_FLAGS(obj, flags)	(GNT_MENU_FLAGS(obj) &= ~(flags))

typedef struct _GntMenu			GntMenu;
typedef struct _GntMenuPriv		GntMenuPriv;
typedef struct _GntMenuClass		GntMenuClass;

#include "gntmenuitem.h"

/**
 * A toplevel-menu is displayed at the top of the screen, and it spans accross
 * the entire width of the screen.
 * A popup-menu could be displayed, for example, as a context menu for widgets.
 */
typedef enum
{
	GNT_MENU_TOPLEVEL = 1,  /* Menu for a toplevel window */
	GNT_MENU_POPUP,         /* A popup menu */
} GntMenuType;

struct _GntMenu
{
	GntTree parent;
	GntMenuType type;

	GList *list;
	int selected;

	/* This will keep track of its immediate submenu which is visible so that
	 * keystrokes can be passed to it. */
	GntMenu *submenu;
	GntMenu *parentmenu;
};

struct _GntMenuClass
{
	GntTreeClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * @return  The GType for GntMenu.
 */
GType gnt_menu_get_gtype(void);

/**
 * Create a new menu.
 *
 * @param type  The type of the menu, whether it's a toplevel menu or a popup menu.
 *
 * @return  The newly created menu.
 */
GntWidget * gnt_menu_new(GntMenuType type);

/**
 * Add an item to the menu.
 *
 * @param menu   The menu.
 * @param item   The item to add to the menu.
 */
void gnt_menu_add_item(GntMenu *menu, GntMenuItem *item);

/**
 * Return the GntMenuItem with the given ID.
 *
 * @param menu   The menu.
 * @param id     The ID for an item.
 *
 * @return  The menuitem with the given ID, or @c NULL.
 *
 * @since 2.3.0
 */
GntMenuItem *gnt_menu_get_item(GntMenu *menu, const char *id);

G_END_DECLS

#endif /* GNT_MENU_H */
