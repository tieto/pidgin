/*
 *  win32dep.h
 */

#ifndef _WIN32DEP_H_
#define _WIN32DEP_H_
#include <winsock.h>
#include <gdk/gdkevents.h>
#include "winerror.h"

/*
 *  PROTOS
 */

/* win32dep.c */
extern char* wgaim_install_dir(void);
extern char* wgaim_lib_dir(void);
extern char* wgaim_locale_dir(void);
extern GdkFilterReturn wgaim_window_filter(GdkXEvent *xevent, 
					   GdkEvent *event, 
					   gpointer data);
extern void wgaim_init(void);

/* sys/socket.h */
extern int wgaim_socket(int namespace, int style, int protocol);
extern int wgaim_connect(int socket, struct sockaddr *addr, u_long length);
extern int wgaim_getsockopt(int socket, int level, int optname, void *optval, unsigned int *optlenptr);
/* sys/ioctl.h */
extern int wgaim_ioctl(int fd, int command, void* opt);
/* fcntl.h */
extern int wgaim_fcntl(int socket, int command, int val);
/* arpa/inet.h */
extern int wgaim_inet_aton(const char *name, struct in_addr *addr);
/* netdb.h */
extern struct hostent* wgaim_gethostbyname(const char *name);
/* string.h */
extern char* wgaim_strerror( int errornum );
/* unistd.h */
extern int wgaim_read(int fd, void *buf, unsigned int size);
extern int wgaim_write(int fd, const void *buf, unsigned int size);
extern int wgaim_close(int fd);

/*
 *  MACROS
 */

#define bzero( dest, size ) memset( ## dest ##, 0, ## size ## )
#define sleep(x) Sleep((x)*1000)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define mkdir(a,b) _mkdir((a))
#define open( args... ) _open( ## args )

/* sockets */
#define socket( namespace, style, protocol ) \
wgaim_socket( ## namespace ##, ## style ##, ## protocol ## )

#define connect( socket, addr, length ) \
wgaim_connect( ## socket ##, ## addr ##, ## length ## )

#define getsockopt( args... ) \
wgaim_getsockopt( ## args )

#define read( fd, buf, buflen ) \
wgaim_read( ## fd ##, ## buf ##, ## buflen ## )

#define write( socket, buf, buflen ) \
wgaim_write( ## socket ##, ## buf ##, ## buflen ## )

#define close( fd ) \
wgaim_close( ## fd ## )

#define inet_aton( name, addr ) \
wgaim_inet_aton( ## name ##, ## addr ## )

/* hostent */
#define gethostbyname( name ) \
wgaim_gethostbyname( ## name ## )

#define hstrerror( herror ) wgaim_strerror( errno )

/* fcntl commands & options */
#define F_SETFL 1
#define O_NONBLOCK 1
#define fcntl( fd, command, val ) \
wgaim_fcntl( ## fd ##, ## command ##, ## val ## )

#define ioctl( fd, command, val ) \
wgaim_ioctl( ## fd ##, ## command ##, ## val ## )

#define strerror( errornum ) \
wgaim_strerror( ## errornum ## )

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

