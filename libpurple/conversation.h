/**
 * @file conversation.h Conversation API
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
#ifndef _PURPLE_CONVERSATION_H_
#define _PURPLE_CONVERSATION_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

/** @copydoc _PurpleConversation */
typedef struct _PurpleConversation           PurpleConversation;
/** @copydoc _PurpleConversation */
typedef struct _PurpleConversationClass      PurpleConversationClass;

/** @copydoc _PurpleChatConversation */
typedef struct _PurpleChatConversation       PurpleChatConversation;
/** @copydoc _PurpleChatConversationClass */
typedef struct _PurpleChatConversationClass  PurpleChatConversationClass;

/** @copydoc _PurpleIMConversation */
typedef struct _PurpleIMConversation         PurpleIMConversation;
/** @copydoc _PurpleIMConversationClass */
typedef struct _PurpleIMConversationClass    PurpleIMConversationClass;

/** @copydoc _PurpleChatConversationBuddy */
typedef struct _PurpleChatConversationBuddy       PurpleChatConversationBuddy;
/** @copydoc _PurpleChatConversationBuddyClass */
typedef struct _PurpleChatConversationBuddyClass  PurpleChatConversationBuddyClass;

/** @copydoc _PurpleConversationUiOps */
typedef struct _PurpleConversationUiOps      PurpleConversationUiOps;
/** @copydoc _PurpleConversationMessage */
typedef struct _PurpleConversationMessage    PurpleConversationMessage;

/**
 * Conversation update type.
 */
typedef enum
{
	PURPLE_CONVERSATION_UPDATE_ADD = 0, /**< The buddy associated with the conversation
	                                         was added.   */
	PURPLE_CONVERSATION_UPDATE_REMOVE,  /**< The buddy associated with the conversation
	                                         was removed. */
	PURPLE_CONVERSATION_UPDATE_ACCOUNT, /**< The purple_account was changed. */
	PURPLE_CONVERSATION_UPDATE_TYPING,  /**< The typing state was updated. */
	PURPLE_CONVERSATION_UPDATE_UNSEEN,  /**< The unseen state was updated. */
	PURPLE_CONVERSATION_UPDATE_LOGGING, /**< Logging for this conversation was
	                                         enabled or disabled. */
	PURPLE_CONVERSATION_UPDATE_TOPIC,   /**< The topic for a chat was updated. */
	/*
	 * XXX These need to go when we implement a more generic core/UI event
	 * system.
	 */
	PURPLE_CONVERSATION_ACCOUNT_ONLINE,  /**< One of the user's accounts went online.  */
	PURPLE_CONVERSATION_ACCOUNT_OFFLINE, /**< One of the user's accounts went offline. */
	PURPLE_CONVERSATION_UPDATE_AWAY,     /**< The other user went away.                */
	PURPLE_CONVERSATION_UPDATE_ICON,     /**< The other user's buddy icon changed.     */
	PURPLE_CONVERSATION_UPDATE_TITLE,
	PURPLE_CONVERSATION_UPDATE_CHATLEFT,

	PURPLE_CONVERSATION_UPDATE_FEATURES  /**< The features for a chat have changed */

} PurpleConversationUpdateType;

/**
 * The typing state of a user.
 */
typedef enum
{
	PURPLE_IM_CONVERSATION_NOT_TYPING = 0,  /**< Not typing.                 */
	PURPLE_IM_CONVERSATION_TYPING,          /**< Currently typing.           */
	PURPLE_IM_CONVERSATION_TYPED            /**< Stopped typing momentarily. */

} PurpleIMConversationTypingState;

/**
 * Flags applicable to a message. Most will have send, recv or system.
 */
typedef enum /*< flags >*/
{
	PURPLE_CONVERSATION_MESSAGE_SEND        = 0x0001, /**< Outgoing message.        */
	PURPLE_CONVERSATION_MESSAGE_RECV        = 0x0002, /**< Incoming message.        */
	PURPLE_CONVERSATION_MESSAGE_SYSTEM      = 0x0004, /**< System message.          */
	PURPLE_CONVERSATION_MESSAGE_AUTO_RESP   = 0x0008, /**< Auto response.           */
	PURPLE_CONVERSATION_MESSAGE_ACTIVE_ONLY = 0x0010,  /**< Hint to the UI that this
	                                                        message should not be
	                                                        shown in conversations
	                                                        which are only open for
	                                                        internal UI purposes
	                                                        (e.g. for contact-aware
	                                                        conversations).         */
	PURPLE_CONVERSATION_MESSAGE_NICK        = 0x0020, /**< Contains your nick.      */
	PURPLE_CONVERSATION_MESSAGE_NO_LOG      = 0x0040, /**< Do not log.              */
	PURPLE_CONVERSATION_MESSAGE_WHISPER     = 0x0080, /**< Whispered message.       */
	PURPLE_CONVERSATION_MESSAGE_ERROR       = 0x0200, /**< Error message.           */
	PURPLE_CONVERSATION_MESSAGE_DELAYED     = 0x0400, /**< Delayed message.         */
	PURPLE_CONVERSATION_MESSAGE_RAW         = 0x0800, /**< "Raw" message - don't
	                                                        apply formatting        */
	PURPLE_CONVERSATION_MESSAGE_IMAGES      = 0x1000, /**< Message contains images  */
	PURPLE_CONVERSATION_MESSAGE_NOTIFY      = 0x2000, /**< Message is a notification */
	PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY  = 0x4000, /**< Message should not be auto-
										                   linkified */
	PURPLE_CONVERSATION_MESSAGE_INVISIBLE   = 0x8000  /**< Message should not be displayed */
} PurpleConversationMessageFlags;

/**
 * Flags applicable to users in Chats.
 */
typedef enum /*< flags >*/
{
	PURPLE_CHAT_CONVERSATION_BUDDY_NONE     = 0x0000, /**< No flags                     */
	PURPLE_CHAT_CONVERSATION_BUDDY_VOICE    = 0x0001, /**< Voiced user or "Participant" */
	PURPLE_CHAT_CONVERSATION_BUDDY_HALFOP   = 0x0002, /**< Half-op                      */
	PURPLE_CHAT_CONVERSATION_BUDDY_OP       = 0x0004, /**< Channel Op or Moderator      */
	PURPLE_CHAT_CONVERSATION_BUDDY_FOUNDER  = 0x0008, /**< Channel Founder              */
	PURPLE_CHAT_CONVERSATION_BUDDY_TYPING   = 0x0010, /**< Currently typing             */
	PURPLE_CHAT_CONVERSATION_BUDDY_AWAY     = 0x0020  /**< Currently away.              */

} PurpleChatConversationBuddyFlags;

#include "account.h"
#include "buddyicon.h"
#include "log.h"
#include "server.h"

/**************************************************************************/
/** PurpleConversation                                                    */
/**************************************************************************/
/** Structure representing a conversation instance. */
struct _PurpleConversation
{
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleConversation's */
struct _PurpleConversationClass {
	/*< private >*/
	GObjectClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleChatConversation                                                */
/**************************************************************************/
/** Structure representing a chat conversation instance. */
struct _PurpleChatConversation
{
	/*< private >*/
	PurpleConversation parent_object;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleChatConversation's */
struct _PurpleChatConversationClass {
	/*< private >*/
	PurpleConversationClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleIMConversation                                                  */
/**************************************************************************/
/** Structure representing an IM conversation instance. */
struct _PurpleIMConversation
{
	/*< private >*/
	PurpleConversation parent_object;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleIMConversation's */
struct _PurpleIMConversationClass {
	/*< private >*/
	PurpleConversationClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleChatConversationBuddy                                           */
/**************************************************************************/
/** Structure representing a chat buddy instance. */
struct _PurpleChatConversationBuddy
{
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleChatConversationBuddy's */
struct _PurpleChatConversationBuddyClass {
	/*< private >*/
	GObjectClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleConversationUiOps                                               */
/**************************************************************************/
/**
 * Conversation operations and events.
 *
 * Any UI representing a conversation must assign a filled-out
 * PurpleConversationUiOps structure to the PurpleConversation.
 */
struct _PurpleConversationUiOps
{
	/** Called when @a conv is created (but before the @ref
	 *  conversation-created signal is emitted).
	 */
	void (*create_conversation)(PurpleConversation *conv);

	/** Called just before @a conv is freed. */
	void (*destroy_conversation)(PurpleConversation *conv);
	/** Write a message to a chat.  If this field is @c NULL, libpurple will
	 *  fall back to using #write_conv.
	 *  @see purple_chat_conversation_write()
	 */
	void (*write_chat)(PurpleConversation *conv, const char *who,
	                  const char *message, PurpleConversationMessageFlags flags,
	                  time_t mtime);
	/** Write a message to an IM conversation.  If this field is @c NULL,
	 *  libpurple will fall back to using #write_conv.
	 *  @see purple_im_conversation_write()
	 */
	void (*write_im)(PurpleConversation *conv, const char *who,
	                 const char *message, PurpleConversationMessageFlags flags,
	                 time_t mtime);
	/** Write a message to a conversation.  This is used rather than the
	 *  chat- or im-specific ops for errors, system messages (such as "x is
	 *  now know as y"), and as the fallback if #write_im and #write_chat
	 *  are not implemented.  It should be implemented, or the UI will miss
	 *  conversation error messages and your users will hate you.
	 *
	 *  @see purple_conversation_write()
	 */
	void (*write_conv)(PurpleConversation *conv,
	                   const char *name,
	                   const char *alias,
	                   const char *message,
	                   PurpleConversationMessageFlags flags,
	                   time_t mtime);

	/** Add @a cbuddies to a chat.
	 *  @param cbuddies      A @c GList of #PurpleChatConversationBuddy structs.
	 *  @param new_arrivals  Whether join notices should be shown.
	 *                       (Join notices are actually written to the
	 *                       conversation by
	 *                       #purple_chat_conversation_add_users().)
	 */
	void (*chat_add_users)(PurpleConversation *conv,
	                       GList *cbuddies,
	                       gboolean new_arrivals);
	/** Rename the user in this chat named @a old_name to @a new_name.  (The
	 *  rename message is written to the conversation by libpurple.)
	 *  @param new_alias  @a new_name's new alias, if they have one.
	 *  @see purple_chat_conversation_add_users()
	 */
	void (*chat_rename_user)(PurpleConversation *conv, const char *old_name,
	                         const char *new_name, const char *new_alias);
	/** Remove @a users from a chat.
	 *  @param users    A @c GList of <tt>const char *</tt>s.
	 *  @see purple_chat_conversation_rename_user()
	 */
	void (*chat_remove_users)(PurpleConversation *conv, GList *users);
	/** Called when a user's flags are changed.
	 *  @see purple_chat_conversation_user_set_flags()
	 */
	void (*chat_update_user)(PurpleConversation *conv, const char *user);

	/** Present this conversation to the user; for example, by displaying
	 *  the IM dialog.
	 */
	void (*present)(PurpleConversation *conv);

	/** If this UI has a concept of focus (as in a windowing system) and
	 *  this conversation has the focus, return @c TRUE; otherwise, return
	 *  @c FALSE.
	 */
	gboolean (*has_focus)(PurpleConversation *conv);

	/* Custom Smileys */
	gboolean (*custom_smiley_add)(PurpleConversation *conv, const char *smile,
	                            boolean remote);
	void (*custom_smiley_write)(PurpleConversation *conv, const char *smile,
	                            const guchar *data, gsize size);
	void (*custom_smiley_close)(PurpleConversation *conv, const char *smile);

	/** Prompt the user for confirmation to send @a message.  This function
	 *  should arrange for the message to be sent if the user accepts.  If
	 *  this field is @c NULL, libpurple will fall back to using
	 *  #purple_request_action().
	 */
	void (*send_confirm)(PurpleConversation *conv, const char *message);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Conversation API                                                */
/**************************************************************************/
/*@{*/

/** TODO GObjectify
 * Creates a new conversation of the specified type.
 *
 * @param type    The type of conversation.
 * @param account The account opening the conversation window on the purple
 *                user's end.
 * @param name    The name of the conversation. For PURPLE_CONVERSATION_TYPE_IM,
 *                this is the name of the buddy.
 *
 * @return The new conversation.
 */
PurpleConversation *purple_conversation_new(PurpleConversationType type,
										PurpleAccount *account,
										const char *name);

/** TODO dispose/fnalize
 * Destroys the specified conversation and removes it from the parent
 * window.
 *
 * If this conversation is the only one contained in the parent window,
 * that window is also destroyed.
 *
 * @param conv The conversation to destroy.
 */
void purple_conversation_destroy(PurpleConversation *conv);


/**
 * Present a conversation to the user. This allows core code to initiate a
 * conversation by displaying the IM dialog.
 * @param conv The conversation to present
 */
void purple_conversation_present(PurpleConversation *conv);


/** TODO REMOVE, return the GObject GType
 * Returns the specified conversation's type.
 *
 * @param conv The conversation.
 *
 * @return The conversation's type.
 */
PurpleConversationType purple_conversation_get_type(const PurpleConversation *conv);

/**
 * Sets the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 * @param ops  The UI conversation operations structure.
 */
void purple_conversation_set_ui_ops(PurpleConversation *conv,
								  PurpleConversationUiOps *ops);

/**
 * Returns the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 *
 * @return The operations structure.
 */
PurpleConversationUiOps *purple_conversation_get_ui_ops(const PurpleConversation *conv);

/**
 * Sets the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 * @param account The purple_account.
 */
void purple_conversation_set_account(PurpleConversation *conv,
                                   PurpleAccount *account);

/**
 * Returns the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 *
 * @return The conversation's purple_account.
 */
PurpleAccount *purple_conversation_get_account(const PurpleConversation *conv);

/**
 * Returns the specified conversation's purple_connection.
 *
 * @param conv The conversation.
 *
 * @return The conversation's purple_connection.
 */
PurpleConnection *purple_conversation_get_connection(const PurpleConversation *conv);

/**
 * Sets the specified conversation's title.
 *
 * @param conv  The conversation.
 * @param title The title.
 */
void purple_conversation_set_title(PurpleConversation *conv, const char *title);

/**
 * Returns the specified conversation's title.
 *
 * @param conv The conversation.
 *
 * @return The title.
 */
const char *purple_conversation_get_title(const PurpleConversation *conv);

/**
 * Automatically sets the specified conversation's title.
 *
 * This function takes OPT_IM_ALIAS_TAB into account, as well as the
 * user's alias.
 *
 * @param conv The conversation.
 */
void purple_conversation_autoset_title(PurpleConversation *conv);

/**
 * Sets the specified conversation's name.
 *
 * @param conv The conversation.
 * @param name The conversation's name.
 */
void purple_conversation_set_name(PurpleConversation *conv, const char *name);

/**
 * Returns the specified conversation's name.
 *
 * @param conv The conversation.
 *
 * @return The conversation's name. If the conversation is an IM with a PurpleBuddy,
 *         then it's the name of the PurpleBuddy.
 */
const char *purple_conversation_get_name(const PurpleConversation *conv);

/**
 * Enables or disables logging for this conversation.
 *
 * @param conv The conversation.
 * @param log  @c TRUE if logging should be enabled, or @c FALSE otherwise.
 */
void purple_conversation_set_logging(PurpleConversation *conv, gboolean log);

/**
 * Returns whether or not logging is enabled for this conversation.
 *
 * @param conv The conversation.
 *
 * @return @c TRUE if logging is enabled, or @c FALSE otherwise.
 */
gboolean purple_conversation_is_logging(const PurpleConversation *conv);

/**
 * Closes any open logs for this conversation.
 *
 * Note that new logs will be opened as necessary (e.g. upon receipt of a
 * message, if the conversation has logging enabled. To disable logging for
 * the remainder of the conversation, use purple_conversation_set_logging().
 *
 * @param conv The conversation.
 */
void purple_conversation_close_logs(PurpleConversation *conv);

/**
 * Sets extra data for a conversation.
 *
 * @param conv The conversation.
 * @param key  The unique key.
 * @param data The data to assign.
 */
void purple_conversation_set_data(PurpleConversation *conv, const char *key,
								gpointer data);

/**
 * Returns extra data in a conversation.
 *
 * @param conv The conversation.
 * @param key  The unqiue key.
 *
 * @return The data associated with the key.
 */
gpointer purple_conversation_get_data(PurpleConversation *conv, const char *key);

/**
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * purple_im_conversation_write() and purple_chat_conversation_write() instead. Those functions will
 * most likely call this anyway, but they may do their own formatting,
 * sound playback, etc.
 *
 * This can be used to write generic messages, such as "so and so closed
 * the conversation window."
 *
 * @param conv    The conversation.
 * @param who     The user who sent the message.
 * @param message The message.
 * @param flags   The message flags.
 * @param mtime   The time the message was sent.
 *
 * @see purple_im_conversation_write()
 * @see purple_chat_conversation_write()
 */
void purple_conversation_write(PurpleConversation *conv, const char *who,
		const char *message, PurpleConversationMessageFlags flags,
		time_t mtime);

/**
	Set the features as supported for the given conversation.
	@param conv      The conversation
	@param features  Bitset defining supported features
*/
void purple_conversation_set_features(PurpleConversation *conv,
		PurpleConnectionFlags features);


/**
	Get the features supported by the given conversation.
	@param conv  The conversation
*/
PurpleConnectionFlags purple_conversation_get_features(PurpleConversation *conv);

/**
 * Determines if a conversation has focus
 *
 * @param conv    The conversation.
 *
 * @return @c TRUE if the conversation has focus, @c FALSE if
 * it does not or the UI does not have a concept of conversation focus
 */
gboolean purple_conversation_has_focus(PurpleConversation *conv);

/**
 * Updates the visual status and UI of a conversation.
 *
 * @param conv The conversation.
 * @param type The update type.
 */
void purple_conversation_update(PurpleConversation *conv, PurpleConversationUpdateType type);

/**
 * Retrieve the message history of a conversation.
 *
 * @param conv   The conversation
 *
 * @return  A GList of PurpleConversationMessage's. The must not modify the list or the data within.
 *          The list contains the newest message at the beginning, and the oldest message at
 *          the end.
 */
GList *purple_conversation_get_message_history(PurpleConversation *conv);

/**
 * Clear the message history of a conversation.
 *
 * @param conv  The conversation
 */
void purple_conversation_clear_message_history(PurpleConversation *conv);

/**
 * Set the UI data associated with this conversation.
 *
 * @param conv			The conversation.
 * @param ui_data		A pointer to associate with this conversation.
 */
void purple_conversation_set_ui_data(PurpleConversation *conv, gpointer ui_data);

/**
 * Get the UI data associated with this conversation.
 *
 * @param conv			The conversation.
 *
 * @return The UI data associated with this conversation.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_conversation_get_ui_data(const PurpleConversation *conv);

/**
 * Sends a message to a conversation after confirming with
 * the user.
 *
 * This function is intended for use in cases where the user
 * hasn't explicitly and knowingly caused a message to be sent.
 * The confirmation ensures that the user isn't sending a
 * message by mistake.
 *
 * @param conv    The conversation.
 * @param message The message to send.
 */
void purple_conversation_send_confirm(PurpleConversation *conv, const char *message);

/**
 * Adds a smiley to the conversation's smiley tree. If this returns
 * @c TRUE you should call purple_conversation_custom_smiley_write() one or more
 * times, and then purple_conversation_custom_smiley_close(). If this returns
 * @c FALSE, either the conv or smile were invalid, or the icon was
 * found in the cache. In either case, calling write or close would
 * be an error.
 *
 * @param conv The conversation to associate the smiley with.
 * @param smile The text associated with the smiley
 * @param cksum_type The type of checksum.
 * @param chksum The checksum, as a NUL terminated base64 string.
 * @param remote @c TRUE if the custom smiley is set by the remote user (buddy).
 * @return      @c TRUE if an icon is expected, else FALSE. Note that
 *              it is an error to never call purple_conversation_custom_smiley_close if
 *              this function returns @c TRUE, but an error to call it if
 *              @c FALSE is returned.
 */

gboolean purple_conversation_custom_smiley_add(PurpleConversation *conv, const char *smile,
                                      const char *cksum_type, const char *chksum,
									  gboolean remote);

/**
 * Updates the image associated with the current smiley.
 *
 * @param conv The conversation associated with the smiley.
 * @param smile The text associated with the smiley.
 * @param data The actual image data.
 * @param size The length of the data.
 */

void purple_conversation_custom_smiley_write(PurpleConversation *conv,
                                   const char *smile,
                                   const guchar *data,
                                   gsize size);

/**
 * Close the custom smiley, all data has been written with
 * purple_conversation_custom_smiley_write, and it is no longer valid
 * to call that function on that smiley.
 *
 * @param conv The purple conversation associated with the smiley.
 * @param smile The text associated with the smiley
 */

void purple_conversation_custom_smiley_close(PurpleConversation *conv, const char *smile);

/**
 * Retrieves the extended menu items for the conversation.
 *
 * @param conv The conversation.
 *
 * @return  A list of PurpleMenuAction items, harvested by the
 *          chat-extended-menu signal. The list and the menuaction
 *          items should be freed by the caller.
 */
GList * purple_conversation_get_extended_menu(PurpleConversation *conv);

/**
 * Perform a command in a conversation. Similar to @see purple_cmd_do_command
 *
 * @param conv    The conversation.
 * @param cmdline The entire command including the arguments.
 * @param markup  @c NULL, or the formatted command line.
 * @param error   If the command failed errormsg is filled in with the appropriate error
 *                message, if not @c NULL. It must be freed by the caller with g_free().
 *
 * @return  @c TRUE if the command was executed successfully, @c FALSE otherwise.
 */
gboolean purple_conversation_do_command(PurpleConversation *conv,
		const gchar *cmdline, const gchar *markup, gchar **error);

/*@}*/

/**************************************************************************/
/** @name Conversation Helper API                                         */
/**************************************************************************/
/*@{*/

/**
 * Presents an IM-error to the user
 *
 * This is a helper function to find a conversation, write an error to it, and
 * raise the window.  If a conversation with this user doesn't already exist,
 * the function will return FALSE and the calling function can attempt to present
 * the error another way (purple_notify_error, most likely)
 *
 * @param who     The user this error is about
 * @param account The account this error is on
 * @param what    The error
 * @return        TRUE if the error was presented, else FALSE
 */
gboolean purple_conversation_helper_present_error(const char *who, PurpleAccount *account, const char *what);

/*@}*/

/**************************************************************************/
/** @name Conversation Message API                                        */
/**************************************************************************/
/*@{*/

/**
 * Get the sender from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The name of the sender of the message
 */
const char *purple_conversation_message_get_sender(const PurpleConversationMessage *msg);

/**
 * Get the message from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The name of the sender of the message
 */
const char *purple_conversation_message_get_message(const PurpleConversationMessage *msg);

/**
 * Get the message-flags of a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The message flags
 */
PurpleConversationMessageFlags purple_conversation_message_get_flags(const PurpleConversationMessage *msg);

/**
 * Get the timestamp of a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The timestamp of the message
 */
time_t purple_conversation_message_get_timestamp(const PurpleConversationMessage *msg);

/**
 * Get the alias from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The alias of the sender of the message
 */
const char *purple_conversation_message_get_alias(const PurpleConversationMessage *msg);

/**
 * Get the conversation associated with the PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The conversation
 */
PurpleConversation *purple_conversation_message_get_conv(const PurpleConversationMessage *msg);

/*@}*/

/**************************************************************************/
/** @name IM Conversation API                                             */
/**************************************************************************/
/*@{*/

/**
 * Sets the IM's buddy icon.
 *
 * This should only be called from within Purple. You probably want to
 * call purple_buddy_icon_set_data().
 *
 * @param im   The IM.
 * @param icon The buddy icon.
 *
 * @see purple_buddy_icon_set_data()
 */
void purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon);

/**
 * Returns the IM's buddy icon.
 *
 * @param im The IM.
 *
 * @return The buddy icon.
 */
PurpleBuddyIcon *purple_im_conversation_get_icon(const PurpleIMConversation *im);

/**
 * Sets the IM's typing state.
 *
 * @param im    The IM.
 * @param state The typing state.
 */
void purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMConversationTypingState state);

/**
 * Returns the IM's typing state.
 *
 * @param im The IM.
 *
 * @return The IM's typing state.
 */
PurpleIMConversationTypingState purple_im_conversation_get_typing_state(const PurpleIMConversation *im);

/**
 * Starts the IM's typing timeout.
 *
 * @param im      The IM.
 * @param timeout The timeout.
 */
void purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout);

/**
 * Stops the IM's typing timeout.
 *
 * @param im The IM.
 */
void purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im);

/**
 * Returns the IM's typing timeout.
 *
 * @param im The IM.
 *
 * @return The timeout.
 */
guint purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im);

/**
 * Sets the quiet-time when no PURPLE_IM_CONVERSATION_TYPING messages will be sent.
 * Few protocols need this (maybe only MSN).  If the user is still
 * typing after this quiet-period, then another PURPLE_IM_CONVERSATION_TYPING message
 * will be sent.
 *
 * @param im  The IM.
 * @param val The number of seconds to wait before allowing another
 *            PURPLE_IM_CONVERSATION_TYPING message to be sent to the user.  Or 0 to
 *            not send another PURPLE_IM_CONVERSATION_TYPING message.
 */
void purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val);

/**
 * Returns the time after which another PURPLE_IM_CONVERSATION_TYPING message should be sent.
 *
 * @param im The IM.
 *
 * @return The time in seconds since the epoch.  Or 0 if no additional
 *         PURPLE_IM_CONVERSATION_TYPING message should be sent.
 */
time_t purple_im_conversation_get_type_again(const PurpleIMConversation *im);

/**
 * Starts the IM's type again timeout.
 *
 * @param im      The IM.
 */
void purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im);

/**
 * Stops the IM's type again timeout.
 *
 * @param im The IM.
 */
void purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im);

/**
 * Returns the IM's type again timeout interval.
 *
 * @param im The IM.
 *
 * @return The type again timeout interval.
 */
guint purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im);

/**
 * Updates the visual typing notification for an IM conversation.
 *
 * @param im The IM.
 */
void purple_im_conversation_update_typing(PurpleIMConversation *im);

/**
 * Writes to an IM.
 *
 * @param im      The IM.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flags   The message flags.
 * @param mtime   The time the message was sent.
 */
void purple_im_conversation_write(PurpleIMConversation *im, const char *who,
						const char *message, PurpleConversationMessageFlags flags,
						time_t mtime);

/**
 * Sends a message to this IM conversation.
 *
 * @param im      The IM.
 * @param message The message to send.
 */
void purple_im_conversation_send(PurpleIMConversation *im, const char *message);

/**
 * Sends a message to this IM conversation with specified flags.
 *
 * @param im      The IM.
 * @param message The message to send.
 * @param flags   The PurpleConversationMessageFlags flags to use in addition to
 *                PURPLE_CONVERSATION_MESSAGE_SEND.
 */
void purple_im_conversation_send_with_flags(PurpleIMConversation *im,
		const char *message, PurpleConversationMessageFlags flags);

/*@}*/

/**************************************************************************/
/** @name Chat Conversation API                                           */
/**************************************************************************/
/*@{*/

/**
 * Returns a list of users in the chat room.  The members of the list
 * are PurpleChatConversationBuddy objects.
 *
 * @param chat The chat.
 *
 * @constreturn The list of users.
 */
GList *purple_chat_conversation_get_users(const PurpleChatConversation *chat);

/**
 * Ignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void purple_chat_conversation_ignore(PurpleChatConversation *chat, const char *name);

/**
 * Unignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void purple_chat_conversation_unignore(PurpleChatConversation *chat, const char *name);

/**
 * Sets the list of ignored users in the chat room.
 *
 * @param chat    The chat.
 * @param ignored The list of ignored users.
 *
 * @return The list passed.
 */
GList *purple_chat_conversation_set_ignored(PurpleChatConversation *chat, GList *ignored);

/**
 * Returns the list of ignored users in the chat room.
 *
 * @param chat The chat.
 *
 * @constreturn The list of ignored users.
 */
GList *purple_chat_conversation_get_ignored(const PurpleChatConversation *chat);

/**
 * Returns the actual name of the specified ignored user, if it exists in
 * the ignore list.
 *
 * If the user found contains a prefix, such as '+' or '\@', this is also
 * returned. The username passed to the function does not have to have this
 * formatting.
 *
 * @param chat The chat.
 * @param user The user to check in the ignore list.
 *
 * @return The ignored user if found, complete with prefixes, or @c NULL
 *         if not found.
 */
const char *purple_chat_conversation_get_ignored_user(const PurpleChatConversation *chat,
											const char *user);

/**
 * Returns @c TRUE if the specified user is ignored.
 *
 * @param chat The chat.
 * @param user The user.
 *
 * @return @c TRUE if the user is in the ignore list; @c FALSE otherwise.
 */
gboolean purple_chat_conversation_is_ignored_user(const PurpleChatConversation *chat,
										const char *user);

/**
 * Sets the chat room's topic.
 *
 * @param chat  The chat.
 * @param who   The user that set the topic.
 * @param topic The topic.
 */
void purple_chat_conversation_set_topic(PurpleChatConversation *chat, const char *who,
							  const char *topic);

/**
 * Returns the chat room's topic.
 *
 * @param chat The chat.
 *
 * @return The chat's topic.
 */
const char *purple_chat_conversation_get_topic(const PurpleChatConversation *chat);

/**
 * Sets the chat room's ID.
 *
 * @param chat The chat.
 * @param id   The ID.
 */
void purple_chat_conversation_set_id(PurpleChatConversation *chat, int id);

/**
 * Returns the chat room's ID.
 *
 * @param chat The chat.
 *
 * @return The ID.
 */
int purple_chat_conversation_get_id(const PurpleChatConversation *chat);

/**
 * Writes to a chat.
 *
 * @param chat    The chat.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flags   The flags.
 * @param mtime   The time the message was sent.
 */
void purple_chat_conversation_write(PurpleChatConversation *chat, const char *who,
						  const char *message, PurpleConversationMessageFlags flags,
						  time_t mtime);

/**
 * Sends a message to this chat conversation.
 *
 * @param chat    The chat.
 * @param message The message to send.
 */
void purple_chat_conversation_send(PurpleChatConversation *chat, const char *message);

/**
 * Sends a message to this chat conversation with specified flags.
 *
 * @param chat    The chat.
 * @param message The message to send.
 * @param flags   The PurpleConversationMessageFlags flags to use.
 */
void purple_chat_conversation_send_with_flags(PurpleChatConversation *chat,
		const char *message, PurpleConversationMessageFlags flags);

/**
 * Adds a user to a chat.
 *
 * @param chat        The chat.
 * @param user        The user to add.
 * @param extra_msg   An extra message to display with the join message.
 * @param flags       The users flags
 * @param new_arrival Decides whether or not to show a join notice.
 */
void purple_chat_conversation_add_user(PurpleChatConversation *chat, const char *user,
							 const char *extra_msg, PurpleChatConversationBuddyFlags flags,
							 gboolean new_arrival);

/**
 * Adds a list of users to a chat.
 *
 * The data is copied from @a users, @a extra_msgs, and @a flags, so it is up to
 * the caller to free this list after calling this function.
 *
 * @param chat         The chat.
 * @param users        The list of users to add.
 * @param extra_msgs   An extra message to display with the join message for each
 *                     user.  This list may be shorter than @a users, in which
 *                     case, the users after the end of extra_msgs will not have
 *                     an extra message.  By extension, this means that extra_msgs
 *                     can simply be @c NULL and none of the users will have an
 *                     extra message.
 * @param flags        The list of flags for each user.
 * @param new_arrivals Decides whether or not to show join notices.
 */
void purple_chat_conversation_add_users(PurpleChatConversation *chat,
		GList *users, GList *extra_msgs, GList *flags, gboolean new_arrivals);

/**
 * Renames a user in a chat.
 *
 * @param chat     The chat.
 * @param old_user The old username.
 * @param new_user The new username.
 */
void purple_chat_conversation_rename_user(PurpleChatConversation *chat,
		const char *old_user, const char *new_user);

/**
 * Removes a user from a chat, optionally with a reason.
 *
 * It is up to the developer to free this list after calling this function.
 *
 * @param chat   The chat.
 * @param user   The user that is being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void purple_chat_conversation_remove_user(PurpleChatConversation *chat,
		const char *user, const char *reason);

/**
 * Removes a list of users from a chat, optionally with a single reason.
 *
 * @param chat   The chat.
 * @param users  The users that are being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void purple_chat_conversation_remove_users(PurpleChatConversation *chat,
		GList *users, const char *reason);

/**
 * Finds a user in a chat
 *
 * @param chat   The chat.
 * @param user   The user to look for.
 *
 * @return TRUE if the user is in the chat, FALSE if not
 */
gboolean purple_chat_conversation_find_user(PurpleChatConversation *chat,
		const char *user);

/**
 * Set a users flags in a chat
 *
 * @param chat   The chat.
 * @param user   The user to update.
 * @param flags  The new flags.
 */
void purple_chat_conversation_user_set_flags(PurpleChatConversation *chat,
		const char *user, PurpleChatConversationBuddyFlags flags);

/**
 * Get the flags for a user in a chat
 *
 * @param chat   The chat.
 * @param user   The user to find the flags for
 *
 * @return The flags for the user
 */
PurpleChatConversationBuddyFlags purple_chat_conversation_user_get_flags(PurpleChatConversation *chat,
													 const char *user);

/**
 * Clears all users from a chat.
 *
 * @param chat The chat.
 */
void purple_chat_conversation_clear_users(PurpleChatConversation *chat);

/**
 * Sets your nickname (used for hilighting) for a chat.
 *
 * @param chat The chat.
 * @param nick The nick.
 */
void purple_chat_conversation_set_nick(PurpleChatConversation *chat,
		const char *nick);

/**
 * Gets your nickname (used for hilighting) for a chat.
 *
 * @param chat The chat.
 * @return  The nick.
 */
const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat);

/**
 * Lets the core know we left a chat, without destroying it.
 * Called from serv_got_chat_left().
 *
 * @param chat The chat.
 */
void purple_chat_conversation_leave(PurpleChatConversation *chat);

/**
 * Find a chat buddy in a chat
 *
 * @param chat The chat.
 * @param name The name of the chat buddy to find.
 */
PurpleChatConversationBuddy *purple_chat_conversation_find_buddy(PurpleChatConversation *chat, const char *name);

/**
 * Invite a user to a chat.
 * The user will be prompted to enter the user's name or a message if one is
 * not given.
 *
 * @param chat     The chat.
 * @param user     The user to invite to the chat.
 * @param message  The message to send with the invitation.
 * @param confirm  Prompt before sending the invitation. The user is always
 *                 prompted if either \a user or \a message is @c NULL.
 */
void purple_chat_conversation_invite_user(PurpleChatConversation *chat,
		const char *user, const char *message, gboolean confirm);

/**
 * Returns true if we're no longer in this chat,
 * and just left the window open.
 *
 * @param chat The chat.
 *
 * @return @c TRUE if we left the chat already, @c FALSE if
 * we're still there.
 */
gboolean purple_chat_conversation_has_left(PurpleChatConversation *chat);

/*@}*/

/**************************************************************************/
/** @name Chat Conversation Buddy API                                     */
/**************************************************************************/
/*@{*/

/**
 * Get an attribute of a chat buddy
 *
 * @param cb	The chat buddy.
 * @param key	The key of the attribute.
 *
 * @return The value of the attribute key.
 */
const char *purple_chat_conversation_buddy_get_attribute(PurpleChatConversationBuddy *cb, const char *key);

/**
 * Get the keys of all atributes of a chat buddy
 *
 * @param cb	The chat buddy.
 *
 * @return A list of the attributes of a chat buddy.
 */
GList *purple_chat_conversation_buddy_get_attribute_keys(PurpleChatConversationBuddy *cb);
	
/**
 * Set an attribute of a chat buddy
 *
 * @param chat	The chat.
 * @param cb	The chat buddy.
 * @param key	The key of the attribute.
 * @param value	The value of the attribute.
 */
void purple_chat_conversation_buddy_set_attribute(PurpleChatConversation *chat,
		PurpleChatConversationBuddy *cb, const char *key, const char *value);

/**
 * Set attributes of a chat buddy
 *
 * @param chat	The chat.
 * @param cb	The chat buddy.
 * @param keys	A GList of the keys.
 * @param values A GList of the values.
 */
void
purple_chat_conversation_buddy_set_attributes(PurpleChatConversation *chat,
		PurpleChatConversationBuddy *cb, GList *keys, GList *values);

/** TODO GObjectify
 * Creates a new chat buddy
 *
 * @param name The name.
 * @param alias The alias.
 * @param flags The flags.
 *
 * @return The new chat buddy
 */
PurpleChatConversationBuddy *purple_chat_conversation_buddy_new(const char *name,
		const char *alias, PurpleChatConversationBuddyFlags flags);

/**
 * Set the UI data associated with this chat buddy.
 *
 * @param cb			The chat buddy
 * @param ui_data		A pointer to associate with this chat buddy.
 */
void purple_chat_conversation_buddy_set_ui_data(PurpleChatConversationBuddy *cb, gpointer ui_data);

/**
 * Get the UI data associated with this chat buddy.
 *
 * @param cb			The chat buddy.
 *
 * @return The UI data associated with this chat buddy.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_chat_conversation_buddy_get_ui_data(const PurpleChatConversationBuddy *cb);

/**
 * Get the alias of a chat buddy
 *
 * @param cb    The chat buddy.
 *
 * @return The alias of the chat buddy.
 */
const char *purple_chat_conversation_buddy_get_alias(const PurpleChatConversationBuddy *cb);

/**
 * Get the name of a chat buddy
 *
 * @param cb    The chat buddy.
 *
 * @return The name of the chat buddy.
 */
const char *purple_chat_conversation_buddy_get_name(const PurpleChatConversationBuddy *cb);

/**
 * Get the flags of a chat buddy.
 *
 * @param cb	The chat buddy.
 *
 * @return The flags of the chat buddy.
 */
PurpleChatConversationBuddyFlags purple_chat_conversation_buddy_get_flags(const PurpleChatConversationBuddy *cb);

/**
 * Indicates if this chat buddy is on the buddy list.
 *
 * @param cb	The chat buddy.
 *
 * @return TRUE if the chat buddy is on the buddy list.
 */
gboolean purple_chat_conversation_buddy_is_buddy(const PurpleChatConversationBuddy *cb);

/** TODO finalize/dispose
 * Destroys a chat buddy
 *
 * @param cb The chat buddy to destroy
 */
void purple_chat_conversation_buddy_destroy(PurpleChatConversationBuddy *cb);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CONVERSATION_H_ */
