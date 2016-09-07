/*
 * purple
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
 *
 */
#include "internal.h"
#include "chat.h"
#include "dbus-maybe.h"
#include "util.h"

#define PURPLE_CHAT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT, PurpleChatPrivate))

typedef struct _PurpleChatPrivate       PurpleChatPrivate;

/* Private data for a chat node */
struct _PurpleChatPrivate {
	char *alias;              /* The display name of this chat.             */
	PurpleAccount *account;   /* The account this chat is attached to       */
	GHashTable *components;   /* the stuff the protocol needs to know to
	                             join the chat                              */

	gboolean is_constructed;  /* Indicates if the chat has finished being
	                             constructed.                               */
};

/* Chat property enums */
enum
{
	PROP_0,
	PROP_ALIAS,
	PROP_ACCOUNT,
	PROP_COMPONENTS,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static PurpleBlistNode *blistnode_parent_class;
static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * API
 *****************************************************************************/

const char *purple_chat_get_name(PurpleChat *chat)
{
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	if ((priv->alias != NULL) && (*priv->alias != '\0'))
		return priv->alias;

	return purple_chat_get_name_only(chat);
}

const char *purple_chat_get_name_only(PurpleChat *chat)
{
	char *ret = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(priv->account));

	if (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT_IFACE, info)) {
		PurpleProtocolChatEntry *pce;
		GList *parts = purple_protocol_chat_iface_info(protocol, purple_account_get_connection(priv->account));
		pce = parts->data;
		ret = g_hash_table_lookup(priv->components, pce->identifier);
		g_list_foreach(parts, (GFunc)g_free, NULL);
		g_list_free(parts);
	}

	return ret;
}

void
purple_chat_set_alias(PurpleChat *chat, const char *alias)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	char *old_alias;
	char *new_alias = NULL;
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strequal(priv->alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->alias = new_alias;
	else {
		priv->alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	g_object_notify_by_pspec(G_OBJECT(chat), properties[PROP_ALIAS]);

	if (ops) {
		if (ops->save_node)
			ops->save_node(PURPLE_BLIST_NODE(chat));
		if (ops->update)
			ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(chat));
	}

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 chat, old_alias);
	g_free(old_alias);
}

PurpleGroup *
purple_chat_get_group(PurpleChat *chat)
{
	g_return_val_if_fail(PURPLE_IS_CHAT(chat), NULL);

	return PURPLE_GROUP(PURPLE_BLIST_NODE(chat)->parent);
}

PurpleAccount *
purple_chat_get_account(PurpleChat *chat)
{
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

GHashTable *
purple_chat_get_components(PurpleChat *chat)
{
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->components;
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
static void
purple_chat_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChat *chat = PURPLE_CHAT(obj);
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	switch (param_id) {
		case PROP_ALIAS:
			if (priv->is_constructed)
				purple_chat_set_alias(chat, g_value_get_string(value));
			else
				priv->alias =
					purple_utf8_strip_unprintables(g_value_get_string(value));
			break;
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_COMPONENTS:
			priv->components = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_chat_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChat *chat = PURPLE_CHAT(obj);
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	switch (param_id) {
		case PROP_ALIAS:
			g_value_set_string(value, priv->alias);
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_chat_get_account(chat));
			break;
		case PROP_COMPONENTS:
			g_value_set_pointer(value, purple_chat_get_components(chat));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_chat_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_CHAT(instance), PurpleChat);
}

/* Called when done constructing */
static void
purple_chat_constructed(GObject *object)
{
	PurpleChat *chat = PURPLE_CHAT(object);
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	G_OBJECT_CLASS(blistnode_parent_class)->constructed(object);

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node(PURPLE_BLIST_NODE(chat));

	priv->is_constructed = TRUE;
}

/* GObject finalize function */
static void
purple_chat_finalize(GObject *object)
{
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(object);

	g_free(priv->alias);
	g_hash_table_destroy(priv->components);

	PURPLE_DBUS_UNREGISTER_POINTER(object);

	G_OBJECT_CLASS(blistnode_parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_chat_class_init(PurpleChatClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	blistnode_parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_chat_finalize;

	/* Setup properties */
	obj_class->get_property = purple_chat_get_property;
	obj_class->set_property = purple_chat_set_property;
	obj_class->constructed = purple_chat_constructed;

	g_type_class_add_private(klass, sizeof(PurpleChatPrivate));

	properties[PROP_ALIAS] = g_param_spec_string(
		"alias",
		"Alias",
		"The alias for the chat.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account",
		"Account",
		"The account that the chat belongs to.",
		PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_COMPONENTS] = g_param_spec_pointer(
		"components",
		"Components",
		"The protocol components of the chat.",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_chat_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleChatClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_chat_class_init,
			NULL,
			NULL,
			sizeof(PurpleChat),
			0,
			(GInstanceInitFunc)purple_chat_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_BLIST_NODE,
				"PurpleChat",
				&info, 0);
	}

	return type;
}

PurpleChat *
purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(components != NULL, NULL);

	return g_object_new(PURPLE_TYPE_CHAT,
			"account",     account,
			"alias",       alias,
			"components",  components,
			NULL);
}
