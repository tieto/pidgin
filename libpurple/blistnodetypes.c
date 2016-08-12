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
#include "glibcompat.h"
#include "dbus-maybe.h"
#include "debug.h"

#define PURPLE_BUDDY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BUDDY, PurpleBuddyPrivate))

typedef struct _PurpleBuddyPrivate      PurpleBuddyPrivate;

#define PURPLE_CHAT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT, PurpleChatPrivate))

typedef struct _PurpleChatPrivate       PurpleChatPrivate;

/**************************************************************************/
/* Private data                                                           */
/**************************************************************************/
/* Private data for a buddy. */
struct _PurpleBuddyPrivate {
	char *name;                  /* The name of the buddy.                  */
	char *local_alias;           /* The user-set alias of the buddy         */
	char *server_alias;          /* The server-specified alias of the buddy.
	                                (i.e. MSN "Friendly Names")             */
	void *proto_data;            /* This allows the protocol to associate
	                                whatever data it wants with a buddy.    */
	PurpleBuddyIcon *icon;       /* The buddy icon.                         */
	PurpleAccount *account;      /* the account this buddy belongs to       */
	PurplePresence *presence;    /* Presense information of the buddy       */
	PurpleMediaCaps media_caps;  /* The media capabilities of the buddy.    */

	gboolean is_constructed;     /* Indicates if the buddy has finished
	                                being constructed.                      */
};

/* Buddy property enums */
enum
{
	BUDDY_PROP_0,
	BUDDY_PROP_NAME,
	BUDDY_PROP_LOCAL_ALIAS,
	BUDDY_PROP_SERVER_ALIAS,
	BUDDY_PROP_ICON,
	BUDDY_PROP_ACCOUNT,
	BUDDY_PROP_PRESENCE,
	BUDDY_PROP_MEDIA_CAPS,
	BUDDY_PROP_LAST
};

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
	CHAT_PROP_0,
	CHAT_PROP_ALIAS,
	CHAT_PROP_ACCOUNT,
	CHAT_PROP_COMPONENTS,
	CHAT_PROP_LAST
};

static PurpleBlistNode     *blistnode_parent_class;

static GParamSpec *bd_properties[BUDDY_PROP_LAST];
static GParamSpec *ch_properties[CHAT_PROP_LAST];

static gboolean
purple_strings_are_different(const char *one, const char *two)
{
	return !((one && two && g_utf8_collate(one, two) == 0) ||
			((one == NULL || *one == '\0') && (two == NULL || *two == '\0')));
}

/**************************************************************************/
/* Buddy API                                                              */
/**************************************************************************/

void
purple_buddy_set_icon(PurpleBuddy *buddy, PurpleBuddyIcon *icon)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	if (priv->icon != icon)
	{
		purple_buddy_icon_unref(priv->icon);
		priv->icon = (icon != NULL ? purple_buddy_icon_ref(icon) : NULL);

		g_object_notify_by_pspec(G_OBJECT(buddy),
				bd_properties[BUDDY_PROP_ICON]);
	}

	purple_signal_emit(purple_blist_get_handle(), "buddy-icon-changed", buddy);

	if (ops && ops->update)
		ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(buddy));
}

PurpleBuddyIcon *
purple_buddy_get_icon(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->icon;
}

PurpleAccount *
purple_buddy_get_account(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

void
purple_buddy_set_name(PurpleBuddy *buddy, const char *name)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	g_return_if_fail(priv != NULL);

	purple_blist_update_buddies_cache(buddy, name);

	g_free(priv->name);
	priv->name = purple_utf8_strip_unprintables(name);

	g_object_notify_by_pspec(G_OBJECT(buddy), bd_properties[BUDDY_PROP_NAME]);

	if (ops) {
		if (ops->save_node)
			ops->save_node(PURPLE_BLIST_NODE(buddy));
		if (ops->update)
			ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(buddy));
	}
}

const char *
purple_buddy_get_name(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

gpointer
purple_buddy_get_protocol_data(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void
purple_buddy_set_protocol_data(PurpleBuddy *buddy, gpointer data)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	priv->proto_data = data;
}

const char *purple_buddy_get_alias_only(PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	if ((priv->local_alias != NULL) && (*priv->local_alias != '\0')) {
		return priv->local_alias;
	} else if ((priv->server_alias != NULL) &&
		   (*priv->server_alias != '\0')) {

		return priv->server_alias;
	}

	return NULL;
}

const char *purple_buddy_get_contact_alias(PurpleBuddy *buddy)
{
	PurpleContact *c;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	/* Search for an alias for the buddy. In order of precedence: */
	/* The local buddy alias */
	if (priv->local_alias != NULL)
		return priv->local_alias;

	/* The contact alias */
	c = purple_buddy_get_contact(buddy);
	if ((c != NULL) && (purple_contact_get_alias(c) != NULL))
		return purple_contact_get_alias(c);

	/* The server alias */
	if ((priv->server_alias) && (*priv->server_alias))
		return priv->server_alias;

	/* The buddy's user name (i.e. no alias) */
	return priv->name;
}

const char *purple_buddy_get_alias(PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	/* Search for an alias for the buddy. In order of precedence: */
	/* The buddy alias */
	if (priv->local_alias != NULL)
		return priv->local_alias;

	/* The server alias */
	if ((priv->server_alias) && (*priv->server_alias))
		return priv->server_alias;

	/* The buddy's user name (i.e. no alias) */
	return priv->name;
}

void
purple_buddy_set_local_alias(PurpleBuddy *buddy, const char *alias)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	char *old_alias;
	char *new_alias = NULL;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->local_alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->local_alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->local_alias = new_alias;
	else {
		priv->local_alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	g_object_notify_by_pspec(G_OBJECT(buddy),
			bd_properties[BUDDY_PROP_LOCAL_ALIAS]);

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(buddy));

	if (ops && ops->update)
		ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(buddy));

	im = purple_conversations_find_im_with_account(priv->name,
											   priv->account);
	if (im)
		purple_conversation_autoset_title(PURPLE_CONVERSATION(im));

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

const char *purple_buddy_get_local_alias(PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->local_alias;
}

void
purple_buddy_set_server_alias(PurpleBuddy *buddy, const char *alias)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	char *old_alias;
	char *new_alias = NULL;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0') && g_utf8_validate(alias, -1, NULL))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->server_alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->server_alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->server_alias = new_alias;
	else {
		priv->server_alias = NULL;
		g_free(new_alias); /* could be "\0"; */
	}

	g_object_notify_by_pspec(G_OBJECT(buddy),
			bd_properties[BUDDY_PROP_SERVER_ALIAS]);

	if (ops) {
		if (ops->save_node)
			ops->save_node(PURPLE_BLIST_NODE(buddy));
		if (ops->update)
			ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(buddy));
	}

	im = purple_conversations_find_im_with_account(priv->name,
											   priv->account);
	if (im)
		purple_conversation_autoset_title(PURPLE_CONVERSATION(im));

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

const char *purple_buddy_get_server_alias(PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	if ((priv->server_alias) && (*priv->server_alias))
	    return priv->server_alias;

	return NULL;
}

PurpleContact *purple_buddy_get_contact(PurpleBuddy *buddy)
{
	g_return_val_if_fail(PURPLE_IS_BUDDY(buddy), NULL);

	return PURPLE_CONTACT(PURPLE_BLIST_NODE(buddy)->parent);
}

PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->presence;
}

void
purple_buddy_update_status(PurpleBuddy *buddy, PurpleStatus *old_status)
{
	PurpleStatus *status;
	PurpleBlistNode *cnode;
	PurpleContact *contact;
	PurpleCountingNode *contact_counter, *group_counter;
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	status = purple_presence_get_active_status(priv->presence);

	purple_debug_info("blistnodetypes", "Updating buddy status for %s (%s)\n",
			priv->name, purple_account_get_protocol_name(priv->account));

	if (purple_status_is_online(status) &&
		!purple_status_is_online(old_status)) {

		purple_signal_emit(purple_blist_get_handle(), "buddy-signed-on", buddy);

		cnode = PURPLE_BLIST_NODE(buddy)->parent;
		contact = PURPLE_CONTACT(cnode);
		contact_counter = PURPLE_COUNTING_NODE(contact);
		group_counter = PURPLE_COUNTING_NODE(cnode->parent);

		purple_counting_node_change_online_count(contact_counter, +1);
		if (purple_counting_node_get_online_count(contact_counter) == 1)
			purple_counting_node_change_online_count(group_counter, +1);
	} else if (!purple_status_is_online(status) &&
				purple_status_is_online(old_status)) {

		purple_blist_node_set_int(PURPLE_BLIST_NODE(buddy), "last_seen", time(NULL));
		purple_signal_emit(purple_blist_get_handle(), "buddy-signed-off", buddy);

		cnode = PURPLE_BLIST_NODE(buddy)->parent;
		contact = PURPLE_CONTACT(cnode);
		contact_counter = PURPLE_COUNTING_NODE(contact);
		group_counter = PURPLE_COUNTING_NODE(cnode->parent);

		purple_counting_node_change_online_count(contact_counter, -1);
		if (purple_counting_node_get_online_count(contact_counter) == 0)
			purple_counting_node_change_online_count(group_counter, -1);
	} else {
		purple_signal_emit(purple_blist_get_handle(),
		                 "buddy-status-changed", buddy, old_status,
		                 status);
	}

	/*
	 * This function used to only call the following two functions if one of
	 * the above signals had been triggered, but that's not good, because
	 * if someone's away message changes and they don't go from away to back
	 * to away then no signal is triggered.
	 *
	 * It's a safe assumption that SOMETHING called this function.  PROBABLY
	 * because something, somewhere changed.  Calling the stuff below
	 * certainly won't hurt anything.  Unless you're on a K6-2 300.
	 */
	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	if (ops && ops->update)
		ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(buddy));
}

PurpleMediaCaps purple_buddy_get_media_caps(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->media_caps;
}

void purple_buddy_set_media_caps(PurpleBuddy *buddy, PurpleMediaCaps media_caps)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	priv->media_caps = media_caps;

	g_object_notify_by_pspec(G_OBJECT(buddy),
			bd_properties[BUDDY_PROP_MEDIA_CAPS]);
}

PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy)
{
	g_return_val_if_fail(PURPLE_IS_BUDDY(buddy), NULL);

	if (PURPLE_BLIST_NODE(buddy)->parent == NULL)
		return purple_blist_get_default_group();

	return PURPLE_GROUP(PURPLE_BLIST_NODE(buddy)->parent->parent);
}

/**************************************************************************
 * GObject code for PurpleBuddy
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_buddy_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(obj);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	switch (param_id) {
		case BUDDY_PROP_NAME:
			if (priv->is_constructed)
				purple_buddy_set_name(buddy, g_value_get_string(value));
			else
				priv->name =
					purple_utf8_strip_unprintables(g_value_get_string(value));
			break;
		case BUDDY_PROP_LOCAL_ALIAS:
			if (priv->is_constructed)
				purple_buddy_set_local_alias(buddy, g_value_get_string(value));
			else
				priv->local_alias =
					purple_utf8_strip_unprintables(g_value_get_string(value));
			break;
		case BUDDY_PROP_SERVER_ALIAS:
			purple_buddy_set_server_alias(buddy, g_value_get_string(value));
			break;
		case BUDDY_PROP_ICON:
			purple_buddy_set_icon(buddy, g_value_get_pointer(value));
			break;
		case BUDDY_PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case BUDDY_PROP_MEDIA_CAPS:
			purple_buddy_set_media_caps(buddy, g_value_get_enum(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_buddy_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(obj);

	switch (param_id) {
		case BUDDY_PROP_NAME:
			g_value_set_string(value, purple_buddy_get_name(buddy));
			break;
		case BUDDY_PROP_LOCAL_ALIAS:
			g_value_set_string(value, purple_buddy_get_local_alias(buddy));
			break;
		case BUDDY_PROP_SERVER_ALIAS:
			g_value_set_string(value, purple_buddy_get_server_alias(buddy));
			break;
		case BUDDY_PROP_ICON:
			g_value_set_pointer(value, purple_buddy_get_icon(buddy));
			break;
		case BUDDY_PROP_ACCOUNT:
			g_value_set_object(value, purple_buddy_get_account(buddy));
			break;
		case BUDDY_PROP_PRESENCE:
			g_value_set_object(value, purple_buddy_get_presence(buddy));
			break;
		case BUDDY_PROP_MEDIA_CAPS:
			g_value_set_enum(value, purple_buddy_get_media_caps(buddy));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_buddy_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_BUDDY(instance), PurpleBuddy);
}

/* Called when done constructing */
static void
purple_buddy_constructed(GObject *object)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(object);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	G_OBJECT_CLASS(blistnode_parent_class)->constructed(object);

	priv->presence = PURPLE_PRESENCE(purple_buddy_presence_new(buddy));
	purple_presence_set_status_active(priv->presence, "offline", TRUE);

	if (ops && ops->new_node)
		ops->new_node((PurpleBlistNode *)buddy);

	priv->is_constructed = TRUE;
}

/* GObject dispose function */
static void
purple_buddy_dispose(GObject *object)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(object);

	if (priv->icon) {
		purple_buddy_icon_unref(priv->icon);
		priv->icon = NULL;
	}

	if (priv->presence) {
		g_object_unref(priv->presence);
		priv->presence = NULL;
	}

	G_OBJECT_CLASS(blistnode_parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_buddy_finalize(GObject *object)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(object);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);
	PurpleProtocol *protocol;

	/*
	 * Tell the owner protocol that we're about to free the buddy so it
	 * can free proto_data
	 */
	protocol = purple_protocols_find(purple_account_get_protocol_id(priv->account));
	if (protocol)
		purple_protocol_client_iface_buddy_free(protocol, buddy);

	g_free(priv->name);
	g_free(priv->local_alias);
	g_free(priv->server_alias);

	PURPLE_DBUS_UNREGISTER_POINTER(buddy);

	G_OBJECT_CLASS(blistnode_parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_buddy_class_init(PurpleBuddyClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	blistnode_parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_buddy_dispose;
	obj_class->finalize = purple_buddy_finalize;

	/* Setup properties */
	obj_class->get_property = purple_buddy_get_property;
	obj_class->set_property = purple_buddy_set_property;
	obj_class->constructed = purple_buddy_constructed;

	g_type_class_add_private(klass, sizeof(PurpleBuddyPrivate));

	bd_properties[BUDDY_PROP_NAME] = g_param_spec_string("name", "Name",
				"The name of the buddy.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_LOCAL_ALIAS] = g_param_spec_string("local-alias",
				"Local alias",
				"Local alias of thee buddy.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_SERVER_ALIAS] = g_param_spec_string("server-alias",
				"Server alias",
				"Server-side alias of the buddy.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_ICON] = g_param_spec_pointer("icon", "Buddy icon",
				"The icon for the buddy.",
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_ACCOUNT] = g_param_spec_object("account",
				"Account",
				"The account for the buddy.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_PRESENCE] = g_param_spec_object("presence",
				"Presence",
				"The status information for the buddy.", PURPLE_TYPE_PRESENCE,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	bd_properties[BUDDY_PROP_MEDIA_CAPS] = g_param_spec_enum("media-caps",
				"Media capabilities",
				"The media capabilities of the buddy.",
				PURPLE_MEDIA_TYPE_CAPS, PURPLE_MEDIA_CAPS_NONE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, BUDDY_PROP_LAST,
				bd_properties);
}

GType
purple_buddy_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleBuddyClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_buddy_class_init,
			NULL,
			NULL,
			sizeof(PurpleBuddy),
			0,
			(GInstanceInitFunc)purple_buddy_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_BLIST_NODE,
				"PurpleBuddy",
				&info, 0);
	}

	return type;
}

PurpleBuddy *
purple_buddy_new(PurpleAccount *account, const char *name, const char *alias)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_object_new(PURPLE_TYPE_BUDDY,
			"account",      account,
			"name",         name,
			"local-alias",  alias,
			NULL);
}

/**************************************************************************/
/* Chat API                                                               */
/**************************************************************************/

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

	if (!purple_strings_are_different(priv->alias, new_alias)) {
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

	g_object_notify_by_pspec(G_OBJECT(chat), ch_properties[CHAT_PROP_ALIAS]);

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

/**************************************************************************
 * GObject code for PurpleChat
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_chat_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChat *chat = PURPLE_CHAT(obj);
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	switch (param_id) {
		case CHAT_PROP_ALIAS:
			if (priv->is_constructed)
				purple_chat_set_alias(chat, g_value_get_string(value));
			else
				priv->alias =
					purple_utf8_strip_unprintables(g_value_get_string(value));
			break;
		case CHAT_PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case CHAT_PROP_COMPONENTS:
			priv->components = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_chat_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChat *chat = PURPLE_CHAT(obj);
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	switch (param_id) {
		case CHAT_PROP_ALIAS:
			g_value_set_string(value, priv->alias);
			break;
		case CHAT_PROP_ACCOUNT:
			g_value_set_object(value, purple_chat_get_account(chat));
			break;
		case CHAT_PROP_COMPONENTS:
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

	ch_properties[CHAT_PROP_ALIAS] = g_param_spec_string("alias", "Alias",
				"The alias for the chat.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	ch_properties[CHAT_PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The account that the chat belongs to.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	ch_properties[CHAT_PROP_COMPONENTS] = g_param_spec_pointer("components",
				"Components",
				"The protocol components of the chat.",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, CHAT_PROP_LAST, ch_properties);
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

