/**
 * @file gtkwin32dep.h UI Win32 Specific Functionality
 * @ingroup win32
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution
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
#ifndef _GTKWIN32DEP_H_
#define _GTKWIN32DEP_H_
#include <windows.h>
#include <gtk/gtk.h>
#include "conversation.h"

HINSTANCE winpidgin_dll_hinstance(void);
HINSTANCE winpidgin_exe_hinstance(void);

/* Utility */
int winpidgin_gz_decompress(const char* in, const char* out);
int winpidgin_gz_untar(const char* filename, const char* destdir);

/* Misc */
void winpidgin_notify_uri(const char *uri);
void winpidgin_shell_execute(const char *target, const char *verb, const char *clazz);
void winpidgin_ensure_onscreen(GtkWidget *win);
void winpidgin_conv_blink(PurpleConversation *conv, PurpleMessageFlags flags);
void winpidgin_window_flash(GtkWindow *window, gboolean flash);

/* init / cleanup */
void winpidgin_init(HINSTANCE);
void winpidgin_post_init(void);
void winpidgin_cleanup(void);

#endif /* _GTKWIN32DEP_H_ */

