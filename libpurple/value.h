/**
 * @file value.h Value wrapper API
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
#ifndef _GAIM_VALUE_H_
#define _GAIM_VALUE_H_

#include <glib.h>

/**
 * Specific value types.
 */
typedef enum
{
	GAIM_TYPE_UNKNOWN = 0,  /**< Unknown type.                     */
	GAIM_TYPE_SUBTYPE,      /**< Subtype.                          */
	GAIM_TYPE_CHAR,         /**< Character.                        */
	GAIM_TYPE_UCHAR,        /**< Unsigned character.               */
	GAIM_TYPE_BOOLEAN,      /**< Boolean.                          */
	GAIM_TYPE_SHORT,        /**< Short integer.                    */
	GAIM_TYPE_USHORT,       /**< Unsigned short integer.           */
	GAIM_TYPE_INT,          /**< Integer.                          */
	GAIM_TYPE_UINT,         /**< Unsigned integer.                 */
	GAIM_TYPE_LONG,         /**< Long integer.                     */
	GAIM_TYPE_ULONG,        /**< Unsigned long integer.            */
	GAIM_TYPE_INT64,        /**< 64-bit integer.                   */
	GAIM_TYPE_UINT64,       /**< 64-bit unsigned integer.          */
	GAIM_TYPE_STRING,       /**< String.                           */
	GAIM_TYPE_OBJECT,       /**< Object pointer.                   */
	GAIM_TYPE_POINTER,      /**< Generic pointer.                  */
	GAIM_TYPE_ENUM,         /**< Enum.                             */
	GAIM_TYPE_BOXED         /**< Boxed pointer with specific type. */

} GaimType;


/**
 * Gaim-specific subtype values.
 */
typedef enum
{
	GAIM_SUBTYPE_UNKNOWN = 0,
	GAIM_SUBTYPE_ACCOUNT,
	GAIM_SUBTYPE_BLIST,
	GAIM_SUBTYPE_BLIST_BUDDY,
	GAIM_SUBTYPE_BLIST_GROUP,
	GAIM_SUBTYPE_BLIST_CHAT,
	GAIM_SUBTYPE_BUDDY_ICON,
	GAIM_SUBTYPE_CONNECTION,
	GAIM_SUBTYPE_CONVERSATION,
	GAIM_SUBTYPE_PLUGIN,
	GAIM_SUBTYPE_BLIST_NODE,
	GAIM_SUBTYPE_CIPHER,
	GAIM_SUBTYPE_STATUS,
	GAIM_SUBTYPE_LOG,
	GAIM_SUBTYPE_XFER,
	GAIM_SUBTYPE_SAVEDSTATUS,
	GAIM_SUBTYPE_XMLNODE,
	GAIM_SUBTYPE_USERINFO
} GaimSubType;

/**
 * A wrapper for a type, subtype, and specific type of value.
 */
typedef struct
{
	GaimType type;
	unsigned short flags;

	union
	{
		char char_data;
		unsigned char uchar_data;
		gboolean boolean_data;
		short short_data;
		unsigned short ushort_data;
		int int_data;
		unsigned int uint_data;
		long long_data;
		unsigned long ulong_data;
		gint64 int64_data;
		guint64 uint64_data;
		char *string_data;
		void *object_data;
		void *pointer_data;
		int enum_data;
		void *boxed_data;

	} data;

	union
	{
		unsigned int subtype;
		char *specific_type;

	} u;

} GaimValue;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a new GaimValue.
 *
 * This function takes a type and, depending on that type, a sub-type
 * or specific type.
 *
 * If @a type is GAIM_TYPE_BOXED, the next parameter must be a
 * string representing the specific type.
 *
 * If @a type is GAIM_TYPE_SUBTYPE, the next parameter must be a
 * integer or enum representing the sub-type.
 *
 * If the subtype or specific type is not set when required, random
 * errors may occur. You have been warned.
 *
 * @param type The type.
 *
 * @return The new value.
 */
GaimValue *gaim_value_new(GaimType type, ...);

/**
 * Creates a new outgoing GaimValue.  If a value is an "outgoing" value
 * it means the value can be modified by plugins and scripts.
 *
 * This function takes a type and, depending on that type, a sub-type
 * or specific type.
 *
 * If @a type is GAIM_TYPE_BOXED, the next parameter must be a
 * string representing the specific type.
 *
 * If @a type is GAIM_TYPE_SUBTYPE, the next parameter must be a
 * integer or enum representing the sub-type.
 *
 * If the sub-type or specific type is not set when required, random
 * errors may occur. You have been warned.
 *
 * @param type The type.
 *
 * @return The new value.
 */
GaimValue *gaim_value_new_outgoing(GaimType type, ...);

/**
 * Destroys a GaimValue.
 *
 * @param value The value to destroy.
 */
void gaim_value_destroy(GaimValue *value);

/**
 * Duplicated a GaimValue.
 *
 * @param value The value to duplicate.
 *
 * @return The duplicate value.
 */
GaimValue *gaim_value_dup(const GaimValue *value);

/**
 * Returns a value's type.
 *
 * @param value The value whose type you want.
 *
 * @return The value's type.
 */
GaimType gaim_value_get_type(const GaimValue *value);

/**
 * Returns a value's subtype.
 *
 * If the value's type is not GAIM_TYPE_SUBTYPE, this will return 0.
 * Subtypes should never have a subtype of 0.
 *
 * @param value The value whose subtype you want.
 *
 * @return The value's subtype, or 0 if @a type is not GAIM_TYPE_SUBTYPE.
 */
unsigned int gaim_value_get_subtype(const GaimValue *value);

/**
 * Returns a value's specific type.
 *
 * If the value's type is not GAIM_TYPE_BOXED, this will return @c NULL.
 *
 * @param value The value whose specific type you want.
 *
 * @return The value's specific type, or @a NULL if not GAIM_TYPE_BOXED.
 */
const char *gaim_value_get_specific_type(const GaimValue *value);

/**
 * Returns whether or not the value is an outgoing value.
 *
 * @param value The value.
 *
 * @return TRUE if the value is outgoing, or FALSE otherwise.
 */
gboolean gaim_value_is_outgoing(const GaimValue *value);

/**
 * Sets the value's character data.
 *
 * @param value The value.
 * @param data The character data.
 */
void gaim_value_set_char(GaimValue *value, char data);

/**
 * Sets the value's unsigned character data.
 *
 * @param value The value.
 * @param data The unsigned character data.
 */
void gaim_value_set_uchar(GaimValue *value, unsigned char data);

/**
 * Sets the value's boolean data.
 *
 * @param value The value.
 * @param data The boolean data.
 */
void gaim_value_set_boolean(GaimValue *value, gboolean data);

/**
 * Sets the value's short integer data.
 *
 * @param value The value.
 * @param data The short integer data.
 */
void gaim_value_set_short(GaimValue *value, short data);

/**
 * Sets the value's unsigned short integer data.
 *
 * @param value The value.
 * @param data The unsigned short integer data.
 */
void gaim_value_set_ushort(GaimValue *value, unsigned short data);

/**
 * Sets the value's integer data.
 *
 * @param value The value.
 * @param data The integer data.
 */
void gaim_value_set_int(GaimValue *value, int data);

/**
 * Sets the value's unsigned integer data.
 *
 * @param value The value.
 * @param data The unsigned integer data.
 */
void gaim_value_set_uint(GaimValue *value, unsigned int data);

/**
 * Sets the value's long integer data.
 *
 * @param value The value.
 * @param data The long integer data.
 */
void gaim_value_set_long(GaimValue *value, long data);

/**
 * Sets the value's unsigned long integer data.
 *
 * @param value The value.
 * @param data The unsigned long integer data.
 */
void gaim_value_set_ulong(GaimValue *value, unsigned long data);

/**
 * Sets the value's 64-bit integer data.
 *
 * @param value The value.
 * @param data The 64-bit integer data.
 */
void gaim_value_set_int64(GaimValue *value, gint64 data);

/**
 * Sets the value's unsigned 64-bit integer data.
 *
 * @param value The value.
 * @param data The unsigned 64-bit integer data.
 */
void gaim_value_set_uint64(GaimValue *value, guint64 data);

/**
 * Sets the value's string data.
 *
 * @param value The value.
 * @param data The string data.
 */
void gaim_value_set_string(GaimValue *value, const char *data);

/**
 * Sets the value's object data.
 *
 * @param value The value.
 * @param data The object data.
 */
void gaim_value_set_object(GaimValue *value, void *data);

/**
 * Sets the value's pointer data.
 *
 * @param value The value.
 * @param data The pointer data.
 */
void gaim_value_set_pointer(GaimValue *value, void *data);

/**
 * Sets the value's enum data.
 *
 * @param value The value.
 * @param data The enum data.
 */
void gaim_value_set_enum(GaimValue *value, int data);

/**
 * Sets the value's boxed data.
 *
 * @param value The value.
 * @param data The boxed data.
 */
void gaim_value_set_boxed(GaimValue *value, void *data);

/**
 * Returns the value's character data.
 *
 * @param value The value.
 *
 * @return The character data.
 */
char gaim_value_get_char(const GaimValue *value);

/**
 * Returns the value's unsigned character data.
 *
 * @param value The value.
 *
 * @return The unsigned character data.
 */
unsigned char gaim_value_get_uchar(const GaimValue *value);

/**
 * Returns the value's boolean data.
 *
 * @param value The value.
 *
 * @return The boolean data.
 */
gboolean gaim_value_get_boolean(const GaimValue *value);

/**
 * Returns the value's short integer data.
 *
 * @param value The value.
 *
 * @return The short integer data.
 */
short gaim_value_get_short(const GaimValue *value);

/**
 * Returns the value's unsigned short integer data.
 *
 * @param value The value.
 *
 * @return The unsigned short integer data.
 */
unsigned short gaim_value_get_ushort(const GaimValue *value);

/**
 * Returns the value's integer data.
 *
 * @param value The value.
 *
 * @return The integer data.
 */
int gaim_value_get_int(const GaimValue *value);

/**
 * Returns the value's unsigned integer data.
 *
 * @param value The value.
 *
 * @return The unsigned integer data.
 */
unsigned int gaim_value_get_uint(const GaimValue *value);

/**
 * Returns the value's long integer data.
 *
 * @param value The value.
 *
 * @return The long integer data.
 */
long gaim_value_get_long(const GaimValue *value);

/**
 * Returns the value's unsigned long integer data.
 *
 * @param value The value.
 *
 * @return The unsigned long integer data.
 */
unsigned long gaim_value_get_ulong(const GaimValue *value);

/**
 * Returns the value's 64-bit integer data.
 *
 * @param value The value.
 *
 * @return The 64-bit integer data.
 */
gint64 gaim_value_get_int64(const GaimValue *value);

/**
 * Returns the value's unsigned 64-bit integer data.
 *
 * @param value The value.
 *
 * @return The unsigned 64-bit integer data.
 */
guint64 gaim_value_get_uint64(const GaimValue *value);

/**
 * Returns the value's string data.
 *
 * @param value The value.
 *
 * @return The string data.
 */
const char *gaim_value_get_string(const GaimValue *value);

/**
 * Returns the value's object data.
 *
 * @param value The value.
 *
 * @return The object data.
 */
void *gaim_value_get_object(const GaimValue *value);

/**
 * Returns the value's pointer data.
 *
 * @param value The value.
 *
 * @return The pointer data.
 */
void *gaim_value_get_pointer(const GaimValue *value);

/**
 * Returns the value's enum data.
 *
 * @param value The value.
 *
 * @return The enum data.
 */
int gaim_value_get_enum(const GaimValue *value);

/**
 * Returns the value's boxed data.
 *
 * @param value The value.
 *
 * @return The boxed data.
 */
void *gaim_value_get_boxed(const GaimValue *value);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_VALUE_H_ */
