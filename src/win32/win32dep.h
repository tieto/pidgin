/*
 * gaim
 *
 * File: win32dep.h
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
#ifndef _WIN32DEP_H_
#define _WIN32DEP_H_
#include <winsock.h>
#include <process.h>
#include <gtk/gtk.h>
#include <gdk/gdkevents.h>
#include "wgaimerror.h"
#include "libc_interface.h"
#include "systray.h"

/*
 *  PROTOS
 */

/**
 ** win32dep.c
 **/
/* Windows helper functions */
HINSTANCE wgaim_hinstance(void);
FARPROC wgaim_find_and_loadproc(char*, char*);
/* Determine Gaim paths */
extern char* wgaim_install_dir(void);
extern char* wgaim_lib_dir(void);
extern char* wgaim_locale_dir(void);
extern char* wgaim_escape_dirsep(char*);
/* UI related */
extern void wgaim_im_blink(GtkWidget*);
extern void wgaim_gtk_window_move(GtkWindow *window, gint x, gint y);
/* Utility */
extern int wgaim_gz_decompress(const char* in, const char* out);
extern int wgaim_gz_untar(const char* filename, const char* destdir);
/* init / cleanup */
extern void wgaim_init(void);
extern void wgaim_cleanup(void);

/*
 *  MACROS
 */

/*
 *  Gaim specific
 */
#define DATADIR wgaim_install_dir()
#define LIBDIR wgaim_lib_dir()
#define LOCALEDIR wgaim_locale_dir()

/* Temp solution for gtk_window_get_pos & gtk_window_move conflict */
#define gtk_window_move( window, x, y ) \
wgaim_gtk_window_move( ## window ##, ## x ##, ## y ## )

#endif /* _WIN32DEP_H_ */

