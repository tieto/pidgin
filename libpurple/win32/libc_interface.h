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
#define socket( namespace, style, protocol ) \
wpurple_socket( namespace, style, protocol )

#define connect( socket, addr, length ) \
wpurple_connect( socket, addr, length )

#define getsockopt( args... ) \
wpurple_getsockopt( args )

#define setsockopt( args... ) \
wpurple_setsockopt( args )

#define getsockname( socket, addr, lenptr ) \
wpurple_getsockname( socket, addr, lenptr )

#define bind( socket, addr, length ) \
wpurple_bind( socket, addr, length )

#define listen( socket, n ) \
wpurple_listen( socket, n )

#define sendto(socket, buf, len, flags, to, tolen) \
wpurple_sendto(socket, buf, len, flags, to, tolen)

#define recv(fd, buf, len, flags) \
wpurple_recv(fd, buf, len, flags)

#define send(socket, buf, buflen, flags) \
wpurple_send(socket, buf, buflen, flags)

/* sys/ioctl.h */
#define ioctl( fd, command, val ) \
wpurple_ioctl( fd, command, val )

/* fcntl.h */
#define fcntl( fd, command, val ) \
wpurple_fcntl( fd, command, val )

/* arpa/inet.h */
#define inet_aton( name, addr ) \
wpurple_inet_aton( name, addr )

#define inet_ntop( af, src, dst, cnt ) \
wpurple_inet_ntop( af, src, dst, cnt )

/* netdb.h */
#define gethostbyname( name ) \
wpurple_gethostbyname( name )

/* netinet/in.h */
#define ntohl( netlong ) \
(unsigned int)ntohl( netlong )

/* string.h */
#define hstrerror( herror ) \
wpurple_strerror( errno )
#define strerror( errornum ) \
wpurple_strerror( errornum )

#define bzero( dest, size ) memset( dest, 0, size )

/* unistd.h */
#define read( fd, buf, buflen ) \
wpurple_read( fd, buf, buflen )

#define write( socket, buf, buflen ) \
wpurple_write( socket, buf, buflen )

#define close( fd ) \
wpurple_close( fd )

#if !GLIB_CHECK_VERSION(2,8,0)
#define g_access( filename, mode) \
wpurple_g_access( filename, mode )
#endif

#ifndef sleep
#define sleep(x) Sleep((x)*1000)
#endif

#define gethostname( name, size ) \
wpurple_gethostname( name, size )

/* sys/time.h */
#define gettimeofday( timeval, timezone ) \
wpurple_gettimeofday( timeval, timezone )

/* stdio.h */
#define snprintf _snprintf
#define vsnprintf _vsnprintf

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
#define localtime_r( time, resultp ) \
wpurple_localtime_r( time, resultp )

/* helper for purple_utf8_strftime() by way of purple_internal_strftime() in src/util.c */
const char *wpurple_get_timezone_abbreviation(const struct tm *tm);

#endif /* _LIBC_INTERFACE_H_ */
