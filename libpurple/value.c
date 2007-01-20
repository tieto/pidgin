/**
 * @file value.c Value wrapper API
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
		g_free(value->u.specific_type);
	}
	else if (gaim_value_get_type(value) == GAIM_TYPE_STRING)
	{
		g_free(value->data.string_data);
	}

	g_free(value);
}

GaimValue *
gaim_value_dup(const GaimValue *value)
{
	GaimValue *new_value;
	GaimType type;

	g_return_val_if_fail(value != NULL, NULL);

	type = gaim_value_get_type(value);

	if (type == GAIM_TYPE_SUBTYPE)
	{
		new_value = gaim_value_new(GAIM_TYPE_SUBTYPE,
								   gaim_value_get_subtype(value));
	}
	else if (type == GAIM_TYPE_BOXED)
	{
		new_value = gaim_value_new(GAIM_TYPE_BOXED,
								   gaim_value_get_specific_type(value));
	}
	else
		new_value = gaim_value_new(type);

	new_value->flags = value->flags;

	switch (type)
	{
		case GAIM_TYPE_CHAR:
			gaim_value_set_char(new_value, gaim_value_get_char(value));
			break;

		case GAIM_TYPE_UCHAR:
			gaim_value_set_uchar(new_value, gaim_value_get_uchar(value));
			break;

		case GAIM_TYPE_BOOLEAN:
			gaim_value_set_boolean(new_value, gaim_value_get_boolean(value));
			break;

		case GAIM_TYPE_SHORT:
			gaim_value_set_short(new_value, gaim_value_get_short(value));
			break;

		case GAIM_TYPE_USHORT:
			gaim_value_set_ushort(new_value, gaim_value_get_ushort(value));
			break;

		case GAIM_TYPE_INT:
			gaim_value_set_int(new_value, gaim_value_get_int(value));
			break;

		case GAIM_TYPE_UINT:
			gaim_value_set_uint(new_value, gaim_value_get_uint(value));
			break;

		case GAIM_TYPE_LONG:
			gaim_value_set_long(new_value, gaim_value_get_long(value));
			break;

		case GAIM_TYPE_ULONG:
			gaim_value_set_ulong(new_value, gaim_value_get_ulong(value));
			break;

		case GAIM_TYPE_INT64:
			gaim_value_set_int64(new_value, gaim_value_get_int64(value));
			break;

		case GAIM_TYPE_UINT64:
			gaim_value_set_uint64(new_value, gaim_value_get_uint64(value));
			break;

		case GAIM_TYPE_STRING:
			gaim_value_set_string(new_value, gaim_value_get_string(value));
			break;

		case GAIM_TYPE_OBJECT:
			gaim_value_set_object(new_value, gaim_value_get_object(value));
			break;

		case GAIM_TYPE_POINTER:
			gaim_value_set_pointer(new_value, gaim_value_get_pointer(value));
			break;

		case GAIM_TYPE_ENUM:
			gaim_value_set_enum(new_value, gaim_value_get_enum(value));
			break;

		case GAIM_TYPE_BOXED:
			gaim_value_set_boxed(new_value, gaim_value_get_boxed(value));
			break;

		default:
			break;
	}

	return new_value;
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

	g_free(value->data.string_data);
	value->data.string_data = g_strdup(data);
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

