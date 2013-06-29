/**
 * @file conversationtypes.h Chat and IM Conversation API
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
#ifndef _PURPLE_CONVERSATION_TYPES_H_
#define _PURPLE_CONVERSATION_TYPES_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

#define PURPLE_TYPE_IM_CONVERSATION              (purple_im_conversation_get_type())
#define PURPLE_IM_CONVERSATION(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_IM_CONVERSATION, PurpleIMConversation))
#define PURPLE_IM_CONVERSATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_IM_CONVERSATION, PurpleIMConversationClass))
#define PURPLE_IS_IM_CONVERSATION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_IM_CONVERSATION))
#define PURPLE_IS_IM_CONVERSATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_IM_CONVERSATION))
#define PURPLE_IM_CONVERSATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_IM_CONVERSATION, PurpleIMConversationClass))

/** @copydoc _PurpleIMConversation */
typedef struct _PurpleIMConversation         PurpleIMConversation;
/** @copydoc _PurpleIMConversationClass */
typedef struct _PurpleIMConversationClass    PurpleIMConversationClass;

#define PURPLE_TYPE_CHAT_CONVERSATION            (purple_chat_conversation_get_type())
#define PURPLE_CHAT_CONVERSATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversation))
#define PURPLE_CHAT_CONVERSATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationClass))
#define PURPLE_IS_CHAT_CONVERSATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT_CONVERSATION))
#define PURPLE_IS_CHAT_CONVERSATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT_CONVERSATION))
#define PURPLE_CHAT_CONVERSATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationClass))

/** @copydoc _PurpleChatConversation */
typedef struct _PurpleChatConversation       PurpleChatConversation;
/** @copydoc _PurpleChatConversationClass */
typedef struct _PurpleChatConversationClass  PurpleChatConversationClass;

#define PURPLE_TYPE_CHAT_USER                    (purple_chat_user_get_type())
#define PURPLE_CHAT_USER(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT_USER, PurpleChatUser))
#define PURPLE_CHAT_USER_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT_USER, PurpleChatUserClass))
#define PURPLE_IS_CHAT_USER(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT_USER))
#define PURPLE_IS_CHAT_USER_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT_USER))
#define PURPLE_CHAT_USER_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT_USER, PurpleChatUserClass))

/** @copydoc _PurpleChatUser */
typedef struct _PurpleChatUser       PurpleChatUser;
/** @copydoc _PurpleChatUserClass */
typedef struct _PurpleChatUserClass  PurpleChatUserClass;

/**
 * The typing state of a user.
 */
typedef enum
{
	PURPLE_IM_NOT_TYPING = 0,  /**< Not typing.                 */
	PURPLE_IM_TYPING,          /**< Currently typing.           */
	PURPLE_IM_TYPED            /**< Stopped typing momentarily. */

} PurpleIMTypingState;

/**
 * Flags applicable to users in Chats.
 */
typedef enum /*< flags >*/
{
	PURPLE_CHAT_USER_NONE     = 0x0000, /**< No flags                     */
	PURPLE_CHAT_USER_VOICE    = 0x0001, /**< Voiced user or "Participant" */
	PURPLE_CHAT_USER_HALFOP   = 0x0002, /**< Half-op                      */
	PURPLE_CHAT_USER_OP       = 0x0004, /**< Channel Op or Moderator      */
	PURPLE_CHAT_USER_FOUNDER  = 0x0008, /**< Channel Founder              */
	PURPLE_CHAT_USER_TYPING   = 0x0010, /**< Currently typing             */
	PURPLE_CHAT_USER_AWAY     = 0x0020  /**< Currently away.              */

} PurpleChatUserFlags;

#include "conversation.h"

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
/** PurpleChatUser                                           */
/**************************************************************************/
/** Structure representing a chat user instance. */
struct _PurpleChatUser
{
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleChatUser's */
struct _PurpleChatUserClass {
	/*< private >*/
	GObjectClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name IM Conversation API                                             */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the IMConversation object.
 */
GType purple_im_conversation_get_type(void);

/**
 * Creates a new IM conversation.
 *
 * @param account The account opening the conversation window on the purple
 *                user's end.
 * @param name    Name of the buddy.
 *
 * @return The new conversation.
 */
PurpleIMConversation *purple_im_conversation_new(PurpleAccount *account,
		const char *name);

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
void purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMTypingState state);

/**
 * Returns the IM's typing state.
 *
 * @param im The IM.
 *
 * @return The IM's typing state.
 */
PurpleIMTypingState purple_im_conversation_get_typing_state(const PurpleIMConversation *im);

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
 * Sets the quiet-time when no PURPLE_IM_TYPING messages will be sent.
 * Few protocols need this (maybe only MSN).  If the user is still
 * typing after this quiet-period, then another PURPLE_IM_TYPING message
 * will be sent.
 *
 * @param im  The IM.
 * @param val The number of seconds to wait before allowing another
 *            PURPLE_IM_TYPING message to be sent to the user.  Or 0 to
 *            not send another PURPLE_IM_TYPING message.
 */
void purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val);

/**
 * Returns the time after which another PURPLE_IM_TYPING message should be sent.
 *
 * @param im The IM.
 *
 * @return The time in seconds since the epoch.  Or 0 if no additional
 *         PURPLE_IM_TYPING message should be sent.
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

/*@}*/

/**************************************************************************/
/** @name Chat Conversation API                                           */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the ChatConversation object.
 */
GType purple_chat_conversation_get_type(void);

/**
 * Creates a new chat conversation.
 *
 * @param account The account opening the conversation window on the purple
 *                user's end.
 * @param name    The name of the conversation.
 *
 * @return The new conversation.
 */
PurpleChatConversation *purple_chat_conversation_new(PurpleAccount *account,
		const char *name);

/**
 * Returns a list of users in the chat room.  The members of the list
 * are PurpleChatUser objects.
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
 * Returns who set the chat room's topic.
 *
 * @param chat The chat.
 *
 * @return Who set the topic.
 */
const char *purple_chat_conversation_get_topic_who(const PurpleChatConversation *chat);

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
 * Adds a user to a chat.
 *
 * @param chat        The chat.
 * @param user        The user to add.
 * @param extra_msg   An extra message to display with the join message.
 * @param flags       The users flags
 * @param new_arrival Decides whether or not to show a join notice.
 */
void purple_chat_conversation_add_user(PurpleChatConversation *chat, const char *user,
							 const char *extra_msg, PurpleChatUserFlags flags,
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
 * Checks if a user is in a chat
 *
 * @param chat   The chat.
 * @param user   The user to look for.
 *
 * @return TRUE if the user is in the chat, FALSE if not
 */
gboolean purple_chat_conversation_has_user(PurpleChatConversation *chat,
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
 * Find a chat user in a chat
 *
 * @param chat The chat.
 * @param name The name of the chat user to find.
 */
PurpleChatUser *purple_chat_conversation_find_user(PurpleChatConversation *chat,
		const char *name);

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
/** @name Chat Conversation User API                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the ChatConversationBuddy object.
 */
GType purple_chat_user_get_type(void);

/**
 * Set the chat conversation associated with this chat user.
 *
 * @param cb	The chat user
 * @param chat	The chat conversation that the buddy belongs to.
 */
void purple_chat_user_set_chat(PurpleChatUser *cb,
		PurpleChatConversation *chat);

/**
 * Get the chat conversation associated with this chat user.
 *
 * @param cb	The chat user.
 *
 * @return		The chat conversation that the buddy belongs to.
 */
PurpleChatConversation *purple_chat_user_get_chat(const PurpleChatUser *cb);

/**
 * Creates a new chat user
 *
 * @param chat The chat that the buddy belongs to.
 * @param name The name.
 * @param alias The alias.
 * @param flags The flags.
 *
 * @return The new chat user
 */
PurpleChatUser *purple_chat_user_new(PurpleChatConversation *chat,
		const char *name, const char *alias, PurpleChatUserFlags flags);

/**
 * Get the alias of a chat user
 *
 * @param cb    The chat user.
 *
 * @return The alias of the chat user.
 */
const char *purple_chat_user_get_alias(const PurpleChatUser *cb);

/**
 * Get the name of a chat user
 *
 * @param cb    The chat user.
 *
 * @return The name of the chat user.
 */
const char *purple_chat_user_get_name(const PurpleChatUser *cb);

/**
 * Set the flags of a chat user.
 *
 * @param cb     The chat user.
 * @param flags  The new flags.
 */
void purple_chat_user_set_flags(PurpleChatUser *cb, PurpleChatUserFlags flags);

/**
 * Get the flags of a chat user.
 *
 * @param cb	The chat user.
 *
 * @return The flags of the chat user.
 */
PurpleChatUserFlags purple_chat_user_get_flags(const PurpleChatUser *cb);

/**
 * Sets if this chat user is on the buddy list.
 *
 * @param cb	The chat user.
 * @param buddy TRUE if the chat user is on the buddy list.
 */
void purple_chat_user_set_buddy(const PurpleChatUser *cb, gboolean buddy);

/**
 * Indicates if this chat user is on the buddy list.
 *
 * @param cb	The chat user.
 *
 * @return TRUE if the chat user is on the buddy list.
 */
gboolean purple_chat_user_is_buddy(const PurpleChatUser *cb);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CONVERSATION_TYPES_H_ */
