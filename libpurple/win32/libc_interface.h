/*
 * purple
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
int wpurple_socket(int namespace, int style, int protocol);
#define socket( namespace, style, protocol ) \
wpurple_socket( namespace, style, protocol )

int wpurple_connect(int socket, struct sockaddr *addr, u_long length);
#define connect( socket, addr, length ) \
wpurple_connect( socket, addr, length )

int wpurple_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlenptr);
#define getsockopt( args... ) \
wpurple_getsockopt( args )

int wpurple_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);
#define setsockopt( args... ) \
wpurple_setsockopt( args )

int wpurple_getsockname (int socket, struct sockaddr *addr, socklen_t *lenptr);
#define getsockname( socket, addr, lenptr ) \
wpurple_getsockname( socket, addr, lenptr )

int wpurple_bind(int socket, struct sockaddr *addr, socklen_t length);
#define bind( socket, addr, length ) \
wpurple_bind( socket, addr, length )

int wpurple_listen(int socket, unsigned int n);
#define listen( socket, n ) \
wpurple_listen( socket, n )

int wpurple_sendto(int socket, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
#define sendto(socket, buf, len, flags, to, tolen) \
wpurple_sendto(socket, buf, len, flags, to, tolen)

/* sys/ioctl.h */
int wpurple_ioctl(int fd, int command, void* opt);
#define ioctl( fd, command, val ) \
wpurple_ioctl( fd, command, val )

/* fcntl.h */
int wpurple_fcntl(int socket, int command, int val);
#define fcntl( fd, command, val ) \
wpurple_fcntl( fd, command, val )

/* arpa/inet.h */
int wpurple_inet_aton(const char *name, struct in_addr *addr);
#define inet_aton( name, addr ) \
wpurple_inet_aton( name, addr )

const char *
wpurple_inet_ntop (int af, const void *src, char *dst, socklen_t cnt);
#define inet_ntop( af, src, dst, cnt ) \
wpurple_inet_ntop( af, src, dst, cnt )

/* netdb.h */
struct hostent* wpurple_gethostbyname(const char *name);
#define gethostbyname( name ) \
wpurple_gethostbyname( name )

/* netinet/in.h */
#define ntohl( netlong ) \
(unsigned int)ntohl( netlong )

/* string.h */
char* wpurple_strerror( int errornum );
#define hstrerror( herror ) \
wpurple_strerror( errno )
#define strerror( errornum ) \
wpurple_strerror( errornum )

#define bzero( dest, size ) memset( dest, 0, size )

/* unistd.h */
int wpurple_read(int fd, void *buf, unsigned int size);
#define read( fd, buf, buflen ) \
wpurple_read( fd, buf, buflen )

int wpurple_write(int fd, const void *buf, unsigned int size);
#define write( socket, buf, buflen ) \
wpurple_write( socket, buf, buflen )

int wpurple_recv(int fd, void *buf, size_t len, int flags);
#define recv(fd, buf, len, flags) \
wpurple_recv(fd, buf, len, flags)

int wpurple_send(int fd, const void *buf, unsigned int size, int flags);
#define send(socket, buf, buflen, flags) \
wpurple_send(socket, buf, buflen, flags)

int wpurple_close(int fd);
#define close( fd ) \
wpurple_close( fd )

#if !GLIB_CHECK_VERSION(2,8,0)
int wpurple_g_access(const gchar *filename, int mode);
#define g_access( filename, mode) \
wpurple_g_access( filename, mode )
#endif

#ifndef sleep
#define sleep(x) Sleep((x)*1000)
#endif

int wpurple_gethostname(char *name, size_t size);
#define gethostname( name, size ) \
wpurple_gethostname( name, size )

/* sys/time.h */
int wpurple_gettimeofday(struct timeval *p, struct timezone *z);
#define gettimeofday( timeval, timezone ) \
wpurple_gettimeofday( timeval, timezone )

/* stdio.h */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

int wpurple_rename(const char *oldname, const char *newname);
#define rename( oldname, newname ) \
wpurple_rename( oldname, newname )

#if GLIB_CHECK_VERSION(2,6,0)
#ifdef g_rename
# undef g_rename
#endif
/* This is necessary because we want rename on win32 to be able to overwrite an existing file, it is done in internal.h if GLib < 2.6*/
#define g_rename(oldname, newname) \
wpurple_rename(oldname, newname)
#endif


/* sys/stat.h */

#define fchmod(a,b)

/* time.h */
struct tm *wpurple_localtime_r(const time_t *time, struct tm *resultp);
#define localtime_r( time, resultp ) \
wpurple_localtime_r( time, resultp )

/* helper for purple_utf8_strftime() by way of purple_internal_strftime() in src/util.c */
const char *wpurple_get_timezone_abbreviation(const struct tm *tm);

#endif /* _LIBC_INTERFACE_H_ */
