/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: icqtypes.h 2096 2001-07-31 01:00:39Z warmenhoven $
 *
 * Copyright (C) 1998-2001, Denis V. Dmitrienko <denis@null.net> and
 *                          Bill Soudan <soudan@kde.org>
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

#ifndef _ICQTYPES_H_
#define _ICQTYPES_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

#ifndef _WIN32
typedef unsigned char BOOL;
#endif

typedef signed long S_DWORD;
typedef signed short S_WORD;
typedef signed char S_BYTE;

#endif /* _ICQTYPES_H_ */
