/**
 * @file conversation.h Conversation API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @see @ref conversation-signals
 */
#ifndef _GAIM_CONVERSATION_H_
#define _GAIM_CONVERSATION_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/


typedef struct _GaimConversationUiOps GaimConversationUiOps;
typedef struct _GaimConversation      GaimConversation;
typedef struct _GaimConvIm            GaimConvIm;
typedef struct _GaimConvChat          GaimConvChat;
typedef struct _GaimConvChatBuddy     GaimConvChatBuddy;

/**
 * A type of conversation.
 */
typedef enum
{
	GAIM_CONV_TYPE_UNKNOWN = 0, /**< Unknown conversation type. */
	GAIM_CONV_TYPE_IM,          /**< Instant Message.           */
	GAIM_CONV_TYPE_CHAT,        /**< Chat room.                 */
	GAIM_CONV_TYPE_MISC,        /**< A misc. conversation.      */
	GAIM_CONV_TYPE_ANY          /**< Any type of conversation.  */

} GaimConversationType;

/**
 * Conversation update type.
 */
typedef enum
{
	GAIM_CONV_UPDATE_ADD = 0, /**< The buddy associated with the conversation
	                               was added.   */
	GAIM_CONV_UPDATE_REMOVE,  /**< The buddy associated with the conversation
	                               was removed. */
	GAIM_CONV_UPDATE_ACCOUNT, /**< The gaim_account was changed. */
	GAIM_CONV_UPDATE_TYPING,  /**< The typing state was updated. */
	GAIM_CONV_UPDATE_UNSEEN,  /**< The unseen state was updated. */
	GAIM_CONV_UPDATE_LOGGING, /**< Logging for this conversation was
	                               enabled or disabled. */
	GAIM_CONV_UPDATE_TOPIC,   /**< The topic for a chat was updated. */
	/*
	 * XXX These need to go when we implement a more generic core/UI event
	 * system.
	 */
	GAIM_CONV_ACCOUNT_ONLINE,  /**< One of the user's accounts went online.  */
	GAIM_CONV_ACCOUNT_OFFLINE, /**< One of the user's accounts went offline. */
	GAIM_CONV_UPDATE_AWAY,     /**< The other user went away.                */
	GAIM_CONV_UPDATE_ICON,     /**< The other user's buddy icon changed.     */
	GAIM_CONV_UPDATE_TITLE,
	GAIM_CONV_UPDATE_CHATLEFT,

	GAIM_CONV_UPDATE_FEATURES, /**< The features for a chat have changed */

} GaimConvUpdateType;

/**
 * The typing state of a user.
 */
typedef enum
{
	GAIM_NOT_TYPING = 0,  /**< Not typing.                 */
	GAIM_TYPING,          /**< Currently typing.           */
	GAIM_TYPED            /**< Stopped typing momentarily. */

} GaimTypingState;

/**
 * Flags applicable to a message. Most will have send, recv or system.
 */
typedef enum
{
	GAIM_MESSAGE_SEND        = 0x0001, /**< Outgoing message.        */
	GAIM_MESSAGE_RECV        = 0x0002, /**< Incoming message.        */
	GAIM_MESSAGE_SYSTEM      = 0x0004, /**< System message.          */
	GAIM_MESSAGE_AUTO_RESP   = 0x0008, /**< Auto response.           */
	GAIM_MESSAGE_ACTIVE_ONLY = 0x0010,  /**< Hint to the UI that this
	                                        message should not be
	                                        shown in conversations
	                                        which are only open for
	                                        internal UI purposes
	                                        (e.g. for contact-aware
	                                         conversions).           */
	GAIM_MESSAGE_NICK        = 0x0020, /**< Contains your nick.      */
	GAIM_MESSAGE_NO_LOG      = 0x0040, /**< Do not log.              */
	GAIM_MESSAGE_WHISPER     = 0x0080, /**< Whispered message.       */
	GAIM_MESSAGE_ERROR       = 0x0200, /**< Error message.           */
	GAIM_MESSAGE_DELAYED     = 0x0400, /**< Delayed message.         */
	GAIM_MESSAGE_RAW         = 0x0800, /**< "Raw" message - don't
	                                        apply formatting         */
	GAIM_MESSAGE_IMAGES      = 0x1000  /**< Message contains images  */

} GaimMessageFlags;

/**
 * Flags applicable to users in Chats.
 */
typedef enum
{
	GAIM_CBFLAGS_NONE          = 0x0000, /**< No flags                     */
	GAIM_CBFLAGS_VOICE         = 0x0001, /**< Voiced user or "Participant" */
	GAIM_CBFLAGS_HALFOP        = 0x0002, /**< Half-op                      */
	GAIM_CBFLAGS_OP            = 0x0004, /**< Channel Op or Moderator      */
	GAIM_CBFLAGS_FOUNDER       = 0x0008, /**< Channel Founder              */
	GAIM_CBFLAGS_TYPING        = 0x0010, /**< Currently typing             */

} GaimConvChatBuddyFlags;

#include "account.h"
#include "buddyicon.h"
#include "log.h"
#include "server.h"

/**
 * Conversation operations and events.
 *
 * Any UI representing a conversation must assign a filled-out
 * GaimConversationUiOps structure to the GaimConversation.
 */
struct _GaimConversationUiOps
{
	void (*create_conversation)(GaimConversation *conv);
	void (*destroy_conversation)(GaimConversation *conv);
	void (*write_chat)(GaimConversation *conv, const char *who,
	                   const char *message, GaimMessageFlags flags,
	                   time_t mtime);
	void (*write_im)(GaimConversation *conv, const char *who,
	                 const char *message, GaimMessageFlags flags,
	                 time_t mtime);
	void (*write_conv)(GaimConversation *conv, const char *name, const char *alias,
	                   const char *message, GaimMessageFlags flags,
	                   time_t mtime);

	void (*chat_add_users)(GaimConversation *conv, GList *users,
						   GList *flags, GList *aliases, gboolean new_arrivals);
	void (*chat_rename_user)(GaimConversation *conv, const char *old_name,
	                         const char *new_name, const char *new_alias);
	void (*chat_remove_users)(GaimConversation *conv, GList *users);
	void (*chat_update_user)(GaimConversation *conv, const char *user);

	void (*present)(GaimConversation *conv);

	gboolean (*has_focus)(GaimConversation *conv);

	/* Custom Smileys */
	gboolean (*custom_smiley_add)(GaimConversation *conv, const char *smile, gboolean remote);
	void (*custom_smiley_write)(GaimConversation *conv, const char *smile,
	                            const guchar *data, gsize size);
	void (*custom_smiley_close)(GaimConversation *conv, const char *smile);
};

/**
 * Data specific to Instant Messages.
 */
struct _GaimConvIm
{
	GaimConversation *conv;            /**< The parent conversation.     */

	GaimTypingState typing_state;      /**< The current typing state.    */
	guint  typing_timeout;             /**< The typing timer handle.     */
	time_t type_again;                 /**< The type again time.         */
	guint  send_typed_timeout;         /**< The type again timer handle. */

	GaimBuddyIcon *icon;               /**< The buddy icon.              */
};

/**
 * Data specific to Chats.
 */
struct _GaimConvChat
{
	GaimConversation *conv;          /**< The parent conversation.      */

	GList *in_room;                  /**< The users in the room.        */
	GList *ignored;                  /**< Ignored users.                */
	char  *who;                      /**< The person who set the topic. */
	char  *topic;                    /**< The topic.                    */
	int    id;                       /**< The chat ID.                  */
	char *nick;                      /**< Your nick in this chat.       */

	gboolean left;                   /**< We left the chat and kept the window open */
};

/**
 * Data for "Chat Buddies"
 */
struct _GaimConvChatBuddy
{
	char *name;                      /**< The name                      */
	GaimConvChatBuddyFlags flags;    /**< Flags (ops, voice etc.)       */
};

/**
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat.
 */
struct _GaimConversation
{
	GaimConversationType type;  /**< The type of conversation.          */

	GaimAccount *account;       /**< The user using this conversation.  */


	char *name;                 /**< The name of the conversation.      */
	char *title;                /**< The window title.                  */

	gboolean logging;           /**< The status of logging.             */

	GList *logs;                /**< This conversation's logs           */

	union
	{
		GaimConvIm   *im;       /**< IM-specific data.                  */
		GaimConvChat *chat;     /**< Chat-specific data.                */
		void *misc;             /**< Misc. data.                        */

	} u;

	GaimConversationUiOps *ui_ops;           /**< UI-specific operations. */
	void *ui_data;                           /**< UI-specific data.       */

	GHashTable *data;                        /**< Plugin-specific data.   */

	GaimConnectionFlags features; /**< The supported features */

};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Conversation API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new conversation of the specified type.
 *
 * @param type    The type of conversation.
 * @param account The account opening the conversation window on the gaim
 *                user's end.
 * @param name    The name of the conversation.
 *
 * @return The new conversation.
 */
GaimConversation *gaim_conversation_new(GaimConversationType type,
										GaimAccount *account,
										const char *name);

/**
 * Destroys the specified conversation and removes it from the parent
 * window.
 *
 * If this conversation is the only one contained in the parent window,
 * that window is also destroyed.
 *
 * @param conv The conversation to destroy.
 */
void gaim_conversation_destroy(GaimConversation *conv);


/**
 * Present a conversation to the user. This allows core code to initiate a
 * conversation by displaying the IM dialog.
 * @param conv The conversation to present
 */
void gaim_conversation_present(GaimConversation *conv);


/**
 * Returns the specified conversation's type.
 *
 * @param conv The conversation.
 *
 * @return The conversation's type.
 */
GaimConversationType gaim_conversation_get_type(const GaimConversation *conv);

/**
 * Sets the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 * @param ops  The UI conversation operations structure.
 */
void gaim_conversation_set_ui_ops(GaimConversation *conv,
								  GaimConversationUiOps *ops);

/**
 * Sets the default conversation UI operations structure.
 *
 * @param ops  The UI conversation operations structure.
 */
void gaim_conversations_set_ui_ops(GaimConversationUiOps *ops);

/**
 * Returns the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 *
 * @return The operations structure.
 */
GaimConversationUiOps *gaim_conversation_get_ui_ops(
		const GaimConversation *conv);

/**
 * Sets the specified conversation's gaim_account.
 *
 * This gaim_account represents the user using gaim, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 * @param account The gaim_account.
 */
void gaim_conversation_set_account(GaimConversation *conv,
                                   GaimAccount *account);

/**
 * Returns the specified conversation's gaim_account.
 *
 * This gaim_account represents the user using gaim, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 *
 * @return The conversation's gaim_account.
 */
GaimAccount *gaim_conversation_get_account(const GaimConversation *conv);

/**
 * Returns the specified conversation's gaim_connection.
 *
 * This is the same as gaim_conversation_get_user(conv)->gc.
 *
 * @param conv The conversation.
 *
 * @return The conversation's gaim_connection.
 */
GaimConnection *gaim_conversation_get_gc(const GaimConversation *conv);

/**
 * Sets the specified conversation's title.
 *
 * @param conv  The conversation.
 * @param title The title.
 */
void gaim_conversation_set_title(GaimConversation *conv, const char *title);

/**
 * Returns the specified conversation's title.
 *
 * @param conv The conversation.
 *
 * @return The title.
 */
const char *gaim_conversation_get_title(const GaimConversation *conv);

/**
 * Automatically sets the specified conversation's title.
 *
 * This function takes OPT_IM_ALIAS_TAB into account, as well as the
 * user's alias.
 *
 * @param conv The conversation.
 */
void gaim_conversation_autoset_title(GaimConversation *conv);

/**
 * Sets the specified conversation's name.
 *
 * @param conv The conversation.
 * @param name The conversation's name.
 */
void gaim_conversation_set_name(GaimConversation *conv, const char *name);

/**
 * Returns the specified conversation's name.
 *
 * @param conv The conversation.
 *
 * @return The conversation's name.
 */
const char *gaim_conversation_get_name(const GaimConversation *conv);

/**
 * Enables or disables logging for this conversation.
 *
 * @param conv The conversation.
 * @param log  @c TRUE if logging should be enabled, or @c FALSE otherwise.
 */
void gaim_conversation_set_logging(GaimConversation *conv, gboolean log);

/**
 * Returns whether or not logging is enabled for this conversation.
 *
 * @param conv The conversation.
 *
 * @return @c TRUE if logging is enabled, or @c FALSE otherwise.
 */
gboolean gaim_conversation_is_logging(const GaimConversation *conv);

/**
 * Closes any open logs for this conversation.
 *
 * Note that new logs will be opened as necessary (e.g. upon receipt of a
 * message, if the conversation has logging enabled. To disable logging for
 * the remainder of the conversation, use gaim_conversation_set_logging().
 *
 * @param conv The conversation.
 */
void gaim_conversation_close_logs(GaimConversation *conv);

/**
 * Returns the specified conversation's IM-specific data.
 *
 * If the conversation type is not GAIM_CONV_TYPE_IM, this will return @c NULL.
 *
 * @param conv The conversation.
 *
 * @return The IM-specific data.
 */
GaimConvIm *gaim_conversation_get_im_data(const GaimConversation *conv);

#define GAIM_CONV_IM(c) (gaim_conversation_get_im_data(c))

/**
 * Returns the specified conversation's chat-specific data.
 *
 * If the conversation type is not GAIM_CONV_TYPE_CHAT, this will return @c NULL.
 *
 * @param conv The conversation.
 *
 * @return The chat-specific data.
 */
GaimConvChat *gaim_conversation_get_chat_data(const GaimConversation *conv);

#define GAIM_CONV_CHAT(c) (gaim_conversation_get_chat_data(c))

/**
 * Sets extra data for a conversation.
 *
 * @param conv The conversation.
 * @param key  The unique key.
 * @param data The data to assign.
 */
void gaim_conversation_set_data(GaimConversation *conv, const char *key,
								gpointer data);

/**
 * Returns extra data in a conversation.
 *
 * @param conv The conversation.
 * @param key  The unqiue key.
 *
 * @return The data associated with the key.
 */
gpointer gaim_conversation_get_data(GaimConversation *conv, const char *key);

/**
 * Returns a list of all conversations.
 *
 * This list includes both IMs and chats.
 *
 * @return A GList of all conversations.
 */
GList *gaim_get_conversations(void);

/**
 * Returns a list of all IMs.
 *
 * @return A GList of all IMs.
 */
GList *gaim_get_ims(void);

/**
 * Returns a list of all chats.
 *
 * @return A GList of all chats.
 */
GList *gaim_get_chats(void);

/**
 * Finds a conversation with the specified type, name, and Gaim account.
 *
 * @param type The type of the conversation.
 * @param name The name of the conversation.
 * @param account The gaim_account associated with the conversation.
 *
 * @return The conversation if found, or @c NULL otherwise.
 */
GaimConversation *gaim_find_conversation_with_account(
		GaimConversationType type, const char *name,
		const GaimAccount *account);

/**
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * gaim_conv_im_write() and gaim_conv_chat_write() instead. Those functions will
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
 * @see gaim_conv_im_write()
 * @see gaim_conv_chat_write()
 */
void gaim_conversation_write(GaimConversation *conv, const char *who,
		const char *message, GaimMessageFlags flags,
		time_t mtime);


/**
	Set the features as supported for the given conversation.
	@param conv      The conversation
	@param features  Bitset defining supported features
*/
void gaim_conversation_set_features(GaimConversation *conv,
		GaimConnectionFlags features);


/**
	Get the features supported by the given conversation.
	@param conv  The conversation
*/
GaimConnectionFlags gaim_conversation_get_features(GaimConversation *conv);

/**
 * Determines if a conversation has focus
 *
 * @param conv    The conversation.
 *
 * @return @c TRUE if the conversation has focus, @c FALSE if
 * it does not or the UI does not have a concept of conversation focus
 */
gboolean gaim_conversation_has_focus(GaimConversation *conv);

/**
 * Updates the visual status and UI of a conversation.
 *
 * @param conv The conversation.
 * @param type The update type.
 */
void gaim_conversation_update(GaimConversation *conv, GaimConvUpdateType type);

/**
 * Calls a function on each conversation.
 *
 * @param func The function.
 */
void gaim_conversation_foreach(void (*func)(GaimConversation *conv));

/*@}*/


/**************************************************************************/
/** @name IM Conversation API                                             */
/**************************************************************************/
/*@{*/

/**
 * Gets an IM's parent conversation.
 *
 * @param im The IM.
 *
 * @return The parent conversation.
 */
GaimConversation *gaim_conv_im_get_conversation(const GaimConvIm *im);

/**
 * Sets the IM's buddy icon.
 *
 * This should only be called from within Gaim. You probably want to
 * call gaim_buddy_icon_set_data().
 *
 * @param im   The IM.
 * @param icon The buddy icon.
 *
 * @see gaim_buddy_icon_set_data()
 */
void gaim_conv_im_set_icon(GaimConvIm *im, GaimBuddyIcon *icon);

/**
 * Returns the IM's buddy icon.
 *
 * @param im The IM.
 *
 * @return The buddy icon.
 */
GaimBuddyIcon *gaim_conv_im_get_icon(const GaimConvIm *im);

/**
 * Sets the IM's typing state.
 *
 * @param im    The IM.
 * @param state The typing state.
 */
void gaim_conv_im_set_typing_state(GaimConvIm *im, GaimTypingState state);

/**
 * Returns the IM's typing state.
 *
 * @param im The IM.
 *
 * @return The IM's typing state.
 */
GaimTypingState gaim_conv_im_get_typing_state(const GaimConvIm *im);

/**
 * Starts the IM's typing timeout.
 *
 * @param im      The IM.
 * @param timeout The timeout.
 */
void gaim_conv_im_start_typing_timeout(GaimConvIm *im, int timeout);

/**
 * Stops the IM's typing timeout.
 *
 * @param im The IM.
 */
void gaim_conv_im_stop_typing_timeout(GaimConvIm *im);

/**
 * Returns the IM's typing timeout.
 *
 * @param im The IM.
 *
 * @return The timeout.
 */
guint gaim_conv_im_get_typing_timeout(const GaimConvIm *im);

/**
 * Sets the quiet-time when no GAIM_TYPING messages will be sent.
 * Few protocols need this (maybe only MSN).  If the user is still
 * typing after this quiet-period, then another GAIM_TYPING message
 * will be sent.
 *
 * @param im  The IM.
 * @param val The number of seconds to wait before allowing another
 *            GAIM_TYPING message to be sent to the user.  Or 0 to
 *            not send another GAIM_TYPING message.
 */
void gaim_conv_im_set_type_again(GaimConvIm *im, unsigned int val);

/**
 * Returns the time after which another GAIM_TYPING message should be sent.
 *
 * @param im The IM.
 *
 * @return The time in seconds since the epoch.  Or 0 if no additional
 *         GAIM_TYPING message should be sent.
 */
time_t gaim_conv_im_get_type_again(const GaimConvIm *im);

/**
 * Starts the IM's type again timeout.
 *
 * @param im      The IM.
 */
void gaim_conv_im_start_send_typed_timeout(GaimConvIm *im);

/**
 * Stops the IM's type again timeout.
 *
 * @param im The IM.
 */
void gaim_conv_im_stop_send_typed_timeout(GaimConvIm *im);

/**
 * Returns the IM's type again timeout interval.
 *
 * @param im The IM.
 *
 * @return The type again timeout interval.
 */
guint gaim_conv_im_get_send_typed_timeout(const GaimConvIm *im);

/**
 * Updates the visual typing notification for an IM conversation.
 *
 * @param im The IM.
 */
void gaim_conv_im_update_typing(GaimConvIm *im);

/**
 * Writes to an IM.
 *
 * @param im      The IM.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flags   The message flags.
 * @param mtime   The time the message was sent.
 */
void gaim_conv_im_write(GaimConvIm *im, const char *who,
						const char *message, GaimMessageFlags flags,
						time_t mtime);

/**
 * Presents an IM-error to the user
 *
 * This is a helper function to find a conversation, write an error to it, and
 * raise the window.  If a conversation with this user doesn't already exist,
 * the function will return FALSE and the calling function can attempt to present
 * the error another way (gaim_notify_error, most likely)
 *
 * @param who     The user this error is about
 * @param account The account this error is on
 * @param what    The error
 * @return        TRUE if the error was presented, else FALSE
 */
gboolean gaim_conv_present_error(const char *who, GaimAccount *account, const char *what);

/**
 * Sends a message to this IM conversation.
 *
 * @param im      The IM.
 * @param message The message to send.
 */
void gaim_conv_im_send(GaimConvIm *im, const char *message);

/**
 * Sends a message to this IM conversation with specified flags.
 *
 * @param im      The IM.
 * @param message The message to send.
 * @param flags   The GaimMessageFlags flags to use in addition to GAIM_MESSAGE_SEND.
 */
void gaim_conv_im_send_with_flags(GaimConvIm *im, const char *message, GaimMessageFlags flags);

/**
 * Adds a smiley to the conversation's smiley tree. If this returns
 * @c TRUE you should call gaim_conv_custom_smiley_write() one or more
 * times, and then gaim_conv_custom_smiley_close(). If this returns
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
 *              it is an error to never call gaim_conv_custom_smiley_close if
 *              this function returns @c TRUE, but an error to call it if
 *              @c FALSE is returned.
 */

gboolean gaim_conv_custom_smiley_add(GaimConversation *conv, const char *smile,
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

void gaim_conv_custom_smiley_write(GaimConversation *conv,
                                   const char *smile,
                                   const guchar *data,
                                   gsize size);

/**
 * Close the custom smiley, all data has been written with
 * gaim_conv_custom_smiley_write, and it is no longer valid
 * to call that function on that smiley.
 *
 * @param conv The gaim conversation associated with the smiley.
 * @param smile The text associated with the smiley
 */

void gaim_conv_custom_smiley_close(GaimConversation *conv, const char *smile);

/*@}*/


/**************************************************************************/
/** @name Chat Conversation API                                           */
/**************************************************************************/
/*@{*/

/**
 * Gets a chat's parent conversation.
 *
 * @param chat The chat.
 *
 * @return The parent conversation.
 */
GaimConversation *gaim_conv_chat_get_conversation(const GaimConvChat *chat);

/**
 * Sets the list of users in the chat room.
 *
 * @note Calling this function will not update the display of the users.
 *       Please use gaim_conv_chat_add_user(), gaim_conv_chat_add_users(),
 *       gaim_conv_chat_remove_user(), and gaim_conv_chat_remove_users() instead.
 *
 * @param chat  The chat.
 * @param users The list of users.
 *
 * @return The list passed.
 */
GList *gaim_conv_chat_set_users(GaimConvChat *chat, GList *users);

/**
 * Returns a list of users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of users.
 */
GList *gaim_conv_chat_get_users(const GaimConvChat *chat);

/**
 * Ignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_conv_chat_ignore(GaimConvChat *chat, const char *name);

/**
 * Unignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_conv_chat_unignore(GaimConvChat *chat, const char *name);

/**
 * Sets the list of ignored users in the chat room.
 *
 * @param chat    The chat.
 * @param ignored The list of ignored users.
 *
 * @return The list passed.
 */
GList *gaim_conv_chat_set_ignored(GaimConvChat *chat, GList *ignored);

/**
 * Returns the list of ignored users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of ignored users.
 */
GList *gaim_conv_chat_get_ignored(const GaimConvChat *chat);

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
const char *gaim_conv_chat_get_ignored_user(const GaimConvChat *chat,
											const char *user);

/**
 * Returns @c TRUE if the specified user is ignored.
 *
 * @param chat The chat.
 * @param user The user.
 *
 * @return @c TRUE if the user is in the ignore list; @c FALSE otherwise.
 */
gboolean gaim_conv_chat_is_user_ignored(const GaimConvChat *chat,
										const char *user);

/**
 * Sets the chat room's topic.
 *
 * @param chat  The chat.
 * @param who   The user that set the topic.
 * @param topic The topic.
 */
void gaim_conv_chat_set_topic(GaimConvChat *chat, const char *who,
							  const char *topic);

/**
 * Returns the chat room's topic.
 *
 * @param chat The chat.
 *
 * @return The chat's topic.
 */
const char *gaim_conv_chat_get_topic(const GaimConvChat *chat);

/**
 * Sets the chat room's ID.
 *
 * @param chat The chat.
 * @param id   The ID.
 */
void gaim_conv_chat_set_id(GaimConvChat *chat, int id);

/**
 * Returns the chat room's ID.
 *
 * @param chat The chat.
 *
 * @return The ID.
 */
int gaim_conv_chat_get_id(const GaimConvChat *chat);

/**
 * Writes to a chat.
 *
 * @param chat    The chat.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flags   The flags.
 * @param mtime   The time the message was sent.
 */
void gaim_conv_chat_write(GaimConvChat *chat, const char *who,
						  const char *message, GaimMessageFlags flags,
						  time_t mtime);

/**
 * Sends a message to this chat conversation.
 *
 * @param chat    The chat.
 * @param message The message to send.
 */
void gaim_conv_chat_send(GaimConvChat *chat, const char *message);

/**
 * Sends a message to this chat conversation with specified flags.
 *
 * @param chat    The chat.
 * @param message The message to send.
 * @param flags   The GaimMessageFlags flags to use.
 */
void gaim_conv_chat_send_with_flags(GaimConvChat *chat, const char *message, GaimMessageFlags flags);

/**
 * Adds a user to a chat.
 *
 * @param chat        The chat.
 * @param user        The user to add.
 * @param extra_msg   An extra message to display with the join message.
 * @param flags       The users flags
 * @param new_arrival Decides whether or not to show a join notice.
 */
void gaim_conv_chat_add_user(GaimConvChat *chat, const char *user,
							 const char *extra_msg, GaimConvChatBuddyFlags flags,
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
void gaim_conv_chat_add_users(GaimConvChat *chat, GList *users, GList *extra_msgs,
							  GList *flags, gboolean new_arrivals);

/**
 * Renames a user in a chat.
 *
 * @param chat     The chat.
 * @param old_user The old username.
 * @param new_user The new username.
 */
void gaim_conv_chat_rename_user(GaimConvChat *chat, const char *old_user,
								const char *new_user);

/**
 * Removes a user from a chat, optionally with a reason.
 *
 * It is up to the developer to free this list after calling this function.
 *
 * @param chat   The chat.
 * @param user   The user that is being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void gaim_conv_chat_remove_user(GaimConvChat *chat, const char *user,
								const char *reason);

/**
 * Removes a list of users from a chat, optionally with a single reason.
 *
 * @param chat   The chat.
 * @param users  The users that are being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void gaim_conv_chat_remove_users(GaimConvChat *chat, GList *users,
								 const char *reason);

/**
 * Finds a user in a chat
 *
 * @param chat   The chat.
 * @param user   The user to look for.
 *
 * @return TRUE if the user is in the chat, FALSE if not
 */
gboolean gaim_conv_chat_find_user(GaimConvChat *chat, const char *user);

/**
 * Set a users flags in a chat
 *
 * @param chat   The chat.
 * @param user   The user to update.
 * @param flags  The new flags.
 */
void gaim_conv_chat_user_set_flags(GaimConvChat *chat, const char *user,
								   GaimConvChatBuddyFlags flags);

/**
 * Get the flags for a user in a chat
 *
 * @param chat   The chat.
 * @param user   The user to find the flags for
 *
 * @return The flags for the user
 */
GaimConvChatBuddyFlags gaim_conv_chat_user_get_flags(GaimConvChat *chat,
													 const char *user);

/**
 * Clears all users from a chat.
 *
 * @param chat The chat.
 */
void gaim_conv_chat_clear_users(GaimConvChat *chat);

/**
 * Sets your nickname (used for hilighting) for a chat.
 *
 * @param chat The chat.
 * @param nick The nick.
 */
void gaim_conv_chat_set_nick(GaimConvChat *chat, const char *nick);

/**
 * Gets your nickname (used for hilighting) for a chat.
 *
 * @param chat The chat.
 * @return  The nick.
 */
const char *gaim_conv_chat_get_nick(GaimConvChat *chat);

/**
 * Finds a chat with the specified chat ID.
 *
 * @param gc The gaim_connection.
 * @param id The chat ID.
 *
 * @return The chat conversation.
 */
GaimConversation *gaim_find_chat(const GaimConnection *gc, int id);

/**
 * Lets the core know we left a chat, without destroying it.
 * Called from serv_got_chat_left().
 *
 * @param chat The chat.
 */
void gaim_conv_chat_left(GaimConvChat *chat);

/**
 * Returns true if we're no longer in this chat,
 * and just left the window open.
 *
 * @param chat The chat.
 *
 * @return @c TRUE if we left the chat already, @c FALSE if
 * we're still there.
 */
gboolean gaim_conv_chat_has_left(GaimConvChat *chat);

/**
 * Creates a new chat buddy
 *
 * @param name The name.
 * @param flags The flags.
 *
 * @return The new chat buddy
 */
GaimConvChatBuddy *gaim_conv_chat_cb_new(const char *name,
										GaimConvChatBuddyFlags flags);

/**
 * Find a chat buddy in a chat
 *
 * @param chat The chat.
 * @param name The name of the chat buddy to find.
 */
GaimConvChatBuddy *gaim_conv_chat_cb_find(GaimConvChat *chat, const char *name);

/**
 * Get the name of a chat buddy
 *
 * @param cb    The chat buddy.
 *
 * @return The name of the chat buddy.
 */
const char *gaim_conv_chat_cb_get_name(GaimConvChatBuddy *cb);

/**
 * Destroys a chat buddy
 *
 * @param cb The chat buddy to destroy
 */
void gaim_conv_chat_cb_destroy(GaimConvChatBuddy *cb);

/*@}*/

/**************************************************************************/
/** @name Conversations Subsystem                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns the conversation subsystem handle.
 *
 * @return The conversation subsystem handle.
 */
void *gaim_conversations_get_handle(void);

/**
 * Initializes the conversation subsystem.
 */
void gaim_conversations_init(void);

/**
 * Uninitializes the conversation subsystem.
 */
void gaim_conversations_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_CONVERSATION_H_ */
