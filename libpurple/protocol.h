/**
 * @file protocol.h PurpleProtocol and PurpleProtocolInterface API
 * @ingroup core
 */

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

#define PURPLE_TYPE_PROTOCOL                (purple_protocol_get_type())
#define PURPLE_PROTOCOL(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocol))
#define PURPLE_PROTOCOL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))
#define PURPLE_IS_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL))
#define PURPLE_IS_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PROTOCOL))
#define PURPLE_PROTOCOL_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))
#define PURPLE_PROTOCOL_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocolInterface))

/** @copydoc _PurpleProtocol */
typedef struct _PurpleProtocol PurpleProtocol;
/** @copydoc _PurpleProtocolClass */
typedef struct _PurpleProtocolClass PurpleProtocolClass;

/** @copydoc _PurpleProtocolInterface */
typedef struct _PurpleProtocolInterface PurpleProtocolInterface;

#include "account.h"
#include "accountopt.h"
#include "buddylist.h"
#include "connection.h"
#include "conversations.h"
#include "ft.h"
#include "imgstore.h"
#include "roomlist.h"
#include "whiteboard.h"

/**
 * Represents an instance of a protocol registered with the protocols
 * subsystem.
 */
struct _PurpleProtocol
{
	/*< private >*/
	GObject gparent;
};

/**
 * The base class for all protocols.
 *
 * Protocols must set the members of this class to appropriate values upon
 * class initialization.
 */
struct _PurpleProtocolClass
{
	/*< private >*/
	GObjectClass parent_class;

	const char *id;                 /**< Protocol ID */
	const char *name;               /**< Translated name of the protocol */

	PurpleProtocolOptions options;  /**< Protocol options */

	GList *user_splits;             /**< A GList of PurpleAccountUserSplit */
	GList *protocol_options;        /**< A GList of PurpleAccountOption */

	PurpleBuddyIconSpec icon_spec;  /**< The icon spec. */

	PurpleWhiteboardPrplOps *whiteboard_protocol_ops;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * The protocol interface.
 *
 * Every protocol implements this interface. It is the gateway between purple
 * and the protocol's functions. Many of these callbacks can be NULL. If a
 * callback must be implemented, it has a comment indicating so.
 */
struct _PurpleProtocolInterface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/**
	 * Returns the actions the protocol can perform. These will show up in the
	 * Accounts menu, under a submenu with the name of the account.
	 */
	GList *(*get_actions)(PurpleConnection *);

	/**
	 * Returns the base icon name for the given buddy and account.
	 * If buddy is NULL and the account is non-NULL, it will return the
	 * name to use for the account's icon. If both are NULL, it will
	 * return the name to use for the protocol's icon.
	 *
	 * This must be implemented.
	 */
	const char *(*list_icon)(PurpleAccount *account, PurpleBuddy *buddy);

	/**
	 * Fills the four char**'s with string identifiers for "emblems"
	 * that the UI will interpret and display as relevant
	 */
	const char *(*list_emblem)(PurpleBuddy *buddy);

	/**
	 * Gets a short string representing this buddy's status.  This will
	 * be shown on the buddy list.
	 */
	char *(*status_text)(PurpleBuddy *buddy);

	/**
	 * Allows the protocol to add text to a buddy's tooltip.
	 */
	void (*tooltip_text)(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info,
						 gboolean full);

	/**
	 * Returns a list of #PurpleStatusType which exist for this account;
	 * this must be implemented, and must add at least the offline and
	 * online states.
	 */
	GList *(*status_types)(PurpleAccount *account);

	/**
	 * Returns a list of #PurpleMenuAction structs, which represent extra
	 * actions to be shown in (for example) the right-click menu for @a
	 * node.
	 */
	GList *(*blist_node_menu)(PurpleBlistNode *node);

	/**
	 * Returns a list of #PurpleProtocolChatEntry structs, which represent
	 * information required by the protocol to join a chat. libpurple will
	 * call join_chat along with the information filled by the user.
	 *
	 * @return A list of #PurpleProtocolChatEntry structs
	 */
	GList *(*chat_info)(PurpleConnection *);

	/**
	 * Returns a hashtable which maps #PurpleProtocolChatEntry struct
	 * identifiers to default options as strings based on chat_name. The
	 * resulting hashtable should be created with
	 * g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);. Use
	 * #get_chat_name if you instead need to extract a chat name from a
	 * hashtable.
	 *
	 * @param chat_name The chat name to be turned into components
	 * @return Hashtable containing the information extracted from chat_name
	 */
	GHashTable *(*chat_info_defaults)(PurpleConnection *,
									  const char *chat_name);

	/* All the server-related functions */

	/** This must be implemented. */
	void (*login)(PurpleAccount *);

	/** This must be implemented. */
	void (*close)(PurpleConnection *);

	/**
	 * This protocol function should return a positive value on success.
	 * If the message is too big to be sent, return -E2BIG.  If
	 * the account is not connected, return -ENOTCONN.  If the
	 * protocol is unable to send the message for another reason, return
	 * some other negative value.  You can use one of the valid
	 * errno values, or just big something.  If the message should
	 * not be echoed to the conversation window, return 0.
	 */
	int  (*send_im)(PurpleConnection *, const char *who,
					const char *message,
					PurpleMessageFlags flags);

	void (*set_info)(PurpleConnection *, const char *info);

	/**
	 * @return If this protocol requires the PURPLE_IM_TYPING message to
	 *         be sent repeatedly to signify that the user is still
	 *         typing, then the protocol should return the number of
	 *         seconds to wait before sending a subsequent notification.
	 *         Otherwise the protocol should return 0.
	 */
	unsigned int (*send_typing)(PurpleConnection *, const char *name,
								PurpleIMTypingState state);

	/**
	 * Should arrange for purple_notify_userinfo() to be called with
	 * @a who's user info.
	 */
	void (*get_info)(PurpleConnection *, const char *who);
	void (*set_status)(PurpleAccount *account, PurpleStatus *status);

	void (*set_idle)(PurpleConnection *, int idletime);
	void (*change_passwd)(PurpleConnection *, const char *old_pass,
						  const char *new_pass);

	/**
	 * Add a buddy to a group on the server.
	 *
	 * This protocol function may be called in situations in which the buddy is
	 * already in the specified group. If the protocol supports
	 * authorization and the user is not already authorized to see the
	 * status of \a buddy, \a add_buddy should request authorization.
	 *
	 * If authorization is required, then use the supplied invite message.
	 */
	void (*add_buddy)(PurpleConnection *pc, PurpleBuddy *buddy,
					  PurpleGroup *group, const char *message);
	void (*add_buddies)(PurpleConnection *pc, GList *buddies, GList *groups,
						const char *message);
	void (*remove_buddy)(PurpleConnection *, PurpleBuddy *buddy,
						 PurpleGroup *group);
	void (*remove_buddies)(PurpleConnection *, GList *buddies, GList *groups);
	void (*add_permit)(PurpleConnection *, const char *name);
	void (*add_deny)(PurpleConnection *, const char *name);
	void (*rem_permit)(PurpleConnection *, const char *name);
	void (*rem_deny)(PurpleConnection *, const char *name);
	void (*set_permit_deny)(PurpleConnection *);

	/**
	 * Called when the user requests joining a chat. Should arrange for
	 * #serv_got_joined_chat to be called.
	 *
	 * @param components A hashtable containing information required to
	 *                   join the chat as described by the entries returned
	 *                   by #chat_info. It may also be called when accepting
	 *                   an invitation, in which case this matches the
	 *                   data parameter passed to #serv_got_chat_invite.
	 */
	void (*join_chat)(PurpleConnection *, GHashTable *components);

	/**
	 * Called when the user refuses a chat invitation.
	 *
	 * @param components A hashtable containing information required to
	 *                   join the chat as passed to #serv_got_chat_invite.
	 */
	void (*reject_chat)(PurpleConnection *, GHashTable *components);

	/**
	 * Returns a chat name based on the information in components. Use
	 * #chat_info_defaults if you instead need to generate a hashtable
	 * from a chat name.
	 *
	 * @param components A hashtable containing information about the chat.
	 */
	char *(*get_chat_name)(GHashTable *components);

	/**
	 * Invite a user to join a chat.
	 *
	 * @param id      The id of the chat to invite the user to.
	 * @param message A message displayed to the user when the invitation
	 *                is received.
	 * @param who     The name of the user to send the invation to.
	 */
	void (*chat_invite)(PurpleConnection *, int id,
						const char *message, const char *who);
	/**
	 * Called when the user requests leaving a chat.
	 *
	 * @param id The id of the chat to leave
	 */
	void (*chat_leave)(PurpleConnection *, int id);

	/**
	 * Send a whisper to a user in a chat.
	 *
	 * @param id      The id of the chat.
	 * @param who     The name of the user to send the whisper to.
	 * @param message The message of the whisper.
	 */
	void (*chat_whisper)(PurpleConnection *, int id,
						 const char *who, const char *message);

	/**
	 * Send a message to a chat.
	 * This protocol function should return a positive value on success.
	 * If the message is too big to be sent, return -E2BIG.  If
	 * the account is not connected, return -ENOTCONN.  If the
	 * protocol is unable to send the message for another reason, return
	 * some other negative value.  You can use one of the valid
	 * errno values, or just big something.
	 *
	 * @param id      The id of the chat to send the message to.
	 * @param message The message to send to the chat.
	 * @param flags   A bitwise OR of #PurpleMessageFlags representing
	 *                message flags.
	 * @return 	  A positive number or 0 in case of success,
	 *                a negative error number in case of failure.
	 */
	int  (*chat_send)(PurpleConnection *, int id, const char *message,
					  PurpleMessageFlags flags);

	/** If implemented, this will be called regularly for this protocol's
	 *  active connections.  You'd want to do this if you need to repeatedly
	 *  send some kind of keepalive packet to the server to avoid being
	 *  disconnected.  ("Regularly" is defined by
	 *  <code>KEEPALIVE_INTERVAL</code> in <tt>libpurple/connection.c</tt>.)
	 */
	void (*keepalive)(PurpleConnection *);

	/** new user registration */
	void (*register_user)(PurpleAccount *);

	/** Remove the user from the server.  The account can either be
	 * connected or disconnected. After the removal is finished, the
	 * connection will stay open and has to be closed!
	 */
	void (*unregister_user)(PurpleAccount *, PurpleAccountUnregistrationCb cb,
							void *user_data);

	/**
	 * @deprecated Use #PurpleProtocol.get_info instead.
	 */
	void (*get_cb_info)(PurpleConnection *, int, const char *who);

	/** save/store buddy's alias on server list/roster */
	void (*alias_buddy)(PurpleConnection *, const char *who,
						const char *alias);

	/** change a buddy's group on a server list/roster */
	void (*group_buddy)(PurpleConnection *, const char *who,
						const char *old_group, const char *new_group);

	/** rename a group on a server list/roster */
	void (*rename_group)(PurpleConnection *, const char *old_name,
						 PurpleGroup *group, GList *moved_buddies);

	void (*buddy_free)(PurpleBuddy *);

	void (*convo_closed)(PurpleConnection *, const char *who);

	/**
	 * Convert the username @a who to its canonical form. Also checks for
	 * validity.
	 *
	 * For example, AIM treats "fOo BaR" and "foobar" as the same user; this
	 * function should return the same normalized string for both of those.
	 * On the other hand, both of these are invalid for protocols with
	 * number-based usernames, so function should return NULL in such case.
	 *
	 * @param account  The account the username is related to. Can
	 *                 be NULL.
	 * @param who      The username to convert.
	 * @return         Normalized username, or NULL, if it's invalid.
	 */
	const char *(*normalize)(const PurpleAccount *account, const char *who);

	/**
	 * Set the buddy icon for the given connection to @a img.  The protocol
	 * does NOT own a reference to @a img; if it needs one, it must
	 * #purple_imgstore_ref(@a img) itself.
	 */
	void (*set_buddy_icon)(PurpleConnection *, PurpleStoredImage *img);

	void (*remove_group)(PurpleConnection *gc, PurpleGroup *group);

	/** Gets the real name of a participant in a chat.  For example, on
	 *  XMPP this turns a chat room nick <tt>foo</tt> into
	 *  <tt>room\@server/foo</tt>
	 *  @param gc  the connection on which the room is.
	 *  @param id  the ID of the chat room.
	 *  @param who the nickname of the chat participant.
	 *  @return    the real name of the participant.  This string must be
	 *             freed by the caller.
	 */
	char *(*get_cb_real_name)(PurpleConnection *gc, int id, const char *who);

	void (*set_chat_topic)(PurpleConnection *gc, int id, const char *topic);

	PurpleChat *(*find_blist_chat)(PurpleAccount *account, const char *name);

	/* room listing protocol callbacks */
	PurpleRoomlist *(*roomlist_get_list)(PurpleConnection *gc);
	void (*roomlist_cancel)(PurpleRoomlist *list);
	void (*roomlist_expand_category)(PurpleRoomlist *list,
									 PurpleRoomlistRoom *category);

	/* file transfer callbacks */
	gboolean (*can_receive_file)(PurpleConnection *, const char *who);
	void (*send_file)(PurpleConnection *, const char *who,
					  const char *filename);
	PurpleXfer *(*new_xfer)(PurpleConnection *, const char *who);

	/** Checks whether offline messages to @a buddy are supported.
	 *  @return @c TRUE if @a buddy can be sent messages while they are
	 *          offline, or @c FALSE if not.
	 */
	gboolean (*offline_message)(const PurpleBuddy *buddy);

	PurpleWhiteboardPrplOps *whiteboard_protocol_ops;

	/** For use in plugins that may understand the underlying protocol */
	int (*send_raw)(PurpleConnection *gc, const char *buf, int len);

	/* room list serialize */
	char *(*roomlist_room_serialize)(PurpleRoomlistRoom *room);

	/* Attention API for sending & receiving zaps/nudges/buzzes etc. */
	gboolean (*send_attention)(PurpleConnection *gc, const char *username,
							   guint type);
	GList *(*get_attention_types)(PurpleAccount *acct);

	/** This allows protocols to specify additional strings to be used for
	 * various purposes.  The idea is to stuff a bunch of strings in this hash
	 * table instead of expanding the struct for every addition.  This hash
	 * table is allocated every call and MUST be unrefed by the caller.
	 *
	 * @param account The account to specify.  This can be NULL.
	 * @return The protocol's string hash table. The hash table should be
	 *         destroyed by the caller when it's no longer needed.
	 */
	GHashTable *(*get_account_text_table)(PurpleAccount *account);

	/**
	 * Initiate a media session with the given contact.
	 *
	 * @param account The account to initiate the media session on.
	 * @param who The remote user to initiate the session with.
	 * @param type The type of media session to initiate.
	 * @return TRUE if the call succeeded else FALSE. (Doesn't imply the media
	 *         session or stream will be successfully created)
	 */
	gboolean (*initiate_media)(PurpleAccount *account, const char *who,
					PurpleMediaSessionType type);

	/**
	 * Checks to see if the given contact supports the given type of media
	 * session.
	 *
	 * @param account The account the contact is on.
	 * @param who The remote user to check for media capability with.
	 * @return The media caps the contact supports.
	 */
	PurpleMediaCaps (*get_media_caps)(PurpleAccount *account,
					  const char *who);

	/**
	 * Returns an array of "PurpleMood"s, with the last one having
	 * "mood" set to @c NULL.
	 */
	PurpleMood *(*get_moods)(PurpleAccount *account);

	/**
	 * Set the user's "friendly name" (or alias or nickname or
	 * whatever term you want to call it) on the server.  The
	 * protocol should call success_cb or failure_cb
	 * *asynchronously* (if it knows immediately that the set will fail,
	 * call one of the callbacks from an idle/0-second timeout) depending
	 * on if the nickname is set successfully.
	 *
	 * @param gc    The connection for which to set an alias
	 * @param alias The new server-side alias/nickname for this account,
	 *              or NULL to unset the alias/nickname (or return it to
	 *              a protocol-specific "default").
	 * @param success_cb Callback to be called if the public alias is set
	 * @param failure_cb Callback to be called if setting the public alias
	 *                   fails
	 * @see purple_account_set_public_alias
	 */
	void (*set_public_alias)(PurpleConnection *gc, const char *alias,
	                         PurpleSetPublicAliasSuccessCallback success_cb,
	                         PurpleSetPublicAliasFailureCallback failure_cb);
	/**
	 * Retrieve the user's "friendly name" as set on the server.
	 * The protocol should call success_cb or failure_cb
	 * *asynchronously* (even if it knows immediately that the get will fail,
	 * call one of the callbacks from an idle/0-second timeout) depending
	 * on if the nickname is retrieved.
	 *
	 * @param gc    The connection for which to retireve the alias
	 * @param success_cb Callback to be called with the retrieved alias
	 * @param failure_cb Callback to be called if the protocol is unable to
	 *                   retrieve the alias
	 * @see purple_account_get_public_alias
	 */
	void (*get_public_alias)(PurpleConnection *gc,
	                         PurpleGetPublicAliasSuccessCallback success_cb,
	                         PurpleGetPublicAliasFailureCallback failure_cb);

	/**
	 * Gets the maximum message size for the protocol. It may depend on
	 * connection-specific variables (like protocol version).
	 *
	 * This value is intended for plaintext message, the exact value may be
	 * lower because of:
	 *  - used newlines (some protocols count them as more than one byte),
	 *  - formatting,
	 *  - used special characters.
	 *
	 * @param gc The connection to query, or NULL to get safe minimum.
	 * @return   Maximum message size, or 0 if unspecified or infinite.
	 */
	gsize (*get_max_message_size)(PurpleConnection *gc);
};

/**
 * Defines a protocol type using the CamelCase type name of the protocol and
 * the function prefix for the *_get_type(), *_base_init(), *_base_finalize()
 * and *_interface_init() functions of the protocol.
 */
#define PURPLE_PROTOCOL_DEFINE(TypeName, func_prefix) \
	PURPLE_PROTOCOL_DEFINE_EXTENDED(TypeName, func_prefix, \
	                                PURPLE_TYPE_PROTOCOL, 0)

/** TODO register type dynamically (or statically if configured so)
 * Defines a protocol type using the CamelCase type name of the protocol, the
 * function prefix for the *_get_type(), *_base_init(), *_base_finalize() and
 * *_interface_init() functions, the base type, and the type flags.
 */
#define PURPLE_PROTOCOL_DEFINE_EXTENDED(TypeName, func_prefix, BaseType, TypeFlags) \
	GType func_prefix##_get_type(void) { \
		static GType type = 0; \
		if (type == 0) { \
			static const GTypeInfo info = { \
				.instance_size = sizeof(TypeName), \
				.class_size = sizeof(TypeName##Class), \
				.base_init = (GBaseInitFunc)func_prefix##_base_init, \
				.base_finalize = (GBaseFinalizeFunc)func_prefix##_base_finalize, \
			}; \
			static const GInterfaceInfo iface_info = { \
				.interface_init = (GInterfaceInitFunc)func_prefix##_interface_init, \
			}; \
			type = g_type_register_static(BaseType, #TypeName, \
				                          &info, TypeFlags); \
			g_type_add_interface_static(type, PURPLE_TYPE_PROTOCOL, &iface_info); \
		} \
		return type; \
	}

G_BEGIN_DECLS

/**************************************************************************/
/** @name Protocol API                                                    */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for #PurpleProtocol.
 */
GType purple_protocol_get_type(void);

/** TODO
 * Returns the ID of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The ID of the protocol.
 */
const char *purple_protocol_get_id(const PurpleProtocol *protocol);

/** TODO
 * Returns the translated name of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The translated name of the protocol.
 */
const char *purple_protocol_get_name(const PurpleProtocol *protocol);

/** TODO
 * Returns the options of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The options of the protocol.
 */
PurpleProtocolOptions purple_protocol_get_options(const PurpleProtocol *protocol);

/** TODO
 * Returns the user splits of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The user splits of the protocol.
 */
GList *purple_protocol_get_user_splits(const PurpleProtocol *protocol);

/** TODO
 * Returns the protocol options of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The protocol options of the protocol.
 */
GList *purple_protocol_get_protocol_options(const PurpleProtocol *protocol);

/** TODO
 * Returns the icon spec of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The icon spec of the protocol.
 */
PurpleBuddyIconSpec purple_protocol_get_icon_spec(const PurpleProtocol *protocol);

/** TODO
 * Returns the whiteboard ops of a protocol.
 *
 * @param protocol The protocol.
 *
 * @return The whiteboard ops of the protocol.
 */
PurpleWhiteboardPrplOps *purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol);

/**
 * Notifies Purple that our account's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account.
 * @param idle      The user's idle state.
 * @param idle_time The user's idle time.
 */
void purple_protocol_got_account_idle(PurpleAccount *account, gboolean idle,
                                      time_t idle_time);

/**
 * Notifies Purple of our account's log-in time.
 *
 * This is meant to be called from protocols.
 *
 * @param account    The account the user is on.
 * @param login_time The user's log-in time.
 */
void purple_protocol_got_account_login_time(PurpleAccount *account,
                                            time_t login_time);

/**
 * Notifies Purple that our account's status has changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param status_id The status ID.
 * @param ...       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @a attr_id.
 */
void purple_protocol_got_account_status(PurpleAccount *account,
                                        const char *status_id, ...)
                                        G_GNUC_NULL_TERMINATED;

/**
 * Notifies Purple that our account's actions have changed. This is only
 * called after the initial connection. Emits the account-actions-changed
 * signal.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account.
 *
 * @see account-actions-changed
 */
void purple_protocol_got_account_actions(PurpleAccount *account);

/**
 * Notifies Purple that a buddy's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param idle      The user's idle state.
 * @param idle_time The user's idle time.  This is the time at
 *                  which the user became idle, in seconds since
 *                  the epoch.  If the protocol does not know this value
 *                  then it should pass 0.
 */
void purple_protocol_got_user_idle(PurpleAccount *account, const char *name,
                                   gboolean idle, time_t idle_time);

/**
 * Notifies Purple of a buddy's log-in time.
 *
 * This is meant to be called from protocols.
 *
 * @param account    The account the user is on.
 * @param name       The name of the buddy.
 * @param login_time The user's log-in time.
 */
void purple_protocol_got_user_login_time(PurpleAccount *account,
                                         const char *name, time_t login_time);

/**
 * Notifies Purple that a buddy's status has been activated.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param status_id The status ID.
 * @param ...       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @a attr_id.
 */
void purple_protocol_got_user_status(PurpleAccount *account, const char *name,
                                     const char *status_id, ...)
                                     G_GNUC_NULL_TERMINATED;

/**
 * Notifies libpurple that a buddy's status has been deactivated
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param status_id The status ID.
 */
void purple_protocol_got_user_status_deactive(PurpleAccount *account,
                                              const char *name,
                                              const char *status_id);

/**
 * Informs the server that our account's status changed.
 *
 * @param account    The account the user is on.
 * @param old_status The previous status.
 * @param new_status The status that was activated, or deactivated
 *                   (in the case of independent statuses).
 */
void purple_protocol_change_account_status(PurpleAccount *account,
                                           PurpleStatus *old_status,
                                           PurpleStatus *new_status);

/**
 * Retrieves the list of stock status types from a protocol.
 *
 * @param account The account the user is on.
 * @param presence The presence for which we're going to get statuses
 *
 * @return List of statuses
 */
GList *purple_protocol_get_statuses(PurpleAccount *account,
                                    PurplePresence *presence);

/**
 * Send an attention request message.
 *
 * @param gc The connection to send the message on.
 * @param who Whose attention to request.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send. 0 if protocol only defines one (for example, Yahoo and
 *                  MSN), but some protocols define more (MySpaceIM).
 *
 * Note that you can't send arbitrary PurpleAttentionType's, because there is
 * only a fixed set of attention commands.
 */
void purple_protocol_send_attention(PurpleConnection *gc, const char *who,
                                    guint type_code);

/**
 * Process an incoming attention message.
 *
 * @param gc The connection that received the attention message.
 * @param who Who requested your attention.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 */
void purple_protocol_got_attention(PurpleConnection *gc, const char *who,
                                   guint type_code);

/**
 * Process an incoming attention message in a chat.
 *
 * @param gc The connection that received the attention message.
 * @param id The chat id.
 * @param who Who requested your attention.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 */
void purple_protocol_got_attention_in_chat(PurpleConnection *gc, int id,
                                           const char *who, guint type_code);

/**
 * Determines if the contact supports the given media session type.
 *
 * @param account The account the user is on.
 * @param who The name of the contact to check capabilities for.
 *
 * @return The media caps the contact supports.
 */
PurpleMediaCaps purple_protocol_get_media_caps(PurpleAccount *account,
                                               const char *who);

/**
 * Initiates a media session with the given contact.
 *
 * @param account The account the user is on.
 * @param who The name of the contact to start a session with.
 * @param type The type of media session to start.
 *
 * @return TRUE if the call succeeded else FALSE. (Doesn't imply the media
 *         session or stream will be successfully created)
 */
gboolean purple_protocol_initiate_media(PurpleAccount *account,
                                        const char *who,
                                        PurpleMediaSessionType type);

/**
 * Signals that the protocol received capabilities for the given contact.
 *
 * This function is intended to be used only by protocols.
 *
 * @param account The account the user is on.
 * @param who The name of the contact for which capabilities have been received.
 */
void purple_protocol_got_media_caps(PurpleAccount *account, const char *who);

/*@}*/

/* TODO */
/**************************************************************************/
/** @name Protocol Interface API                                          */
/**************************************************************************/
/*@{*/

/** @copydoc  _PurpleProtocolInterface::get_actions */
GList *purple_protocol_iface_get_actions(PurpleProtocol *, PurpleConnection *);

/** @copydoc  _PurpleProtocolInterface::list_icon */
const char *purple_protocol_iface_list_icon(PurpleProtocol *,
                                            PurpleAccount *account,
                                            PurpleBuddy *buddy);

/** @copydoc  _PurpleProtocolInterface::list_emblem */
const char *purple_protocol_iface_list_emblem(PurpleProtocol *,
                                              PurpleBuddy *buddy);

/** @copydoc  _PurpleProtocolInterface::status_text */
char *purple_protocol_iface_status_text(PurpleProtocol *, PurpleBuddy *buddy);

/** @copydoc  _PurpleProtocolInterface::tooltip_text */
void purple_protocol_iface_tooltip_text(PurpleProtocol *, PurpleBuddy *buddy,
                                        PurpleNotifyUserInfo *user_info,
                                        gboolean full);

/** @copydoc  _PurpleProtocolInterface::status_types */
GList *purple_protocol_iface_status_types(PurpleProtocol *,
                                          PurpleAccount *account);

/** @copydoc  _PurpleProtocolInterface::blist_node_menu */
GList *purple_protocol_iface_blist_node_menu(PurpleProtocol *,
                                             PurpleBlistNode *node);

/** @copydoc  _PurpleProtocolInterface::chat_info */
GList *purple_protocol_iface_chat_info(PurpleProtocol *, PurpleConnection *);

/** @copydoc  _PurpleProtocolInterface::chat_info_defaults */
GHashTable *purple_protocol_iface_chat_info_defaults(PurpleProtocol *,
                                                     PurpleConnection *,
                                                     const char *chat_name);

/** @copydoc  _PurpleProtocolInterface::login */
void purple_protocol_iface_login(PurpleProtocol *, PurpleAccount *);

/** @copydoc  _PurpleProtocolInterface::close */
void purple_protocol_iface_close(PurpleProtocol *, PurpleConnection *);

/** @copydoc  _PurpleProtocolInterface::send_im */
int  purple_protocol_iface_send_im(PurpleProtocol *, PurpleConnection *, 
                                   const char *who, const char *message,
                                   PurpleMessageFlags flags);

/** @copydoc  _PurpleProtocolInterface::set_info */
void purple_protocol_iface_set_info(PurpleProtocol *, PurpleConnection *,
                                    const char *info);

/** @copydoc  _PurpleProtocolInterface::send_typing */
unsigned int purple_protocol_iface_send_typing(PurpleProtocol *,
                                               PurpleConnection *,
                                               const char *name,
                                               PurpleIMTypingState state);

/** @copydoc  _PurpleProtocolInterface::get_info */
void purple_protocol_iface_get_info(PurpleProtocol *, PurpleConnection *,
                                    const char *who);

/** @copydoc  _PurpleProtocolInterface::set_status */
void purple_protocol_iface_set_status(PurpleProtocol *, PurpleAccount *account,
                                      PurpleStatus *status);

/** @copydoc  _PurpleProtocolInterface::set_idle */
void purple_protocol_iface_set_idle(PurpleProtocol *, PurpleConnection *,
                                    int idletime);

/** @copydoc  _PurpleProtocolInterface::change_passwd */
void purple_protocol_iface_change_passwd(PurpleProtocol *, PurpleConnection *,
                                         const char *old_pass,
                                         const char *new_pass);

/** @copydoc  _PurpleProtocolInterface::add_buddy */
void purple_protocol_iface_add_buddy(PurpleProtocol *, PurpleConnection *pc,
                                     PurpleBuddy *buddy, PurpleGroup *group,
                                     const char *message);

/** @copydoc  _PurpleProtocolInterface::add_buddies */
void purple_protocol_iface_add_buddies(PurpleProtocol *, PurpleConnection *pc,
                                       GList *buddies, GList *groups,
                                       const char *message);

/** @copydoc  _PurpleProtocolInterface::remove_buddy */
void purple_protocol_iface_remove_buddy(PurpleProtocol *, PurpleConnection *,
                                        PurpleBuddy *buddy, PurpleGroup *group);

/** @copydoc  _PurpleProtocolInterface::remove_buddies */
void purple_protocol_iface_remove_buddies(PurpleProtocol *, PurpleConnection *,
                                          GList *buddies, GList *groups);

/** @copydoc  _PurpleProtocolInterface::add_permit */
void purple_protocol_iface_add_permit(PurpleProtocol *, PurpleConnection *,
                                      const char *name);

/** @copydoc  _PurpleProtocolInterface::add_deny */
void purple_protocol_iface_add_deny(PurpleProtocol *, PurpleConnection *,
                                    const char *name);

/** @copydoc  _PurpleProtocolInterface::rem_permit */
void purple_protocol_iface_rem_permit(PurpleProtocol *, PurpleConnection *,
                                      const char *name);

/** @copydoc  _PurpleProtocolInterface::rem_deny */
void purple_protocol_iface_rem_deny(PurpleProtocol *, PurpleConnection *,
                                    const char *name);

/** @copydoc  _PurpleProtocolInterface::set_permit_deny */
void purple_protocol_iface_set_permit_deny(PurpleProtocol *,
                                           PurpleConnection *);

/** @copydoc  _PurpleProtocolInterface::join_chat */
void purple_protocol_iface_join_chat(PurpleProtocol *, PurpleConnection *,
                                     GHashTable *components);

/** @copydoc  _PurpleProtocolInterface::reject_chat */
void purple_protocol_iface_reject_chat(PurpleProtocol *, PurpleConnection *,
                                       GHashTable *components);

/** @copydoc  _PurpleProtocolInterface::get_chat_name */
char *purple_protocol_iface_get_chat_name(PurpleProtocol *,
                                          GHashTable *components);

/** @copydoc  _PurpleProtocolInterface::chat_invite */
void purple_protocol_iface_chat_invite(PurpleProtocol *, PurpleConnection *,
                                       int id, const char *message,
                                       const char *who);

/** @copydoc  _PurpleProtocolInterface::chat_leave */
void purple_protocol_iface_chat_leave(PurpleProtocol *, PurpleConnection *,
                                      int id);

/** @copydoc  _PurpleProtocolInterface::chat_whisper */
void purple_protocol_iface_chat_whisper(PurpleProtocol *, PurpleConnection *,
                                        int id, const char *who,
                                        const char *message);

/** @copydoc  _PurpleProtocolInterface::chat_send */
int  purple_protocol_iface_chat_send(PurpleProtocol *, PurpleConnection *,
                                     int id, const char *message,
                                     PurpleMessageFlags flags);

/** @copydoc  _PurpleProtocolInterface::keepalive */
void purple_protocol_iface_keepalive(PurpleProtocol *, PurpleConnection *);

/** @copydoc  _PurpleProtocolInterface::register_user */
void purple_protocol_iface_register_user(PurpleProtocol *, PurpleAccount *);

/** @copydoc  _PurpleProtocolInterface::unregister_user */
void purple_protocol_iface_unregister_user(PurpleProtocol *, PurpleAccount *,
                                     PurpleAccountUnregistrationCb cb,
                                     void *user_data);

/** @copydoc  _PurpleProtocolInterface::get_cb_info */
void purple_protocol_iface_get_cb_info(PurpleProtocol *, PurpleConnection *,
                                       int, const char *who);

/** @copydoc  _PurpleProtocolInterface::alias_buddy */
void purple_protocol_iface_alias_buddy(PurpleProtocol *, PurpleConnection *,
                                       const char *who, const char *alias);

/** @copydoc  _PurpleProtocolInterface::group_buddy */
void purple_protocol_iface_group_buddy(PurpleProtocol *, PurpleConnection *,
                                       const char *who, const char *old_group,
                                       const char *new_group);

/** @copydoc  _PurpleProtocolInterface::rename_group */
void purple_protocol_iface_rename_group(PurpleProtocol *, PurpleConnection *,
                                        const char *old_name,
                                        PurpleGroup *group,
                                        GList *moved_buddies);

/** @copydoc  _PurpleProtocolInterface::buddy_free */
void purple_protocol_iface_buddy_free(PurpleProtocol *, PurpleBuddy *);

/** @copydoc  _PurpleProtocolInterface::convo_closed */
void purple_protocol_iface_convo_closed(PurpleProtocol *, PurpleConnection *,
                                        const char *who);

/** @copydoc  _PurpleProtocolInterface::normalize */
const char *purple_protocol_iface_normalize(PurpleProtocol *,
                                            const PurpleAccount *account,
                                            const char *who);

/** @copydoc  _PurpleProtocolInterface::set_buddy_icon */
void purple_protocol_iface_set_buddy_icon(PurpleProtocol *, PurpleConnection *,
                                          PurpleStoredImage *img);

/** @copydoc  _PurpleProtocolInterface::remove_group */
void purple_protocol_iface_remove_group(PurpleProtocol *, PurpleConnection *gc,
                                        PurpleGroup *group);

/** @copydoc  _PurpleProtocolInterface::get_cb_real_name */
char *purple_protocol_iface_get_cb_real_name(PurpleProtocol *,
                                             PurpleConnection *gc, int id,
                                             const char *who);

/** @copydoc  _PurpleProtocolInterface::set_chat_topic */
void purple_protocol_iface_set_chat_topic(PurpleProtocol *,
                                          PurpleConnection *gc, int id,
                                          const char *topic);

/** @copydoc  _PurpleProtocolInterface::find_blist_chat */
PurpleChat *purple_protocol_iface_find_blist_chat(PurpleProtocol *,
                                                  PurpleAccount *account,
                                                  const char *name);

/** @copydoc  _PurpleProtocolInterface::roomlist_get_list */
PurpleRoomlist *purple_protocol_iface_roomlist_get_list(PurpleProtocol *,
                                                        PurpleConnection *gc);

/** @copydoc  _PurpleProtocolInterface::roomlist_cancel */
void purple_protocol_iface_roomlist_cancel(PurpleProtocol *,
                                           PurpleRoomlist *list);

/** @copydoc  _PurpleProtocolInterface::roomlist_expand_category */
void purple_protocol_iface_roomlist_expand_category(
                                                  PurpleProtocol *,
                                                  PurpleRoomlist *list,
                                                  PurpleRoomlistRoom *category);

/** @copydoc  _PurpleProtocolInterface::can_receive_file */
gboolean purple_protocol_iface_can_receive_file(PurpleProtocol *,
                                                PurpleConnection *,
                                                const char *who);

/** @copydoc  _PurpleProtocolInterface::send_file */
void purple_protocol_iface_send_file(PurpleProtocol *, PurpleConnection *,
                                     const char *who, const char *filename);

/** @copydoc  _PurpleProtocolInterface::new_xfer */
PurpleXfer *purple_protocol_iface_new_xfer(PurpleProtocol *, PurpleConnection *,
                                           const char *who);

/** @copydoc  _PurpleProtocolInterface::offline_message */
gboolean purple_protocol_iface_offline_message(PurpleProtocol *,
                                               const PurpleBuddy *buddy);

/** @copydoc  _PurpleProtocolInterface::send_raw */
int purple_protocol_iface_send_raw(PurpleProtocol *, PurpleConnection *gc,
                                   const char *buf, int len);

/** @copydoc  _PurpleProtocolInterface::roomlist_room_serialize */
char *purple_protocol_iface_roomlist_room_serialize(PurpleProtocol *,
                                                    PurpleRoomlistRoom *room);

/** @copydoc  _PurpleProtocolInterface::send_attention */
gboolean purple_protocol_iface_send_attention(PurpleProtocol *,
                                              PurpleConnection *gc,
                                              const char *username, guint type);

/** @copydoc  _PurpleProtocolInterface::get_attention_types */
GList *purple_protocol_iface_get_attention_types(PurpleProtocol *,
                                                 PurpleAccount *acct);

/** @copydoc  _PurpleProtocolInterface::get_account_text_table */
GHashTable *purple_protocol_iface_get_account_text_table(
                                                        PurpleProtocol *,
                                                        PurpleAccount *account);

/** @copydoc  _PurpleProtocolInterface::initiate_media */
gboolean purple_protocol_iface_initiate_media(PurpleProtocol *,
                                              PurpleAccount *account,
                                              const char *who,
                                              PurpleMediaSessionType type);

/** @copydoc  _PurpleProtocolInterface::get_media_caps */
PurpleMediaCaps purple_protocol_iface_get_media_caps(PurpleProtocol *,
                                                     PurpleAccount *account,
                                                     const char *who);

/** @copydoc  _PurpleProtocolInterface::get_moods */
PurpleMood *purple_protocol_iface_get_moods(PurpleProtocol *,
                                            PurpleAccount *account);

/** @copydoc  _PurpleProtocolInterface::set_public_alias */
void purple_protocol_iface_set_public_alias(
                                PurpleProtocol *, PurpleConnection *gc,
                                const char *alias,
                                PurpleSetPublicAliasSuccessCallback success_cb,
                                PurpleSetPublicAliasFailureCallback failure_cb);

/** @copydoc  _PurpleProtocolInterface::get_public_alias */
void purple_protocol_iface_get_public_alias(
                                PurpleProtocol *, PurpleConnection *gc,
                                PurpleGetPublicAliasSuccessCallback success_cb,
                                PurpleGetPublicAliasFailureCallback failure_cb);

/** @copydoc  _PurpleProtocolInterface::get_max_message_size */
gsize purple_protocol_iface_get_max_message_size(PurpleProtocol *,
                                                 PurpleConnection *gc);

/*@}*/

G_END_DECLS

#endif /* _PROTOCOL_H_ */
