/*
 *  win32dep.h
 */

#ifndef _WIN32DEP_H_
#define _WIN32DEP_H_
#include <winsock.h>
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
/* Misc */
FARPROC wgaim_find_and_loadproc(char*, char*);
HINSTANCE wgaim_hinstance(void);
extern void wgaim_im_blink(GtkWidget*);

/* Determine Gaim paths */
extern char* wgaim_install_dir(void);
extern char* wgaim_lib_dir(void);
extern char* wgaim_locale_dir(void);

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

/*
 *  Gtk specific
 */
/* Needed for accessing global variables outside the current module */
#ifdef G_MODULE_IMPORT
#undef G_MODULE_IMPORT
#endif
#define G_MODULE_IMPORT __declspec(dllimport)


#endif /* _WIN32DEP_H_ */

