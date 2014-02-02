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
#include "buddylist.h"
#include "gtkblist-theme.h"

/**************************************************************************
 * @name Structures
 **************************************************************************/
/**
 * PidginBuddyList:
 * @notebook:          The notebook that switches between the real buddy list
 *                     and the helpful instructions page
 * @main_vbox:         This vbox contains the menu and notebook
 * @vbox:              This is the vbox that everything important gets packed
 *                     into.  Your plugin might want to pack something in it
 *                     itself.  Go, plugins!
 * @treeview:          It's a treeview... d'uh.
 * @treemodel:         This is the treemodel. 
 * @text_column:       Column
 * @menutray:          The menu tray widget.
 * @menutrayicon:      The menu tray icon.
 * @refresh_timer:     The timer for refreshing every 30 seconds
 * @timeout:           The timeout for the tooltip.
 * @drag_timeout:      The timeout for expanding contacts on drags
 * @tip_rect:          This is the bounding rectangle of the cell we're
 *                     currently hovering over.  This is used for tooltips.
 * @contact_rect:      This is the bounding rectangle of the contact node and
 *                     its children.  This is used for auto-expand on mouseover.
 * @mouseover_contact: This is the contact currently mouse-over expanded
 * @tipwindow:         The window used by the tooltip
 * @tooltipdata:       The data for each "chunk" of the tooltip
 * @selected_node:     The currently selected node
 * @scrollbook:        Scrollbook for alerts
 * @headline:          Widget for headline notifications
 * @headline_label:    Label for headline notifications
 * @headline_image:    Image for headline notifications
 * @headline_callback: Callback for headline notifications
 * @headline_data:     User data for headline notifications
 * @headline_destroy:  Callback to use for destroying the headline-data
 * @statusbox:         The status selector dropdown
 * @empty_avatar:      A 32x32 transparent pixbuf
 * @priv:              Pointer to opaque private data
 *
 * Like, everything you need to know about the gtk buddy list
 */
struct _PidginBuddyList {
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *main_vbox;
	GtkWidget *vbox;

	GtkWidget *treeview;
	GtkTreeStore *treemodel;
	GtkTreeViewColumn *text_column;

	GtkCellRenderer *text_rend;

	GtkUIManager *ui;
	GtkWidget *menutray;
	GtkWidget *menutrayicon;

	guint refresh_timer;

	guint      timeout;
	guint      drag_timeout;
	GdkRectangle tip_rect;
	GdkRectangle contact_rect;
	PurpleBlistNode *mouseover_contact;

	GtkWidget *tipwindow;
	GList *tooltipdata;

	PurpleBlistNode *selected_node;

	GtkWidget *scrollbook;
	GtkWidget *headline;
	GtkWidget *headline_label;
	GtkWidget *headline_image;
	GCallback headline_callback;
	gpointer headline_data;
	GDestroyNotify headline_destroy;

	GtkWidget *statusbox;
	GdkPixbuf *empty_avatar;

	gpointer priv;
};

#define PIDGIN_BLIST(list) ((PidginBuddyList *)purple_blist_get_ui_data())
#define PIDGIN_IS_PIDGIN_BLIST(list) \
	(purple_blist_get_ui_ops() == pidgin_blist_get_ui_ops())

G_BEGIN_DECLS

/**************************************************************************
 * @name GTK+ Buddy List API
 **************************************************************************/

/**
 * pidgin_blist_get_handle:
 *
 * Get the handle for the GTK+ blist system.
 *
 * Returns: the handle to the blist system
 */
void *pidgin_blist_get_handle(void);

/**
 * pidgin_blist_init:
 *
 * Initializes the GTK+ blist system.
 */
void pidgin_blist_init(void);

/**
 * pidgin_blist_uninit:
 *
 * Uninitializes the GTK+ blist system.
 */
void pidgin_blist_uninit(void);

/**
 * pidgin_blist_get_ui_ops:
 *
 * Returns the UI operations structure for the buddy list.
 *
 * Returns: The GTK+ list operations structure.
 */
PurpleBlistUiOps *pidgin_blist_get_ui_ops(void);

/**
 * pidgin_blist_get_default_gtk_blist:
 *
 * Returns the default gtk buddy list
 *
 * There's normally only one buddy list window, but that isn't a necessity. This function
 * returns the PidginBuddyList we're most likely wanting to work with. This is slightly
 * cleaner than an externed global.
 *
 * Returns: The default GTK+ buddy list
 */
PidginBuddyList *pidgin_blist_get_default_gtk_blist(void);

/**
 * pidgin_blist_make_buddy_menu:
 * @menu:  The menu to populate
 * @buddy: The buddy whose menu to get
 * @sub:   %TRUE if this is a sub-menu, %FALSE otherwise
 *
 * Populates a menu with the items shown on the buddy list for a buddy.
 */
void pidgin_blist_make_buddy_menu(GtkWidget *menu, PurpleBuddy *buddy, gboolean sub);

/**
 * pidgin_blist_refresh:
 * @list:   This is the core list that gets updated from
 *
 * Refreshes all the nodes of the buddy list.
 * This should only be called when something changes to affect most of the nodes (such as a ui preference changing)
 */
void pidgin_blist_refresh(PurpleBuddyList *list);

void pidgin_blist_update_columns(void);
void pidgin_blist_update_refresh_timeout(void);

/**
 * pidgin_blist_get_emblem:
 * @node:   The node to return an emblem for
 *
 * Returns the blist emblem.
 *
 * This may be an existing pixbuf that has been given an additional ref,
 * so it shouldn't be modified.
 *
 * Returns:  A GdkPixbuf for the emblem to show, or NULL
 */
GdkPixbuf *
pidgin_blist_get_emblem(PurpleBlistNode *node);

/**
 * pidgin_blist_get_status_icon:
 *
 * Useful for the buddy ticker
 */
GdkPixbuf *pidgin_blist_get_status_icon(PurpleBlistNode *node,
		PidginStatusIconSize size);

/**
 * pidgin_blist_node_is_contact_expanded:
 * @node: The node in question.
 *
 * Returns a boolean indicating if @node is part of an expanded contact.
 *
 * This only makes sense for contact and buddy nodes. %FALSE is returned
 * for other types of nodes.
 *
 * Returns: A boolean indicating if @node is part of an expanded contact.
 */
gboolean pidgin_blist_node_is_contact_expanded(PurpleBlistNode *node);

/**
 * pidgin_blist_toggle_visibility:
 *
 * Intelligently toggles the visibility of the buddy list. If the buddy
 * list is obscured, it is brought to the front. If it is not obscured,
 * it is hidden. If it is hidden it is shown.
 */
void pidgin_blist_toggle_visibility(void);

/**
 * pidgin_blist_visibility_manager_add:
 *
 * Increases the reference count of visibility managers. Callers should
 * call the complementary remove function when no longer managing
 * visibility.
 *
 * A visibility manager is something that provides some method for
 * showing the buddy list after it is hidden (e.g. docklet plugin).
 */
void pidgin_blist_visibility_manager_add(void);

/**
 * pidgin_blist_visibility_manager_remove:
 *
 * Decreases the reference count of visibility managers. If the count
 * drops below zero, the buddy list is shown.
 */
void pidgin_blist_visibility_manager_remove(void);

/**
 * pidgin_blist_add_alert:
 * @widget:   The widget to add
 *
 * Adds a mini-alert to the blist scrollbook
 */
void pidgin_blist_add_alert(GtkWidget *widget);

/**
 * pidgin_blist_set_theme:
 * @theme:	the new theme to use
 *
 * Sets the current theme for Pidgin to use
 */
void pidgin_blist_set_theme(PidginBlistTheme *theme);

/**
 * pidgin_blist_get_theme:
 *
 * Gets Pidgin's current buddy list theme
 *
 * Returns:	the current theme
 */
PidginBlistTheme *pidgin_blist_get_theme(void);

/**************************************************************************
 * @name GTK+ Buddy List sorting functions
 **************************************************************************/

typedef void (*pidgin_blist_sort_function)(PurpleBlistNode *new, PurpleBuddyList *blist, GtkTreeIter group, GtkTreeIter *cur, GtkTreeIter *iter);

/**
 * pidgin_blist_get_sort_methods:
 *
 * Gets the current list of sort methods.
 *
 * Returns: A GSlist of sort methods
 */
GList *pidgin_blist_get_sort_methods(void);

struct pidgin_blist_sort_method {
	char *id;
	char *name;
	pidgin_blist_sort_function func;
};

typedef struct pidgin_blist_sort_method PidginBlistSortMethod;

/**
 * pidgin_blist_sort_method_reg:
 * @id:   The unique ID of the sorting method
 * @name: The method's name.
 * @func:  A pointer to the function.
 *
 * Registers a buddy list sorting method.
 */
void pidgin_blist_sort_method_reg(const char *id, const char *name, pidgin_blist_sort_function func);

/**
 * pidgin_blist_sort_method_unreg:
 * @id: The method's id
 *
 * Unregisters a buddy list sorting method.
 */
void pidgin_blist_sort_method_unreg(const char *id);

/**
 * pidgin_blist_sort_method_set:
 * @id: The method's id.
 *
 * Sets a buddy list sorting method.
 */
void pidgin_blist_sort_method_set(const char *id);

/**
 * pidgin_blist_setup_sort_methods:
 *
 * Sets up the programs default sort methods
 */
void pidgin_blist_setup_sort_methods(void);

/**
 * pidgin_blist_update_accounts_menu:
 *
 * Updates the accounts menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_accounts_menu(void);

/**
 * pidgin_blist_update_plugin_actions:
 *
 * Updates the plugin actions menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_plugin_actions(void);

/**
 * pidgin_blist_update_sort_methods:
 *
 * Updates the Sorting menu on the GTK+ buddy list window.
 */
void pidgin_blist_update_sort_methods(void);

/**
 * pidgin_blist_joinchat_is_showable:
 *
 * Determines if showing the join chat dialog is a valid action.
 *
 * Returns: Returns TRUE if there are accounts online capable of
 *         joining chat rooms.  Otherwise returns FALSE.
 */
gboolean pidgin_blist_joinchat_is_showable(void);

/**
 * pidgin_blist_joinchat_show:
 *
 * Shows the join chat dialog.
 */
void pidgin_blist_joinchat_show(void);

/**
 * pidgin_append_blist_node_privacy_menu:
 *
 * Appends the privacy menu items for a PurpleBlistNode
 * @todo Rename these.
 */
void pidgin_append_blist_node_privacy_menu(GtkWidget *menu, PurpleBlistNode *node);

/**
 * pidgin_append_blist_node_proto_menu:
 *
 * Appends the protocol specific menu items for a PurpleBlistNode
 * @todo Rename these.
 */
void pidgin_append_blist_node_proto_menu (GtkWidget *menu, PurpleConnection *gc, PurpleBlistNode *node);

/**
 * pidgin_append_blist_node_extended_menu:
 *
 * Appends the extended menu items for a PurpleBlistNode
 * @todo Rename these.
 */
void pidgin_append_blist_node_extended_menu(GtkWidget *menu, PurpleBlistNode *node);

/**
 * pidgin_blist_set_headline:
 * @text:	    Pango Markup for the label text
 * @pixbuf:    The GdkPixbuf for the icon
 * @callback:  The callback to call when headline is clicked
 * @user_data: The userdata to include in the callback
 * @destroy:   The callback to call when headline is closed or replaced by another headline.
 *
 * Sets a headline notification
 *
 * This is currently used for mail notification, but could theoretically be used for anything.
 * Only the most recent headline will be shown.
 */
void pidgin_blist_set_headline(const char *text, GdkPixbuf *pixbuf, GCallback callback, gpointer user_data,
		GDestroyNotify destroy);

/**
 * pidgin_blist_get_name_markup:
 * @buddy: The buddy to return markup from
 * @selected:  Whether this buddy is selected. If TRUE, the markup will not change the color.
 * @aliased:  TRUE to return the appropriate alias of this buddy, FALSE to return its username and status information
 *
 * Returns a buddy's Pango markup appropriate for setting in a GtkCellRenderer.
 *
 * Returns: The markup for this buddy
 */
gchar *pidgin_blist_get_name_markup(PurpleBuddy *buddy, gboolean selected, gboolean aliased);

/**
 * pidgin_blist_draw_tooltip:
 * @node: The buddy list node to show a tooltip for
 * @widget: The widget to draw the tooltip on
 *
 * Creates the Buddy List tooltip at the current pointer location for the given buddy list node.
 *
 * This tooltip will be destroyed the next time this function is called, or when XXXX
 * is called
 */
void pidgin_blist_draw_tooltip(PurpleBlistNode *node, GtkWidget *widget);

/**
 * pidgin_blist_tooltip_destroy:
 *
 * Destroys the current (if any) Buddy List tooltip
 */
void pidgin_blist_tooltip_destroy(void);

G_END_DECLS

#endif /* _PIDGINBLIST_H_ */
