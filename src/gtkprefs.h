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
 * Sets the default away message.
 *
 * @todo This should be moved or renamed or something?
 */
void set_default_away(GtkWidget *, gpointer);

/**
 * Initializes the default away menu.
 *
 * @todo This should be moved or renamed or something?
 */
void default_away_menu_init(GtkWidget *);

void apply_font_dlg(GtkWidget *, GtkWidget *);
void apply_color_dlg(GtkWidget *, gpointer);
void destroy_colorsel(GtkWidget *, gpointer);

#endif /* _GAIM_GTK_PREFS_H_ */
