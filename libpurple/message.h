/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_MESSAGE_H_
#define _PURPLE_MESSAGE_H_
/**
 * SECTION:message
 * @include:message.h
 * @section_id: libpurple-message
 * @short_description: serializable messages
 * @title: Message model
 *
 * #PurpleMessage object collects data about a certain (incoming or outgoing) message.
 * It (TODO: will be) serializable, so it can be stored in log and retrieved
 * with any metadata.
 */

#include <glib-object.h>

typedef struct _PurpleMessage PurpleMessage;
typedef struct _PurpleMessageClass PurpleMessageClass;

#define PURPLE_TYPE_MESSAGE            (purple_message_get_type())
#define PURPLE_MESSAGE(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PURPLE_TYPE_MESSAGE, PurpleMessage))
#define PURPLE_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MESSAGE, PurpleMessageClass))
#define PURPLE_IS_MESSAGE(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PURPLE_TYPE_MESSAGE))
#define PURPLE_IS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MESSAGE))
#define PURPLE_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MESSAGE, PurpleMessageClass))

/**
 * PurpleMessage:
 *
 * An message data container.
 */
struct _PurpleMessage
{
	/*< private >*/
	GObject parent;
};

/**
 * PurpleMessageClass:
 *
 * Base class for #PurpleMessage objects.
 */
struct _PurpleMessageClass
{
	/*< private >*/
	GObjectClass parent_class;

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_message_get_type:
 *
 * Returns: the #GType for a message.
 */
GType
purple_message_get_type(void);

PurpleMessage *
purple_message_new_outgoing(const gchar *who, const gchar *contents,
	PurpleMessageFlags flags);

PurpleMessage *
purple_message_new_incoming(const gchar *who, const gchar *contents,
	PurpleMessageFlags flags, guint64 timestamp);

PurpleMessage *
purple_message_new_system(const gchar *contents, PurpleMessageFlags flags);

guint
purple_message_get_id(const PurpleMessage *msg);

PurpleMessage *
purple_message_find_by_id(guint id);

const gchar *
purple_message_get_author(const PurpleMessage *msg);

const gchar *
purple_message_get_recipient(const PurpleMessage *msg);

void
purple_message_set_author_alias(PurpleMessage *msg, const gchar *alias);

const gchar *
purple_message_get_author_alias(const PurpleMessage *msg);

void
purple_message_set_contents(PurpleMessage *msg, const gchar *cont);

const gchar *
purple_message_get_contents(const PurpleMessage *msg);

gboolean
purple_message_is_empty(const PurpleMessage *msg);

void
purple_message_set_time(PurpleMessage *msg, guint64 msgtime);

guint64
purple_message_get_time(const PurpleMessage *msg);

void
purple_message_set_flags(PurpleMessage *msg, PurpleMessageFlags flags);

PurpleMessageFlags
purple_message_get_flags(const PurpleMessage *msg);

void
_purple_message_init(void);

void
_purple_message_uninit(void);

G_END_DECLS

#endif /* _PURPLE_MESSAGE_H_ */
