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
#include "conversations.h"

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static PurpleConversationUiOps *default_ops = NULL;

/**
 * A hash table used for efficient lookups of conversations by name.
 * struct _purple_hconv => PurpleConversation*
 */
static GHashTable *conversation_cache = NULL;

struct _purple_hconv {
	gboolean im;
	char *name;
	const PurpleAccount *account;
};

static guint
_purple_conversations_hconv_hash(struct _purple_hconv *hc)
{
	return g_str_hash(hc->name) ^ hc->im ^ g_direct_hash(hc->account);
}

static guint
_purple_conversations_hconv_equal(struct _purple_hconv *hc1, struct _purple_hconv *hc2)
{
	return (hc1->im == hc2->im &&
	        hc1->account == hc2->account &&
	        g_str_equal(hc1->name, hc2->name));
}

static void
_purple_conversations_hconv_free_key(struct _purple_hconv *hc)
{
	g_free(hc->name);
	g_free(hc);
}

void
purple_conversations_add(PurpleConversation *conv)
{
	PurpleAccount *account;
	struct _purple_hconv *hc;

	g_return_if_fail(conv != NULL);

	if (g_list_find(conversations, conv) != NULL)
		return;

	conversations = g_list_prepend(conversations, conv);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		ims = g_list_prepend(ims, conv);
	else
		chats = g_list_prepend(chats, conv);

	account = purple_conversation_get_account(conv);

	hc = g_new(struct _purple_hconv, 1);
	hc->name = g_strdup(purple_normalize(account,
				purple_conversation_get_name(conv)));
	hc->account = account;
	hc->im = PURPLE_IS_IM_CONVERSATION(conv);

	g_hash_table_insert(conversation_cache, hc, conv);
}

void
purple_conversations_remove(PurpleConversation *conv)
{
	PurpleAccount *account;
	struct _purple_hconv hc;

	g_return_if_fail(conv != NULL);

	conversations = g_list_remove(conversations, conv);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		ims = g_list_remove(ims, conv);
	else
		chats = g_list_remove(chats, conv);

	account = purple_conversation_get_account(conv);

	hc.name = (gchar *)purple_normalize(account,
				purple_conversation_get_name(conv));
	hc.account = account;
	hc.im = PURPLE_IS_IM_CONVERSATION(conv);

	g_hash_table_remove(conversation_cache, &hc);
}

void
purple_conversations_update_cache(PurpleConversation *conv, const char *name,
		PurpleAccount *account)
{
	PurpleAccount *old_account;
	struct _purple_hconv *hc;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(account != NULL || name != NULL);

	old_account = purple_conversation_get_account(conv);

	hc = g_new(struct _purple_hconv, 1);
	hc->im = PURPLE_IS_IM_CONVERSATION(conv);
	hc->account = old_account;
	hc->name = (gchar *)purple_normalize(old_account,
				purple_conversation_get_name(conv));

	g_hash_table_remove(conversation_cache, hc);

	if (account)
		hc->account = account;
	if (name)
		hc->name = g_strdup(purple_normalize(account, name));

	g_hash_table_insert(conversation_cache, hc, conv);
}

GList *
purple_conversations_get_all(void)
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

PurpleConversation *
purple_conversations_find_with_account(const char *name,
		const PurpleAccount *account)
{
	PurpleConversation *c = NULL;
	struct _purple_hconv hc;

	g_return_val_if_fail(name != NULL, NULL);

	hc.name = (gchar *)purple_normalize(account, name);
	hc.account = account;

	hc.im = TRUE;
	c = g_hash_table_lookup(conversation_cache, &hc);
	if (!c) {
		hc.im = FALSE;
		c = g_hash_table_lookup(conversation_cache, &hc);
	}

	return c;
}

PurpleIMConversation *
purple_conversations_find_im_with_account(const char *name,
		const PurpleAccount *account)
{
	PurpleIMConversation *im = NULL;
	struct _purple_hconv hc;

	g_return_val_if_fail(name != NULL, NULL);

	hc.name = (gchar *)purple_normalize(account, name);
	hc.account = account;
	hc.im = TRUE;

	im = g_hash_table_lookup(conversation_cache, &hc);

	return im;
}

PurpleChatConversation *
purple_conversations_find_chat_with_account(const char *name,
		const PurpleAccount *account)
{
	PurpleChatConversation *c = NULL;
	struct _purple_hconv hc;

	g_return_val_if_fail(name != NULL, NULL);

	hc.name = (gchar *)purple_normalize(account, name);
	hc.account = account;
	hc.im = FALSE;

	c = g_hash_table_lookup(conversation_cache, &hc);

	return c;
}

PurpleChatConversation *
purple_conversations_find_chat(const PurpleConnection *gc, int id)
{
	GList *l;
	PurpleChatConversation *chat;

	for (l = purple_conversations_get_chats(); l != NULL; l = l->next) {
		chat = (PurpleChatConversation *)l->data;

		if (purple_chat_conversation_get_id(chat) == id &&
				purple_conversation_get_connection(PURPLE_CONVERSATION(chat)) == gc)
			return chat;
	}

	return NULL;
}

void
purple_conversations_set_ui_ops(PurpleConversationUiOps *ops)
{
	default_ops = ops;
}

PurpleConversationUiOps *
purple_conversations_get_ui_ops(void)
{
	return default_ops;
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
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "wrote-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "sent-attention",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 4, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "got-attention",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 4, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "sending-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_POINTER); /* pointer to a string */

	purple_signal_register(handle, "sent-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING);

	purple_signal_register(handle, "receiving-im-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT,
						 G_TYPE_POINTER, /* pointer to a string */
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION,
						 G_TYPE_POINTER); /* pointer to a string */

	purple_signal_register(handle, "received-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "blocked-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_UINT_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

	purple_signal_register(handle, "writing-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "wrote-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "sending-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_UINT, G_TYPE_NONE,
						 3, PURPLE_TYPE_ACCOUNT,
						 G_TYPE_POINTER, /* pointer to a string */
						 G_TYPE_UINT);

	purple_signal_register(handle, "sent-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_UINT, G_TYPE_NONE,
						 3, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING, G_TYPE_UINT);

	purple_signal_register(handle, "receiving-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT,
						 G_TYPE_POINTER, /* pointer to a string */
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION,
						 G_TYPE_POINTER); /* pointer to an unsigned int */

	purple_signal_register(handle, "received-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "conversation-created",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "conversation-updated",
						 purple_marshal_VOID__POINTER_UINT, G_TYPE_NONE, 2,
						 PURPLE_TYPE_CONVERSATION, G_TYPE_UINT);

	purple_signal_register(handle, "deleting-conversation",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "buddy-typing",
						 purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
						 PURPLE_TYPE_ACCOUNT, G_TYPE_STRING);

	purple_signal_register(handle, "buddy-typed",
						 purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
						 PURPLE_TYPE_ACCOUNT, G_TYPE_STRING);

	purple_signal_register(handle, "buddy-typing-stopped",
						 purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
						 PURPLE_TYPE_ACCOUNT, G_TYPE_STRING);

	purple_signal_register(handle, "chat-user-joining",
						 purple_marshal_BOOLEAN__POINTER_POINTER_UINT,
						 G_TYPE_BOOLEAN, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_UINT);

	purple_signal_register(handle, "chat-user-joined",
						 purple_marshal_VOID__POINTER_POINTER_UINT_UINT,
						 G_TYPE_NONE, 4, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_UINT, G_TYPE_BOOLEAN);

	purple_signal_register(handle, "chat-user-flags",
						 purple_marshal_VOID__POINTER_UINT_UINT, G_TYPE_NONE, 3,
						 PURPLE_TYPE_CHAT_USER, G_TYPE_UINT, G_TYPE_UINT);

	purple_signal_register(handle, "chat-user-leaving",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_STRING);

	purple_signal_register(handle, "chat-user-left",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_STRING);

	purple_signal_register(handle, "deleting-chat-user",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CHAT_USER);

	purple_signal_register(handle, "chat-inviting-user",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_CONVERSATION, 
						 G_TYPE_STRING,
						 G_TYPE_POINTER); /* pointer to a string */

	purple_signal_register(handle, "chat-invited-user",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_STRING);

	purple_signal_register(handle, "chat-invited",
						 purple_marshal_INT__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_INT, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	purple_signal_register(handle, "chat-invite-blocked",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_POINTER); /* (GHashTable *) */

	purple_signal_register(handle, "chat-joined",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "chat-join-failed",
						   purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
						   PURPLE_TYPE_CONNECTION, G_TYPE_POINTER);

	purple_signal_register(handle, "chat-left",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "chat-topic-changed",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_STRING);

	purple_signal_register(handle, "cleared-message-history",
	                       purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                       PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "conversation-extended-menu",
			     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			     PURPLE_TYPE_CONVERSATION,
			     G_TYPE_POINTER); /* (GList **) */
}

void
purple_conversations_uninit(void)
{
	while (conversations)
		g_object_unref(G_OBJECT(conversations->data));

	g_hash_table_destroy(conversation_cache);
	purple_signals_unregister_by_instance(purple_conversations_get_handle());
}
