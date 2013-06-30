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
#include "enums.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "util.h"

#define PURPLE_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CONVERSATION, PurpleConversationPrivate))

/** @copydoc _PurpleConversationPrivate */
typedef struct _PurpleConversationPrivate  PurpleConversationPrivate;

/**
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat.
 */
struct _PurpleConversationPrivate
{
	PurpleAccount *account;           /**< The user using this conversation. */

	char *name;                       /**< The name of the conversation.     */
	char *title;                      /**< The window title.                 */

	gboolean logging;                 /**< The status of logging.            */

	GList *logs;                      /**< This conversation's logs          */

	PurpleConversationUiOps *ui_ops;  /**< UI-specific operations.           */

	PurpleConnectionFlags features;   /**< The supported features            */
	GList *message_history;           /**< Message history, as a GList of
	                                       PurpleConversationMessage's       */
};

/**
 * Description of a conversation message
 */
struct _PurpleConversationMessage
{
	char *who;
	char *what;
	PurpleMessageFlags flags;
	time_t when;
	PurpleConversation *conv;
	char *alias;
};

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_ACCOUNT,
	PROP_NAME,
	PROP_TITLE,
	PROP_LOGGING,
	PROP_FEATURES,
	PROP_LAST
};

static GObjectClass *parent_class;


static void
common_send(PurpleConversation *conv, const char *message, PurpleMessageFlags msgflags)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	char *displayed = NULL, *sent = NULL;
	int err = 0;

	g_return_if_fail(priv != NULL);

	if (*message == '\0')
		return;

	account = purple_conversation_get_account(conv);
	gc = purple_conversation_get_connection(conv);

	g_return_if_fail(account != NULL);
	g_return_if_fail(gc != NULL);

	/* Always linkfy the text for display, unless we're
	 * explicitly asked to do otheriwse*/
	if (!(msgflags & PURPLE_MESSAGE_INVISIBLE)) {
		if(msgflags & PURPLE_MESSAGE_NO_LINKIFY)
			displayed = g_strdup(message);
		else
			displayed = purple_markup_linkify(message);
	}

	if (displayed && (priv->features & PURPLE_CONNECTION_HTML) &&
		!(msgflags & PURPLE_MESSAGE_RAW)) {
		sent = g_strdup(displayed);
	} else
		sent = g_strdup(message);

	msgflags |= PURPLE_MESSAGE_SEND;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		purple_signal_emit(purple_conversations_get_handle(), "sending-im-msg",
						 account,
						 purple_conversation_get_name(conv), &sent);

		if (sent != NULL && sent[0] != '\0') {

			err = serv_send_im(gc, purple_conversation_get_name(conv),
			                   sent, msgflags);

			if ((err > 0) && (displayed != NULL))
				purple_conversation_write_message(conv, NULL, displayed, msgflags, time(NULL));

			purple_signal_emit(purple_conversations_get_handle(), "sent-im-msg",
							 account,
							 purple_conversation_get_name(conv), sent);
		}
	}
	else {
		PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(conv);
		purple_signal_emit(purple_conversations_get_handle(), "sending-chat-msg",
						 account, &sent,
						 purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)));

		if (sent != NULL && sent[0] != '\0') {
			err = serv_chat_send(gc, purple_chat_conversation_get_id(chat), sent, msgflags);

			purple_signal_emit(purple_conversations_get_handle(), "sent-chat-msg",
							 account, sent,
							 purple_chat_conversation_get_id(chat));
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
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	priv->logs = g_list_append(NULL, purple_log_new(PURPLE_IS_CHAT_CONVERSATION(conv) ? PURPLE_LOG_CHAT :
							   PURPLE_LOG_IM, priv->name, priv->account,
							   conv, time(NULL), NULL));
}

/* Functions that deal with PurpleConversationMessage */

static void
add_message_to_history(PurpleConversation *conv, const char *who, const char *alias,
		const char *message, PurpleMessageFlags flags, time_t when)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleConversationMessage *msg;
	PurpleConnection *gc;

	g_return_if_fail(priv != NULL);

	gc = purple_account_get_connection(priv->account);

	if (flags & PURPLE_MESSAGE_SEND) {
		const char *me = NULL;
		if (gc)
			me = purple_connection_get_display_name(gc);
		if (!me)
			me = purple_account_get_username(priv->account);
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

	priv->message_history = g_list_prepend(priv->message_history, msg);
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
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	priv->features = features;
	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_FEATURES);
}

PurpleConnectionFlags
purple_conversation_get_features(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->features;
}

void
purple_conversation_set_ui_ops(PurpleConversation *conv,
							 PurpleConversationUiOps *ops)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (priv->ui_ops == ops)
		return;

	if (priv->ui_ops != NULL && priv->ui_ops->destroy_conversation != NULL)
		priv->ui_ops->destroy_conversation(conv);

	priv->ui_ops = ops;
}

PurpleConversationUiOps *
purple_conversation_get_ui_ops(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ui_ops;
}

void
purple_conversation_set_account(PurpleConversation *conv, PurpleAccount *account)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (account == purple_conversation_get_account(conv))
		return;

	purple_conversations_update_cache(conv, NULL, account);
	priv->account = account;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_ACCOUNT);
}

PurpleAccount *
purple_conversation_get_account(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
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
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(title != NULL);

	g_free(priv->title);
	priv->title = g_strdup(title);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_TITLE);
}

const char *
purple_conversation_get_title(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->title;
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

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (account && ((b = purple_find_buddy(account, name)) != NULL))
			text = purple_buddy_get_contact_alias(b);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		if (account && ((chat = purple_blist_find_chat(account, name)) != NULL))
			text = purple_chat_get_name(chat);
	}

	if(text == NULL)
		text = name;

	purple_conversation_set_title(conv, text);
}

void
purple_conversation_set_name(PurpleConversation *conv, const char *name)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	purple_conversations_update_cache(conv, name, NULL);

	g_free(priv->name);
	priv->name = g_strdup(name);

	purple_conversation_autoset_title(conv);
}

const char *
purple_conversation_get_name(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

void
purple_conversation_set_logging(PurpleConversation *conv, gboolean log)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (priv->logging != log)
	{
		priv->logging = log;
		if (log && priv->logs == NULL)
			open_log(conv);

		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_LOGGING);
	}
}

gboolean
purple_conversation_is_logging(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->logging;
}

void
purple_conversation_close_logs(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	g_list_foreach(priv->logs, (GFunc)purple_log_free, NULL);
	g_list_free(priv->logs);
	priv->logs = NULL;
}

void
purple_conversation_write(PurpleConversation *conv, const char *who,
						const char *message, PurpleMessageFlags flags,
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
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	/* int logging_font_options = 0; */

	g_return_if_fail(priv    != NULL);
	g_return_if_fail(message != NULL);

	ops = purple_conversation_get_ui_ops(conv);

	account = purple_conversation_get_account(conv);

	if (account != NULL)
		gc = purple_account_get_connection(account);

	if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
		(gc != NULL && !g_slist_find(gc->buddy_chats, conv)))
		return;

	if (PURPLE_IS_IM_CONVERSATION(conv) &&
		!g_list_find(purple_conversations_get_all(), conv))
		return;

	displayed = g_strdup(message);

	if (who == NULL || *who == '\0')
		who = purple_conversation_get_name(conv);
	alias = who;

	plugin_return =
		GPOINTER_TO_INT(purple_signal_emit_return_1(
			purple_conversations_get_handle(),
			(PURPLE_IS_IM_CONVERSATION(conv) ? "writing-im-msg" : "writing-chat-msg"),
			account, who, &displayed, conv, flags));

	if (displayed == NULL)
		return;

	if (plugin_return) {
		g_free(displayed);
		return;
	}

	if (account != NULL) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(account)));

		if (PURPLE_IS_IM_CONVERSATION(conv) ||
			!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (flags & PURPLE_MESSAGE_SEND) {
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

	if (!(flags & PURPLE_MESSAGE_NO_LOG) && purple_conversation_is_logging(conv)) {
		GList *log;

		log = priv->logs;
		while (log != NULL) {
			purple_log_write((PurpleLog *)log->data, flags, alias, mtime, displayed);
			log = log->next;
		}
	}

	if (ops && ops->write_conv)
		ops->write_conv(conv, who, alias, displayed, flags, mtime);

	add_message_to_history(conv, who, alias, message, flags, mtime);

	purple_signal_emit(purple_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "wrote-im-msg" : "wrote-chat-msg"),
		account, who, displayed, conv, flags);

	g_free(displayed);
}

void
purple_conversation_write_message(PurpleConversation *conv, const char *who,
		const char *message, PurpleMessageFlags flags, time_t mtime)
{
	PurpleConversationClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	klass = PURPLE_CONVERSATION_GET_CLASS(conv);

	if (klass && klass->write_message)
		klass->write_message(conv, who, message, flags, mtime);
}

void
purple_conversation_send(PurpleConversation *conv, const char *message)
{
	purple_conversation_send_with_flags(conv, message, 0);
}

void
purple_conversation_send_with_flags(PurpleConversation *conv, const char *message,
		PurpleMessageFlags flags)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(message != NULL);

	common_send(conv, message, flags);
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

gboolean purple_conversation_helper_present_error(const char *who, PurpleAccount *account, const char *what)
{
	PurpleConversation *conv;

	g_return_val_if_fail(who != NULL, FALSE);
	g_return_val_if_fail(account !=NULL, FALSE);
	g_return_val_if_fail(what != NULL, FALSE);

	conv = purple_conversations_find_with_account(who, account);
	if (conv != NULL)
		purple_conversation_write(conv, NULL, what, PURPLE_MESSAGE_ERROR, time(NULL));
	else
		return FALSE;

	return TRUE;
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
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(message != NULL);

	if (priv->ui_ops != NULL && priv->ui_ops->send_confirm != NULL)
	{
		priv->ui_ops->send_confirm(conv, message);
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

gboolean
purple_conversation_custom_smiley_add(PurpleConversation *conv, const char *smile,
                            const char *cksum_type, const char *chksum,
							gboolean remote)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, FALSE);

	if (smile == NULL || !*smile) {
		return FALSE;
	}

	/* TODO: check if the icon is in the cache and return false if so */
	/* TODO: add an icon cache (that doesn't suck) */
	if (priv->ui_ops != NULL && priv->ui_ops->custom_smiley_add !=NULL) {
		return priv->ui_ops->custom_smiley_add(conv, smile, remote);
	} else {
		purple_debug_info("conversation", "Could not find add custom smiley function");
		return FALSE;
	}

}

void
purple_conversation_custom_smiley_write(PurpleConversation *conv, const char *smile,
                                   const guchar *data, gsize size)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (priv->ui_ops != NULL && priv->ui_ops->custom_smiley_write != NULL)
		priv->ui_ops->custom_smiley_write(conv, smile, data, size);
	else
		purple_debug_info("conversation", "Could not find the smiley write function");
}

void
purple_conversation_custom_smiley_close(PurpleConversation *conv, const char *smile)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(smile != NULL && *smile);

	if (priv->ui_ops != NULL && priv->ui_ops->custom_smiley_close != NULL)
		priv->ui_ops->custom_smiley_close(conv, smile);
	else
		purple_debug_info("conversation", "Could not find custom smiley close function");
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
	GList *list;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	list = priv->message_history;
	message_history_free(list);
	priv->message_history = NULL;

	purple_signal_emit(purple_conversations_get_handle(),
			"cleared-message-history", conv);
}

GList *purple_conversation_get_message_history(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->message_history;
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

PurpleMessageFlags purple_conversation_message_get_flags(const PurpleConversationMessage *msg)
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

PurpleConversation *purple_conversation_message_get_conversation(const PurpleConversationMessage *msg)
{
	g_return_val_if_fail(msg, NULL);
	return msg->conv;
}

static PurpleConversationMessage *
purple_conversation_message_copy(PurpleConversationMessage *msg)
{
	PurpleConversationMessage *newmsg = g_new(PurpleConversationMessage, 1);
	*newmsg = *msg;
	newmsg->who = g_strdup(msg->who);
	newmsg->what = g_strdup(msg->what);
	newmsg->alias = g_strdup(msg->alias);

	return newmsg;
}

static void
purple_conversation_message_free(PurpleConversationMessage *msg)
{
	g_free(msg->who);
	g_free(msg->what);
	g_free(msg->alias);

	g_free(msg);
}

GType
purple_conversation_message_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleConversationMessage",
				(GBoxedCopyFunc)purple_conversation_message_copy,
				(GBoxedFreeFunc)purple_conversation_message_free);
	}

	return type;
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

/**************************************************************************
 * GObject code
 **************************************************************************/

/* GObject Property names */
#define PROP_ACCOUNT_S   "account"
#define PROP_NAME_S      "name"
#define PROP_TITLE_S     "title"
#define PROP_LOGGING_S   "logging"
#define PROP_FEATURES_S  "features"

/* Set method for GObject properties */
static void
purple_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(obj);
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	switch (param_id) {
		/* account, name and title are assigned directly here as
		 * purple_im_conversation_new() and purple_chat_conversation_new()
		 * pass these properties as parameters, and so the conversation hasn't
		 * finished being set up */
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_NAME:
			g_free(priv->name);
			priv->name = g_strdup(g_value_get_string(value));
			break;
		case PROP_TITLE:
			g_free(priv->title);
			priv->title = g_strdup(g_value_get_string(value));
			break;
		case PROP_LOGGING:
			purple_conversation_set_logging(conv, g_value_get_boolean(value));
			break;
		case PROP_FEATURES:
			purple_conversation_set_features(conv, g_value_get_flags(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(obj);

	switch (param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_conversation_get_account(conv));
			break;
		case PROP_NAME:
			g_value_set_string(value, purple_conversation_get_name(conv));
			break;
		case PROP_TITLE:
			g_value_set_string(value, purple_conversation_get_title(conv));
			break;
		case PROP_LOGGING:
			g_value_set_boolean(value, purple_conversation_is_logging(conv));
			break;
		case PROP_FEATURES:
			g_value_set_flags(value, purple_conversation_get_features(conv));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_conversation_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_CONVERSATION(instance), PurpleConversation);
}

/* GObject dispose function */
static void
purple_conversation_dispose(GObject *object)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(object);

	g_return_if_fail(conv != NULL);

	purple_request_close_with_handle(conv);

	/* remove from conversations and im/chats lists prior to emit */
	purple_conversations_remove(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "deleting-conversation", conv);

	purple_conversation_close_logs(conv);
	purple_conversation_clear_message_history(conv);

	PURPLE_DBUS_UNREGISTER_POINTER(conv);

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_conversation_finalize(GObject *object)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(object);
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleConversationUiOps *ops  = purple_conversation_get_ui_ops(conv);

	g_free(priv->name);
	g_free(priv->title);

	priv->name = NULL;
	priv->title = NULL;

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

	parent_class->finalize(object);
}

/* Class initializer function */
static void purple_conversation_class_init(PurpleConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_conversation_dispose;
	obj_class->finalize = purple_conversation_finalize;

	/* Setup properties */
	obj_class->get_property = purple_conversation_get_property;
	obj_class->set_property = purple_conversation_set_property;

	g_object_class_install_property(obj_class, PROP_ACCOUNT,
			g_param_spec_object(PROP_ACCOUNT_S, _("Account"),
				_("The account for the conversation."), PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, PROP_NAME,
			g_param_spec_string(PROP_NAME_S, _("Name"),
				_("The name of the conversation."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_TITLE,
			g_param_spec_string(PROP_TITLE_S, _("Title"),
				_("The title of the conversation."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_LOGGING,
			g_param_spec_boolean(PROP_LOGGING_S, _("Logging status"),
				_("Whether logging is enabled or not."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_FEATURES,
			g_param_spec_flags(PROP_FEATURES_S, _("Connection features"),
				_("The connection features of the conversation."),
				PURPLE_TYPE_CONNECTION_FLAGS, 0,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleConversationPrivate));
}

GType
purple_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleConversation),
			0,
			(GInstanceInitFunc)purple_conversation_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleConversation",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}
