/**
 * @file tcl_win32.c Gaim Tcl Windows Init
 *
 * gaim
 *
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
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
 */
#include "internal.h"
#include "debug.h"
#include <tcl.h>

#ifdef HAVE_TK
#include <tk.h>
#endif

typedef Tcl_Interp* (CALLBACK* LPFNTCLCREATEINTERP)(void);
typedef void        (CALLBACK* LPFNTKINIT)(Tcl_Interp*);

LPFNTCLCREATEINTERP wtcl_CreateInterp = NULL;
LPFNTKINIT wtk_Init = NULL;

BOOL tcl_win32_init() {
        if(!(wtcl_CreateInterp=(LPFNTCLCREATEINTERP)wgaim_find_and_loadproc("tcl84.dll", "Tcl_CreateInterp"))) {
                gaim_debug(GAIM_DEBUG_INFO, "tcl", "tcl_win32_init error loading Tcl_CreateInterp\n");
                return FALSE;
        }

        if(!(wtk_Init=(LPFNTKINIT)wgaim_find_and_loadproc("tk84.dll", "Tk_Init"))) {
                gaim_debug(GAIM_DEBUG_INFO, "tcl", "tcl_win32_init error loading Tk_Init\n");
                return FALSE;
        }
        return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
        switch( fdwReason ) { 
        case DLL_PROCESS_ATTACH:
                return tcl_win32_init();

        case DLL_THREAD_ATTACH:
                break;

        case DLL_THREAD_DETACH:
                break;

        case DLL_PROCESS_DETACH:
                break;
    }
    return TRUE;
}


