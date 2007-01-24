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
#include "wgaimerror.h"
#include "libc_interface.h"

/* rpcndr.h defines small as char, causing problems, so we need to undefine it */
#ifdef _WIN32
#undef small
#endif

/*
 *  PROTOS
 */

/**
 ** win32dep.c
 **/
/* Windows helper functions */
FARPROC wgaim_find_and_loadproc(const char *dllname, const char *procedure);
char *wgaim_read_reg_string(HKEY rootkey, const char *subkey, const char *valname); /* needs to be g_free'd */
gboolean wgaim_write_reg_string(HKEY rootkey, const char *subkey, const char *valname, const char *value);
char *wgaim_escape_dirsep(const char *filename); /* needs to be g_free'd */
GIOChannel *wgaim_g_io_channel_win32_new_socket(int socket); /* Until we get the post-2.8 glib win32 giochannel implementation working, use the thread-based one */
/** Check for changes to the system proxy settings and update the HTTP_PROXY env. var. if there have been changes */
gboolean wgaim_check_for_proxy_changes(void);

/* Determine Gaim paths */
char *wgaim_get_special_folder(int folder_type); /* needs to be g_free'd */
const char *wgaim_install_dir(void);
const char *wgaim_lib_dir(void);
const char *wgaim_locale_dir(void);
const char *wgaim_data_dir(void);

/* init / cleanup */
void wgaim_init(void);
void wgaim_cleanup(void);


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

