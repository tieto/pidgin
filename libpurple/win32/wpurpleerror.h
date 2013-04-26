/*
 * purple
 *
 * File: wpurpleerror.h
 * Date: October 14, 2002
 * Description: Convert Winsock errors to Unix errors
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#ifndef _WPURPLEERROR_H
#define _WPURPLEERROR_H

/* Here we define unix socket errors as windows socket errors */

#undef ENETDOWN
#define ENETDOWN WSAENETDOWN
#undef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#undef ENOBUFS
#define ENOBUFS WSAENOBUFS
#undef EPROTONOSUPPORT
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#undef EPROTOTYPE
#define EPROTOTYPE WSAEPROTOTYPE
#undef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT

#undef EADDRINUSE
#define EADDRINUSE WSAEADDRINUSE
#undef EALREADY
#define EALREADY WSAEALREADY
#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#undef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#undef EISCONN
#define EISCONN WSAEISCONN
#undef ENETUNREACH
#define ENETUNREACH WSAENETUNREACH
#undef ENOTSOCK
#define ENOTSOCK WSAENOTSOCK
#undef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK

#undef ENOTCONN
#define ENOTCONN WSAENOTCONN
#undef ENETRESET
#define ENETRESET WSAENETRESET
#undef EOPNOTSUPP
#define EOPNOTSUPP WSAEOPNOTSUPP
#undef ESHUTDOWN
#define ESHUTDOWN WSAESHUTDOWN
#undef EMSGSIZE
#define EMSGSIZE WSAEMSGSIZE
#undef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#undef ECONNRESET
#define ECONNRESET WSAECONNRESET
#undef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH

#endif /* end _WPURPLEERROR_H */
