/*
 * Signals test plugin.
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#define SIGNAL_TEST_PLUGIN_ID "core-signals-test"

#include <stdio.h>

#include "internal.h"
#include "cipher.h"
#include "connection.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "ft.h"
#include "signals.h"
#include "version.h"
#include "status.h"
#include "sound.h"

/**************************************************************************
 * Account subsystem signal callbacks
 **************************************************************************/
static void
account_connecting_cb(GaimAccount *account, void *data)
{
	gaim_debug_misc("signals test", "account-connecting (%s)\n",
					gaim_account_get_username(account));
}

static void
account_setting_info_cb(GaimAccount *account, const char *info, void *data)
{
	gaim_debug_misc("signals test", "account-setting-info (%s, %s)\n",
					gaim_account_get_username(account), info);
}

static void
account_set_info_cb(GaimAccount *account, const char *info, void *data)
{
	gaim_debug_misc("signals test", "account-set-info (%s, %s)\n",
					gaim_account_get_username(account), info);
}

static void
account_status_changed(GaimAccount *account, GaimStatus *old, GaimStatus *new,
						gpointer data)
{
	gaim_debug_misc("signals test", "account-status-changed (%s, %s, %s)\n",
					gaim_account_get_username(account),
					gaim_status_get_name(old),
					gaim_status_get_name(new));
}

static void
account_alias_changed(GaimAccount *account, const char *old, gpointer data)
{
	gaim_debug_misc("signals test", "account-alias-changed (%s, %s, %s)\n",
					gaim_account_get_username(account),
					old, gaim_account_get_alias(account));
}

/**************************************************************************
 * Buddy Icons signal callbacks
 **************************************************************************/
static void
buddy_icon_changed_cb(GaimBuddy *buddy)
{
	gaim_debug_misc("signals test", "buddy icon changed (%s)\n",
					gaim_buddy_get_name(buddy));
}

/**************************************************************************
 * Buddy List subsystem signal callbacks
 **************************************************************************/
static void
buddy_status_changed_cb(GaimBuddy *buddy, GaimStatus *old_status,
                        GaimStatus *status, void *data)
{
	gaim_debug_misc("signals test", "buddy-status-changed (%s %s to %s)\n",
	                buddy->name, gaim_status_get_id(old_status),
	                gaim_status_get_id(status));
}

static void
buddy_idle_changed_cb(GaimBuddy *buddy, gboolean old_idle, gboolean idle,
                      void *data)
{
	gaim_debug_misc("signals test", "buddy-idle-changed (%s %s)\n",
	                buddy->name, old_idle ? "unidled" : "idled");
}

static void
buddy_signed_on_cb(GaimBuddy *buddy, void *data)
{
	gaim_debug_misc("signals test", "buddy-signed-on (%s)\n", buddy->name);
}

static void
buddy_signed_off_cb(GaimBuddy *buddy, void *data)
{
	gaim_debug_misc("signals test", "buddy-signed-off (%s)\n", buddy->name);
}

static void
buddy_added_cb(GaimBuddy *buddy, void *data)
{
	gaim_debug_misc("signals test", "buddy_added_cb (%s)\n", gaim_buddy_get_name(buddy));
}

static void
buddy_removed_cb(GaimBuddy *buddy, void *data)
{
	gaim_debug_misc("signals test", "buddy_removed_cb (%s)\n", gaim_buddy_get_name(buddy));
}

static void
blist_node_aliased(GaimBlistNode *node, const char *old_alias)
{
	GaimContact *p = (GaimContact *)node;
	GaimBuddy *b = (GaimBuddy *)node;
	GaimChat *c = (GaimChat *)node;
	GaimGroup *g = (GaimGroup *)node;

	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		gaim_debug_misc("signals test", "blist-node-aliased (Contact: %s, %s)\n", p->alias, old_alias);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		gaim_debug_misc("signals test", "blist-node-aliased (Buddy: %s, %s)\n", b->name, old_alias);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		gaim_debug_misc("signals test", "blist-node-aliased (Chat: %s, %s)\n", c->alias, old_alias);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		gaim_debug_misc("signals test", "blist-node-aliased (Group: %s, %s)\n", g->name, old_alias);
	else
		gaim_debug_misc("signals test", "blist-node-aliased (UNKNOWN: %d, %s)\n", node->type, old_alias);

}

static void
blist_node_extended_menu_cb(GaimBlistNode *node, void *data)
{
	GaimContact *p = (GaimContact *)node;
	GaimBuddy *b = (GaimBuddy *)node;
	GaimChat *c = (GaimChat *)node;
	GaimGroup *g = (GaimGroup *)node;

	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		gaim_debug_misc("signals test", "blist-node-extended-menu (Contact: %s)\n", p->alias);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		gaim_debug_misc("signals test", "blist-node-extended-menu (Buddy: %s)\n", b->name);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		gaim_debug_misc("signals test", "blist-node-extended-menu (Chat: %s)\n", c->alias);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		gaim_debug_misc("signals test", "blist-node-extended-menu (Group: %s)\n", g->name);
	else
		gaim_debug_misc("signals test", "blist-node-extended-menu (UNKNOWN: %d)\n", node->type);

}


/**************************************************************************
 * Connection subsystem signal callbacks
 **************************************************************************/
static void
signing_on_cb(GaimConnection *gc, void *data)
{
	gaim_debug_misc("signals test", "signing-on (%s)\n",
					gaim_account_get_username(gaim_connection_get_account(gc)));
}

static void
signed_on_cb(GaimConnection *gc, void *data)
{
	gaim_debug_misc("signals test", "signed-on (%s)\n",
					gaim_account_get_username(gaim_connection_get_account(gc)));
}

static void
signing_off_cb(GaimConnection *gc, void *data)
{
	gaim_debug_misc("signals test", "signing-off (%s)\n",
					gaim_account_get_username(gaim_connection_get_account(gc)));
}

static void
signed_off_cb(GaimConnection *gc, void *data)
{
	gaim_debug_misc("signals test", "signed-off (%s)\n",
					gaim_account_get_username(gaim_connection_get_account(gc)));
}

/**************************************************************************
 * Conversation subsystem signal callbacks
 **************************************************************************/
static gboolean
writing_im_msg_cb(GaimAccount *account, const char *who, char **buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("signals test", "writing-im-msg (%s, %s, %s)\n",
					gaim_account_get_username(account), gaim_conversation_get_name(conv), *buffer);

	return FALSE;

}

static void
wrote_im_msg_cb(GaimAccount *account, const char *who, const char *buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("signals test", "wrote-im-msg (%s, %s, %s)\n",
					gaim_account_get_username(account), gaim_conversation_get_name(conv), buffer);
}

static void
sending_im_msg_cb(GaimAccount *account, char *recipient, char **buffer, void *data)
{
	gaim_debug_misc("signals test", "sending-im-msg (%s, %s, %s)\n",
					gaim_account_get_username(account), recipient, *buffer);

}

static void
sent_im_msg_cb(GaimAccount *account, const char *recipient, const char *buffer, void *data)
{
	gaim_debug_misc("signals test", "sent-im-msg (%s, %s, %s)\n",
					gaim_account_get_username(account), recipient, buffer);
}

static gboolean
receiving_im_msg_cb(GaimAccount *account, char **sender, char **buffer,
				    GaimConversation *conv, int *flags, void *data)
{
	gaim_debug_misc("signals test", "receiving-im-msg (%s, %s, %s, %s, %d)\n",
					gaim_account_get_username(account), *sender, *buffer,
					(conv != NULL) ? gaim_conversation_get_name(conv) : "(null)", *flags);

	return FALSE;
}

static void
received_im_msg_cb(GaimAccount *account, char *sender, char *buffer,
				   GaimConversation *conv, int flags, void *data)
{
	gaim_debug_misc("signals test", "received-im-msg (%s, %s, %s, %s, %d)\n",
					gaim_account_get_username(account), sender, buffer,
					(conv != NULL) ? gaim_conversation_get_name(conv) : "(null)", flags);
}

static gboolean
writing_chat_msg_cb(GaimAccount *account, const char *who, char **buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("signals test", "writing-chat-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), *buffer);

	return FALSE;
}

static void
wrote_chat_msg_cb(GaimAccount *account, const char *who, const char *buffer,
				GaimConversation *conv, GaimMessageFlags flags, void *data)
{
	gaim_debug_misc("signals test", "wrote-chat-msg (%s, %s)\n",
					gaim_conversation_get_name(conv), buffer);
}

static gboolean
sending_chat_msg_cb(GaimAccount *account, char **buffer, int id, void *data)
{
	gaim_debug_misc("signals test", "sending-chat-msg (%s, %s, %d)\n",
					gaim_account_get_username(account), *buffer, id);

	return FALSE;
}

static void
sent_chat_msg_cb(GaimAccount *account, const char *buffer, int id, void *data)
{
	gaim_debug_misc("signals test", "sent-chat-msg (%s, %s, %d)\n",
					gaim_account_get_username(account), buffer, id);
}

static gboolean
receiving_chat_msg_cb(GaimAccount *account, char **sender, char **buffer,
					 GaimConversation *chat, int *flags, void *data)
{
	gaim_debug_misc("signals test",
					"receiving-chat-msg (%s, %s, %s, %s, %d)\n",
					gaim_account_get_username(account), *sender, *buffer,
					gaim_conversation_get_name(chat), *flags);

	return FALSE;
}

static void
received_chat_msg_cb(GaimAccount *account, char *sender, char *buffer,
					 GaimConversation *chat, int flags, void *data)
{
	gaim_debug_misc("signals test",
					"received-chat-msg (%s, %s, %s, %s, %d)\n",
					gaim_account_get_username(account), sender, buffer,
					gaim_conversation_get_name(chat), flags);
}

static void
conversation_created_cb(GaimConversation *conv, void *data)
{
	gaim_debug_misc("signals test", "conversation-created (%s)\n",
					gaim_conversation_get_name(conv));
}

static void
deleting_conversation_cb(GaimConversation *conv, void *data)
{
	gaim_debug_misc("signals test", "deleting-conversation (%s)\n",
					gaim_conversation_get_name(conv));
}

static void
buddy_typing_cb(GaimAccount *account, const char *name, void *data)
{
	gaim_debug_misc("signals test", "buddy-typing (%s, %s)\n",
					gaim_account_get_username(account), name);
}

static void
buddy_typing_stopped_cb(GaimAccount *account, const char *name, void *data)
{
	gaim_debug_misc("signals test", "buddy-typing-stopped (%s, %s)\n",
					gaim_account_get_username(account), name);
}

static gboolean
chat_buddy_joining_cb(GaimConversation *conv, const char *user,
					  GaimConvChatBuddyFlags flags, void *data)
{
	gaim_debug_misc("signals test", "chat-buddy-joining (%s, %s, %d)\n",
					gaim_conversation_get_name(conv), user, flags);

	return FALSE;
}

static void
chat_buddy_joined_cb(GaimConversation *conv, const char *user,
					 GaimConvChatBuddyFlags flags, gboolean new_arrival, void *data)
{
	gaim_debug_misc("signals test", "chat-buddy-joined (%s, %s, %d, %d)\n",
					gaim_conversation_get_name(conv), user, flags, new_arrival);
}

static void
chat_buddy_flags_cb(GaimConversation *conv, const char *user,
					GaimConvChatBuddyFlags oldflags, GaimConvChatBuddyFlags newflags, void *data)
{
	gaim_debug_misc("signals test", "chat-buddy-flags (%s, %s, %d, %d)\n",
					gaim_conversation_get_name(conv), user, oldflags, newflags);
}

static gboolean
chat_buddy_leaving_cb(GaimConversation *conv, const char *user,
					  const char *reason, void *data)
{
	gaim_debug_misc("signals test", "chat-buddy-leaving (%s, %s, %s)\n",
					gaim_conversation_get_name(conv), user, reason);

	return FALSE;
}

static void
chat_buddy_left_cb(GaimConversation *conv, const char *user,
				   const char *reason, void *data)
{
	gaim_debug_misc("signals test", "chat-buddy-left (%s, %s, %s)\n",
					gaim_conversation_get_name(conv), user, reason);
}

static void
chat_inviting_user_cb(GaimConversation *conv, const char *name,
					  char **reason, void *data)
{
	gaim_debug_misc("signals test", "chat-inviting-user (%s, %s, %s)\n",
					gaim_conversation_get_name(conv), name, *reason);
}

static void
chat_invited_user_cb(GaimConversation *conv, const char *name,
					  const char *reason, void *data)
{
	gaim_debug_misc("signals test", "chat-invited-user (%s, %s, %s)\n",
					gaim_conversation_get_name(conv), name, reason);
}

static gint
chat_invited_cb(GaimAccount *account, const char *inviter,
				const char *room_name, const char *message,
				const GHashTable *components, void *data)
{
	gaim_debug_misc("signals test", "chat-invited (%s, %s, %s, %s)\n",
					gaim_account_get_username(account), inviter,
					room_name, message);

	return 0;
}

static void
chat_joined_cb(GaimConversation *conv, void *data)
{
	gaim_debug_misc("signals test", "chat-joined (%s)\n",
					gaim_conversation_get_name(conv));
}

static void
chat_left_cb(GaimConversation *conv, void *data)
{
	gaim_debug_misc("signals test", "chat-left (%s)\n",
					gaim_conversation_get_name(conv));
}

static void
chat_topic_changed_cb(GaimConversation *conv, const char *who,
					  const char *topic, void *data)
{
	gaim_debug_misc("signals test",
					"chat-topic-changed (%s topic changed to: \"%s\" by %s)\n",
					gaim_conversation_get_name(conv), topic,
					(who) ? who : "unknown");
}
/**************************************************************************
 * Ciphers signal callbacks
 **************************************************************************/
static void
cipher_added_cb(GaimCipher *cipher, void *data) {
	gaim_debug_misc("signals test", "cipher %s added\n",
					gaim_cipher_get_name(cipher));
}

static void
cipher_removed_cb(GaimCipher *cipher, void *data) {
	gaim_debug_misc("signals test", "cipher %s removed\n",
					gaim_cipher_get_name(cipher));
}

/**************************************************************************
 * Core signal callbacks
 **************************************************************************/
static void
quitting_cb(void *data)
{
	gaim_debug_misc("signals test", "quitting ()\n");
}

/**************************************************************************
 * File transfer signal callbacks
 **************************************************************************/
static void
ft_recv_accept_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file receive accepted\n");
}

static void
ft_send_accept_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file send accepted\n");
}

static void
ft_recv_start_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file receive started\n");
}

static void
ft_send_start_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file send started\n");
}

static void
ft_recv_cancel_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file receive canceled\n");
}

static void
ft_send_cancel_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file send canceled\n");
}

static void
ft_recv_complete_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file receive completed\n");
}

static void
ft_send_complete_cb(GaimXfer *xfer, gpointer data) {
	gaim_debug_misc("signals test", "file send completed\n");
}

/**************************************************************************
 * Sound signal callbacks
 **************************************************************************/
static int
sound_playing_event_cb(GaimSoundEventID event, const GaimAccount *account) {
	if (account != NULL)
		gaim_debug_misc("signals test", "sound playing event: %d for account: %s\n",
	    	            event, gaim_account_get_username(account));
	else
		gaim_debug_misc("signals test", "sound playing event: %d\n", event);

	return 0;
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *core_handle     = gaim_get_core();
	void *blist_handle    = gaim_blist_get_handle();
	void *conn_handle     = gaim_connections_get_handle();
	void *conv_handle     = gaim_conversations_get_handle();
	void *accounts_handle = gaim_accounts_get_handle();
	void *ciphers_handle  = gaim_ciphers_get_handle();
	void *ft_handle       = gaim_xfers_get_handle();
	void *sound_handle    = gaim_sounds_get_handle();

	/* Accounts subsystem signals */
	gaim_signal_connect(accounts_handle, "account-connecting",
						plugin, GAIM_CALLBACK(account_connecting_cb), NULL);
	gaim_signal_connect(accounts_handle, "account-setting-info",
						plugin, GAIM_CALLBACK(account_setting_info_cb), NULL);
	gaim_signal_connect(accounts_handle, "account-set-info",
						plugin, GAIM_CALLBACK(account_set_info_cb), NULL);
	gaim_signal_connect(accounts_handle, "account-status-changed",
						plugin, GAIM_CALLBACK(account_status_changed), NULL);
	gaim_signal_connect(accounts_handle, "account-alias-changed",
						plugin, GAIM_CALLBACK(account_alias_changed), NULL);

	/* Buddy List subsystem signals */
	gaim_signal_connect(blist_handle, "buddy-status-changed",
						plugin, GAIM_CALLBACK(buddy_status_changed_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-idle-changed",
						plugin, GAIM_CALLBACK(buddy_idle_changed_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-signed-on",
						plugin, GAIM_CALLBACK(buddy_signed_on_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-signed-off",
						plugin, GAIM_CALLBACK(buddy_signed_off_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-added",
						plugin, GAIM_CALLBACK(buddy_added_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-removed",
						plugin, GAIM_CALLBACK(buddy_removed_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-icon-changed",
						plugin, GAIM_CALLBACK(buddy_icon_changed_cb), NULL);
	gaim_signal_connect(blist_handle, "blist-node-aliased",
						plugin, GAIM_CALLBACK(blist_node_aliased), NULL);
	gaim_signal_connect(blist_handle, "blist-node-extended-menu",
						plugin, GAIM_CALLBACK(blist_node_extended_menu_cb), NULL);

	/* Connection subsystem signals */
	gaim_signal_connect(conn_handle, "signing-on",
						plugin, GAIM_CALLBACK(signing_on_cb), NULL);
	gaim_signal_connect(conn_handle, "signed-on",
						plugin, GAIM_CALLBACK(signed_on_cb), NULL);
	gaim_signal_connect(conn_handle, "signing-off",
						plugin, GAIM_CALLBACK(signing_off_cb), NULL);
	gaim_signal_connect(conn_handle, "signed-off",
						plugin, GAIM_CALLBACK(signed_off_cb), NULL);

	/* Conversations subsystem signals */
	gaim_signal_connect(conv_handle, "writing-im-msg",
						plugin, GAIM_CALLBACK(writing_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "wrote-im-msg",
						plugin, GAIM_CALLBACK(wrote_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "sending-im-msg",
						plugin, GAIM_CALLBACK(sending_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "sent-im-msg",
						plugin, GAIM_CALLBACK(sent_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "receiving-im-msg",
						plugin, GAIM_CALLBACK(receiving_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "received-im-msg",
						plugin, GAIM_CALLBACK(received_im_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "writing-chat-msg",
						plugin, GAIM_CALLBACK(writing_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "wrote-chat-msg",
						plugin, GAIM_CALLBACK(wrote_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "sending-chat-msg",
						plugin, GAIM_CALLBACK(sending_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "sent-chat-msg",
						plugin, GAIM_CALLBACK(sent_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "receiving-chat-msg",
						plugin, GAIM_CALLBACK(receiving_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "received-chat-msg",
						plugin, GAIM_CALLBACK(received_chat_msg_cb), NULL);
	gaim_signal_connect(conv_handle, "conversation-created",
						plugin, GAIM_CALLBACK(conversation_created_cb), NULL);
	gaim_signal_connect(conv_handle, "deleting-conversation",
						plugin, GAIM_CALLBACK(deleting_conversation_cb), NULL);
	gaim_signal_connect(conv_handle, "buddy-typing",
						plugin, GAIM_CALLBACK(buddy_typing_cb), NULL);
	gaim_signal_connect(conv_handle, "buddy-typing-stopped",
						plugin, GAIM_CALLBACK(buddy_typing_stopped_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-buddy-joining",
						plugin, GAIM_CALLBACK(chat_buddy_joining_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-buddy-joined",
						plugin, GAIM_CALLBACK(chat_buddy_joined_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-buddy-flags",
						plugin, GAIM_CALLBACK(chat_buddy_flags_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-buddy-leaving",
						plugin, GAIM_CALLBACK(chat_buddy_leaving_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-buddy-left",
						plugin, GAIM_CALLBACK(chat_buddy_left_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-inviting-user",
						plugin, GAIM_CALLBACK(chat_inviting_user_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-invited-user",
						plugin, GAIM_CALLBACK(chat_invited_user_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-invited",
						plugin, GAIM_CALLBACK(chat_invited_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-joined",
						plugin, GAIM_CALLBACK(chat_joined_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-left",
						plugin, GAIM_CALLBACK(chat_left_cb), NULL);
	gaim_signal_connect(conv_handle, "chat-topic-changed",
						plugin, GAIM_CALLBACK(chat_topic_changed_cb), NULL);

	/* Ciphers signals */
	gaim_signal_connect(ciphers_handle, "cipher-added",
						plugin, GAIM_CALLBACK(cipher_added_cb), NULL);
	gaim_signal_connect(ciphers_handle, "cipher-removed",
						plugin, GAIM_CALLBACK(cipher_removed_cb), NULL);

	/* Core signals */
	gaim_signal_connect(core_handle, "quitting",
						plugin, GAIM_CALLBACK(quitting_cb), NULL);

	/* File transfer signals */
	gaim_signal_connect(ft_handle, "file-recv-accept",
						plugin, GAIM_CALLBACK(ft_recv_accept_cb), NULL);
	gaim_signal_connect(ft_handle, "file-recv-start",
						plugin, GAIM_CALLBACK(ft_recv_start_cb), NULL);
	gaim_signal_connect(ft_handle, "file-recv-cancel",
						plugin, GAIM_CALLBACK(ft_recv_cancel_cb), NULL);
	gaim_signal_connect(ft_handle, "file-recv-complete",
						plugin, GAIM_CALLBACK(ft_recv_complete_cb), NULL);
	gaim_signal_connect(ft_handle, "file-send-accept",
						plugin, GAIM_CALLBACK(ft_send_accept_cb), NULL);
	gaim_signal_connect(ft_handle, "file-send-start",
						plugin, GAIM_CALLBACK(ft_send_start_cb), NULL);
	gaim_signal_connect(ft_handle, "file-send-cancel",
						plugin, GAIM_CALLBACK(ft_send_cancel_cb), NULL);
	gaim_signal_connect(ft_handle, "file-send-complete",
						plugin, GAIM_CALLBACK(ft_send_complete_cb), NULL);

	/* Sound signals */
	gaim_signal_connect(sound_handle, "playing-sound-event", plugin,
	                    GAIM_CALLBACK(sound_playing_event_cb), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	SIGNAL_TEST_PLUGIN_ID,                            /**< id             */
	N_("Signals Test"),                               /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Test to see that all signals are working properly."),
	                                                  /**  description    */
	N_("Test to see that all signals are working properly."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(signalstest, init_plugin, info)
