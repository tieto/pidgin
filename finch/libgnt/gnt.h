/**
 * @defgroup gnt GNT (GLib Ncurses Toolkit)
 *
 * GNT is an ncurses toolkit for creating text-mode graphical user interfaces
 * in a fast and easy way.
 */
/**
 * @file gnt.h GNT API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include <glib.h>
#include "gntwidget.h"
#include "gntclipboard.h"
#include "gntcolors.h"
#include "gntkeys.h"

/**
 * 
 */
void gnt_init(void);

/**
 * 
 */
void gnt_main(void);

/**
 * 
 *
 * @return
 */
gboolean gnt_ascii_only(void);

void gnt_window_present(GntWidget *window);
/**
 * 
 * @param widget
 */
void gnt_screen_occupy(GntWidget *widget);

/**
 * 
 * @param widget
 */
void gnt_screen_release(GntWidget *widget);

/**
 * 
 * @param widget
 */
void gnt_screen_update(GntWidget *widget);

/**
 * 
 * @param widget
 * @param width
 * @param height
 */
void gnt_screen_resize_widget(GntWidget *widget, int width, int height);

/**
 * 
 * @param widget
 * @param x
 * @param y
 */
void gnt_screen_move_widget(GntWidget *widget, int x, int y);

/**
 * 
 * @param widget
 * @param text
 */
void gnt_screen_rename_widget(GntWidget *widget, const char *text);

/**
 * 
 * @param widget
 *
 * @return
 */
gboolean gnt_widget_has_focus(GntWidget *widget);

/**
 * 
 * @param widget
 */
void gnt_widget_set_urgent(GntWidget *widget);

/**
 * 
 * @param label
 * @param callback
 */
void gnt_register_action(const char *label, void (*callback)());

/**
 * 
 * @param menu
 *
 * @return
 */
gboolean gnt_screen_menu_show(gpointer menu);

/**
 * 
 */
void gnt_quit(void);

/**
 * 
 *
 * @return
 */
GntClipboard * gnt_get_clipboard(void);

/**
 * 
 *
 * @return
 */
gchar * gnt_get_clipboard_string(void);

/**
 * 
 * @param string
 */
void gnt_set_clipboard_string(gchar *string);

/**
 * Spawn a different application that will consume the console.
 */
gboolean gnt_giveup_console(const char *wd, char **argv, char **envp,
		gint *stin, gint *stout, gint *sterr,
		void (*callback)(int status, gpointer data), gpointer data);

gboolean gnt_is_refugee(void);
