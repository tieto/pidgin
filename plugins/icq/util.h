/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: util.h 1987 2001-06-09 14:46:51Z warmenhoven $
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

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "icqtypes.h"
#include "list.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE !FALSE
#endif

void hex_dump(char *data, long size);

WORD Chars_2_Word(unsigned char *buf);
DWORD Chars_2_DW(unsigned char *buf);
void DW_2_Chars(unsigned char *buf, DWORD num);
void Word_2_Chars(unsigned char *buf, WORD num);

const char *icq_ConvertStatus2Str(unsigned long status);
int icq_SplitFields(icq_List *strList, const char *str);

#endif /* _UTIL_H_ */
