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
 * I am really trying to use builtin optimized byte swap routines.
 * they are highly optimised on some platforms.
 * But as last resort this simple code is used.
 */

#ifndef HAVE_BYTESWAP_H
# ifndef bswap_32
unsigned long bswap_32(unsigned long v)
{
  unsigned char c,*x=(unsigned char *)&v;
  c=x[0];x[0]=x[3];x[3]=c;
  c=x[1];x[1]=x[2];x[2]=c;
  return v;
}
# endif

# ifndef bswap_16
unsigned short bswap_16(unsigned short v)
{
  unsigned char c,*x=(unsigned char *)&v;
  c=x[0];x[0]=x[1];x[1]=c;
  return v;
}
# endif
#endif
