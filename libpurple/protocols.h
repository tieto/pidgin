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

#ifndef _PURPLE_PROTOCOLS_H_
#define _PURPLE_PROTOCOLS_H_
/**
 * SECTION:protocols
 * @section_id: libpurple-protocols
 * @short_description: <filename>protocols.h</filename>
 * @title: Protocols Subsystem API
 * @see_also: <link linkend="chapter-signals-protocol">Protocol signals</link>
 */

#define PURPLE_PROTOCOLS_DOMAIN  (g_quark_from_static_string("protocols"))

#define PURPLE_TYPE_PROTOCOL_ACTION  (purple_protocol_action_get_type())

typedef struct _PurpleProtocolAction PurpleProtocolAction;
typedef void (*PurpleProtocolActionCallback)(PurpleProtocolAction *);

#define PURPLE_TYPE_ATTENTION_TYPE  (purple_attention_type_get_type())

/**
 * PurpleAttentionType:
 *
 * Represents "nudges" and "buzzes" that you may send to a buddy to attract
 * their attention (or vice-versa).
 */
typedef struct _PurpleAttentionType PurpleAttentionType;

/**************************************************************************/
/* Basic Protocol Information                                             */
/**************************************************************************/

typedef struct _PurpleProtocolChatEntry PurpleProtocolChatEntry;

/**
 * PurpleProtocolOptions:
 * @OPT_PROTO_UNIQUE_CHATNAME: User names are unique to a chat and are not
 *           shared between rooms.<sbr/>
 *           XMPP lets you choose what name you want in chats, so it shouldn't
 *           be pulling the aliases from the buddy list for the chat list; it
 *           gets annoying.
 * @OPT_PROTO_CHAT_TOPIC: Chat rooms have topics.<sbr/>
 *           IRC and XMPP support this.
 * @OPT_PROTO_NO_PASSWORD: Don't require passwords for sign-in.<sbr/>
 *           Zephyr doesn't require passwords, so there's no need for a
 *           password prompt.
 * @OPT_PROTO_MAIL_CHECK: Notify on new mail.<sbr/>
 *           MSN and Yahoo notify you when you have new mail.
 * @OPT_PROTO_PASSWORD_OPTIONAL: Allow passwords to be optional.<sbr/>
 *           Passwords in IRC are optional, and are needed for certain
 *           functionality.
 * @OPT_PROTO_USE_POINTSIZE: Allows font size to be specified in sane point
 *           size.<sbr/>
 *           Probably just XMPP and Y!M
 * @OPT_PROTO_REGISTER_NOSCREENNAME: Set the Register button active even when
 *           the username has not been specified.<sbr/>
 *           Gadu-Gadu doesn't need a username to register new account (because
 *           usernames are assigned by the server).
 * @OPT_PROTO_SLASH_COMMANDS_NATIVE: Indicates that slash commands are native
 *           to this protocol.<sbr/>
 *           Used as a hint that unknown commands should not be sent as
 *           messages.
 * @OPT_PROTO_INVITE_MESSAGE: Indicates that this protocol supports sending a
 *           user-supplied message along with an invitation.
 * @OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE: Indicates that this protocol
 *           supports sending a user-supplied message along with an
 *           authorization acceptance.
 * @OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE: Indicates that this protocol
 *           supports sending a user-supplied message along with an
 *           authorization denial.
 *
 * Protocol options
 *
 * These should all be stuff that some protocols can do and others can't.
 */
typedef enum  /*< flags >*/
{
	OPT_PROTO_UNIQUE_CHATNAME               = 0x00000004,
	OPT_PROTO_CHAT_TOPIC                    = 0x00000008,
	OPT_PROTO_NO_PASSWORD                   = 0x00000010,
	OPT_PROTO_MAIL_CHECK                    = 0x00000020,
	OPT_PROTO_PASSWORD_OPTIONAL             = 0x00000080,
	OPT_PROTO_USE_POINTSIZE                 = 0x00000100,
	OPT_PROTO_REGISTER_NOSCREENNAME         = 0x00000200,
	OPT_PROTO_SLASH_COMMANDS_NATIVE         = 0x00000400,
	OPT_PROTO_INVITE_MESSAGE                = 0x00000800,
	OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE = 0x00001000,
	OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE  = 0x00002000

} PurpleProtocolOptions;

#include "media.h"
#include "protocol.h"
#include "status.h"

#define PURPLE_TYPE_PROTOCOL_CHAT_ENTRY  (purple_protocol_chat_entry_get_type())

/**
 * PurpleProtocolChatEntry:
 * @label:      User-friendly name of the entry
 * @identifier: Used by the protocol to identify the option
 * @required:   True if it's required
 * @is_int:     True if the entry expects an integer
 * @min:        Minimum value in case of integer
 * @max:        Maximum value in case of integer
 * @secret:     True if the entry is secret (password)
 *
 * Represents an entry containing information that must be supplied by the
 * user when joining a chat.
 */
struct _PurpleProtocolChatEntry {
	const char *label;
	const char *identifier;
	gboolean required;
	gboolean is_int;
	int min;
	int max;
	gboolean secret;
};

/**
 * PurpleProtocolAction:
 *
 * Represents an action that the protocol can perform. This shows up in the
 * Accounts menu, under a submenu with the name of the account.
 */
struct _PurpleProtocolAction {
	char *label;
	PurpleProtocolActionCallback callback;
	PurpleConnection *connection;
	gpointer user_data;
};

G_BEGIN_DECLS

/**************************************************************************/
/* Attention Type API                                                     */
/**************************************************************************/

/**
 * purple_attention_type_get_type:
 *
 * Returns: The #GType for the #PurpleAttentionType boxed structure.
 */
GType purple_attention_type_get_type(void);

/**
 * purple_attention_type_new:
 * @ulname: A non-localized string that can be used by UIs in need of such
 *               non-localized strings.  This should be the same as @name,
 *               without localization.
 * @name: A localized string that the UI may display for the event. This
 *             should be the same string as @ulname, with localization.
 * @inc_desc: A localized description shown when the event is received.
 * @out_desc: A localized description shown when the event is sent.
 *
 * Creates a new #PurpleAttentionType object and sets its mandatory parameters.
 *
 * Returns: A pointer to the new object.
 */
PurpleAttentionType *purple_attention_type_new(const char *ulname, const char *name,
								const char *inc_desc, const char *out_desc);

/**
 * purple_attention_type_set_name:
 * @type: The attention type.
 * @name: The localized name that will be displayed by UIs. This should be
 *             the same string given as the unlocalized name, but with
 *             localization.
 *
 * Sets the displayed name of the attention-demanding event.
 */
void purple_attention_type_set_name(PurpleAttentionType *type, const char *name);

/**
 * purple_attention_type_set_incoming_desc:
 * @type: The attention type.
 * @desc: The localized description for incoming events.
 *
 * Sets the description of the attention-demanding event shown in  conversations
 * when the event is received.
 */
void purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const char *desc);

/**
 * purple_attention_type_set_outgoing_desc:
 * @type: The attention type.
 * @desc: The localized description for outgoing events.
 *
 * Sets the description of the attention-demanding event shown in conversations
 * when the event is sent.
 */
void purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const char *desc);

/**
 * purple_attention_type_set_icon_name:
 * @type: The attention type.
 * @name: The icon's name.
 *
 * Sets the name of the icon to display for the attention event; this is optional.
 *
 * Note: Icons are optional for attention events.
 */
void purple_attention_type_set_icon_name(PurpleAttentionType *type, const char *name);

/**
 * purple_attention_type_set_unlocalized_name:
 * @type: The attention type.
 * @ulname: The unlocalized name.  This should be the same string given as
 *               the localized name, but without localization.
 *
 * Sets the unlocalized name of the attention event; some UIs may need this,
 * thus it is required.
 */
void purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const char *ulname);

/**
 * purple_attention_type_get_name:
 * @type: The attention type.
 *
 * Get the attention type's name as displayed by the UI.
 *
 * Returns: The name.
 */
const char *purple_attention_type_get_name(const PurpleAttentionType *type);

/**
 * purple_attention_type_get_incoming_desc:
 * @type: The attention type.
 *
 * Get the attention type's description shown when the event is received.
 *
 * Returns: The description.
 */
const char *purple_attention_type_get_incoming_desc(const PurpleAttentionType *type);

/**
 * purple_attention_type_get_outgoing_desc:
 * @type: The attention type.
 *
 * Get the attention type's description shown when the event is sent.
 *
 * Returns: The description.
 */
const char *purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type);

/**
 * purple_attention_type_get_icon_name:
 * @type: The attention type.
 *
 * Get the attention type's icon name.
 *
 * Note: Icons are optional for attention events.
 *
 * Returns: The icon name or %NULL if unset/empty.
 */
const char *purple_attention_type_get_icon_name(const PurpleAttentionType *type);

/**
 * purple_attention_type_get_unlocalized_name:
 * @type: The attention type
 *
 * Get the attention type's unlocalized name; this is useful for some UIs.
 *
 * Returns: The unlocalized name.
 */
const char *purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type);

/**************************************************************************/
/* Protocol Action API                                                    */
/**************************************************************************/

/**
 * purple_protocol_action_get_type:
 *
 * Returns: The #GType for the #PurpleProtocolAction boxed structure.
 */
GType purple_protocol_action_get_type(void);

/**
 * purple_protocol_action_new:
 * @label:    The description of the action to show to the user.
 * @callback: (scope call): The callback to call when the user selects this
 *            action.
 *
 * Allocates and returns a new PurpleProtocolAction. Use this to add actions in
 * a list in the get_actions function of the protocol.
 */
PurpleProtocolAction *purple_protocol_action_new(const char* label,
		PurpleProtocolActionCallback callback);

/**
 * purple_protocol_action_free:
 * @action: The PurpleProtocolAction to free.
 *
 * Frees a PurpleProtocolAction
 */
void purple_protocol_action_free(PurpleProtocolAction *action);

/**************************************************************************/
/* Protocol Chat Entry API                                                */
/**************************************************************************/

/**
 * purple_protocol_chat_entry_get_type:
 *
 * Returns: The #GType for the #PurpleProtocolChatEntry boxed structure.
 */
GType purple_protocol_chat_entry_get_type(void);

/**************************************************************************/
/* Protocol API                                                           */
/**************************************************************************/

/**
 * purple_protocol_got_account_idle:
 * @account:   The account.
 * @idle:      The user's idle state.
 * @idle_time: The user's idle time.
 *
 * Notifies Purple that our account's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_account_idle(PurpleAccount *account, gboolean idle,
                                      time_t idle_time);

/**
 * purple_protocol_got_account_login_time:
 * @account:    The account the user is on.
 * @login_time: The user's log-in time.
 *
 * Notifies Purple of our account's log-in time.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_account_login_time(PurpleAccount *account,
                                            time_t login_time);

/**
 * purple_protocol_got_account_status:
 * @account:   The account the user is on.
 * @status_id: The status ID.
 * @...:       A NULL-terminated list of attribute IDs and values,
 *             beginning with the value for <literal>attr_id</literal>.
 *
 * Notifies Purple that our account's status has changed.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_account_status(PurpleAccount *account,
                                        const char *status_id, ...)
                                        G_GNUC_NULL_TERMINATED;

/**
 * purple_protocol_got_account_actions:
 * @account:   The account.
 *
 * Notifies Purple that our account's actions have changed. This is only
 * called after the initial connection. Emits the account-actions-changed
 * signal.
 *
 * This is meant to be called from protocols.
 *
 * See <link linkend="accounts-account-actions-changed"><literal>"account-actions-changed"</literal></link>
 */
void purple_protocol_got_account_actions(PurpleAccount *account);

/**
 * purple_protocol_got_user_idle:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @idle:      The user's idle state.
 * @idle_time: The user's idle time.  This is the time at
 *                  which the user became idle, in seconds since
 *                  the epoch.  If the protocol does not know this value
 *                  then it should pass 0.
 *
 * Notifies Purple that a buddy's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_user_idle(PurpleAccount *account, const char *name,
                                   gboolean idle, time_t idle_time);

/**
 * purple_protocol_got_user_login_time:
 * @account:    The account the user is on.
 * @name:       The name of the buddy.
 * @login_time: The user's log-in time.
 *
 * Notifies Purple of a buddy's log-in time.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_user_login_time(PurpleAccount *account,
                                         const char *name, time_t login_time);

/**
 * purple_protocol_got_user_status:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @status_id: The status ID.
 * @...:       A NULL-terminated list of attribute IDs and values,
 *             beginning with the value for <literal>attr_id</literal>.
 *
 * Notifies Purple that a buddy's status has been activated.
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_user_status(PurpleAccount *account, const char *name,
                                     const char *status_id, ...)
                                     G_GNUC_NULL_TERMINATED;

/**
 * purple_protocol_got_user_status_deactive:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @status_id: The status ID.
 *
 * Notifies libpurple that a buddy's status has been deactivated
 *
 * This is meant to be called from protocols.
 */
void purple_protocol_got_user_status_deactive(PurpleAccount *account,
                                              const char *name,
                                              const char *status_id);

/**
 * purple_protocol_change_account_status:
 * @account:    The account the user is on.
 * @old_status: The previous status.
 * @new_status: The status that was activated, or deactivated
 *                   (in the case of independent statuses).
 *
 * Informs the server that our account's status changed.
 */
void purple_protocol_change_account_status(PurpleAccount *account,
                                           PurpleStatus *old_status,
                                           PurpleStatus *new_status);

/**
 * purple_protocol_get_statuses:
 * @account: The account the user is on.
 * @presence: The presence for which we're going to get statuses
 *
 * Retrieves the list of stock status types from a protocol.
 *
 * Returns: (element-type PurpleStatus): List of statuses
 */
GList *purple_protocol_get_statuses(PurpleAccount *account,
                                    PurplePresence *presence);

/**
 * purple_protocol_send_attention:
 * @gc: The connection to send the message on.
 * @who: Whose attention to request.
 * @type_code: An index into the protocol's attention_types list determining the type
 *        of the attention request command to send. 0 if protocol only defines one
 *        (for example, Yahoo and MSN), but protocols are allowed to define more.
 *
 * Send an attention request message.
 *
 * Note that you can't send arbitrary PurpleAttentionType's, because there is
 * only a fixed set of attention commands.
 */
void purple_protocol_send_attention(PurpleConnection *gc, const char *who,
                                    guint type_code);

/**
 * purple_protocol_got_attention:
 * @gc: The connection that received the attention message.
 * @who: Who requested your attention.
 * @type_code: An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 *
 * Process an incoming attention message.
 */
void purple_protocol_got_attention(PurpleConnection *gc, const char *who,
                                   guint type_code);

/**
 * purple_protocol_got_attention_in_chat:
 * @gc: The connection that received the attention message.
 * @id: The chat id.
 * @who: Who requested your attention.
 * @type_code: An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 *
 * Process an incoming attention message in a chat.
 */
void purple_protocol_got_attention_in_chat(PurpleConnection *gc, int id,
                                           const char *who, guint type_code);

/**
 * purple_protocol_get_media_caps:
 * @account: The account the user is on.
 * @who: The name of the contact to check capabilities for.
 *
 * Determines if the contact supports the given media session type.
 *
 * Returns: The media caps the contact supports.
 */
PurpleMediaCaps purple_protocol_get_media_caps(PurpleAccount *account,
                                               const char *who);

/**
 * purple_protocol_initiate_media:
 * @account: The account the user is on.
 * @who: The name of the contact to start a session with.
 * @type: The type of media session to start.
 *
 * Initiates a media session with the given contact.
 *
 * Returns: TRUE if the call succeeded else FALSE. (Doesn't imply the media
 *          session or stream will be successfully created)
 */
gboolean purple_protocol_initiate_media(PurpleAccount *account,
                                        const char *who,
                                        PurpleMediaSessionType type);

/**
 * purple_protocol_got_media_caps:
 * @account: The account the user is on.
 * @who: The name of the contact for which capabilities have been received.
 *
 * Signals that the protocol received capabilities for the given contact.
 *
 * This function is intended to be used only by protocols.
 */
void purple_protocol_got_media_caps(PurpleAccount *account, const char *who);

/**
 * purple_protocol_get_max_message_size:
 * @protocol: The protocol to query.
 *
 * Gets the safe maximum message size in bytes for the protocol.
 *
 * See #PurpleProtocolClientIface's <literal>get_max_message_size</literal>.
 *
 * Returns: Maximum message size, 0 if unspecified, -1 for infinite.
 */
gssize
purple_protocol_get_max_message_size(PurpleProtocol *protocol);

/**************************************************************************/
/* Protocols API                                                          */
/**************************************************************************/

/**
 * purple_protocols_find:
 * @id: The protocol's ID.
 *
 * Finds a protocol by ID.
 */
PurpleProtocol *purple_protocols_find(const char *id);

/**
 * purple_protocols_add:
 * @protocol_type:  The type of the protocol to add.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *          will be set to the reason if adding fails.
 *
 * Adds a protocol to the list of protocols.
 *
 * Returns: The protocol instance if the protocol was added, else %NULL.
 */
PurpleProtocol *purple_protocols_add(GType protocol_type, GError **error);

/**
 * purple_protocols_remove:
 * @protocol:  The protocol to remove.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *          will be set to the reason if removing fails.
 *
 * Removes a protocol from the list of protocols. This will disconnect all
 * connected accounts using this protocol, and free the protocol's user splits
 * and protocol options.
 *
 * Returns: TRUE if the protocol was removed, else FALSE.
 */
gboolean purple_protocols_remove(PurpleProtocol *protocol, GError **error);

/**
 * purple_protocols_get_all:
 *
 * Returns a list of all loaded protocols.
 *
 * Returns: (element-type PurpleProtocol) (transfer container): A list of all
 *          loaded protocols.
 */
GList *purple_protocols_get_all(void);

/**************************************************************************/
/* Protocols Subsytem API                                                 */
/**************************************************************************/

/**
 * purple_protocols_init:
 *
 * Initializes the protocols subsystem.
 */
void purple_protocols_init(void);

/**
 * purple_protocols_get_handle:
 *
 * Returns the protocols subsystem handle.
 *
 * Returns: The protocols subsystem handle.
 */
void *purple_protocols_get_handle(void);

/**
 * purple_protocols_uninit:
 *
 * Uninitializes the protocols subsystem.
 */
void purple_protocols_uninit(void);

G_END_DECLS

#endif /* _PROTOCOLS_H_ */
