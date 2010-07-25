/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup pidgin
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
#ifndef _PIDGINUTILS_H_
#define _PIDGINUTILS_H_

#include "gtkconv.h"
#include "pidgin.h"
#include "prpl.h"
#include "util.h"





typedef enum
{
	PIDGIN_BUTTON_HORIZONTAL,
	PIDGIN_BUTTON_VERTICAL

} PidginButtonOrientation;

typedef enum
{
	PIDGIN_BUTTON_NONE = 0,
	PIDGIN_BUTTON_TEXT,
	PIDGIN_BUTTON_IMAGE,
	PIDGIN_BUTTON_TEXT_IMAGE

} PidginButtonStyle;

typedef enum
{
	PIDGIN_PRPL_ICON_SMALL,
	PIDGIN_PRPL_ICON_MEDIUM,
	PIDGIN_PRPL_ICON_LARGE
} PidginPrplIconSize;

#ifndef _WIN32
typedef enum
{
	PIDGIN_BROWSER_DEFAULT = 0,
	PIDGIN_BROWSER_CURRENT,
	PIDGIN_BROWSER_NEW_WINDOW,
	PIDGIN_BROWSER_NEW_TAB

} PidginBrowserPlace;
#endif /* _WIN32 */

typedef struct {
	gboolean is_buddy;
	union {
		PurpleBuddy *buddy;
		PurpleLogSet *logged_buddy;
	} entry;
} PidginBuddyCompletionEntry;

typedef gboolean (*PidginFilterBuddyCompletionEntryFunc) (const PidginBuddyCompletionEntry *completion_entry, gpointer user_data);


/**
 * Sets up a gtkimhtml widget, loads it with smileys, and sets the
 * default signal handlers.
 *
 * @param imhtml The gtkimhtml widget to setup.
 */
void pidgin_setup_imhtml(GtkWidget *imhtml);

/**
 * Create an GtkIMHtml widget and associated GtkIMHtmlToolbar widget.  This
 * functions puts both widgets in a nice GtkFrame.  They're separate by an
 * attractive GtkSeparator.
 *
 * @param editable @c TRUE if this imhtml should be editable.  If this is @c FALSE,
 *        then the toolbar will NOT be created.  If this imthml should be
 *        read-only at first, but may become editable later, then pass in
 *        @c TRUE here and then manually call gtk_imhtml_set_editable() later.
 * @param imhtml_ret A pointer to a pointer to a GtkWidget.  This pointer
 *        will be set to the imhtml when this function exits.
 * @param toolbar_ret A pointer to a pointer to a GtkWidget.  If editable is
 *        TRUE then this will be set to the toolbar when this function exits.
 *        Otherwise this will be set to @c NULL.
 * @param sw_ret This will be filled with a pointer to the scrolled window
 *        widget which contains the imhtml.
 * @return The GtkFrame containing the toolbar and imhtml.
 */
GtkWidget *pidgin_create_imhtml(gboolean editable, GtkWidget **imhtml_ret, GtkWidget **toolbar_ret, GtkWidget **sw_ret);

/**
 * Creates a small button
 *
 * @param  image   A button image.
 *
 * @return   A GtkButton created from the image.
 * @since 2.7.0
 */
GtkWidget *pidgin_create_small_button(GtkWidget *image);

/**
 * Creates a new window
 *
 * @param title        The window title, or @c NULL
 * @param border_width The window's desired border width
 * @param role         A string indicating what the window is responsible for doing, or @c NULL
 * @param resizable    Whether the window should be resizable (@c TRUE) or not (@c FALSE)
 *
 * @since 2.1.0
 */
GtkWidget *pidgin_create_window(const char *title, guint border_width, const char *role, gboolean resizable);

/**
 * Creates a new dialog window
 *
 * @param title        The window title, or @c NULL
 * @param border_width The window's desired border width
 * @param role         A string indicating what the window is responsible for doing, or @c NULL
 * @param resizable    Whether the window should be resizable (@c TRUE) or not (@c FALSE)
 *
 * @since 2.4.0
 */
GtkWidget *pidgin_create_dialog(const char *title, guint border_width, const char *role, gboolean resizable);

/**
 * Retrieves the main content box (vbox) from a pidgin dialog window
 *
 * @param dialog       The dialog window
 * @param homogeneous  TRUE if all children are to be given equal space allotments.
 * @param spacing      the number of pixels to place by default between children
 *
 * @since 2.4.0
 */
GtkWidget *pidgin_dialog_get_vbox_with_properties(GtkDialog *dialog, gboolean homogeneous, gint spacing);

/**
 * Retrieves the main content box (vbox) from a pidgin dialog window
 *
 * @param dialog       The dialog window
 *
 * @since 2.4.0
 */
GtkWidget *pidgin_dialog_get_vbox(GtkDialog *dialog);

/**
 * Add a button to a dialog created by #pidgin_create_dialog.
 *
 * @param dialog         The dialog window
 * @param label          The stock-id or the label for the button
 * @param callback       The callback function for the button
 * @param callbackdata   The user data for the callback function
 *
 * @return The created button.
 * @since 2.4.0
 */
GtkWidget *pidgin_dialog_add_button(GtkDialog *dialog, const char *label,
		GCallback callback, gpointer callbackdata);

/**
 * Retrieves the action area (button box) from a pidgin dialog window
 *
 * @param dialog       The dialog window
 *
 * @since 2.4.0
 */
GtkWidget *pidgin_dialog_get_action_area(GtkDialog *dialog);

/**
 * Toggles the sensitivity of a widget.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The widget to toggle.
 */
void pidgin_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Checks if text has been entered into a GtkTextEntry widget.  If
 * so, the GTK_RESPONSE_OK on the given dialog is set to TRUE.
 * Otherwise GTK_RESPONSE_OK is set to FALSE.
 *
 * @param entry  The text entry widget.
 * @param dialog The dialog containing the text entry widget.
 */
void pidgin_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog);

/**
 * Toggles the sensitivity of all widgets in a pointer array.
 *
 * @param w    @c NULL. Used for signal handlers.
 * @param data The array containing the widgets to toggle.
 */
void pidgin_toggle_sensitive_array(GtkWidget *w, GPtrArray *data);

/**
 * Toggles the visibility of a widget.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The widget to toggle.
 */
void pidgin_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Adds a separator to a menu.
 *
 * @param menu The menu to add a separator to.
 *
 * @return The separator.
 */
GtkWidget *pidgin_separator(GtkWidget *menu);

/**
 * Creates a menu item.
 *
 * @param menu The menu to which to append the menu item.
 * @param str  The title to use for the newly created menu item.
 *
 * @return The newly created menu item.
 */
GtkWidget *pidgin_new_item(GtkWidget *menu, const char *str);

/**
 * Creates a check menu item.
 *
 * @param menu     The menu to which to append the check menu item.
 * @param str      The title to use for the newly created menu item.
 * @param cb       A function to call when the menu item is activated.
 * @param data     Data to pass to the signal function.
 * @param checked  The initial state of the check item
 *
 * @return The newly created menu item.
 */
GtkWidget *pidgin_new_check_item(GtkWidget *menu, const char *str,
		GCallback cb, gpointer data, gboolean checked);

/**
 * Creates a menu item.
 *
 * @param menu       The menu to which to append the menu item.
 * @param str        The title for the menu item.
 * @param icon       An icon to place to the left of the menu item,
 *                   or @c NULL for no icon.
 * @param cb         A function to call when the menu item is activated.
 * @param data       Data to pass to the signal function.
 * @param accel_key  Something.
 * @param accel_mods Something.
 * @param mod        Something.
 *
 * @return The newly created menu item.
 */
GtkWidget *pidgin_new_item_from_stock(GtkWidget *menu, const char *str,
									const char *icon, GCallback cb,
									gpointer data, guint accel_key,
									guint accel_mods, char *mod);

/**
 * Creates a button with the specified text and stock icon.
 *
 * @param text  The text for the button.
 * @param icon  The stock icon name.
 * @param style The orientation of the button.
 *
 * @return The button.
 */
GtkWidget *pidgin_pixbuf_button_from_stock(const char *text, const char *icon,
										 PidginButtonOrientation style);

/**
 * Creates a toolbar button with the stock icon.
 *
 * @param stock The stock icon name.
 *
 * @return The button.
 */
GtkWidget *pidgin_pixbuf_toolbar_button_from_stock(const char *stock);

/**
 * Creates a HIG preferences frame.
 *
 * @param parent The widget to put the frame into.
 * @param title  The title for the frame.
 *
 * @return The vbox to put things into.
 */
GtkWidget *pidgin_make_frame(GtkWidget *parent, const char *title);

/**
 * Creates a drop-down option menu filled with protocols.
 *
 * @param id        The protocol to select by default.
 * @param cb        The callback to call when a protocol is selected.
 * @param user_data Data to pass to the callback function.
 *
 * @return The drop-down option menu.
 */
GtkWidget *pidgin_protocol_option_menu_new(const char *id,
											 GCallback cb,
											 gpointer user_data);

/**
 * Gets the currently selected protocol from a protocol drop down box.
 *
 * @param optmenu The drop-down option menu created by
 *        pidgin_account_option_menu_new.
 * @return Returns the protocol ID that is currently selected.
 */
const char *pidgin_protocol_option_menu_get_selected(GtkWidget *optmenu);

/**
 * Creates a drop-down option menu filled with accounts.
 *
 * @param default_account The account to select by default.
 * @param show_all        Whether or not to show all accounts, or just
 *                        active accounts.
 * @param cb              The callback to call when an account is selected.
 * @param filter_func     A function for checking if an account should
 *                        be shown. This can be NULL.
 * @param user_data       Data to pass to the callback function.
 *
 * @return The drop-down option menu.
 */
GtkWidget *pidgin_account_option_menu_new(PurpleAccount *default_account,
		gboolean show_all, GCallback cb,
		PurpleFilterAccountFunc filter_func, gpointer user_data);

/**
 * Gets the currently selected account from an account drop down box.
 *
 * @param optmenu The drop-down option menu created by
 *        pidgin_account_option_menu_new.
 * @return Returns the PurpleAccount that is currently selected.
 */
PurpleAccount *pidgin_account_option_menu_get_selected(GtkWidget *optmenu);

/**
 * Sets the currently selected account for an account drop down box.
 *
 * @param optmenu The GtkOptionMenu created by
 *        pidgin_account_option_menu_new.
 * @param account The PurpleAccount to select.
 */
void pidgin_account_option_menu_set_selected(GtkWidget *optmenu, PurpleAccount *account);

/**
 * Add autocompletion of screenames to an entry, supporting a filtering function.
 *
 * @param entry       The GtkEntry on which to setup autocomplete.
 * @param optmenu     A menu for accounts, returned by gaim_gtk_account_option_menu_new().
 *                    If @a optmenu is not @c NULL, it'll be updated when a username is chosen
 *                    from the autocomplete list.
 * @param filter_func A function for checking if an autocomplete entry
 *                    should be shown. This can be @c NULL.
 * @param user_data  The data to be passed to the filter_func function.
 */
void pidgin_setup_screenname_autocomplete_with_filter(GtkWidget *entry, GtkWidget *optmenu, PidginFilterBuddyCompletionEntryFunc filter_func, gpointer user_data);

/**
 * The default filter function for username autocomplete.
 *
 * @param completion_entry The completion entry to filter.
 * @param all_accounts  If this is @c FALSE, only the autocompletion entries
 *                         which belong to an online account will be filtered.
 * @return Returns @c TRUE if the autocompletion entry is filtered.
 */
gboolean pidgin_screenname_autocomplete_default_filter(const PidginBuddyCompletionEntry *completion_entry, gpointer all_accounts);

/**
 * Add autocompletion of screenames to an entry.
 *
 * @deprecated
 *   For new code, use the equivalent:
 *   #pidgin_setup_screenname_autocomplete_with_filter(@a entry, @a optmenu,
 *   #pidgin_screenname_autocomplete_default_filter, <tt>GINT_TO_POINTER(@a
 *   all)</tt>)
 *
 * @param entry     The GtkEntry on which to setup autocomplete.
 * @param optmenu   A menu for accounts, returned by
 *                  pidgin_account_option_menu_new().  If @a optmenu is not @c
 *                  NULL, it'll be updated when a username is chosen from the
 *                  autocomplete list.
 * @param all       Whether to include usernames from disconnected accounts.
 */
void pidgin_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *optmenu, gboolean all);

/**
 * Check if the given path is a directory or not.  If it is, then modify
 * the given GtkFileSelection dialog so that it displays the given path.
 * If the given path is not a directory, then do nothing.
 *
 * @param path    The path entered in the file selection window by the user.
 * @param filesel The file selection window.
 *
 * @return TRUE if given path is a directory, FALSE otherwise.
 */
#if GTK_CHECK_VERSION(2,4,0)
gboolean pidgin_check_if_dir(const char *path, gpointer filesel);
#else
gboolean pidgin_check_if_dir(const char *path, GtkFileSelection *filesel);
#endif

/**
 * Sets up GtkSpell for the given GtkTextView, reporting errors
 * if encountered.
 *
 * This does nothing if Pidgin is not compiled with GtkSpell support.
 *
 * @param textview The textview widget to setup spellchecking for.
 */
void pidgin_setup_gtkspell(GtkTextView *textview);

/**
 * Save menu accelerators callback
 */
void pidgin_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
							 GdkModifierType arg2, GClosure *arg3,
							 gpointer data);

/**
 * Save menu accelerators
 */
gboolean pidgin_save_accels(gpointer data);

/**
 * Load menu accelerators
 */
void pidgin_load_accels(void);

/**
 * Get information about a user. Show immediate feedback.
 *
 * @param conn   The connection to get information from.
 * @param name   The user to get information about.
 *
 * @since 2.1.0
 */
void pidgin_retrieve_user_info(PurpleConnection *conn, const char *name);

/**
 * Get information about a user in a chat. Show immediate feedback.
 *
 * @param conn   The connection to get information from.
 * @param name   The user to get information about.
 * @param chatid The chat id.
 *
 * @since 2.1.0
 */
void pidgin_retrieve_user_info_in_chat(PurpleConnection *conn, const char *name, int chatid);

/**
 * Parses an application/x-im-contact MIME message and returns the
 * data inside.
 *
 * @param msg          The MIME message.
 * @param all_accounts If TRUE, check all compatible accounts, online or
 *                     offline. If FALSE, check only online accounts.
 * @param ret_account  The best guess at a compatible protocol,
 *                     based on ret_protocol. If NULL, no account was found.
 * @param ret_protocol The returned protocol type.
 * @param ret_username The returned username.
 * @param ret_alias    The returned alias.
 *
 * @return TRUE if the message was parsed for the minimum necessary data.
 *         FALSE otherwise.
 */
gboolean pidgin_parse_x_im_contact(const char *msg, gboolean all_accounts,
									 PurpleAccount **ret_account,
									 char **ret_protocol, char **ret_username,
									 char **ret_alias);

/**
 * Sets an ATK name for a given widget.  Also sets the labelled-by
 * and label-for ATK relationships.
 *
 * @param w The widget that we want to name.
 * @param l A GtkLabel that we want to use as the ATK name for the widget.
 */
void pidgin_set_accessible_label(GtkWidget *w, GtkWidget *l);

/**
 * Sets the labelled-by and label-for ATK relationships.
 *
 * @param w The widget that we want to label.
 * @param l A GtkLabel that we want to use as the label for the widget.
 *
 * @since 2.2.0
 */
void pidgin_set_accessible_relations(GtkWidget *w, GtkWidget *l);

/**
 * A helper function for GtkMenuPositionFuncs. This ensures the menu will
 * be kept on screen if possible.
 *
 * @param menu The menu we are positioning.
 * @param x Address of the gint representing the horizontal position
 *        where the menu shall be drawn. This is an output parameter.
 * @param y Address of the gint representing the vertical position
 *        where the menu shall be drawn. This is an output parameter.
 * @param push_in This is an output parameter?
 * @param data Not used by this particular position function.
 *
 * @since 2.1.0
 */
void pidgin_menu_position_func_helper(GtkMenu *menu, gint *x, gint *y,
										gboolean *push_in, gpointer data);

/**
 * A valid GtkMenuPositionFunc.  This is used to determine where
 * to draw context menus when the menu is activated with the
 * keyboard (shift+F10).  If the menu is activated with the mouse,
 * then you should just use GTK's built-in position function,
 * because it does a better job of positioning the menu.
 *
 * @param menu The menu we are positioning.
 * @param x Address of the gint representing the horizontal position
 *        where the menu shall be drawn. This is an output parameter.
 * @param y Address of the gint representing the vertical position
 *        where the menu shall be drawn. This is an output parameter.
 * @param push_in This is an output parameter?
 * @param user_data Not used by this particular position function.
 */
void pidgin_treeview_popup_menu_position_func(GtkMenu *menu,
												gint *x,
												gint *y,
												gboolean *push_in,
												gpointer user_data);

/**
 * Manages drag'n'drop of files.
 *
 * @param sd GtkSelectionData for managing drag'n'drop
 * @param account Account to be used (may be NULL if conv is not NULL)
 * @param who Buddy name (may be NULL if conv is not NULL)
 */
void pidgin_dnd_file_manage(GtkSelectionData *sd, PurpleAccount *account, const char *who);

/**
 * Convenience wrapper for purple_buddy_icon_get_scale_size
 */
void pidgin_buddy_icon_get_scale_size(GdkPixbuf *buf, PurpleBuddyIconSpec *spec, PurpleIconScaleRules rules, int *width, int *height);

/**
 * Returns the base image to represent the account, based on
 * the currently selected theme.
 *
 * @param account      The account.
 * @param size         The size of the icon to return.
 *
 * @return A newly-created pixbuf with a reference count of 1,
 *         or NULL if any of several error conditions occurred:
 *         the file could not be opened, there was no loader
 *         for the file's format, there was not enough memory
 *         to allocate the image buffer, or the image file
 *         contained invalid data.
 */
GdkPixbuf *pidgin_create_prpl_icon(PurpleAccount *account, PidginPrplIconSize size);

/**
 * Creates a status icon for a given primitve
 *
 * @param primitive  The status primitive
 * @param w          The widget to render this
 * @param size       The icon size to render at
 * @return A GdkPixbuf, created from stock
 */
GdkPixbuf * pidgin_create_status_icon(PurpleStatusPrimitive primitive, GtkWidget *w, const char *size);

/**
 * Returns an appropriate stock-id for a status primitive.
 *
 * @param prim   The status primitive
 *
 * @return The stock-id
 *
 * @since 2.6.0
 */
const char *pidgin_stock_id_from_status_primitive(PurpleStatusPrimitive prim);

/**
 * Returns an appropriate stock-id for a PurplePresence.
 *
 * @param presence   The presence.
 *
 * @return The stock-id
 *
 * @since 2.6.0
 */
const char *pidgin_stock_id_from_presence(PurplePresence *presence);

/**
 * Append a PurpleMenuAction to a menu.
 *
 * @param menu    The menu to append to.
 * @param act     The PurpleMenuAction to append.
 * @param gobject The object to be passed to the action callback.
 *
 * @return   The menuitem added.
 */
GtkWidget *pidgin_append_menu_action(GtkWidget *menu, PurpleMenuAction *act,
                                 gpointer gobject);

/**
 * Sets the mouse pointer for a GtkWidget.
 *
 * After setting the cursor, the display is flushed, so the change will
 * take effect immediately.
 *
 * If the window for @a widget is @c NULL, this function simply returns.
 *
 * @param widget      The widget for which to set the mouse pointer
 * @param cursor_type The type of cursor to set
 */
void pidgin_set_cursor(GtkWidget *widget, GdkCursorType cursor_type);

/**
 * Sets the mouse point for a GtkWidget back to that of its parent window.
 *
 * If @a widget is @c NULL, this function simply returns.
 *
 * If the window for @a widget is @c NULL, this function simply returns.
 *
 * @note The display is not flushed from this function.
 */
void pidgin_clear_cursor(GtkWidget *widget);

/**
 * Creates a File Selection widget for choosing a buddy icon
 *
 * @param parent      The parent window
 * @param callback    The callback to call when the window is closed. If the user chose an icon, the char* argument will point to its path
 * @param data        Data to pass to @a callback
 * @return            The file dialog
 */
GtkWidget *pidgin_buddy_icon_chooser_new(GtkWindow *parent, void(*callback)(const char*,gpointer), gpointer data);

/**
 * Converts a buddy icon to the required size and format
 *
 * @param plugin     The prpl to convert the icon
 * @param path       The path of a file to convert
 * @param len        If not @c NULL, the length of the returned data will be set here.
 *
 * @return           The converted image data, or @c NULL if an error occurred.
 */
gpointer pidgin_convert_buddy_icon(PurplePlugin *plugin, const char *path, size_t *len);

#if !(defined PIDGIN_DISABLE_DEPRECATED) || (defined _PIDGIN_GTKUTILS_C_)
/**
 * Set or unset a custom buddyicon for a user.
 *
 * @param account   The account the user belongs to.
 * @param who       The name of the user.
 * @param filename  The path of the custom icon. If this is @c NULL, then any
 *                  previously set custom buddy icon for the user is removed.
 * @deprecated See purple_buddy_icons_node_set_custom_icon_from_file()
 */
void pidgin_set_custom_buddy_icon(PurpleAccount *account, const char *who, const char *filename);
#endif

/**
 * Converts "->" and "<-" in strings to Unicode arrow characters, for use in referencing
 * menu items.
 *
 * @param str      The text to convert
 * @return         A newly allocated string with unicode arrow characters
 */
char *pidgin_make_pretty_arrows(const char *str);

/**
 * The type of callbacks passed to pidgin_make_mini_dialog().
 */
typedef void (*PidginUtilMiniDialogCallback)(gpointer user_data, GtkButton *);

/**
 * Creates a #PidginMiniDialog, tied to a #PurpleConnection, suitable for
 * embedding in the buddy list scrollbook with pidgin_blist_add_alert().
 *
 * @param handle         The #PurpleConnection to which this mini-dialog
 *                       refers, or @c NULL if it does not refer to a
 *                       connection.  If @a handle is supplied, the mini-dialog
 *                       will be automatically removed and destroyed when the
 *                       connection signs off.
 * @param stock_id       The ID of a stock image to use in the mini dialog.
 * @param primary        The primary text
 * @param secondary      The secondary text, or @c NULL for no description.
 * @param user_data      Data to pass to the callbacks
 * @param ...            a <tt>NULL</tt>-terminated list of button labels
 *                       (<tt>char *</tt>) and callbacks
 *                       (#PidginUtilMiniDialogCallback).  @a user_data will be
 *                       passed as the first argument.  (Callbacks may lack a
 *                       second argument, or be @c NULL to take no action when
 *                       the corresponding button is pressed.) When a button is
 *                       pressed, the callback (if any) will be called; when
 *                       the callback returns the dialog will be destroyed.
 * @return               A #PidginMiniDialog, suitable for passing to
 *                       pidgin_blist_add_alert().
 * @see pidginstock.h
 */
GtkWidget *pidgin_make_mini_dialog(PurpleConnection *handle,
	const char* stock_id, const char *primary, const char *secondary,
	void *user_data, ...) G_GNUC_NULL_TERMINATED;

/**
 * This is a callback function to be used for Ctrl+F searching in treeviews.
 * Sample Use:
 * 		gtk_tree_view_set_search_equal_func(treeview,
 * 				pidgin_tree_view_search_equal_func,
 * 				search_data, search_data_destroy_cb);
 *
 */
gboolean pidgin_tree_view_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data);

/**
 * Sets or resets a window to 'urgent,' by setting the URGENT hint in X
 * or blinking in the win32 taskbar
 *
 * @param window  The window to draw attention to
 * @param urgent  Whether to set the urgent hint or not
 */
void pidgin_set_urgent(GtkWindow *window, gboolean urgent);

/**
 * Returns TRUE if the GdkPixbuf is opaque, as determined by no
 * alpha at any of the edge pixels.
 *
 * @param pixbuf  The pixbug
 * @return TRUE if the pixbuf is opaque around the edges, FALSE otherwise
 */
gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf);

/**
 * Rounds the corners of a 32x32 GdkPixbuf in place
 *
 * @param pixbuf  The buddy icon to transform
 */
void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf);

/**
 * Returns an HTML-style color string for use as a dim grey
 * string
 *
 * @param widget  The widget to return dim grey for
 * @return The dim grey string
 */
const char *pidgin_get_dim_grey_string(GtkWidget *widget);

/**
 * Create a simple text GtkComboBoxEntry equivalent
 *
 * @param default_item   Initial contents of GtkEntry
 * @param items          GList containing strings to add to GtkComboBox
 *
 * @return               A newly created text GtkComboBox containing a GtkEntry
 *                       child.
 *
 * @since 2.2.0
 */
GtkWidget *pidgin_text_combo_box_entry_new(const char *default_item, GList *items);

/**
 * Retrieve the text from the entry of the simple text GtkComboBoxEntry equivalent
 *
 * @param widget         The simple text GtkComboBoxEntry equivalent widget
 *
 * @return               The text in the widget's entry. It must not be freed
 *
 * @since 2.2.0
 */
const char *pidgin_text_combo_box_entry_get_text(GtkWidget *widget);

/**
 * Set the text in the entry of the simple text GtkComboBoxEntry equivalent
 *
 * @param widget         The simple text GtkComboBoxEntry equivalent widget
 * @param text           The text to set
 *
 * @since 2.2.0
 */
void pidgin_text_combo_box_entry_set_text(GtkWidget *widget, const char *text);

/**
 * Automatically make a window transient to a suitable parent window.
 *
 * @param window    The window to make transient.
 *
 * @return Whether the window was made transient or not.
 *
 * @since 2.4.0
 */
gboolean pidgin_auto_parent_window(GtkWidget *window);

/**
 * Add a labelled widget to a GtkVBox
 *
 * @param vbox         The GtkVBox to add the widget to.
 * @param widget_label The label to give the widget, can be @c NULL.
 * @param sg           The GtkSizeGroup to add the label to, can be @c NULL.
 * @param widget       The GtkWidget to add.
 * @param expand       Whether to expand the widget horizontally.
 * @param p_label      Place to store a pointer to the GtkLabel, or @c NULL if you don't care.
 *
 * @return  A GtkHBox already added to the GtkVBox containing the GtkLabel and the GtkWidget.
 * @since 2.4.0
 */
GtkWidget *pidgin_add_widget_to_vbox(GtkBox *vbox, const char *widget_label, GtkSizeGroup *sg, GtkWidget *widget, gboolean expand, GtkWidget **p_label);

/**
 * Create a GdkPixbuf from a PurpleStoredImage.
 *
 * @param  image   A PurpleStoredImage.
 *
 * @return   A GdkPixbuf created from the stored image.
 *
 * @since 2.5.0
 */
GdkPixbuf *pidgin_pixbuf_from_imgstore(PurpleStoredImage *image);

/**
 * Initialize some utility functions.
 *
 * @since 2.6.0
 */
void pidgin_utils_init(void);

/**
 * Uninitialize some utility functions.
 *
 * @since 2.6.0
 */
void pidgin_utils_uninit(void);

#endif /* _PIDGINUTILS_H_ */

