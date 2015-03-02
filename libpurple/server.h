/* purple
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

#ifndef _PURPLE_SERVER_H_
#define _PURPLE_SERVER_H_
/**
 * SECTION:server
 * @section_id: libpurple-server
 * @short_description: <filename>server.h</filename>
 * @title: Server API
 */

#include "accounts.h"
#include "conversations.h"
#include "message.h"
#include "protocols.h"

G_BEGIN_DECLS

/**
 * purple_serv_send_typing:
 * @gc:    The connection over which to send the typing notification.
 * @name:  The user to send the typing notification to.
 * @state: One of PURPLE_IM_TYPING, PURPLE_IM_TYPED, or PURPLE_IM_NOT_TYPING.
 *
 * Send a typing message to a given user over a given connection.
 *
 * Returns: A quiet-period, specified in seconds, where Purple will not
 *         send any additional typing notification messages.  Most
 *         protocols should return 0, which means that no additional
 *         PURPLE_IM_TYPING messages need to be sent.  If this is 5, for
 *         example, then Purple will wait five seconds, and if the Purple
 *         user is still typing then Purple will send another PURPLE_IM_TYPING
 *         message.
 */
/* TODO Could probably move this into the conversation API. */
unsigned int purple_serv_send_typing(PurpleConnection *gc, const char *name, PurpleIMTypingState state);

/**
 * purple_serv_move_buddy:
 * @buddy:  The Buddy.
 * @orig:   Original group.
 * @dest:   Destiny group.
 *
 * Move a buddy from one group to another on server.
 */
void purple_serv_move_buddy(PurpleBuddy *buddy, PurpleGroup *orig, PurpleGroup *dest);

/**
 * purple_serv_send_im:
 * @gc:     The connection over which to send the typing notification.
 * @msg:    The message.
 *
 * Sends the message to the user through the required protocol.
 *
 * Returns: The error value returned from the protocol interface function.
 */
int  purple_serv_send_im(PurpleConnection *gc, PurpleMessage *msg);

/**
 * purple_get_attention_type_from_code:
 *
 * Get information about an account's attention commands, from the protocol.
 *
 * Returns: The attention command numbered 'code' from the protocol's attention_types, or NULL.
 */
PurpleAttentionType *purple_get_attention_type_from_code(PurpleAccount *account, guint type_code);

/**
 * purple_serv_get_info:
 * @gc:     The connection over which to send the typing notification.
 * @name:   The name of the buddy we were asking information from.
 *
 * Request user infromation from the server.
 */
void purple_serv_get_info(PurpleConnection *gc, const char *name);

/**
 * purple_serv_set_info:
 * @gc:     The connection over which to send the typing notification.
 * @info:   Information text to be sent to the server.
 *
 * Set user account information on the server.
 */
void purple_serv_set_info(PurpleConnection *gc, const char *info);

/******************************************************************************
 * Privacy interface
 *****************************************************************************/

/**
 * purple_serv_add_permit:
 * @gc:     The connection over which to send the typing notification.
 * @name:   The name of the remote user.
 *
 * Add the buddy on the required authorized list.
 */
void purple_serv_add_permit(PurpleConnection *gc, const char *name);

/**
 * purple_serv_add_deny:
 * @gc:     The connection over which to send the typing notification.
 * @name:   The name of the remote user.
 *
 * Add the buddy on the required blocked list.
 */
void purple_serv_add_deny(PurpleConnection *gc, const char *name);

/**
 * purple_serv_rem_permit:
 * @gc:     The connection over which to send the typing notification.
 * @name:   The name of the remote user.
 *
 * Remove the buddy from the required authorized list.
 */
void purple_serv_rem_permit(PurpleConnection *gc, const char *name);

/**
 * purple_serv_rem_deny:
 * @gc:     The connection over which to send the typing notification.
 * @name:   The name of the remote user.
 *
 * Remove the buddy from the required blocked list.
 */
void purple_serv_rem_deny(PurpleConnection *gc, const char *name);

/**
 * purple_serv_set_permit_deny:
 * @gc:     The connection over which to send the typing notification.
 *
 * Update the server with the privacy information on the permit and deny lists.
 */
void purple_serv_set_permit_deny(PurpleConnection *gc);

/******************************************************************************
 * Chat Interface
 *****************************************************************************/

/**
 * purple_serv_chat_invite
 * @gc:     The connection over which to send the typing notification.
 * @id:     The id of the chat to invite the user to.
 * @message:A message displayed to the user when the invitation.
 * @name:   The name of the remote user to send the invitation to.
 *
 * Invite a user to join a chat.
 */
void purple_serv_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name);

/**
 * purple_serv_chat_leave:
 * @gc:     The connection over which to send the typing notification.
 * @id:     The id of the chat to leave.
 *
 * Called when the user requests leaving a chat.
 */
void purple_serv_chat_leave(PurpleConnection *gc, int id);

/**
 * purple_serv_chat_send:
 * @gc:     The connection over which to send the typing notification.
 * @id:     The id of the chat to send the message to.
 * @msg:    The message to send to the chat.
 *
 * Send a message to a chat.
 *
 * This protocol function should return a positive value on
 * success. If the message is too big to be sent, return
 * <literal>-E2BIG</literal>. If the account is not connected,
 * return <literal>-ENOTCONN</literal>. If the protocol is unable
 * to send the message for another reason, return some other
 * negative value. You can use one of the valid #errno values, or
 * just big something.
 *
 * Returns:  A positive number or 0 in case of success, a
 *           negative error number in case of failure.
 */
int  purple_serv_chat_send(PurpleConnection *gc, int id, PurpleMessage *msg);

/******************************************************************************
 * Server Interface
 *****************************************************************************/

/**
 * purple_serv_alias_buddy:
 * @buddy:  The Buddy.
 *
 * Save/store buddy's alias on server list/roster
 */
void purple_serv_alias_buddy(PurpleBuddy *buddy);

/**
 * purple_serv_got_alias:
 * @gc:     The connection over which to send the typing notification.
 * @who: The name of the buddy whose alias was received.
 * @alias: The alias that was received.
 *
 * Protocol should call this function when it retrieves an alias form the server.
 *
 */
void purple_serv_got_alias(PurpleConnection *gc, const char *who, const char *alias);

/**
 * purple_serv_got_private_alias:
 * @gc: The connection on which the alias was received.
 * @who: The name of the buddy whose alias was received.
 * @alias: The alias that was received.
 *
 * A protocol should call this when it retrieves a private alias from
 * the server.  Private aliases are the aliases the user sets, while public
 * aliases are the aliases or display names that buddies set for themselves.
 */
void purple_serv_got_private_alias(PurpleConnection *gc, const char *who, const char *alias);


/**
 * purple_serv_got_typing:
 * @gc:      The connection on which the typing message was received.
 * @name:    The name of the remote user.
 * @timeout: If this is a number greater than 0, then
 *        Purple will wait this number of seconds and then
 *        set this buddy to the PURPLE_IM_NOT_TYPING state.  This
 *        is used by protocols that send repeated typing messages
 *        while the user is composing the message.
 * @state:   The typing state received
 *
 * Receive a typing message from a remote user.  Either PURPLE_IM_TYPING
 * or PURPLE_IM_TYPED.  If the user has stopped typing then use
 * purple_serv_got_typing_stopped instead.
 *
 * @todo Could probably move this into the conversation API.
 */
void purple_serv_got_typing(PurpleConnection *gc, const char *name, int timeout,
					 PurpleIMTypingState state);

/**
 * purple_serv_got_typing_stopped:
 *
 * @todo Could probably move this into the conversation API.
 */
void purple_serv_got_typing_stopped(PurpleConnection *gc, const char *name);

/**
 * purple_serv_got_im:
 * @gc:     The connection on which the typing message was received.
 * @who:    The username of the buddy that sent the message.
 * @msg:    The actual message received.
 * @flags:  The flags applicable to this message.
 *
 * This function is called by the protocol when it receives an IM message.
 */
void purple_serv_got_im(PurpleConnection *gc, const char *who, const char *msg,
				 PurpleMessageFlags flags, time_t mtime);

/**
 * purple_serv_join_chat:
 * @data: The hash function should be g_str_hash() and the equal
 *             function should be g_str_equal().
 */
void purple_serv_join_chat(PurpleConnection *gc, GHashTable *data);

/**
 * purple_serv_reject_chat:
 * @data: The hash function should be g_str_hash() and the equal
 *             function should be g_str_equal().
 */
void purple_serv_reject_chat(PurpleConnection *gc, GHashTable *data);

/**
 * purple_serv_got_chat_invite:
 * @gc:      The connection on which the invite arrived.
 * @name:    The name of the chat you're being invited to.
 * @who:     The username of the person inviting the account.
 * @message: The optional invite message.
 * @data:    The components necessary if you want to call purple_serv_join_chat().
 *                The hash function should be g_str_hash() and the equal
 *                function should be g_str_equal().
 *
 * Called by a protocol when an account is invited into a chat.
 */
void purple_serv_got_chat_invite(PurpleConnection *gc, const char *name,
						  const char *who, const char *message,
						  GHashTable *data);

/**
 * purple_serv_got_joined_chat:
 * @gc:   The connection on which the chat was joined.
 * @id:   The id of the chat, assigned by the protocol.
 * @name: The name of the chat.
 *
 * Called by a protocol when an account has joined a chat.
 *
 * Returns:     The resulting conversation
 */
PurpleChatConversation *purple_serv_got_joined_chat(PurpleConnection *gc,
									   int id, const char *name);
/**
 * purple_serv_got_join_chat_failed:
 * @gc:      The connection on which chat joining failed
 * @data:    The components passed to purple_serv_join_chat() originally.
 *                The hash function should be g_str_hash() and the equal
 *                function should be g_str_equal().
 *
 * Called by a protocol when an attempt to join a chat via purple_serv_join_chat()
 * fails.
 */
void purple_serv_got_join_chat_failed(PurpleConnection *gc, GHashTable *data);

/**
 * purple_serv_got_chat_left:
 * @g:  The connection on which the chat was left.
 * @id: The id of the chat, as assigned by the protocol.
 *
 * Called by a protocol when an account has left a chat.
 */
void purple_serv_got_chat_left(PurpleConnection *g, int id);

/**
 * purple_serv_got_chat_in:
 * @g:       The connection on which the message was received.
 * @id:      The id of the chat, as assigned by the protocol.
 * @who:     The name of the user who sent the message.
 * @flags:   The flags of the message.
 * @message: The message received in the chat.
 * @mtime:   The time when the message was received.
 *
 * Called by a protocol when a message has been received in a chat.
 */
void purple_serv_got_chat_in(PurpleConnection *g, int id, const char *who,
					  PurpleMessageFlags flags, const char *message, time_t mtime);

/**
 * purple_serv_send_file:
 * @g:      The connection on which the message was received.
 * @who:    The name of the user to who send the file.
 * @file:   The filename to send.
 *
 * Send a filename to a given contact.
 */
void purple_serv_send_file(PurpleConnection *gc, const char *who, const char *file);

G_END_DECLS

#endif /* _PURPLE_SERVER_H_ */

