/**
 * @file gntblist.h GNT BuddyList API
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
#ifndef _GNT_BLIST_H
#define _GNT_BLIST_H

#include "blist.h"
#include "gnttree.h"

/**********************************************************************
 * @name GNT BuddyList API
 **********************************************************************/
/*@{*/

/**
 * Buddylist manager for finch. This decides the visility, ordering and hierarchy
 * of the buddylist nodes. This also manages the creation of tooltips.
 */
typedef struct
{
	const char *id;                                    /**< An identifier for the manager. */
	const char *name;                                  /**< Displayable name for the manager. */
	gboolean (*init)(void);                            /**< Called right before it's being used. */
	gboolean (*uninit)(void);                          /**< Called right after it's not being used any more. */
	gboolean (*can_add_node)(PurpleBListNode *node);   /**< Whether a node should be added to the view. */
	gpointer (*find_parent)(PurpleBListNode *node);    /**< Find the parent row for a node. */
	gboolean (*create_tooltip)(gpointer selected_row, GString **body, char **title);  /**< Create tooltip for a selected row. */
	gpointer reserved[4];
} FinchBlistManager;

/**
 * Get the ui-functions.
 *
 * @return The PurpleBlistUiOps structure populated with the appropriate functions.
 */
PurpleBlistUiOps * finch_blist_get_ui_ops(void);

/**
 * Perform necessary initializations.
 */
void finch_blist_init(void);

/**
 * Perform necessary uninitializations.
 */
void finch_blist_uninit(void);

/**
 * Show the buddy list.
 */
void finch_blist_show(void);

/**
 * Get the position of the buddy list.
 *
 * @param x The x-coordinate is set here if not @ NULL.
 * @param y The y-coordinate is set here if not @c NULL.
 *
 * @return Returns @c TRUE if the values were set, @c FALSE otherwise.
 */
gboolean finch_blist_get_position(int *x, int *y);

/**
 * Set the position of the buddy list.
 *
 * @param x The x-coordinate of the buddy list.
 * @param y The y-coordinate of the buddy list.
 */
void finch_blist_set_position(int x, int y);

/**
 * Get the size of the buddy list.
 *
 * @param width  The width is set here if not @ NULL.
 * @param height The height is set here if not @c NULL.
 *
 * @return Returns @c TRUE if the values were set, @c FALSE otherwise.
 */
gboolean finch_blist_get_size(int *width, int *height);

/**
 * Set the size of the buddy list.
 *
 * @param width  The width of the buddy list.
 * @param height The height of the buddy list.
 */
void finch_blist_set_size(int width, int height);

/**
 * Get information about a user. Show immediate feedback.
 *
 * @param conn   The connection to get information fro
 * @param name   The user to get information about.
 *
 * @return  Returns the ui-handle for the userinfo notification.
 */
gpointer finch_retrieve_user_info(PurpleConnection *conn, const char *name);

/**
 * Get the tree list of the buddy list.
 * @return  The GntTree widget.
 */
GntTree * finch_blist_get_tree(void);

/**
 * Add an alternate buddy list manager.
 *
 * @param manager   The alternate buddylist manager.
 */
void finch_blist_install_manager(const FinchBlistManager *manager);

/**
 * Remove an alternate buddy list manager.
 *
 * @param manager   The buddy list manager to remove.
 */
void finch_blist_uninstall_manager(const FinchBlistManager *manager);

/**
 * Find a buddy list manager.
 *
 * @param id   The identifier for the desired buddy list manager.
 *
 * @return  The manager with the requested identifier, if available. @c NULL otherwise.
 */
FinchBlistManager * finch_blist_manager_find(const char *id);

/**
 * Request the active buddy list manager to add a node.
 *
 * @param node  The node to add
 */
void finch_blist_manager_add_node(PurpleBListNode *node);

/*@}*/

#endif
