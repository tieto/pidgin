/**
 * @file conversation.h Conversation API
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
 *
 */

#ifndef _CONVERSATION_H_
#define _CONVERSATION_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

typedef enum _GaimConversationType GaimConversationType;
typedef enum _GaimUnseenState      GaimUnseenState;
typedef enum _GaimConvUpdateType   GaimConvUpdateType;
struct gaim_window_ops;
struct gaim_window;
struct gaim_conversation;
struct gaim_im;
struct gaim_chat;

/**
 * A type of conversation.
 */
enum _GaimConversationType
{
	GAIM_CONV_UNKNOWN = 0, /**< Unknown conversation type. */
	GAIM_CONV_IM,          /**< Instant Message.           */
	GAIM_CONV_CHAT         /**< Chat room.                 */
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
	GAIM_CONV_UPDATE_USER,    /**< The aim_user was changed. */
	GAIM_CONV_UPDATE_TYPING,  /**< The typing state was updated. */
	GAIM_CONV_UPDATE_UNSEEN,  /**< The unseen state was updated. */
	GAIM_CONV_UPDATE_LOGGING, /**< Logging for this conversation was
								   enabled or disabled. */
	GAIM_CONV_UPDATE_TOPIC,   /**< The topic for a chat was updated. */

	/*
	 * XXX These need to go when we implement a more generic core/UI event
	 * system.
	 */
	GAIM_CONV_ACCOUNT_ONLINE, /**< One of the user's accounts went online.  */
	GAIM_CONV_ACCOUNT_OFFLINE /**< One of the user's accounts went offline. */
};

/* Yeah, this has to be included here. Ugh. */
#include "gaim.h"

/**
 * Conversation window operations.
 *
 * Any UI representing a window must assign a filled-out gaim_window_ops
 * structure to the gaim_window.
 */
struct gaim_window_ops
{
	struct gaim_conversation_ops *(*get_conversation_ops)(void);

	void (*new_window)(struct gaim_window *win);
	void (*destroy_window)(struct gaim_window *win);

	void (*show)(struct gaim_window *win);
	void (*hide)(struct gaim_window *win);
	void (*raise)(struct gaim_window *win);
	void (*flash)(struct gaim_window *win);

	void (*switch_conversation)(struct gaim_window *win, unsigned int index);
	void (*add_conversation)(struct gaim_window *win,
							 struct gaim_conversation *conv);
	void (*remove_conversation)(struct gaim_window *win,
								struct gaim_conversation *conv);
	void (*move_conversation)(struct gaim_window *win,
							  struct gaim_conversation *conv,
							  unsigned int newIndex);
	int (*get_active_index)(const struct gaim_window *win);
};

/**
 * Conversation operations and events.
 *
 * Any UI representing a conversation must assign a filled-out
 * gaim_conversation_ops structure to the gaim_conversation.
 */
struct gaim_conversation_ops
{
	void (*destroy_conversation)(struct gaim_conversation *conv);
	void (*write_chat)(struct gaim_conversation *conv, const char *who,
					   const char *message, int flags, time_t mtime);
	void (*write_im)(struct gaim_conversation *conv, const char *who,
					 const char *message, size_t len, int flags, time_t mtime);
	void (*write_conv)(struct gaim_conversation *conv, const char *who,
					   const char *message, size_t length, int flags,
					   time_t mtime);

	void (*chat_add_user)(struct gaim_conversation *conv, const char *user);
	void (*chat_rename_user)(struct gaim_conversation *conv,
							 const char *old_name, const char *new_name);
	void (*chat_remove_user)(struct gaim_conversation *conv, const char *user);

	void (*set_title)(struct gaim_conversation *conv,
					  const char *title);
	void (*update_progress)(struct gaim_conversation *conv, float percent);

	/* Events */
	void (*updated)(struct gaim_conversation *conv, GaimConvUpdateType type);
};

/**
 * A core representation of a graphical window containing one or more
 * conversations.
 */
struct gaim_window
{
	GList *conversations;        /**< The conversations in the window. */
	size_t conversation_count;   /**< The number of conversations.     */

	struct gaim_window_ops *ops; /**< UI-specific window operations.   */

	void *ui_data;               /**< UI-specific data.                */
};

/**
 * Data specific to Instant Messages.
 */
struct gaim_im
{
	struct gaim_conversation *conv;

	int    typing_state;
	guint  typing_timeout;
	time_t type_again;
	guint  type_again_timeout;

	GSList *images;
};

/**
 * Data specific to Chats.
 */
struct gaim_chat
{
	struct gaim_conversation *conv;

	GList *in_room;
	GList *ignored;
	char  *who;
	char  *topic;
	int    id;
};

/**
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat. Each conversation is kept
 * in a gaim_window and has a UI representation.
 */
struct gaim_conversation
{
	GaimConversationType type;  /**< The type of conversation.          */

	struct aim_user *user;      /**< The user using this conversation.  */
	struct gaim_window *window; /**< The parent window.                 */

	/** UI-specific conversation operations.*/
	struct gaim_conversation_ops *ops; 

	int conversation_pos;       /**< The position in the window's list. */

	char *name;                 /**< The name of the conversation.      */
	char *title;                /**< The window title.                  */

	gboolean logging;           /**< The status of logging.             */

	GList *send_history;        /**< The send history.                  */
	GString *history;           /**< The conversation history.          */

	GaimUnseenState unseen;     /**< The unseen tab state.              */

	void *ui_data;              /**< UI-specific data.                  */

	union
	{
		struct gaim_im   *im;   /**< IM-specific data.                  */
		struct gaim_chat *chat; /**< Chat-specific data.                */

	} u;
};


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
struct gaim_window *gaim_window_new(void);

/**
 * Destroys the specified conversation window and all conversations in it.
 *
 * @param win The window to destroy.
 */
void gaim_window_destroy(struct gaim_window *win);

/**
 * Shows the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_show(struct gaim_window *win);

/**
 * Hides the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_hide(struct gaim_window *win);

/**
 * Raises the specified conversation window.
 *
 * @param win The window.
 */
void gaim_window_raise(struct gaim_window *win);

/**
 * Causes the window to flash for IM notification, if the UI supports this.
 * 
 * @param win The window.
 */
void gaim_window_flash(struct gaim_window *win);

/**
 * Sets the specified window's UI window operations structure.
 *
 * @param win The window.
 * @param ops The UI window operations structure.
 */
void gaim_window_set_ops(struct gaim_window *win,
						 struct gaim_window_ops *ops);

/**
 * Returns the specified window's UI window operations structure.
 *
 * @param win The window.
 *
 * @return The UI window operations structure.
 */
struct gaim_window_ops *gaim_window_get_ops(const struct gaim_window *win);

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
int gaim_window_add_conversation(struct gaim_window *win,
								 struct gaim_conversation *conv);

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
struct gaim_conversation *gaim_window_remove_conversation(
		struct gaim_window *win, unsigned int index);

/**
 * Moves the conversation at the specified index in a window to a new index.
 *
 * @param win      The window.
 * @param index     The index of the conversation to move.
 * @param new_index The new index.
 */
void gaim_window_move_conversation(struct gaim_window *win,
								   unsigned int index, unsigned int new_index);

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
struct gaim_conversation *gaim_window_get_conversation_at(
		const struct gaim_window *win, unsigned int index);

/**
 * Returns the number of conversations in the window.
 *
 * @param win The window.
 *
 * @return The number of conversations.
 */
size_t gaim_window_get_conversation_count(const struct gaim_window *win);

/**
 * Switches the active conversation to the one at the specified index.
 *
 * If @a index is out of range, this does nothing.
 *
 * @param win   The window.
 * @param index The new index.
 */
void gaim_window_switch_conversation(struct gaim_window *win,
									 unsigned int index);

/**
 * Returns the active conversation in the window.
 *
 * @param win The window.
 *
 * @return The active conversation.
 */
struct gaim_conversation *gaim_window_get_active_conversation(
		const struct gaim_window *win);

/**
 * Returns the list of conversations in the specified window.
 *
 * @param win The window.
 *
 * @return The list of conversations.
 */
GList *gaim_window_get_conversations(const struct gaim_window *win);

/**
 * Returns a list of all windows.
 *
 * @return A list of windows.
 */
GList *gaim_get_windows(void);

/*@}*/

/**************************************************************************/
/** @name Conversation API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new conversation of the specified type.
 *
 * @param type The type of conversation.
 * @param name The name of the conversation.
 *
 * @return The new conversation.
 */
struct gaim_conversation *gaim_conversation_new(GaimConversationType type,
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
void gaim_conversation_destroy(struct gaim_conversation *conv);

/**
 * Returns the specified conversation's type.
 *
 * @param conv The conversation.
 *
 * @return The conversation's type.
 */
GaimConversationType gaim_conversation_get_type(
		const struct gaim_conversation *conv);

/**
 * Sets the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 * @param ops  The UI conversation operations structure.
 */
void gaim_conversation_set_ops(struct gaim_conversation *conv,
							   struct gaim_conversation_ops *ops);

/**
 * Returns the specified conversation's UI operations structure.
 * 
 * @param conv The conversation.
 *
 * @return The operations structure.
 */
struct gaim_conversation_ops *gaim_conversation_get_ops(
		struct gaim_conversation *conv);

/**
 * Sets the specified conversation's aim_user.
 *
 * This aim_user represents the user using gaim, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 * @param user The aim_user.
 */
void gaim_conversation_set_user(struct gaim_conversation *conv,
								struct aim_user *user);

/**
 * Returns the specified conversation's aim_user.
 *
 * This aim_user represents the user using gaim, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 *
 * @return The conversation's aim_user.
 */
struct aim_user *gaim_conversation_get_user(
		const struct gaim_conversation *conv);

#if 0
/**
 * Sets the specified conversation's gaim_connection.
 *
 * @param conv The conversation.
 * @param gc   The gaim_connection.
 */
void gaim_conversation_set_gc(struct gaim_conversation *conv,
							  struct gaim_connection *gc);
#endif

/**
 * Returns the specified conversation's gaim_connection.
 *
 * This is the same as gaim_conversation_get_user(conv)->gc.
 *
 * @param conv The conversation.
 *
 * @return The conversation's gaim_connection.
 */
struct gaim_connection *gaim_conversation_get_gc(
		const struct gaim_conversation *conv);

/**
 * Sets the specified conversation's title.
 *
 * @param conv  The conversation.
 * @param title The title.
 */
void gaim_conversation_set_title(struct gaim_conversation *conv,
								 const char *title);

/**
 * Returns the specified conversation's title.
 *
 * @param win The conversation.
 *
 * @return The title.
 */
const char *gaim_conversation_get_title(const struct gaim_conversation *conv);

/**
 * Automatically sets the specified conversation's title.
 *
 * This function takes OPT_IM_ALIAS_TAB into account, as well as the
 * user's alias.
 *
 * @param conv The conversation.
 */
void gaim_conversation_autoset_title(struct gaim_conversation *conv);

/**
 * Returns the specified conversation's index in the parent window.
 *
 * @param conv The conversation.
 *
 * @return The current index in the parent window.
 */
int gaim_conversation_get_index(const struct gaim_conversation *conv);

/**
 * Sets the conversation's unseen state.
 *
 * @param conv  The conversation.
 * @param state The new unseen state.
 */
void gaim_conversation_set_unseen(struct gaim_conversation *conv,
								  GaimUnseenState state);

/**
 * Returns the conversation's unseen state.
 *
 * @param conv The conversation.
 *
 * @param The conversation's unseen state.
 */
GaimUnseenState gaim_conversation_get_unseen(
		const struct gaim_conversation *conv);

/**
 * Returns the specified conversation's name.
 *
 * @param conv The conversation.
 *
 * @return The conversation's name.
 */
const char *gaim_conversation_get_name(const struct gaim_conversation *conv);

/**
 * Enables or disables logging for this conversation.
 *
 * @param log @c TRUE if logging should be enabled, or @c FALSE otherwise.
 */
void gaim_conversation_set_logging(struct gaim_conversation *conv,
								   gboolean log);

/**
 * Returns whether or not logging is enabled for this conversation.
 *
 * @return @c TRUE if logging is enabled, or @c FALSE otherwise.
 */
gboolean gaim_conversation_is_logging(const struct gaim_conversation *conv);

/**
 * Returns the specified conversation's send history.
 *
 * @param conv The conversation.
 *
 * @return The conversation's send history.
 */
GList *gaim_conversation_get_send_history(
		const struct gaim_conversation *conv);

/**
 * Sets the specified conversation's history.
 *
 * @param conv    The conversation.
 * @param history The history.
 */
void gaim_conversation_set_history(struct gaim_conversation *conv,
								   GString *history);

/**
 * Returns the specified conversation's history.
 *
 * @param conv The conversation.
 *
 * @return The conversation's history.
 */
GString *gaim_conversation_get_history(const struct gaim_conversation *conv);

/**
 * Returns the specified conversation's parent window.
 *
 * @param conv The conversation.
 *
 * @return The conversation's parent window.
 */
struct gaim_window *gaim_conversation_get_window(
		const struct gaim_conversation *conv);

/**
 * Returns the specified conversation's IM-specific data.
 *
 * If the conversation type is not GAIM_CONV_IM, this will return @c NULL.
 *
 * @param conv The conversation.
 *
 * @return The IM-specific data.
 */
struct gaim_im *gaim_conversation_get_im_data(
		const struct gaim_conversation *conv);

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
struct gaim_chat *gaim_conversation_get_chat_data(
		const struct gaim_conversation *conv);

#define GAIM_CHAT(c) (gaim_conversation_get_chat_data(c))

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
struct gaim_conversation *gaim_find_conversation(const char *name);

/**
 * Finds a conversation with the specified name and user.
 *
 * @param name The name of the conversation.
 * @param user The aim_user associated with the conversation.
 *
 * @return The conversation if found, or @c NULL otherwise.
 */
struct gaim_conversation *gaim_find_conversation_with_user(
		const char *name, const struct aim_user *user);

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
void gaim_conversation_write(struct gaim_conversation *conv, const char *who,
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
void gaim_conversation_update_progress(struct gaim_conversation *conv,
									   float percent);

/**
 * Updates the visual status and UI of a conversation.
 *
 * @param conv The conversation.
 * @param type The update type.
 */
void gaim_conversation_update(struct gaim_conversation *conv,
							  GaimConvUpdateType type);

/**
 * Calls a function on each conversation.
 *
 * @param func The function.
 */
void gaim_conversation_foreach(void (*func)(struct gaim_conversation *conv));

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
struct gaim_conversation *gaim_im_get_conversation(struct gaim_im *im);

/**
 * Sets the IM's typing state.
 *
 * @param im    The IM.
 * @param state The typing state.
 */
void gaim_im_set_typing_state(struct gaim_im *im, int state);

/**
 * Returns the IM's typing state.
 *
 * @param im The IM.
 *
 * @return The IM's typing state.
 */
int gaim_im_get_typing_state(const struct gaim_im *im);

/**
 * Starts the IM's typing timeout.
 *
 * @param im      The IM.
 * @param timeout The timeout.
 */
void gaim_im_start_typing_timeout(struct gaim_im *im, int timeout);

/**
 * Stops the IM's typing timeout.
 *
 * @param im The IM.
 */
void gaim_im_stop_typing_timeout(struct gaim_im *im);

/**
 * Returns the IM's typing timeout.
 *
 * @param im The IM.
 *
 * @return The timeout.
 */
guint gaim_im_get_typing_timeout(const struct gaim_im *im);

/**
 * Sets the IM's time until it should send another typing notification.
 *
 * @param im  The IM.
 * @param val The time.
 */
void gaim_im_set_type_again(struct gaim_im *im, time_t val);

/**
 * Returns the IM's time until it should send another typing notification.
 *
 * @param im The IM.
 *
 * @return The time.
 */
time_t gaim_im_get_type_again(const struct gaim_im *im);

/**
 * Starts the IM's type again timeout.
 *
 * @param im      The IM.
 */
void gaim_im_start_type_again_timeout(struct gaim_im *im);

/**
 * Stops the IM's type again timeout.
 *
 * @param im The IM.
 */
void gaim_im_stop_type_again_timeout(struct gaim_im *im);

/**
 * Returns the IM's type again timeout interval.
 *
 * @param im The IM.
 *
 * @return The type again timeout interval.
 */
guint gaim_im_get_type_again_timeout(const struct gaim_im *im);

/**
 * Updates the visual typing notification for an IM conversation.
 *
 * @param im The IM.
 */
void gaim_im_update_typing(struct gaim_im *im);

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
void gaim_im_write(struct gaim_im *im, const char *who,
				   const char *message, size_t len, int flag, time_t mtime);

/**
 * Sends a message to this IM conversation.
 *
 * @param im      The IM.
 * @param message The message to send.
 */
void gaim_im_send(struct gaim_im *im, const char *message);

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
struct gaim_conversation *gaim_chat_get_conversation(struct gaim_chat *chat);

/**
 * Sets the list of users in the chat room.
 *
 * @param chat  The chat.
 * @param users The list of users.
 *
 * @return The list passed.
 */
GList *gaim_chat_set_users(struct gaim_chat *chat, GList *users);

/**
 * Returns a list of users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of users.
 */
GList *gaim_chat_get_users(const struct gaim_chat *chat);

/**
 * Ignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_chat_ignore(struct gaim_chat *chat, const char *name);

/**
 * Unignores a user in a chat room.
 *
 * @param chat The chat.
 * @param name The name of the user.
 */
void gaim_chat_unignore(struct gaim_chat *chat, const char *name);

/**
 * Sets the list of ignored users in the chat room.
 *
 * @param chat    The chat.
 * @param ignored The list of ignored users.
 *
 * @return The list passed.
 */
GList *gaim_chat_set_ignored(struct gaim_chat *chat, GList *ignored);

/**
 * Returns the list of ignored users in the chat room.
 *
 * @param chat The chat.
 *
 * @return The list of ignored users.
 */
GList *gaim_chat_get_ignored(const struct gaim_chat *chat);

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
const char *gaim_chat_get_ignored_user(const struct gaim_chat *chat,
									   const char *user);

/**
 * Returns @c TRUE if the specified user is ignored.
 *
 * @param chat The chat.
 * @param user The user.
 *
 * @return @c TRUE if the user is in the ignore list; @c FALSE otherwise.
 */
gboolean gaim_chat_is_user_ignored(const struct gaim_chat *chat,
								   const char *user);

/**
 * Sets the chat room's topic.
 *
 * @param chat  The chat.
 * @param who   The user that set the topic.
 * @param topic The topic.
 */
void gaim_chat_set_topic(struct gaim_chat *chat, const char *who,
						 const char *topic);

/**
 * Returns the chat room's topic.
 *
 * @param chat The chat.
 *
 * @return The chat's topic.
 */
const char *gaim_chat_get_topic(const struct gaim_chat *chat);

/**
 * Sets the chat room's ID.
 *
 * @param chat The chat.
 * @param id   The ID.
 */
void gaim_chat_set_id(struct gaim_chat *chat, int id);

/**
 * Returns the chat room's ID.
 *
 * @param chat The chat.
 *
 * @return The ID.
 */
int gaim_chat_get_id(const struct gaim_chat *chat);

/**
 * Writes to a chat.
 *
 * @param chat    The chat.
 * @param who     The user who sent the message.
 * @param message The message to write.
 * @param flag    The flags.
 * @param mtime   The time the message was sent.
 */
void gaim_chat_write(struct gaim_chat *chat, const char *who,
					 const char *message, int flag, time_t mtime);

/**
 * Sends a message to this chat conversation.
 *
 * @param chat    The chat.
 * @param message The message to send.
 */
void gaim_chat_send(struct gaim_chat *chat, const char *message);

/**
 * Adds a user to a chat.
 *
 * @param chat      The chat.
 * @param user      The user to add.
 * @param extra_msg An extra message to display with the join message.
 */
void gaim_chat_add_user(struct gaim_chat *chat, const char *user,
						const char *extra_msg);

/**
 * Renames a user in a chat.
 *
 * @param chat     The chat.
 * @param old_user The old username.
 * @param new_user The new username.
 */
void gaim_chat_rename_user(struct gaim_chat *chat, const char *old_user,
						   const char *new_user);

/**
 * Removes a user from a chat, optionally with a reason.
 *
 * @param chat   The chat.
 * @param user   The user that is being removed.
 * @param reason The optional reason given for the removal. Can be @c NULL.
 */
void gaim_chat_remove_user(struct gaim_chat *chat, const char *user,
						   const char *reason);

/**
 * Finds a chat with the specified chat ID.
 *
 * @param gc The gaim_connection.
 * @param id The chat ID.
 *
 * @return The chat conversation.
 */
struct gaim_conversation *gaim_find_chat(struct gaim_connection *gc, int id);

/*@}*/

#endif /* _CONVERSATION_H_ */
