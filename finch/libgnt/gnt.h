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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_H
#define GNT_H
/**
 * SECTION:gnt
 * @section_id: libgnt-gnt
 * @short_description: <filename>gnt.h</filename>
 * @title: GNT API
 */

#include <glib.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#ifdef _WIN32
#  undef KEY_EVENT
#endif
#ifdef NO_WIDECHAR
#  define NCURSES_WIDECHAR 0
#else
#  define NCURSES_WIDECHAR 1
#endif
#include <ncurses.h>

#include "gntwidget.h"
#include "gntclipboard.h"
#include "gntcolors.h"
#include "gntkeys.h"

/**
 * gnt_init:
 *
 * Initialize GNT.
 */
void gnt_init(void);

/**
 * gnt_set_config_dir:
 * @config_dir: the path to a configuration directory, may be %NULL.
 *
 * Change directory to store gnt configuration files (default is ~).
 *
 * You have to call this before #gnt_init. You might want to call this
 * with %NULL, to free the resources, but not before a call to #gnt_quit.
 */
void gnt_set_config_dir(const gchar *config_dir);

/**
 * gnt_main:
 *
 * Start running the mainloop for gnt.
 */
void gnt_main(void);

/**
 * gnt_ascii_only:
 *
 * Check whether the terminal is capable of UTF8 display.
 *
 * Returns:  %FALSE if the terminal is capable of drawing UTF-8, %TRUE otherwise.
 */
gboolean gnt_ascii_only(void);

/**
 * gnt_window_present:
 * @window:   The window the present.
 *
 * Present a window. If the event was triggered because of user interaction,
 * the window is moved to the foreground. Otherwise, the Urgent hint is set.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_window_present(GntWidget *window);

/**
 * gnt_screen_occupy:
 *
 * Internal function -- do not use.
 * Use gnt_widget_show() instead.
 */
void gnt_screen_occupy(GntWidget *widget);

/**
 * gnt_screen_release:
 *
 * Internal function -- do not use.
 * Use gnt_widget_hide() instead.
 */
void gnt_screen_release(GntWidget *widget);

/**
 * gnt_screen_update:
 *
 * Internal function -- do not use.
 * Use gnt_widget_draw() instead.
 */
void gnt_screen_update(GntWidget *widget);

/**
 * gnt_screen_resize_widget:
 * @widget:  The widget to resize.
 * @width:   The desired width.
 * @height:  The desired height.
 *
 * Resize a widget.
 */
void gnt_screen_resize_widget(GntWidget *widget, int width, int height);

/**
 * gnt_screen_move_widget:
 * @widget: The widget to move.
 * @x:      The desired x-coordinate.
 * @y:      The desired y-coordinate.
 *
 * Move a widget.
 */
void gnt_screen_move_widget(GntWidget *widget, int x, int y);

/**
 * gnt_screen_rename_widget:
 * @widget:  The widget to rename.
 * @text:    The new name for the widget.
 *
 * Rename a widget.
 */
void gnt_screen_rename_widget(GntWidget *widget, const char *text);

/**
 * gnt_widget_has_focus:
 * @widget:  The widget.
 *
 * Check whether a widget has focus.
 *
 * Returns:  %TRUE if the widget has the current focus, %FALSE otherwise.
 */
gboolean gnt_widget_has_focus(GntWidget *widget);

/**
 * gnt_widget_set_urgent:
 * @widget:  The widget to set the URGENT hint for.
 *
 * Set the URGENT hint for a widget.
 */
void gnt_widget_set_urgent(GntWidget *widget);

/**
 * gnt_register_action:
 * @label:      The user-visible label for the action.
 * @callback:   The callback function for the action.
 *
 * Register a global action.
 */
void gnt_register_action(const char *label, void (*callback)(void));

/**
 * gnt_screen_menu_show:
 * @menu:  The menu to display.
 *
 * Show a menu.
 *
 * Returns: %TRUE if the menu is displayed, %FALSE otherwise (e.g., if another menu is currently displayed).
 */
gboolean gnt_screen_menu_show(gpointer menu);

/**
 * gnt_quit:
 *
 * Terminate the mainloop of gnt.
 */
void gnt_quit(void);

/**
 * gnt_get_clipboard:
 *
 * Get the global clipboard.
 *
 * Returns: (transfer none): The clipboard.
 */
GntClipboard * gnt_get_clipboard(void);

/**
 * gnt_get_clipboard_string:
 *
 * Get the string in the clipboard.
 *
 * Returns: A copy of the string in the clipboard. The caller
 *          must g_free() the string.
 */
gchar * gnt_get_clipboard_string(void);

/**
 * gnt_set_clipboard_string:
 * @string:  The new content of the new clipboard.
 *
 * Set the contents of the global clipboard.
 */
void gnt_set_clipboard_string(const gchar *string);

/**
 * gnt_giveup_console:
 * @wd:    The working directory for the new application.
 * @argv:  The argument vector.
 * @envp:  The environment, or %NULL.
 * @stin:  Location to store the child's stdin, or %NULL.
 * @stout: Location to store the child's stdout, or %NULL.
 * @sterr: Location to store the child's stderr, or %NULL.
 * @callback:   The callback to call after the child exits.
 * @data:  The data to pass to the callback.
 *
 * Spawn a different application that will consume the console.
 *
 * Returns:  %TRUE if the child was successfully spawned, %FALSE otherwise.
 */
gboolean gnt_giveup_console(const char *wd, char **argv, char **envp,
		gint *stin, gint *stout, gint *sterr,
		void (*callback)(int status, gpointer data), gpointer data);

/**
 * gnt_is_refugee:
 *
 * Check whether a child process is in control of the current terminal.
 *
 * Returns: %TRUE if a child process (eg., PAGER) is occupying the current
 *         terminal, %FALSE otherwise.
 */
gboolean gnt_is_refugee(void);

#endif /* GNT_H */
