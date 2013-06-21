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
#include "conversations.h"

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static PurpleConversationUiOps *default_ops = NULL;

GList *
purple_conversations_get(void)
{
	return conversations;
}

GList *
purple_conversations_get_ims(void)
{
	return ims;
}

GList *
purple_conversations_get_chats(void)
{
	return chats;
}

PurpleChatConversation *
purple_conversations_find_chat(const PurpleConnection *gc, int id)
{
	GList *l;
	PurpleConversation *conv;

	for (l = purple_conversations_get_chats(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv)) == id &&
			purple_conversation_get_connection(conv) == gc)
			return conv;
	}

	return NULL;
}

void
purple_conversations_set_ui_ops(PurpleConversationUiOps *ops)
{
	default_ops = ops;
}

void *
purple_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_conversations_init(void)
{
	void *handle = purple_conversations_get_handle();

	conversation_cache = g_hash_table_new_full((GHashFunc)_purple_conversations_hconv_hash,
						(GEqualFunc)_purple_conversations_hconv_equal,
						(GDestroyNotify)_purple_conversations_hconv_free_key, NULL);

	/**********************************************************************
	 * Register preferences
	 **********************************************************************/

	/* Conversations */
	purple_prefs_add_none("/purple/conversations");

	/* Conversations -> Chat */
	purple_prefs_add_none("/purple/conversations/chat");
	purple_prefs_add_bool("/purple/conversations/chat/show_nick_change", TRUE);

	/* Conversations -> IM */
	purple_prefs_add_none("/purple/conversations/im");
	purple_prefs_add_bool("/purple/conversations/im/send_typing", TRUE);


	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	purple_signal_register(handle, "writing-im-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_UINT,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "wrote-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "sent-attention",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT,
						 NULL, 4,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "got-attention",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT,
						 NULL, 4,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "sending-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "sent-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "receiving-im-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new_outgoing(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "received-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "blocked-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
							 PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "writing-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_UINT,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "wrote-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "sending-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "sent-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "receiving-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new_outgoing(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "received-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "conversation-created",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION));

	purple_signal_register(handle, "conversation-updated",
						 purple_marshal_VOID__POINTER_UINT, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "deleting-conversation",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION));

	purple_signal_register(handle, "buddy-typing",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "buddy-typed",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "buddy-typing-stopped",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "chat-buddy-joining",
						 purple_marshal_BOOLEAN__POINTER_POINTER_UINT,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "chat-buddy-joined",
						 purple_marshal_VOID__POINTER_POINTER_UINT_UINT, NULL, 4,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT),
						 purple_value_new(PURPLE_TYPE_BOOLEAN));

	purple_signal_register(handle, "chat-buddy-flags",
						 purple_marshal_VOID__POINTER_POINTER_UINT_UINT, NULL, 4,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_UINT),
						 purple_value_new(PURPLE_TYPE_UINT));

	purple_signal_register(handle, "chat-buddy-leaving",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "chat-buddy-left",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "deleting-chat-buddy",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CHATBUDDY));

	purple_signal_register(handle, "chat-inviting-user",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "chat-invited-user",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "chat-invited",
						 purple_marshal_INT__POINTER_POINTER_POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_INT), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_POINTER));

	purple_signal_register(handle, "chat-invite-blocked",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_POINTER,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
							 PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_BOXED, "GHashTable *"));

	purple_signal_register(handle, "chat-joined",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION));

	purple_signal_register(handle, "chat-join-failed",
						   purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						   purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONNECTION),
						   purple_value_new(PURPLE_TYPE_POINTER));

	purple_signal_register(handle, "chat-left",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION));

	purple_signal_register(handle, "chat-topic-changed",
						 purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "cleared-message-history",
	                       purple_marshal_VOID__POINTER, NULL, 1,
	                       purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                        PURPLE_SUBTYPE_CONVERSATION));

	purple_signal_register(handle, "conversation-extended-menu",
			     purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			     purple_value_new(PURPLE_TYPE_SUBTYPE,
					    PURPLE_SUBTYPE_CONVERSATION),
			     purple_value_new(PURPLE_TYPE_BOXED, "GList **"));
}

void
purple_conversations_uninit(void)
{
	while (conversations)
		purple_conversation_destroy((PurpleConversation*)conversations->data);
	g_hash_table_destroy(conversation_cache);
	purple_signals_unregister_by_instance(purple_conversations_get_handle());
}
