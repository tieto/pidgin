/** MySpaceIM protocol messages
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@soc.pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _MYSPACE_MESSAGE_H
#define _MYSPACE_MESSAGE_H

#include <glib.h>

/* Types */
#define MsimMessage GList               /* #define instead of typedef to avoid casting */
typedef struct _MsimMessageElement
{
	const gchar *name;              /**< Textual name of element. */
	gboolean dynamic_name;          /**< TRUE if 'name' is a dynamic string to be freed, not static. */
	guint type;                     /**< MSIM_TYPE_* code. */
	gpointer data;                  /**< Pointer to data, or GUINT_TO_POINTER for int/bool. */
} MsimMessageElement;

typedef gchar MsimMessageType;

#define msim_msg_get_next_element_node(msg)    ((MsimMessage *)(msg->next))

/* Protocol field types */
#define MSIM_TYPE_RAW            '-'
#define MSIM_TYPE_INTEGER        'i'
#define MSIM_TYPE_STRING         's'
#define MSIM_TYPE_BINARY         'b'
#define MSIM_TYPE_BOOLEAN        'f'
#define MSIM_TYPE_DICTIONARY     'd'
#define MSIM_TYPE_LIST           'l'

gchar *msim_escape(const gchar *msg);
gchar *msim_unescape(const gchar *msg);

MsimMessage *msim_msg_new(gchar *first_key, ...);
/* No sentinel attribute, because can leave off varargs if not_empty is FALSE. */

MsimMessage *msim_msg_clone(MsimMessage *old);
void msim_msg_free_element_data(MsimMessageElement *elem);
void msim_msg_free(MsimMessage *msg);
MsimMessage *msim_msg_append(MsimMessage *msg, const gchar *name, MsimMessageType type, gpointer data);
MsimMessage *msim_msg_insert_before(MsimMessage *msg, const gchar *name_before, const gchar *name, MsimMessageType type, gpointer data);
gchar *msim_msg_dump_to_str(MsimMessage *msg);
gchar *msim_msg_pack_element_data(MsimMessageElement *elem);
void msim_msg_dump(const char *fmt_string, MsimMessage *msg);
gchar *msim_msg_pack(MsimMessage *msg);
gchar *msim_msg_pack_dict(MsimMessage *msg);

GList *msim_msg_list_copy(GList *old);
void msim_msg_list_free(GList *l);
GList *msim_msg_list_parse(const gchar *raw);

/* Defined in myspace.h */
struct _MsimSession;

/* Based on http://permalink.gmane.org/gmane.comp.parsers.sparse/695 
 * Define macros for useful gcc attributes. */
#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#define FORMAT_ATTR(pos) __attribute__ ((__format__ (__printf__, pos, pos+1)))
#define NORETURN_ATTR __attribute__ ((__noreturn__))
/* __sentinel__ attribute was introduced in gcc 3.5 */
#if (GCC_VERSION >= 3005)
	#define SENTINEL_ATTR __attribute__ ((__sentinel__(0)))
#else
	#define SENTINEL_ATTR
#endif /* gcc >= 3.5 */
#else
	#define FORMAT_ATTR(pos)
	#define NORETURN_ATTR
	#define SENTINEL_ATTR
#endif 

/* Cause gcc to emit "a missing sentinel in function call" if forgot
 * to write NULL as last, terminating parameter. */
gboolean msim_send(struct _MsimSession *session, ...) SENTINEL_ATTR;

gboolean msim_msg_send(struct _MsimSession *session, MsimMessage *msg);

MsimMessage *msim_parse(gchar *raw);
MsimMessage *msim_msg_dictionary_parse(gchar *raw);

MsimMessageElement *msim_msg_get(MsimMessage *msg, const gchar *name);

/* Retrieve data by name */
gchar *msim_msg_get_string(MsimMessage *msg, const gchar *name);
GList *msim_msg_get_list(MsimMessage *msg, const gchar *name);
MsimMessage *msim_msg_get_dictionary(MsimMessage *msg, const gchar *name);
guint msim_msg_get_integer(MsimMessage *msg, const gchar *name);
gboolean msim_msg_get_binary(MsimMessage *msg, const gchar *name, gchar **binary_data, gsize *binary_length);

/* Retrieve data by element (MsimMessageElement *), returned from msim_msg_get() */
gchar *msim_msg_get_string_from_element(MsimMessageElement *elem);
GList *msim_msg_get_list_from_element(MsimMessageElement *elem);
MsimMessage *msim_msg_get_dictionary_from_element(MsimMessageElement *elem);
guint msim_msg_get_integer_from_element(MsimMessageElement *elem);
gboolean msim_msg_get_binary_from_element(MsimMessageElement *elem, 
		gchar **binary_data, gsize *binary_length);

#endif /* _MYSPACE_MESSAGE_H */
