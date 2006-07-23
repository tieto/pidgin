/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"
#include "blist.h"
#include "conversation.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "signals.h"
#include "util.h"

#define SEND_TYPED_TIMEOUT 5000

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static GaimConversationUiOps *default_ops = NULL;

void
gaim_conversations_set_ui_ops(GaimConversationUiOps *ops)
{
	default_ops = ops;
}

static gboolean
reset_typing_cb(gpointer data)
{
	GaimConversation *c = (GaimConversation *)data;
	GaimConvIm *im;

	im = GAIM_CONV_IM(c);

	gaim_conv_im_set_typing_state(im, GAIM_NOT_TYPING);
	gaim_conv_im_update_typing(im);
	gaim_conv_im_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed_cb(gpointer data)
{
	GaimConversation *conv = (GaimConversation *)data;
	GaimConnection *gc;
	const char *name;

	g_return_val_if_fail(conv != NULL, FALSE);

	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (gc != NULL && name != NULL) {
		/* We set this to 1 so that GAIM_TYPING will be sent
		 * if the Gaim user types anything else.
		 */
		gaim_conv_im_set_type_again(GAIM_CONV_IM(conv), 1);

		serv_send_typing(gc, name, GAIM_TYPED);
		gaim_signal_emit(gaim_conversations_get_handle(),
						 "buddy-typed", conv->account, conv->name);

		gaim_debug(GAIM_DEBUG_MISC, "conversation", "typed...\n");
	}

	return FALSE;
}

static void
common_send(GaimConversation *conv, const char *message, GaimMessageFlags msgflags)
{
	GaimConversationType type;
	GaimAccount *account;
	GaimConnection *gc;
	char *displayed = NULL, *sent = NULL;
	int err = 0;

	if (strlen(message) == 0)
		return;

	account = gaim_conversation_get_account(conv);
	gc = gaim_conversation_get_gc(conv);

	g_return_if_fail(account != NULL);
	g_return_if_fail(gc != NULL);

	type = gaim_conversation_get_type(conv);

	/* Always linkfy the text for display */
	displayed = gaim_markup_linkify(message);

	if ((conv->features & GAIM_CONNECTION_HTML) &&
		!(msgflags & GAIM_MESSAGE_RAW))
	{
		sent = g_strdup(displayed);
	}
	else
		sent = g_strdup(message);

	msgflags |= GAIM_MESSAGE_SEND;

	if (type == GAIM_CONV_TYPE_IM) {
		GaimConvIm *im = GAIM_CONV_IM(conv);

		gaim_signal_emit(gaim_conversations_get_handle(), "sending-im-msg",
						 account,
						 gaim_conversation_get_name(conv), &sent);

		if (sent != NULL && sent[0] != '\0') {

			err = serv_send_im(gc, gaim_conversation_get_name(conv),
			                   sent, msgflags);

			if ((err > 0) && (displayed != NULL))
				gaim_conv_im_write(im, NULL, displayed, msgflags, time(NULL));

			gaim_signal_emit(gaim_conversations_get_handle(), "sent-im-msg",
							 account,
							 gaim_conversation_get_name(conv), sent);
		}
	}
	else {
		gaim_signal_emit(gaim_conversations_get_handle(), "sending-chat-msg",
						 account, &sent,
						 gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));

		if (sent != NULL && sent[0] != '\0') {
			err = serv_chat_send(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)), sent, msgflags);

			gaim_signal_emit(gaim_conversations_get_handle(), "sent-chat-msg",
							 account, sent,
							 gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));
		}
	}

	if (err < 0) {
		const char *who;
		const char *msg;

		who = gaim_conversation_get_name(conv);

		if (err == -E2BIG) {
			msg = _("Unable to send message: The message is too large.");

			if (!gaim_conv_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				gaim_notify_error(gc, NULL, msg2, _("The message is too large."));
				g_free(msg2);
			}
		}
		else if (err == -ENOTCONN) {
			gaim_debug(GAIM_DEBUG_ERROR, "conversation",
					   "Not yet connected.\n");
		}
		else {
			msg = _("Unable to send message.");

			if (!gaim_conv_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				gaim_notify_error(gc, NULL, msg2, NULL);
				g_free(msg2);
			}
		}
	}

	g_free(displayed);
	g_free(sent);
}

static void
open_log(GaimConversation *conv)
{
	conv->logs = g_list_append(NULL, gaim_log_new(conv->type == GAIM_CONV_TYPE_CHAT ? GAIM_LOG_CHAT :
							   GAIM_LOG_IM, conv->name, conv->account,
							   conv, time(NULL), NULL));
}


/**************************************************************************
 * Conversation API
 **************************************************************************/
static void
gaim_conversation_chat_cleanup_for_rejoin(GaimConversation *conv)
{
	const char *disp;
	GaimAccount *account;
	GaimConnection *gc;

	account = gaim_conversation_get_account(conv);

	gaim_conversation_close_logs(conv);
	open_log(conv);

	gc = gaim_account_get_connection(account);

	if ((disp = gaim_connection_get_display_name(gc)) != NULL)
		gaim_conv_chat_set_nick(GAIM_CONV_CHAT(conv), disp);
	else
	{
		gaim_conv_chat_set_nick(GAIM_CONV_CHAT(conv),
								gaim_account_get_username(account));
	}

	gaim_conv_chat_clear_users(GAIM_CONV_CHAT(conv));
	gaim_conv_chat_set_topic(GAIM_CONV_CHAT(conv), NULL, NULL);
	GAIM_CONV_CHAT(conv)->left = FALSE;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_CHATLEFT);
}

GaimConversation *
gaim_conversation_new(GaimConversationType type, GaimAccount *account,
					  const char *name)
{
	GaimConversation *conv;
	GaimConnection *gc;
	GaimConversationUiOps *ops;

	g_return_val_if_fail(type    != GAIM_CONV_TYPE_UNKNOWN, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((conv = gaim_find_conversation_with_account(type, name, account)) != NULL)
	{
		if (gaim_conversation_get_type(conv) != GAIM_CONV_TYPE_CHAT ||
		    gaim_conv_chat_has_left(GAIM_CONV_CHAT(conv)))
		{
			if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT)
				gaim_conversation_chat_cleanup_for_rejoin(conv);

			return conv;
		}
	}

	gc = gaim_account_get_connection(account);
	g_return_val_if_fail(gc != NULL, NULL);

	conv = g_new0(GaimConversation, 1);
	GAIM_DBUS_REGISTER_POINTER(conv, GaimConversation);

	conv->type         = type;
	conv->account      = account;
	conv->name         = g_strdup(name);
	conv->title        = g_strdup(name);
	conv->data         = g_hash_table_new_full(g_str_hash, g_str_equal,
											   g_free, NULL);
	/* copy features from the connection. */
	conv->features = gc->flags;

	if (type == GAIM_CONV_TYPE_IM)
	{
		GaimBuddyIcon *icon;
		conv->u.im = g_new0(GaimConvIm, 1);
		conv->u.im->conv = conv;
		GAIM_DBUS_REGISTER_POINTER(conv->u.im, GaimConvIm);

		ims = g_list_append(ims, conv);
		if ((icon = gaim_buddy_icons_find(account, name)))
			gaim_conv_im_set_icon(conv->u.im, icon);

		if (gaim_prefs_get_bool("/core/logging/log_ims"))
		{
			gaim_conversation_set_logging(conv, TRUE);
			open_log(conv);
		}
	}
	else if (type == GAIM_CONV_TYPE_CHAT)
	{
		const char *disp;

		conv->u.chat = g_new0(GaimConvChat, 1);
		conv->u.chat->conv = conv;
		GAIM_DBUS_REGISTER_POINTER(conv->u.chat, GaimConvChat);

		chats = g_list_append(chats, conv);

		if ((disp = gaim_connection_get_display_name(account->gc)))
			gaim_conv_chat_set_nick(conv->u.chat, disp);
		else
			gaim_conv_chat_set_nick(conv->u.chat,
									gaim_account_get_username(account));

		if (gaim_prefs_get_bool("/core/logging/log_chats"))
		{
			gaim_conversation_set_logging(conv, TRUE);
			open_log(conv);
		}
	}

	conversations = g_list_append(conversations, conv);

	/* Auto-set the title. */
	gaim_conversation_autoset_title(conv);

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = conv->ui_ops = default_ops;
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(conv);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "conversation-created", conv);

	return conv;
}

void
gaim_conversation_destroy(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConversationUiOps *ops;
	GaimConnection *gc;
	const char *name;
	GList *node;

	g_return_if_fail(conv != NULL);

	ops  = gaim_conversation_get_ui_ops(conv);
	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM)
		{
			if (gaim_prefs_get_bool("/core/conversations/im/send_typing"))
				serv_send_typing(gc, name, GAIM_NOT_TYPING);

			if (gc && prpl_info->convo_closed != NULL)
				prpl_info->convo_closed(gc, name);
		}
		else if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT)
		{
			int chat_id = gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv));
#if 0
			/*
			 * This is unfortunately necessary, because calling
			 * serv_chat_leave() calls this gaim_conversation_destroy(),
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
			if (!gaim_conv_chat_has_left(GAIM_CONV_CHAT(conv)))
				serv_chat_leave(gc, chat_id);

			/*
			 * If they didn't call serv_got_chat_left by now, it's too late.
			 * So we better do it for them before we destroy the thing.
			 */
			if (!gaim_conv_chat_has_left(GAIM_CONV_CHAT(conv)))
				serv_got_chat_left(gc, chat_id);
		}
	}

	/* remove from conversations and im/chats lists prior to emit */
	conversations = g_list_remove(conversations, conv);

	if(conv->type==GAIM_CONV_TYPE_IM)
		ims = g_list_remove(ims, conv);
	else if(conv->type==GAIM_CONV_TYPE_CHAT)
		chats = g_list_remove(chats, conv);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "deleting-conversation", conv);

	g_free(conv->name);
	g_free(conv->title);

	conv->name = NULL;
	conv->title = NULL;

	if (conv->type == GAIM_CONV_TYPE_IM) {
		gaim_conv_im_stop_typing_timeout(conv->u.im);
		gaim_conv_im_stop_send_typed_timeout(conv->u.im);

		if (conv->u.im->icon != NULL)
			gaim_buddy_icon_unref(conv->u.im->icon);
		conv->u.im->icon = NULL;

		GAIM_DBUS_UNREGISTER_POINTER(conv->u.im);
		g_free(conv->u.im);
		conv->u.im = NULL;
	}
	else if (conv->type == GAIM_CONV_TYPE_CHAT) {

		for (node = conv->u.chat->in_room; node != NULL; node = node->next) {
			if (node->data != NULL)
				gaim_conv_chat_cb_destroy((GaimConvChatBuddy *)node->data);
			node->data = NULL;
		}

		for (node = conv->u.chat->ignored; node != NULL; node = node->next) {
			if (node->data != NULL)
				g_free(node->data);
			node->data = NULL;
		}

		g_list_free(conv->u.chat->in_room);
		g_list_free(conv->u.chat->ignored);

		conv->u.chat->in_room = NULL;
		conv->u.chat->ignored = NULL;

		if (conv->u.chat->who != NULL)
			g_free(conv->u.chat->who);
		conv->u.chat->who = NULL;

		if (conv->u.chat->topic != NULL)
			g_free(conv->u.chat->topic);
		conv->u.chat->topic = NULL;

		if(conv->u.chat->nick)
			g_free(conv->u.chat->nick);

		GAIM_DBUS_UNREGISTER_POINTER(conv->u.chat);
		g_free(conv->u.chat);
		conv->u.chat = NULL;
	}

	g_hash_table_destroy(conv->data);
	conv->data = NULL;

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

	gaim_conversation_close_logs(conv);

	GAIM_DBUS_UNREGISTER_POINTER(conv);
	g_free(conv);
	conv = NULL;
}


void
gaim_conversation_present(GaimConversation *conv) {
	GaimConversationUiOps *ops;

	g_return_if_fail(conv != NULL);

	ops = gaim_conversation_get_ui_ops(conv);
	if(ops && ops->present)
		ops->present(conv);
}


void
gaim_conversation_set_features(GaimConversation *conv, GaimConnectionFlags features)
{
	g_return_if_fail(conv != NULL);

	conv->features = features;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_FEATURES);
}


GaimConnectionFlags
gaim_conversation_get_features(GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, 0);
	return conv->features;
}


GaimConversationType
gaim_conversation_get_type(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, GAIM_CONV_TYPE_UNKNOWN);

	return conv->type;
}

void
gaim_conversation_set_ui_ops(GaimConversation *conv,
							 GaimConversationUiOps *ops)
{
	g_return_if_fail(conv != NULL);

	if (conv->ui_ops == ops)
		return;

	if (conv->ui_ops != NULL && conv->ui_ops->destroy_conversation != NULL)
		conv->ui_ops->destroy_conversation(conv);

	conv->ui_data = NULL;

	conv->ui_ops = ops;
}

GaimConversationUiOps *
gaim_conversation_get_ui_ops(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->ui_ops;
}

void
gaim_conversation_set_account(GaimConversation *conv, GaimAccount *account)
{
	g_return_if_fail(conv != NULL);

	if (account == gaim_conversation_get_account(conv))
		return;

	conv->account = account;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_ACCOUNT);
}

GaimAccount *
gaim_conversation_get_account(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->account;
}

GaimConnection *
gaim_conversation_get_gc(const GaimConversation *conv)
{
	GaimAccount *account;

	g_return_val_if_fail(conv != NULL, NULL);

	account = gaim_conversation_get_account(conv);

	if (account == NULL)
		return NULL;

	return account->gc;
}

void
gaim_conversation_set_title(GaimConversation *conv, const char *title)
{
	g_return_if_fail(conv  != NULL);
	g_return_if_fail(title != NULL);

	if (conv->title != NULL)
		g_free(conv->title);

	conv->title = g_strdup(title);

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_TITLE);
}

const char *
gaim_conversation_get_title(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->title;
}

void
gaim_conversation_autoset_title(GaimConversation *conv)
{
	GaimAccount *account;
	GaimBuddy *b;
	GaimChat *chat;
	const char *text = NULL, *name;

	g_return_if_fail(conv != NULL);

	account = gaim_conversation_get_account(conv);
	name = gaim_conversation_get_name(conv);

	if(gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM) {
		if(account && ((b = gaim_find_buddy(account, name)) != NULL))
			text = gaim_buddy_get_contact_alias(b);
	} else if(gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT) {
		if(account && ((chat = gaim_blist_find_chat(account, name)) != NULL))
			text = chat->alias;
	}


	if(text == NULL)
		text = name;

	gaim_conversation_set_title(conv, text);
}

void
gaim_conversation_foreach(void (*func)(GaimConversation *conv))
{
	GaimConversation *conv;
	GList *l;

	g_return_if_fail(func != NULL);

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		func(conv);
	}
}

void
gaim_conversation_set_name(GaimConversation *conv, const char *name)
{
	g_return_if_fail(conv != NULL);

	if (conv->name != NULL)
		g_free(conv->name);

	conv->name = (name == NULL ? NULL : g_strdup(name));

	gaim_conversation_autoset_title(conv);
}

const char *
gaim_conversation_get_name(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->name;
}

void
gaim_conversation_set_logging(GaimConversation *conv, gboolean log)
{
	g_return_if_fail(conv != NULL);

	if (conv->logging != log)
	{
		conv->logging = log;
		gaim_conversation_update(conv, GAIM_CONV_UPDATE_LOGGING);
	}
}

gboolean
gaim_conversation_is_logging(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, FALSE);

	return conv->logging;
}

void
gaim_conversation_close_logs(GaimConversation *conv)
{
	g_return_if_fail(conv != NULL);

	g_list_foreach(conv->logs, (GFunc)gaim_log_free, NULL);
	g_list_free(conv->logs);
	conv->logs = NULL;
}

GaimConvIm *
gaim_conversation_get_im_data(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	if (gaim_conversation_get_type(conv) != GAIM_CONV_TYPE_IM)
		return NULL;

	return conv->u.im;
}

GaimConvChat *
gaim_conversation_get_chat_data(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	if (gaim_conversation_get_type(conv) != GAIM_CONV_TYPE_CHAT)
		return NULL;

	return conv->u.chat;
}

void
gaim_conversation_set_data(GaimConversation *conv, const char *key,
						   gpointer data)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(key  != NULL);

	g_hash_table_replace(conv->data, g_strdup(key), data);
}

gpointer
gaim_conversation_get_data(GaimConversation *conv, const char *key)
{
	g_return_val_if_fail(conv != NULL, NULL);
	g_return_val_if_fail(key  != NULL, NULL);

	return g_hash_table_lookup(conv->data, key);
}

GList *
gaim_get_conversations(void)
{
	return conversations;
}

GList *
gaim_get_ims(void)
{
	return ims;
}

GList *
gaim_get_chats(void)
{
	return chats;
}


GaimConversation *
gaim_find_conversation_with_account(GaimConversationType type,
									const char *name,
									const GaimAccount *account)
{
	GaimConversation *c = NULL;
	gchar *name1;
	const gchar *name2;
	GList *cnv;

	g_return_val_if_fail(name != NULL, NULL);

	name1 = g_strdup(gaim_normalize(account, name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (GaimConversation *)cnv->data;
		name2 = gaim_normalize(account, gaim_conversation_get_name(c));

		if (((type == GAIM_CONV_TYPE_ANY) || (type == gaim_conversation_get_type(c))) &&
				(account == gaim_conversation_get_account(c)) &&
				!gaim_utf8_strcasecmp(name1, name2)) {

			break;
		}

		c = NULL;
	}

	g_free(name1);

	return c;
}

void
gaim_conversation_write(GaimConversation *conv, const char *who,
						const char *message, GaimMessageFlags flags,
						time_t mtime)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = NULL;
	GaimAccount *account;
	GaimConversationUiOps *ops;
	const char *alias;
	char *displayed = NULL;
	GaimBuddy *b;
	int plugin_return;
	GaimConversationType type;
	/* int logging_font_options = 0; */

	g_return_if_fail(conv    != NULL);
	g_return_if_fail(message != NULL);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops == NULL || ops->write_conv == NULL)
		return;

	account = gaim_conversation_get_account(conv);
	type = gaim_conversation_get_type(conv);

	if (account != NULL)
		gc = gaim_account_get_connection(account);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT &&
		(gc == NULL || !g_slist_find(gc->buddy_chats, conv)))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM &&
		!g_list_find(gaim_get_conversations(), conv))
		return;

	displayed = g_strdup(message);

	plugin_return =
		GPOINTER_TO_INT(gaim_signal_emit_return_1(
			gaim_conversations_get_handle(),
			(type == GAIM_CONV_TYPE_IM ? "writing-im-msg" : "writing-chat-msg"),
			account, who, &displayed, conv, flags));

	if (displayed == NULL)
		return;

	if (plugin_return) {
		g_free(displayed);
		return;
	}

	if (who == NULL || *who == '\0')
		who = gaim_conversation_get_name(conv);

	alias = who;

	if (account != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gaim_find_prpl(gaim_account_get_protocol_id(account)));

		if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM ||
			!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (flags & GAIM_MESSAGE_SEND) {
				b = gaim_find_buddy(account,
							gaim_account_get_username(account));

				if (gaim_account_get_alias(account) != NULL)
					alias = account->alias;
				else if (b != NULL && strcmp(b->name, gaim_buddy_get_contact_alias(b)))
					alias = gaim_buddy_get_contact_alias(b);
				else if (gaim_connection_get_display_name(gc) != NULL)
					alias = gaim_connection_get_display_name(gc);
				else
					alias = gaim_account_get_username(account);
			}
			else
			{
				b = gaim_find_buddy(account, who);

				if (b != NULL)
					alias = gaim_buddy_get_contact_alias(b);
			}
		}
	}

	if (!(flags & GAIM_MESSAGE_NO_LOG) && gaim_conversation_is_logging(conv)) {
		GList *log;

		if (conv->logs == NULL)
			open_log(conv);

		log = conv->logs;
		while (log != NULL) {
			gaim_log_write((GaimLog *)log->data, flags, alias, mtime, displayed);
			log = log->next;
		}
	}

	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM) {
		if ((flags & GAIM_MESSAGE_RECV) == GAIM_MESSAGE_RECV) {
			gaim_conv_im_set_typing_state(GAIM_CONV_IM(conv), GAIM_NOT_TYPING);
		}
	}

	ops->write_conv(conv, who, alias, displayed, flags, mtime);

	gaim_signal_emit(gaim_conversations_get_handle(),
		(type == GAIM_CONV_TYPE_IM ? "wrote-im-msg" : "wrote-chat-msg"),
		account, who, displayed, conv, flags);

	g_free(displayed);
}

gboolean
gaim_conversation_has_focus(GaimConversation *conv)
{
	gboolean ret = FALSE;
	GaimConversationUiOps *ops;

	g_return_val_if_fail(conv != NULL, FALSE);

	ops = gaim_conversation_get_ui_ops(conv);

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
gaim_conversation_update(GaimConversation *conv, GaimConvUpdateType type)
{
	g_return_if_fail(conv != NULL);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "conversation-updated", conv, type);
}

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
GaimConversation *
gaim_conv_im_get_conversation(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->conv;
}

void
gaim_conv_im_set_icon(GaimConvIm *im, GaimBuddyIcon *icon)
{
	g_return_if_fail(im != NULL);

	if (im->icon != icon)
	{
		if (im->icon != NULL)
			gaim_buddy_icon_unref(im->icon);

		im->icon = (icon == NULL ? NULL : gaim_buddy_icon_ref(icon));
	}

	gaim_conversation_update(gaim_conv_im_get_conversation(im),
							 GAIM_CONV_UPDATE_ICON);
}

GaimBuddyIcon *
gaim_conv_im_get_icon(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->icon;
}

void
gaim_conv_im_set_typing_state(GaimConvIm *im, GaimTypingState state)
{
	g_return_if_fail(im != NULL);

	if (im->typing_state != state)
	{
		im->typing_state = state;

		if (state == GAIM_TYPING)
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typing", im->conv->account, im->conv->name);
		}
		else if (state == GAIM_TYPED)
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typed", im->conv->account, im->conv->name);
		}
		else if (state == GAIM_NOT_TYPING)
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typing-stopped", im->conv->account, im->conv->name);
		}
	}
}

GaimTypingState
gaim_conv_im_get_typing_state(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_state;
}

void
gaim_conv_im_start_typing_timeout(GaimConvIm *im, int timeout)
{
	GaimConversation *conv;
	const char *name;

	g_return_if_fail(im != NULL);

	if (im->typing_timeout > 0)
		gaim_conv_im_stop_typing_timeout(im);

	conv = gaim_conv_im_get_conversation(im);
	name = gaim_conversation_get_name(conv);

	im->typing_timeout = gaim_timeout_add(timeout * 1000, reset_typing_cb, conv);
}

void
gaim_conv_im_stop_typing_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->typing_timeout == 0)
		return;

	gaim_timeout_remove(im->typing_timeout);
	im->typing_timeout = 0;
}

guint
gaim_conv_im_get_typing_timeout(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_timeout;
}

void
gaim_conv_im_set_type_again(GaimConvIm *im, unsigned int val)
{
	g_return_if_fail(im != NULL);

	if (val == 0)
		im->type_again = 0;
	else
		im->type_again = time(NULL) + val;
}

time_t
gaim_conv_im_get_type_again(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again;
}

void
gaim_conv_im_start_send_typed_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	im->send_typed_timeout = gaim_timeout_add(SEND_TYPED_TIMEOUT, send_typed_cb,
											  gaim_conv_im_get_conversation(im));
}

void
gaim_conv_im_stop_send_typed_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->send_typed_timeout == 0)
		return;

	gaim_timeout_remove(im->send_typed_timeout);
	im->send_typed_timeout = 0;
}

guint
gaim_conv_im_get_send_typed_timeout(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->send_typed_timeout;
}

void
gaim_conv_im_update_typing(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	gaim_conversation_update(gaim_conv_im_get_conversation(im),
							 GAIM_CONV_UPDATE_TYPING);
}

void
gaim_conv_im_write(GaimConvIm *im, const char *who, const char *message,
			  GaimMessageFlags flags, time_t mtime)
{
	GaimConversation *c;

	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	c = gaim_conv_im_get_conversation(im);

	/* Raise the window, if specified in prefs. */
	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, flags, mtime);
	else
		gaim_conversation_write(c, who, message, flags, mtime);
}

gboolean gaim_conv_present_error(const char *who, GaimAccount *account, const char *what)
{
	GaimConversation *conv;

	g_return_val_if_fail(who != NULL, FALSE);
	g_return_val_if_fail(account !=NULL, FALSE);
	g_return_val_if_fail(what != NULL, FALSE);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY, who, account);
	if (conv != NULL)
		gaim_conversation_write(conv, NULL, what, GAIM_MESSAGE_ERROR, time(NULL));
	else
		return FALSE;

	return TRUE;
}

void
gaim_conv_im_send(GaimConvIm *im, const char *message)
{
	gaim_conv_im_send_with_flags(im, message, 0);
}

void
gaim_conv_im_send_with_flags(GaimConvIm *im, const char *message, GaimMessageFlags flags)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_conv_im_get_conversation(im), message, flags);
}

gboolean
gaim_conv_custom_smiley_add(GaimConversation *conv, const char *smile,
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
		gaim_debug_info("conversation", "Could not find add custom smiley function");
		return FALSE;
	}

}

void
gaim_conv_custom_smiley_write(GaimConversation *conv, const char *smile,
                                   const guchar *data, gsize size)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (conv->ui_ops != NULL && conv->ui_ops->custom_smiley_write != NULL)
		conv->ui_ops->custom_smiley_write(conv, smile, data, size);
	else
		gaim_debug_info("conversation", "Could not find the smiley write function");
}

void
gaim_conv_custom_smiley_close(GaimConversation *conv, const char *smile)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (conv->ui_ops != NULL && conv->ui_ops->custom_smiley_close != NULL)
		conv->ui_ops->custom_smiley_close(conv, smile);
	else
		gaim_debug_info("conversation", "Could not find custom smiley close function");
}


/**************************************************************************
 * Chat Conversation API
 **************************************************************************/

GaimConversation *
gaim_conv_chat_get_conversation(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->conv;
}

GList *
gaim_conv_chat_set_users(GaimConvChat *chat, GList *users)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->in_room = users;

	return users;
}

GList *
gaim_conv_chat_get_users(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->in_room;
}

void
gaim_conv_chat_ignore(GaimConvChat *chat, const char *name)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user isn't already ignored. */
	if (gaim_conv_chat_is_user_ignored(chat, name))
		return;

	gaim_conv_chat_set_ignored(chat,
		g_list_append(gaim_conv_chat_get_ignored(chat), g_strdup(name)));
}

void
gaim_conv_chat_unignore(GaimConvChat *chat, const char *name)
{
	GList *item;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user is actually ignored. */
	if (!gaim_conv_chat_is_user_ignored(chat, name))
		return;

	item = g_list_find(gaim_conv_chat_get_ignored(chat),
					   gaim_conv_chat_get_ignored_user(chat, name));

	gaim_conv_chat_set_ignored(chat,
		g_list_remove_link(gaim_conv_chat_get_ignored(chat), item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
gaim_conv_chat_set_ignored(GaimConvChat *chat, GList *ignored)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->ignored = ignored;

	return ignored;
}

GList *
gaim_conv_chat_get_ignored(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->ignored;
}

const char *
gaim_conv_chat_get_ignored_user(const GaimConvChat *chat, const char *user)
{
	GList *ignored;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(user != NULL, NULL);

	for (ignored = gaim_conv_chat_get_ignored(chat);
		 ignored != NULL;
		 ignored = ignored->next) {

		const char *ign = (const char *)ignored->data;

		if (!gaim_utf8_strcasecmp(user, ign) ||
			((*ign == '+' || *ign == '%') && !gaim_utf8_strcasecmp(user, ign + 1)))
			return ign;

		if (*ign == '@') {
			ign++;

			if ((*ign == '+' && !gaim_utf8_strcasecmp(user, ign + 1)) ||
				(*ign != '+' && !gaim_utf8_strcasecmp(user, ign)))
				return ign;
		}
	}

	return NULL;
}

gboolean
gaim_conv_chat_is_user_ignored(const GaimConvChat *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (gaim_conv_chat_get_ignored_user(chat, user) != NULL);
}

void
gaim_conv_chat_set_topic(GaimConvChat *chat, const char *who, const char *topic)
{
	g_return_if_fail(chat != NULL);

	if (chat->who   != NULL) g_free(chat->who);
	if (chat->topic != NULL) g_free(chat->topic);

	chat->who   = (who   == NULL ? NULL : g_strdup(who));
	chat->topic = (topic == NULL ? NULL : g_strdup(topic));

	gaim_conversation_update(gaim_conv_chat_get_conversation(chat),
							 GAIM_CONV_UPDATE_TOPIC);

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-topic-changed",
					 chat->conv, chat->who, chat->topic);
}

const char *
gaim_conv_chat_get_topic(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->topic;
}

void
gaim_conv_chat_set_id(GaimConvChat *chat, int id)
{
	g_return_if_fail(chat != NULL);

	chat->id = id;
}

int
gaim_conv_chat_get_id(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, -1);

	return chat->id;
}

void
gaim_conv_chat_write(GaimConvChat *chat, const char *who, const char *message,
				GaimMessageFlags flags, time_t mtime)
{
	GaimAccount *account;
	GaimConversation *conv;
	GaimConnection *gc;
	GaimPluginProtocolInfo *prpl_info;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	conv      = gaim_conv_chat_get_conversation(chat);
	gc        = gaim_conversation_get_gc(conv);
	account   = gaim_connection_get_account(gc);
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	/* Don't display this if the person who wrote it is ignored. */
	if (gaim_conv_chat_is_user_ignored(chat, who))
		return;

	if (!(flags & GAIM_MESSAGE_WHISPER)) {
		char *str;

		str = g_strdup(gaim_normalize(account, who));

		if (!strcmp(str, gaim_normalize(account, chat->nick))) {
			flags |= GAIM_MESSAGE_SEND;
		} else {
			flags |= GAIM_MESSAGE_RECV;

			if (gaim_utf8_has_word(message, chat->nick))
				flags |= GAIM_MESSAGE_NICK;
		}

		g_free(str);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (conv->ui_ops != NULL && conv->ui_ops->write_chat != NULL)
		conv->ui_ops->write_chat(conv, who, message, flags, mtime);
	else
		gaim_conversation_write(conv, who, message, flags, mtime);
}

void
gaim_conv_chat_send(GaimConvChat *chat, const char *message)
{
	gaim_conv_chat_send_with_flags(chat, message, 0);
}

void
gaim_conv_chat_send_with_flags(GaimConvChat *chat, const char *message, GaimMessageFlags flags)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_conv_chat_get_conversation(chat), message, flags);
}

void
gaim_conv_chat_add_user(GaimConvChat *chat, const char *user,
						const char *extra_msg, GaimConvChatBuddyFlags flags,
						gboolean new_arrival)
{
	GList *users = g_list_append(NULL, (char *)user);
	GList *extra_msgs = g_list_append(NULL, (char *)extra_msg);
	GList *flags2 = g_list_append(NULL, GINT_TO_POINTER(flags));

	gaim_conv_chat_add_users(chat, users, extra_msgs, flags2, new_arrival);

	g_list_free(users);
	g_list_free(extra_msgs);
	g_list_free(flags2);
}

static int
gaim_conv_chat_cb_compare(GaimConvChatBuddy *a, GaimConvChatBuddy *b)
{
	GaimConvChatBuddyFlags f1 = 0, f2 = 0;
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
		ret = strcasecmp(user1, user2);
	}

	return ret;
}

void
gaim_conv_chat_add_users(GaimConvChat *chat, GList *users, GList *extra_msgs,
						 GList *flags, gboolean new_arrivals)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GaimConvChatBuddy *cbuddy;
	GaimConnection *gc;
	GaimPluginProtocolInfo *prpl_info;
	GList *ul, *fl;
	GList *cbuddies = NULL;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gc = gaim_conversation_get_gc(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	g_return_if_fail(prpl_info != NULL);

	ul = users;
	fl = flags;
	while ((ul != NULL) && (fl != NULL)) {
		const char *user = (const char *)ul->data;
		const char *alias = user;
		gboolean quiet;
		GaimConvChatBuddyFlags flag = GPOINTER_TO_INT(fl->data);
		const char *extra_msg = (extra_msgs ? extra_msgs->data : NULL);

		if (!strcmp(chat->nick, gaim_normalize(conv->account, user))) {
			const char *alias2 = gaim_account_get_alias(conv->account);
			if (alias2 != NULL)
				alias = alias2;
			else
			{
				const char *display_name = gaim_connection_get_display_name(gc);
				if (display_name != NULL)
					alias = display_name;
			}
		} else if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			GaimBuddy *buddy;
			if ((buddy = gaim_find_buddy(gc->account, user)) != NULL)
				alias = gaim_buddy_get_contact_alias(buddy);
		}

		quiet = GPOINTER_TO_INT(gaim_signal_emit_return_1(gaim_conversations_get_handle(),
						 "chat-buddy-joining", conv, user, flag)) |
				gaim_conv_chat_is_user_ignored(chat, user);

		cbuddy = gaim_conv_chat_cb_new(user, alias, flag);
		/* This seems dumb. Why should we set users thousands of times? */
		gaim_conv_chat_set_users(chat,
				g_list_prepend(gaim_conv_chat_get_users(chat), cbuddy));

		cbuddies = g_list_prepend(cbuddies, cbuddy);

		if (!quiet && new_arrivals) {
			char *escaped = g_markup_escape_text(alias, -1);
			char *tmp;

			if (extra_msg == NULL)
				tmp = g_strdup_printf(_("%s entered the room."), escaped);
			else {
				char *escaped2 = g_markup_escape_text(extra_msg, -1);
				tmp = g_strdup_printf(_("%s [<I>%s</I>] entered the room."),
									  escaped, escaped2);
				g_free(escaped2);
			}
			g_free(escaped);

			gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
			g_free(tmp);
		}

		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-joined", conv, user, flag, new_arrivals);
		ul = ul->next;
		fl = fl->next;
		if (extra_msgs != NULL)
			extra_msgs = extra_msgs->next;
	}

	cbuddies = g_list_sort(cbuddies, (GCompareFunc)gaim_conv_chat_cb_compare);
	
	if (ops != NULL && ops->chat_add_users != NULL)
		ops->chat_add_users(conv, cbuddies, new_arrivals);

	g_list_free(cbuddies);
}

void
gaim_conv_chat_rename_user(GaimConvChat *chat, const char *old_user,
						   const char *new_user)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GaimConnection *gc;
	GaimPluginProtocolInfo *prpl_info;
	GaimConvChatBuddy *cb;
	GaimConvChatBuddyFlags flags;
	const char *new_alias = new_user;
	char tmp[BUF_LONG];
	gboolean is_me = FALSE;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gc = gaim_conversation_get_gc(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	g_return_if_fail(prpl_info != NULL);

	flags = gaim_conv_chat_user_get_flags(chat, old_user);
	cb = gaim_conv_chat_cb_new(new_user, NULL, flags);
	gaim_conv_chat_set_users(chat,
		g_list_prepend(gaim_conv_chat_get_users(chat), cb));

	if (!strcmp(chat->nick, gaim_normalize(conv->account, old_user))) {
		const char *alias;

		/* Note this for later. */
		is_me = TRUE;

		alias = gaim_account_get_alias(conv->account);
		if (alias != NULL)
			new_alias = alias;
		else
		{
			const char *display_name = gaim_connection_get_display_name(gc);
			if (display_name != NULL)
				alias = display_name;
		}
	} else if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		GaimBuddy *buddy;
		if ((buddy = gaim_find_buddy(gc->account, new_user)) != NULL)
			new_alias = gaim_buddy_get_contact_alias(buddy);
	}

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user, new_alias);

	cb = gaim_conv_chat_cb_find(chat, old_user);

	if (cb) {
		gaim_conv_chat_set_users(chat,
				g_list_remove(gaim_conv_chat_get_users(chat), cb));
		gaim_conv_chat_cb_destroy(cb);
	}

	if (gaim_conv_chat_is_user_ignored(chat, old_user)) {
		gaim_conv_chat_unignore(chat, old_user);
		gaim_conv_chat_ignore(chat, new_user);
	}
	else if (gaim_conv_chat_is_user_ignored(chat, new_user))
		gaim_conv_chat_unignore(chat, new_user);

	if (is_me)
		gaim_conv_chat_set_nick(chat, new_user);

	if (gaim_prefs_get_bool("/core/conversations/chat/show_nick_change") &&
	    !gaim_conv_chat_is_user_ignored(chat, new_user)) {

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
				GaimBuddy *buddy;

				if ((buddy = gaim_find_buddy(gc->account, old_user)) != NULL)
					old_alias = gaim_buddy_get_contact_alias(buddy);
				if ((buddy = gaim_find_buddy(gc->account, new_user)) != NULL)
					new_alias = gaim_buddy_get_contact_alias(buddy);
			}

			escaped = g_markup_escape_text(old_alias, -1);
			escaped2 = g_markup_escape_text(new_alias, -1);
			g_snprintf(tmp, sizeof(tmp),
					_("%s is now known as %s"), escaped, escaped2);
			g_free(escaped);
			g_free(escaped2);
		}

		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	}
}

void
gaim_conv_chat_remove_user(GaimConvChat *chat, const char *user, const char *reason)
{
	GList *users = g_list_append(NULL, (char *)user);

	gaim_conv_chat_remove_users(chat, users, reason);

	g_list_free(users);
}

void
gaim_conv_chat_remove_users(GaimConvChat *chat, GList *users, const char *reason)
{
	GaimConversation *conv;
	GaimConnection *gc;
	GaimPluginProtocolInfo *prpl_info;
	GaimConversationUiOps *ops;
	GaimConvChatBuddy *cb;
	GList *l;
	gboolean quiet;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = gaim_conv_chat_get_conversation(chat);

	gc = gaim_conversation_get_gc(conv);
	g_return_if_fail(gc != NULL);
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	g_return_if_fail(prpl_info != NULL);

	ops  = gaim_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;
		quiet = GPOINTER_TO_INT(gaim_signal_emit_return_1(gaim_conversations_get_handle(),
					"chat-buddy-leaving", conv, user, reason)) |
				gaim_conv_chat_is_user_ignored(chat, user);

		cb = gaim_conv_chat_cb_find(chat, user);

		if (cb) {
			gaim_conv_chat_set_users(chat,
					g_list_remove(gaim_conv_chat_get_users(chat), cb));
			gaim_conv_chat_cb_destroy(cb);
		}

		/* NOTE: Don't remove them from ignored in case they re-enter. */
	
		if (!quiet) {
			const char *alias = user;
			char *escaped;
			char *tmp;

			if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
				GaimBuddy *buddy;

				if ((buddy = gaim_find_buddy(gc->account, user)) != NULL)
					alias = gaim_buddy_get_contact_alias(buddy);
			}

			escaped = g_markup_escape_text(alias, -1);

			if (reason == NULL || !*reason)
				tmp = g_strdup_printf(_("%s left the room."), escaped);
			else {
				char *escaped2 = g_markup_escape_text(reason, -1);
				tmp = g_strdup_printf(_("%s left the room (%s)."),
									  escaped, escaped2);
				g_free(escaped2);
			}
			g_free(escaped);

			gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
			g_free(tmp);
		}

		gaim_signal_emit(gaim_conversations_get_handle(), "chat-buddy-left",
						 conv, user, reason);
	}

	if (ops != NULL && ops->chat_remove_users != NULL)
		ops->chat_remove_users(conv, users);
}

void
gaim_conv_chat_clear_users(GaimConvChat *chat)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GList *users, *names = NULL;
	GList *l;

	g_return_if_fail(chat != NULL);

	conv  = gaim_conv_chat_get_conversation(chat);
	ops   = gaim_conversation_get_ui_ops(conv);
	users = gaim_conv_chat_get_users(chat);

	if (ops != NULL && ops->chat_remove_users != NULL) {
		for (l = users; l; l = l->next) {
			GaimConvChatBuddy *cb = l->data;
			names = g_list_append(names, cb->name);
		}
		ops->chat_remove_users(conv, names);
		g_list_free(names);
	}

	for (l = users; l; l = l->next)
	{
		GaimConvChatBuddy *cb = l->data;

		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-leaving", conv, cb->name, NULL);
		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-left", conv, cb->name, NULL);

		gaim_conv_chat_cb_destroy(cb);
	}

	g_list_free(users);
	gaim_conv_chat_set_users(chat, NULL);
}


gboolean
gaim_conv_chat_find_user(GaimConvChat *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (gaim_conv_chat_cb_find(chat, user) != NULL);
}

void
gaim_conv_chat_user_set_flags(GaimConvChat *chat, const char *user,
							  GaimConvChatBuddyFlags flags)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GaimConvChatBuddy *cb;
	GaimConvChatBuddyFlags oldflags;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	cb = gaim_conv_chat_cb_find(chat, user);

	if (!cb)
		return;

	if (flags == cb->flags)
		return;

	oldflags = cb->flags;
	cb->flags = flags;

	conv = gaim_conv_chat_get_conversation(chat);
	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(conv, user);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "chat-buddy-flags", conv, user, oldflags, flags);
}

GaimConvChatBuddyFlags
gaim_conv_chat_user_get_flags(GaimConvChat *chat, const char *user)
{
	GaimConvChatBuddy *cb;

	g_return_val_if_fail(chat != NULL, 0);
	g_return_val_if_fail(user != NULL, 0);

	cb = gaim_conv_chat_cb_find(chat, user);

	if (!cb)
		return GAIM_CBFLAGS_NONE;

	return cb->flags;
}

void gaim_conv_chat_set_nick(GaimConvChat *chat, const char *nick) {
	g_return_if_fail(chat != NULL);

	if(chat->nick)
		g_free(chat->nick);
	chat->nick = g_strdup(gaim_normalize(chat->conv->account, nick));
}

const char *gaim_conv_chat_get_nick(GaimConvChat *chat) {
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->nick;
}

GaimConversation *
gaim_find_chat(const GaimConnection *gc, int id)
{
	GList *l;
	GaimConversation *conv;

	for (l = gaim_get_chats(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)) == id &&
			gaim_conversation_get_gc(conv) == gc)
			return conv;
	}

	return NULL;
}

void
gaim_conv_chat_left(GaimConvChat *chat)
{
	g_return_if_fail(chat != NULL);

	chat->left = TRUE;
	gaim_conversation_update(chat->conv, GAIM_CONV_UPDATE_CHATLEFT);
}

gboolean
gaim_conv_chat_has_left(GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, TRUE);

	return chat->left;
}
GaimConvChatBuddy *
gaim_conv_chat_cb_new(const char *name, const char *alias, GaimConvChatBuddyFlags flags)
{
	GaimConvChatBuddy *cb;

	g_return_val_if_fail(name != NULL, NULL);

	cb = g_new0(GaimConvChatBuddy, 1);
	cb->name = g_strdup(name);
	cb->flags = flags;
	if (alias)
		cb->alias = g_strdup(alias);
	else
		cb->alias = NULL;

	GAIM_DBUS_REGISTER_POINTER(cb, GaimConvChatBuddy);
	return cb;
}

GaimConvChatBuddy *
gaim_conv_chat_cb_find(GaimConvChat *chat, const char *name)
{
	GList *l;
	GaimConvChatBuddy *cb = NULL;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	for (l = gaim_conv_chat_get_users(chat); l; l = l->next) {
		cb = l->data;
		if (!gaim_utf8_strcasecmp(cb->name, name))
			return cb;
	}

	return NULL;
}

void
gaim_conv_chat_cb_destroy(GaimConvChatBuddy *cb)
{
	g_return_if_fail(cb != NULL);

	g_free(cb->name);
	cb->name = NULL;
	cb->flags = 0;

	GAIM_DBUS_UNREGISTER_POINTER(cb);
	g_free(cb);
}

const char *
gaim_conv_chat_cb_get_name(GaimConvChatBuddy *cb)
{
	g_return_val_if_fail(cb != NULL, NULL);

	return cb->name;
}

void *
gaim_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_conversations_init(void)
{
	void *handle = gaim_conversations_get_handle();

	/**********************************************************************
	 * Register preferences
	 **********************************************************************/

	/* Conversations */
	gaim_prefs_add_none("/core/conversations");

	/* Conversations -> Chat */
	gaim_prefs_add_none("/core/conversations/chat");
	gaim_prefs_add_bool("/core/conversations/chat/show_nick_change", TRUE);

	/* Conversations -> IM */
	gaim_prefs_add_none("/core/conversations/im");
	gaim_prefs_add_bool("/core/conversations/im/send_typing", TRUE);


	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	gaim_signal_register(handle, "writing-im-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "wrote-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "sending-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "sent-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "receiving-im-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new_outgoing(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "received-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "writing-chat-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "wrote-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "sending-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "sent-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "receiving-chat-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new_outgoing(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "received-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "conversation-created",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "conversation-updated",
						 gaim_marshal_VOID__POINTER_UINT, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "deleting-conversation",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "buddy-typing",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "buddy-typed",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "buddy-typing-stopped",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-buddy-joining",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_UINT,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "chat-buddy-joined",
						 gaim_marshal_VOID__POINTER_POINTER_UINT_UINT, NULL, 4,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT),
						 gaim_value_new(GAIM_TYPE_BOOLEAN));

	gaim_signal_register(handle, "chat-buddy-flags",
						 gaim_marshal_VOID__POINTER_POINTER_UINT_UINT, NULL, 4,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "chat-buddy-leaving",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-buddy-left",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-inviting-user",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-invited-user",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-invited",
						 gaim_marshal_INT__POINTER_POINTER_POINTER_POINTER_POINTER,
						 NULL, 5,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_POINTER));

	gaim_signal_register(handle, "chat-joined",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "chat-left",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "chat-topic-changed",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));
}

void
gaim_conversations_uninit(void)
{
	while (conversations)
		gaim_conversation_destroy((GaimConversation*)conversations->data);
	gaim_signals_unregister_by_instance(gaim_conversations_get_handle());
}
