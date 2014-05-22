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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "dbus-maybe.h"
#include "glibcompat.h"

#include "debug.h"
#include "message.h"

#define PURPLE_MESSAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MESSAGE, PurpleMessagePrivate))

typedef struct {
	guint id;
	gchar *who;
	gchar *contents;
	guint64 msgtime;
	PurpleMessageFlags flags;
} PurpleMessagePrivate;

enum
{
	PROP_0,
	PROP_ID,
	PROP_WHO,
	PROP_CONTENTS,
	PROP_TIME,
	PROP_FLAGS,
	PROP_LAST
};

static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

static GHashTable *messages = NULL;

/******************************************************************************
 * API implementation
 ******************************************************************************/

PurpleMessage *
purple_message_new(const gchar *who, const gchar *contents,
	PurpleMessageFlags flags)
{
	return g_object_new(PURPLE_TYPE_MESSAGE,
		"who", who,
		"contents", contents,
		"flags", flags,
		"time", (guint64)time(NULL),
		NULL);
}

guint
purple_message_get_id(PurpleMessage *msg)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->id;
}

PurpleMessage *
purple_message_find_by_id(guint id)
{
	g_return_val_if_fail(id > 0, NULL);

	return g_hash_table_lookup(messages, GINT_TO_POINTER(id));
}

const gchar *
purple_message_get_who(PurpleMessage *msg)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;
}

void
purple_message_set_contents(PurpleMessage *msg, const gchar *cont)
{
	g_object_set(msg, "contents", cont, NULL);
}

const gchar *
purple_message_get_contents(PurpleMessage *msg)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->contents;
}

gboolean
purple_message_is_empty(PurpleMessage *msg)
{
	const gchar *cont = purple_message_get_contents(msg);

	return (cont == NULL || cont[0] == '\0');
}

void
purple_message_set_time(PurpleMessage *msg, guint64 msgtime)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_if_fail(priv != NULL);

	priv->msgtime = msgtime;
}

guint64
purple_message_get_time(PurpleMessage *msg)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->msgtime;
}

PurpleMessageFlags
purple_message_get_flags(PurpleMessage *msg)
{
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->flags;
}

/******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_message_init(GTypeInstance *instance, gpointer klass)
{
	static guint max_id = 0;

	PurpleMessage *msg = PURPLE_MESSAGE(instance);
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(msg);
	PURPLE_DBUS_REGISTER_POINTER(msg, PurpleMessage);

	priv->id = ++max_id;
	g_hash_table_insert(messages, GINT_TO_POINTER(max_id), msg);
}

static void
purple_message_finalize(GObject *obj)
{
	PurpleMessage *message = PURPLE_MESSAGE(obj);
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(message);

	g_free(priv->who);
	g_free(priv->contents);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_message_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleMessage *message = PURPLE_MESSAGE(object);
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(message);

	switch (par_id) {
		case PROP_ID:
			g_value_set_uint(value, priv->id);
			break;
		case PROP_WHO:
			g_value_set_string(value, priv->who);
			break;
		case PROP_CONTENTS:
			g_value_set_string(value, priv->contents);
			break;
		case PROP_TIME:
			g_value_set_uint64(value, priv->msgtime);
			break;
		case PROP_FLAGS:
			g_value_set_uint(value, priv->flags);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_message_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleMessage *message = PURPLE_MESSAGE(object);
	PurpleMessagePrivate *priv = PURPLE_MESSAGE_GET_PRIVATE(message);

	switch (par_id) {
		case PROP_WHO:
			g_free(priv->who);
			priv->who = g_strdup(g_value_get_string(value));
			break;
		case PROP_CONTENTS:
			g_free(priv->contents);
			priv->contents = g_strdup(g_value_get_string(value));
			break;
		case PROP_TIME:
			priv->msgtime = g_value_get_uint64(value);
			break;
		case PROP_FLAGS:
			priv->flags = g_value_get_uint(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_message_class_init(PurpleMessageClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleMessagePrivate));

	gobj_class->finalize = purple_message_finalize;
	gobj_class->get_property = purple_message_get_property;
	gobj_class->set_property = purple_message_set_property;

	properties[PROP_ID] = g_param_spec_uint("id",
		"ID", "The session-unique message id",
		0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_WHO] = g_param_spec_string("who",
		"Who", "The nick of the person, who sent the message (for "
		"incoming messages) or the recipient (for outgoing). "
		"Unused for outgoing chat messages.",
		NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_CONTENTS] = g_param_spec_string("contents",
		"Contents", "The message text",
		NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_TIME] = g_param_spec_uint64("time",
		"Time", "Message time",
		0, G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	/* XXX: it should be spec_enum, but PurpleMessageFlags isn't
	 * gobjectified (yet) */
	properties[PROP_FLAGS] = g_param_spec_uint("flags",
		"Flags", "Bitwise set of #PurpleMessageFlags flags",
		0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
}

GType
purple_message_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleMessageClass),
			.class_init = (GClassInitFunc)purple_message_class_init,
			.instance_size = sizeof(PurpleMessage),
			.instance_init = purple_message_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleMessage", &info, 0);
	}

	return type;
}

void
_purple_message_init(void)
{
	messages = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, g_object_unref);
}

void
_purple_message_uninit(void)
{
	g_hash_table_destroy(messages);
	messages = NULL;
}
