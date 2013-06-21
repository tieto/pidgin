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
#include "blist.h"
#include "cmds.h"
#include "conversation.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "util.h"

#define SEND_TYPED_TIMEOUT_SECONDS 5

/** @copydoc _PurpleConversationPrivate */
typedef struct _PurpleConversationPrivate         PurpleConversationPrivate;
/** @copydoc _PurpleChatConversationPrivate */
typedef struct _PurpleChatConversationPrivate     PurpleChatConversationPrivate;
/** @copydoc _PurpleIMConversationPrivate */
typedef struct _PurpleIMConversationPrivate       PurpleIMConversationPrivate;
/** @copydoc _PurpleChatConversationBuddyPrivate */
typedef struct _PurpleChatConversationBuddyPrivate  PurpleChatConversationBuddyPrivate;

/**
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat.
 */
struct _PurpleConversationPrivate
{
	PurpleConversationType type;  /**< The type of conversation.          */

	PurpleAccount *account;       /**< The user using this conversation.  */


	char *name;                 /**< The name of the conversation.      */
	char *title;                /**< The window title.                  */

	gboolean logging;           /**< The status of logging.             */

	GList *logs;                /**< This conversation's logs           */

	PurpleConversationUiOps *ui_ops;           /**< UI-specific operations. */
	void *ui_data;                           /**< UI-specific data.       */

	GHashTable *data;                        /**< Plugin-specific data.   */

	PurpleConnectionFlags features; /**< The supported features */
	GList *message_history;         /**< Message history, as a GList of PurpleConversationMessage's */
};

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

/** TODO GBoxed
 * Description of a conversation message
 */
struct _PurpleConversationMessage
{
	char *who;
	char *what;
	PurpleConversationMessageFlags flags;
	time_t when;
	PurpleConversation *conv;
	char *alias;
};

/**
 * A hash table used for efficient lookups of conversations by name.
 * struct _purple_hconv => PurpleConversation*
 */
static GHashTable *conversation_cache = NULL;

struct _purple_hconv {
	PurpleConversationType type;
	char *name;
	const PurpleAccount *account;
};

static guint _purple_conversations_hconv_hash(struct _purple_hconv *hc)
{
	return g_str_hash(hc->name) ^ hc->type ^ g_direct_hash(hc->account);
}

static guint _purple_conversations_hconv_equal(struct _purple_hconv *hc1, struct _purple_hconv *hc2)
{
	return (hc1->type == hc2->type &&
	        hc1->account == hc2->account &&
	        g_str_equal(hc1->name, hc2->name));
}

static void _purple_conversations_hconv_free_key(struct _purple_hconv *hc)
{
	g_free(hc->name);
	g_free(hc);
}

static guint _purple_conversation_user_hash(gconstpointer data)
{
	const gchar *name = data;
	gchar *collated;
	guint hash;

	collated = g_utf8_collate_key(name, -1);
	hash     = g_str_hash(collated);
	g_free(collated);
	return hash;
}

static gboolean _purple_conversation_user_equal(gconstpointer a, gconstpointer b)
{
	return !g_utf8_collate(a, b);
}

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

static void
common_send(PurpleConversation *conv, const char *message, PurpleConversationMessageFlags msgflags)
{
	PurpleConversationType type;
	PurpleAccount *account;
	PurpleConnection *gc;
	char *displayed = NULL, *sent = NULL;
	int err = 0;

	if (*message == '\0')
		return;

	account = purple_conversation_get_account(conv);
	gc = purple_conversation_get_connection(conv);

	g_return_if_fail(account != NULL);
	g_return_if_fail(gc != NULL);

	type = purple_conversation_get_type(conv);

	/* Always linkfy the text for display, unless we're
	 * explicitly asked to do otheriwse*/
	if (!(msgflags & PURPLE_CONVERSATION_MESSAGE_INVISIBLE)) {
		if(msgflags & PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY)
			displayed = g_strdup(message);
		else
			displayed = purple_markup_linkify(message);
	}

	if (displayed && (conv->features & PURPLE_CONNECTION_HTML) &&
		!(msgflags & PURPLE_CONVERSATION_MESSAGE_RAW)) {
		sent = g_strdup(displayed);
	} else
		sent = g_strdup(message);

	msgflags |= PURPLE_CONVERSATION_MESSAGE_SEND;

	if (type == PURPLE_CONVERSATION_TYPE_IM) {
		PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(conv);

		purple_signal_emit(purple_conversations_get_handle(), "sending-im-msg",
						 account,
						 purple_conversation_get_name(conv), &sent);

		if (sent != NULL && sent[0] != '\0') {

			err = serv_send_im(gc, purple_conversation_get_name(conv),
			                   sent, msgflags);

			if ((err > 0) && (displayed != NULL))
				purple_im_conversation_write(im, NULL, displayed, msgflags, time(NULL));

			purple_signal_emit(purple_conversations_get_handle(), "sent-im-msg",
							 account,
							 purple_conversation_get_name(conv), sent);
		}
	}
	else {
		purple_signal_emit(purple_conversations_get_handle(), "sending-chat-msg",
						 account, &sent,
						 purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)));

		if (sent != NULL && sent[0] != '\0') {
			err = serv_chat_send(gc, purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)), sent, msgflags);

			purple_signal_emit(purple_conversations_get_handle(), "sent-chat-msg",
							 account, sent,
							 purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)));
		}
	}

	if (err < 0) {
		const char *who;
		const char *msg;

		who = purple_conversation_get_name(conv);

		if (err == -E2BIG) {
			msg = _("Unable to send message: The message is too large.");

			if (!purple_conversation_helper_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				purple_notify_error(gc, NULL, msg2, _("The message is too large."));
				g_free(msg2);
			}
		}
		else if (err == -ENOTCONN) {
			purple_debug(PURPLE_DEBUG_ERROR, "conversation",
					   "Not yet connected.\n");
		}
		else {
			msg = _("Unable to send message.");

			if (!purple_conversation_helper_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				purple_notify_error(gc, NULL, msg2, NULL);
				g_free(msg2);
			}
		}
	}

	g_free(displayed);
	g_free(sent);
}

static void
open_log(PurpleConversation *conv)
{
	conv->logs = g_list_append(NULL, purple_log_new(conv->type == PURPLE_CONVERSATION_TYPE_CHAT ? PURPLE_LOG_CHAT :
							   PURPLE_LOG_IM, conv->name, conv->account,
							   conv, time(NULL), NULL));
}

/* Functions that deal with PurpleConversationMessage */

static void
add_message_to_history(PurpleConversation *conv, const char *who, const char *alias,
		const char *message, PurpleConversationMessageFlags flags, time_t when)
{
	PurpleConversationMessage *msg;
	PurpleConnection *gc;

	gc = purple_account_get_connection(conv->account);

	if (flags & PURPLE_CONVERSATION_MESSAGE_SEND) {
		const char *me = NULL;
		if (gc)
			me = purple_connection_get_display_name(gc);
		if (!me)
			me = purple_account_get_username(conv->account);
		who = me;
	}

	msg = g_new0(PurpleConversationMessage, 1);
	PURPLE_DBUS_REGISTER_POINTER(msg, PurpleConversationMessage);
	msg->who = g_strdup(who);
	msg->alias = g_strdup(alias);
	msg->flags = flags;
	msg->what = g_strdup(message);
	msg->when = when;
	msg->conv = conv;

	conv->message_history = g_list_prepend(conv->message_history, msg);
}

static void
free_conv_message(PurpleConversationMessage *msg)
{
	g_free(msg->who);
	g_free(msg->alias);
	g_free(msg->what);
	PURPLE_DBUS_UNREGISTER_POINTER(msg);
	g_free(msg);
}

static void
message_history_free(GList *list)
{
	g_list_foreach(list, (GFunc)free_conv_message, NULL);
	g_list_free(list);
}

/**************************************************************************
 * Conversation API
 **************************************************************************/
static void
purple_conversation_chat_cleanup_for_rejoin(PurpleConversation *conv)
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

PurpleConversation *
purple_conversation_new(PurpleConversationType type, PurpleAccount *account,
					  const char *name)
{
	PurpleConversation *conv;
	PurpleConnection *gc;
	PurpleConversationUiOps *ops;
	struct _purple_hconv *hc;

	g_return_val_if_fail(type    != PURPLE_CONVERSATION_TYPE_UNKNOWN, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((conv = purple_conversations_find_with_account(type, name, account)) != NULL)
	{
		if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_CHAT &&
				!purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv))) {
			purple_debug_warning("conversation", "Trying to create multiple "
					"chats (%s) with the same name is deprecated and will be "
					"removed in libpurple 3.0.0", name);
		}

		/*
		 * This hack is necessary because some prpls (MSN) have unnamed chats
		 * that all use the same name.  A PurpleConversation for one of those
		 * is only ever re-used if the user has left, so calls to
		 * purple_conversation_new need to fall-through to creating a new
		 * chat.
		 * TODO 3.0.0: Remove this workaround and mandate unique names.
		 */
		if (purple_conversation_get_type(conv) != PURPLE_CONVERSATION_TYPE_CHAT ||
				purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)))
		{
			if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_CHAT)
				purple_conversation_chat_cleanup_for_rejoin(conv);

			return conv;
		}
	}

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	conv = g_new0(PurpleConversation, 1);
	PURPLE_DBUS_REGISTER_POINTER(conv, PurpleConversation);

	conv->type         = type;
	conv->account      = account;
	conv->name         = g_strdup(name);
	conv->title        = g_strdup(name);
	conv->data         = g_hash_table_new_full(g_str_hash, g_str_equal,
											   g_free, NULL);
	/* copy features from the connection. */
	conv->features = purple_connection_get_flags(gc);

	if (type == PURPLE_CONVERSATION_TYPE_IM)
	{
		PurpleBuddyIcon *icon;
		conv->u.im = g_new0(PurpleIMConversationPrivate, 1);
		conv->u.im->conv = conv;
		PURPLE_DBUS_REGISTER_POINTER(conv->u.im, PurpleIMConversationPrivate);

		ims = g_list_prepend(ims, conv);
		if ((icon = purple_buddy_icons_find(account, name)))
		{
			purple_im_conversation_set_icon(conv->u.im, icon);
			/* purple_im_conversation_set_icon refs the icon. */
			purple_buddy_icon_unref(icon);
		}

		if (purple_prefs_get_bool("/purple/logging/log_ims"))
		{
			purple_conversation_set_logging(conv, TRUE);
			open_log(conv);
		}
	}
	else if (type == PURPLE_CONVERSATION_TYPE_CHAT)
	{
		const char *disp;

		conv->u.chat = g_new0(PurpleChatConversationPrivate, 1);
		conv->u.chat->conv = conv;
		conv->u.chat->users = g_hash_table_new_full(_purple_conversation_user_hash,
				_purple_conversation_user_equal, g_free, NULL);
		PURPLE_DBUS_REGISTER_POINTER(conv->u.chat, PurpleChatConversationPrivate);

		chats = g_list_prepend(chats, conv);

		if ((disp = purple_connection_get_display_name(purple_account_get_connection(account))))
			purple_chat_conversation_set_nick(conv->u.chat, disp);
		else
			purple_chat_conversation_set_nick(conv->u.chat,
									purple_account_get_username(account));

		if (purple_prefs_get_bool("/purple/logging/log_chats"))
		{
			purple_conversation_set_logging(conv, TRUE);
			open_log(conv);
		}
	}

	conversations = g_list_prepend(conversations, conv);

	hc = g_new(struct _purple_hconv, 1);
	hc->name = g_strdup(purple_normalize(account, conv->name));
	hc->account = account;
	hc->type = type;

	g_hash_table_insert(conversation_cache, hc, conv);

	/* Auto-set the title. */
	purple_conversation_autoset_title(conv);

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = conv->ui_ops = default_ops;
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-created", conv);

	return conv;
}

void
purple_conversation_destroy(PurpleConversation *conv)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConversationUiOps *ops;
	PurpleConnection *gc;
	const char *name;
	struct _purple_hconv hc;

	g_return_if_fail(conv != NULL);

	purple_request_close_with_handle(conv);

	ops  = purple_conversation_get_ui_ops(conv);
	gc   = purple_conversation_get_connection(conv);
	name = purple_conversation_get_name(conv);

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

		if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_IM)
		{
			if (purple_prefs_get_bool("/purple/conversations/im/send_typing"))
				serv_send_typing(gc, name, PURPLE_IM_CONVERSATION_NOT_TYPING);

			if (gc && prpl_info->convo_closed != NULL)
				prpl_info->convo_closed(gc, name);
		}
		else if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_CHAT)
		{
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
	}

	/* remove from conversations and im/chats lists prior to emit */
	conversations = g_list_remove(conversations, conv);

	if(conv->type==PURPLE_CONVERSATION_TYPE_IM)
		ims = g_list_remove(ims, conv);
	else if(conv->type==PURPLE_CONVERSATION_TYPE_CHAT)
		chats = g_list_remove(chats, conv);

	hc.name = (gchar *)purple_normalize(conv->account, conv->name);
	hc.account = conv->account;
	hc.type = conv->type;

	g_hash_table_remove(conversation_cache, &hc);

	purple_signal_emit(purple_conversations_get_handle(),
					 "deleting-conversation", conv);

	g_free(conv->name);
	g_free(conv->title);

	conv->name = NULL;
	conv->title = NULL;

	if (conv->type == PURPLE_CONVERSATION_TYPE_IM) {
		purple_im_conversation_stop_typing_timeout(conv->u.im);
		purple_im_conversation_stop_send_typed_timeout(conv->u.im);

		purple_buddy_icon_unref(conv->u.im->icon);
		conv->u.im->icon = NULL;

		PURPLE_DBUS_UNREGISTER_POINTER(conv->u.im);
		g_free(conv->u.im);
		conv->u.im = NULL;
	}
	else if (conv->type == PURPLE_CONVERSATION_TYPE_CHAT) {
		g_hash_table_destroy(conv->u.chat->users);
		conv->u.chat->users = NULL;

		g_list_foreach(conv->u.chat->in_room, (GFunc)purple_chat_conversation_buddy_destroy, NULL);
		g_list_free(conv->u.chat->in_room);

		g_list_foreach(conv->u.chat->ignored, (GFunc)g_free, NULL);
		g_list_free(conv->u.chat->ignored);

		conv->u.chat->in_room = NULL;
		conv->u.chat->ignored = NULL;

		g_free(conv->u.chat->who);
		conv->u.chat->who = NULL;

		g_free(conv->u.chat->topic);
		conv->u.chat->topic = NULL;

		g_free(conv->u.chat->nick);

		PURPLE_DBUS_UNREGISTER_POINTER(conv->u.chat);
		g_free(conv->u.chat);
		conv->u.chat = NULL;
	}

	g_hash_table_destroy(conv->data);
	conv->data = NULL;

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);
	conv->ui_data = NULL;

	purple_conversation_close_logs(conv);

	purple_conversation_clear_message_history(conv);

	PURPLE_DBUS_UNREGISTER_POINTER(conv);
	g_free(conv);
	conv = NULL;
}


void
purple_conversation_present(PurpleConversation *conv) {
	PurpleConversationUiOps *ops;

	g_return_if_fail(conv != NULL);

	ops = purple_conversation_get_ui_ops(conv);
	if(ops && ops->present)
		ops->present(conv);
}


void
purple_conversation_set_features(PurpleConversation *conv, PurpleConnectionFlags features)
{
	g_return_if_fail(conv != NULL);

	conv->features = features;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_FEATURES);
}


PurpleConnectionFlags
purple_conversation_get_features(PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, 0);
	return conv->features;
}


PurpleConversationType
purple_conversation_get_type(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, PURPLE_CONVERSATION_TYPE_UNKNOWN);

	return conv->type;
}

void
purple_conversation_set_ui_ops(PurpleConversation *conv,
							 PurpleConversationUiOps *ops)
{
	g_return_if_fail(conv != NULL);

	if (conv->ui_ops == ops)
		return;

	if (conv->ui_ops != NULL && conv->ui_ops->destroy_conversation != NULL)
		conv->ui_ops->destroy_conversation(conv);

	conv->ui_data = NULL;

	conv->ui_ops = ops;
}

PurpleConversationUiOps *
purple_conversation_get_ui_ops(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->ui_ops;
}

void
purple_conversation_set_account(PurpleConversation *conv, PurpleAccount *account)
{
	g_return_if_fail(conv != NULL);

	if (account == purple_conversation_get_account(conv))
		return;

	conv->account = account;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_ACCOUNT);
}

PurpleAccount *
purple_conversation_get_account(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->account;
}

PurpleConnection *
purple_conversation_get_connection(const PurpleConversation *conv)
{
	PurpleAccount *account;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);

	if (account == NULL)
		return NULL;

	return purple_account_get_connection(account);
}

void
purple_conversation_set_title(PurpleConversation *conv, const char *title)
{
	g_return_if_fail(conv  != NULL);
	g_return_if_fail(title != NULL);

	g_free(conv->title);
	conv->title = g_strdup(title);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_TITLE);
}

const char *
purple_conversation_get_title(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->title;
}

void
purple_conversation_autoset_title(PurpleConversation *conv)
{
	PurpleAccount *account;
	PurpleBuddy *b;
	PurpleChat *chat;
	const char *text = NULL, *name;

	g_return_if_fail(conv != NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	if(purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_IM) {
		if(account && ((b = purple_find_buddy(account, name)) != NULL))
			text = purple_buddy_get_contact_alias(b);
	} else if(purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_CHAT) {
		if(account && ((chat = purple_blist_find_chat(account, name)) != NULL))
			text = purple_chat_get_name(chat);
	}


	if(text == NULL)
		text = name;

	purple_conversation_set_title(conv, text);
}

void
purple_conversation_set_name(PurpleConversation *conv, const char *name)
{
	struct _purple_hconv *hc;
	g_return_if_fail(conv != NULL);

	hc = g_new(struct _purple_hconv, 1);
	hc->type = conv->type;
	hc->account = conv->account;
	hc->name = (gchar *)purple_normalize(conv->account, conv->name);

	g_hash_table_remove(conversation_cache, hc);
	g_free(conv->name);

	conv->name = g_strdup(name);
	hc->name = g_strdup(purple_normalize(conv->account, conv->name));
	g_hash_table_insert(conversation_cache, hc, conv);

	purple_conversation_autoset_title(conv);
}

const char *
purple_conversation_get_name(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->name;
}

void
purple_conversation_set_logging(PurpleConversation *conv, gboolean log)
{
	g_return_if_fail(conv != NULL);

	if (conv->logging != log)
	{
		conv->logging = log;
		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_LOGGING);
	}
}

gboolean
purple_conversation_is_logging(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, FALSE);

	return conv->logging;
}

void
purple_conversation_close_logs(PurpleConversation *conv)
{
	g_return_if_fail(conv != NULL);

	g_list_foreach(conv->logs, (GFunc)purple_log_free, NULL);
	g_list_free(conv->logs);
	conv->logs = NULL;
}

void
purple_conversation_set_data(PurpleConversation *conv, const char *key,
						   gpointer data)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(key  != NULL);

	g_hash_table_replace(conv->data, g_strdup(key), data);
}

gpointer
purple_conversation_get_data(PurpleConversation *conv, const char *key)
{
	g_return_val_if_fail(conv != NULL, NULL);
	g_return_val_if_fail(key  != NULL, NULL);

	return g_hash_table_lookup(conv->data, key);
}

void
purple_conversation_write(PurpleConversation *conv, const char *who,
						const char *message, PurpleConversationMessageFlags flags,
						time_t mtime)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = NULL;
	PurpleAccount *account;
	PurpleConversationUiOps *ops;
	const char *alias;
	char *displayed = NULL;
	PurpleBuddy *b;
	int plugin_return;
	PurpleConversationType type;
	/* int logging_font_options = 0; */

	g_return_if_fail(conv    != NULL);
	g_return_if_fail(message != NULL);

	ops = purple_conversation_get_ui_ops(conv);

	account = purple_conversation_get_account(conv);
	type = purple_conversation_get_type(conv);

	if (account != NULL)
		gc = purple_account_get_connection(account);

	if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_CHAT &&
		(gc != NULL && !g_slist_find(gc->buddy_chats, conv)))
		return;

	if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_IM &&
		!g_list_find(purple_conversations_get(), conv))
		return;

	displayed = g_strdup(message);

	if (who == NULL || *who == '\0')
		who = purple_conversation_get_name(conv);
	alias = who;

	plugin_return =
		GPOINTER_TO_INT(purple_signal_emit_return_1(
			purple_conversations_get_handle(),
			(type == PURPLE_CONVERSATION_TYPE_IM ? "writing-im-msg" : "writing-chat-msg"),
			account, who, &displayed, conv, flags));

	if (displayed == NULL)
		return;

	if (plugin_return) {
		g_free(displayed);
		return;
	}

	if (account != NULL) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(account)));

		if (purple_conversation_get_type(conv) == PURPLE_CONVERSATION_TYPE_IM ||
			!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (flags & PURPLE_CONVERSATION_MESSAGE_SEND) {
				b = purple_find_buddy(account,
							purple_account_get_username(account));

				if (purple_account_get_private_alias(account) != NULL)
					alias = purple_account_get_private_alias(account);
				else if (b != NULL && !purple_strequal(purple_buddy_get_name(b), purple_buddy_get_contact_alias(b)))
					alias = purple_buddy_get_contact_alias(b);
				else if (purple_connection_get_display_name(gc) != NULL)
					alias = purple_connection_get_display_name(gc);
				else
					alias = purple_account_get_username(account);
			}
			else
			{
				b = purple_find_buddy(account, who);

				if (b != NULL)
					alias = purple_buddy_get_contact_alias(b);
			}
		}
	}

	if (!(flags & PURPLE_CONVERSATION_MESSAGE_NO_LOG) && purple_conversation_is_logging(conv)) {
		GList *log;

		if (conv->logs == NULL)
			open_log(conv);

		log = conv->logs;
		while (log != NULL) {
			purple_log_write((PurpleLog *)log->data, flags, alias, mtime, displayed);
			log = log->next;
		}
	}

	if (ops && ops->write_conv)
		ops->write_conv(conv, who, alias, displayed, flags, mtime);

	add_message_to_history(conv, who, alias, message, flags, mtime);

	purple_signal_emit(purple_conversations_get_handle(),
		(type == PURPLE_CONVERSATION_TYPE_IM ? "wrote-im-msg" : "wrote-chat-msg"),
		account, who, displayed, conv, flags);

	g_free(displayed);
}

gboolean
purple_conversation_has_focus(PurpleConversation *conv)
{
	gboolean ret = FALSE;
	PurpleConversationUiOps *ops;

	g_return_val_if_fail(conv != NULL, FALSE);

	ops = purple_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->has_focus != NULL)
		ret = ops->has_focus(conv);

	return ret;
}

/*
 * TODO: Need to make sure calls to this function happen in the core
 * instead of the UI.  That way UIs have less work to do, and the
 * core/UI split is cleaner.  Also need to make sure this is called
 * when chats are added/removed from the blist.
 */
void
purple_conversation_update(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	g_return_if_fail(conv != NULL);

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-updated", conv, type);
}

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
void
purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon)
{
	g_return_if_fail(im != NULL);

	if (im->icon != icon)
	{
		purple_buddy_icon_unref(im->icon);

		im->icon = (icon == NULL ? NULL : purple_buddy_icon_ref(icon));
	}

	purple_conversation_update(purple_im_conversation_get_conversation(im),
							 PURPLE_CONVERSATION_UPDATE_ICON);
}

PurpleBuddyIcon *
purple_im_conversation_get_icon(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->icon;
}

void
purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMConversationTypingState state)
{
	g_return_if_fail(im != NULL);

	if (im->typing_state != state)
	{
		im->typing_state = state;

		switch (state)
		{
			case PURPLE_IM_CONVERSATION_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing", im->conv->account, im->conv->name);
				break;
			case PURPLE_IM_CONVERSATION_TYPED:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typed", im->conv->account, im->conv->name);
				break;
			case PURPLE_IM_CONVERSATION_NOT_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing-stopped", im->conv->account, im->conv->name);
				break;
		}

		purple_im_conversation_update_typing(im);
	}
}

PurpleIMConversationTypingState
purple_im_conversation_get_typing_state(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_state;
}

void
purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout)
{
	PurpleConversation *conv;

	g_return_if_fail(im != NULL);

	if (im->typing_timeout > 0)
		purple_im_conversation_stop_typing_timeout(im);

	conv = purple_im_conversation_get_conversation(im);

	im->typing_timeout = purple_timeout_add_seconds(timeout, reset_typing_cb, conv);
}

void
purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	if (im->typing_timeout == 0)
		return;

	purple_timeout_remove(im->typing_timeout);
	im->typing_timeout = 0;
}

guint
purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_timeout;
}

void
purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val)
{
	g_return_if_fail(im != NULL);

	if (val == 0)
		im->type_again = 0;
	else
		im->type_again = time(NULL) + val;
}

time_t
purple_im_conversation_get_type_again(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again;
}

void
purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	im->send_typed_timeout = purple_timeout_add_seconds(SEND_TYPED_TIMEOUT_SECONDS,
	                                                    send_typed_cb,
	                                                    purple_im_conversation_get_conversation(im));
}

void
purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	if (im->send_typed_timeout == 0)
		return;

	purple_timeout_remove(im->send_typed_timeout);
	im->send_typed_timeout = 0;
}

guint
purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->send_typed_timeout;
}

void
purple_im_conversation_update_typing(PurpleIMConversation *im)
{
	g_return_if_fail(im != NULL);

	purple_conversation_update(purple_im_conversation_get_conversation(im),
							 PURPLE_CONVERSATION_UPDATE_TYPING);
}

void
purple_im_conversation_write(PurpleIMConversation *im, const char *who, const char *message,
			  PurpleConversationMessageFlags flags, time_t mtime)
{
	PurpleConversation *c;

	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	c = purple_im_conversation_get_conversation(im);

	if ((flags & PURPLE_CONVERSATION_MESSAGE_RECV) == PURPLE_CONVERSATION_MESSAGE_RECV) {
		purple_im_conversation_set_typing_state(im, PURPLE_IM_CONVERSATION_NOT_TYPING);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, flags, mtime);
	else
		purple_conversation_write(c, who, message, flags, mtime);
}

gboolean purple_conversation_helper_present_error(const char *who, PurpleAccount *account, const char *what)
{
	PurpleConversation *conv;

	g_return_val_if_fail(who != NULL, FALSE);
	g_return_val_if_fail(account !=NULL, FALSE);
	g_return_val_if_fail(what != NULL, FALSE);

	conv = purple_conversations_find_with_account(PURPLE_CONVERSATION_TYPE_ANY, who, account);
	if (conv != NULL)
		purple_conversation_write(conv, NULL, what, PURPLE_CONVERSATION_MESSAGE_ERROR, time(NULL));
	else
		return FALSE;

	return TRUE;
}

void
purple_im_conversation_send(PurpleIMConversation *im, const char *message)
{
	purple_im_conversation_send_with_flags(im, message, 0);
}

static void
purple_conversation_send_confirm_cb(gpointer *data)
{
	PurpleConversation *conv = data[0];
	char *message = data[1];

	g_free(data);
	common_send(conv, message, 0);
}

void
purple_conversation_send_confirm(PurpleConversation *conv, const char *message)
{
	char *text;
	gpointer *data;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(message != NULL);

	if (conv->ui_ops != NULL && conv->ui_ops->send_confirm != NULL)
	{
		conv->ui_ops->send_confirm(conv, message);
		return;
	}

	text = g_strdup_printf("You are about to send the following message:\n%s", message);
	data = g_new0(gpointer, 2);
	data[0] = conv;
	data[1] = (gpointer)message;

	purple_request_action(conv, NULL, _("Send Message"), text, 0,
						  purple_conversation_get_account(conv), NULL, conv,
						  data, 2,
						  _("_Send Message"), G_CALLBACK(purple_conversation_send_confirm_cb),
						  _("Cancel"), NULL);
}

void
purple_im_conversation_send_with_flags(PurpleIMConversation *im, const char *message, PurpleConversationMessageFlags flags)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(purple_im_conversation_get_conversation(im), message, flags);
}

gboolean
purple_conversation_custom_smiley_add(PurpleConversation *conv, const char *smile,
                            const char *cksum_type, const char *chksum,
							gboolean remote)
{
	if (conv == NULL || smile == NULL || !*smile) {
		return FALSE;
	}

	/* TODO: check if the icon is in the cache and return false if so */
	/* TODO: add an icon cache (that doesn't suck) */
	if (conv->ui_ops != NULL && conv->ui_ops->custom_smiley_add !=NULL) {
		return conv->ui_ops->custom_smiley_add(conv, smile, remote);
	} else {
		purple_debug_info("conversation", "Could not find add custom smiley function");
		return FALSE;
	}

}

void
purple_conversation_custom_smiley_write(PurpleConversation *conv, const char *smile,
                                   const guchar *data, gsize size)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (conv->ui_ops != NULL && conv->ui_ops->custom_smiley_write != NULL)
		conv->ui_ops->custom_smiley_write(conv, smile, data, size);
	else
		purple_debug_info("conversation", "Could not find the smiley write function");
}

void
purple_conversation_custom_smiley_close(PurpleConversation *conv, const char *smile)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (conv->ui_ops != NULL && conv->ui_ops->custom_smiley_close != NULL)
		conv->ui_ops->custom_smiley_close(conv, smile);
	else
		purple_debug_info("conversation", "Could not find custom smiley close function");
}


/**************************************************************************
 * Chat Conversation API
 **************************************************************************/

PurpleConversation *
purple_chat_conversation_get_conversation(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->conv;
}

GList *
purple_chat_conversation_get_users(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->in_room;
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
		g_list_append(chat->ignored, g_strdup(name)));
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
		g_list_remove_link(chat->ignored, item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
purple_chat_conversation_set_ignored(PurpleChatConversation *chat, GList *ignored)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->ignored = ignored;

	return ignored;
}

GList *
purple_chat_conversation_get_ignored(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->ignored;
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

	g_free(chat->who);
	g_free(chat->topic);

	chat->who   = g_strdup(who);
	chat->topic = g_strdup(topic);

	purple_conversation_update(purple_chat_conversation_get_conversation(chat),
							 PURPLE_CONVERSATION_UPDATE_TOPIC);

	purple_signal_emit(purple_conversations_get_handle(), "chat-topic-changed",
					 chat->conv, chat->who, chat->topic);
}

const char *
purple_chat_conversation_get_topic(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->topic;
}

void
purple_chat_conversation_set_id(PurpleChatConversation *chat, int id)
{
	g_return_if_fail(chat != NULL);

	chat->id = id;
}

int
purple_chat_conversation_get_id(const PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, -1);

	return chat->id;
}

void
purple_chat_conversation_write(PurpleChatConversation *chat, const char *who, const char *message,
				PurpleConversationMessageFlags flags, time_t mtime)
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

	if (!(flags & PURPLE_CONVERSATION_MESSAGE_WHISPER)) {
		const char *str;

		str = purple_normalize(account, who);

		if (purple_strequal(str, chat->nick)) {
			flags |= PURPLE_CONVERSATION_MESSAGE_SEND;
		} else {
			flags |= PURPLE_CONVERSATION_MESSAGE_RECV;

			if (purple_utf8_has_word(message, chat->nick))
				flags |= PURPLE_CONVERSATION_MESSAGE_NICK;
		}
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (conv->ui_ops != NULL && conv->ui_ops->write_chat != NULL)
		conv->ui_ops->write_chat(conv, who, message, flags, mtime);
	else
		purple_conversation_write(conv, who, message, flags, mtime);
}

void
purple_chat_conversation_send(PurpleChatConversation *chat, const char *message)
{
	purple_chat_conversation_send_with_flags(chat, message, 0);
}

void
purple_chat_conversation_send_with_flags(PurpleChatConversation *chat, const char *message, PurpleConversationMessageFlags flags)
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
			if (purple_strequal(chat->nick, purple_normalize(conv->account, user))) {
				const char *alias2 = purple_account_get_private_alias(conv->account);
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
		cbuddy->buddy = purple_find_buddy(conv->account, user) != NULL;

		chat->in_room = g_list_prepend(chat->in_room, cbuddy);
		g_hash_table_replace(chat->users, g_strdup(cbuddy->name), cbuddy);

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
					PURPLE_CONVERSATION_MESSAGE_SYSTEM | PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY,
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

	if (purple_strequal(chat->nick, purple_normalize(conv->account, old_user))) {
		const char *alias;

		/* Note this for later. */
		is_me = TRUE;

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			alias = purple_account_get_private_alias(conv->account);
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
	cb->buddy = purple_find_buddy(conv->account, new_user) != NULL;

	chat->in_room = g_list_prepend(chat->in_room, cb);
	g_hash_table_replace(chat->users, g_strdup(cb->name), cb);

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user, new_alias);

	cb = purple_chat_conversation_find_buddy(chat, old_user);

	if (cb) {
		chat->in_room = g_list_remove(chat->in_room, cb);
		g_hash_table_remove(chat->users, cb->name);
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
				PURPLE_CONVERSATION_MESSAGE_SYSTEM | PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY,
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
			chat->in_room = g_list_remove(chat->in_room, cb);
			g_hash_table_remove(chat->users, cb->name);
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
					PURPLE_CONVERSATION_MESSAGE_SYSTEM | PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY,
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
	users = chat->in_room;

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

	g_hash_table_remove_all(chat->users);

	g_list_free(users);
	chat->in_room = NULL;
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

void purple_chat_conversation_set_nick(PurpleChatConversation *chat, const char *nick) {
	g_return_if_fail(chat != NULL);

	g_free(chat->nick);
	chat->nick = g_strdup(purple_normalize(chat->conv->account, nick));
}

const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat) {
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->nick;
}

void
purple_chat_conversation_leave(PurpleChatConversation *chat)
{
	g_return_if_fail(chat != NULL);

	chat->left = TRUE;
	purple_conversation_update(chat->conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
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
	account = conv->account;

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
purple_chat_conversation_has_left(PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, TRUE);

	return chat->left;
}

PurpleChatConversationBuddy *
purple_chat_conversation_buddy_new(const char *name, const char *alias, PurpleChatConversationBuddyFlags flags)
{
	PurpleChatConversationBuddy *cb;

	g_return_val_if_fail(name != NULL, NULL);

	cb = g_new0(PurpleChatConversationBuddy, 1);
	cb->name = g_strdup(name);
	cb->flags = flags;
	cb->alias = g_strdup(alias);
	cb->attributes = g_hash_table_new_full(g_str_hash, g_str_equal,
										   g_free, g_free);

	PURPLE_DBUS_REGISTER_POINTER(cb, PurpleChatConversationBuddy);
	return cb;
}

PurpleChatConversationBuddy *
purple_chat_conversation_find_buddy(PurpleChatConversation *chat, const char *name)
{
	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_hash_table_lookup(chat->users, name);
}

void
purple_chat_conversation_buddy_destroy(PurpleChatConversationBuddy *cb)
{
	if (cb == NULL)
		return;

	purple_signal_emit(purple_conversations_get_handle(),
			"deleting-chat-buddy", cb);

	g_free(cb->alias);
	g_free(cb->alias_key);
	g_free(cb->name);
	g_hash_table_destroy(cb->attributes);

	PURPLE_DBUS_UNREGISTER_POINTER(cb);
	g_free(cb);
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

gboolean purple_chat_conversation_buddy_is_buddy(const PurpleChatConversationBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, FALSE);

	return cb->buddy;
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

GList *
purple_conversation_get_extended_menu(PurpleConversation *conv)
{
	GList *menu = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	purple_signal_emit(purple_conversations_get_handle(),
			"conversation-extended-menu", conv, &menu);
	return menu;
}

void purple_conversation_clear_message_history(PurpleConversation *conv)
{
	GList *list = conv->message_history;
	message_history_free(list);
	conv->message_history = NULL;

	purple_signal_emit(purple_conversations_get_handle(),
			"cleared-message-history", conv);
}

GList *purple_conversation_get_message_history(PurpleConversation *conv)
{
	return conv->message_history;
}

const char *purple_conversation_message_get_sender(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, NULL);
	return msg->who;
}

const char *purple_conversation_message_get_message(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, NULL);
	return msg->what;
}

PurpleConversationMessageFlags purple_conversation_message_get_flags(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, 0);
	return msg->flags;
}

time_t purple_conversation_message_get_timestamp(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, 0);
	return msg->when;
}

const char *purple_conversation_message_get_alias(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, NULL);
	return msg->alias;
}

PurpleConversation *purple_conversation_message_get_conv(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, NULL);
	return msg->conv;
}

void purple_conversation_set_ui_data(PurpleConversation *conv, gpointer ui_data)
{
	g_return_if_fail(conv != NULL);

	conv->ui_data = ui_data;
}

gpointer purple_conversation_get_ui_data(const PurpleConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->ui_data;
}


gboolean
purple_conversation_do_command(PurpleConversation *conv, const gchar *cmdline,
				const gchar *markup, gchar **error)
{
	char *mark = (markup && *markup) ? NULL : g_markup_escape_text(cmdline, -1), *err = NULL;
	PurpleCmdStatus status = purple_cmd_do_command(conv, cmdline, mark ? mark : markup, error ? error : &err);
	g_free(mark);
	g_free(err);
	return (status == PURPLE_CMD_STATUS_OK);
}
