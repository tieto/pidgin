/**
 * @file protocols.h Protocols API
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

#ifndef _PURPLE_PROTOCOLS_H_
#define _PURPLE_PROTOCOLS_H_

#define PURPLE_TYPE_PROTOCOL_ACTION  (purple_protocol_action_get_type())

/** @copydoc _PurpleProtocolAction */
typedef struct _PurpleProtocolAction PurpleProtocolAction;
typedef void (*PurpleProtocolActionCallback)(PurpleProtocolAction *);

#define PURPLE_TYPE_ATTENTION_TYPE  (purple_attention_type_get_type())

/** Represents "nudges" and "buzzes" that you may send to a buddy to attract
 *  their attention (or vice-versa).
 */
typedef struct _PurpleAttentionType PurpleAttentionType;

/**************************************************************************/
/** @name Basic Protocol Information                                      */
/**************************************************************************/

typedef enum {
	PURPLE_ICON_SCALE_DISPLAY = 0x01,		/**< We scale the icon when we display it */
	PURPLE_ICON_SCALE_SEND = 0x02			/**< We scale the icon before we send it to the server */
} PurpleIconScaleRules;

#define PURPLE_TYPE_BUDDY_ICON_SPEC  (purple_buddy_icon_spec_get_type())

/**
 * A description of a Buddy Icon specification.  This tells Purple what kind of image file
 * it should give this protocol, and what kind of image file it should expect back.
 * Dimensions less than 1 should be ignored and the image not scaled.
 */
typedef struct _PurpleBuddyIconSpec PurpleBuddyIconSpec;

#define PURPLE_TYPE_THUMBNAIL_SPEC  (purple_thumbnail_spec_get_type())

/**
 * A description of a file transfer thumbnail specification.
 * This tells the UI if and what image formats the protocol support for file
 * transfer thumbnails.
 */
typedef struct _PurpleThumbnailSpec PurpleThumbnailSpec;

/**
 * Represents an entry containing information that must be supplied by the
 * user when joining a chat.
 */
typedef struct _PurpleProtocolChatEntry PurpleProtocolChatEntry;

/**
 * Protocol options
 *
 * These should all be stuff that some protocols can do and others can't.
 */
typedef enum
{
	/**
	 * User names are unique to a chat and are not shared between rooms.
	 *
	 * XMPP lets you choose what name you want in chats, so it shouldn't
	 * be pulling the aliases from the buddy list for the chat list;
	 * it gets annoying.
	 */
	OPT_PROTO_UNIQUE_CHATNAME = 0x00000004,

	/**
	 * Chat rooms have topics.
	 *
	 * IRC and XMPP support this.
	 */
	OPT_PROTO_CHAT_TOPIC = 0x00000008,

	/**
	 * Don't require passwords for sign-in.
	 *
	 * Zephyr doesn't require passwords, so there's no
	 * need for a password prompt.
	 */
	OPT_PROTO_NO_PASSWORD = 0x00000010,

	/**
	 * Notify on new mail.
	 *
	 * MSN and Yahoo notify you when you have new mail.
	 */
	OPT_PROTO_MAIL_CHECK = 0x00000020,

	/**
	 * Images in IMs.
	 *
	 * Oscar lets you send images in direct IMs.
	 */
	OPT_PROTO_IM_IMAGE = 0x00000040,

	/**
	 * Allow passwords to be optional.
	 *
	 * Passwords in IRC are optional, and are needed for certain
	 * functionality.
	 */
	OPT_PROTO_PASSWORD_OPTIONAL = 0x00000080,

	/**
	 * Allows font size to be specified in sane point size
	 *
	 * Probably just XMPP and Y!M
	 */
	OPT_PROTO_USE_POINTSIZE = 0x00000100,

	/**
	 * Set the Register button active even when the username has not
	 * been specified.
	 *
	 * Gadu-Gadu doesn't need a username to register new account (because
	 * usernames are assigned by the server).
	 */
	OPT_PROTO_REGISTER_NOSCREENNAME = 0x00000200,

	/**
	 * Indicates that slash commands are native to this protocol.
	 * Used as a hint that unknown commands should not be sent as messages.
	 */
	OPT_PROTO_SLASH_COMMANDS_NATIVE = 0x00000400,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an invitation.
	 */
	OPT_PROTO_INVITE_MESSAGE = 0x00000800,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an authorization acceptance.
	 */
	OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE = 0x00001000,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an authorization denial.
	 */
	OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE = 0x00002000

} PurpleProtocolOptions;

#include "protocol.h"

/** @copydoc PurpleBuddyIconSpec */
struct _PurpleBuddyIconSpec {
	/** This is a comma-delimited list of image formats or @c NULL if icons
	 *  are not supported.  Neither the core nor the protocol will actually
	 *  check to see if the data it's given matches this; it's entirely up
	 *  to the UI to do what it wants
	 */
	char *format;

	int min_width;                     /**< Minimum width of this icon  */
	int min_height;                    /**< Minimum height of this icon */
	int max_width;                     /**< Maximum width of this icon  */
	int max_height;                    /**< Maximum height of this icon */
	size_t max_filesize;               /**< Maximum size in bytes */
	PurpleIconScaleRules scale_rules;  /**< How to stretch this icon */
};

#define PURPLE_TYPE_PROTOCOL_CHAT_ENTRY  (purple_protocol_chat_entry_get_type())

/** @copydoc PurpleProtocolChatEntry */
struct _PurpleProtocolChatEntry {
	const char *label;       /**< User-friendly name of the entry */
	const char *identifier;  /**< Used by the protocol to identify the option */
	gboolean required;       /**< True if it's required */
	gboolean is_int;         /**< True if the entry expects an integer */
	int min;                 /**< Minimum value in case of integer */
	int max;                 /**< Maximum value in case of integer */
	gboolean secret;         /**< True if the entry is secret (password) */
};

/**
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
/** @name Attention Type API                                              */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for the #PurpleAttentionType boxed structure.
 */
GType purple_attention_type_get_type(void);

/**
 * Creates a new #PurpleAttentionType object and sets its mandatory parameters.
 *
 * @param ulname A non-localized string that can be used by UIs in need of such
 *               non-localized strings.  This should be the same as @a name,
 *               without localization.
 * @param name A localized string that the UI may display for the event. This
 *             should be the same string as @a ulname, with localization.
 * @param inc_desc A localized description shown when the event is received.
 * @param out_desc A localized description shown when the event is sent.
 *
 * @return A pointer to the new object.
 */
PurpleAttentionType *purple_attention_type_new(const char *ulname, const char *name,
								const char *inc_desc, const char *out_desc);

/**
 * Sets the displayed name of the attention-demanding event.
 *
 * @param type The attention type.
 * @param name The localized name that will be displayed by UIs. This should be
 *             the same string given as the unlocalized name, but with
 *             localization.
 */
void purple_attention_type_set_name(PurpleAttentionType *type, const char *name);

/**
 * Sets the description of the attention-demanding event shown in  conversations
 * when the event is received.
 *
 * @param type The attention type.
 * @param desc The localized description for incoming events.
 */
void purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const char *desc);

/**
 * Sets the description of the attention-demanding event shown in conversations
 * when the event is sent.
 *
 * @param type The attention type.
 * @param desc The localized description for outgoing events.
 */
void purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const char *desc);

/**
 * Sets the name of the icon to display for the attention event; this is optional.
 *
 * @param type The attention type.
 * @param name The icon's name.
 * @note Icons are optional for attention events.
 */
void purple_attention_type_set_icon_name(PurpleAttentionType *type, const char *name);

/**
 * Sets the unlocalized name of the attention event; some UIs may need this,
 * thus it is required.
 *
 * @param type The attention type.
 * @param ulname The unlocalized name.  This should be the same string given as
 *               the localized name, but without localization.
 */
void purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const char *ulname);

/**
 * Get the attention type's name as displayed by the UI.
 *
 * @param type The attention type.
 *
 * @return The name.
 */
const char *purple_attention_type_get_name(const PurpleAttentionType *type);

/**
 * Get the attention type's description shown when the event is received.
 *
 * @param type The attention type.
 * @return The description.
 */
const char *purple_attention_type_get_incoming_desc(const PurpleAttentionType *type);

/**
 * Get the attention type's description shown when the event is sent.
 *
 * @param type The attention type.
 * @return The description.
 */
const char *purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type);

/**
 * Get the attention type's icon name.
 *
 * @param type The attention type.
 * @return The icon name or @c NULL if unset/empty.
 * @note Icons are optional for attention events.
 */
const char *purple_attention_type_get_icon_name(const PurpleAttentionType *type);

/**
 * Get the attention type's unlocalized name; this is useful for some UIs.
 *
 * @param type The attention type
 * @return The unlocalized name.
 */
const char *purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type);

/*@}*/

/**************************************************************************/
/** @name Protocol Action API                                             */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for the #PurpleProtocolAction boxed structure.
 */
GType purple_protocol_action_get_type(void);

/** TODO A sanity check is needed
 * Allocates and returns a new PurpleProtocolAction. Use this to add actions in
 * a list in the get_actions function of the protocol.
 *
 * @param label    The description of the action to show to the user.
 * @param callback The callback to call when the user selects this action.
 */
PurpleProtocolAction *purple_protocol_action_new(const char* label,
		PurpleProtocolActionCallback callback);

/** TODO A sanity check is needed
 * Frees a PurpleProtocolAction
 *
 * @param action The PurpleProtocolAction to free.
 */
void purple_protocol_action_free(PurpleProtocolAction *action);

/*@}*/

/**************************************************************************/
/** @name Buddy Icon Spec API                                             */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for the #PurpleBuddyIconSpec boxed structure.
 */
GType purple_buddy_icon_spec_get_type(void);

/*@}*/

/**************************************************************************/
/** @name Thumbnail API                                                   */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for the #PurpleThumbnailSpec boxed structure.
 */
GType purple_thumbnail_spec_get_type(void);

/*@}*/

/**************************************************************************/
/** @name Protocol Chat Entry API                                         */
/**************************************************************************/
/*@{*/

/** TODO
 * Returns the GType for the #PurpleProtocolChatEntry boxed structure.
 */
GType purple_protocol_chat_entry_get_type(void);

/*@}*/

/**************************************************************************/
/** @name Protocols API                                                   */
/**************************************************************************/
/*@{*/

/** TODO rename
 * Finds a protocol by ID.
 *
 * @param id The protocol's ID.
 */
PurpleProtocol *purple_find_protocol_info(const char *id);

/** TODO A sanity check is needed
 * Adds a protocol to the list of protocols.
 *
 * @param protocol  The protocol to add.
 *
 * @return TRUE if the protocol was added, else FALSE.
 */
gboolean purple_protocols_add(PurpleProtocol *protocol);

/** TODO A sanity check is needed
 * Removes a protocol from the list of protocols. This will disconnect all
 * connected accounts using this protocol, and free the protocol's user splits
 * and protocol options.
 *
 * @param protocol  The protocol to remove.
 *
 * @return TRUE if the protocol was removed, else FALSE.
 */
gboolean purple_protocols_remove(PurpleProtocol *protocol);

/** TODO A sanity check is needed
 * Returns a list of all loaded protocols.
 *
 * @constreturn A list of all loaded protocols.
 */
GList *purple_protocols_get_all(void);

/*@}*/

/**************************************************************************/
/** @name Protocols Subsytem API                                          */
/**************************************************************************/
/*@{*/

/**
 * Initializes the protocols subsystem.
 */
void purple_protocols_init(void);

/** TODO Make protocols use this handle, instead of plugins handle
 * Returns the protocols subsystem handle.
 *
 * @return The protocols subsystem handle.
 */
void *purple_protocols_get_handle(void);

/**
 * Uninitializes the protocols subsystem.
 */
void purple_protocols_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PROTOCOLS_H_ */
