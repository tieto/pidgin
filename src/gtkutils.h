/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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

#ifndef _GAIM_GTK_UTILS_H_
#define _GAIM_GTK_UTILS_H_

#include "gaim.h"
#include "conversation.h"
#include "prpl.h"

/**
 * Sets up a gtkimhtml widget, loads it with smileys, and sets the
 * default signal handlers.
 *
 * @param imhtml The gtkimhtml widget to setup.
 */
void gaim_setup_imhtml(GtkWidget *imhtml);

/**
 * Surrounds the selected text in a conversation with the specified
 * pre and post strings.
 *
 * @param gtkconv The GTK+ conversation.
 * @param pre     The prefix string.
 * @param post    The postfix string.
 */
void gaim_gtk_surround(struct gaim_gtk_conversation *gtkconv,
					   const char *pre, const char *post);

/**
 * Advances the cursor past the position of the specified tags.
 *
 * @param gtkconv The GTK+ conversation.
 * @param pre     The prefix string.
 * @param post    The postfix string.
 */
void gaim_gtk_advance_past(struct gaim_gtk_conversation *gtkconv,
						   const char *pre, const char *post);

/**
 * Surrounds the selected text with the specified font.
 *
 * @param conv The conversation.
 * @param font The new font.
 */
void gaim_gtk_set_font_face(struct gaim_gtk_conversation *gtkconv,
							const char *font);

/**
 * Displays a dialog for saving the buddy icon in a conversation.
 *
 * @param obj  @c NULL
 * @param conv The conversation.
 */
void gaim_gtk_save_icon_dialog(GtkObject *obj, struct gaim_conversation *conv);

/**
 * Returns the display style for buttons for the specified conversation
 * type.
 *
 * @param type The conversation type.
 *
 * @return The display style.
 */
int gaim_gtk_get_dispstyle(GaimConversationType type);

/**
 * Changes a button to be either text or image, depending on
 * preferences.
 *
 * This function destroys the old button pointed to by @a button and
 * returns the new replacement button.
 *
 * @param text   The text for the button.
 * @param button The button widget.
 * @param stock  The stock image.
 * @param type   The conversation type the button belongs to.
 *
 * @return The new button widget to replace the old one.
 */
GtkWidget *gaim_gtk_change_text(const char *text, GtkWidget *button,
								const char *stock, GaimConversationType type);

/**
 * Toggles the sensitivity of a widget.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The widget to toggle.
 */
void gaim_gtk_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle);

/**
 * Toggles the sensitivity of all widgets in a pointer array.
 *
 * @param widget    @c NULL. Used for signal handlers.
 * @param to_toggle The array containing the widgets to toggle.
 */
void gtk_toggle_sensitive_array(GtkWidget *w, GPtrArray *data);

/**
 * Adds a seperator to a menu.
 *
 * @param menu The menu to add a seperator to.
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
 * @param protocol  The protocol to select by default.
 * @param cb        The callback to call when a protocol is selected.
 * @param user_data Data to pass to the callback function.
 *
 * @return The drop-down option menu.
 */
GtkWidget *gaim_gtk_protocol_option_menu_new(GaimProtocol protocol,
											 GCallback cb,
											 gpointer user_data);

#endif /* _GAIM_GTK_UTILS_H_ */
