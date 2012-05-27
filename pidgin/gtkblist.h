/**
 * @file gtkblist.h GTK+ Buddy List API
 * @ingroup pidgin
 * @see @ref gtkblist-signals
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#ifndef _PIDGINBLIST_H_
#define _PIDGINBLIST_H_

/** @copydoc _PidginBuddyList */
typedef struct _PidginBuddyList PidginBuddyList;

enum {
	STATUS_ICON_COLUMN,
	STATUS_ICON_VISIBLE_COLUMN,
	NAME_COLUMN,
	IDLE_COLUMN,
	IDLE_VISIBLE_COLUMN,
	BUDDY_ICON_COLUMN,
	BUDDY_ICON_VISIBLE_COLUMN,
	NODE_COLUMN,
	BGCOLOR_COLUMN,
	GROUP_EXPANDER_COLUMN,
	GROUP_EXPANDER_VISIBLE_COLUMN,
	CONTACT_EXPANDER_COLUMN,
	CONTACT_EXPANDER_VISIBLE_COLUMN,
	EMBLEM_COLUMN,
	EMBLEM_VISIBLE_COLUMN,
	PROTOCOL_ICON_COLUMN,
	PROTOCOL_ICON_VISIBLE_COLUMN,
	BLIST_COLUMNS

};

typedef enum {
	PIDGIN_STATUS_ICON_LARGE,
	PIDGIN_STATUS_ICON_SMALL

} PidginStatusIconSize;

#include "pidgin.h"
#include "blist.h"
#include "gtkblist-theme.h"

/**************************************************************************
 * @name Structures
 **************************************************************************/
/**
 * Like, everything you need to know about the gtk buddy list
 */
struct _PidginBuddyList {
	GtkWidget *window;
	GtkWidget *notebook;            /**< The notebook that switches between the real buddy list and the helpful
					   instructions page */
	GtkWidget *main_vbox;           /**< This vbox contains the menu and notebook */
	GtkWidget *vbox;                /**< This is the vbox that everything important gets packed into.
					   Your plugin might want to pack something in it itself.  Go, plugins! */

	GtkWidget *treeview;            /**< It's a treeview... d'uh. */
	GtkTreeStore *treemodel;        /**< This is the treemodel.  */
	GtkTreeViewColumn *text_column; /**< Column */

	GtkCellRenderer *text_rend;

	GtkUIManager *ui;
	GtkWidget *menutray;            /**< The menu tray widget. */
	GtkWidget *menutrayicon;        /**< The menu tray icon. */

	guint refresh_timer;            /**< The timer for refreshing every 30 seconds */

	guint      timeout;              /**< The timeout for the tooltip. */
	guint      drag_timeout;         /**< The timeout for expanding contacts on drags */
	GdkRectangle tip_rect;           /**< This is the bounding rectangle of the
					      cell we're currently hovering over.  This is
					      used for tooltips. */
	GdkRectangle contact_rect;       /**< This is the bounding rectangle of the contact node
					      and its children.  This is used for auto-expand on
					      mouseover. */
	PurpleBlistNode *mouseover_contact; /**< This is the contact currently mouse-over expanded */

	GtkWidget *tipwindow;            /**< The window used by the tooltip */
	GList *tooltipdata;              /**< The data for each "chunk" of the tooltip */

	PurpleBlistNode *selected_node;    /**< The currently selected node */

	GdkCursor *hand_cursor;         /**< Hand cursor */
	GdkCursor *arrow_cursor;        /**< Arrow cursor */

	GtkWidget *scrollbook;          /**< Scrollbook for alerts */
	GtkWidget *headline_hbox;       /**< Hbox for headline notification */
	GtkWidget *headline_label;	/**< Label for headline notifications */
	GtkWidget *headline_image;      /**< Image for headline notifications */
	GCallback headline_callback;    /**< Callback for headline notifications */
	gpointer headline_data;         /**< User data for headline notifications */
	GDestroyNotify headline_destroy; /**< Callback to use for destroying the headline-data */
	gboolean changing_style;        /**< True when changing GTK+ theme style */

	GtkWidget *statusbox;            /**< The status selector dropdown */
	GdkPixbuf *empty_avatar;         /**< A 32x32 transparent pixbuf */

	gpointer priv;                   /**< Pointer to opaque private data */
};

#define PIDGIN_BLIST(list) ((PidginBuddyList *)purple_blist_get_ui_data())
#define PIDGIN_IS_PIDGIN_BLIST(list) \
	(purple_blist_get_ui_ops() == pidgin_blist_get_ui_ops())

G_BEGIN_DECLS

/**************************************************************************
 * @name GTK+ Buddy List API
 **************************************************************************/

/**
 * Get the handle for the GTK+ blist system.
 *
 * @return the handle to the blist system
 */
void *pidgin_blist_get_handle(void);

/**
 * Initializes the GTK+ blist system.
 */
void pidgin_blist_init(void);

/**
 * Uninitializes the GTK+ blist system.
 */
void pidgin_blist_uninit(void);

/**
 * Returns the UI operations structure for the buddy list.
 *
 * @return The GTK+ list operations structure.
 */
PurpleBlistUiOps *pidgin_blist_get_ui_ops(void);

/**
 * Returns the default gtk buddy list
 *
 * There's normally only one buddy list window, but that isn't a necessity. This function
 * returns the PidginBuddyList we're most likely wanting to work with. This is slightly
 * cleaner than an externed global.
 *
 * @return The default GTK+ buddy list
 */
PidginBuddyList *pidgin_blist_get_default_gtk_blist(void);

/**
 * Populates a menu with the items shown on the buddy list for a buddy.
 *
 * @param menu  The menu to populate
 * @param buddy The buddy whose menu to get
 * @param sub   TRUE if this is a sub-menu, FALSE otherwise
 */
void pidgin_blist_make_buddy_menu(GtkWidget *menu, PurpleBuddy *buddy, gboolean sub);

/**
 * Refreshes all the nodes of the buddy list.
 * This should only be called when something changes to affect most of the nodes (such as a ui preference changing)
 *
 * @param list   This is the core list that gets updated from
 */
void pidgin_blist_refresh(PurpleBuddyList *list);

void pidgin_blist_update_columns(void);
void pidgin_blist_update_refresh_timeout(void);

/**
 * Returns the blist emblem.
 *
 * This may be an existing pixbuf that has been given an additional ref,
 * so it shouldn't be modified.
 *
 * @param node   The node to return an emblem for
 *
 * @return  A GdkPixbuf for the emblem to show, or NULL
 */
GdkPixbuf *
pidgin_blist_get_emblem(PurpleBlistNode *node);

/**
 * Useful for the buddy ticker
 */
GdkPixbuf *pidgin_blist_get_status_icon(PurpleBlistNode *node,
		PidginStatusIconSize size);

/**
 * Returns a boolean indicating if @a node is part of an expanded contact.
 *
 * This only makes sense for contact and buddy nodes. @c FALSE is returned
 * for other types of nodes.
 *
 * @param node The node in question.
 * @return A boolean indicating if @a node is part of an expanded contact.
 */
gboolean pidgin_blist_node_is_contact_expanded(PurpleBlistNode *node);

/**
 * Intelligently toggles the visibility of the buddy list. If the buddy
 * list is obscured, it is brought to the front. If it is not obscured,
 * it is hidden. If it is hidden it is shown.
 */
void pidgin_blist_toggle_visibility(void);

/**
 * Increases the reference count of visibility managers. Callers should
 * call the complementary remove function when no longer managing
 * visibility.
 *
 * A visibility manager is something that provides some method for
 * showing the buddy list after it is hidden (e.g. docklet plugin).
 */
void pidgin_blist_visibility_manager_add(void);

/**
 * Decreases the reference count of visibility managers. If the count
 * drops below zero, the buddy list is shown.
 */
void pidgin_blist_visibility_manager_remove(void);

/**
 * Adds a mini-alert to the blist scrollbook
 *
 * @param widget   The widget to add
 */
void pidgin_blist_add_alert(GtkWidget *widget);

/**
 * Sets the current theme for Pidgin to use
 *
 * @param theme	the new theme to use
 */
void pidgin_blist_set_theme(PidginBlistTheme *theme);

/**
 * Gets Pidgin's current buddy list theme
 *
 * @returns	the current theme
 */
PidginBlistTheme *pidgin_blist_get_theme(void);

/**************************************************************************
 * @name GTK+ Buddy List sorting functions
 **************************************************************************/

typedef void (*pidgin_blist_sort_function)(PurpleBlistNode *new, PurpleBuddyList *blist, GtkTreeIter group, GtkTreeIter *cur, GtkTreeIter *iter);

/**
 * Gets the current list of sort methods.
 *
 * @return A GSlist of sort methods
 */
GList *pidgin_blist_get_sort_methods(void);

struct pidgin_blist_sort_method {
	char *id;
	char *name;
	pidgin_blist_sort_function func;
};

typedef struct pidgin_blist_sort_method PidginBlistSortMethod;

/**
 * Registers a buddy list sorting method.
 *
 * @param id   The unique ID of the sorting method
 * @param name The method's name.
 * @param func  A pointer to the function.
 *
 */
void pidgin_blist_sort_method_reg(const char *id, const char *name, pidgin_blist_sort_function func);

/**
 * Unregisters a buddy list sorting method.
 *
 * @param id The method's id
 */
void pidgin_blist_sort_method_unreg(const char *id);

/**
 * Sets a buddy list sorting method.
 *
 * @param id The method's id.
 */
void pidgin_blist_sort_method_set(const char *id);

/**
 * Sets up the programs default sort methods
 */
void pidgin_blist_setup_sort_methods(void);

/**
 * Updates the accounts menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_accounts_menu(void);

/**
 * Updates the plugin actions menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_plugin_actions(void);

/**
 * Updates the Sorting menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_sort_methods(void);

/**
 * Determines if showing the join chat dialog is a valid action.
 *
 * @return Returns TRUE if there are accounts online capable of
 *         joining chat rooms.  Otherwise returns FALSE.
 */
gboolean pidgin_blist_joinchat_is_showable(void);

/**
 * Shows the join chat dialog.
 */
void pidgin_blist_joinchat_show(void);

/**
 * Appends the privacy menu items for a PurpleBlistNode
 * TODO: Rename these.
 */
void pidgin_append_blist_node_privacy_menu(GtkWidget *menu, PurpleBlistNode *node);

/**
 * Appends the protocol specific menu items for a PurpleBlistNode
 * TODO: Rename these.
 */
void pidgin_append_blist_node_proto_menu (GtkWidget *menu, PurpleConnection *gc, PurpleBlistNode *node);

/**
 * Appends the extended menu items for a PurpleBlistNode
 * TODO: Rename these.
 */
void pidgin_append_blist_node_extended_menu(GtkWidget *menu, PurpleBlistNode *node);

/**
 * Sets a headline notification
 *
 * This is currently used for mail notification, but could theoretically be used for anything.
 * Only the most recent headline will be shown.
 *
 * @param text	    Pango Markup for the label text
 * @param pixbuf    The GdkPixbuf for the icon
 * @param callback  The callback to call when headline is clicked
 * @param user_data The userdata to include in the callback
 * @param destroy   The callback to call when headline is closed or replaced by another headline.
 */
void pidgin_blist_set_headline(const char *text, GdkPixbuf *pixbuf, GCallback callback, gpointer user_data,
		GDestroyNotify destroy);

/**
 * Returns a buddy's Pango markup appropriate for setting in a GtkCellRenderer.
 *
 * @param buddy The buddy to return markup from
 * @param selected  Whether this buddy is selected. If TRUE, the markup will not change the color.
 * @param aliased  TRUE to return the appropriate alias of this buddy, FALSE to return its username and status information
 * @return The markup for this buddy
 */
gchar *pidgin_blist_get_name_markup(PurpleBuddy *buddy, gboolean selected, gboolean aliased);

/**
 * Creates the Buddy List tooltip at the current pointer location for the given buddy list node.
 *
 * This tooltip will be destroyed the next time this function is called, or when XXXX
 * is called
 *
 * @param node The buddy list node to show a tooltip for
 * @param widget The widget to draw the tooltip on
 */
void pidgin_blist_draw_tooltip(PurpleBlistNode *node, GtkWidget *widget);

/**
 * Destroys the current (if any) Buddy List tooltip
 */
void pidgin_blist_tooltip_destroy(void);

G_END_DECLS

#endif /* _PIDGINBLIST_H_ */
