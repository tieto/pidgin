/**
 * @file stringref.c Reference-counted strings
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Ethan Blanton <elb@elitists.net>
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

#include <string.h>

#include "stringref.h"

GaimStringref *gaim_stringref_new(const char *value)
{
	GaimStringref *newref;

	newref = g_malloc(sizeof(GaimStringref) + strlen(value) + 1);
	strcpy(newref->value, value);
	newref->ref = 1;

	return newref;
}

GaimStringref *gaim_stringref_ref(GaimStringref *stringref)
{
	if (stringref == NULL)
		return NULL;
	stringref->ref++;
	return stringref;
}

void gaim_stringref_unref(GaimStringref *stringref)
{
	g_return_if_fail(stringref != NULL);
	if (--stringref->ref == 0)
		g_free(stringref);
}

const char *gaim_stringref_value(GaimStringref *stringref)
{
	return (stringref == NULL ? NULL : stringref->value);
}
