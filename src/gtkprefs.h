/**
 * @file gtkprefs.h GTK+ Preferences
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 1998-2002, Mark Spencer <markster@marko.net>
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
 *
 */
#ifndef _GAIM_GTK_PREFS_H_
#define _GAIM_GTK_PREFS_H_
#include "prefs.h"

/* XXX These should go. */
extern GtkListStore *prefs_away_store;
extern GtkWidget *prefs_away_menu;
extern GtkWidget *pref_fg_picture;
extern GtkWidget *pref_bg_picture;

/**
 * Initializes all UI-specific preferences.
 */
void gaim_gtk_prefs_init(void);

/**
 * Shows the preferences dialog.
 */
void gaim_gtk_prefs_show(void);

/**
 * Initializes the default away menu.
 *
 * @todo This should be moved or renamed or something?
 */
void default_away_menu_init(GtkWidget *);

void apply_font_dlg(GtkWidget *, GtkWidget *);
void apply_color_dlg(GtkWidget *, gpointer);
void destroy_colorsel(GtkWidget *, gpointer);

/**
 * Add a new checkbox for a boolean preference
 *
 * @param title The text to be displayed as the checkbox label
 * @param key   The key of the gaim bool pref that will be represented by the checkbox
 * @param page  The page to which the new checkbox will be added
 */
GtkWidget *prefs_checkbox(const char *title, const char *key,
                         GtkWidget *page);

/**
 * Add a new spin button representing an int preference
 *
 * @param page  The page to which the spin button will be added
 * @param title The text to be displayed as the spin button label
 * @param key   The key of the gaim int pref that will be represented by the spin button
 * @param min   The minimum value of the spin button
 * @param max   The maximum value of the spin button
 * @param sg    If not NULL, the size group to which the spin button will be added
 */
GtkWidget *prefs_labeled_spin_button(GtkWidget *page, 
                                    const gchar *title,
                                    char *key, int min, int max,
                                    GtkSizeGroup *sg);

/**
 * Add a new dropdown representing a preference of the specified type
 *
 * @param page  The page to which the spin button will be added
 * @param title The text to be displayed as the spin button label
 * @param type  The type of preference to be stored in the generated dropdown
 * @param key   The key of the gaim int pref that will be represented by the spin button
 * @param ...   The choices to be added to the dropdown
 */
GtkWidget *prefs_dropdown(GtkWidget *page, const gchar *title,
                         GaimPrefType type,
                         const char *key, ...);

/**
 * Add a new dropdown representing a preference of the specified type
 *
 * @param page      The page to which the spin button will be added
 * @param title     The text to be displayed as the spin button label
 * @param type      The type of preference to be stored in the generated dropdown
 * @param key       The key of the gaim int pref that will be represented by the spin button
 * @param menuitems The choices to be added to the dropdown
 */
GtkWidget *prefs_dropdown_from_list(GtkWidget *page,
                                   const gchar * title,
                                   GaimPrefType type, const char *key,
                                   GList *menuitems); 

#endif /* _GAIM_GTK_PREFS_H_ */
