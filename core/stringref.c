/**
 * @file stringref.c Reference-counted immutable strings
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include <stdarg.h>

#include "debug.h"
#include "stringref.h"

#define REFCOUNT(x) ((x) & 0x7fffffff)

static GList *gclist = NULL;

static void stringref_free(GaimStringref *stringref);
static gboolean gs_idle_cb(gpointer data);

GaimStringref *gaim_stringref_new(const char *value)
{
	GaimStringref *newref;

	if (value == NULL)
		return NULL;

	newref = g_malloc(sizeof(GaimStringref) + strlen(value));
	strcpy(newref->value, value);
	newref->ref = 1;

	return newref;
}

GaimStringref *gaim_stringref_new_noref(const char *value)
{
	GaimStringref *newref;

	if (value == NULL)
		return NULL;

	newref = g_malloc(sizeof(GaimStringref) + strlen(value));
	strcpy(newref->value, value);
	newref->ref = 0x80000000;

	if (gclist == NULL)
		g_idle_add(gs_idle_cb, NULL);
	gclist = g_list_prepend(gclist, newref);

	return newref;
}

GaimStringref *gaim_stringref_printf(const char *format, ...)
{
	GaimStringref *newref;
	va_list ap;

	if (format == NULL)
		return NULL;

	va_start(ap, format);
	newref = g_malloc(sizeof(GaimStringref) + g_printf_string_upper_bound(format, ap));
	vsprintf(newref->value, format, ap);
	va_end(ap);
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
	if (stringref == NULL)
		return;
	if (REFCOUNT(--(stringref->ref)) == 0) {
		if (stringref->ref & 0x80000000)
			gclist = g_list_remove(gclist, stringref);
		stringref_free(stringref);
	}
}

const char *gaim_stringref_value(const GaimStringref *stringref)
{
	return (stringref == NULL ? NULL : stringref->value);
}

int gaim_stringref_cmp(const GaimStringref *s1, const GaimStringref *s2)
{
	return (s1 == s2 ? 0 : strcmp(gaim_stringref_value(s1), gaim_stringref_value(s2)));
}

size_t gaim_stringref_len(const GaimStringref *stringref)
{
	return strlen(gaim_stringref_value(stringref));
}

static void stringref_free(GaimStringref *stringref)
{
#ifdef DEBUG
	if (REFCOUNT(stringref->ref) != 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "stringref", "Free of nonzero (%d) ref stringref!\n", REFCOUNT(stringref->ref));
		return;
	}
#endif /* DEBUG */
	g_free(stringref);
}

static gboolean gs_idle_cb(gpointer data)
{
	GaimStringref *ref;
	GList *del;

	while (gclist != NULL) {
		ref = gclist->data;
		if (REFCOUNT(ref->ref) == 0) {
			stringref_free(ref);
		}
		del = gclist;
		gclist = gclist->next;
		g_list_free_1(del);
	}

	return FALSE;
}
