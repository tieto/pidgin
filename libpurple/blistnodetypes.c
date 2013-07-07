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
#include "blistnodetypes.h"

#define PURPLE_BUDDY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BUDDY, PurpleBuddyPrivate))

/** @copydoc _PurpleBuddyPrivate */
typedef struct _PurpleBuddyPrivate      PurpleBuddyPrivate;

#define PURPLE_CONTACT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CONTACT, PurpleContactPrivate))

/** @copydoc _PurpleContactPrivate */
typedef struct _PurpleContactPrivate    PurpleContactPrivate;

#define PURPLE_GROUP_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_GROUP, PurpleGroupPrivate))

/** @copydoc _PurpleGroupPrivate */
typedef struct _PurpleGroupPrivate      PurpleGroupPrivate;

#define PURPLE_CHAT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT, PurpleChatPrivate))

/** @copydoc _PurpleChatPrivate */
typedef struct _PurpleChatPrivate       PurpleChatPrivate;

/**************************************************************************/
/* Private data                                                           */
/**************************************************************************/
/** Private data for a buddy. */
struct _PurpleBuddyPrivate {
	char *name;                  /**< The name of the buddy.                  */
	char *local_alias;           /**< The user-set alias of the buddy         */
	char *server_alias;          /**< The server-specified alias of the buddy.
	                                  (i.e. MSN "Friendly Names")             */
	void *proto_data;            /**< TODO remove - use protocol subclasses
	                                  This allows the prpl to associate
	                                  whatever data it wants with a buddy     */
	PurpleBuddyIcon *icon;       /**< The buddy icon.                         */
	PurpleAccount *account;      /**< the account this buddy belongs to       */
	PurplePresence *presence;    /**< Presense information of the buddy       */
	PurpleMediaCaps media_caps;  /**< The media capabilities of the buddy.    */
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

/** Private data for a contact TODO: move counts to PurpleCountingNode */
struct _PurpleContactPrivate {
	char *alias;              /**< The user-set alias of the contact         */
	int totalsize;            /**< The number of buddies in this contact     */
	int currentsize;          /**< The number of buddies in this contact
	                               corresponding to online accounts          */
	int online;               /**< The number of buddies in this contact who
	                               are currently online                      */
	PurpleBuddy *priority;    /**< TODO rename to priority_buddy
	                               The "top" buddy for this contact          */
	gboolean priority_valid;  /**< Is priority valid?                        */
};

/* Contact property enums */
enum
{
	CONTACT_PROP_0,
	CONTACT_PROP_ALIAS,
	CONTACT_PROP_PRIORITY_BUDDY,
	CONTACT_PROP_LAST
};

/** Private data for a group TODO: move counts to PurpleCountingNode */
struct _PurpleGroupPrivate {
	char *name;       /**< The name of this group.                            */
	int totalsize;    /**< The number of chats and contacts in this group     */
	int currentsize;  /**< The number of chats and contacts in this group
	                       corresponding to online accounts                   */
	int online;       /**< The number of chats and contacts in this group who
	                       are currently online                               */
};

/* Group property enums */
enum
{
	GROUP_PROP_0,
	GROUP_PROP_NAME,
	GROUP_PROP_LAST
};

/** Private data for a chat node */
struct _PurpleChatPrivate {
	char *alias;             /**< The display name of this chat.              */
	GHashTable *components;  /**< the stuff the protocol needs to know to join
	                              the chat                                    */
	PurpleAccount *account;  /**< The account this chat is attached to        */
};

/* Chat property enums */
enum
{
	CHAT_PROP_0,
	CHAT_PROP_ALIAS,
	CHAT_PROP_ACCOUNT,
	CHAT_PROP_LAST
};

static PurpleBListNode *parent_class;

/**************************************************************************/
/* Buddy API                                                              */
/**************************************************************************/

void
purple_buddy_set_icon(PurpleBuddy *buddy, PurpleBuddyIcon *icon)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	if (priv->icon != icon)
	{
		purple_buddy_icon_unref(priv->icon);
		priv->icon = (icon != NULL ? purple_buddy_icon_ref(icon) : NULL);
	}

	purple_signal_emit(purple_blist_get_handle(), "buddy-icon-changed", buddy);

	purple_blist_update_node_icon(PURPLE_BLIST_NODE(buddy));
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

	g_return_if_fail(priv != NULL);

	priv->name = purple_utf8_strip_unprintables(name);
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
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	priv->local_alias = purple_utf8_strip_unprintables(alias);
}

const char *purple_buddy_get_local_alias(PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->local_alias;
}

void
purple_buddy_set_server_alias(PurpleBuddy *buddy, const char *server_alias)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	priv->server_alias = purple_utf8_strip_unprintables(server_alias);
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
	g_return_val_if_fail(buddy != NULL, NULL);

	return PURPLE_CONTACT(PURPLE_BLIST_NODE(buddy)->parent);
}

PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->presence;
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
}

PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	if (PURPLE_BLIST_NODE(buddy)->parent == NULL)
		return NULL;

	return PURPLE_GROUP(PURPLE_BLIST_NODE(buddy)->parent->parent);
}

/**************************************************************************
 * GObject code for PurpleBuddy
 **************************************************************************/

/* GObject Property names */
#define BUDDY_PROP_NAME_S          "name"
#define BUDDY_PROP_LOCAL_ALIAS_S   "local-alias"
#define BUDDY_PROP_SERVER_ALIAS_S  "server-alias"
#define BUDDY_PROP_ICON_S          "icon"
#define BUDDY_PROP_ACCOUNT_S       "account"
#define BUDDY_PROP_PRESENCE_S      "presence"
#define BUDDY_PROP_MEDIA_CAPS_S    "media-caps"

/* Set method for GObject properties */
static void
purple_buddy_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(obj);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	switch (param_id) {
		case BUDDY_PROP_NAME:
			purple_buddy_set_name(buddy, g_value_get_string(value));
			break;
		case BUDDY_PROP_LOCAL_ALIAS:
			purple_buddy_set_local_alias(buddy, g_value_get_string(value));
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
#warning TODO: change set_pointer to set_object when PurplePresence is a GObject
			g_value_set_pointer(value, purple_buddy_get_presence(buddy));
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
	PurpleBuddy *buddy = PURPLE_BUDDY(instance);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	priv->presence = purple_presence_new_for_buddy(buddy);
	purple_presence_set_status_active(priv->presence, "offline", TRUE);

	if (ops && ops->new_node)
		ops->new_node((PurpleBListNode *)buddy);

	PURPLE_DBUS_REGISTER_POINTER(buddy, PurpleBuddy);
}

/* GObject dispose function */
static void
purple_buddy_dispose(GObject *object)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(object);
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;

	/*
	 * Tell the owner PRPL that we're about to free the buddy so it
	 * can free proto_data
	 */
	prpl = purple_find_prpl(purple_account_get_protocol_id(priv->account));
	if (prpl) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
		if (prpl_info && prpl_info->buddy_free)
			prpl_info->buddy_free(buddy);
	}

	/* Delete the node */
	purple_buddy_icon_unref(priv->icon);
	purple_presence_destroy(priv->presence);

	PURPLE_DBUS_UNREGISTER_POINTER(buddy);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_buddy_finalize(GObject *object)
{
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(object);

	g_free(priv->name);
	g_free(priv->local_alias);
	g_free(priv->server_alias);

	/* TODO: Now that PurpleBuddy is a GObject, timeout callbacks can
	 * g_object_ref() it when connecting the callback and
	 * g_object_unref() it in the handler.  That way, it won't
	 * get freed while the timeout is pending and this line can
	 * be removed. */
	while (g_source_remove_by_user_data((gpointer *)object));

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_buddy_class_init(PurpleBuddyClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_buddy_dispose;
	obj_class->finalize = purple_buddy_finalize;

	/* Setup properties */
	obj_class->get_property = purple_buddy_get_property;
	obj_class->set_property = purple_buddy_set_property;

	g_object_class_install_property(obj_class, BUDDY_PROP_NAME,
			g_param_spec_string(BUDDY_PROP_NAME_S, _("Name"),
				_("The name of the buddy."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, BUDDY_PROP_LOCAL_ALIAS,
			g_param_spec_string(BUDDY_PROP_LOCAL_ALIAS_S, _("Local alias"),
				_("Local alias of thee buddy."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, BUDDY_PROP_SERVER_ALIAS,
			g_param_spec_string(BUDDY_PROP_SERVER_ALIAS_S, _("Server alias"),
				_("Server-side alias of the buddy."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, BUDDY_PROP_ICON,
			g_param_spec_pointer(BUDDY_PROP_ICON_S, _("Buddy icon"),
				_("The icon for the buddy."),
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, BUDDY_PROP_ACCOUNT,
			g_param_spec_object(BUDDY_PROP_ACCOUNT_S, _("Account"),
				_("The account for the buddy."), PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

#warning TODO: change spec_pointer to spec_object when PurplePresence is a GObject
	g_object_class_install_property(obj_class, BUDDY_PROP_PRESENCE,
			g_param_spec_pointer(BUDDY_PROP_PRESENCE_S, _("Presence"),
				_("The status information for the buddy."),
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, BUDDY_PROP_MEDIA_CAPS,
			g_param_spec_enum(BUDDY_PROP_MEDIA_CAPS_S, _("Media capabilities"),
				_("The media capabilities of the buddy."),
				PURPLE_MEDIA_TYPE_CAPS, PURPLE_MEDIA_CAPS_NONE,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleBuddyPrivate));
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
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_object_new(PURPLE_TYPE_BUDDY,
			BUDDY_PROP_ACCOUNT_S,      account,
			BUDDY_PROP_NAME_S,         name,
			BUDDY_PROP_LOCAL_ALIAS_S,  alias,
			NULL);
}

/**************************************************************************/
/* Contact API                                                            */
/**************************************************************************/

static void
purple_contact_compute_priority_buddy(PurpleContact *contact)
{
	PurpleBListNode *bnode;
	PurpleBuddy *new_priority = NULL;
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(priv != NULL);

	priv->priority = NULL;
	for (bnode = PURPLE_BLIST_NODE(contact)->child;
			bnode != NULL;
			bnode = bnode->next)
	{
		PurpleBuddy *buddy;

		if (!PURPLE_IS_BUDDY(bnode))
			continue;

		buddy = PURPLE_BUDDY(bnode);
		if (new_priority == NULL)
		{
			new_priority = buddy;
			continue;
		}

		if (purple_account_is_connected(purple_buddy_get_account(buddy)))
		{
			int cmp = 1;
			if (purple_account_is_connected(purple_buddy_get_account(new_priority->account)))
				cmp = purple_presence_compare(purple_buddy_get_presence(new_priority),
						purple_buddy_get_presence(buddy));

			if (cmp > 0 || (cmp == 0 &&
			                purple_prefs_get_bool("/purple/contact/last_match")))
			{
				new_priority = buddy;
			}
		}
	}

	priv->priority = new_priority;
	priv->priority_valid = TRUE;
}

PurpleGroup *
purple_contact_get_group(const PurpleContact *contact)
{
	g_return_val_if_fail(contact, NULL);

	return PURPLE_GROUP(PURPLE_BLIST_NODE(contact)->parent);
}

const char *purple_contact_get_alias(PurpleContact* contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->alias)
		return priv->alias;

	return purple_buddy_get_alias(purple_contact_get_priority_buddy(contact));
}

gboolean purple_contact_on_account(PurpleContact *c, PurpleAccount *account)
{
	PurpleBListNode *bnode, *cnode = (PurpleBListNode *) c;

	g_return_val_if_fail(c != NULL, FALSE);
	g_return_val_if_fail(account != NULL, FALSE);

	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		PurpleBuddy *buddy;

		if (! PURPLE_IS_BUDDY(bnode))
			continue;

		buddy = (PurpleBuddy *)bnode;
		if (purple_buddy_get_account(buddy) == account)
			return TRUE;
	}
	return FALSE;
}

void purple_contact_invalidate_priority_buddy(PurpleContact *contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(priv != NULL);

	priv->priority_valid = FALSE;
}

int purple_contact_get_contact_size(PurpleContact *contact, gboolean offline)   
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_val_if_fail(priv != NULL, 0);

	return offline ? priv->totalsize : priv->currentsize;
}

PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_val_if_fail(priv != NULL, NULL);

	if (!priv->priority_valid)
		purple_contact_compute_priority_buddy(contact);

	return priv->priority;
}

/**************************************************************************
 * GObject code for PurpleContact
 **************************************************************************/

/* GObject Property names */
#define CONTACT_PROP_ALIAS_S           "alias"
#define CONTACT_PROP_PRIORITY_BUDDY_S  "priority-buddy"

/* TODO GObjectify */
PurpleContact *purple_contact_new()
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	PurpleContact *contact = g_new0(PurpleContact, 1);
	contact->totalsize = 0;
	contact->currentsize = 0;
	contact->online = 0;
	purple_blist_node_initialize_settings((PurpleBListNode *)contact);
	((PurpleBListNode *)contact)->type = PURPLE_BLIST_CONTACT_NODE;

	if (ops && ops->new_node)
		ops->new_node((PurpleBListNode *)contact);

	PURPLE_DBUS_REGISTER_POINTER(contact, PurpleContact);
	return contact;
}

/* TODO GObjectify */
void
purple_contact_destroy(PurpleContact *contact)
{
	g_free(contact->alias);
	PURPLE_DBUS_UNREGISTER_POINTER(contact);
	g_free(contact);
}

/**************************************************************************/
/* Chat API                                                               */
/**************************************************************************/

const char *purple_chat_get_name(PurpleChat *chat)
{
	char *ret = NULL;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	if ((priv->alias != NULL) && (*priv->alias != '\0'))
		return priv->alias;

	prpl = purple_find_prpl(purple_account_get_protocol_id(priv->account));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->chat_info) {
		struct proto_chat_entry *pce;
		GList *parts = prpl_info->chat_info(purple_account_get_connection(priv->account));
		pce = parts->data;
		ret = g_hash_table_lookup(priv->components, pce->identifier);
		g_list_foreach(parts, (GFunc)g_free, NULL);
		g_list_free(parts);
	}

	return ret;
}

PurpleGroup *
purple_chat_get_group(PurpleChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

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

/* GObject Property names */
#define CHAT_PROP_ALIAS_S    "alias"
#define CHAT_PROP_ACCOUNT_S  "account"

/* TODO GObjectify */
PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleChat *chat;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(components != NULL, NULL);

	chat = g_new0(PurpleChat, 1);
	chat->account = account;
	if ((alias != NULL) && (*alias != '\0'))
		chat->alias = purple_utf8_strip_unprintables(alias);
	chat->components = components;
	purple_blist_node_initialize_settings((PurpleBListNode *)chat);
	((PurpleBListNode *)chat)->type = PURPLE_BLIST_CHAT_NODE;

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((PurpleBListNode *)chat);

	PURPLE_DBUS_REGISTER_POINTER(chat, PurpleChat);
	return chat;
}

/* TODO GObjectify */
void
purple_chat_destroy(PurpleChat *chat)
{
	g_hash_table_destroy(chat->components);
	g_free(chat->alias);
	PURPLE_DBUS_UNREGISTER_POINTER(chat);
	g_free(chat);
}

/**************************************************************************/
/* Group API                                                              */
/**************************************************************************/

GSList *purple_group_get_accounts(PurpleGroup *group)
{
	GSList *l = NULL;
	PurpleBListNode *gnode, *cnode, *bnode;

	gnode = (PurpleBListNode *)group;

	for (cnode = gnode->child;  cnode; cnode = cnode->next) {
		if (PURPLE_IS_CHAT(cnode)) {
			if (!g_slist_find(l, purple_chat_get_account(PURPLE_CHAT(cnode))))
				l = g_slist_append(l, purple_chat_get_account(PURPLE_CHAT(cnode)));
		} else if (PURPLE_IS_CONTACT(cnode)) {
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				if (PURPLE_IS_BUDDY(bnode)) {
					if (!g_slist_find(l, purple_buddy_get_account(PURPLE_BUDDY(bnode))))
						l = g_slist_append(l, purple_buddy_get_account(PURPLE_BUDDY(bnode)));
				}
			}
		}
	}

	return l;
}

gboolean purple_group_on_account(PurpleGroup *g, PurpleAccount *account)
{
	PurpleBListNode *cnode;
	for (cnode = ((PurpleBListNode *)g)->child; cnode; cnode = cnode->next) {
		if (PURPLE_IS_CONTACT(cnode)) {
			if(purple_contact_on_account((PurpleContact *) cnode, account))
				return TRUE;
		} else if (PURPLE_IS_CHAT(cnode)) {
			PurpleChat *chat = (PurpleChat *)cnode;
			if ((!account && purple_account_is_connected(purple_chat_get_account(chat)))
					|| purple_chat_get_account(chat) == account)
				return TRUE;
		}
	}
	return FALSE;
}

const char *purple_group_get_name(PurpleGroup *group)
{
	PurpleGroupPrivate *priv = PURPLE_GROUP_GET_PRIVATE(group);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

/**************************************************************************
 * GObject code for PurpleGroup
 **************************************************************************/

/* GObject Property names */
#define GROUP_PROP_NAME_S          "name"

/* TODO GObjectify */
PurpleGroup *purple_group_new(const char *name)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleGroup *group;

	g_return_val_if_fail(name  != NULL, NULL);
	g_return_val_if_fail(*name != '\0', NULL);

	group = purple_find_group(name);
	if (group != NULL)
		return group;

	group = g_new0(PurpleGroup, 1);
	group->name = purple_utf8_strip_unprintables(name);
	group->totalsize = 0;
	group->currentsize = 0;
	group->online = 0;
	purple_blist_node_initialize_settings((PurpleBListNode *)group);
	((PurpleBListNode *)group)->type = PURPLE_BLIST_GROUP_NODE;

	if (ops && ops->new_node)
		ops->new_node((PurpleBListNode *)group);

	PURPLE_DBUS_REGISTER_POINTER(group, PurpleGroup);
	return group;
}

/* TODO GObjectify */
void
purple_group_destroy(PurpleGroup *group)
{
	g_free(group->name);
	PURPLE_DBUS_UNREGISTER_POINTER(group);
	g_free(group);
}
