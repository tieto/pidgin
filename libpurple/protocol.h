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

#ifndef _PURPLE_PROTOCOL_H_
#define _PURPLE_PROTOCOL_H_
/**
 * SECTION:protocol
 * @section_id: libpurple-protocol
 * @short_description: <filename>protocol.h</filename>
 * @title: Protocol Object and Interfaces
 */

#define PURPLE_TYPE_PROTOCOL            (purple_protocol_get_type())
#define PURPLE_PROTOCOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocol))
#define PURPLE_PROTOCOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))
#define PURPLE_IS_PROTOCOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL))
#define PURPLE_IS_PROTOCOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PROTOCOL))
#define PURPLE_PROTOCOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))

typedef struct _PurpleProtocol PurpleProtocol;
typedef struct _PurpleProtocolClass PurpleProtocolClass;

#include "account.h"
#include "accountopt.h"
#include "buddyicon.h"
#include "buddylist.h"
#include "connection.h"
#include "conversations.h"
#include "debug.h"
#include "xfer.h"
#include "imgstore.h"
#include "media.h"
#include "notify.h"
#include "plugins.h"
#include "roomlist.h"
#include "status.h"
#include "whiteboard.h"

/**
 * PurpleProtocolOverrideFlags:
 *
 * Flags to indicate what base protocol's data a derived protocol wants to
 * override.
 *
 * See purple_protocol_override().
 */
typedef enum /*< flags >*/
{
    PURPLE_PROTOCOL_OVERRIDE_USER_SPLITS      = 1 << 1,
    PURPLE_PROTOCOL_OVERRIDE_PROTOCOL_OPTIONS = 1 << 2,
    PURPLE_PROTOCOL_OVERRIDE_ICON_SPEC        = 1 << 3,
} PurpleProtocolOverrideFlags;

/**
 * PurpleProtocol:
 * @id:              Protocol ID
 * @name:            Translated name of the protocol
 * @options:         Protocol options
 * @user_splits:     A GList of PurpleAccountUserSplit
 * @account_options: A GList of PurpleAccountOption
 * @icon_spec:       The icon spec.
 * @whiteboard_ops:  Whiteboard operations
 *
 * Represents an instance of a protocol registered with the protocols
 * subsystem. Protocols must initialize the members to appropriate values.
 */
struct _PurpleProtocol
{
	GObject gparent;

	/*< public >*/
	const char *id;
	const char *name;

	PurpleProtocolOptions options;

	GList *user_splits;
	GList *account_options;

	PurpleBuddyIconSpec *icon_spec;
	PurpleWhiteboardOps *whiteboard_ops;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleProtocolClass:
 * @login:        Log in to the server.
 * @close:        Close connection with the server.
 * @status_types: Returns a list of #PurpleStatusType which exist for this
 *                account; and must add at least the offline and online states.
 * @list_icon:    Returns the base icon name for the given buddy and account. If
 *                buddy is %NULL and the account is non-%NULL, it will return
 *                the name to use for the account's icon. If both are %NULL, it
 *                will return the name to use for the protocol's icon.
 *
 * The base class for all protocols.
 *
 * All protocol types must implement the methods in this class.
 */
/* If adding new methods to this class, ensure you add checks for them in
   purple_protocols_add().
*/
struct _PurpleProtocolClass
{
	GObjectClass parent_class;

	void (*login)(PurpleAccount *);

	void (*close)(PurpleConnection *);

	GList *(*status_types)(PurpleAccount *account);

	const char *(*list_icon)(PurpleAccount *account, PurpleBuddy *buddy);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#define PURPLE_TYPE_PROTOCOL_CLIENT_IFACE     (purple_protocol_client_iface_get_type())

typedef struct _PurpleProtocolClientIface PurpleProtocolClientIface;

/**
 * PurpleProtocolClientIface:
 * @get_actions:     Returns the actions the protocol can perform. These will
 *                   show up in the Accounts menu, under a submenu with the name
 *                   of the account.
 * @list_emblem:     Fills the four <type>char**</type>'s with string
 *                   identifiers for "emblems" that the UI will interpret and
 *                   display as relevant
 * @status_text:     Gets a short string representing this buddy's status. This
 *                   will be shown on the buddy list.
 * @tooltip_text:    Allows the protocol to add text to a buddy's tooltip.
 * @blist_node_menu: Returns a list of #PurpleMenuAction structs, which
 *                   represent extra actions to be shown in (for example) the
 *                   right-click menu for @node.
 * @normalize: Convert the username @who to its canonical form. Also checks for
 *             validity.
 *             <sbr/>For example, AIM treats "fOo BaR" and "foobar" as the same
 *             user; this function should return the same normalized string for
 *             both of those. On the other hand, both of these are invalid for
 *             protocols with number-based usernames, so function should return
 *             %NULL in such case.
 *             <sbr/>@account: The account the username is related to. Can be
 *                             %NULL.
 *             <sbr/>@who:     The username to convert.
 *             <sbr/>Returns:  Normalized username, or %NULL, if it's invalid.
 * @offline_message: Checks whether offline messages to @buddy are supported.
 *                   <sbr/>Returns: %TRUE if @buddy can be sent messages while
 *                                  they are offline, or %FALSE if not.
 * @get_account_text_table: This allows protocols to specify additional strings
 *                          to be used for various purposes. The idea is to
 *                          stuff a bunch of strings in this hash table instead
 *                          of expanding the struct for every addition. This
 *                          hash table is allocated every call and
 *                          <emphasis>MUST</emphasis> be unrefed by the caller.
 *                          <sbr/>@account: The account to specify.  This can be
 *                                          %NULL.
 *                          <sbr/>Returns:  The protocol's string hash table.
 *                                          The hash table should be destroyed
 *                                          by the caller when it's no longer
 *                                          needed.
 * @get_moods: Returns an array of #PurpleMood's, with the last one having
 *             "mood" set to %NULL.
 * @get_max_message_size: Gets the maximum message size in bytes for the
 *                        conversation.
 *                        <sbr/>It may depend on connection-specific or
 *                        conversation-specific variables, like channel or
 *                        buddy's name length.
 *                        <sbr/>This value is intended for plaintext message,
 *                              the exact value may be lower because of:
 *                        <sbr/> - used newlines (some protocols count them as
 *                                 more than one byte),
 *                        <sbr/> - formatting,
 *                        <sbr/> - used special characters.
 *                        <sbr/>@conv:   The conversation to query, or NULL to
 *                                       get safe minimum for the protocol.
 *                        <sbr/>Returns: Maximum message size, 0 if unspecified,
 *                                       -1 for infinite.
 *
 * The protocol client interface.
 *
 * This interface provides a gateway between purple and the protocol.
 */
struct _PurpleProtocolClientIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	GList *(*get_actions)(PurpleConnection *);

	const char *(*list_emblem)(PurpleBuddy *buddy);

	char *(*status_text)(PurpleBuddy *buddy);

	void (*tooltip_text)(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info,
						 gboolean full);

	GList *(*blist_node_menu)(PurpleBlistNode *node);

	void (*buddy_free)(PurpleBuddy *);

	void (*convo_closed)(PurpleConnection *, const char *who);

	const char *(*normalize)(const PurpleAccount *account, const char *who);

	PurpleChat *(*find_blist_chat)(PurpleAccount *account, const char *name);

	gboolean (*offline_message)(const PurpleBuddy *buddy);

	GHashTable *(*get_account_text_table)(PurpleAccount *account);

	PurpleMood *(*get_moods)(PurpleAccount *account);

	gssize (*get_max_message_size)(PurpleConversation *conv);
};

#define PURPLE_PROTOCOL_HAS_CLIENT_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_CLIENT_IFACE))
#define PURPLE_PROTOCOL_GET_CLIENT_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_CLIENT_IFACE, \
                                               PurpleProtocolClientIface))

#define PURPLE_TYPE_PROTOCOL_SERVER_IFACE     (purple_protocol_server_iface_get_type())

typedef struct _PurpleProtocolServerIface PurpleProtocolServerIface;

/**
 * PurpleProtocolServerIface:
 * @register_user:   New user registration
 * @unregister_user: Remove the user from the server. The account can either be
 *                   connected or disconnected. After the removal is finished,
 *                   the connection will stay open and has to be closed!
 * @get_info:        Should arrange for purple_notify_userinfo() to be called
 *                   with @who's user info.
 * @add_buddy:       Add a buddy to a group on the server.
 *                   <sbr/>This protocol function may be called in situations in
 *                   which the buddy is already in the specified group. If the
 *                   protocol supports authorization and the user is not already
 *                   authorized to see the status of @buddy, @add_buddy should
 *                   request authorization.
 *                   <sbr/>If authorization is required, then use the supplied
 *                   invite message.
 * @keepalive:       If implemented, this will be called regularly for this
 *                   protocol's active connections. You'd want to do this if you
 *                   need to repeatedly send some kind of keepalive packet to
 *                   the server to avoid being disconnected. ("Regularly" is
 *                   defined by <literal>KEEPALIVE_INTERVAL</literal> in
 *                   <filename>libpurple/connection.c</filename>.)
 * @alias_buddy:     Save/store buddy's alias on server list/roster
 * @group_buddy:     Change a buddy's group on a server list/roster
 * @rename_group:    Rename a group on a server list/roster
 * @set_buddy_icon:  Set the buddy icon for the given connection to @img. The
 *                   protocol does <emphasis>NOT</emphasis> own a reference to
 *                   @img; if it needs one, it must #purple_imgstore_ref(@img)
 *                   itself.
 * @send_raw:        For use in plugins that may understand the underlying
 *                   protocol
 * @set_public_alias: Set the user's "friendly name" (or alias or nickname or
 *                    whatever term you want to call it) on the server. The
 *                    protocol should call @success_cb or @failure_cb
 *                    <emphasis>asynchronously</emphasis> (if it knows
 *                    immediately that the set will fail, call one of the
 *                    callbacks from an idle/0-second timeout) depending on if
 *                    the nickname is set successfully. See
 *                    purple_account_set_public_alias().
 *                    <sbr/>@gc:    The connection for which to set an alias
 *                    <sbr/>@alias: The new server-side alias/nickname for this
 *                                  account, or %NULL to unset the
 *                                  alias/nickname (or return it to a
 *                                  protocol-specific "default").
 *                    <sbr/>@success_cb: Callback to be called if the public
 *                                       alias is set
 *                    <sbr/>@failure_cb: Callback to be called if setting the
 *                                       public alias fails
 * @get_public_alias: Retrieve the user's "friendly name" as set on the server.
 *                    The protocol should call @success_cb or @failure_cb
 *                    <emphasis>asynchronously</emphasis> (even if it knows
 *                    immediately that the get will fail, call one of the
 *                    callbacks from an idle/0-second timeout) depending on if
 *                    the nickname is retrieved. See
 *                    purple_account_get_public_alias().
 *                    <sbr/>@gc:         The connection for which to retireve
 *                                       the alias
 *                    <sbr/>@success_cb: Callback to be called with the
 *                                       retrieved alias
 *                    <sbr/>@failure_cb: Callback to be called if the protocol
 *                                       is unable to retrieve the alias
 *
 * The protocol server interface.
 *
 * This interface provides a gateway between purple and the protocol's server.
 */
struct _PurpleProtocolServerIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	void (*register_user)(PurpleAccount *);

	void (*unregister_user)(PurpleAccount *, PurpleAccountUnregistrationCb cb,
							void *user_data);

	void (*set_info)(PurpleConnection *, const char *info);

	void (*get_info)(PurpleConnection *, const char *who);

	void (*set_status)(PurpleAccount *account, PurpleStatus *status);

	void (*set_idle)(PurpleConnection *, int idletime);

	void (*change_passwd)(PurpleConnection *, const char *old_pass,
						  const char *new_pass);

	void (*add_buddy)(PurpleConnection *pc, PurpleBuddy *buddy,
					  PurpleGroup *group, const char *message);

	void (*add_buddies)(PurpleConnection *pc, GList *buddies, GList *groups,
						const char *message);

	void (*remove_buddy)(PurpleConnection *, PurpleBuddy *buddy,
						 PurpleGroup *group);

	void (*remove_buddies)(PurpleConnection *, GList *buddies, GList *groups);

	void (*keepalive)(PurpleConnection *);

	void (*alias_buddy)(PurpleConnection *, const char *who,
						const char *alias);

	void (*group_buddy)(PurpleConnection *, const char *who,
					const char *old_group, const char *new_group);

	void (*rename_group)(PurpleConnection *, const char *old_name,
					 PurpleGroup *group, GList *moved_buddies);

	void (*set_buddy_icon)(PurpleConnection *, PurpleStoredImage *img);

	void (*remove_group)(PurpleConnection *gc, PurpleGroup *group);

	int (*send_raw)(PurpleConnection *gc, const char *buf, int len);

	void (*set_public_alias)(PurpleConnection *gc, const char *alias,
	                         PurpleSetPublicAliasSuccessCallback success_cb,
	                         PurpleSetPublicAliasFailureCallback failure_cb);

	void (*get_public_alias)(PurpleConnection *gc,
	                         PurpleGetPublicAliasSuccessCallback success_cb,
	                         PurpleGetPublicAliasFailureCallback failure_cb);
};

#define PURPLE_PROTOCOL_HAS_SERVER_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_SERVER_IFACE))
#define PURPLE_PROTOCOL_GET_SERVER_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_SERVER_IFACE, \
                                               PurpleProtocolServerIface))

#define PURPLE_TYPE_PROTOCOL_IM_IFACE     (purple_protocol_im_iface_get_type())

typedef struct _PurpleProtocolIMIface PurpleProtocolIMIface;

/**
 * PurpleProtocolIMIface:
 * @send:        This protocol function should return a positive value on
 *               success. If the message is too big to be sent, return
 *               <literal>-E2BIG</literal>. If the account is not connected,
 *               return <literal>-ENOTCONN</literal>. If the protocol is unable
 *               to send the message for another reason, return some other
 *               negative value. You can use one of the valid #errno values, or
 *               just big something. If the message should not be echoed to the
 *               conversation window, return 0.
 * @send_typing: If this protocol requires the #PURPLE_IM_TYPING message to be
 *               sent repeatedly to signify that the user is still typing, then
 *               the protocol should return the number of seconds to wait before
 *               sending a subsequent notification. Otherwise the protocol
 *               should return 0.
 *
 * The protocol IM interface.
 *
 * This interface provides callbacks needed by protocols that implement IMs.
 */
struct _PurpleProtocolIMIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	int  (*send)(PurpleConnection *, const char *who,
					const char *message,
					PurpleMessageFlags flags);

	unsigned int (*send_typing)(PurpleConnection *, const char *name,
							PurpleIMTypingState state);
};

#define PURPLE_PROTOCOL_HAS_IM_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_IM_IFACE))
#define PURPLE_PROTOCOL_GET_IM_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_IM_IFACE, \
                                           PurpleProtocolIMIface))

#define PURPLE_TYPE_PROTOCOL_CHAT_IFACE     (purple_protocol_chat_iface_get_type())

typedef struct _PurpleProtocolChatIface PurpleProtocolChatIface;

/**
 * PurpleProtocolChatIface:
 * @info: Returns a list of #PurpleProtocolChatEntry structs, which represent
 *        information required by the protocol to join a chat. libpurple will
 *        call join_chat along with the information filled by the user.
 *        <sbr/>Returns: A list of #PurpleProtocolChatEntry's
 * @info_defaults: Returns a hashtable which maps #PurpleProtocolChatEntry
 *                 struct identifiers to default options as strings based on
 *                 @chat_name. The resulting hashtable should be created with
 *                 #g_hash_table_new_full(#g_str_hash, #g_str_equal, %NULL,
 *                 #g_free). Use @get_name if you instead need to extract a chat
 *                 name from a hashtable.
 *                 <sbr/>@chat_name: The chat name to be turned into components
 *                 <sbr/>Returns: Hashtable containing the information extracted
 *                                from @chat_name
 * @join: Called when the user requests joining a chat. Should arrange for
 *        serv_got_joined_chat() to be called.
 *        <sbr/>@components: A hashtable containing information required to join
 *                           the chat as described by the entries returned by
 *                           @info. It may also be called when accepting an
 *                           invitation, in which case this matches the data
 *                           parameter passed to serv_got_chat_invite().
 * @reject: Called when the user refuses a chat invitation.
 *          <sbr/>@components: A hashtable containing information required to
 *                            join the chat as passed to serv_got_chat_invite().
 * @get_name: Returns a chat name based on the information in components. Use
 *            @info_defaults if you instead need to generate a hashtable from a
 *            chat name.
 *            <sbr/>@components: A hashtable containing information about the
 *                               chat.
 * @invite: Invite a user to join a chat.
 *          <sbr/>@id:      The id of the chat to invite the user to.
 *          <sbr/>@message: A message displayed to the user when the invitation
 *                          is received.
 *          <sbr/>@who:     The name of the user to send the invation to.
 * @leave: Called when the user requests leaving a chat.
 *         <sbr/>@id: The id of the chat to leave
 * @whisper: Send a whisper to a user in a chat.
 *           <sbr/>@id:      The id of the chat.
 *           <sbr/>@who:     The name of the user to send the whisper to.
 *           <sbr/>@message: The message of the whisper.
 * @send: Send a message to a chat.
 *              <sbr/>This protocol function should return a positive value on
 *              success. If the message is too big to be sent, return
 *              <literal>-E2BIG</literal>. If the account is not connected,
 *              return <literal>-ENOTCONN</literal>. If the protocol is unable
 *              to send the message for another reason, return some other
 *              negative value. You can use one of the valid #errno values, or
 *              just big something.
 *              <sbr/>@id:      The id of the chat to send the message to.
 *              <sbr/>@message: The message to send to the chat.
 *              <sbr/>@flags:   A bitwise OR of #PurpleMessageFlags representing
 *                              message flags.
 *              <sbr/>Returns:  A positive number or 0 in case of success, a
 *                              negative error number in case of failure.
 * @get_user_real_name: Gets the real name of a participant in a chat. For
 *                      example, on XMPP this turns a chat room nick
 *                      <literal>foo</literal> into
 *                      <literal>room\@server/foo</literal>.
 *                      <sbr/>@gc:  the connection on which the room is.
 *                      <sbr/>@id:  the ID of the chat room.
 *                      <sbr/>@who: the nickname of the chat participant.
 *                      <sbr/>Returns: the real name of the participant. This
 *                                     string must be freed by the caller.
 *
 * The protocol chat interface.
 *
 * This interface provides callbacks needed by protocols that implement chats.
 */
struct _PurpleProtocolChatIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	GList *(*info)(PurpleConnection *);

	GHashTable *(*info_defaults)(PurpleConnection *, const char *chat_name);

	void (*join)(PurpleConnection *, GHashTable *components);

	void (*reject)(PurpleConnection *, GHashTable *components);

	char *(*get_name)(GHashTable *components);

	void (*invite)(PurpleConnection *, int id,
					const char *message, const char *who);

	void (*leave)(PurpleConnection *, int id);

	void (*whisper)(PurpleConnection *, int id,
					 const char *who, const char *message);

	int  (*send)(PurpleConnection *, int id, const char *message,
					  PurpleMessageFlags flags);

	char *(*get_user_real_name)(PurpleConnection *gc, int id, const char *who);

	void (*set_topic)(PurpleConnection *gc, int id, const char *topic);
};

#define PURPLE_PROTOCOL_HAS_CHAT_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_CHAT_IFACE))
#define PURPLE_PROTOCOL_GET_CHAT_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_CHAT_IFACE, \
                                             PurpleProtocolChatIface))

#define PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE     (purple_protocol_privacy_iface_get_type())

typedef struct _PurpleProtocolPrivacyIface PurpleProtocolPrivacyIface;

/**
 * PurpleProtocolPrivacyIface:
 *
 * The protocol privacy interface.
 *
 * This interface provides privacy callbacks such as to permit/deny users.
 */
struct _PurpleProtocolPrivacyIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	void (*add_permit)(PurpleConnection *, const char *name);

	void (*add_deny)(PurpleConnection *, const char *name);

	void (*rem_permit)(PurpleConnection *, const char *name);

	void (*rem_deny)(PurpleConnection *, const char *name);

	void (*set_permit_deny)(PurpleConnection *);
};

#define PURPLE_PROTOCOL_HAS_PRIVACY_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE))
#define PURPLE_PROTOCOL_GET_PRIVACY_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE, \
                                                PurpleProtocolPrivacyIface))

#define PURPLE_TYPE_PROTOCOL_XFER_IFACE     (purple_protocol_xfer_iface_get_type())

typedef struct _PurpleProtocolXferIface PurpleProtocolXferIface;

/**
 * PurpleProtocolXferIface:
 *
 * The protocol file transfer interface.
 *
 * This interface provides file transfer callbacks for the protocol.
 */
struct _PurpleProtocolXferIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	gboolean (*can_receive)(PurpleConnection *, const char *who);

	void (*send)(PurpleConnection *, const char *who,
					  const char *filename);

	PurpleXfer *(*new_xfer)(PurpleConnection *, const char *who);
};

#define PURPLE_PROTOCOL_HAS_XFER_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_XFER_IFACE))
#define PURPLE_PROTOCOL_GET_XFER_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_XFER_IFACE, \
                                             PurpleProtocolXferIface))

#define PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE     (purple_protocol_roomlist_iface_get_type())

typedef struct _PurpleProtocolRoomlistIface PurpleProtocolRoomlistIface;

/**
 * PurpleProtocolRoomlistIface:
 *
 * The protocol roomlist interface.
 *
 * This interface provides callbacks for room listing.
 */
struct _PurpleProtocolRoomlistIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	PurpleRoomlist *(*get_list)(PurpleConnection *gc);

	void (*cancel)(PurpleRoomlist *list);

	void (*expand_category)(PurpleRoomlist *list,
						 PurpleRoomlistRoom *category);

	/* room list serialize */
	char *(*room_serialize)(PurpleRoomlistRoom *room);
};

#define PURPLE_PROTOCOL_HAS_ROOMLIST_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE))
#define PURPLE_PROTOCOL_GET_ROOMLIST_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE, \
                                                 PurpleProtocolRoomlistIface))

#define PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE     (purple_protocol_attention_iface_get_type())

typedef struct _PurpleProtocolAttentionIface PurpleProtocolAttentionIface;

/**
 * PurpleProtocolAttentionIface:
 *
 * The protocol attention interface.
 *
 * This interface provides attention API for sending and receiving
 * zaps/nudges/buzzes etc.
 */
struct _PurpleProtocolAttentionIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	gboolean (*send)(PurpleConnection *gc, const char *username,
							   guint type);

	GList *(*get_types)(PurpleAccount *acct);
};

#define PURPLE_PROTOCOL_HAS_ATTENTION_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE))
#define PURPLE_PROTOCOL_GET_ATTENTION_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE, \
                                                  PurpleProtocolAttentionIface))

#define PURPLE_TYPE_PROTOCOL_MEDIA_IFACE     (purple_protocol_media_iface_get_type())

typedef struct _PurpleProtocolMediaIface PurpleProtocolMediaIface;

/**
 * PurpleProtocolMediaIface:
 * @initiate_session: Initiate a media session with the given contact.
 *                    <sbr/>@account: The account to initiate the media session
 *                                    on.
 *                    <sbr/>@who: The remote user to initiate the session with.
 *                    <sbr/>@type: The type of media session to initiate.
 *                    <sbr/>Returns: %TRUE if the call succeeded else %FALSE.
 *                                   (Doesn't imply the media session or stream
 *                                   will be successfully created)
 * @get_caps: Checks to see if the given contact supports the given type of
 *            media session.
 *            <sbr/>@account: The account the contact is on.
 *            <sbr/>@who: The remote user to check for media capability with.
 *            <sbr/>Returns: The media caps the contact supports.
 *
 * The protocol media interface.
 *
 * This interface provides callbacks for media sessions on the protocol.
 */
struct _PurpleProtocolMediaIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	gboolean (*initiate_session)(PurpleAccount *account, const char *who,
					PurpleMediaSessionType type);

	PurpleMediaCaps (*get_caps)(PurpleAccount *account,
					  const char *who);
};

#define PURPLE_PROTOCOL_HAS_MEDIA_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_MEDIA_IFACE))
#define PURPLE_PROTOCOL_GET_MEDIA_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_MEDIA_IFACE, \
                                              PurpleProtocolMediaIface))

#define PURPLE_TYPE_PROTOCOL_FACTORY_IFACE     (purple_protocol_factory_iface_get_type())

typedef struct _PurpleProtocolFactoryIface PurpleProtocolFactoryIface;

/**
 * PurpleProtocolFactoryIface:
 * @connection_new: Creates a new protocol-specific connection object that
 *                  inherits #PurpleConnection.
 * @roomlist_new:   Creates a new protocol-specific room list object that
 *                  inherits #PurpleRoomlist.
 * @whiteboard_new: Creates a new protocol-specific whiteboard object that
 *                  inherits #PurpleWhiteboard.
 * @xfer_new:       Creates a new protocol-specific file transfer object that
 *                  inherits #PurpleXfer.
 *
 * The protocol factory interface.
 *
 * This interface provides callbacks for construction of protocol-specific
 * subclasses of some purple objects.
 */
struct _PurpleProtocolFactoryIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	PurpleConnection *(*connection_new)(PurpleProtocol *protocol,
	                                    PurpleAccount *account,
	                                    const char *password);

	PurpleRoomlist *(*roomlist_new)(PurpleAccount *account);

	PurpleWhiteboard *(*whiteboard_new)(PurpleAccount *account,
	                                    const char *who, int state);

	PurpleXfer *(*xfer_new)(PurpleAccount *account, PurpleXferType type,
	                        const char *who);
};

#define PURPLE_PROTOCOL_HAS_FACTORY_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_FACTORY_IFACE))
#define PURPLE_PROTOCOL_GET_FACTORY_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_FACTORY_IFACE, \
                                                PurpleProtocolFactoryIface))

/**
 * PURPLE_PROTOCOL_IMPLEMENTS:
 * @protocol: The protocol in which to check
 * @IFACE:    The interface name in caps. e.g. <literal>CLIENT_IFACE</literal>
 * @func:     The function to check
 *
 * Returns: %TRUE if a protocol implements a function in an interface,
 *          %FALSE otherwise.
 */
#define PURPLE_PROTOCOL_IMPLEMENTS(protocol, IFACE, func) \
	(PURPLE_PROTOCOL_HAS_##IFACE(protocol) && \
	 PURPLE_PROTOCOL_GET_##IFACE(protocol)->func != NULL)

G_BEGIN_DECLS

/**************************************************************************/
/* Protocol Object API                                                    */
/**************************************************************************/

/**
 * purple_protocol_get_type:
 *
 * Returns: The #GType for #PurpleProtocol.
 */
GType purple_protocol_get_type(void);

/**
 * purple_protocol_get_id:
 * @protocol: The protocol.
 *
 * Returns the ID of a protocol.
 *
 * Returns: The ID of the protocol.
 */
const char *purple_protocol_get_id(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_name:
 * @protocol: The protocol.
 *
 * Returns the translated name of a protocol.
 *
 * Returns: The translated name of the protocol.
 */
const char *purple_protocol_get_name(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_options:
 * @protocol: The protocol.
 *
 * Returns the options of a protocol.
 *
 * Returns: The options of the protocol.
 */
PurpleProtocolOptions purple_protocol_get_options(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_user_splits:
 * @protocol: The protocol.
 *
 * Returns the user splits of a protocol.
 *
 * Returns: The user splits of the protocol.
 */
GList *purple_protocol_get_user_splits(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_account_options:
 * @protocol: The protocol.
 *
 * Returns the account options for a protocol.
 *
 * Returns: The account options for the protocol.
 */
GList *purple_protocol_get_account_options(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_icon_spec:
 * @protocol: The protocol.
 *
 * Returns the icon spec of a protocol.
 *
 * Returns: The icon spec of the protocol.
 */
PurpleBuddyIconSpec *purple_protocol_get_icon_spec(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_whiteboard_ops:
 * @protocol: The protocol.
 *
 * Returns the whiteboard ops of a protocol.
 *
 * Returns: The whiteboard ops of the protocol.
 */
PurpleWhiteboardOps *purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol);

/**
 * purple_protocol_override:
 * @protocol: The protocol instance.
 * @flags:    What instance data to delete.
 *
 * Lets derived protocol types override the base type's instance data, such as
 * protocol options, user splits, icon spec, etc.
 * This function is called in the *_init() function of your derived protocol,
 * to delete the parent type's data so you can define your own.
 */
void purple_protocol_override(PurpleProtocol *protocol,
		PurpleProtocolOverrideFlags flags);

/**************************************************************************/
/* Protocol Class API                                                     */
/**************************************************************************/

void purple_protocol_class_login(PurpleProtocol *, PurpleAccount *);

void purple_protocol_class_close(PurpleProtocol *, PurpleConnection *);

GList *purple_protocol_class_status_types(PurpleProtocol *,
		PurpleAccount *account);

const char *purple_protocol_class_list_icon(PurpleProtocol *,
		PurpleAccount *account, PurpleBuddy *buddy);

/**************************************************************************/
/* Protocol Client Interface API                                          */
/**************************************************************************/

/**
 * purple_protocol_client_iface_get_type:
 *
 * Returns: The #GType for the protocol client interface.
 */
GType purple_protocol_client_iface_get_type(void);

GList *purple_protocol_client_iface_get_actions(PurpleProtocol *,
		PurpleConnection *);

const char *purple_protocol_client_iface_list_emblem(PurpleProtocol *,
		PurpleBuddy *buddy);

char *purple_protocol_client_iface_status_text(PurpleProtocol *,
		PurpleBuddy *buddy);

void purple_protocol_client_iface_tooltip_text(PurpleProtocol *,
		PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full);

GList *purple_protocol_client_iface_blist_node_menu(PurpleProtocol *,
		PurpleBlistNode *node);

void purple_protocol_client_iface_buddy_free(PurpleProtocol *, PurpleBuddy *);

void purple_protocol_client_iface_convo_closed(PurpleProtocol *,
		PurpleConnection *, const char *who);

const char *purple_protocol_client_iface_normalize(PurpleProtocol *,
		const PurpleAccount *account, const char *who);

PurpleChat *purple_protocol_client_iface_find_blist_chat(PurpleProtocol *,
		PurpleAccount *account, const char *name);

gboolean purple_protocol_client_iface_offline_message(PurpleProtocol *,
		const PurpleBuddy *buddy);

GHashTable *purple_protocol_client_iface_get_account_text_table(PurpleProtocol *,
		PurpleAccount *account);

PurpleMood *purple_protocol_client_iface_get_moods(PurpleProtocol *,
		PurpleAccount *account);

gssize purple_protocol_client_iface_get_max_message_size(PurpleProtocol *,
		PurpleConversation *conv);

/**************************************************************************/
/* Protocol Server Interface API                                          */
/**************************************************************************/

/**
 * purple_protocol_server_iface_get_type:
 *
 * Returns: The #GType for the protocol server interface.
 */
GType purple_protocol_server_iface_get_type(void);

void purple_protocol_server_iface_register_user(PurpleProtocol *,
		PurpleAccount *);

void purple_protocol_server_iface_unregister_user(PurpleProtocol *,
		PurpleAccount *, PurpleAccountUnregistrationCb cb, void *user_data);

void purple_protocol_server_iface_set_info(PurpleProtocol *, PurpleConnection *,
		const char *info);

void purple_protocol_server_iface_get_info(PurpleProtocol *, PurpleConnection *,
		const char *who);

void purple_protocol_server_iface_set_status(PurpleProtocol *,
		PurpleAccount *account, PurpleStatus *status);

void purple_protocol_server_iface_set_idle(PurpleProtocol *, PurpleConnection *,
		int idletime);

void purple_protocol_server_iface_change_passwd(PurpleProtocol *,
		PurpleConnection *, const char *old_pass, const char *new_pass);

void purple_protocol_server_iface_add_buddy(PurpleProtocol *,
		PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group,
		const char *message);

void purple_protocol_server_iface_add_buddies(PurpleProtocol *,
		PurpleConnection *pc, GList *buddies, GList *groups,
		const char *message);

void purple_protocol_server_iface_remove_buddy(PurpleProtocol *,
		PurpleConnection *, PurpleBuddy *buddy, PurpleGroup *group);

void purple_protocol_server_iface_remove_buddies(PurpleProtocol *,
		PurpleConnection *, GList *buddies, GList *groups);

void purple_protocol_server_iface_keepalive(PurpleProtocol *,
		PurpleConnection *);

void purple_protocol_server_iface_alias_buddy(PurpleProtocol *,
		PurpleConnection *, const char *who, const char *alias);

void purple_protocol_server_iface_group_buddy(PurpleProtocol *,
		PurpleConnection *, const char *who, const char *old_group,
		const char *new_group);

void purple_protocol_server_iface_rename_group(PurpleProtocol *,
		PurpleConnection *, const char *old_name, PurpleGroup *group,
		GList *moved_buddies);

void purple_protocol_server_iface_set_buddy_icon(PurpleProtocol *,
		PurpleConnection *, PurpleStoredImage *img);

void purple_protocol_server_iface_remove_group(PurpleProtocol *,
		PurpleConnection *gc, PurpleGroup *group);

int purple_protocol_server_iface_send_raw(PurpleProtocol *,
		PurpleConnection *gc, const char *buf, int len);

void purple_protocol_server_iface_set_public_alias(PurpleProtocol *,
		PurpleConnection *gc, const char *alias,
		PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb);

void purple_protocol_server_iface_get_public_alias(PurpleProtocol *,
		PurpleConnection *gc, PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb);

/**************************************************************************/
/* Protocol IM Interface API                                              */
/**************************************************************************/

/**
 * purple_protocol_im_iface_get_type:
 *
 * Returns: The #GType for the protocol IM interface.
 */
GType purple_protocol_im_iface_get_type(void);

int purple_protocol_im_iface_send(PurpleProtocol *, PurpleConnection *, 
		const char *who, const char *message, PurpleMessageFlags flags);

unsigned int purple_protocol_im_iface_send_typing(PurpleProtocol *,
		PurpleConnection *, const char *name, PurpleIMTypingState state);

/**************************************************************************/
/* Protocol Chat Interface API                                            */
/**************************************************************************/

/**
 * purple_protocol_chat_iface_get_type:
 *
 * Returns: The #GType for the protocol chat interface.
 */
GType purple_protocol_chat_iface_get_type(void);

GList *purple_protocol_chat_iface_info(PurpleProtocol *,
		PurpleConnection *);

GHashTable *purple_protocol_chat_iface_info_defaults(PurpleProtocol *,
		PurpleConnection *, const char *chat_name);

void purple_protocol_chat_iface_join(PurpleProtocol *, PurpleConnection *,
		GHashTable *components);

void purple_protocol_chat_iface_reject(PurpleProtocol *,
		PurpleConnection *, GHashTable *components);

char *purple_protocol_chat_iface_get_name(PurpleProtocol *,
		GHashTable *components);

void purple_protocol_chat_iface_invite(PurpleProtocol *,
		PurpleConnection *, int id, const char *message, const char *who);

void purple_protocol_chat_iface_leave(PurpleProtocol *, PurpleConnection *,
		int id);

void purple_protocol_chat_iface_whisper(PurpleProtocol *,
		PurpleConnection *, int id, const char *who, const char *message);

int  purple_protocol_chat_iface_send(PurpleProtocol *, PurpleConnection *,
		int id, const char *message, PurpleMessageFlags flags);

char *purple_protocol_chat_iface_get_user_real_name(PurpleProtocol *,
		PurpleConnection *gc, int id, const char *who);

void purple_protocol_chat_iface_set_topic(PurpleProtocol *,
		PurpleConnection *gc, int id, const char *topic);

/**************************************************************************/
/* Protocol Privacy Interface API                                         */
/**************************************************************************/

/**
 * purple_protocol_privacy_iface_get_type:
 *
 * Returns: The #GType for the protocol privacy interface.
 */
GType purple_protocol_privacy_iface_get_type(void);

void purple_protocol_privacy_iface_add_permit(PurpleProtocol *,
		PurpleConnection *, const char *name);

void purple_protocol_privacy_iface_add_deny(PurpleProtocol *,
		PurpleConnection *, const char *name);

void purple_protocol_privacy_iface_rem_permit(PurpleProtocol *,
		PurpleConnection *, const char *name);

void purple_protocol_privacy_iface_rem_deny(PurpleProtocol *,
		PurpleConnection *, const char *name);

void purple_protocol_privacy_iface_set_permit_deny(PurpleProtocol *,
		PurpleConnection *);

/**************************************************************************/
/* Protocol Xfer Interface API                                            */
/**************************************************************************/

/**
 * purple_protocol_xfer_iface_get_type:
 *
 * Returns: The #GType for the protocol xfer interface.
 */
GType purple_protocol_xfer_iface_get_type(void);

gboolean purple_protocol_xfer_iface_can_receive(PurpleProtocol *,
		PurpleConnection *, const char *who);

void purple_protocol_xfer_iface_send(PurpleProtocol *, PurpleConnection *,
		const char *who, const char *filename);

PurpleXfer *purple_protocol_xfer_iface_new_xfer(PurpleProtocol *,
		PurpleConnection *, const char *who);

/**************************************************************************/
/* Protocol Roomlist Interface API                                        */
/**************************************************************************/

/**
 * purple_protocol_roomlist_iface_get_type:
 *
 * Returns: The #GType for the protocol roomlist interface.
 */
GType purple_protocol_roomlist_iface_get_type(void);

PurpleRoomlist *purple_protocol_roomlist_iface_get_list(PurpleProtocol *,
		PurpleConnection *gc);

void purple_protocol_roomlist_iface_cancel(PurpleProtocol *,
		PurpleRoomlist *list);

void purple_protocol_roomlist_iface_expand_category(PurpleProtocol *,
		PurpleRoomlist *list, PurpleRoomlistRoom *category);

char *purple_protocol_roomlist_iface_room_serialize(PurpleProtocol *,
		PurpleRoomlistRoom *room);

/**************************************************************************/
/* Protocol Attention Interface API                                       */
/**************************************************************************/

/**
 * purple_protocol_attention_iface_get_type:
 *
 * Returns: The #GType for the protocol attention interface.
 */
GType purple_protocol_attention_iface_get_type(void);

gboolean purple_protocol_attention_iface_send(PurpleProtocol *,
		PurpleConnection *gc, const char *username, guint type);

GList *purple_protocol_attention_iface_get_types(PurpleProtocol *,
		PurpleAccount *acct);

/**************************************************************************/
/* Protocol Media Interface API                                           */
/**************************************************************************/

/**
 * purple_protocol_media_iface_get_type:
 *
 * Returns: The #GType for the protocol media interface.
 */
GType purple_protocol_media_iface_get_type(void);

gboolean purple_protocol_media_iface_initiate_session(PurpleProtocol *,
		PurpleAccount *account, const char *who, PurpleMediaSessionType type);

PurpleMediaCaps purple_protocol_media_iface_get_caps(PurpleProtocol *,
		PurpleAccount *account, const char *who);

/**************************************************************************/
/* Protocol Factory Interface API                                         */
/**************************************************************************/

/**
 * purple_protocol_factory_iface_get_type:
 *
 * Returns: The #GType for the protocol factory interface.
 */
GType purple_protocol_factory_iface_get_type(void);

PurpleConnection *purple_protocol_factory_iface_connection_new(PurpleProtocol *,
		PurpleAccount *account, const char *password);

PurpleRoomlist *purple_protocol_factory_iface_roomlist_new(PurpleProtocol *,
		PurpleAccount *account);

PurpleWhiteboard *purple_protocol_factory_iface_whiteboard_new(PurpleProtocol *,
		PurpleAccount *account, const char *who, int state);

PurpleXfer *purple_protocol_factory_iface_xfer_new(PurpleProtocol *,
		PurpleAccount *account, PurpleXferType type, const char *who);

G_END_DECLS

#endif /* _PROTOCOL_H_ */
