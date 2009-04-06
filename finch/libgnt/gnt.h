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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef GNT_H
#define GNT_H

#include <glib.h>
#include "gntwidget.h"
#include "gntclipboard.h"
#include "gntcolors.h"
#include "gntkeys.h"

/**
 * Get things to compile in Glib < 2.8
 */
#if !GLIB_CHECK_VERSION(2,8,0)
	#define G_PARAM_STATIC_NAME  G_PARAM_PRIVATE
	#define G_PARAM_STATIC_NICK  G_PARAM_PRIVATE
	#define G_PARAM_STATIC_BLURB  G_PARAM_PRIVATE
#endif

#if !GLIB_CHECK_VERSION(2,14,0)
	#define g_timeout_add_seconds(time, callback, data)  g_timeout_add(time * 1000, callback, data)
#endif

/**
 * Initialize GNT.
 */
void gnt_init(void);

/**
 * Start running the mainloop for gnt.
 */
void gnt_main(void);

/**
 * Check whether the terminal is capable of UTF8 display.
 *
 * @return  @c FALSE if the terminal is capable of drawing UTF-8, @c TRUE otherwise.
 */
gboolean gnt_ascii_only(void);

/**
 * Present a window. If the event was triggered because of user interaction,
 * the window is moved to the foreground. Otherwise, the Urgent hint is set.
 *
 * @param window   The window the present.
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_window_present(GntWidget *window);

/**
 * @internal
 * Use #gnt_widget_show instead.
 */
void gnt_screen_occupy(GntWidget *widget);

/**
 * @internal
 * Use #gnt_widget_hide instead.
 */
void gnt_screen_release(GntWidget *widget);

/**
 * @internal
 * Use #gnt_widget_draw instead.
 */
void gnt_screen_update(GntWidget *widget);

/**
 * Resize a widget.
 *
 * @param widget  The widget to resize.
 * @param width   The desired width.
 * @param height  The desired height.
 */
void gnt_screen_resize_widget(GntWidget *widget, int width, int height);

/**
 * Move a widget.
 *
 * @param widget The widget to move.
 * @param x      The desired x-coordinate.
 * @param y      The desired y-coordinate.
 */
void gnt_screen_move_widget(GntWidget *widget, int x, int y);

/**
 * Rename a widget.
 *
 * @param widget  The widget to rename.
 * @param text    The new name for the widget.
 */
void gnt_screen_rename_widget(GntWidget *widget, const char *text);

/**
 * Check whether a widget has focus.
 *
 * @param widget  The widget.
 *
 * @return  @c TRUE if the widget has the current focus, @c FALSE otherwise.
 */
gboolean gnt_widget_has_focus(GntWidget *widget);

/**
 * Set the URGENT hint for a widget.
 *
 * @param widget  The widget to set the URGENT hint for.
 */
void gnt_widget_set_urgent(GntWidget *widget);

/**
 * Register a global action.
 *
 * @param label      The user-visible label for the action.
 * @param callback   The callback function for the action.
 */
void gnt_register_action(const char *label, void (*callback)(void));

/**
 * Show a menu.
 *
 * @param menu  The menu to display.
 *
 * @return @c TRUE if the menu is displayed, @c FALSE otherwise (e.g., if another menu is currently displayed).
 */
gboolean gnt_screen_menu_show(gpointer menu);

/**
 * Terminate the mainloop of gnt.
 */
void gnt_quit(void);

/**
 * Get the global clipboard.
 *
 * @return  The clipboard.
 */
GntClipboard * gnt_get_clipboard(void);

/**
 * Get the string in the clipboard.
 *
 * @return A copy of the string in the clipboard. The caller must @c g_free the string.
 */
gchar * gnt_get_clipboard_string(void);

/**
 * Set the contents of the global clipboard.
 *
 * @param string  The new content of the new clipboard.
 */
void gnt_set_clipboard_string(const gchar *string);

/**
 * Spawn a different application that will consume the console.
 *
 * @param wd    The working directory for the new application.
 * @param argv  The argument vector.
 * @param envp  The environment, or @c NULL.
 * @param stin  Location to store the child's stdin, or @c NULL.
 * @param stout Location to store the child's stdout, or @c NULL.
 * @param sterr Location to store the child's stderr, or @c NULL.
 * @param callback   The callback to call after the child exits.
 * @param data  The data to pass to the callback.
 *
 * @return  @c TRUE if the child was successfully spawned, @c FALSE otherwise.
 */
gboolean gnt_giveup_console(const char *wd, char **argv, char **envp,
		gint *stin, gint *stout, gint *sterr,
		void (*callback)(int status, gpointer data), gpointer data);

/**
 * Check whether a child process is in control of the current terminal.
 *
 * @return @c TRUE if a child process (eg., PAGER) is occupying the current
 *         terminal, @c FALSE otherwise.
 */
gboolean gnt_is_refugee(void);

#endif /* GNT_H */
