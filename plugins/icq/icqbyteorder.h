/*
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

/* linux way */
#ifdef HAVE_ENDIAN_H
# include <endian.h> 
#endif
#ifdef HAVE_BYTESWAP_H
# include <byteswap.h>
#endif

/* BSD way */
#ifdef HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#endif

/* HP-UX way */
#ifdef hpux
# ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
# endif
#endif

/* Cygwin way */
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

/*
 * Kind of portable way. this common header, at least I found it on BSD and Solaris.
 * On Solaris this is only place where endiannes is defined.
 */
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
# include <arpa/nameser_compat.h>
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

#ifdef BYTE_ORDER_LITTLE_ENDIAN
# define htoicql(x)   (x)
# define icqtohl(x)   (x)
# define htoicqs(x)   (x)
# define icqtohs(x)   (x)
#else

#ifndef BYTE_ORDER
#   error "Unknown byte order!"
#endif

#if BYTE_ORDER == BIG_ENDIAN
/* host is different from ICQ byte order */
# define htoicql(x)   bswap_32(x)
# define icqtohl(x)   bswap_32(x)
# define htoicqs(x)   bswap_16(x)
# define icqtohs(x)   bswap_16(x)
#else
# if BYTE_ORDER == LITTLE_ENDIAN
/* host byte order match ICQ byte order */
#   define htoicql(x)   (x)
#   define icqtohl(x)   (x)
#   define htoicqs(x)   (x)
#   define icqtohs(x)   (x)
# else
#   error "Unsupported byte order!"
# endif
#endif

#endif

/* ICQ byte order is always different from network byte order. */
# define ntoicql(x)   bswap_32(x)
# define icqtonl(x)   bswap_32(x)
# define ntoicqs(x)   bswap_16(x)
# define icqtons(x)   bswap_16(x)

#endif
