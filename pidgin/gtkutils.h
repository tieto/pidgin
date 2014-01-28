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
#include "protocol.h"
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
	PIDGIN_PROTOCOL_ICON_SMALL,
	PIDGIN_PROTOCOL_ICON_MEDIUM,
	PIDGIN_PROTOCOL_ICON_LARGE
} PidginProtocolIconSize;

#ifndef _WIN32
typedef enum
{
	PIDGIN_BROWSER_DEFAULT = 0,
	/* value '1' was used by PIDGIN_BROWSER_CURRENT, which no longer exists */
	PIDGIN_BROWSER_NEW_WINDOW = 2,
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


G_BEGIN_DECLS

/**
 * Sets up a gtkwebview widget, loads it with smileys, and sets the
 * default signal handlers.
 *
 * @webview: The gtkwebview widget to setup.
 */
void pidgin_setup_webview(GtkWidget *webview);

/**
 * Create an GtkWebView widget and associated GtkWebViewToolbar widget.  This
 * function puts both widgets in a nice GtkFrame.  They're separated by an
 * attractive GtkSeparator.
 *
 * @editable: @c TRUE if this webview should be editable.  If this is
 *        @c FALSE, then the toolbar will NOT be created.  If this webview
 *        should be read-only at first, but may become editable later, then
 *        pass in @c TRUE here and then manually call gtk_webview_set_editable()
 *        later.
 * @webview_ret: A pointer to a pointer to a GtkWidget.  This pointer
 *        will be set to the webview when this function exits.
 * @sw_ret: This will be filled with a pointer to the scrolled window
 *        widget which contains the webview.
 *
 * Returns: The GtkFrame containing the toolbar and webview.
 */
GtkWidget *pidgin_create_webview(gboolean editable, GtkWidget **webview_ret, GtkWidget **sw_ret);

/**
 * Creates a small button
 *
 * @image:   A button image.
 *
 * Returns:   A GtkButton created from the image.
 */
GtkWidget *pidgin_create_small_button(GtkWidget *image);

/**
 * Creates a new window
 *
 * @title:        The window title, or @c NULL
 * @border_width: The window's desired border width
 * @role:         A string indicating what the window is responsible for doing, or @c NULL
 * @resizable:    Whether the window should be resizable (@c TRUE) or not (@c FALSE)
 */
GtkWidget *pidgin_create_window(const char *title, guint border_width, const char *role, gboolean resizable);

/**
 * Creates a new dialog window
 *
 * @title:        The window title, or @c NULL
 * @border_width: The window's desired border width
 * @role:         A string indicating what the window is responsible for doing, or @c NULL
 * @resizable:    Whether the window should be resizable (@c TRUE) or not (@c FALSE)
 */
GtkWidget *pidgin_create_dialog(const char *title, guint border_width, const char *role, gboolean resizable);

/**
 * Retrieves the main content box (vbox) from a pidgin dialog window
 *
 * @dialog:       The dialog window
 * @homogeneous:  TRUE if all children are to be given equal space allotments.
 * @spacing:      the number of pixels to place by default between children
 */
GtkWidget *pidgin_dialog_get_vbox_with_properties(GtkDialog *dialog, gboolean homogeneous, gint spacing);

/**
 * Retrieves the main content box (vbox) from a pidgin dialog window
 *
 * @dialog:       The dialog window
 */
GtkWidget *pidgin_dialog_get_vbox(GtkDialog *dialog);

/**
 * Add a button to a dialog created by #pidgin_create_dialog.
 *
 * @dialog:         The dialog window
 * @label:          The stock-id or the label for the button
 * @callback:       The callback function for the button
 * @callbackdata:   The user data for the callback function
 *
 * Returns: The created button.
 */
GtkWidget *pidgin_dialog_add_button(GtkDialog *dialog, const char *label,
		GCallback callback, gpointer callbackdata);

/**
 * Retrieves the action area (button box) from a pidgin dialog window
 *
 * @dialog:       The dialog window
 */
GtkWidget *pidgin_dialog_get_action_area(GtkDialog *dialog);

/**
 * Toggles the sensitivity of a widget.
 *
 * @widget:    @c NULL. Used for signal handlers.
 * @to_toggle: The widget to toggle.
 */
void pidgin_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Checks if text has been entered into a GtkTextEntry widget.  If
 * so, the GTK_RESPONSE_OK on the given dialog is set to TRUE.
 * Otherwise GTK_RESPONSE_OK is set to FALSE.
 *
 * @entry:  The text entry widget.
 * @dialog: The dialog containing the text entry widget.
 */
void pidgin_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog);

/**
 * Toggles the sensitivity of all widgets in a pointer array.
 *
 * @param w    @c NULL. Used for signal handlers.
 * @data: The array containing the widgets to toggle.
 */
void pidgin_toggle_sensitive_array(GtkWidget *w, GPtrArray *data);

/**
 * Toggles the visibility of a widget.
 *
 * @widget:    @c NULL. Used for signal handlers.
 * @to_toggle: The widget to toggle.
 */
void pidgin_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Adds a separator to a menu.
 *
 * @menu: The menu to add a separator to.
 *
 * Returns: The separator.
 */
GtkWidget *pidgin_separator(GtkWidget *menu);

/**
 * Creates a menu item.
 *
 * @menu: The menu to which to append the menu item.
 * @str:  The title to use for the newly created menu item.
 *
 * Returns: The newly created menu item.
 */
GtkWidget *pidgin_new_item(GtkWidget *menu, const char *str);

/**
 * Creates a check menu item.
 *
 * @menu:     The menu to which to append the check menu item.
 * @str:      The title to use for the newly created menu item.
 * @cb:       A function to call when the menu item is activated.
 * @data:     Data to pass to the signal function.
 * @checked:  The initial state of the check item
 *
 * Returns: The newly created menu item.
 */
GtkWidget *pidgin_new_check_item(GtkWidget *menu, const char *str,
		GCallback cb, gpointer data, gboolean checked);

/**
 * Creates a menu item.
 *
 * @menu:       The menu to which to append the menu item.
 * @str:        The title for the menu item.
 * @icon:       An icon to place to the left of the menu item,
 *                   or @c NULL for no icon.
 * @cb:         A function to call when the menu item is activated.
 * @data:       Data to pass to the signal function.
 * @accel_key:  Something.
 * @accel_mods: Something.
 * @mod:        Something.
 *
 * Returns: The newly created menu item.
 */
GtkWidget *pidgin_new_item_from_stock(GtkWidget *menu, const char *str,
									const char *icon, GCallback cb,
									gpointer data, guint accel_key,
									guint accel_mods, char *mod);

/**
 * Creates a button with the specified text and stock icon.
 *
 * @text:  The text for the button.
 * @icon:  The stock icon name.
 * @style: The orientation of the button.
 *
 * Returns: The button.
 */
GtkWidget *pidgin_pixbuf_button_from_stock(const char *text, const char *icon,
										 PidginButtonOrientation style);

/**
 * Creates a toolbar button with the stock icon.
 *
 * @stock: The stock icon name.
 *
 * Returns: The button.
 */
GtkWidget *pidgin_pixbuf_toolbar_button_from_stock(const char *stock);

/**
 * Creates a HIG preferences frame.
 *
 * @parent: The widget to put the frame into.
 * @title:  The title for the frame.
 *
 * Returns: The vbox to put things into.
 */
GtkWidget *pidgin_make_frame(GtkWidget *parent, const char *title);

/**
 * Creates a drop-down option menu filled with protocols.
 *
 * @id:        The protocol to select by default.
 * @cb:        The callback to call when a protocol is selected.
 * @user_data: Data to pass to the callback function.
 *
 * Returns: The drop-down option menu.
 */
GtkWidget *pidgin_protocol_option_menu_new(const char *id,
											 GCallback cb,
											 gpointer user_data);

/**
 * Gets the currently selected protocol from a protocol drop down box.
 *
 * @optmenu: The drop-down option menu created by
 *        pidgin_account_option_menu_new.
 * Returns: Returns the protocol ID that is currently selected.
 */
const char *pidgin_protocol_option_menu_get_selected(GtkWidget *optmenu);

/**
 * Creates a drop-down option menu filled with accounts.
 *
 * @default_account: The account to select by default.
 * @show_all:        Whether or not to show all accounts, or just
 *                        active accounts.
 * @cb:              The callback to call when an account is selected.
 * @filter_func:     A function for checking if an account should
 *                        be shown. This can be NULL.
 * @user_data:       Data to pass to the callback function.
 *
 * Returns: The drop-down option menu.
 */
GtkWidget *pidgin_account_option_menu_new(PurpleAccount *default_account,
		gboolean show_all, GCallback cb,
		PurpleFilterAccountFunc filter_func, gpointer user_data);

/**
 * Gets the currently selected account from an account drop down box.
 *
 * @optmenu: The drop-down option menu created by
 *        pidgin_account_option_menu_new.
 * Returns: Returns the PurpleAccount that is currently selected.
 */
PurpleAccount *pidgin_account_option_menu_get_selected(GtkWidget *optmenu);

/**
 * Sets the currently selected account for an account drop down box.
 *
 * @optmenu: The GtkOptionMenu created by
 *        pidgin_account_option_menu_new.
 * @account: The PurpleAccount to select.
 */
void pidgin_account_option_menu_set_selected(GtkWidget *optmenu, PurpleAccount *account);

/**
 * Add autocompletion of screenames to an entry, supporting a filtering function.
 *
 * @entry:       The GtkEntry on which to setup autocomplete.
 * @optmenu:     A menu for accounts, returned by pidgin_account_option_menu_new().
 *                    If @a optmenu is not @c NULL, it'll be updated when a username is chosen
 *                    from the autocomplete list.
 * @filter_func: A function for checking if an autocomplete entry
 *                    should be shown. This can be @c NULL.
 * @user_data:  The data to be passed to the filter_func function.
 */
void pidgin_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *optmenu, PidginFilterBuddyCompletionEntryFunc filter_func, gpointer user_data);

/**
 * The default filter function for username autocomplete.
 *
 * @completion_entry: The completion entry to filter.
 * @all_accounts:  If this is @c FALSE, only the autocompletion entries
 *                         which belong to an online account will be filtered.
 * Returns: Returns @c TRUE if the autocompletion entry is filtered.
 */
gboolean pidgin_screenname_autocomplete_default_filter(const PidginBuddyCompletionEntry *completion_entry, gpointer all_accounts);

/**
 * Sets up GtkSpell for the given GtkTextView, reporting errors
 * if encountered.
 *
 * This does nothing if Pidgin is not compiled with GtkSpell support.
 *
 * @textview: The textview widget to setup spellchecking for.
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
 * @conn:   The connection to get information from.
 * @name:   The user to get information about.
 */
void pidgin_retrieve_user_info(PurpleConnection *conn, const char *name);

/**
 * Get information about a user in a chat. Show immediate feedback.
 *
 * @conn:   The connection to get information from.
 * @name:   The user to get information about.
 * @chatid: The chat id.
 */
void pidgin_retrieve_user_info_in_chat(PurpleConnection *conn, const char *name, int chatid);

/**
 * Parses an application/x-im-contact MIME message and returns the
 * data inside.
 *
 * @msg:          The MIME message.
 * @all_accounts: If TRUE, check all compatible accounts, online or
 *                     offline. If FALSE, check only online accounts.
 * @ret_account:  The best guess at a compatible protocol,
 *                     based on ret_protocol. If NULL, no account was found.
 * @ret_protocol: The returned protocol type.
 * @ret_username: The returned username.
 * @ret_alias:    The returned alias.
 *
 * Returns: TRUE if the message was parsed for the minimum necessary data.
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
 */
void pidgin_set_accessible_relations(GtkWidget *w, GtkWidget *l);

/**
 * A helper function for GtkMenuPositionFuncs. This ensures the menu will
 * be kept on screen if possible.
 *
 * @menu: The menu we are positioning.
 * @param x Address of the gint representing the horizontal position
 *        where the menu shall be drawn. This is an output parameter.
 * @param y Address of the gint representing the vertical position
 *        where the menu shall be drawn. This is an output parameter.
 * @push_in: This is an output parameter?
 * @data: Not used by this particular position function.
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
 * @menu: The menu we are positioning.
 * @param x Address of the gint representing the horizontal position
 *        where the menu shall be drawn. This is an output parameter.
 * @param y Address of the gint representing the vertical position
 *        where the menu shall be drawn. This is an output parameter.
 * @push_in: This is an output parameter?
 * @user_data: Not used by this particular position function.
 */
void pidgin_treeview_popup_menu_position_func(GtkMenu *menu,
												gint *x,
												gint *y,
												gboolean *push_in,
												gpointer user_data);

/**
 * Manages drag'n'drop of files.
 *
 * @sd: GtkSelectionData for managing drag'n'drop
 * @account: Account to be used (may be NULL if conv is not NULL)
 * @who: Buddy name (may be NULL if conv is not NULL)
 */
void pidgin_dnd_file_manage(GtkSelectionData *sd, PurpleAccount *account, const char *who);

/**
 * Convenience wrapper for purple_buddy_icon_spec_get_scaled_size
 */
void pidgin_buddy_icon_get_scale_size(GdkPixbuf *buf, PurpleBuddyIconSpec *spec, PurpleIconScaleRules rules, int *width, int *height);

/**
 * Returns the base image to represent the account, based on
 * the currently selected theme.
 *
 * @account:      The account.
 * @size:         The size of the icon to return.
 *
 * Returns: A newly-created pixbuf with a reference count of 1,
 *         or NULL if any of several error conditions occurred:
 *         the file could not be opened, there was no loader
 *         for the file's format, there was not enough memory
 *         to allocate the image buffer, or the image file
 *         contained invalid data.
 */
GdkPixbuf *pidgin_create_protocol_icon(PurpleAccount *account, PidginProtocolIconSize size);

/**
 * Creates a status icon for a given primitve
 *
 * @primitive:  The status primitive
 * @param w          The widget to render this
 * @size:       The icon size to render at
 * Returns: A GdkPixbuf, created from stock
 */
GdkPixbuf * pidgin_create_status_icon(PurpleStatusPrimitive primitive, GtkWidget *w, const char *size);

/**
 * Returns an appropriate stock-id for a status primitive.
 *
 * @prim:   The status primitive
 *
 * Returns: The stock-id
 */
const char *pidgin_stock_id_from_status_primitive(PurpleStatusPrimitive prim);

/**
 * Returns an appropriate stock-id for a PurplePresence.
 *
 * @presence:   The presence.
 *
 * Returns: The stock-id
 */
const char *pidgin_stock_id_from_presence(PurplePresence *presence);

/**
 * Append a PurpleMenuAction to a menu.
 *
 * @menu:    The menu to append to.
 * @act:     The PurpleMenuAction to append.
 * @gobject: The object to be passed to the action callback.
 *
 * Returns:   The menuitem added.
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
 * @widget:      The widget for which to set the mouse pointer
 * @cursor_type: The type of cursor to set
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
 * @parent:      The parent window
 * @callback:    The callback to call when the window is closed. If the user chose an icon, the char* argument will point to its path
 * @data:        Data to pass to @a callback
 * Returns:            The file dialog
 */
GtkWidget *pidgin_buddy_icon_chooser_new(GtkWindow *parent, void(*callback)(const char*,gpointer), gpointer data);

/**
 * Converts a buddy icon to the required size and format
 *
 * @protocol:   The protocol to convert the icon
 * @path:       The path of a file to convert
 * @len:        If not @c NULL, the length of the returned data will be set here.
 *
 * Returns:           The converted image data, or @c NULL if an error occurred.
 */
gpointer pidgin_convert_buddy_icon(PurpleProtocol *protocol, const char *path, size_t *len);

/**
 * Converts "->" and "<-" in strings to Unicode arrow characters, for use in referencing
 * menu items.
 *
 * @str:      The text to convert
 * Returns:         A newly allocated string with unicode arrow characters
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
 * @handle:         The #PurpleConnection to which this mini-dialog
 *                       refers, or @c NULL if it does not refer to a
 *                       connection.  If @a handle is supplied, the mini-dialog
 *                       will be automatically removed and destroyed when the
 *                       connection signs off.
 * @stock_id:       The ID of a stock image to use in the mini dialog.
 * @primary:        The primary text
 * @secondary:      The secondary text, or @c NULL for no description.
 * @user_data:      Data to pass to the callbacks
 * @...:            a <tt>NULL</tt>-terminated list of button labels
 *                       (<tt>char *</tt>) and callbacks
 *                       (#PidginUtilMiniDialogCallback).  @a user_data will be
 *                       passed as the first argument.  (Callbacks may lack a
 *                       second argument, or be @c NULL to take no action when
 *                       the corresponding button is pressed.) When a button is
 *                       pressed, the callback (if any) will be called; when
 *                       the callback returns the dialog will be destroyed.
 * Returns:               A #PidginMiniDialog, suitable for passing to
 *                       pidgin_blist_add_alert().
 * @see pidginstock.h
 */
GtkWidget *pidgin_make_mini_dialog(PurpleConnection *handle,
	const char* stock_id, const char *primary, const char *secondary,
	void *user_data, ...) G_GNUC_NULL_TERMINATED;

/**
 * Does exactly what pidgin_make_mini_dialog() does, except you can specify
 * a custom icon for the dialog.
 */
GtkWidget *pidgin_make_mini_dialog_with_custom_icon(PurpleConnection *gc,
	GdkPixbuf *custom_icon,
	const char *primary,
	const char *secondary,
	void *user_data,
	...) G_GNUC_NULL_TERMINATED;

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
 * @window:  The window to draw attention to
 * @urgent:  Whether to set the urgent hint or not
 */
void pidgin_set_urgent(GtkWindow *window, gboolean urgent);

/**
 * Returns TRUE if the GdkPixbuf is opaque, as determined by no
 * alpha at any of the edge pixels.
 *
 * @pixbuf:  The pixbug
 * Returns: TRUE if the pixbuf is opaque around the edges, FALSE otherwise
 */
gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf);

/**
 * Rounds the corners of a 32x32 GdkPixbuf in place
 *
 * @pixbuf:  The buddy icon to transform
 */
void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf);

/**
 * Returns an HTML-style color string for use as a dim grey
 * string
 *
 * @widget:  The widget to return dim grey for
 * Returns: The dim grey string
 */
const char *pidgin_get_dim_grey_string(GtkWidget *widget);

/**
 * Create a simple text GtkComboBoxEntry equivalent
 *
 * @default_item:   Initial contents of GtkEntry
 * @items:          GList containing strings to add to GtkComboBox
 *
 * Returns:               A newly created text GtkComboBox containing a GtkEntry
 *                       child.
 */
GtkWidget *pidgin_text_combo_box_entry_new(const char *default_item, GList *items);

/**
 * Retrieve the text from the entry of the simple text GtkComboBoxEntry equivalent
 *
 * @widget:         The simple text GtkComboBoxEntry equivalent widget
 *
 * Returns:               The text in the widget's entry. It must not be freed
 */
const char *pidgin_text_combo_box_entry_get_text(GtkWidget *widget);

/**
 * Set the text in the entry of the simple text GtkComboBoxEntry equivalent
 *
 * @widget:         The simple text GtkComboBoxEntry equivalent widget
 * @text:           The text to set
 */
void pidgin_text_combo_box_entry_set_text(GtkWidget *widget, const char *text);

/**
 * Automatically make a window transient to a suitable parent window.
 *
 * @window:    The window to make transient.
 *
 * Returns: Whether the window was made transient or not.
 */
gboolean pidgin_auto_parent_window(GtkWidget *window);

/**
 * Add a labelled widget to a GtkVBox
 *
 * @vbox:         The GtkVBox to add the widget to.
 * @widget_label: The label to give the widget, can be @c NULL.
 * @sg:           The GtkSizeGroup to add the label to, can be @c NULL.
 * @widget:       The GtkWidget to add.
 * @expand:       Whether to expand the widget horizontally.
 * @p_label:      Place to store a pointer to the GtkLabel, or @c NULL if you don't care.
 *
 * Returns:  A GtkHBox already added to the GtkVBox containing the GtkLabel and the GtkWidget.
 */
GtkWidget *pidgin_add_widget_to_vbox(GtkBox *vbox, const char *widget_label, GtkSizeGroup *sg, GtkWidget *widget, gboolean expand, GtkWidget **p_label);

/**
 * Create a GdkPixbuf from a chunk of image data.
 *
 * @buf: The raw binary image data.
 * @count: The length of buf in bytes.
 *
 * Returns: A GdkPixbuf created from the image data, or NULL if
 *         there was an error parsing the data.
 */
GdkPixbuf *pidgin_pixbuf_from_data(const guchar *buf, gsize count);

/**
 * Create a GdkPixbufAnimation from a chunk of image data.
 *
 * @buf: The raw binary image data.
 * @count: The length of buf in bytes.
 *
 * Returns: A GdkPixbufAnimation created from the image data, or NULL if
 *         there was an error parsing the data.
 */
GdkPixbufAnimation *pidgin_pixbuf_anim_from_data(const guchar *buf, gsize count);

/**
 * Create a GdkPixbuf from a PurpleStoredImage.
 *
 * @image:   A PurpleStoredImage.
 *
 * Returns:   A GdkPixbuf created from the stored image.
 */
GdkPixbuf *pidgin_pixbuf_from_imgstore(PurpleStoredImage *image);

/**
 * Helper function that calls gdk_pixbuf_new_from_file() and checks both
 * the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * @filename: Name of file to load, in the GLib file name encoding
 *
 * Returns: The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file(const char *filename);

/**
 * Helper function that calls gdk_pixbuf_new_from_file_at_size() and checks
 * both the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file_at_size() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * @filename: Name of file to load, in the GLib file name encoding
 * @width: The width the image should have or -1 to not constrain the width
 * @height: The height the image should have or -1 to not constrain the height
 *
 * Returns: The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file_at_size(const char *filename, int width, int height);

/**
 * Helper function that calls gdk_pixbuf_new_from_file_at_scale() and checks
 * both the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file_at_scale() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * @filename: Name of file to load, in the GLib file name encoding
 * @width: The width the image should have or -1 to not constrain the width
 * @height: The height the image should have or -1 to not constrain the height
 * @preserve_aspect_ratio: TRUE to preserve the image's aspect ratio
 *
 * Returns: The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, gboolean preserve_aspect_ratio);

/**
 * Add scrollbars to a widget
 * @child:              The child widget
 * @hscrollbar_policy:  Horizontal scrolling policy
 * @vscrollbar_policy:  Vertical scrolling policy
 * @shadow_type:        Shadow type
 * @width:              Desired widget width, or -1 for default
 * @height:             Desired widget height, or -1 for default
 */
GtkWidget *pidgin_make_scrollable(GtkWidget *child, GtkPolicyType hscrollbar_policy, GtkPolicyType vscrollbar_policy, GtkShadowType shadow_type, int width, int height);

/**
 * Initialize some utility functions.
 */
void pidgin_utils_init(void);

/**
 * Uninitialize some utility functions.
 */
void pidgin_utils_uninit(void);

G_END_DECLS

#endif /* _PIDGINUTILS_H_ */

