/*
 * $Id: icqbyteorder.h 1987 2001-06-09 14:46:51Z warmenhoven $
 *
 * This header defines macros to handle ICQ protocol byte order conversion.
 *
 * Vadim Zaliva <lord@crocodile.org>
 * http://www.crocodile.org/
 * 
 * Copyright (C) 1999 Vadim Zaliva
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * From ICQ protocol description:
 * "Integers consisting of more than one byte is stored
 * with the least significant byte first, and the most significant byte last
 * (as is usual on the PC/Intel architecture)".
 *
 * whereas the network byte order, as used on the
 * Internet, is Most Significant Byte first. (aka. Big Endian)
 */

#ifndef ICQ_BYTEORDER_H_FLAG
#define ICQ_BYTEORDER_H_FLAG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif         

/*
 * I am really trying to use builtin optimized byte swap routines.
 * they are highly optimised on some platforms.
 * But as last resort this simple code is used.
 */
#ifndef HAVE_BYTESWAP_H
# ifndef bswap_32
extern unsigned long bswap_32(unsigned long v);
# endif
# ifndef bswap_16
extern unsigned short bswap_16(unsigned short v);
# endif
#endif

#ifdef WORDS_BIGENDIAN
# define htoicql(x)   bswap_32(x)
# define icqtohl(x)   bswap_32(x)
# define htoicqs(x)   bswap_16(x)
# define icqtohs(x)   bswap_16(x)
#else
# define htoicql(x)   (x)
# define icqtohl(x)   (x)
# define htoicqs(x)   (x)
# define icqtohs(x)   (x)
#endif

#endif
