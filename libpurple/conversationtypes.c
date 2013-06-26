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
#include "conversationtypes.h"

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
	PROP_0,
	PROP_TOPIC_WHO,
	PROP_TOPIC,
	PROP_CHAT_ID,
	PROP_NICK,
	PROP_LEFT,
	PROP_LAST
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
	PROP_0,
	PROP_TYPING_STATE,
	PROP_ICON,
	PROP_LAST
};

/**
 * Data for "Chat Buddies"
 */
struct _PurpleChatConversationBuddyPrivate
{
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
	PROP_0,
	PROP_NAME,
	PROP_ALIAS,
	PROP_BUDDY,
	PROP_FLAGS,
	PROP_LAST
};

static PurpleConversationClass *parent_class;
static GObjectClass *cb_parent_class;

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
static gboolean
reset_typing_cb(gpointer data)
{
	PurpleConversation *c = (PurpleConversation *)data;
	PurpleIMConversationPrivate *priv;

	im = PURPLE_IM_CONVERSATION_GET_PRIVATE(c);

	purple_im_conversation_set_typing_state(im, PURPLE_IM_CONVERSATION_NOT_TYPING);
	purple_im_conversation_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed_cb(gpointer data)
{
	PurpleConversation *conv = (PurpleConversation *)data;
	PurpleConnection *gc;
	const char *name;

	g_return_val_if_fail(conv != NULL, FALSE);

	gc   = purple_conversation_get_connection(conv);
	name = purple_conversation_get_name(conv);

	if (gc != NULL && name != NULL) {
		/* We set this to 1 so that PURPLE_IM_CONVERSATION_TYPING will be sent
		 * if the Purple user types anything else.
		 */
		purple_im_conversation_set_type_again(PURPLE_IM_CONVERSATION_GET_PRIVATE(conv), 1);

		serv_send_typing(gc, name, PURPLE_IM_CONVERSATION_TYPED);

		purple_debug(PURPLE_DEBUG_MISC, "conversation", "typed...\n");
	}

	return FALSE;
}

void
purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon)
{
	g_return_if_fail(im != NULL);

	if (priv->icon != icon)
	{
		purple_buddy_icon_unref(priv->icon);

		priv->icon = (icon == NULL ? NULL : purple_buddy_icon_ref(icon));
	}

	purple_conversation_update(purple_im_conversation_get_conversation(im),
							 PURPLE_CONVERSATION_UPDATE_ICON);
}

PurpleBuddyIcon *
purple_im_conversation_get_icon(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return priv->icon;
}

void
purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMConversationTypingState state)
{
	g_return_if_fail(im != NULL);

	if (priv->typing_state != state)
	{
		priv->typing_state = state;

		switch (state)
		{
			case PURPLE_IM_CONVERSATION_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing", priv->priv->account, priv->priv->name);
				break;
			case PURPLE_IM_CONVERSATION_TYPED:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typed", priv->priv->account, priv->priv->name);
				break;
			case PURPLE_IM_CONVERSATION_NOT_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing-stopped", priv->priv->account, priv->priv->name);
				break;
		}

		purple_im_conversation_update_typing(im);
	}
}

PurpleIMConversationTypingState
purple_im_conversation_get_typing_state(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return priv->typing_state;
}

void
purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout)
{
	PurpleConversation *conv;

	g_return_if_fail(im != NULL);

	if (priv->typing_timeout > 0)
		purple_im_conversation_stop_typing_timeout(im);

	conv = purple_im_conversation_get_conversation(im);

	priv->typing_timeout = purple_timeout_add_seconds(timeout, reset_typing_cb, conv);
}

void
purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	if (priv->typing_timeout == 0)
		return;

	purple_timeout_remove(priv->typing_timeout);
	priv->typing_timeout = 0;
}

guint
purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return priv->typing_timeout;
}

void
purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val)
{
	g_return_if_fail(im != NULL);

	if (val == 0)
		priv->type_again = 0;
	else
		priv->type_again = time(NULL) + val;
}

time_t
purple_im_conversation_get_type_again(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return priv->type_again;
}

void
purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	priv->send_typed_timeout = purple_timeout_add_seconds(SEND_TYPED_TIMEOUT_SECONDS,
	                                                    send_typed_cb,
	                                                    purple_im_conversation_get_conversation(im));
}

void
purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	if (priv->send_typed_timeout == 0)
		return;

	purple_timeout_remove(priv->send_typed_timeout);
	priv->send_typed_timeout = 0;
}

guint
purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return priv->send_typed_timeout;
}

void
purple_im_conversation_update_typing(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	purple_conversation_update(purple_im_conversation_get_conversation(im),
							 PURPLE_CONVERSATION_UPDATE_TYPING);
}

static void
im_conversation_write_message(PurpleIMConversation *im, const char *who, const char *message,
			  PurpleMessageFlags flags, time_t mtime)
{
	PurpleConversation *c;

	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	c = purple_im_conversation_get_conversation(im);

	if ((flags & PURPLE_MESSAGE_RECV) == PURPLE_MESSAGE_RECV) {
		purple_im_conversation_set_typing_state(im, PURPLE_IM_CONVERSATION_NOT_TYPING);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, flags, mtime);
	else
		purple_conversation_write(c, who, message, flags, mtime);
}

static void
im_conversation_send_message(PurpleIMConversation *im, const char *message, PurpleMessageFlags flags)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(purple_im_conversation_get_conversation(im), message, flags);
}

/**************************************************************************
 * GObject code for IMs
 **************************************************************************/

/* GObject Property names */
#define PROP_TYPING_STATE_S  "typing-state"
#define PROP_ICON_S          "icon"

/* Set method for GObject properties */
static void
purple_im_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case PROP_TYPING_STATE:
			purple_im_conversation_set_typing_state(im, g_value_get_enum(value));
			break;
		case PROP_ICON:
#warning TODO: change get_pointer to get_object if PurpleBuddyIcon is a GObject
			purple_im_conversation_set_icon(chat, g_value_get_pointer(value));
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
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case PROP_TYPING_STATE:
			g_value_set_enum(value, purple_im_conversation_get_typing_state(chat);
			break;
		case PROP_ICON:
#warning TODO: change set_pointer to set_object if PurpleBuddyIcon is a GObject
			g_value_set_pointer(value, purple_im_conversation_get_icon(im);
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
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

		if (purple_prefs_get_bool("/purple/conversations/im/send_typing"))
			serv_send_typing(gc, name, PURPLE_IM_CONVERSATION_NOT_TYPING);

		if (gc && prpl_info->convo_closed != NULL)
			prpl_info->convo_closed(gc, name);
	}

	purple_im_conversation_stop_typing_timeout(priv->u.im);
	purple_im_conversation_stop_send_typed_timeout(priv->u.im);

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_im_conversation_finalize(GObject *object)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(object);
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	purple_buddy_icon_unref(priv->u.im->icon);

	g_free(priv->u.im);

	parent_class->finalize(object);
}

/* Class initializer function */
static void purple_im_conversation_class_init(PurpleIMConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_im_conversation_dispose;
	obj_class->finalize = purple_im_conversation_finalize;

	/* Setup properties */
	obj_class->get_property = purple_im_conversation_get_property;
	obj_class->set_property = purple_im_conversation_set_property;

	parent_class->write_message = im_conversation_write_message;
	parent_class->send_message = im_conversation_send_message;

	g_object_class_install_property(obj_class, PROP_TYPING_STATE,
			g_param_spec_enum(PROP_TYPING_STATE_S, _("Typing state"),
				_("Status of the user's typing of a message."),
				PURPLE_TYPE_IM_CONVERSATION_TYPING_STATE,
				PURPLE_IM_CONVERSATION_NOT_TYPING, G_PARAM_READWRITE)
			);

#warning TODO: change spec_pointer to spec_object if PurpleBuddyIcon is a GObject
	g_object_class_install_property(obj_class, PROP_ICON,
			g_param_spec_pointer(PROP_ICON_S, _("Buddy icon"),
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
	PurpleConnection *gc;
	PurpleConversationUiOps *ops;
	PurpleBuddyIcon *icon;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((im = purple_conversations_find_im_with_account(type, name, account)) != NULL)
		return im;

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	/* TODO check here. conversation-updated signals are emitted before
	 * conversation-created signals because of the _set()'s
	 */
	im = g_object_new(PURPLE_TYPE_IM_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	/* copy features from the connection. */
	purple_conversation_set_features(PURPLE_CONVERSATION(im),
			purple_connection_get_flags(gc));

	purple_conversations_add(PURPLE_CONVERSATION(im));
	if ((icon = purple_buddy_icons_find(account, name)))
	{
		purple_im_conversation_set_icon(im, icon);
		/* purple_im_conversation_set_icon refs the icon. */
		purple_buddy_icon_unref(icon);
	}

	if (purple_prefs_get_bool("/purple/logging/log_ims"))
	{
		purple_conversation_set_logging(PURPLE_CONVERSATION(im), TRUE);
		open_log(conv);
	}

	/* Auto-set the title. */
	purple_conversation_autoset_title(PURPLE_CONVERSATION(im));

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = purple_conversations_get_ui_ops();
	purple_conversation_set_ui_ops(PURPLE_CONVERSATION(im), ops);
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(PURPLE_CONVERSATION(im));

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-created", im); /* TODO im-created */

	return im;
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/
PurpleConversation *
purple_chat_conversation_get_conversation(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return priv->conv;
}

GList *
purple_chat_conversation_get_users(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return priv->in_room;
}

void
purple_chat_conversation_ignore(PurpleChatConversation *chat, const char *name)
{
	g_return_if_fail(chat != NULL);
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

	g_return_if_fail(chat != NULL);
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
	g_return_val_if_fail(chat != NULL, NULL);

	priv->ignored = ignored;

	return ignored;
}

GList *
purple_chat_conversation_get_ignored(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

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
	g_return_if_fail(chat != NULL);

	g_free(priv->who);
	g_free(priv->topic);

	priv->who   = g_strdup(who);
	priv->topic = g_strdup(topic);

	purple_conversation_update(purple_chat_conversation_get_conversation(chat),
							 PURPLE_CONVERSATION_UPDATE_TOPIC);

	purple_signal_emit(purple_conversations_get_handle(), "chat-topic-changed",
					 priv->conv, priv->who, priv->topic);
}

const char *
purple_chat_conversation_get_topic(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return priv->topic;
}

const char *
purple_chat_conversation_get_topic_who(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return priv->who;
}

void
purple_chat_conversation_set_id(PurpleChatConversation *chat, int id)
{
	g_return_if_fail(chat != NULL);

	priv->id = id;
}

int
purple_chat_conversation_get_id(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, -1);

	return priv->id;
}

static void
chat_conversation_write_message(PurpleChatConversation *chat, const char *who, const char *message,
				PurpleMessageFlags flags, time_t mtime)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	PurpleConnection *gc;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	conv      = purple_chat_conversation_get_conversation(chat);
	gc        = purple_conversation_get_connection(conv);
	account   = purple_connection_get_account(gc);

	/* Don't display this if the person who wrote it is ignored. */
	if (purple_chat_conversation_is_ignored_user(chat, who))
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

	/* Pass this on to either the ops structure or the default write func. */
	if (priv->ui_ops != NULL && priv->ui_ops->write_chat != NULL)
		priv->ui_ops->write_chat(conv, who, message, flags, mtime);
	else
		purple_conversation_write(conv, who, message, flags, mtime);
}

static void
chat_conversation_send_message(PurpleChatConversation *chat, const char *message, PurpleMessageFlags flags)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(message != NULL);

	common_send(purple_chat_conversation_get_conversation(chat), message, flags);
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
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	GList *ul, *fl;
	GList *cbuddies = NULL;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = purple_chat_conversation_get_conversation(chat);
	ops  = purple_conversation_get_ui_ops(conv);

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
			if (purple_strequal(priv->nick, purple_normalize(priv->account, user))) {
				const char *alias2 = purple_account_get_private_alias(priv->account);
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
						 "chat-buddy-joining", conv, user, flag)) ||
				purple_chat_conversation_is_ignored_user(chat, user);

		cbuddy = purple_chat_conversation_buddy_new(user, alias, flag);
		cbuddy->buddy = purple_find_buddy(priv->account, user) != NULL;

		priv->in_room = g_list_prepend(priv->in_room, cbuddy);
		g_hash_table_replace(priv->users, g_strdup(cbuddy->name), cbuddy);

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
						 "chat-buddy-joined", conv, user, flag, new_arrivals);
		ul = ul->next;
		fl = fl->next;
		if (extra_msgs != NULL)
			extra_msgs = extra_msgs->next;
	}

	cbuddies = g_list_sort(cbuddies, (GCompareFunc)purple_chat_conversation_buddy_compare);

	if (ops != NULL && ops->chat_add_users != NULL)
		ops->chat_add_users(conv, cbuddies, new_arrivals);

	g_list_free(cbuddies);
}

void
purple_chat_conversation_rename_user(PurpleChatConversation *chat, const char *old_user,
						   const char *new_user)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	PurpleChatConversationBuddy *cb;
	PurpleChatConversationBuddyFlags flags;
	const char *new_alias = new_user;
	char tmp[BUF_LONG];
	gboolean is_me = FALSE;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv = purple_chat_conversation_get_conversation(chat);
	ops  = purple_conversation_get_ui_ops(conv);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	if (purple_strequal(priv->nick, purple_normalize(priv->account, old_user))) {
		const char *alias;

		/* Note this for later. */
		is_me = TRUE;

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			alias = purple_account_get_private_alias(priv->account);
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

	flags = purple_chat_conversation_user_get_flags(chat, old_user);
	cb = purple_chat_conversation_buddy_new(new_user, new_alias, flags);
	cb->buddy = purple_find_buddy(priv->account, new_user) != NULL;

	priv->in_room = g_list_prepend(priv->in_room, cb);
	g_hash_table_replace(priv->users, g_strdup(cb->name), cb);

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user, new_alias);

	cb = purple_chat_conversation_find_buddy(chat, old_user);

	if (cb) {
		priv->in_room = g_list_remove(priv->in_room, cb);
		g_hash_table_remove(priv->users, cb->name);
		purple_chat_conversation_buddy_destroy(cb);
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
	GList *l;
	gboolean quiet;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = purple_chat_conversation_get_conversation(chat);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	ops  = purple_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;
		quiet = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_conversations_get_handle(),
					"chat-buddy-leaving", conv, user, reason)) |
				purple_chat_conversation_is_ignored_user(chat, user);

		cb = purple_chat_conversation_find_buddy(chat, user);

		if (cb) {
			priv->in_room = g_list_remove(priv->in_room, cb);
			g_hash_table_remove(priv->users, cb->name);
			purple_chat_conversation_buddy_destroy(cb);
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
		ops->chat_remove_users(conv, users);
}

void
purple_chat_conversation_clear_users(PurpleChatConversation *chat)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	GList *users;
	GList *l;
	GList *names = NULL;

	g_return_if_fail(chat != NULL);

	conv  = purple_chat_conversation_get_conversation(chat);
	ops   = purple_conversation_get_ui_ops(conv);
	users = priv->in_room;

	if (ops != NULL && ops->chat_remove_users != NULL) {
		for (l = users; l; l = l->next) {
			PurpleChatConversationBuddy *cb = l->data;
			names = g_list_prepend(names, cb->name);
		}
		ops->chat_remove_users(conv, names);
		g_list_free(names);
	}

	for (l = users; l; l = l->next)
	{
		PurpleChatConversationBuddy *cb = l->data;

		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-buddy-leaving", conv, cb->name, NULL);
		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-buddy-left", conv, cb->name, NULL);

		purple_chat_conversation_buddy_destroy(cb);
	}

	g_hash_table_remove_all(priv->users);

	g_list_free(users);
	priv->in_room = NULL;
}

void purple_chat_conversation_set_nick(PurpleChatConversation *chat, const char *nick) {
	g_return_if_fail(chat != NULL);

	g_free(chat->nick);
	chat->nick = g_strdup(purple_normalize(chat->priv->account, nick));
}

const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat) {
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->nick;
}

static void
invite_user_to_chat(gpointer data, PurpleRequestFields *fields)
{
	PurpleConversation *conv;
	PurpleChatConversationPrivate *priv;
	const char *user, *message;

	conv = data;
	chat = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv);
	user = purple_request_fields_get_string(fields, "screenname");
	message = purple_request_fields_get_string(fields, "message");

	serv_chat_invite(purple_conversation_get_connection(conv), chat->id, message, user);
}

void purple_chat_conversation_invite_user(PurpleChatConversation *chat, const char *user,
		const char *message, gboolean confirm)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	g_return_if_fail(chat);

	if (!user || !*user || !message || !*message)
		confirm = TRUE;

	conv = chat->conv;
	account = priv->account;

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

	purple_request_fields(conv, _("Invite to chat"), NULL,
			_("Please enter the name of the user you wish to invite, "
				"along with an optional invite message."),
			fields,
			_("Invite"), G_CALLBACK(invite_user_to_chat),
			_("Cancel"), NULL,
			account, user, conv,
			conv);
}

gboolean
purple_chat_conversation_find_user(PurpleChatConversation *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (purple_chat_conversation_find_buddy(chat, user) != NULL);
}

void
purple_chat_conversation_user_set_flags(PurpleChatConversation *chat, const char *user,
							  PurpleChatConversationBuddyFlags flags)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleChatConversationBuddy *cb;
	PurpleChatConversationBuddyFlags oldflags;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	cb = purple_chat_conversation_find_buddy(chat, user);

	if (!cb)
		return;

	if (flags == cb->flags)
		return;

	oldflags = cb->flags;
	cb->flags = flags;

	conv = purple_chat_conversation_get_conversation(chat);
	ops = purple_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(conv, user);

	purple_signal_emit(purple_conversations_get_handle(),
					 "chat-buddy-flags", conv, user, oldflags, flags);
}

PurpleChatConversationBuddyFlags
purple_chat_conversation_user_get_flags(PurpleChatConversation *chat, const char *user)
{
	PurpleChatConversationBuddy *cb;

	g_return_val_if_fail(chat != NULL, 0);
	g_return_val_if_fail(user != NULL, 0);

	cb = purple_chat_conversation_find_buddy(chat, user);

	if (!cb)
		return PURPLE_CHAT_CONVERSATION_BUDDY_NONE;

	return cb->flags;
}

void
purple_chat_conversation_leave(PurpleChatConversation *chat)
{
	g_return_if_fail(chat != NULL);

	priv->left = TRUE;
	purple_conversation_update(priv->conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

gboolean
purple_chat_conversation_has_left(PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, TRUE);

	return priv->left;
}

static void
purple_chat_conversation_cleanup_for_rejoin(PurpleChatConversation *chat)
{
	const char *disp;
	PurpleAccount *account;
	PurpleConnection *gc;

	account = purple_conversation_get_account(conv);

	purple_conversation_close_logs(conv);
	open_log(conv);

	gc = purple_account_get_connection(account);

	if ((disp = purple_connection_get_display_name(gc)) != NULL)
		purple_chat_conversation_set_nick(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv), disp);
	else
	{
		purple_chat_conversation_set_nick(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv),
								purple_account_get_username(account));
	}

	purple_chat_conversation_clear_users(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv));
	purple_chat_conversation_set_topic(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv), NULL, NULL);
	PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)->left = FALSE;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

PurpleChatConversationBuddy *
purple_chat_conversation_find_buddy(PurpleChatConversation *chat, const char *name)
{
	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_hash_table_lookup(priv->users, name);
}

/**************************************************************************
 * GObject code for chats
 **************************************************************************/

/* GObject Property names */
#define PROP_TOPIC_WHO_S  "topic-who"
#define PROP_TOPIC_S      "topic"
#define PROP_CHAT_ID_S    "chat-id"
#define PROP_NICK_S       "nick"
#define PROP_LEFT_S       "left"

/* Set method for GObject properties */
static void
purple_chat_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(obj);

	switch (param_id) {
		case PROP_CHAT_ID:
			purple_chat_conversation_set_id(chat, g_value_get_int(value));
			break;
		case PROP_NICK:
			purple_chat_conversation_set_nick(chat, g_value_get_string(value));
			break;
		case PROP_LEFT:
			{
				gboolean left = g_value_get_boolean(value);
				if (left == TRUE)
					purple_chat_conversation_leave(chat, left);
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
		case PROP_TOPIC_WHO:
			g_value_set_string(value, purple_chat_conversation_get_topic_who(chat);
			break;
		case PROP_TOPIC:
			g_value_set_string(value, purple_chat_conversation_get_topic(chat);
			break;
		case PROP_CHAT_ID:
			g_value_set_int(value, purple_chat_conversation_get_id(chat);
			break;
		case PROP_NICK:
			g_value_set_string(value, purple_chat_conversation_get_nick(chat);
			break;
		case PROP_LEFT:
			g_value_set_boolean(value, purple_chat_conversation_has_left(chat);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_chat_conversation_init(GTypeInstance *instance, gpointer klass)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(instance);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(im);

	priv->users = g_hash_table_new_full(_purple_conversation_user_hash,
			_purple_conversation_user_equal, g_free, NULL);
}

/* GObject dispose function */
static void
purple_chat_conversation_dispose(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

		int chat_id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv));
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
		if (!purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)))
			serv_chat_leave(gc, chat_id);

		/*
		 * If they didn't call serv_got_chat_left by now, it's too late.
		 * So we better do it for them before we destroy the thing.
		 */
		if (!purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)))
			serv_got_chat_left(gc, chat_id);
	}

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_chat_conversation_finalize(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_hash_table_destroy(priv->u.chat->users);

	g_list_foreach(priv->u.chat->in_room, (GFunc)purple_chat_conversation_buddy_destroy, NULL);
	g_list_free(priv->u.chat->in_room);

	g_list_foreach(priv->u.chat->ignored, (GFunc)g_free, NULL);
	g_list_free(priv->u.chat->ignored);

	g_free(priv->u.chat->who);
	g_free(priv->u.chat->topic);
	g_free(priv->u.chat->nick);
	g_free(priv->u.chat);

	parent_class->finalize(object);
}

/* Class initializer function */
static void purple_chat_conversation_class_init(PurpleChatConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_chat_conversation_dispose;
	obj_class->finalize = purple_chat_conversation_finalize;

	/* Setup properties */
	obj_class->get_property = purple_chat_conversation_get_property;
	obj_class->set_property = purple_chat_conversation_set_property;

	parent_class->write_message = chat_conversation_write_message;
	parent_class->send_message = chat_conversation_send_message;

	g_object_class_install_property(obj_class, PROP_TOPIC_WHO,
			g_param_spec_string(PROP_TOPIC_WHO_S, _("Who set topic"),
				_("Who set the chat topic."), NULL,
				G_PARAM_READONLY)
			);

	g_object_class_install_property(obj_class, PROP_TOPIC,
			g_param_spec_string(PROP_TOPIC_S, _("Topic"),
				_("Topic of the chat."), NULL,
				G_PARAM_READONLY)
			);

	g_object_class_install_property(obj_class, PROP_CHAT_ID,
			g_param_spec_int(PROP_CHAT_ID_S, _("Chat ID"),
				_("The ID of the chat."), G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_NICK,
			g_param_spec_string(PROP_NICK_S, _("Nickname"),
				_("The nickname of the user in a chat."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_LEFT,
			g_param_spec_boolean(PROP_LEFT_S, _("Left the chat"),
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

			purple_chat_conversation_cleanup_for_rejoin(chat);
			return chat;
		}
	}

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	/* TODO check here. conversation-updated signals are emitted before
	 * conversation-created signals because of the _set()'s
	 */
	chat = g_object_new(PURPLE_TYPE_CHAT_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	/* copy features from the connection. */
	purple_conversation_set_features(PURPLE_CONVERSATION(chat),
			purple_connection_get_flags(gc));

	purple_conversations_add(chat);

	if ((disp = purple_connection_get_display_name(purple_account_get_connection(account))))
		purple_chat_conversation_set_nick(chat, disp);
	else
		purple_chat_conversation_set_nick(chat,
								purple_account_get_username(account));

	if (purple_prefs_get_bool("/purple/logging/log_chats"))
	{
		purple_conversation_set_logging(PURPLE_CONVERSATION(chat), TRUE);
		open_log(conv);
	}

	/* Auto-set the title. */
	purple_conversation_autoset_title(PURPLE_CONVERSATION(chat));

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = purple_conversations_get_ui_ops();
	purple_conversation_set_ui_ops(PURPLE_CONVERSATION(im), ops);
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(PURPLE_CONVERSATION(chat));

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
	char *user1 = NULL, *user2 = NULL;
	gint ret = 0;

	if (a) {
		f1 = a->flags;
		if (a->alias_key)
			user1 = a->alias_key;
		else if (a->name)
			user1 = a->name;
	}

	if (b) {
		f2 = b->flags;
		if (b->alias_key)
			user2 = b->alias_key;
		else if (b->name)
			user2 = b->name;
	}

	if (user1 == NULL || user2 == NULL) {
		if (!(user1 == NULL && user2 == NULL))
			ret = (user1 == NULL) ? -1: 1;
	} else if (f1 != f2) {
		/* sort more important users first */
		ret = (f1 > f2) ? -1 : 1;
	} else if (a->buddy != b->buddy) {
		ret = a->buddy ? -1 : 1;
	} else {
		ret = purple_utf8_strcasecmp(user1, user2);
	}

	return ret;
}

const char *
purple_chat_conversation_buddy_get_alias(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, NULL);

	return cb->alias;
}

const char *
purple_chat_conversation_buddy_get_name(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, NULL);

	return cb->name;
}

PurpleChatConversationBuddyFlags
purple_chat_conversation_buddy_get_flags(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, PURPLE_CHAT_CONVERSATION_BUDDY_NONE);

	return cb->flags;
}

const char *
purple_chat_conversation_buddy_get_attribute(PurpleChatConversationBuddy *cb, const char *key)
{
	g_return_val_if_fail(cb != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);
	
	return g_hash_table_lookup(cb->attributes, key);
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
	
	g_return_val_if_fail(cb != NULL, NULL);
	
	g_hash_table_foreach(cb->attributes, (GHFunc)append_attribute_key, &keys);
	
	return keys;
}

void
purple_chat_conversation_buddy_set_attribute(PurpleChatConversation *chat, PurpleChatConversationBuddy *cb, const char *key, const char *value)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	
	g_return_if_fail(cb != NULL);
	g_return_if_fail(key != NULL);
	g_return_if_fail(value != NULL);
	
	g_hash_table_replace(cb->attributes, g_strdup(key), g_strdup(value));
	
	conv = purple_chat_conversation_get_conversation(chat);
	ops = purple_conversation_get_ui_ops(conv);
	
	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(conv, cb->name);
}

void
purple_chat_conversation_buddy_set_attributes(PurpleChatConversation *chat, PurpleChatConversationBuddy *cb, GList *keys, GList *values)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	
	g_return_if_fail(cb != NULL);
	g_return_if_fail(keys != NULL);
	g_return_if_fail(values != NULL);
	
	while (keys != NULL && values != NULL) {
		g_hash_table_replace(cb->attributes, g_strdup(keys->data), g_strdup(values->data));
		keys = g_list_next(keys);
		values = g_list_next(values);
	}
	
	conv = purple_chat_conversation_get_conversation(chat);
	ops = purple_conversation_get_ui_ops(conv);
	
	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(conv, cb->name);
}

void purple_chat_conversation_buddy_set_ui_data(PurpleChatConversationBuddy *cb, gpointer ui_data)
{
	g_return_if_fail(cb != NULL);

	cb->ui_data = ui_data;
}

gpointer purple_chat_conversation_buddy_get_ui_data(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, NULL);

	return cb->ui_data;
}

gboolean purple_chat_conversation_buddy_is_buddy(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, FALSE);

	return cb->buddy;
}

/**************************************************************************
 * GObject code for chat buddy
 **************************************************************************/

/* GObject Property names */
#define PROP_NAME_S   "name"
#define PROP_ALIAS_S  "alias"
#define PROP_BUDDY_S  "buddy"
#define PROP_FLAGS_S  "flags"

/* Set method for GObject properties */
static void
purple_chat_conversation_buddy_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversationBuddy *cb = PURPLE_CHAT_CONVERSATION_BUDDY(obj);
	PurpleChatConversationBuddyPrivate *priv = PURPLE_CHAT_CONVERSATION_BUDDY_GET_PRIVATE(cb);

	switch (param_id) {
		case PROP_NAME:
			g_free(priv->name);
			priv->name = g_strdup(g_value_get_string(value));
			break;
		case PROP_ALIAS:
			g_free(priv->alias);
			priv->alias = g_strdup(g_value_get_string(value));
			break;
		case PROP_BUDDY:
			priv->buddy = g_value_get_boolean(value);
			break;
		case PROP_FLAGS:
			priv->flags = g_value_get_flags(value);
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
		case PROP_NAME:
			g_value_set_string(value, purple_chat_conversation_buddy_get_name(cb));
			break;
		case PROP_ALIAS:
			g_value_set_string(value, purple_chat_conversation_buddy_get_alias(cb));
			break;
		case PROP_BUDDY:
			g_value_set_boolean(value, purple_chat_conversation_buddy_is_buddy(cb));
			break;
		case PROP_FLAGS:
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
	cb->attributes = g_hash_table_new_full(g_str_hash, g_str_equal,
										   g_free, g_free);
}

/* GObject dispose function */
static void
purple_chat_conversation_buddy_dispose(GObject *object)
{
	purple_signal_emit(purple_conversations_get_handle(),
			"deleting-chat-buddy", cb);
	PURPLE_DBUS_UNREGISTER_POINTER(cb);

	cb_parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_chat_conversation_buddy_finalize(GObject *object)
{
	g_free(cb->alias);
	g_free(cb->alias_key);
	g_free(cb->name);
	g_hash_table_destroy(cb->attributes);

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

	g_object_class_install_property(obj_class, PROP_NAME,
			g_param_spec_string(PROP_NAME_S, _("Name"),
				_("Name of the chat buddy."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_ALIAS,
			g_param_spec_string(PROP_ALIAS_S, _("Alias"),
				_("Alias of the chat buddy."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_BUDDY,
			g_param_spec_boolean(PROP_BUDDY_S, _("Is buddy"),
				_("Whether the chat buddy is in the buddy list."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_FLAGS,
			g_param_spec_flags(PROP_FLAGS_S, _("Buddy flags"),
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
purple_chat_conversation_buddy_new(const char *name, const char *alias, PurpleChatConversationBuddyFlags flags)
{
	PurpleChatConversationBuddy *cb;

	g_return_val_if_fail(name != NULL, NULL);

	cb = g_object_new(PURPLE_TYPE_CHAT_CONVERSATION_BUDDY,
			"name",  name,
			"alias", alias,
			"flags", flags,
			NULL);

	PURPLE_DBUS_REGISTER_POINTER(cb, PurpleChatConversationBuddy);
	return cb;
}
