/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKUTILS_H_
#define _GAIM_GTKUTILS_H_

#include "gtkconv.h"
#include "gtkgaim.h"
#include "prpl.h"
#include "util.h"

typedef enum
{
	GAIM_BUTTON_HORIZONTAL,
	GAIM_BUTTON_VERTICAL

} GaimButtonOrientation;

typedef enum
{
	GAIM_BUTTON_NONE = 0,
	GAIM_BUTTON_TEXT,
	GAIM_BUTTON_IMAGE,
	GAIM_BUTTON_TEXT_IMAGE

} GaimButtonStyle;

#ifndef _WIN32
typedef enum
{
	GAIM_BROWSER_DEFAULT = 0,
	GAIM_BROWSER_CURRENT,
	GAIM_BROWSER_NEW_WINDOW,
	GAIM_BROWSER_NEW_TAB

} GaimBrowserPlace;
#endif /* _WIN32 */

/**
 * Sets up a gtkimhtml widget, loads it with smileys, and sets the
 * default signal handlers.
 *
 * @param imhtml The gtkimhtml widget to setup.
 */
void gaim_setup_imhtml(GtkWidget *imhtml);

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
GtkWidget *gaim_gtk_create_imhtml(gboolean editable, GtkWidget **imhtml_ret, GtkWidget **toolbar_ret, GtkWidget **sw_ret);

/**
 * Toggles the sensitivity of a widget.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The widget to toggle.
 */
void gaim_gtk_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Checks if text has been entered into a GtkTextEntry widget.  If 
 * so, the GTK_RESPONSE_OK on the given dialog is set to TRUE.  
 * Otherwise GTK_RESPONSE_OK is set to FALSE.
 *
 * @param entry  The text entry widget.
 * @param dialog The dialog containing the text entry widget.
 */
void gaim_gtk_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog);

/**
 * Toggles the sensitivity of all widgets in a pointer array.
 *
 * @param w    @c NULL. Used for signal handlers.
 * @param data The array containing the widgets to toggle.
 */
void gaim_gtk_toggle_sensitive_array(GtkWidget *w, GPtrArray *data);

/**
 * Toggles the visibility of a widget.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The widget to toggle.
 */
void gaim_gtk_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Adds a separator to a menu.
 *
 * @param menu The menu to add a separator to.
 */
void gaim_separator(GtkWidget *menu);

/**
 * Creates a menu item.
 *
 * @param menu The menu to which to append the menu item.
 * @param str  The title to use for the newly created menu item.
 *
 * @return The newly created menu item.
 */
GtkWidget *gaim_new_item(GtkWidget *menu, const char *str);

/**
 * Creates a check menu item.
 *
 * @param menu     The menu to which to append the check menu item.
 * @param str      The title to use for the newly created menu item.
 * @param sf       A function to call when the menu item is activated.
 * @param data     Data to pass to the signal function.
 * @param checked  The initial state of the check item
 *
 * @return The newly created menu item.
 */
GtkWidget *gaim_new_check_item(GtkWidget *menu, const char *str,
		GtkSignalFunc sf, gpointer data, gboolean checked);

/**
 * Creates a menu item.
 *
 * @param menu       The menu to which to append the menu item.
 * @param str        The title for the menu item.
 * @param icon       An icon to place to the left of the menu item,
 *                   or @c NULL for no icon.
 * @param sf         A function to call when the menu item is activated.
 * @param data       Data to pass to the signal function.
 * @param accel_key  Something.
 * @param accel_mods Something.
 * @param mod        Something.
 *
 * @return The newly created menu item.
 */
GtkWidget *gaim_new_item_from_stock(GtkWidget *menu, const char *str,
									const char *icon, GtkSignalFunc sf,
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
GtkWidget *gaim_pixbuf_button_from_stock(const char *text, const char *icon,
										 GaimButtonOrientation style);

/**
 * Creates a toolbar button with the stock icon.
 *
 * @param stock The stock icon name.
 *
 * @return The button.
 */
GtkWidget *gaim_pixbuf_toolbar_button_from_stock(const char *stock);

/**
 * Creates a HIG preferences frame.
 *
 * @param parent The widget to put the frame into.
 * @param title  The title for the frame.
 *
 * @return The vbox to put things into.
 */
GtkWidget *gaim_gtk_make_frame(GtkWidget *parent, const char *title);

/**
 * Creates a drop-down option menu filled with protocols.
 *
 * @param id        The protocol to select by default.
 * @param cb        The callback to call when a protocol is selected.
 * @param user_data Data to pass to the callback function.
 *
 * @return The drop-down option menu.
 */
GtkWidget *gaim_gtk_protocol_option_menu_new(const char *id,
											 GCallback cb,
											 gpointer user_data);

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
GtkWidget *gaim_gtk_account_option_menu_new(GaimAccount *default_account,
		gboolean show_all, GCallback cb,
		GaimFilterAccountFunc filter_func, gpointer user_data);

/**
 * Gets the currently selected account from an account drop down box.
 *
 * @param optmenu The GtkOptionMenu created by
 *        gaim_gtk_account_option_menu_new.
 * @return Returns the GaimAccount that is currently selected.
 */
GaimAccount *gaim_gtk_account_option_menu_get_selected(GtkWidget *optmenu);

/**
 * Sets the currently selected account for an account drop down box.
 *
 * @param optmenu The GtkOptionMenu created by
 *        gaim_gtk_account_option_menu_new.
 * @param account The GaimAccount to select.
 */
void gaim_gtk_account_option_menu_set_selected(GtkWidget *optmenu, GaimAccount *account);

/**
 * Add autocompletion of screenames to an entry.
 *
 * @param entry     The GtkEntry on which to setup autocomplete.
 * @param optmenu   A menu for accounts, returned by gaim_gtk_account_option_menu_new().
 *                  If @a optmenu is not @c NULL, it'll be updated when a screenname is chosen
 *                  from the autocomplete list.
 * @param all       Whether to include screennames from disconnected accounts.
 */
void gaim_gtk_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *optmenu, gboolean all);

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
gboolean gaim_gtk_check_if_dir(const char *path, GtkFileSelection *filesel);

/**
 * Sets up GtkSpell for the given GtkTextView, reporting errors
 * if encountered.
 *
 * This does nothing if Gaim is not compiled with GtkSpell support.
 *
 * @param textview The textview widget to setup spellchecking for.
 */
void gaim_gtk_setup_gtkspell(GtkTextView *textview);

/**
 * Save menu accelerators callback
 */
void gaim_gtk_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
							 GdkModifierType arg2, GClosure *arg3,
							 gpointer data);

/**
 * Save menu accelerators
 */
gboolean gaim_gtk_save_accels(gpointer data);

/**
 * Load menu accelerators
 */
void gaim_gtk_load_accels(void);

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
gboolean gaim_gtk_parse_x_im_contact(const char *msg, gboolean all_accounts,
									 GaimAccount **ret_account,
									 char **ret_protocol, char **ret_username,
									 char **ret_alias);

/**
 * Sets an ATK name for a given widget.  Also sets the labelled-by 
 * and label-for ATK relationships.
 *
 * @param w The widget that we want to name.
 * @param l A GtkLabel that we want to use as the ATK name for the widget.
 */
void gaim_set_accessible_label(GtkWidget *w, GtkWidget *l);

/**
 * A valid GtkMenuPositionFunc.  This is used to determine where 
 * to draw context menu's when the menu is activated with the 
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
void gaim_gtk_treeview_popup_menu_position_func(GtkMenu *menu,
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
void gaim_dnd_file_manage(GtkSelectionData *sd, GaimAccount *account, const char *who);

/**
 * Convenience wrapper for gaim_buddy_icon_get_scale_size
 */
void gaim_gtk_buddy_icon_get_scale_size(GdkPixbuf *buf, GaimBuddyIconSpec *spec, int *width, int *height);

/**
 * Returns the base image to represent the account, based on
 * the currently selected theme.
 *
 * @param account      The account.
 * @param scale_factor The amount to scale to the original image.
 *                     The default size is 32x32 pixels.  A scale
 *                     factor of 1 means no scaling will be done.
 *                     A scale factor of 0.5 means the length
 *                     and width will be 16 pixels each.
 *
 * @return A newly-created pixbuf with a reference count of 1,
 *         or NULL if any of several error conditions occurred:
 *         the file could not be opened, there was no loader
 *         for the file's format, there was not enough memory
 *         to allocate the image buffer, or the image file
 *         contained invalid data.
 */
GdkPixbuf *gaim_gtk_create_prpl_icon(GaimAccount *account, double scale_factor);

/**
 * Create a protocol icon with the status emblem overlayed in
 * the lower right corner.
 *
 * @param account      The account.
 * @param status_type  The status type of the emblem to overlay.
 * @param scale_factor The amount to scale to the original image.
 *                     The default size is 32x32 pixels.  A scale
 *                     factor of 1 means no scaling will be done.
 *                     A scale factor of 0.5 means the length
 *                     and width will be 16 pixels each.
 *
 * @return A newly-created pixbuf with a reference count of 1,
 *         or NULL if any of several error conditions occurred:
 *         the file could not be opened, there was no loader
 *         for the file's format, there was not enough memory
 *         to allocate the image buffer, or the image file
 *         contained invalid data.
 */
GdkPixbuf *gaim_gtk_create_prpl_icon_with_status(GaimAccount *account, GaimStatusType *status_type, double scale_factor);

/**
 * Create a Gaim running-man icon with the status emblem overlayed
 * in the lower right corner.
 *
 * @param primitive    The status type to set the emblem for.
 * @param scale_factor The amount to scale to the original image.
 *                     The default size is 32x32 pixels.  A scale
 *                     factor of 1 means no scaling will be done.
 *                     A scale factor of 0.5 means the length
 *                     and width will be 16 pixels each.
 *
 * @return A newly-created pixbuf with a reference count of 1,
 *         or NULL if any of several error conditions occurred:
 *         the file could not be opened, there was no loader for
 *         the file's format, there was not enough memory to
 *         allocate the image buffer, or the image file contained
 *         invalid data.
 */
GdkPixbuf *gaim_gtk_create_gaim_icon_with_status(GaimStatusPrimitive primitive, double scale_factor);


/**
 * Append a GaimMenuAction to a menu.
 *
 * @param menu    The menu to append to.
 * @param act     The GaimMenuAction to append.
 * @param gobject The object to be passed to the action callback.
 *
 * @return The menu.
 */
void gaim_gtk_append_menu_action(GtkWidget *menu, GaimMenuAction *act,
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
void gaim_gtk_set_cursor(GtkWidget *widget, GdkCursorType cursor_type);

/**
 * Sets the mouse point for a GtkWidget back to that of its parent window.
 *
 * If @a widget is @c NULL, this function simply returns.
 *
 * If the window for @a widget is @c NULL, this function simply returns.
 *
 * @note The display is not flushed from this function.
 */
void gaim_gtk_clear_cursor(GtkWidget *widget);

#endif /* _GAIM_GTKUTILS_H_ */
