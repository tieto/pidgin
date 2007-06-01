/** MySpaceIM protocol messages
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@homing.pidgin.im>
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

#ifndef _MYSPACE_MESSAGE_H
#define _MYSPACE_MESSAGE_H

#include <glib.h>

/* Types */
#define MsimMessage GList		/* #define instead of typedef to avoid casting */
typedef struct _MsimMessageElement
{
	gchar *name;					/**< Textual name of element. */
	guint type;						/**< MSIM_TYPE_* code. */
	gpointer data;					/**< Pointer to data, or GUINT_TO_POINTER for int/bool. */
} MsimMessageElement;

typedef gchar MsimMessageType;

/* Protocol field types */
#define MSIM_TYPE_INTEGER		'i'
#define MSIM_TYPE_STRING		's'
#define MSIM_TYPE_BINARY		'b'
#define MSIM_TYPE_BOOLEAN		'f'
#define MSIM_TYPE_DICTIONARY	'd'
#define MSIM_TYPE_LIST			'l'

MsimMessage *msim_msg_new(void);
void msim_msg_free(MsimMessage *msg);
MsimMessage *msim_msg_append(MsimMessage *msg, gchar *name, MsimMessageType type, gpointer data);
gchar *msim_msg_debug_string(MsimMessage *msg);
gchar *msim_msg_pack(MsimMessage *msg);

/* Defined in myspace.h */
struct _MsimSession;

gboolean msim_send(struct _MsimSession *session, ...);
gboolean msim_msg_send(struct _MsimSession *session, MsimMessage *msg);

MsimMessage *msim_parse(gchar *raw);
GHashTable *msim_parse_body(const gchar *body_str);

MsimMessageElement *msim_msg_get(MsimMessage *msg, gchar *name);
gchar *msim_msg_get_string(MsimMessage *msg, gchar *name);
guint msim_msg_get_integer(MsimMessage *msg, gchar *name);
gboolean msim_msg_get_binary(MsimMessage *msg, gchar *name, gchar **binary_data, guint *binary_length);

#endif /* _MYSPACE_MESSAGE_H */
