/*
 *  libc_interface.h
 */

#ifndef _LIBC_INTERFACE_H_
#define _LIBC_INTERFACE_H_
#include <winsock.h>
#include <io.h>
#include <errno.h>
#include "libc_internal.h"

/* sys/socket.h */
extern int wgaim_socket(int namespace, int style, int protocol);
#define socket( namespace, style, protocol ) \
wgaim_socket( ## namespace ##, ## style ##, ## protocol ## )

extern int wgaim_connect(int socket, struct sockaddr *addr, u_long length);
#define connect( socket, addr, length ) \
wgaim_connect( ## socket ##, ## addr ##, ## length ## )

extern int wgaim_getsockopt(int socket, int level, int optname, void *optval, unsigned int *optlenptr);
#define getsockopt( args... ) \
wgaim_getsockopt( ## args )

/* sys/ioctl.h */
extern int wgaim_ioctl(int fd, int command, void* opt);
#define ioctl( fd, command, val ) \
wgaim_ioctl( ## fd ##, ## command ##, ## val ## )

/* fcntl.h */
extern int wgaim_fcntl(int socket, int command, int val);
#define fcntl( fd, command, val ) \
wgaim_fcntl( ## fd ##, ## command ##, ## val ## )

#define open( args... ) _open( ## args )

/* arpa/inet.h */
extern int wgaim_inet_aton(const char *name, struct in_addr *addr);
#define inet_aton( name, addr ) \
wgaim_inet_aton( ## name ##, ## addr ## )

/* netdb.h */
extern struct hostent* wgaim_gethostbyname(const char *name);
#define gethostbyname( name ) \
wgaim_gethostbyname( ## name ## )

/* string.h */
extern char* wgaim_strerror( int errornum );
#define hstrerror( herror ) \
wgaim_strerror( errno )
#define strerror( errornum ) \
wgaim_strerror( ## errornum ## )

extern char* wgaim_strsep(char **stringp, const char *delim);
#define strsep( stringp, delim ) \
wgaim_strsep( ## stringp ##, ## delim ## )

#define bzero( dest, size ) memset( ## dest ##, 0, ## size ## )

/* unistd.h */
extern int wgaim_read(int fd, void *buf, unsigned int size);
#define read( fd, buf, buflen ) \
wgaim_read( ## fd ##, ## buf ##, ## buflen ## )

extern int wgaim_write(int fd, const void *buf, unsigned int size);
#define write( socket, buf, buflen ) \
wgaim_write( ## socket ##, ## buf ##, ## buflen ## )

extern int wgaim_close(int fd);
#define close( fd ) \
wgaim_close( ## fd ## )

#define sleep(x) Sleep((x)*1000)

/* sys/time.h */
extern int wgaim_gettimeofday(struct timeval *p, struct timezone *z);
#define gettimeofday( timeval, timezone ) \
wgaim_gettimeofday( ## timeval ##, ## timezone ## )

/* stdio.h */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

/* sys/stat.h */
#define mkdir(a,b) _mkdir((a))
#define fchmod(a,b)

#endif /* _LIBC_INTERFACE_H_ */
