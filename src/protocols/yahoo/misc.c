/*
 * libyay
 *
 * Copyright (C) 2001 Eric Warmenhoven <warmenhoven@yahoo.com>
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

#include "internal.h"
#include <ctype.h>
#include <string.h>

char *yahoo_urlencode(const char *str)
{
	int len;
	char *ret;
	const char *s;
	char *r;

	if ((len = strlen(str)) == 0)
		return NULL;

	ret = g_malloc(len * 3 + 1);
	if (!ret)
		return NULL;

	for (s = str, r = ret; *s; s++) {
		if (isdigit(*s) || isalpha(*s) || *s == '_')
			*r++ = *s;
		else {
			int tmp = *s / 16;
			*r++ = '%';
			*r++ = (tmp < 10) ? (tmp + '0') : (tmp - 10 + 'A');
			tmp = *s % 16;
			*r++ = (tmp < 10) ? (tmp + '0') : (tmp - 10 + 'A');
		}
	}

	*r = '\0';

	return ret;
}

int yahoo_makeint(guchar *buf)
{
	return ((buf[3] << 24) + (buf[2] << 16) + (buf[1] << 8) + buf[0]);
}

void yahoo_storeint(guchar *buf, guint data)
{
	int i;
	for (i = 0; i < 4; i++) {
		buf[i] = data % 256;
		data >>= 8;
	}
}
