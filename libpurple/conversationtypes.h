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
/**
 * SECTION:conversationtypes
 * @section_id: libpurple-conversationtypes
 * @short_description: <filename>conversationtypes.h</filename>
 * @title: Chat and IM Conversation Objects
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

typedef struct _PurpleIMConversation         PurpleIMConversation;
typedef struct _PurpleIMConversationClass    PurpleIMConversationClass;

#define PURPLE_TYPE_CHAT_CONVERSATION            (purple_chat_conversation_get_type())
#define PURPLE_CHAT_CONVERSATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversation))
#define PURPLE_CHAT_CONVERSATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationClass))
#define PURPLE_IS_CHAT_CONVERSATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT_CONVERSATION))
#define PURPLE_IS_CHAT_CONVERSATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT_CONVERSATION))
#define PURPLE_CHAT_CONVERSATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationClass))

typedef struct _PurpleChatConversation       PurpleChatConversation;
typedef struct _PurpleChatConversationClass  PurpleChatConversationClass;

#define PURPLE_TYPE_CHAT_USER                    (purple_chat_user_get_type())
#define PURPLE_CHAT_USER(obj)                    (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT_USER, PurpleChatUser))
#define PURPLE_CHAT_USER_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT_USER, PurpleChatUserClass))
#define PURPLE_IS_CHAT_USER(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT_USER))
#define PURPLE_IS_CHAT_USER_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT_USER))
#define PURPLE_CHAT_USER_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT_USER, PurpleChatUserClass))

typedef struct _PurpleChatUser       PurpleChatUser;
typedef struct _PurpleChatUserClass  PurpleChatUserClass;

/**
 * PurpleIMTypingState:
 * @PURPLE_IM_NOT_TYPING: Not typing.
 * @PURPLE_IM_TYPING:     Currently typing.
 * @PURPLE_IM_TYPED:      Stopped typing momentarily.
 *
 * The typing state of a user.
 */
typedef enum
{
	PURPLE_IM_NOT_TYPING = 0,
	PURPLE_IM_TYPING,
	PURPLE_IM_TYPED

} PurpleIMTypingState;

/**
 * PurpleChatUserFlags:
 * @PURPLE_CHAT_USER_NONE:    No flags
 * @PURPLE_CHAT_USER_VOICE:   Voiced user or "Participant"
 * @PURPLE_CHAT_USER_HALFOP:  Half-op
 * @PURPLE_CHAT_USER_OP:      Channel Op or Moderator
 * @PURPLE_CHAT_USER_FOUNDER: Channel Founder
 * @PURPLE_CHAT_USER_TYPING:  Currently typing
 * @PURPLE_CHAT_USER_AWAY:    Currently away.
 *
 * Flags applicable to users in Chats.
 */
typedef enum /*< flags >*/
{
	PURPLE_CHAT_USER_NONE     = 0x0000,
	PURPLE_CHAT_USER_VOICE    = 0x0001,
	PURPLE_CHAT_USER_HALFOP   = 0x0002,
	PURPLE_CHAT_USER_OP       = 0x0004,
	PURPLE_CHAT_USER_FOUNDER  = 0x0008,
	PURPLE_CHAT_USER_TYPING   = 0x0010,
	PURPLE_CHAT_USER_AWAY     = 0x0020

} PurpleChatUserFlags;

#include "conversation.h"

/**************************************************************************/
/** PurpleIMConversation                                                  */
/**************************************************************************/
/**
 * PurpleIMConversation:
 *
 * Structure representing an IM conversation instance.
 */
struct _PurpleIMConversation
{
	PurpleConversation parent_object;
};

/**
 * PurpleIMConversationClass:
 *
 * Base class for all #PurpleIMConversation's
 */
struct _PurpleIMConversationClass {
	PurpleConversationClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleChatConversation                                                */
/**************************************************************************/
/**
 * PurpleChatConversation:
 *
 * Structure representing a chat conversation instance.
 */
struct _PurpleChatConversation
{
	PurpleConversation parent_object;
};

/**
 * PurpleChatConversationClass:
 *
 * Base class for all #PurpleChatConversation's
 */
struct _PurpleChatConversationClass {
	PurpleConversationClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleChatUser                                                        */
/**************************************************************************/
/**
 * PurpleChatUser:
 * @ui_data: The UI data associated with this chat user. This is a convenience
 *           field provided to the UIs -- it is not used by the libpurple core.
 *
 * Structure representing a chat user instance.
 */
struct _PurpleChatUser
{
	GObject gparent;

	/*< public >*/
	gpointer ui_data;
};

/**
 * PurpleChatUserClass:
 *
 * Base class for all #PurpleChatUser's
 */
struct _PurpleChatUserClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* IM Conversation API                                                    */
/**************************************************************************/

/**
 * purple_im_conversation_get_type:
 *
 * Returns: The #GType for the IMConversation object.
 */
GType purple_im_conversation_get_type(void);

/**
 * purple_im_conversation_new:
 * @account: The account opening the conversation window on the purple
 *                user's end.
 * @name:    Name of the buddy.
 *
 * Creates a new IM conversation.
 *
 * Returns: The new conversation.
 */
PurpleIMConversation *purple_im_conversation_new(PurpleAccount *account,
		const char *name);

/**
 * purple_im_conversation_set_icon:
 * @im:   The IM.
 * @icon: The buddy icon.
 *
 * Sets the IM's buddy icon.
 *
 * This should only be called from within Purple. You probably want to
 * call purple_buddy_icon_set_data().
 *
 * See purple_buddy_icon_set_data().
 */
void purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon);

/**
 * purple_im_conversation_get_icon:
 * @im: The IM.
 *
 * Returns the IM's buddy icon.
 *
 * Returns: The buddy icon.
 */
PurpleBuddyIcon *purple_im_conversation_get_icon(const PurpleIMConversation *im);

/**
 * purple_im_conversation_set_typing_state:
 * @im:    The IM.
 * @state: The typing state.
 *
 * Sets the IM's typing state.
 */
void purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMTypingState state);

/**
 * purple_im_conversation_get_typing_state:
 * @im: The IM.
 *
 * Returns the IM's typing state.
 *
 * Returns: The IM's typing state.
 */
PurpleIMTypingState purple_im_conversation_get_typing_state(const PurpleIMConversation *im);

/**
 * purple_im_conversation_start_typing_timeout:
 * @im:      The IM.
 * @timeout: How long in seconds to wait before setting the typing state
 *        to PURPLE_IM_NOT_TYPING.
 *
 * Starts the IM's typing timeout.
 */
void purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout);

/**
 * purple_im_conversation_stop_typing_timeout:
 * @im: The IM.
 *
 * Stops the IM's typing timeout.
 */
void purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im);

/**
 * purple_im_conversation_get_typing_timeout:
 * @im: The IM.
 *
 * Returns the IM's typing timeout.
 *
 * Returns: The timeout.
 */
guint purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im);

/**
 * purple_im_conversation_set_type_again:
 * @im:  The IM.
 * @val: The number of seconds to wait before allowing another
 *            PURPLE_IM_TYPING message to be sent to the user.  Or 0 to
 *            not send another PURPLE_IM_TYPING message.
 *
 * Sets the quiet-time when no PURPLE_IM_TYPING messages will be sent.
 * Few protocols need this (maybe only MSN).  If the user is still
 * typing after this quiet-period, then another PURPLE_IM_TYPING message
 * will be sent.
 */
void purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val);

/**
 * purple_im_conversation_get_type_again:
 * @im: The IM.
 *
 * Returns the time after which another PURPLE_IM_TYPING message should be sent.
 *
 * Returns: The time in seconds since the epoch.  Or 0 if no additional
 *         PURPLE_IM_TYPING message should be sent.
 */
time_t purple_im_conversation_get_type_again(const PurpleIMConversation *im);

/**
 * purple_im_conversation_start_send_typed_timeout:
 * @im:      The IM.
 *
 * Starts the IM's type again timeout.
 */
void purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im);

/**
 * purple_im_conversation_stop_send_typed_timeout:
 * @im: The IM.
 *
 * Stops the IM's type again timeout.
 */
void purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im);

/**
 * purple_im_conversation_get_send_typed_timeout:
 * @im: The IM.
 *
 * Returns the IM's type again timeout interval.
 *
 * Returns: The type again timeout interval.
 */
guint purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im);

/**
 * purple_im_conversation_update_typing:
 * @im: The IM.
 *
 * Updates the visual typing notification for an IM conversation.
 */
void purple_im_conversation_update_typing(PurpleIMConversation *im);

/**************************************************************************/
/* Chat Conversation API                                                  */
/**************************************************************************/

/**
 * purple_chat_conversation_get_type:
 *
 * Returns: The #GType for the ChatConversation object.
 */
GType purple_chat_conversation_get_type(void);

/**
 * purple_chat_conversation_new:
 * @account: The account opening the conversation window on the purple
 *                user's end.
 * @name:    The name of the conversation.
 *
 * Creates a new chat conversation.
 *
 * Returns: The new conversation.
 */
PurpleChatConversation *purple_chat_conversation_new(PurpleAccount *account,
		const char *name);

/**
 * purple_chat_conversation_get_users:
 * @chat: The chat.
 *
 * Returns a list of users in the chat room.  The members of the list
 * are PurpleChatUser objects.
 *
 * Returns: (transfer none): The list of users.
 */
GList *purple_chat_conversation_get_users(const PurpleChatConversation *chat);

/**
 * purple_chat_conversation_ignore:
 * @chat: The chat.
 * @name: The name of the user.
 *
 * Ignores a user in a chat room.
 */
void purple_chat_conversation_ignore(PurpleChatConversation *chat, const char *name);

/**
 * purple_chat_conversation_unignore:
 * @chat: The chat.
 * @name: The name of the user.
 *
 * Unignores a user in a chat room.
 */
void purple_chat_conversation_unignore(PurpleChatConversation *chat, const char *name);

/**
 * purple_chat_conversation_set_ignored:
 * @chat:    The chat.
 * @ignored: The list of ignored users.
 *
 * Sets the list of ignored users in the chat room.
 *
 * Returns: The list passed.
 */
GList *purple_chat_conversation_set_ignored(PurpleChatConversation *chat, GList *ignored);

/**
 * purple_chat_conversation_get_ignored:
 * @chat: The chat.
 *
 * Returns the list of ignored users in the chat room.
 *
 * Returns: (transfer none): The list of ignored users.
 */
GList *purple_chat_conversation_get_ignored(const PurpleChatConversation *chat);

/**
 * purple_chat_conversation_get_ignored_user:
 * @chat: The chat.
 * @user: The user to check in the ignore list.
 *
 * Returns the actual name of the specified ignored user, if it exists in
 * the ignore list.
 *
 * If the user found contains a prefix, such as '+' or '\@', this is also
 * returned. The username passed to the function does not have to have this
 * formatting.
 *
 * Returns: The ignored user if found, complete with prefixes, or %NULL
 *         if not found.
 */
const char *purple_chat_conversation_get_ignored_user(const PurpleChatConversation *chat,
											const char *user);

/**
 * purple_chat_conversation_is_ignored_user:
 * @chat: The chat.
 * @user: The user.
 *
 * Returns %TRUE if the specified user is ignored.
 *
 * Returns: %TRUE if the user is in the ignore list; %FALSE otherwise.
 */
gboolean purple_chat_conversation_is_ignored_user(const PurpleChatConversation *chat,
										const char *user);

/**
 * purple_chat_conversation_set_topic:
 * @chat:  The chat.
 * @who:   The user that set the topic.
 * @topic: The topic.
 *
 * Sets the chat room's topic.
 */
void purple_chat_conversation_set_topic(PurpleChatConversation *chat, const char *who,
							  const char *topic);

/**
 * purple_chat_conversation_get_topic:
 * @chat: The chat.
 *
 * Returns the chat room's topic.
 *
 * Returns: The chat's topic.
 */
const char *purple_chat_conversation_get_topic(const PurpleChatConversation *chat);

/**
 * purple_chat_conversation_get_topic_who:
 * @chat: The chat.
 *
 * Returns who set the chat room's topic.
 *
 * Returns: Who set the topic.
 */
const char *purple_chat_conversation_get_topic_who(const PurpleChatConversation *chat);

/**
 * purple_chat_conversation_set_id:
 * @chat: The chat.
 * @id:   The ID.
 *
 * Sets the chat room's ID.
 */
void purple_chat_conversation_set_id(PurpleChatConversation *chat, int id);

/**
 * purple_chat_conversation_get_id:
 * @chat: The chat.
 *
 * Returns the chat room's ID.
 *
 * Returns: The ID.
 */
int purple_chat_conversation_get_id(const PurpleChatConversation *chat);

/**
 * purple_chat_conversation_add_user:
 * @chat:        The chat.
 * @user:        The user to add.
 * @extra_msg:   An extra message to display with the join message.
 * @flags:       The users flags
 * @new_arrival: Decides whether or not to show a join notice.
 *
 * Adds a user to a chat.
 */
void purple_chat_conversation_add_user(PurpleChatConversation *chat, const char *user,
							 const char *extra_msg, PurpleChatUserFlags flags,
							 gboolean new_arrival);

/**
 * purple_chat_conversation_add_users:
 * @chat:         The chat.
 * @users:        The list of users to add.
 * @extra_msgs:   An extra message to display with the join message for each
 *                     user.  This list may be shorter than @users, in which
 *                     case, the users after the end of extra_msgs will not have
 *                     an extra message.  By extension, this means that extra_msgs
 *                     can simply be %NULL and none of the users will have an
 *                     extra message.
 * @flags:        The list of flags for each user.
 * @new_arrivals: Decides whether or not to show join notices.
 *
 * Adds a list of users to a chat.
 *
 * The data is copied from @users, @extra_msgs, and @flags, so it is up to
 * the caller to free this list after calling this function.
 */
void purple_chat_conversation_add_users(PurpleChatConversation *chat,
		GList *users, GList *extra_msgs, GList *flags, gboolean new_arrivals);

/**
 * purple_chat_conversation_rename_user:
 * @chat:     The chat.
 * @old_user: The old username.
 * @new_user: The new username.
 *
 * Renames a user in a chat.
 */
void purple_chat_conversation_rename_user(PurpleChatConversation *chat,
		const char *old_user, const char *new_user);

/**
 * purple_chat_conversation_remove_user:
 * @chat:   The chat.
 * @user:   The user that is being removed.
 * @reason: The optional reason given for the removal. Can be %NULL.
 *
 * Removes a user from a chat, optionally with a reason.
 *
 * It is up to the developer to free this list after calling this function.
 */
void purple_chat_conversation_remove_user(PurpleChatConversation *chat,
		const char *user, const char *reason);

/**
 * purple_chat_conversation_remove_users:
 * @chat:   The chat.
 * @users:  The users that are being removed.
 * @reason: The optional reason given for the removal. Can be %NULL.
 *
 * Removes a list of users from a chat, optionally with a single reason.
 */
void purple_chat_conversation_remove_users(PurpleChatConversation *chat,
		GList *users, const char *reason);

/**
 * purple_chat_conversation_has_user:
 * @chat:   The chat.
 * @user:   The user to look for.
 *
 * Checks if a user is in a chat
 *
 * Returns: TRUE if the user is in the chat, FALSE if not
 */
gboolean purple_chat_conversation_has_user(PurpleChatConversation *chat,
		const char *user);

/**
 * purple_chat_conversation_clear_users:
 * @chat: The chat.
 *
 * Clears all users from a chat.
 */
void purple_chat_conversation_clear_users(PurpleChatConversation *chat);

/**
 * purple_chat_conversation_set_nick:
 * @chat: The chat.
 * @nick: The nick.
 *
 * Sets your nickname (used for hilighting) for a chat.
 */
void purple_chat_conversation_set_nick(PurpleChatConversation *chat,
		const char *nick);

/**
 * purple_chat_conversation_get_nick:
 * @chat: The chat.
 *
 * Gets your nickname (used for hilighting) for a chat.
 *
 * Returns:  The nick.
 */
const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat);

/**
 * purple_chat_conversation_leave:
 * @chat: The chat.
 *
 * Lets the core know we left a chat, without destroying it.
 * Called from serv_got_chat_left().
 */
void purple_chat_conversation_leave(PurpleChatConversation *chat);

/**
 * purple_chat_conversation_find_user:
 * @chat: The chat.
 * @name: The name of the chat user to find.
 *
 * Find a chat user in a chat
 */
PurpleChatUser *purple_chat_conversation_find_user(PurpleChatConversation *chat,
		const char *name);

/**
 * purple_chat_conversation_invite_user:
 * @chat:     The chat.
 * @user:     The user to invite to the chat.
 * @message:  The message to send with the invitation.
 * @confirm:  Prompt before sending the invitation. The user is always
 *            prompted if either \a user or \a message is %NULL.
 *
 * Invite a user to a chat.
 * The user will be prompted to enter the user's name or a message if one is
 * not given.
 */
void purple_chat_conversation_invite_user(PurpleChatConversation *chat,
		const char *user, const char *message, gboolean confirm);

/**
 * purple_chat_conversation_has_left:
 * @chat: The chat.
 *
 * Returns true if we're no longer in this chat,
 * and just left the window open.
 *
 * Returns: %TRUE if we left the chat already, %FALSE if
 * we're still there.
 */
gboolean purple_chat_conversation_has_left(PurpleChatConversation *chat);

/**************************************************************************/
/* Chat Conversation User API                                             */
/**************************************************************************/

/**
 * purple_chat_user_get_type:
 *
 * Returns: The #GType for the ChatConversationBuddy object.
 */
GType purple_chat_user_get_type(void);

/**
 * purple_chat_user_set_chat:
 * @cb:	The chat user
 * @chat:	The chat conversation that the buddy belongs to.
 *
 * Set the chat conversation associated with this chat user.
 */
void purple_chat_user_set_chat(PurpleChatUser *cb,
		PurpleChatConversation *chat);

/**
 * purple_chat_user_get_chat:
 * @cb:	The chat user.
 *
 * Get the chat conversation associated with this chat user.
 *
 * Returns:		The chat conversation that the buddy belongs to.
 */
PurpleChatConversation *purple_chat_user_get_chat(const PurpleChatUser *cb);

/**
 * purple_chat_user_new:
 * @chat: The chat that the buddy belongs to.
 * @name: The name.
 * @alias: The alias.
 * @flags: The flags.
 *
 * Creates a new chat user
 *
 * Returns: The new chat user
 */
PurpleChatUser *purple_chat_user_new(PurpleChatConversation *chat,
		const char *name, const char *alias, PurpleChatUserFlags flags);

/**
 * purple_chat_user_set_ui_data:
 * @cb:			The chat user
 * @ui_data:		A pointer to associate with this chat user.
 *
 * Set the UI data associated with this chat user.
 */
void purple_chat_user_set_ui_data(PurpleChatUser *cb, gpointer ui_data);

/**
 * purple_chat_user_get_ui_data:
 * @cb:			The chat user.
 *
 * Get the UI data associated with this chat user.
 *
 * Returns: The UI data associated with this chat user.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_chat_user_get_ui_data(const PurpleChatUser *cb);

/**
 * purple_chat_user_get_alias:
 * @cb:    The chat user.
 *
 * Get the alias of a chat user
 *
 * Returns: The alias of the chat user.
 */
const char *purple_chat_user_get_alias(const PurpleChatUser *cb);

/**
 * purple_chat_user_get_name:
 * @cb:    The chat user.
 *
 * Get the name of a chat user
 *
 * Returns: The name of the chat user.
 */
const char *purple_chat_user_get_name(const PurpleChatUser *cb);

/**
 * purple_chat_user_set_flags:
 * @cb:     The chat user.
 * @flags:  The new flags.
 *
 * Set the flags of a chat user.
 */
void purple_chat_user_set_flags(PurpleChatUser *cb, PurpleChatUserFlags flags);

/**
 * purple_chat_user_get_flags:
 * @cb:	The chat user.
 *
 * Get the flags of a chat user.
 *
 * Returns: The flags of the chat user.
 */
PurpleChatUserFlags purple_chat_user_get_flags(const PurpleChatUser *cb);

/**
 * purple_chat_user_is_buddy:
 * @cb:	The chat user.
 *
 * Indicates if this chat user is on the buddy list.
 *
 * Returns: TRUE if the chat user is on the buddy list.
 */
gboolean purple_chat_user_is_buddy(const PurpleChatUser *cb);

G_END_DECLS

#endif /* _PURPLE_CONVERSATION_TYPES_H_ */
