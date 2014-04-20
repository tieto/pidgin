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
/**
 * SECTION:gntblist
 * @section_id: finch-gntblist
 * @short_description: <filename>gntblist.h</filename>
 * @title: Buddy List API
 */

#include "buddylist.h"
#include "gnt.h"
#include "gnttree.h"

#define FINCH_TYPE_BLIST_MANAGER (finch_blist_manager_get_type())

/**********************************************************************
 * GNT BuddyList API
 **********************************************************************/

typedef struct _FinchBlistManager  FinchBlistManager;

/**
 * FinchBlistManager:
 * @id:             An identifier for the manager.
 * @name:           Displayable name for the manager.
 * @init:           Called right before it's being used.
 * @uninit:         Called right after it's not being used any more.
 * @can_add_node:   Whether a node should be added to the view.
 * @find_parent:    Find the parent row for a node.
 * @create_tooltip: Create tooltip for a selected row.
 *
 * Buddylist manager for finch. This decides the visility, ordering and hierarchy
 * of the buddylist nodes. This also manages the creation of tooltips.
 */
struct _FinchBlistManager
{
	const char *id;
	const char *name;
	gboolean (*init)(void);
	gboolean (*uninit)(void);
	gboolean (*can_add_node)(PurpleBlistNode *node);
	gpointer (*find_parent)(PurpleBlistNode *node);
	gboolean (*create_tooltip)(gpointer selected_row, GString **body, char **title);

	/*< private >*/
	gpointer reserved[4];
};

/**
 * finch_blist_manager_get_type:
 *
 * Returns: The #GType for the #FinchBlistManager boxed structure.
 */
GType finch_blist_manager_get_type(void);

/**
 * finch_blist_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleBlistUiOps structure populated with the appropriate functions.
 */
PurpleBlistUiOps * finch_blist_get_ui_ops(void);

/**
 * finch_blist_init:
 *
 * Perform necessary initializations.
 */
void finch_blist_init(void);

/**
 * finch_blist_uninit:
 *
 * Perform necessary uninitializations.
 */
void finch_blist_uninit(void);

/**
 * finch_blist_show:
 *
 * Show the buddy list.
 */
void finch_blist_show(void);

/**
 * finch_blist_get_position:
 * @x: The x-coordinate is set here if not %NULL.
 * @y: The y-coordinate is set here if not %NULL.
 *
 * Get the position of the buddy list.
 *
 * Returns: Returns %TRUE if the values were set, %FALSE otherwise.
 */
gboolean finch_blist_get_position(int *x, int *y);

/**
 * finch_blist_set_position:
 * @x: The x-coordinate of the buddy list.
 * @y: The y-coordinate of the buddy list.
 *
 * Set the position of the buddy list.
 */
void finch_blist_set_position(int x, int y);

/**
 * finch_blist_get_size:
 * @width:  The width is set here if not %NULL.
 * @height: The height is set here if not %NULL.
 *
 * Get the size of the buddy list.
 *
 * Returns: Returns %TRUE if the values were set, %FALSE otherwise.
 */
gboolean finch_blist_get_size(int *width, int *height);

/**
 * finch_blist_set_size:
 * @width:  The width of the buddy list.
 * @height: The height of the buddy list.
 *
 * Set the size of the buddy list.
 */
void finch_blist_set_size(int width, int height);

/**
 * finch_retrieve_user_info:
 * @conn:   The connection to get information from
 * @name:   The user to get information about.
 *
 * Get information about a user. Show immediate feedback.
 *
 * Returns: (transfer none): Returns the ui-handle for the userinfo
 *          notification.
 */
gpointer finch_retrieve_user_info(PurpleConnection *conn, const char *name);

/**
 * finch_blist_get_tree:
 *
 * Get the tree list of the buddy list.
 *
 * Returns: (transfer none): The GntTree widget.
 */
GntTree * finch_blist_get_tree(void);

/**
 * finch_blist_install_manager:
 * @manager:   The alternate buddylist manager.
 *
 * Add an alternate buddy list manager.
 */
void finch_blist_install_manager(const FinchBlistManager *manager);

/**
 * finch_blist_uninstall_manager:
 * @manager:   The buddy list manager to remove.
 *
 * Remove an alternate buddy list manager.
 */
void finch_blist_uninstall_manager(const FinchBlistManager *manager);

/**
 * finch_blist_manager_find:
 * @id:   The identifier for the desired buddy list manager.
 *
 * Find a buddy list manager.
 *
 * Returns: The manager with the requested identifier, if available. %NULL
 *          otherwise.
 */
FinchBlistManager * finch_blist_manager_find(const char *id);

/**
 * finch_blist_manager_add_node:
 * @node:  The node to add
 *
 * Request the active buddy list manager to add a node.
 */
void finch_blist_manager_add_node(PurpleBlistNode *node);

#endif
