/*
 * gaim
 *
 * File: libc_interface.h
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
#ifndef _LIBC_INTERFACE_H_
#define _LIBC_INTERFACE_H_
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <errno.h>
#include "libc_internal.h"
#include <glib.h>

/* sys/socket.h */
int wgaim_socket(int namespace, int style, int protocol);
#define socket( namespace, style, protocol ) \
wgaim_socket( namespace, style, protocol )

int wgaim_connect(int socket, struct sockaddr *addr, u_long length);
#define connect( socket, addr, length ) \
wgaim_connect( socket, addr, length )

int wgaim_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlenptr);
#define getsockopt( args... ) \
wgaim_getsockopt( args )

int wgaim_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);
#define setsockopt( args... ) \
wgaim_setsockopt( args )

int wgaim_getsockname (int socket, struct sockaddr *addr, socklen_t *lenptr);
#define getsockname( socket, addr, lenptr ) \
wgaim_getsockname( socket, addr, lenptr )

int wgaim_bind(int socket, struct sockaddr *addr, socklen_t length);
#define bind( socket, addr, length ) \
wgaim_bind( socket, addr, length )

int wgaim_listen(int socket, unsigned int n);
#define listen( socket, n ) \
wgaim_listen( socket, n )

int wgaim_sendto(int socket, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
#define sendto(socket, buf, len, flags, to, tolen) \
wgaim_sendto(socket, buf, len, flags, to, tolen)

/* sys/ioctl.h */
int wgaim_ioctl(int fd, int command, void* opt);
#define ioctl( fd, command, val ) \
wgaim_ioctl( fd, command, val )

/* fcntl.h */
int wgaim_fcntl(int socket, int command, int val);
#define fcntl( fd, command, val ) \
wgaim_fcntl( fd, command, val )

/* arpa/inet.h */
int wgaim_inet_aton(const char *name, struct in_addr *addr);
#define inet_aton( name, addr ) \
wgaim_inet_aton( name, addr )

const char *
wgaim_inet_ntop (int af, const void *src, char *dst, socklen_t cnt);
#define inet_ntop( af, src, dst, cnt ) \
wgaim_inet_ntop( af, src, dst, cnt )

/* netdb.h */
struct hostent* wgaim_gethostbyname(const char *name);
#define gethostbyname( name ) \
wgaim_gethostbyname( name )

/* netinet/in.h */
#define ntohl( netlong ) \
(unsigned int)ntohl( netlong )

/* string.h */
char* wgaim_strerror( int errornum );
#define hstrerror( herror ) \
wgaim_strerror( errno )
#define strerror( errornum ) \
wgaim_strerror( errornum )

#define bzero( dest, size ) memset( dest, 0, size )

/* unistd.h */
int wgaim_read(int fd, void *buf, unsigned int size);
#define read( fd, buf, buflen ) \
wgaim_read( fd, buf, buflen )

int wgaim_write(int fd, const void *buf, unsigned int size);
#define write( socket, buf, buflen ) \
wgaim_write( socket, buf, buflen )

int wgaim_recv(int fd, void *buf, size_t len, int flags);
#define recv(fd, buf, len, flags) \
wgaim_recv(fd, buf, len, flags)

int wgaim_send(int fd, const void *buf, unsigned int size, int flags);
#define send(socket, buf, buflen, flags) \
wgaim_send(socket, buf, buflen, flags)

int wgaim_close(int fd);
#define close( fd ) \
wgaim_close( fd )

#if !GLIB_CHECK_VERSION(2,8,0)
int wgaim_g_access(const gchar *filename, int mode);
#undef g_access
#define g_access( filename, mode) \
wgaim_g_access( filename, mode )
#endif

#ifndef sleep
#define sleep(x) Sleep((x)*1000)
#endif

int wgaim_gethostname(char *name, size_t size);
#define gethostname( name, size ) \
wgaim_gethostname( name, size )

/* sys/time.h */
int wgaim_gettimeofday(struct timeval *p, struct timezone *z);
#define gettimeofday( timeval, timezone ) \
wgaim_gettimeofday( timeval, timezone )

/* stdio.h */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

int wgaim_rename(const char *oldname, const char *newname);
#define rename( oldname, newname ) \
wgaim_rename( oldname, newname )

#if GLIB_CHECK_VERSION(2,6,0)
#ifdef g_rename
# undef g_rename
#endif
/* This is necessary because we want rename on win32 to be able to overwrite an existing file, it is done in internal.h if GLib < 2.6*/
#define g_rename(oldname, newname) \
wgaim_rename(oldname, newname)
#endif


/* sys/stat.h */

#define fchmod(a,b)

/* time.h */
struct tm *wgaim_localtime_r(const time_t *time, struct tm *resultp);
#define localtime_r( time, resultp ) \
wgaim_localtime_r( time, resultp )

/* helper for gaim_utf8_strftime() by way of gaim_internal_strftime() in src/util.c */
const char *wgaim_get_timezone_abbreviation(const struct tm *tm);

#endif /* _LIBC_INTERFACE_H_ */
