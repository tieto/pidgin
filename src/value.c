/**
 * @file value.c Value wrapper API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
 */
#include "internal.h"

#include "value.h"

#define OUTGOING_FLAG 0x01

GaimValue *
gaim_value_new(GaimType type, ...)
{
	GaimValue *value;
	va_list args;

	g_return_val_if_fail(type != GAIM_TYPE_UNKNOWN, NULL);

	value = g_new0(GaimValue, 1);

	value->type = type;

	va_start(args, type);

	if (type == GAIM_TYPE_SUBTYPE)
		value->u.subtype = va_arg(args, int);
	else if (type == GAIM_TYPE_BOXED)
		value->u.specific_type = g_strdup(va_arg(args, char *));

	va_end(args);

	return value;
}

GaimValue *
gaim_value_new_outgoing(GaimType type, ...)
{
	GaimValue *value;
	va_list args;

	g_return_val_if_fail(type != GAIM_TYPE_UNKNOWN, NULL);

	value = g_new0(GaimValue, 1);

	value->type = type;

	va_start(args, type);

	if (type == GAIM_TYPE_SUBTYPE)
		value->u.subtype = va_arg(args, int);
	else if (type == GAIM_TYPE_BOXED)
		value->u.specific_type = g_strdup(va_arg(args, char *));

	va_end(args);

	value->flags |= OUTGOING_FLAG;

	return value;
}

void
gaim_value_destroy(GaimValue *value)
{
	g_return_if_fail(value != NULL);

	if (gaim_value_get_type(value) == GAIM_TYPE_BOXED)
	{
		if (value->u.specific_type != NULL)
			g_free(value->u.specific_type);
	}
	else if (gaim_value_get_type(value) == GAIM_TYPE_STRING)
	{
		if (value->data.string_data != NULL)
			g_free(value->data.string_data);
	}

	g_free(value);
}

GaimType
gaim_value_get_type(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, GAIM_TYPE_UNKNOWN);

	return value->type;
}

unsigned int
gaim_value_get_subtype(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);
	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_SUBTYPE, 0);

	return value->u.subtype;
}

const char *
gaim_value_get_specific_type(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, NULL);
	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_BOXED, NULL);

	return value->u.specific_type;
}

gboolean
gaim_value_is_outgoing(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, FALSE);

	return (value->flags & OUTGOING_FLAG);
}

void
gaim_value_set_char(GaimValue *value, char data)
{
	g_return_if_fail(value != NULL);

	value->data.char_data = data;
}

void
gaim_value_set_uchar(GaimValue *value, unsigned char data)
{
	g_return_if_fail(value != NULL);

	value->data.uchar_data = data;
}

void
gaim_value_set_boolean(GaimValue *value, gboolean data)
{
	g_return_if_fail(value != NULL);

	value->data.boolean_data = data;
}

void
gaim_value_set_short(GaimValue *value, short data)
{
	g_return_if_fail(value != NULL);

	value->data.short_data = data;
}

void
gaim_value_set_ushort(GaimValue *value, unsigned short data)
{
	g_return_if_fail(value != NULL);

	value->data.ushort_data = data;
}

void
gaim_value_set_int(GaimValue *value, int data)
{
	g_return_if_fail(value != NULL);

	value->data.int_data = data;
}

void
gaim_value_set_uint(GaimValue *value, unsigned int data)
{
	g_return_if_fail(value != NULL);

	value->data.int_data = data;
}

void
gaim_value_set_long(GaimValue *value, long data)
{
	g_return_if_fail(value != NULL);

	value->data.long_data = data;
}

void
gaim_value_set_ulong(GaimValue *value, unsigned long data)
{
	g_return_if_fail(value != NULL);

	value->data.long_data = data;
}

void
gaim_value_set_int64(GaimValue *value, gint64 data)
{
	g_return_if_fail(value != NULL);

	value->data.int64_data = data;
}

void
gaim_value_set_uint64(GaimValue *value, guint64 data)
{
	g_return_if_fail(value != NULL);

	value->data.uint64_data = data;
}

void
gaim_value_set_string(GaimValue *value, const char *data)
{
	g_return_if_fail(value != NULL);

	if (value->data.string_data != NULL)
		g_free(value->data.string_data);

	value->data.string_data = (data == NULL ? NULL : g_strdup(data));
}

void
gaim_value_set_object(GaimValue *value, void *data)
{
	g_return_if_fail(value != NULL);

	value->data.object_data = data;
}

void
gaim_value_set_pointer(GaimValue *value, void *data)
{
	g_return_if_fail(value != NULL);

	value->data.pointer_data = data;
}

void
gaim_value_set_enum(GaimValue *value, int data)
{
	g_return_if_fail(value != NULL);

	value->data.enum_data = data;
}

void
gaim_value_set_boxed(GaimValue *value, void *data)
{
	g_return_if_fail(value != NULL);

	value->data.boxed_data = data;
}

char
gaim_value_get_char(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.char_data;
}

unsigned char
gaim_value_get_uchar(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.uchar_data;
}

gboolean
gaim_value_get_boolean(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, FALSE);

	return value->data.boolean_data;
}

short
gaim_value_get_short(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.short_data;
}

unsigned short
gaim_value_get_ushort(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.ushort_data;
}

int
gaim_value_get_int(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.int_data;
}

unsigned int
gaim_value_get_uint(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.int_data;
}

long
gaim_value_get_long(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.long_data;
}

unsigned long
gaim_value_get_ulong(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.long_data;
}

gint64
gaim_value_get_int64(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.int64_data;
}

guint64
gaim_value_get_uint64(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, 0);

	return value->data.uint64_data;
}

const char *
gaim_value_get_string(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, NULL);

	return value->data.string_data;
}

void *
gaim_value_get_object(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, NULL);

	return value->data.object_data;
}

void *
gaim_value_get_pointer(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, NULL);

	return value->data.pointer_data;
}

int
gaim_value_get_enum(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, -1);

	return value->data.enum_data;
}

void *
gaim_value_get_boxed(const GaimValue *value)
{
	g_return_val_if_fail(value != NULL, NULL);

	return value->data.boxed_data;
}

