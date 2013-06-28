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
 */
#include "internal.h"
#include "conversationtypes.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "enums.h"

#define SEND_TYPED_TIMEOUT_SECONDS 5

#define PURPLE_CHAT_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationPrivate))

/** @copydoc _PurpleChatConversationPrivate */
typedef struct _PurpleChatConversationPrivate     PurpleChatConversationPrivate;

#define PURPLE_IM_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_IM_CONVERSATION, PurpleIMConversationPrivate))

/** @copydoc _PurpleIMConversationPrivate */
typedef struct _PurpleIMConversationPrivate       PurpleIMConversationPrivate;

#define PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT_CONVERSATION_BUDDY, PurpleChatConversationBuddyPrivate))

/** @copydoc _PurpleChatConversationBuddyPrivate */
typedef struct _PurpleChatConversationBuddyPrivate  PurpleChatConversationBuddyPrivate;

/**
 * Data specific to Chats.
 */
struct _PurpleChatConversationPrivate
{
	GList *in_room;                  /**< The users in the room.
	                                  *   @deprecated Will be removed in 3.0.0 TODO
									  */
	GList *ignored;                  /**< Ignored users.                */
	char  *who;                      /**< The person who set the topic. */
	char  *topic;                    /**< The topic.                    */
	int    id;                       /**< The chat ID.                  */
	char *nick;                      /**< Your nick in this chat.       */

	gboolean left;                   /**< We left the chat and kept the window open */
	GHashTable *users;               /**< Hash table of the users in the room. */
};

/* Chat Property enums */
enum {
	CHAT_PROP_0,
	CHAT_PROP_TOPIC_WHO,
	CHAT_PROP_TOPIC,
	CHAT_PROP_ID,
	CHAT_PROP_NICK,
	CHAT_PROP_LEFT,
	CHAT_PROP_LAST
};

/**
 * Data specific to Instant Messages.
 */
struct _PurpleIMConversationPrivate
{
	PurpleIMConversationTypingState typing_state;      /**< The current typing state.    */
	guint  typing_timeout;             /**< The typing timer handle.     */
	time_t type_again;                 /**< The type again time.         */
	guint  send_typed_timeout;         /**< The type again timer handle. */

	PurpleBuddyIcon *icon;               /**< The buddy icon.              */
};

/* IM Property enums */
enum {
	IM_PROP_0,
	IM_PROP_TYPING_STATE,
	IM_PROP_ICON,
	IM_PROP_LAST
};

/**
 * Data for "Chat Buddies"
 */
struct _PurpleChatConversationBuddyPrivate
{
	/** The chat */
	PurpleChatConversation *chat;

	/** The chat participant's name in the chat. */
	char *name;

	/** The chat participant's alias, if known; @a NULL otherwise. */
	char *alias;

	/**
	 * A string by which this buddy will be sorted, or @c NULL if the
	 * buddy should be sorted by its @c name.  (This is currently always
	 * @c NULL.
	 */
	char *alias_key;

	/**
	 * @a TRUE if this chat participant is on the buddy list;
	 * @a FALSE otherwise.
	 */
	gboolean buddy;

	/**
	 * A bitwise OR of flags for this participant, such as whether they
	 * are a channel operator.
	 */
	PurpleChatConversationBuddyFlags flags;

	/**
	 * A hash table of attributes about the user, such as real name,
	 * user\@host, etc.
	 */
	GHashTable *attributes;

	/** The UI can put whatever it wants here. */
	gpointer ui_data;
};

/* Chat Buddy Property enums */
enum {
	CB_PROP_0,
	CB_PROP_CHAT,
	CB_PROP_NAME,
	CB_PROP_ALIAS,
	CB_PROP_BUDDY,
	CB_PROP_FLAGS,
	CB_PROP_LAST
};

static PurpleConversationClass *parent_class;
static GObjectClass            *cb_parent_class;

static int purple_chat_conversation_buddy_compare(PurpleChatConversationBuddy *a,
		PurpleChatConversationBuddy *b);

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
static gboolean
reset_typing_cb(gpointer data)
{
	PurpleIMConversation *im = (PurpleIMConversation *)data;

	purple_im_conversation_set_typing_state(im, PURPLE_IM_CONVERSATION_NOT_TYPING);
	purple_im_conversation_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed_cb(gpointer data)
{
	PurpleIMConversation *im = (PurpleIMConversation *)data;
	PurpleConnection *gc;
	const char *name;

	g_return_val_if_fail(im != NULL, FALSE);

	gc   = purple_conversation_get_connection(PURPLE_CONVERSATION(im));
	name = purple_conversation_get_name(PURPLE_CONVERSATION(im));

	if (gc != NULL && name != NULL) {
		/* We set this to 1 so that PURPLE_IM_CONVERSATION_TYPING will be sent
		 * if the Purple user types anything else.
		 */
		purple_im_conversation_set_type_again(im, 1);

		serv_send_typing(gc, name, PURPLE_IM_CONVERSATION_TYPED);

		purple_debug(PURPLE_DEBUG_MISC, "conversation", "typed...\n");
	}

	return FALSE;
}

void
purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->icon != icon)
	{
		purple_buddy_icon_unref(priv->icon);

		priv->icon = (icon == NULL ? NULL : purple_buddy_icon_ref(icon));
	}

	purple_conversation_update(PURPLE_CONVERSATION(im),
							 PURPLE_CONVERSATION_UPDATE_ICON);
}

PurpleBuddyIcon *
purple_im_conversation_get_icon(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->icon;
}

void
purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMConversationTypingState state)
{
	PurpleAccount *account;
	const char *name;
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	name = purple_conversation_get_name(PURPLE_CONVERSATION(im));
	account = purple_conversation_get_account(PURPLE_CONVERSATION(im));

	if (priv->typing_state != state)
	{
		priv->typing_state = state;

		switch (state)
		{
			case PURPLE_IM_CONVERSATION_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing", account, name);
				break;
			case PURPLE_IM_CONVERSATION_TYPED:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typed", account, name);
				break;
			case PURPLE_IM_CONVERSATION_NOT_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing-stopped", account, name);
				break;
		}

		purple_im_conversation_update_typing(im);
	}
}

PurpleIMConversationTypingState
purple_im_conversation_get_typing_state(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->typing_state;
}

void
purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->typing_timeout > 0)
		purple_im_conversation_stop_typing_timeout(im);

	priv->typing_timeout = purple_timeout_add_seconds(timeout, reset_typing_cb, im);
}

void
purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->typing_timeout == 0)
		return;

	purple_timeout_remove(priv->typing_timeout);
	priv->typing_timeout = 0;
}

guint
purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->typing_timeout;
}

void
purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (val == 0)
		priv->type_again = 0;
	else
		priv->type_again = time(NULL) + val;
}

time_t
purple_im_conversation_get_type_again(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->type_again;
}

void
purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	priv->send_typed_timeout = purple_timeout_add_seconds(SEND_TYPED_TIMEOUT_SECONDS,
	                                                    send_typed_cb, im);
}

void
purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->send_typed_timeout == 0)
		return;

	purple_timeout_remove(priv->send_typed_timeout);
	priv->send_typed_timeout = 0;
}

guint
purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->send_typed_timeout;
}

void
purple_im_conversation_update_typing(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	purple_conversation_update(PURPLE_CONVERSATION(im),
							 PURPLE_CONVERSATION_UPDATE_TYPING);
}

static void
im_conversation_write_message(PurpleConversation *conv, const char *who, const char *message,
			  PurpleMessageFlags flags, time_t mtime)
{
	PurpleConversationUiOps *ops;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(message != NULL);

	ops = purple_conversation_get_ui_ops(conv);

	if ((flags & PURPLE_MESSAGE_RECV) == PURPLE_MESSAGE_RECV) {
		purple_im_conversation_set_typing_state(PURPLE_IM_CONVERSATION(conv),
				PURPLE_IM_CONVERSATION_NOT_TYPING);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (ops != NULL && ops->write_im != NULL)
		ops->write_im(PURPLE_IM_CONVERSATION(conv), who, message, flags, mtime);
	else
		purple_conversation_write(conv, who, message, flags, mtime);
}

/**************************************************************************
 * GObject code for IMs
 **************************************************************************/

/* GObject Property names */
#define IM_PROP_TYPING_STATE_S  "typing-state"
#define IM_PROP_ICON_S          "icon"

/* Set method for GObject properties */
static void
purple_im_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case IM_PROP_TYPING_STATE:
			purple_im_conversation_set_typing_state(im, g_value_get_enum(value));
			break;
		case IM_PROP_ICON:
#warning TODO: change get_pointer to get_object if PurpleBuddyIcon is a GObject
			purple_im_conversation_set_icon(im, g_value_get_pointer(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_im_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case IM_PROP_TYPING_STATE:
			g_value_set_enum(value, purple_im_conversation_get_typing_state(im));
			break;
		case IM_PROP_ICON:
#warning TODO: change set_pointer to set_object if PurpleBuddyIcon is a GObject
			g_value_set_pointer(value, purple_im_conversation_get_icon(im));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject dispose function */
static void
purple_im_conversation_dispose(GObject *object)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(object);
	PurpleConnection *gc = purple_conversation_get_connection(PURPLE_CONVERSATION(im));
	PurplePluginProtocolInfo *prpl_info = NULL;
	const char *name = purple_conversation_get_name(PURPLE_CONVERSATION(im));

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

		if (purple_prefs_get_bool("/purple/conversations/im/send_typing"))
			serv_send_typing(gc, name, PURPLE_IM_CONVERSATION_NOT_TYPING);

		if (gc && prpl_info->convo_closed != NULL)
			prpl_info->convo_closed(gc, name);
	}

	purple_im_conversation_stop_typing_timeout(im);
	purple_im_conversation_stop_send_typed_timeout(im);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_im_conversation_finalize(GObject *object)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(object);
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	purple_buddy_icon_unref(priv->icon);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_im_conversation_class_init(PurpleIMConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleConversationClass *conv_class = PURPLE_CONVERSATION_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_im_conversation_dispose;
	obj_class->finalize = purple_im_conversation_finalize;

	/* Setup properties */
	obj_class->get_property = purple_im_conversation_get_property;
	obj_class->set_property = purple_im_conversation_set_property;

	conv_class->write_message = im_conversation_write_message;

	g_object_class_install_property(obj_class, IM_PROP_TYPING_STATE,
			g_param_spec_enum(IM_PROP_TYPING_STATE_S, _("Typing state"),
				_("Status of the user's typing of a message."),
				PURPLE_TYPE_IM_CONVERSATION_TYPING_STATE,
				PURPLE_IM_CONVERSATION_NOT_TYPING, G_PARAM_READWRITE)
			);

#warning TODO: change spec_pointer to spec_object if PurpleBuddyIcon is a GObject
	g_object_class_install_property(obj_class, IM_PROP_ICON,
			g_param_spec_pointer(IM_PROP_ICON_S, _("Buddy icon"),
				_("The buddy icon for the IM."),
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleIMConversationPrivate));
}

GType
purple_im_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleIMConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_im_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleIMConversation),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CONVERSATION,
				"PurpleIMConversation",
				&info, 0);
	}

	return type;
}

PurpleIMConversation *
purple_im_conversation_new(PurpleAccount *account, const char *name)
{
	PurpleIMConversation *im;
	PurpleConversation *conv;
	PurpleConnection *gc;
	PurpleConversationUiOps *ops;
	PurpleBuddyIcon *icon;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((im = purple_conversations_find_im_with_account(name, account)) != NULL)
		return im;

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	im = g_object_new(PURPLE_TYPE_IM_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	conv = PURPLE_CONVERSATION(im);

	/* copy features from the connection. */
	purple_conversation_set_features(conv,
			purple_connection_get_flags(gc));

	purple_conversations_add(conv);
	if ((icon = purple_buddy_icons_find(account, name)))
	{
		purple_im_conversation_set_icon(im, icon);
		/* purple_im_conversation_set_icon refs the icon. */
		purple_buddy_icon_unref(icon);
	}

	if (purple_prefs_get_bool("/purple/logging/log_ims"))
		purple_conversation_set_logging(conv, TRUE);

	/* Auto-set the title. */
	purple_conversation_autoset_title(conv);

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = purple_conversations_get_ui_ops();
	purple_conversation_set_ui_ops(conv, ops);
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-created", im); /* TODO im-created */

	return im;
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/
static guint
_purple_conversation_user_hash(gconstpointer data)
{
	const gchar *name = data;
	gchar *collated;
	guint hash;

	collated = g_utf8_collate_key(name, -1);
	hash     = g_str_hash(collated);
	g_free(collated);
	return hash;
}

static gboolean
_purple_conversation_user_equal(gconstpointer a, gconstpointer b)
{
	return !g_utf8_collate(a, b);
}

GList *
purple_chat_conversation_get_users(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->in_room;
}

void
purple_chat_conversation_ignore(PurpleChatConversation *chat, const char *name)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user isn't already ignored. */
	if (purple_chat_conversation_is_ignored_user(chat, name))
		return;

	purple_chat_conversation_set_ignored(chat,
		g_list_append(priv->ignored, g_strdup(name)));
}

void
purple_chat_conversation_unignore(PurpleChatConversation *chat, const char *name)
{
	GList *item;
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user is actually ignored. */
	if (!purple_chat_conversation_is_ignored_user(chat, name))
		return;

	item = g_list_find(purple_chat_conversation_get_ignored(chat),
					   purple_chat_conversation_get_ignored_user(chat, name));

	purple_chat_conversation_set_ignored(chat,
		g_list_remove_link(priv->ignored, item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
purple_chat_conversation_set_ignored(PurpleChatConversation *chat, GList *ignored)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	priv->ignored = ignored;
	return ignored;
}

GList *
purple_chat_conversation_get_ignored(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ignored;
}

const char *
purple_chat_conversation_get_ignored_user(const PurpleChatConversation *chat, const char *user)
{
	GList *ignored;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(user != NULL, NULL);

	for (ignored = purple_chat_conversation_get_ignored(chat);
		 ignored != NULL;
		 ignored = ignored->next) {

		const char *ign = (const char *)ignored->data;

		if (!purple_utf8_strcasecmp(user, ign) ||
			((*ign == '+' || *ign == '%') && !purple_utf8_strcasecmp(user, ign + 1)))
			return ign;

		if (*ign == '@') {
			ign++;

			if ((*ign == '+' && !purple_utf8_strcasecmp(user, ign + 1)) ||
				(*ign != '+' && !purple_utf8_strcasecmp(user, ign)))
				return ign;
		}
	}

	return NULL;
}

gboolean
purple_chat_conversation_is_ignored_user(const PurpleChatConversation *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (purple_chat_conversation_get_ignored_user(chat, user) != NULL);
}

void
purple_chat_conversation_set_topic(PurpleChatConversation *chat, const char *who, const char *topic)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	g_free(priv->who);
	g_free(priv->topic);

	priv->who   = g_strdup(who);
	priv->topic = g_strdup(topic);

	purple_conversation_update(PURPLE_CONVERSATION(chat),
							 PURPLE_CONVERSATION_UPDATE_TOPIC);

	purple_signal_emit(purple_conversations_get_handle(), "chat-topic-changed",
					 chat, priv->who, priv->topic);
}

const char *
purple_chat_conversation_get_topic(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->topic;
}

const char *
purple_chat_conversation_get_topic_who(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;
}

void
purple_chat_conversation_set_id(PurpleChatConversation *chat, int id)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	priv->id = id;
}

int
purple_chat_conversation_get_id(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->id;
}

static void
chat_conversation_write_message(PurpleConversation *conv, const char *who, const char *message,
				PurpleMessageFlags flags, time_t mtime)
{
	PurpleAccount *account;
	PurpleConversationUiOps *ops;
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	account = purple_conversation_get_account(conv);

	/* Don't display this if the person who wrote it is ignored. */
	if (purple_chat_conversation_is_ignored_user(PURPLE_CHAT_CONVERSATION(conv), who))
		return;

	if (!(flags & PURPLE_MESSAGE_WHISPER)) {
		const char *str;

		str = purple_normalize(account, who);

		if (purple_strequal(str, priv->nick)) {
			flags |= PURPLE_MESSAGE_SEND;
		} else {
			flags |= PURPLE_MESSAGE_RECV;

			if (purple_utf8_has_word(message, priv->nick))
				flags |= PURPLE_MESSAGE_NICK;
		}
	}

	ops = purple_conversation_get_ui_ops(conv);

	/* Pass this on to either the ops structure or the default write func. */
	if (ops != NULL && ops->write_chat != NULL)
		ops->write_chat(PURPLE_CHAT_CONVERSATION(conv), who, message, flags, mtime);
	else
		purple_conversation_write(conv, who, message, flags, mtime);
}

void
purple_chat_conversation_add_user(PurpleChatConversation *chat, const char *user,
						const char *extra_msg, PurpleChatConversationBuddyFlags flags,
						gboolean new_arrival)
{
	GList *users = g_list_append(NULL, (char *)user);
	GList *extra_msgs = g_list_append(NULL, (char *)extra_msg);
	GList *flags2 = g_list_append(NULL, GINT_TO_POINTER(flags));

	purple_chat_conversation_add_users(chat, users, extra_msgs, flags2, new_arrival);

	g_list_free(users);
	g_list_free(extra_msgs);
	g_list_free(flags2);
}

void
purple_chat_conversation_add_users(PurpleChatConversation *chat, GList *users, GList *extra_msgs,
						 GList *flags, gboolean new_arrivals)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddy *cbuddy;
	PurpleChatConversationPrivate *priv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	GList *ul, *fl;
	GList *cbuddies = NULL;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(users != NULL);

	conv = PURPLE_CONVERSATION(chat);
	ops  = purple_conversation_get_ui_ops(conv);

	account = purple_conversation_get_account(conv);
	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	ul = users;
	fl = flags;
	while ((ul != NULL) && (fl != NULL)) {
		const char *user = (const char *)ul->data;
		const char *alias = user;
		gboolean quiet;
		PurpleChatConversationBuddyFlags flag = GPOINTER_TO_INT(fl->data);
		const char *extra_msg = (extra_msgs ? extra_msgs->data : NULL);

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			if (purple_strequal(priv->nick, purple_normalize(account, user))) {
				const char *alias2 = purple_account_get_private_alias(account);
				if (alias2 != NULL)
					alias = alias2;
				else
				{
					const char *display_name = purple_connection_get_display_name(gc);
					if (display_name != NULL)
						alias = display_name;
				}
			} else {
				PurpleBuddy *buddy;
				if ((buddy = purple_find_buddy(purple_connection_get_account(gc), user)) != NULL)
					alias = purple_buddy_get_contact_alias(buddy);
			}
		}

		quiet = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_conversations_get_handle(),
						 "chat-buddy-joining", chat, user, flag)) ||
				purple_chat_conversation_is_ignored_user(chat, user);

		cbuddy = purple_chat_conversation_buddy_new(chat, user, alias, flag);
		purple_chat_conversation_buddy_set_buddy(cbuddy, purple_find_buddy(account, user) != NULL);

		priv->in_room = g_list_prepend(priv->in_room, cbuddy);
		g_hash_table_replace(priv->users,
				g_strdup(purple_chat_conversation_buddy_get_name(cbuddy)), cbuddy);

		cbuddies = g_list_prepend(cbuddies, cbuddy);

		if (!quiet && new_arrivals) {
			char *alias_esc = g_markup_escape_text(alias, -1);
			char *tmp;

			if (extra_msg == NULL)
				tmp = g_strdup_printf(_("%s entered the room."), alias_esc);
			else {
				char *extra_msg_esc = g_markup_escape_text(extra_msg, -1);
				tmp = g_strdup_printf(_("%s [<I>%s</I>] entered the room."),
				                      alias_esc, extra_msg_esc);
				g_free(extra_msg_esc);
			}
			g_free(alias_esc);

			purple_conversation_write(conv, NULL, tmp,
					PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LINKIFY,
					time(NULL));
			g_free(tmp);
		}

		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-buddy-joined", chat, user, flag, new_arrivals);
		ul = ul->next;
		fl = fl->next;
		if (extra_msgs != NULL)
			extra_msgs = extra_msgs->next;
	}

	cbuddies = g_list_sort(cbuddies, (GCompareFunc)purple_chat_conversation_buddy_compare);

	if (ops != NULL && ops->chat_add_users != NULL)
		ops->chat_add_users(chat, cbuddies, new_arrivals);

	g_list_free(cbuddies);
}

void
purple_chat_conversation_rename_user(PurpleChatConversation *chat, const char *old_user,
						   const char *new_user)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	PurpleChatConversationBuddy *cb;
	PurpleChatConversationBuddyFlags flags;
	PurpleChatConversationPrivate *priv;
	const char *new_alias = new_user;
	char tmp[BUF_LONG];
	gboolean is_me = FALSE;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv    = PURPLE_CONVERSATION(chat);
	ops     = purple_conversation_get_ui_ops(conv);
	account = purple_conversation_get_account(conv);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	if (purple_strequal(priv->nick, purple_normalize(account, old_user))) {
		const char *alias;

		/* Note this for later. */
		is_me = TRUE;

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			alias = purple_account_get_private_alias(account);
			if (alias != NULL)
				new_alias = alias;
			else
			{
				const char *display_name = purple_connection_get_display_name(gc);
				if (display_name != NULL)
					new_alias = display_name;
			}
		}
	} else if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		PurpleBuddy *buddy;
		if ((buddy = purple_find_buddy(purple_connection_get_account(gc), new_user)) != NULL)
			new_alias = purple_buddy_get_contact_alias(buddy);
	}

	flags = purple_chat_conversation_buddy_get_flags(purple_chat_conversation_find_buddy(chat, old_user));
	cb = purple_chat_conversation_buddy_new(chat, new_user, new_alias, flags);
	purple_chat_conversation_buddy_set_buddy(cb, purple_find_buddy(account, new_user) != NULL);

	priv->in_room = g_list_prepend(priv->in_room, cb);
	g_hash_table_replace(priv->users,
			g_strdup(purple_chat_conversation_buddy_get_name(cb)), cb);

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(chat, old_user, new_user, new_alias);

	cb = purple_chat_conversation_find_buddy(chat, old_user);

	if (cb) {
		priv->in_room = g_list_remove(priv->in_room, cb);
		g_hash_table_remove(priv->users, purple_chat_conversation_buddy_get_name(cb));
		g_object_unref(cb);
	}

	if (purple_chat_conversation_is_ignored_user(chat, old_user)) {
		purple_chat_conversation_unignore(chat, old_user);
		purple_chat_conversation_ignore(chat, new_user);
	}
	else if (purple_chat_conversation_is_ignored_user(chat, new_user))
		purple_chat_conversation_unignore(chat, new_user);

	if (is_me)
		purple_chat_conversation_set_nick(chat, new_user);

	if (purple_prefs_get_bool("/purple/conversations/chat/show_nick_change") &&
	    !purple_chat_conversation_is_ignored_user(chat, new_user)) {

		if (is_me) {
			char *escaped = g_markup_escape_text(new_user, -1);
			g_snprintf(tmp, sizeof(tmp),
					_("You are now known as %s"), escaped);
			g_free(escaped);
		} else {
			const char *old_alias = old_user;
			const char *new_alias = new_user;
			char *escaped;
			char *escaped2;

			if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
				PurpleBuddy *buddy;

				if ((buddy = purple_find_buddy(purple_connection_get_account(gc), old_user)) != NULL)
					old_alias = purple_buddy_get_contact_alias(buddy);
				if ((buddy = purple_find_buddy(purple_connection_get_account(gc), new_user)) != NULL)
					new_alias = purple_buddy_get_contact_alias(buddy);
			}

			escaped = g_markup_escape_text(old_alias, -1);
			escaped2 = g_markup_escape_text(new_alias, -1);
			g_snprintf(tmp, sizeof(tmp),
					_("%s is now known as %s"), escaped, escaped2);
			g_free(escaped);
			g_free(escaped2);
		}

		purple_conversation_write(conv, NULL, tmp,
				PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LINKIFY,
				time(NULL));
	}
}

void
purple_chat_conversation_remove_user(PurpleChatConversation *chat, const char *user, const char *reason)
{
	GList *users = g_list_append(NULL, (char *)user);

	purple_chat_conversation_remove_users(chat, users, reason);

	g_list_free(users);
}

void
purple_chat_conversation_remove_users(PurpleChatConversation *chat, GList *users, const char *reason)
{
	PurpleConversation *conv;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddy *cb;
	PurpleChatConversationPrivate *priv;
	GList *l;
	gboolean quiet;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(users != NULL);

	conv = PURPLE_CONVERSATION(chat);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	ops  = purple_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;
		quiet = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_conversations_get_handle(),
					"chat-buddy-leaving", chat, user, reason)) |
				purple_chat_conversation_is_ignored_user(chat, user);

		cb = purple_chat_conversation_find_buddy(chat, user);

		if (cb) {
			priv->in_room = g_list_remove(priv->in_room, cb);
			g_hash_table_remove(priv->users, purple_chat_conversation_buddy_get_name(cb));
			g_object_unref(cb);
		}

		/* NOTE: Don't remove them from ignored in case they re-enter. */

		if (!quiet) {
			const char *alias = user;
			char *alias_esc;
			char *tmp;

			if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
				PurpleBuddy *buddy;

				if ((buddy = purple_find_buddy(purple_connection_get_account(gc), user)) != NULL)
					alias = purple_buddy_get_contact_alias(buddy);
			}

			alias_esc = g_markup_escape_text(alias, -1);

			if (reason == NULL || !*reason)
				tmp = g_strdup_printf(_("%s left the room."), alias_esc);
			else {
				char *reason_esc = g_markup_escape_text(reason, -1);
				tmp = g_strdup_printf(_("%s left the room (%s)."),
				                      alias_esc, reason_esc);
				g_free(reason_esc);
			}
			g_free(alias_esc);

			purple_conversation_write(conv, NULL, tmp,
					PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LINKIFY,
					time(NULL));
			g_free(tmp);
		}

		purple_signal_emit(purple_conversations_get_handle(), "chat-buddy-left",
						 conv, user, reason);
	}

	if (ops != NULL && ops->chat_remove_users != NULL)
		ops->chat_remove_users(chat, users);
}

void
purple_chat_conversation_clear_users(PurpleChatConversation *chat)
{
	PurpleConversationUiOps *ops;
	GList *users;
	GList *l;
	GList *names = NULL;
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	ops   = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(chat));
	users = priv->in_room;

	if (ops != NULL && ops->chat_remove_users != NULL) {
		for (l = users; l; l = l->next) {
			PurpleChatConversationBuddy *cb = l->data;
			names = g_list_prepend(names,
					(gchar *) purple_chat_conversation_buddy_get_name(cb));
		}
		ops->chat_remove_users(chat, names);
		g_list_free(names);
	}

	for (l = users; l; l = l->next)
	{
		PurpleChatConversationBuddy *cb = l->data;
		const char *name = purple_chat_conversation_buddy_get_name(cb);

		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-buddy-leaving", chat, name, NULL);
		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-buddy-left", chat, name, NULL);

		g_object_unref(cb);
	}

	g_hash_table_remove_all(priv->users);

	g_list_free(users);
	priv->in_room = NULL;
}

void purple_chat_conversation_set_nick(PurpleChatConversation *chat, const char *nick) {
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	g_free(priv->nick);
	priv->nick = g_strdup(purple_normalize(
			purple_conversation_get_account(PURPLE_CONVERSATION(chat)), nick));
}

const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat) {
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->nick;
}

static void
invite_user_to_chat(gpointer data, PurpleRequestFields *fields)
{
	PurpleConversation *conv;
	PurpleChatConversationPrivate *priv;
	const char *user, *message;

	conv = data;
	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv);
	user = purple_request_fields_get_string(fields, "screenname");
	message = purple_request_fields_get_string(fields, "message");

	serv_chat_invite(purple_conversation_get_connection(conv), priv->id, message, user);
}

void purple_chat_conversation_invite_user(PurpleChatConversation *chat, const char *user,
		const char *message, gboolean confirm)
{
	PurpleAccount *account;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	g_return_if_fail(chat != NULL);

	if (!user || !*user || !message || !*message)
		confirm = TRUE;

	account = purple_conversation_get_account(PURPLE_CONVERSATION(chat));

	if (!confirm) {
		serv_chat_invite(purple_account_get_connection(account),
				purple_chat_conversation_get_id(chat), message, user);
		return;
	}

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(_("Invite to chat"));
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("Buddy"), user, FALSE);
	purple_request_field_group_add_field(group, field);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_type_hint(field, "screenname");

	field = purple_request_field_string_new("message", _("Message"), message, FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(chat, _("Invite to chat"), NULL,
			_("Please enter the name of the user you wish to invite, "
				"along with an optional invite message."),
			fields,
			_("Invite"), G_CALLBACK(invite_user_to_chat),
			_("Cancel"), NULL,
			account, user, PURPLE_CONVERSATION(chat),
			chat);
}

gboolean
purple_chat_conversation_has_user(PurpleChatConversation *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (purple_chat_conversation_find_buddy(chat, user) != NULL);
}

void
purple_chat_conversation_leave(PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	priv->left = TRUE;
	purple_conversation_update(PURPLE_CONVERSATION(chat), PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

gboolean
purple_chat_conversation_has_left(PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, TRUE);

	return priv->left;
}

static void
chat_conversation_cleanup_for_rejoin(PurpleChatConversation *chat)
{
	const char *disp;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	account = purple_conversation_get_account(conv);

	purple_conversation_close_logs(conv);
	purple_conversation_set_logging(conv, TRUE);

	gc = purple_account_get_connection(account);

	if ((disp = purple_connection_get_display_name(gc)) != NULL)
		purple_chat_conversation_set_nick(chat, disp);
	else
	{
		purple_chat_conversation_set_nick(chat,
								purple_account_get_username(account));
	}

	purple_chat_conversation_clear_users(chat);
	purple_chat_conversation_set_topic(chat, NULL, NULL);
	priv->left = FALSE;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

PurpleChatConversationBuddy *
purple_chat_conversation_find_buddy(PurpleChatConversation *chat, const char *name)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_hash_table_lookup(priv->users, name);
}

/**************************************************************************
 * GObject code for chats
 **************************************************************************/

/* GObject Property names */
#define CHAT_PROP_TOPIC_WHO_S  "topic-who"
#define CHAT_PROP_TOPIC_S      "topic"
#define CHAT_PROP_ID_S         "chat-id"
#define CHAT_PROP_NICK_S       "nick"
#define CHAT_PROP_LEFT_S       "left"

/* Set method for GObject properties */
static void
purple_chat_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(obj);

	switch (param_id) {
		case CHAT_PROP_ID:
			purple_chat_conversation_set_id(chat, g_value_get_int(value));
			break;
		case CHAT_PROP_NICK:
			purple_chat_conversation_set_nick(chat, g_value_get_string(value));
			break;
		case CHAT_PROP_LEFT:
			{
				gboolean left = g_value_get_boolean(value);
				if (left == TRUE)
					purple_chat_conversation_leave(chat);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_chat_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(obj);

	switch (param_id) {
		case CHAT_PROP_TOPIC_WHO:
			g_value_set_string(value, purple_chat_conversation_get_topic_who(chat));
			break;
		case CHAT_PROP_TOPIC:
			g_value_set_string(value, purple_chat_conversation_get_topic(chat));
			break;
		case CHAT_PROP_ID:
			g_value_set_int(value, purple_chat_conversation_get_id(chat));
			break;
		case CHAT_PROP_NICK:
			g_value_set_string(value, purple_chat_conversation_get_nick(chat));
			break;
		case CHAT_PROP_LEFT:
			g_value_set_boolean(value, purple_chat_conversation_has_left(chat));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_chat_conversation_init(GTypeInstance *instance, gpointer klass)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(instance);

	priv->users = g_hash_table_new_full(_purple_conversation_user_hash,
			_purple_conversation_user_equal, g_free, NULL);
}

/* GObject dispose function */
static void
purple_chat_conversation_dispose(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleConnection *gc = purple_conversation_get_connection(PURPLE_CONVERSATION(chat));

	if (gc != NULL)
	{
		/* Still connected */
		int chat_id = purple_chat_conversation_get_id(chat);
#if 0
		/*
		 * This is unfortunately necessary, because calling
		 * serv_chat_leave() calls this purple_conversation_destroy(),
		 * which leads to two calls here.. We can't just return after
		 * this, because then it'll return on the next pass. So, since
		 * serv_got_chat_left(), which is eventually called from the
		 * prpl that serv_chat_leave() calls, removes this conversation
		 * from the gc's buddy_chats list, we're going to check to see
		 * if this exists in the list. If so, we want to return after
		 * calling this, because it'll be called again. If not, fall
		 * through, because it'll have already been removed, and we'd
		 * be on the 2nd pass.
		 *
		 * Long paragraph. <-- Short sentence.
		 *
		 *   -- ChipX86
		 */

		if (gc && g_slist_find(gc->buddy_chats, conv) != NULL) {
			serv_chat_leave(gc, chat_id);

			return;
		}
#endif
		/*
		 * Instead of all of that, lets just close the window when
		 * the user tells us to, and let the prpl deal with the
		 * internals on it's own time. Don't do this if the prpl already
		 * knows it left the chat.
		 */
		if (!purple_chat_conversation_has_left(chat))
			serv_chat_leave(gc, chat_id);

		/*
		 * If they didn't call serv_got_chat_left by now, it's too late.
		 * So we better do it for them before we destroy the thing.
		 */
		if (!purple_chat_conversation_has_left(chat))
			serv_got_chat_left(gc, chat_id);
	}

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_chat_conversation_finalize(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_hash_table_destroy(priv->users);

	g_list_foreach(priv->in_room, (GFunc)g_object_unref, NULL);
	g_list_free(priv->in_room);

	g_list_foreach(priv->ignored, (GFunc)g_free, NULL);
	g_list_free(priv->ignored);

	g_free(priv->who);
	g_free(priv->topic);
	g_free(priv->nick);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_chat_conversation_class_init(PurpleChatConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleConversationClass *conv_class = PURPLE_CONVERSATION_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_chat_conversation_dispose;
	obj_class->finalize = purple_chat_conversation_finalize;

	/* Setup properties */
	obj_class->get_property = purple_chat_conversation_get_property;
	obj_class->set_property = purple_chat_conversation_set_property;

	conv_class->write_message = chat_conversation_write_message;

	g_object_class_install_property(obj_class, CHAT_PROP_TOPIC_WHO,
			g_param_spec_string(CHAT_PROP_TOPIC_WHO_S, _("Who set topic"),
				_("Who set the chat topic."), NULL,
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, CHAT_PROP_TOPIC,
			g_param_spec_string(CHAT_PROP_TOPIC_S, _("Topic"),
				_("Topic of the chat."), NULL,
				G_PARAM_READABLE)
			);

	g_object_class_install_property(obj_class, CHAT_PROP_ID,
			g_param_spec_int(CHAT_PROP_ID_S, _("Chat ID"),
				_("The ID of the chat."), G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CHAT_PROP_NICK,
			g_param_spec_string(CHAT_PROP_NICK_S, _("Nickname"),
				_("The nickname of the user in a chat."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CHAT_PROP_LEFT,
			g_param_spec_boolean(CHAT_PROP_LEFT_S, _("Left the chat"),
				_("Whether the user has left the chat."), FALSE,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleChatConversationPrivate));
}

GType
purple_chat_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleChatConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_chat_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleChatConversation),
			0,
			(GInstanceInitFunc)purple_chat_conversation_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CONVERSATION,
				"PurpleChatConversation",
				&info, 0);
	}

	return type;
}

PurpleChatConversation *
purple_chat_conversation_new(PurpleAccount *account, const char *name)
{
	PurpleChatConversation *chat;
	PurpleConversation *conv;
	PurpleConnection *gc;
	PurpleConversationUiOps *ops;
	const char *disp;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((chat = purple_conversations_find_chat_with_account(name, account)) != NULL)
	{
		if (!purple_chat_conversation_has_left(chat)) {
			purple_debug_warning("conversation", "Trying to create multiple "
					"chats (%s) with the same name is deprecated and will be "
					"removed in libpurple 3.0.0", name);
		} else {
			/*
			 * This hack is necessary because some prpls (MSN) have unnamed chats
			 * that all use the same name.  A PurpleConversation for one of those
			 * is only ever re-used if the user has left, so calls to
			 * purple_conversation_new need to fall-through to creating a new
			 * chat.
			 * TODO 3.0.0: Remove this workaround and mandate unique names.
			 */

			chat_conversation_cleanup_for_rejoin(chat);
			return chat;
		}
	}

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	chat = g_object_new(PURPLE_TYPE_CHAT_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	conv = PURPLE_CONVERSATION(chat);

	/* copy features from the connection. */
	purple_conversation_set_features(conv,
			purple_connection_get_flags(gc));

	purple_conversations_add(conv);

	if ((disp = purple_connection_get_display_name(purple_account_get_connection(account))))
		purple_chat_conversation_set_nick(chat, disp);
	else
		purple_chat_conversation_set_nick(chat,
								purple_account_get_username(account));

	if (purple_prefs_get_bool("/purple/logging/log_chats"))
		purple_conversation_set_logging(conv, TRUE);

	/* Auto-set the title. */
	purple_conversation_autoset_title(conv);

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = purple_conversations_get_ui_ops();
	purple_conversation_set_ui_ops(PURPLE_CONVERSATION(chat), ops);
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-created", chat); /* TODO chat-created */

	return chat;
}

/**************************************************************************
 * Chat Conversation Buddy API
 **************************************************************************/
static int
purple_chat_conversation_buddy_compare(PurpleChatConversationBuddy *a, PurpleChatConversationBuddy *b)
{
	PurpleChatConversationBuddyFlags f1 = 0, f2 = 0;
	PurpleChatConversationBuddyPrivate *priva, *privb;
	char *user1 = NULL, *user2 = NULL;
	gint ret = 0;

	priva = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(a);
	privb = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(b);

	if (priva) {
		f1 = priva->flags;
		if (priva->alias_key)
			user1 = priva->alias_key;
		else if (priva->name)
			user1 = priva->name;
	}

	if (privb) {
		f2 = privb->flags;
		if (privb->alias_key)
			user2 = privb->alias_key;
		else if (privb->name)
			user2 = privb->name;
	}

	if (user1 == NULL || user2 == NULL) {
		if (!(user1 == NULL && user2 == NULL))
			ret = (user1 == NULL) ? -1: 1;
	} else if (f1 != f2) {
		/* sort more important users first */
		ret = (f1 > f2) ? -1 : 1;
	} else if (priva->buddy != privb->buddy) {
		ret = priva->buddy ? -1 : 1;
	} else {
		ret = purple_utf8_strcasecmp(user1, user2);
	}

	return ret;
}

const char *
purple_chat_conversation_buddy_get_alias(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->alias;
}

const char *
purple_chat_conversation_buddy_get_name(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

void
purple_chat_conversation_buddy_set_flags(PurpleChatConversationBuddy *cb,
							  PurpleChatConversationBuddyFlags flags)
{
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddyFlags oldflags;
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	if (flags == priv->flags)
		return;

	oldflags = priv->flags;
	priv->flags = flags;

	ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(priv->chat));

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(cb);

	purple_signal_emit(purple_conversations_get_handle(),
					 "chat-buddy-flags", priv->chat, priv->name, oldflags, flags); /* TODO use ChatBuddy object */
}

PurpleChatConversationBuddyFlags
purple_chat_conversation_buddy_get_flags(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, PURPLE_CHAT_CONVERSATION_BUDDY_NONE);

	return priv->flags;
}

const char *
purple_chat_conversation_buddy_get_attribute(PurpleChatConversationBuddy *cb, const char *key)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);
	
	return g_hash_table_lookup(priv->attributes, key);
}

static void
append_attribute_key(gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;
	*list = g_list_prepend(*list, key);
}

GList *
purple_chat_conversation_buddy_get_attribute_keys(PurpleChatConversationBuddy *cb)
{
	GList *keys = NULL;
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);
	
	g_return_val_if_fail(priv != NULL, NULL);
	
	g_hash_table_foreach(priv->attributes, (GHFunc)append_attribute_key, &keys);
	
	return keys;
}

void
purple_chat_conversation_buddy_set_attribute(PurpleChatConversationBuddy *cb,
		PurpleChatConversation *chat, const char *key, const char *value)
{
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(key != NULL);
	g_return_if_fail(value != NULL);

	g_hash_table_replace(priv->attributes, g_strdup(key), g_strdup(value));

	ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(chat));

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(cb);
}

void
purple_chat_conversation_buddy_set_attributes(PurpleChatConversationBuddy *cb,
		PurpleChatConversation *chat, GList *keys, GList *values)
{
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(keys != NULL);
	g_return_if_fail(values != NULL);

	while (keys != NULL && values != NULL) {
		g_hash_table_replace(priv->attributes, g_strdup(keys->data), g_strdup(values->data));
		keys = g_list_next(keys);
		values = g_list_next(values);
	}

	ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(chat));

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(cb);
}

void
purple_chat_conversation_buddy_set_ui_data(PurpleChatConversationBuddy *cb, gpointer ui_data)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	priv->ui_data = ui_data;
}

gpointer
purple_chat_conversation_buddy_get_ui_data(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ui_data;
}

void
purple_chat_conversation_buddy_set_chat(PurpleChatConversationBuddy *cb,
		PurpleChatConversation *chat)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	priv->chat = chat;
}

PurpleChatConversation *
purple_chat_conversation_buddy_get_chat(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->chat;
}

void
purple_chat_conversation_buddy_set_buddy(const PurpleChatConversationBuddy *cb,
		gboolean buddy)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	priv->buddy = buddy;
}

gboolean
purple_chat_conversation_buddy_is_buddy(const PurpleChatConversationBuddy *cb)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->buddy;
}

/**************************************************************************
 * GObject code for chat buddy
 **************************************************************************/

/* GObject Property names */
#define CB_PROP_CHAT_S   "chat"
#define CB_PROP_NAME_S   "name"
#define CB_PROP_ALIAS_S  "alias"
#define CB_PROP_BUDDY_S  "buddy"
#define CB_PROP_FLAGS_S  "flags"

/* Set method for GObject properties */
static void
purple_chat_conversation_buddy_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversationBuddy *cb = PURPLE_CHAT_CONVERSATION_BUDDY(obj);
	PurpleChatConversationBuddyPrivate *priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	switch (param_id) {
		case CB_PROP_CHAT:
			purple_chat_conversation_buddy_set_chat(cb, g_value_get_object(value));
			break;
		case CB_PROP_NAME:
			g_free(priv->name);
			priv->name = g_strdup(g_value_get_string(value));
			break;
		case CB_PROP_ALIAS:
			g_free(priv->alias);
			priv->alias = g_strdup(g_value_get_string(value));
			break;
		case CB_PROP_BUDDY:
			priv->buddy = g_value_get_boolean(value);
			break;
		case CB_PROP_FLAGS:
			purple_chat_conversation_buddy_set_flags(cb, g_value_get_flags(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_chat_conversation_buddy_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversationBuddy *cb = PURPLE_CHAT_CONVERSATION_BUDDY(obj);

	switch (param_id) {
		case CB_PROP_CHAT:
			g_value_set_object(value, purple_chat_conversation_buddy_get_chat(cb));
			break;
		case CB_PROP_NAME:
			g_value_set_string(value, purple_chat_conversation_buddy_get_name(cb));
			break;
		case CB_PROP_ALIAS:
			g_value_set_string(value, purple_chat_conversation_buddy_get_alias(cb));
			break;
		case CB_PROP_BUDDY:
			g_value_set_boolean(value, purple_chat_conversation_buddy_is_buddy(cb));
			break;
		case CB_PROP_FLAGS:
			g_value_set_flags(value, purple_chat_conversation_buddy_get_flags(cb));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_chat_conversation_buddy_init(GTypeInstance *instance, gpointer klass)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(instance);

	priv->attributes = g_hash_table_new_full(g_str_hash, g_str_equal,
										   g_free, g_free);
}

/* GObject dispose function */
static void
purple_chat_conversation_buddy_dispose(GObject *object)
{
	PurpleChatConversationBuddy *cb = PURPLE_CHAT_CONVERSATION_BUDDY(object);

	purple_signal_emit(purple_conversations_get_handle(),
			"deleting-chat-buddy", cb);
	PURPLE_DBUS_UNREGISTER_POINTER(cb);

	cb_parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_chat_conversation_buddy_finalize(GObject *object)
{
	PurpleChatConversationBuddyPrivate *priv;
	priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(object);

	g_free(priv->alias);
	g_free(priv->alias_key);
	g_free(priv->name);
	g_hash_table_destroy(priv->attributes);

	cb_parent_class->finalize(object);
}

/* Class initializer function */
static void purple_chat_conversation_buddy_class_init(PurpleChatConversationBuddyClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	cb_parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_chat_conversation_buddy_dispose;
	obj_class->finalize = purple_chat_conversation_buddy_finalize;

	/* Setup properties */
	obj_class->get_property = purple_chat_conversation_buddy_get_property;
	obj_class->set_property = purple_chat_conversation_buddy_set_property;

	g_object_class_install_property(obj_class, CB_PROP_CHAT,
			g_param_spec_object(CB_PROP_CHAT_S, _("Chat"),
				_("The chat the buddy belongs to."), PURPLE_TYPE_CHAT_CONVERSATION,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, CB_PROP_NAME,
			g_param_spec_string(CB_PROP_NAME_S, _("Name"),
				_("Name of the chat buddy."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CB_PROP_ALIAS,
			g_param_spec_string(CB_PROP_ALIAS_S, _("Alias"),
				_("Alias of the chat buddy."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CB_PROP_BUDDY,
			g_param_spec_boolean(CB_PROP_BUDDY_S, _("Is buddy"),
				_("Whether the chat buddy is in the buddy list."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CB_PROP_FLAGS,
			g_param_spec_flags(CB_PROP_FLAGS_S, _("Buddy flags"),
				_("The flags for the chat buddy."),
				PURPLE_TYPE_CHAT_CONVERSATION_BUDDY_FLAGS,
				PURPLE_CHAT_CONVERSATION_BUDDY_NONE, G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleChatConversationBuddyPrivate));
}

GType
purple_chat_conversation_buddy_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleChatConversationBuddyClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_chat_conversation_buddy_class_init,
			NULL,
			NULL,
			sizeof(PurpleChatConversationBuddy),
			0,
			(GInstanceInitFunc)purple_chat_conversation_buddy_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleChatConversationBuddy",
				&info, 0);
	}

	return type;
}

PurpleChatConversationBuddy *
purple_chat_conversation_buddy_new(PurpleChatConversation *chat, const char *name,
		const char *alias, PurpleChatConversationBuddyFlags flags)
{
	PurpleChatConversationBuddy *cb;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	cb = g_object_new(PURPLE_TYPE_CHAT_CONVERSATION_BUDDY,
			"chat", chat,
			"name",  name,
			"alias", alias,
			"flags", flags,
			NULL);

	PURPLE_DBUS_REGISTER_POINTER(cb, PurpleChatConversationBuddy);
	return cb;
}
