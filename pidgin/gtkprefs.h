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
 *
 */
/**
 * SECTION:gtkprefs
 * @section_id: pidgin-gtkprefs
 * @short_description: <filename>gtkprefs.h</filename>
 * @title: Preferences
 */

#ifndef _PIDGINPREFS_H_
#define _PIDGINPREFS_H_

#include "prefs.h"

G_BEGIN_DECLS

/**
 * pidgin_prefs_init:
 *
 * Initializes all UI-specific preferences.
 */
void pidgin_prefs_init(void);

/**
 * pidgin_prefs_show:
 *
 * Shows the preferences dialog.
 */
void pidgin_prefs_show(void);

/**
 * pidgin_prefs_checkbox:
 * @title: The text to be displayed as the checkbox label
 * @key:   The key of the purple bool pref that will be represented by the checkbox
 * @page:  The page to which the new checkbox will be added
 *
 * Add a new checkbox for a boolean preference
 */
GtkWidget *pidgin_prefs_checkbox(const char *title, const char *key,
		GtkWidget *page);

/**
 * pidgin_prefs_labeled_spin_button:
 * @page:  The page to which the spin button will be added
 * @title: The text to be displayed as the spin button label
 * @key:   The key of the int pref that will be represented by the spin button
 * @min:   The minimum value of the spin button
 * @max:   The maximum value of the spin button
 * @sg:    If not NULL, the size group to which the spin button will be added
 *
 * Add a new spin button representing an int preference
 *
 * Returns:      An hbox containing both the label and the spinner.  Can be
 *              used to set the widgets to sensitive or insensitive based on the
 *              value of a checkbox.
 */
GtkWidget *pidgin_prefs_labeled_spin_button(GtkWidget *page,
		const gchar *title, const char *key, int min, int max, GtkSizeGroup *sg);

/**
 * pidgin_prefs_labeled_entry:
 * @page:  The page to which the entry will be added
 * @title: The text to be displayed as the entry label
 * @key:   The key of the string pref that will be represented by the entry
 * @sg:    If not NULL, the size group to which the entry will be added
 *
 * Add a new entry representing a string preference
 *
 * Returns:      An hbox containing both the label and the entry.  Can be used to set
 *               the widgets to sensitive or insensitive based on the value of a
 *               checkbox.
 */
GtkWidget *pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
										const char *key, GtkSizeGroup *sg);

/**
 * pidgin_prefs_labeled_password:
 * @page:  The page to which the entry will be added
 * @title: The text to be displayed as the entry label
 * @key:   The key of the string pref that will be represented by the entry
 * @sg:    If not NULL, the size group to which the entry will be added
 *
 * Add a new entry representing a password (string) preference
 * The entry will use a password-style text entry (the text is substituded)
 *
 * Returns:      An hbox containing both the label and the entry.  Can be used to set
 *               the widgets to sensitive or insensitive based on the value of a
 *               checkbox.
 */
GtkWidget *pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
										const char *key, GtkSizeGroup *sg);

/**
 * pidgin_prefs_dropdown:
 * @page:  The page to which the dropdown will be added
 * @title: The text to be displayed as the dropdown label
 * @type:  The type of preference to be stored in the generated dropdown
 * @key:   The key of the pref that will be represented by the dropdown
 * @...:   The choices to be added to the dropdown, choices should be
 *              paired as label/value
 *
 * Add a new dropdown representing a preference of the specified type
 */
GtkWidget *pidgin_prefs_dropdown(GtkWidget *page, const gchar *title,
		PurplePrefType type, const char *key, ...);

/**
 * pidgin_prefs_dropdown_from_list:
 * @page:      The page to which the dropdown will be added
 * @title:     The text to be displayed as the dropdown label
 * @type:      The type of preference to be stored in the dropdown
 * @key:       The key of the pref that will be represented by the dropdown
 * @menuitems: The choices to be added to the dropdown, choices should
 *                  be paired as label/value
 *
 * Add a new dropdown representing a preference of the specified type
 */
GtkWidget *pidgin_prefs_dropdown_from_list(GtkWidget *page,
		const gchar * title, PurplePrefType type, const char *key,
		GList *menuitems);

/**
 * pidgin_prefs_update_old:
 *
 * Rename legacy prefs and delete some that no longer exist.
 */
void pidgin_prefs_update_old(void);

G_END_DECLS

#endif /* _PIDGINPREFS_H_ */
