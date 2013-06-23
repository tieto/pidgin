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

/** @copydoc _PurpleChatConversationPrivate */
typedef struct _PurpleChatConversationPrivate     PurpleChatConversationPrivate;
/** @copydoc _PurpleIMConversationPrivate */
typedef struct _PurpleIMConversationPrivate       PurpleIMConversationPrivate;
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

void
purple_im_conversation_send(PurpleIMConversation *im, const char *message)
{
	purple_im_conversation_send_with_flags(im, message, 0);
}

void
purple_im_conversation_send_with_flags(PurpleIMConversation *im, const char *message, PurpleMessageFlags flags)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(purple_im_conversation_get_conversation(im), message, flags);
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

		if (purple_strequal(str, chat->nick)) {
			flags |= PURPLE_MESSAGE_SEND;
		} else {
			flags |= PURPLE_MESSAGE_RECV;

			if (purple_utf8_has_word(message, chat->nick))
				flags |= PURPLE_MESSAGE_NICK;
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
purple_chat_conversation_send_with_flags(PurpleChatConversation *chat, const char *message, PurpleMessageFlags flags)
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

void
purple_chat_conversation_leave(PurpleChatConversation *chat)
{
	g_return_if_fail(chat != NULL);

	chat->left = TRUE;
	purple_conversation_update(chat->conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

gboolean
purple_chat_conversation_has_left(PurpleChatConversation *chat)
{
	g_return_val_if_fail(chat != NULL, TRUE);

	return chat->left;
}

PurpleChatConversationBuddy *
purple_chat_conversation_find_buddy(PurpleChatConversation *chat, const char *name)
{
	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_hash_table_lookup(chat->users, name);
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
