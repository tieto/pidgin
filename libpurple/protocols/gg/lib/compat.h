/* $Id: compat.h 506 2008-01-14 22:15:05Z wojtekka $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Woźny <speedy@ziew.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/**
 * \file compat.h
 *
 * \brief Makra zapewniające kompatybilność API na różnych systemach
 */

#ifndef __COMPAT_H
#define __COMPAT_H

#ifdef _WIN32
#  include <ws2tcpip.h>
#  include <winsock2.h>
#  define EINPROGRESS WSAEINPROGRESS
#  define ETIMEDOUT WSAETIMEDOUT
#  define ENOTCONN WSAENOTCONN
#  define ECONNRESET WSAECONNRESET
#else
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif

#ifdef sun
#  define INADDR_NONE   ((in_addr_t) 0xffffffff)
#endif

#endif
