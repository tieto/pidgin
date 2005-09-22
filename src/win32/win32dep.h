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
#include <shlobj.h>
#include <winsock2.h>
#include <process.h>
#include <gtk/gtk.h>
#include <gdk/gdkevents.h>
#include "wgaimerror.h"
#include "libc_interface.h"

/*
 *  PROTOS
 */

/**
 ** win32dep.c
 **/
/* Windows helper functions */
extern HINSTANCE wgaim_hinstance(void);
extern FARPROC   wgaim_find_and_loadproc(char*, char*);
extern gboolean  wgaim_read_reg_string(HKEY key, char* sub_key, char* val_name, LPBYTE data, LPDWORD data_len);
extern char*     wgaim_escape_dirsep(char*);
/* Determine Gaim paths */
extern char*     wgaim_get_special_folder(int folder_type); /* needs to be g_free'd */
extern char*     wgaim_install_dir(void);
extern char*     wgaim_lib_dir(void);
extern char*     wgaim_locale_dir(void);
extern char*     wgaim_data_dir(void);
/* Utility */
extern int       wgaim_gz_decompress(const char* in, const char* out);
extern int       wgaim_gz_untar(const char* filename, const char* destdir);
/* Docklet */
extern void      wgaim_systray_minimize( GtkWidget* );
extern void      wgaim_systray_maximize( GtkWidget* );
/* Misc */
extern void      wgaim_notify_uri(const char *uri);
/* init / cleanup */
extern void      wgaim_init(HINSTANCE);
extern void      wgaim_cleanup(void);

/*
 *  MACROS
 */

/*
 *  Gaim specific
 */
#define DATADIR wgaim_install_dir()
#define LIBDIR wgaim_lib_dir()
#define LOCALEDIR wgaim_locale_dir()

#endif /* _WIN32DEP_H_ */

