/*
 *  win32dep.h
 */

#ifndef _WIN32DEP_H_
#define _WIN32DEP_H_
#include <gdk/gdkevents.h>

extern char* wgaim_install_dir(void);
extern char* wgaim_lib_dir(void);
extern char* wgaim_locale_dir(void);
extern GdkFilterReturn wgaim_window_filter(GdkXEvent *xevent, 
					   GdkEvent *event, 
					   gpointer data);
extern void wgaim_init(void);

#define unlink _unlink
#define bzero( dest, size ) memset( ## dest ##, 0, ## size ## )
#define sleep(x) Sleep((x)*1000)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define DATADIR wgaim_install_dir()

/* Needed for accessing global variables outside the current module */
#ifdef G_MODULE_IMPORT
#undef G_MODULE_IMPORT
#endif
#define G_MODULE_IMPORT __declspec(dllimport)

#define LIBDIR wgaim_lib_dir()
#define LOCALEDIR wgaim_locale_dir()

#endif /* _WIN32DEP_H_ */

