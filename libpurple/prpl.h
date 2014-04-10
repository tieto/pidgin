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

#ifndef _PURPLE_PRPL_H_
#define _PURPLE_PRPL_H_
/**
 * SECTION:prpl
 * @section_id: libpurple-prpl
 * @short_description: <filename>prpl.h</filename>
 * @title: Protocol Plugin functions
 */

/* this file should be all that prpls need to include. therefore, by including
 * this file, they should get glib, proxy, purple_connection, prpl, etc. */

typedef struct _PurplePluginProtocolInfo PurplePluginProtocolInfo;

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

/**
 * PurpleIconScaleRules:
 * @PURPLE_ICON_SCALE_DISPLAY: We scale the icon when we display it
 * @PURPLE_ICON_SCALE_SEND:    We scale the icon before we send it to the server
 */
typedef enum {
	PURPLE_ICON_SCALE_DISPLAY = 0x01,
	PURPLE_ICON_SCALE_SEND    = 0x02
} PurpleIconScaleRules;


typedef struct _PurpleBuddyIconSpec PurpleBuddyIconSpec;

/**
 * PurpleThumbnailSpec:
 *
 * A description of a file transfer thumbnail specification.
 * This tells the UI if and what image formats the prpl support for file
 * transfer thumbnails.
 */
typedef struct _PurpleThumbnailSpec PurpleThumbnailSpec;

/**
 * NO_BUDDY_ICONS:
 *
 * This \#define exists just to make it easier to fill out the buddy icon
 * field in the prpl info struct for protocols that couldn't care less.
 */
#define NO_BUDDY_ICONS {NULL, 0, 0, 0, 0, 0, 0}

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "buddylist.h"
#include "conversations.h"
#include "xfer.h"
#include "image.h"
#include "media.h"
#include "notify.h"
#include "proxy.h"
#include "plugin.h"
#include "roomlist.h"
#include "status.h"
#include "whiteboard.h"


/**
 * PurpleBuddyIconSpec:
 * @format:       This is a comma-delimited list of image formats or %NULL if
 *                icons are not supported.  Neither the core nor the prpl will
 *                actually check to see if the data it's given matches this;
 *                it's entirely up to the UI to do what it wants
 * @min_width:    Minimum width of this icon
 * @min_height:   Minimum height of this icon
 * @max_width:    Maximum width of this icon
 * @max_height:   Maximum height of this icon
 * @max_filesize: Maximum size in bytes
 * @scale_rules:  How to stretch this icon
 *
 * A description of a Buddy Icon specification.  This tells Purple what kind of
 * image file it should give this prpl, and what kind of image file it should
 * expect back. Dimensions less than 1 should be ignored and the image not
 * scaled.
 */
struct _PurpleBuddyIconSpec {
	char *format;
	int min_width;
	int min_height;
	int max_width;
	int max_height;
	size_t max_filesize;
	PurpleIconScaleRules scale_rules;
};

/**
 * proto_chat_entry:
 * @label:      User-friendly name of the entry
 * @identifier: Used by the PRPL to identify the option
 * @required:   True if it's required
 * @is_int:     True if the entry expects an integer
 * @min:        Minimum value in case of integer
 * @max:        Maximum value in case of integer
 * @secret:     True if the entry is secret (password)
 *
 * Represents an entry containing information that must be supplied by the
 * user when joining a chat.
 */
struct proto_chat_entry {
	const char *label;
	const char *identifier;
	gboolean required;
	gboolean is_int;
	int min;
	int max;
	gboolean secret;
};

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
 * @OPT_PROTO_IM_IMAGE: Images in IMs.<sbr/>
 *           Oscar lets you send images in direct IMs.
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
	OPT_PROTO_IM_IMAGE                      = 0x00000040,
	OPT_PROTO_PASSWORD_OPTIONAL             = 0x00000080,
	OPT_PROTO_USE_POINTSIZE                 = 0x00000100,
	OPT_PROTO_REGISTER_NOSCREENNAME         = 0x00000200,
	OPT_PROTO_SLASH_COMMANDS_NATIVE         = 0x00000400,
	OPT_PROTO_INVITE_MESSAGE                = 0x00000800,
	OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE = 0x00001000,
	OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE  = 0x00002000

} PurpleProtocolOptions;

/**
 * PurplePluginProtocolInfo:
 *
 * A protocol plugin information structure.
 *
 * Every protocol plugin initializes this structure. It is the gateway
 * between purple and the protocol plugin.  Many of these callbacks can be
 * %NULL. If a callback must be implemented, it has a comment indicating so.
 */
struct _PurplePluginProtocolInfo
{
	/*
	 * The size of the PurplePluginProtocolInfo. This should always be sizeof(PurplePluginProtocolInfo).
	 * This allows adding more functions to this struct without requiring a major version bump.
	 */
	unsigned long struct_size;

	/* NOTE:
	 * If more functions are added, they should accessed using the following syntax:
	 *
	 *		if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl, new_function))
	 *			prpl->new_function(...);
	 *
	 * instead of
	 *
	 *		if (prpl->new_function != NULL)
	 *			prpl->new_function(...);
	 *
	 * The PURPLE_PROTOCOL_PLUGIN_HAS_FUNC macro can be used for the older member
	 * functions (e.g. login, send_im etc.) too.
	 */

	PurpleProtocolOptions options;  /* Protocol options.          */

	GList *user_splits;      /* A GList of PurpleAccountUserSplit */
	GList *protocol_options; /* A GList of PurpleAccountOption    */

	PurpleBuddyIconSpec icon_spec; /* The icon spec. */

	/*
	 * Returns the base icon name for the given buddy and account.
	 * If buddy is NULL and the account is non-NULL, it will return the
	 * name to use for the account's icon. If both are NULL, it will
	 * return the name to use for the protocol's icon.
	 *
	 * This must be implemented.
	 */
	const char *(*list_icon)(PurpleAccount *account, PurpleBuddy *buddy);

	/*
	 * Fills the four char**'s with string identifiers for "emblems"
	 * that the UI will interpret and display as relevant
	 */
	const char *(*list_emblem)(PurpleBuddy *buddy);

	/*
	 * Gets a short string representing this buddy's status.  This will
	 * be shown on the buddy list.
	 */
	char *(*status_text)(PurpleBuddy *buddy);

	/*
	 * Allows the prpl to add text to a buddy's tooltip.
	 */
	void (*tooltip_text)(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full);

	/*
	 * Returns a list of #PurpleStatusType which exist for this account;
	 * this must be implemented, and must add at least the offline and
	 * online states.
	 */
	GList *(*status_types)(PurpleAccount *account);

	/*
	 * Returns a list of #PurpleMenuAction structs, which represent extra
	 * actions to be shown in (for example) the right-click menu for @node.
	 */
	GList *(*blist_node_menu)(PurpleBlistNode *node);

	/*
	 * Returns a list of #proto_chat_entry structs, which represent
	 * information required by the PRPL to join a chat. libpurple will
	 * call join_chat along with the information filled by the user.
	 *
	 * Returns: A list of #proto_chat_entry structs
	 */
	GList *(*chat_info)(PurpleConnection *);

	/*
	 * Returns a hashtable which maps #proto_chat_entry struct identifiers
	 * to default options as strings based on chat_name. The resulting
	 * hashtable should be created with g_hash_table_new_full(g_str_hash,
	 * g_str_equal, NULL, g_free);. Use #get_chat_name if you instead need
	 * to extract a chat name from a hashtable.
	 *
	 * @chat_name: The chat name to be turned into components
	 * Returns: Hashtable containing the information extracted from chat_name
	 */
	GHashTable *(*chat_info_defaults)(PurpleConnection *, const char *chat_name);

	/* All the server-related functions */

	/* This must be implemented. */
	void (*login)(PurpleAccount *);

	/* This must be implemented. */
	void (*close)(PurpleConnection *);

	/*
	 * This PRPL function should return a positive value on success.
	 * If the message is too big to be sent, return -E2BIG.  If
	 * the account is not connected, return -ENOTCONN.  If the
	 * PRPL is unable to send the message for another reason, return
	 * some other negative value.  You can use one of the valid
	 * errno values, or just big something.  If the message should
	 * not be echoed to the conversation window, return 0.
	 */
	int  (*send_im)(PurpleConnection *, const char *who,
					const char *message,
					PurpleMessageFlags flags);

	void (*set_info)(PurpleConnection *, const char *info);

	/*
	 * Returns: If this protocol requires the PURPLE_IM_TYPING message to
	 *          be sent repeatedly to signify that the user is still
	 *          typing, then the PRPL should return the number of
	 *          seconds to wait before sending a subsequent notification.
	 *          Otherwise the PRPL should return 0.
	 */
	unsigned int (*send_typing)(PurpleConnection *, const char *name, PurpleIMTypingState state);

	/*
	 * Should arrange for purple_notify_userinfo() to be called with
	 * @who 's user info.
	 */
	void (*get_info)(PurpleConnection *, const char *who);
	void (*set_status)(PurpleAccount *account, PurpleStatus *status);

	void (*set_idle)(PurpleConnection *, int idletime);
	void (*change_passwd)(PurpleConnection *, const char *old_pass,
						  const char *new_pass);

	/*
	 * Add a buddy to a group on the server.
	 *
	 * This PRPL function may be called in situations in which the buddy is
	 * already in the specified group. If the protocol supports
	 * authorization and the user is not already authorized to see the
	 * status of \a buddy, \a add_buddy should request authorization.
	 *
	 * If authorization is required, then use the supplied invite message.
	 */
	void (*add_buddy)(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char *message);
	void (*add_buddies)(PurpleConnection *pc, GList *buddies, GList *groups, const char *message);
	void (*remove_buddy)(PurpleConnection *, PurpleBuddy *buddy, PurpleGroup *group);
	void (*remove_buddies)(PurpleConnection *, GList *buddies, GList *groups);
	void (*add_permit)(PurpleConnection *, const char *name);
	void (*add_deny)(PurpleConnection *, const char *name);
	void (*rem_permit)(PurpleConnection *, const char *name);
	void (*rem_deny)(PurpleConnection *, const char *name);
	void (*set_permit_deny)(PurpleConnection *);

	/*
	 * Called when the user requests joining a chat. Should arrange for
	 * #purple_serv_got_joined_chat to be called.
	 *
	 * @components: A hashtable containing information required to
	 *              join the chat as described by the entries returned
	 *              by #chat_info. It may also be called when accepting
	 *              an invitation, in which case this matches the
	 *              data parameter passed to #purple_serv_got_chat_invite.
	 */
	void (*join_chat)(PurpleConnection *, GHashTable *components);

	/*
	 * Called when the user refuses a chat invitation.
	 *
	 * @components: A hashtable containing information required to
	 *              join the chat as passed to #purple_serv_got_chat_invite.
	 */
	void (*reject_chat)(PurpleConnection *, GHashTable *components);

	/*
	 * Returns a chat name based on the information in components. Use
	 * #chat_info_defaults if you instead need to generate a hashtable
	 * from a chat name.
	 *
	 * @components: A hashtable containing information about the chat.
	 */
	char *(*get_chat_name)(GHashTable *components);

	/*
	 * Invite a user to join a chat.
	 *
	 * @id:      The id of the chat to invite the user to.
	 * @message: A message displayed to the user when the invitation
	 *           is received.
	 * @who:     The name of the user to send the invation to.
	 */
	void (*chat_invite)(PurpleConnection *, int id,
						const char *message, const char *who);
	/*
	 * Called when the user requests leaving a chat.
	 *
	 * @id: The id of the chat to leave
	 */
	void (*chat_leave)(PurpleConnection *, int id);

	/*
	 * Send a whisper to a user in a chat.
	 *
	 * @id:      The id of the chat.
	 * @who:     The name of the user to send the whisper to.
	 * @message: The message of the whisper.
	 */
	void (*chat_whisper)(PurpleConnection *, int id,
						 const char *who, const char *message);

	/*
	 * Send a message to a chat.
	 * This PRPL function should return a positive value on success.
	 * If the message is too big to be sent, return -E2BIG.  If
	 * the account is not connected, return -ENOTCONN.  If the
	 * PRPL is unable to send the message for another reason, return
	 * some other negative value.  You can use one of the valid
	 * errno values, or just big something.
	 *
	 * @id:      The id of the chat to send the message to.
	 * @message: The message to send to the chat.
	 * @flags:   A bitwise OR of #PurpleMessageFlags representing
	 *           message flags.
	 *
	 * Returns:  A positive number or 0 in case of success,
	 *           a negative error number in case of failure.
	 */
	int  (*chat_send)(PurpleConnection *, int id, const char *message, PurpleMessageFlags flags);

	/* If implemented, this will be called regularly for this prpl's
	 * active connections.  You'd want to do this if you need to repeatedly
	 * send some kind of keepalive packet to the server to avoid being
	 * disconnected.  ("Regularly" is defined by
	 * KEEPALIVE_INTERVAL in libpurple/connection.c.)
	 */
	void (*keepalive)(PurpleConnection *);

	/* new user registration */
	void (*register_user)(PurpleAccount *);

	/*
	 * Deprecated: Use PurplePluginProtocolInfo.get_info instead.
	 */
	void (*get_cb_info)(PurpleConnection *, int, const char *who);

	/* save/store buddy's alias on server list/roster */
	void (*alias_buddy)(PurpleConnection *, const char *who,
						const char *alias);

	/* change a buddy's group on a server list/roster */
	void (*group_buddy)(PurpleConnection *, const char *who,
						const char *old_group, const char *new_group);

	/* rename a group on a server list/roster */
	void (*rename_group)(PurpleConnection *, const char *old_name,
						 PurpleGroup *group, GList *moved_buddies);

	void (*buddy_free)(PurpleBuddy *);

	void (*convo_closed)(PurpleConnection *, const char *who);

	/*
	 * Convert the username @who to its canonical form. Also checks for
	 * validity.
	 *
	 * For example, AIM treats "fOo BaR" and "foobar" as the same user; this
	 * function should return the same normalized string for both of those.
	 * On the other hand, both of these are invalid for protocols with
	 * number-based usernames, so function should return NULL in such case.
	 *
	 * @account:  The account the username is related to. Can
	 *            be NULL.
	 * @who:      The username to convert.
	 * Returns:   Normalized username, or NULL, if it's invalid.
	 */
	const char *(*normalize)(const PurpleAccount *account, const char *who);

	/*
	 * Set the buddy icon for the given connection to @img.  The prpl
	 * does NOT own a reference to @img; if it needs one, it must
	 * #g_object_ref(@img) itself.
	 */
	void (*set_buddy_icon)(PurpleConnection *, PurpleImage *img);

	void (*remove_group)(PurpleConnection *gc, PurpleGroup *group);

	/* Gets the real name of a participant in a chat.  For example, on
	 * XMPP this turns a chat room nick foo into room\@server/foo
	 *
	 * @gc:  the connection on which the room is.
	 * @id:  the ID of the chat room.
	 * @who: the nickname of the chat participant.
	 *
	 * Returns:    the real name of the participant.  This string must be
	 *             freed by the caller.
	 */
	char *(*get_cb_real_name)(PurpleConnection *gc, int id, const char *who);

	void (*set_chat_topic)(PurpleConnection *gc, int id, const char *topic);

	PurpleChat *(*find_blist_chat)(PurpleAccount *account, const char *name);

	/* room listing prpl callbacks */
	PurpleRoomlist *(*roomlist_get_list)(PurpleConnection *gc);
	void (*roomlist_cancel)(PurpleRoomlist *list);
	void (*roomlist_expand_category)(PurpleRoomlist *list, PurpleRoomlistRoom *category);

	/* file transfer callbacks */
	gboolean (*can_receive_file)(PurpleConnection *, const char *who);
	void (*send_file)(PurpleConnection *, const char *who, const char *filename);
	PurpleXfer *(*new_xfer)(PurpleConnection *, const char *who);

	/* Checks whether offline messages to @buddy are supported.
	 * Returns: TRUE if @buddy can be sent messages while they are
	 *          offline, or FALSE if not.
	 */
	gboolean (*offline_message)(const PurpleBuddy *buddy);

	PurpleWhiteboardPrplOps *whiteboard_prpl_ops;

	/* For use in plugins that may understand the underlying protocol */
	int (*send_raw)(PurpleConnection *gc, const char *buf, int len);

	/* room list serialize */
	char *(*roomlist_room_serialize)(PurpleRoomlistRoom *room);

	/* Remove the user from the server.  The account can either be
	 * connected or disconnected. After the removal is finished, the
	 * connection will stay open and has to be closed!
	 */
	/* This is here rather than next to register_user for API compatibility
	 * reasons.
	 */
	void (*unregister_user)(PurpleAccount *, PurpleAccountUnregistrationCb cb, void *user_data);

	/* Attention API for sending & receiving zaps/nudges/buzzes etc. */
	gboolean (*send_attention)(PurpleConnection *gc, const char *username, guint type);
	GList *(*get_attention_types)(PurpleAccount *acct);

	/* This allows protocols to specify additional strings to be used for
	 * various purposes.  The idea is to stuff a bunch of strings in this hash
	 * table instead of expanding the struct for every addition.  This hash
	 * table is allocated every call and MUST be unrefed by the caller.
	 *
	 * @account: The account to specify.  This can be NULL.
	 *
	 * Returns : The protocol's string hash table. The hash table should be
	 *           destroyed by the caller when it's no longer needed.
	 */
	GHashTable *(*get_account_text_table)(PurpleAccount *account);

	/*
	 * Initiate a media session with the given contact.
	 *
	 * @account: The account to initiate the media session on.
	 * @who: The remote user to initiate the session with.
	 * @type: The type of media session to initiate.
	 *
	 * Returns: TRUE if the call succeeded else FALSE. (Doesn't imply the media session or stream will be successfully created)
	 */
	gboolean (*initiate_media)(PurpleAccount *account, const char *who,
					PurpleMediaSessionType type);

	/*
	 * Checks to see if the given contact supports the given type of media session.
	 *
	 * @account: The account the contact is on.
	 * @who: The remote user to check for media capability with.
	 *
	 * Returns: The media caps the contact supports.
	 */
	PurpleMediaCaps (*get_media_caps)(PurpleAccount *account,
					  const char *who);

	/*
	 * Returns an array of "PurpleMood"s, with the last one having
	 * "mood" set to NULL.
	 */
	PurpleMood *(*get_moods)(PurpleAccount *account);

	/*
	 * Set the user's "friendly name" (or alias or nickname or
	 * whatever term you want to call it) on the server.  The
	 * protocol plugin should call success_cb or failure_cb
	 * *asynchronously* (if it knows immediately that the set will fail,
	 * call one of the callbacks from an idle/0-second timeout) depending
	 * on if the nickname is set successfully.
	 *
	 * See purple_account_set_public_alias().
	 *
	 * @gc:    The connection for which to set an alias
	 * @alias: The new server-side alias/nickname for this account,
	 *              or NULL to unset the alias/nickname (or return it to
	 *              a protocol-specific "default").
	 * @success_cb: Callback to be called if the public alias is set
	 * @failure_cb: Callback to be called if setting the public alias
	 *                   fails
	 */
	void (*set_public_alias)(PurpleConnection *gc, const char *alias,
	                         PurpleSetPublicAliasSuccessCallback success_cb,
	                         PurpleSetPublicAliasFailureCallback failure_cb);
	/*
	 * Retrieve the user's "friendly name" as set on the server.
	 * The protocol plugin should call success_cb or failure_cb
	 * *asynchronously* (even if it knows immediately that the get will fail,
	 * call one of the callbacks from an idle/0-second timeout) depending
	 * on if the nickname is retrieved.
	 *
	 * See purple_account_get_public_alias().
	 *
	 * @gc:         The connection for which to retireve the alias
	 * @success_cb: Callback to be called with the retrieved alias
	 * @failure_cb: Callback to be called if the prpl is unable to
	 *              retrieve the alias
	 */
	void (*get_public_alias)(PurpleConnection *gc,
	                         PurpleGetPublicAliasSuccessCallback success_cb,
	                         PurpleGetPublicAliasFailureCallback failure_cb);

	/*
	 * Gets the maximum message size in bytes for the conversation.
	 *
	 * It may depend on connection-specific or conversation-specific
	 * variables, like channel or buddy's name length.
	 *
	 * This value is intended for plaintext message, the exact value may be
	 * lower because of:
	 *  - used newlines (some protocols count them as more than one byte),
	 *  - formatting,
	 *  - used special characters.
	 *
	 * @conv: The conversation to query, or NULL to get safe minimum
	 *        for the protocol.
	 *
	 * Returns: Maximum message size, 0 if unspecified, -1 for infinite.
	 */
	gssize (*get_max_message_size)(PurpleConversation *conv);
};

#define PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl, member) \
	((prpl != NULL) && (G_STRUCT_OFFSET(PurplePluginProtocolInfo, member) < \
	prpl->struct_size && prpl->member != NULL))


#define PURPLE_IS_PROTOCOL_PLUGIN(plugin) \
	((plugin)->info->type == PURPLE_PLUGIN_PROTOCOL)

#define PURPLE_PLUGIN_PROTOCOL_INFO(plugin) \
	((PurplePluginProtocolInfo *)(plugin)->info->extra_info)

G_BEGIN_DECLS

/**************************************************************************/
/* Attention Type API                                                     */
/**************************************************************************/

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
/* Protocol Plugin API                                                    */
/**************************************************************************/

/**
 * purple_prpl_got_account_idle:
 * @account:   The account.
 * @idle:      The user's idle state.
 * @idle_time: The user's idle time.
 *
 * Notifies Purple that our account's idle state and time have changed.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_account_idle(PurpleAccount *account, gboolean idle,
								time_t idle_time);

/**
 * purple_prpl_got_account_login_time:
 * @account:    The account the user is on.
 * @login_time: The user's log-in time.
 *
 * Notifies Purple of our account's log-in time.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_account_login_time(PurpleAccount *account, time_t login_time);

/**
 * purple_prpl_got_account_status:
 * @account:   The account the user is on.
 * @status_id: The status ID.
 * @...:       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @attr_id.
 *
 * Notifies Purple that our account's status has changed.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_account_status(PurpleAccount *account,
								  const char *status_id, ...) G_GNUC_NULL_TERMINATED;

/**
 * purple_prpl_got_account_actions:
 * @account:   The account.
 *
 * Notifies Purple that our account's actions have changed. This is only
 * called after the initial connection. Emits the account-actions-changed
 * signal.
 *
 * This is meant to be called from protocol plugins.
 *
 * See <link linkend="accounts-account-actions-changed"><literal>"account-actions-changed"</literal></link>
 */
void purple_prpl_got_account_actions(PurpleAccount *account);

/**
 * purple_prpl_got_user_idle:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @idle:      The user's idle state.
 * @idle_time: The user's idle time.  This is the time at
 *                  which the user became idle, in seconds since
 *                  the epoch.  If the PRPL does not know this value
 *                  then it should pass 0.
 *
 * Notifies Purple that a buddy's idle state and time have changed.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_user_idle(PurpleAccount *account, const char *name,
							 gboolean idle, time_t idle_time);

/**
 * purple_prpl_got_user_login_time:
 * @account:    The account the user is on.
 * @name:       The name of the buddy.
 * @login_time: The user's log-in time.
 *
 * Notifies Purple of a buddy's log-in time.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_user_login_time(PurpleAccount *account, const char *name,
								   time_t login_time);

/**
 * purple_prpl_got_user_status:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @status_id: The status ID.
 * @...:       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @attr_id.
 *
 * Notifies Purple that a buddy's status has been activated.
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_user_status(PurpleAccount *account, const char *name,
							   const char *status_id, ...) G_GNUC_NULL_TERMINATED;

/**
 * purple_prpl_got_user_status_deactive:
 * @account:   The account the user is on.
 * @name:      The name of the buddy.
 * @status_id: The status ID.
 *
 * Notifies libpurple that a buddy's status has been deactivated
 *
 * This is meant to be called from protocol plugins.
 */
void purple_prpl_got_user_status_deactive(PurpleAccount *account, const char *name,
					const char *status_id);

/**
 * purple_prpl_change_account_status:
 * @account:    The account the user is on.
 * @old_status: The previous status.
 * @new_status: The status that was activated, or deactivated
 *                   (in the case of independent statuses).
 *
 * Informs the server that our account's status changed.
 */
void purple_prpl_change_account_status(PurpleAccount *account,
									 PurpleStatus *old_status,
									 PurpleStatus *new_status);

/**
 * purple_prpl_get_statuses:
 * @account: The account the user is on.
 * @presence: The presence for which we're going to get statuses
 *
 * Retrieves the list of stock status types from a prpl.
 *
 * Returns: List of statuses
 */
GList *purple_prpl_get_statuses(PurpleAccount *account, PurplePresence *presence);

/**
 * purple_prpl_send_attention:
 * @gc: The connection to send the message on.
 * @who: Whose attention to request.
 * @type_code: An index into the prpl's attention_types list determining the type
 *        of the attention request command to send. 0 if prpl only defines one
 *        (for example, Yahoo and MSN), but protocols are allowed to define more.
 *
 * Send an attention request message.
 *
 * Note that you can't send arbitrary PurpleAttentionType's, because there is
 * only a fixed set of attention commands.
 */
void purple_prpl_send_attention(PurpleConnection *gc, const char *who, guint type_code);

/**
 * purple_prpl_got_attention:
 * @gc: The connection that received the attention message.
 * @who: Who requested your attention.
 * @type_code: An index into the prpl's attention_types list determining the type
 *        of the attention request command to send.
 *
 * Process an incoming attention message.
 */
void purple_prpl_got_attention(PurpleConnection *gc, const char *who, guint type_code);

/**
 * purple_prpl_got_attention_in_chat:
 * @gc: The connection that received the attention message.
 * @id: The chat id.
 * @who: Who requested your attention.
 * @type_code: An index into the prpl's attention_types list determining the type
 *        of the attention request command to send.
 *
 * Process an incoming attention message in a chat.
 */
void purple_prpl_got_attention_in_chat(PurpleConnection *gc, int id, const char *who, guint type_code);

/**
 * purple_prpl_get_media_caps:
 * @account: The account the user is on.
 * @who: The name of the contact to check capabilities for.
 *
 * Determines if the contact supports the given media session type.
 *
 * Returns: The media caps the contact supports.
 */
PurpleMediaCaps purple_prpl_get_media_caps(PurpleAccount *account,
				  const char *who);

/**
 * purple_prpl_initiate_media:
 * @account: The account the user is on.
 * @who: The name of the contact to start a session with.
 * @type: The type of media session to start.
 *
 * Initiates a media session with the given contact.
 *
 * Returns: TRUE if the call succeeded else FALSE. (Doesn't imply the media
 *          session or stream will be successfully created)
 */
gboolean purple_prpl_initiate_media(PurpleAccount *account,
					const char *who,
					PurpleMediaSessionType type);

/**
 * purple_prpl_got_media_caps:
 * @account: The account the user is on.
 * @who: The name of the contact for which capabilities have been received.
 *
 * Signals that the prpl received capabilities for the given contact.
 *
 * This function is intended to be used only by prpls.
 */
void purple_prpl_got_media_caps(PurpleAccount *account, const char *who);

/**
 * purple_prpl_get_max_message_size:
 * @prpl: The protocol plugin to query.
 *
 * Gets the safe maximum message size in bytes for the protocol plugin.
 *
 * See #PurplePluginProtocolInfo's #get_max_message_size
 *
 * Returns: Maximum message size, 0 if unspecified, -1 for infinite.
 */
gssize
purple_prpl_get_max_message_size(PurplePlugin *prpl);

/**************************************************************************/
/* Protocol Plugin Subsystem API                                          */
/**************************************************************************/

/**
 * purple_find_prpl:
 * @id: The protocol plugin;
 *
 * Finds a protocol plugin structure of the specified type.
 */
PurplePlugin *purple_find_prpl(const char *id);

G_END_DECLS

#endif /* _PRPL_H_ */
