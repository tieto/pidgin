/**
 * @file conversation.h Conversation API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
 */

#ifndef _GAIM_CONVERSATION_H_
#define _GAIM_CONVERSATION_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

typedef enum   _GaimConversationType  GaimConversationType;
typedef enum   _GaimUnseenState       GaimUnseenState;
typedef enum   _GaimConvUpdateType    GaimConvUpdateType;
typedef struct _GaimWindowUiOps       GaimWindowUiOps;
typedef struct _GaimWindow            GaimWindow;
typedef struct _GaimConversationUiOps GaimConversationUiOps;
typedef struct _GaimConversation      GaimConversation;
typedef struct _GaimIm                GaimIm;
typedef struct _GaimChat              GaimChat;

/**
 * A type of conversation.
 */
enum _GaimConversationType
{
	GAIM_CONV_UNKNOWN = 0, /**< Unknown conversation type. */
	GAIM_CONV_IM,          /**< Instant Message.           */
	GAIM_CONV_CHAT,        /**< Chat room.                 */
	GAIM_CONV_MISC         /**< A misc. conversation.      */
};

/**
 * Unseen text states.
 */
enum _GaimUnseenState
{
	GAIM_UNSEEN_NONE = 0,  /**< No unseen text in the conversation. */
	GAIM_UNSEEN_TEXT,      /**< Unseen text in the conversation.    */
	GAIM_UNSEEN_NICK,      /**< Unseen text and the nick was said.  */
};

/**
 * Conversation update type.
 */
enum _GaimConvUpdateType
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
	GAIM_CONV_UPDATE_AWAY      /**< The other user went away.                */
};

/* Yeah, this has to be included here. Ugh. */
#include "gaim.h"

/**
 * Conversation window operations.
 *
 * Any UI representing a window must assign a filled-out gaim_window_ops
 * structure to the GaimWindow.
 */
struct _GaimWindowUiOps
{
	GaimConversationUiOps *(*get_conversation_ui_ops)(void);

	void (*new_window)(GaimWindow *win);
	void (*destroy_window)(GaimWindow *win);

	void (*show)(GaimWindow *win);
	void (*hide)(GaimWindow *win);
	void (*raise)(GaimWindow *win);
	void (*flash)(GaimWindow *win);

	void (*switch_conversation)(GaimWindow *win, unsigned int index);
	void (*add_conversation)(GaimWindow *win, GaimConversation *conv);
	void (*remove_conversation)(GaimWindow *win, GaimConversation *conv);
	void (*move_conversation)(GaimWindow *win, GaimConversation *conv,
							  unsigned int newIndex);
	int (*get_active_index)(const GaimWindow *win);
};

/**
 * Conversation operations and events.
 *
 * Any UI representing a conversation must assign a filled-out
 * GaimConversationUiOps structure to the GaimConversation.
 */
struct _GaimConversationUiOps
{
	void (*destroy_conversation)(GaimConversation *conv);
	void (*write_chat)(GaimConversation *conv, const char *who,
					   const char *message, int flags, time_t mtime);
	void (*write_im)(GaimConversation *conv, const char *who,
					 const char *message, size_t len, int flags, time_t mtime);
	void (*write_conv)(GaimConversation *conv, const char *who,
					   const char *message, size_t length, int flags,
					   time_t mtime);

	void (*chat_add_user)(GaimConversation *conv, const char *user);
	void (*chat_rename_user)(GaimConversation *conv,
							 const char *old_name, const char *new_name);
	void (*chat_remove_user)(GaimConversation *conv, const char *user);

	void (*set_title)(GaimConversation *conv, const char *title);
	void (*update_progress)(GaimConversation *conv, float percent);

	/* Events */
	void (*updated)(GaimConversation *conv, GaimConvUpdateType type);
};

/**
 * A core representation of a graphical window containing one or more
 * conversations.
 */
struct _GaimWindow
{
	GList *conversations;              /**< The conversations in the window. */
	size_t conversation_count;         /**< The number of conversations.     */

	GaimWindowUiOps *ui_ops;           /**< UI-specific window operations.   */
	void *ui_data;                     /**< UI-specific data.                */
};

/**
 * Data specific to Instant Messages.
 */
struct _GaimIm
{
	GaimConversation *conv;            /**< The parent conversation.     */

	int    typing_state;               /**< The current typing state.    */
	guint  typing_timeout;             /**< The typing timer handle.     */
	time_t type_again;                 /**< The type again time.         */
	guint  type_again_timeout;         /**< The type again timer handle. */

	GSList *images;                    /**< A list of images in the IM.  */
};

/**
 * Data specific to Chats.
 */
struct _GaimChat
{
	GaimConversation *conv;          /**< The parent conversation.      */

	GList *in_room;                  /**< The users in the room.        */
	GList *ignored;                  /**< Ignored users.                */
	char  *who;                      /**< The person who set the topic. */
	char  *topic;                    /**< The topic.                    */
	int    id;                       /**< The chat ID.                  */
};

/**
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat. Each conversation is kept
 * in a GaimWindow and has a UI representation.
 */
struct _GaimConversation
{
	GaimConversationType type;  /**< The type of conversation.          */

	GaimAccount *account;       /**< The user using this conversation.  */
	GaimWindow *window;         /**< The parent window.                 */

	int conversation_pos;       /**< The position in the window's list. */

	char *name;                 /**< The name of the conversation.      */
	char *title;                /**< The window title.                  */

	gboolean logging;           /**< The status of logging.             */

	GList *send_history;        /**< The send history.                  */
	GString *history;           /**< The conversation history.          */

	GaimUnseenState unseen;     /**< The unseen tab state.              */

	union
	{
		GaimIm   *im;           /**< IM-specific data.                  */
		GaimChat *chat;         /**< Chat-specific data.                */
		void *misc;             /**< Misc. data.                        */

	} u;

	GaimConversationUiOps *ui_ops;           /**< UI-specific operations. */
	void *ui_data;                           /**< UI-specific data.       */

	GHashTable *data;                        /**< Plugin-specific data.   */
};

typedef void (*GaimConvPlacementFunc)(GaimConversation *);

/**************************************************************************/
/** @name Conversation Window API                                         */
/**************************************************************************/
/*@{*/

/**
 * Creates a new conversation window.
 *
 * This window is added to the list of windows, but is not shown until
 * gaim_window_show() is called.
 *
 * @return The new conversation window.
 */
GaimWindow *gaim_window_new(void);

/**
 * Destroys the specified conversation window and all conversations in it.
 *
 * @param win The window to destroy.
 */
void gaim_window_destroy(GaimWindow *win);

/**
 * Shows the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_show(GaimWindow *win);

/**
 * Hides the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_hide(GaimWindow *win);

/**
 * Raises the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_raise(GaimWindow *win);

/**
 * Causes the window to flash for IM notification, if the UI supports this.
 * 
 * @param win The window.
 */
void gaim_window_flash(GaimWindow *win);

/**
 * Sets the specified window's UI window operations structure.
 *
 * @param win The window.
 * @param ops The UI window operations structure.
 */
void gaim_window_set_ui_ops(GaimWindow *win, GaimWindowUiOps *ops);

/**
 * Returns the specified window's UI window operations structure.
 *
 * @param win The window.
 *
 * @return The UI window operations structure.
 */
GaimWindowUiOps *gaim_window_get_ui_ops(const GaimWindow *win);

/**
 * Adds a conversation to this window.
 *
 * If the conversation already has a parent window, this will do nothing.
 *
 * @param win  The window.
 * @param conv The conversation.
 *
 * @return The new index of the conversation in the window.
 */
int gaim_window_add_conversation(GaimWindow *win, GaimConversation *conv);

/**
 * Removes the conversation at the specified index from the window.
 *
 * If there is no conversation at this index, this will do nothing.
 *
 * @param win   The window.
 * @param index The index of the conversation.
 *
 * @return The conversation removed.
 */
GaimConversation *gaim_window_remove_conversation(GaimWindow *win,
												  unsigned int index);

/**
 * Moves the conversation at the specified index in a window to a new index.
 *
 * @param win      The window.
 * @param index     The index of the conversation to move.
 * @param new_index The new index.
 */
void gaim_window_move_conversation(GaimWindow *win, unsigned int index,
								   unsigned int new_index);

/**
 * Returns the conversation in the window at the specified index.
 *
 * If the index is out of range, this returns @c NULL.
 *
 * @param win   The window.
 * @param index The index containing a conversation.
 *
 * @return The conversation at the specified index.
 */
GaimConversation *gaim_window_get_conversation_at(const GaimWindow *win,
												  unsigned int index);

/**
 * Returns the number of conversations in the window.
 *
 * @param win The window.
 *
 * @return The number of conversations.
 */
size_t gaim_window_get_conversation_count(const GaimWindow *win);

/**
 * Switches the active conversation to the one at the specified index.
 *
 * If @a index is out of range, this does nothing.
 *
 * @param win   The window.
 * @param index The new index.
 */
void gaim_window_switch_conversation(GaimWindow *win, unsigned int index);

/**
 * Returns the active conversation in the window.
 *
 * @param win The window.
 *
 * @return The active conversation.
 */
GaimConversation *gaim_window_get_active_conversation(const GaimWindow *win);

/**
 * Returns the list of conversations in the specified window.
 *
 * @param win The window.
 *
 * @return The list of conversations.
 */
GList *gaim_window_get_conversations(const GaimWindow *win);

/**
 * Returns a list of all windows.
 *
 * @return A list of windows.
 */
GList *gaim_get_windows(void);

/**
 * Returns the first window containing a conversation of the specified type.
 *
 * @param type The conversation type.
 *
 * @return The window if found, or @c NULL if not found.
 */
GaimWindow *gaim_get_first_window_with_type(GaimConversationType type);

/**
 * Returns the last window containing a conversation of the specified type.
 *
 * @param type The conversation type.
 *
 * @return The window if found, or @c NULL if not found.
 */
GaimWindow *gaim_get_last_window_with_type(GaimConversationType type);

/*@}*/

/**************************************************************************/
/** @name Conversation API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new conversation of the specified type.
 *
 * @param type The type of conversation.
 * @param user The account opening the conversation window on the gaim
 *             user's end.
 * @param name The name of the conversation.
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
 * @param win The conversation.
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
 * Returns the specified conversation's index in the parent window.
 *
 * @param conv The conversation.
 *
 * @return The current index in the parent window.
 */
int gaim_conversation_get_index(const GaimConversation *conv);

/**
 * Sets the conversation's unseen state.
 *
 * @param conv  The conversation.
 * @param state The new unseen state.
 */
void gaim_conversation_set_unseen(GaimConversation *conv,
								  GaimUnseenState state);

/**
 * Returns the conversation's unseen state.
 *
 * @param conv The conversation.
 *
 * @param The conversation's unseen state.
 */
GaimUnseenState gaim_conversation_get_unseen(const GaimConversation *conv);

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
 * @param log @c TRUE if logging should be enabled, or @c FALSE otherwise.
 */
void gaim_conversation_set_logging(GaimConversation *conv, gboolean log);

/**
 * Returns whether or not logging is enabled for this conversation.
 *
 * @return @c TRUE if logging is enabled, or @c FALSE otherwise.
 */
gboolean gaim_conversation_is_logging(const GaimConversation *conv);

/**
 * Returns the specified conversation's send history.
 *
 * @param conv The conversation.
 *
 * @return The conversation's send history.
 */
GList *gaim_conversation_get_send_history(const GaimConversation *conv);

/**
 * Sets the specified conversation's history.
 *
 * @param conv    The conversation.
 * @param history The history.
 */
void gaim_conversation_set_history(GaimConversation *conv, GString *history);

/**
 * Returns the specified conversation's history.
 *
 * @param conv The conversation.
 *
 * @return The conversation's history.
 */
GString *gaim_conversation_get_history(const GaimConversation *conv);

/**
 * Returns the specified conversation's parent window.
 *
 * @param conv The conversation.
 *
 * @return The conversation's parent window.
 */
GaimWindow *gaim_conversation_get_window(const GaimConversation *conv);

/**
 * Returns the specified conversation's IM-specific data.
 *
 * If the conversation type is not GAIM_CONV_IM, this will return @c NULL.
 *
 * @param conv The conversation.
 *
 * @return The IM-specific data.
 */
GaimIm *gaim_conversation_get_im_data(const GaimConversation *conv);

#define GAIM_IM(c) (gaim_conversation_get_im_data(c))

/**
 * Returns the specified conversation's chat-specific data.
 *
 * If the conversation type is not GAIM_CONV_CHAT, this will return @c NULL.
 *
 * @param conv The conversation.
 *
 * @return The chat-specific data.
 */
GaimChat *gaim_conversation_get_chat_data(const GaimConversation *conv);

#define GAIM_CHAT(c) (gaim_conversation_get_chat_data(c))

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
 * Finds the conversation with the specified name.
 *
 * @param name The name of the conversation.
 *
 * @return The conversation if found, or @c NULL otherwise.
 */
GaimConversation *gaim_find_conversation(const char *name);

/**
 * Finds a conversation with the specified name and user.
 *
 * @param name The name of the conversation.
 * @param account The gaim_account associated with the conversation.
 *
 * @return The conversation if found, or @c NULL otherwise.
 */
GaimConversation *gaim_find_conversation_with_account(
		const char *name, const GaimAccount *account);

/**
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * gaim_im_write() and gaim_chat_write() instead. Those functions will
 * most likely call this anyway, but they may do their own formatting,
 * sound playback, etc.
 *
 * This can be used to write generic messages, such as "so and so closed
 * the conversation window."
 *
 * @param conv    The conversation.
 * @param who     The user who sent the message.
 * @param message The message.
 * @param length  The length of the message.
 * @param flags   The flags.
 * @param mtime   The time the message was sent.
 *
 * @see gaim_im_write()
 * @see gaim_chat_write()
 */
void gaim_conversation_write(GaimConversation *conv, const char *who,
							 const char *message, size_t length, int flags,
							 time_t mtime);

/**
 * Updates the progress bar on a conversation window
 * (if one exists in the UI).
 *
 * This is used for loading images typically.
 *
 * @param conv    The conversation.
 * @param percent The percentage.
 */
void gaim_conversation_update_progress(GaimConversation *conv, float percent);

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
GaimConversation *gaim_im_get_conversation(const GaimIm *im);

/**
 * Sets the IM's typing state.
 *
 * @param im    The IM.
 * @param state The typing state.
 */
void gaim_im_set_typing_state(GaimIm *im, int state);

/**
 * Returns the IM's typing state.
 *
 * @param im The IM.
 *
 * @return The IM's typing state.
 */
int gaim_im_get_typing_state(const GaimIm *im);

/**
 * Starts the IM's typing timeout.
 *
 * @param im      The IM.
 * @param timeout The timeout.
 */
void gaim_im_start_typing_timeout(GaimIm *im, int timeout);

/**
 * Stops the IM's typing timeout.
 *
 * @param im The IM.
 */
void gaim_im_stop_typing_timeout(GaimIm *im);

/**
 * Returns the IM's typing timeout.
 *
 * @param im The IM.
 *
 * @return The timeout.
 */
guint gaim_im_get_typing_timeout(const GaimIm *im);

/**
 * Sets the IM's time until it should send another typing notification.
 *
 * @param im  The IM.
 * @param val The time.
 */
void gaim_im_set_type_again(GaimIm *im, time_t val);

/**
 * Returns the IM's time until it should send another typing notification.
 *
 * @param im The IM.
 *
 * @return The time.
 */
time_t gaim_im_get_type_again(const GaimIm *im);

/**
 * Starts the IM's type again timeout.
 *
 * @param im      The IM.
 */
void gaim_im_start_type_again_timeout(GaimIm *im);

/**
 * Stops the IM's type again timeout.
 *
 * @param im The IM.
 */
void gaim_im_stop_type_again_timeout(GaimIm *im);

/**
 * Returns the IM's type again timeout interval.
 *
 * @param im The IM.
 *
 * @return The type again timeout interval.
 */
guint gaim_im_get_type_again_timeout(const GaimIm *im);

/**
 * Updates the visual typing notification for an IM conversation.
 *
 * @param im The IM.
 */
void gaim_im_update_typing(GaimIm *im);

/**
 * Writes to an IM.
 *
 * The @a len parameter is used for writing binary data, such as an
 * image. If @c message is text, specify -1 for @a len.
 *
 * @param im      The IM.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param len     The length of the message, or -1 to specify the length
 *                of @a message.
 * @param flag    The flags.
 * @param mtime   The time the message was sent.
 */
void gaim_im_write(GaimIm *im, const char *who,
				   const char *message, size_t len, int flag, time_t mtime);

/**
 * Sends a message to this IM conversation.
 *
 * @param im      The IM.
 * @param message The message to send.
 */
void gaim_im_send(GaimIm *im, const char *message);

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
GaimConversation *gaim_chat_get_conversation(const GaimChat *chat);

/**
 * Sets the list of users in the chat room.
 *
 * @param chat  The chat.
 * @param users The list of users.
 *
 * @return The list passed.
 */
GList *gaim_chat_set_users(GaimChat *chat, GList *users);

/**
 * Returns a list of users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of users.
 */
GList *gaim_chat_get_users(const GaimChat *chat);

/**
 * Ignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_chat_ignore(GaimChat *chat, const char *name);

/**
 * Unignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_chat_unignore(GaimChat *chat, const char *name);

/**
 * Sets the list of ignored users in the chat room.
 *
 * @param chat    The chat.
 * @param ignored The list of ignored users.
 *
 * @return The list passed.
 */
GList *gaim_chat_set_ignored(GaimChat *chat, GList *ignored);

/**
 * Returns the list of ignored users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of ignored users.
 */
GList *gaim_chat_get_ignored(const GaimChat *chat);

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
const char *gaim_chat_get_ignored_user(const GaimChat *chat,
									   const char *user);

/**
 * Returns @c TRUE if the specified user is ignored.
 *
 * @param chat The chat.
 * @param user The user.
 *
 * @return @c TRUE if the user is in the ignore list; @c FALSE otherwise.
 */
gboolean gaim_chat_is_user_ignored(const GaimChat *chat,
								   const char *user);

/**
 * Sets the chat room's topic.
 *
 * @param chat  The chat.
 * @param who   The user that set the topic.
 * @param topic The topic.
 */
void gaim_chat_set_topic(GaimChat *chat, const char *who,
						 const char *topic);

/**
 * Returns the chat room's topic.
 *
 * @param chat The chat.
 *
 * @return The chat's topic.
 */
const char *gaim_chat_get_topic(const GaimChat *chat);

/**
 * Sets the chat room's ID.
 *
 * @param chat The chat.
 * @param id   The ID.
 */
void gaim_chat_set_id(GaimChat *chat, int id);

/**
 * Returns the chat room's ID.
 *
 * @param chat The chat.
 *
 * @return The ID.
 */
int gaim_chat_get_id(const GaimChat *chat);

/**
 * Writes to a chat.
 *
 * @param chat    The chat.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flag    The flags.
 * @param mtime   The time the message was sent.
 */
void gaim_chat_write(GaimChat *chat, const char *who,
					 const char *message, int flag, time_t mtime);

/**
 * Sends a message to this chat conversation.
 *
 * @param chat    The chat.
 * @param message The message to send.
 */
void gaim_chat_send(GaimChat *chat, const char *message);

/**
 * Adds a user to a chat.
 *
 * @param chat      The chat.
 * @param user      The user to add.
 * @param extra_msg An extra message to display with the join message.
 */
void gaim_chat_add_user(GaimChat *chat, const char *user,
						const char *extra_msg);

/**
 * Renames a user in a chat.
 *
 * @param chat     The chat.
 * @param old_user The old username.
 * @param new_user The new username.
 */
void gaim_chat_rename_user(GaimChat *chat, const char *old_user,
						   const char *new_user);

/**
 * Removes a user from a chat, optionally with a reason.
 *
 * @param chat   The chat.
 * @param user   The user that is being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void gaim_chat_remove_user(GaimChat *chat, const char *user,
						   const char *reason);

/**
 * Finds a chat with the specified chat ID.
 *
 * @param gc The gaim_connection.
 * @param id The chat ID.
 *
 * @return The chat conversation.
 */
GaimConversation *gaim_find_chat(const GaimConnection *gc, int id);

/*@}*/

/**************************************************************************/
/** @name Conversation Placement Functions                                */
/**************************************************************************/
/*@{*/

/**
 * Adds a conversation placement function to the list of possible functions.
 *
 * @param name The name of the function.
 * @param fnc  A pointer to the function.
 *
 * @return The index of this entry.
 */
int gaim_conv_placement_add_fnc(const char *name, GaimConvPlacementFunc fnc);

/**
 * Removes a conversation placement function from the list of possible
 * functions.
 *
 * @param index The index of the function.
 */
void gaim_conv_placement_remove_fnc(int index);

/**
 * Returns the number of conversation placement functions.
 *
 * @return The number of registered functions.
 */
int gaim_conv_placement_get_fnc_count(void);

/**
 * Returns the name of the conversation placement function at the
 * specified index.
 *
 * @param index The index.
 *
 * @return The name of the function, or @c NULL if this index is out of
 *         range.
 */
const char *gaim_conv_placement_get_name(int index);

/**
 * Returns a pointer to the conversation placement function at the
 * specified index.
 *
 * @param index The index.
 *
 * @return A pointer to the function.
 */
GaimConvPlacementFunc gaim_conv_placement_get_fnc(int index);

/**
 * Returns the index of the specified conversation placement function.
 *
 * @param fnc A pointer to the registered function.
 *
 * @return The index of the conversation, or -1 if the function is not
 *         registered.
 */
int gaim_conv_placement_get_fnc_index(GaimConvPlacementFunc fnc);

/**
 * Returns the index of the active conversation placement function.
 *
 * @param index The index of the active function.
 */
int gaim_conv_placement_get_active(void);

/**
 * Sets the active conversation placement function.
 *
 * @param index The index of the function.
 */
void gaim_conv_placement_set_active(int index);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used in all gaim conversation
 * windows.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_win_ui_ops(GaimWindowUiOps *ops);

/**
 * Returns the gaim window UI operations structure to be used in
 * new windows.
 *
 * @return A filled-out GaimWindowUiOps structure.
 */
GaimWindowUiOps *gaim_get_win_ui_ops(void);

/*@}*/

#endif /* _GAIM_CONVERSATION_H_ */
