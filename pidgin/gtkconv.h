/**
 * @file gtkconv.h GTK+ Conversation API
 * @ingroup gtkui
 *
 * gaim
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 */
#ifndef _PIDGIN_CONVERSATION_H_
#define _PIDGIN_CONVERSATION_H_

typedef struct _PidginImPane       PidginImPane;
typedef struct _PidginChatPane     PidginChatPane;
typedef struct _PidginConversation PidginConversation;

/**
 * Unseen text states.
 */
typedef enum
{
	PIDGIN_UNSEEN_NONE,   /**< No unseen text in the conversation. */
	PIDGIN_UNSEEN_EVENT,  /**< Unseen events in the conversation.  */
	PIDGIN_UNSEEN_NO_LOG, /**< Unseen text with NO_LOG flag.       */
	PIDGIN_UNSEEN_TEXT,   /**< Unseen text in the conversation.    */
	PIDGIN_UNSEEN_NICK    /**< Unseen text and the nick was said.  */
} GaimUnseenState;

enum {
	CHAT_USERS_ICON_COLUMN,
	CHAT_USERS_ALIAS_COLUMN,
	CHAT_USERS_ALIAS_KEY_COLUMN,
	CHAT_USERS_NAME_COLUMN,
	CHAT_USERS_FLAGS_COLUMN,
	CHAT_USERS_COLOR_COLUMN,
	CHAT_USERS_WEIGHT_COLUMN,
	CHAT_USERS_COLUMNS
};

#define PIDGIN_CONVERSATION(conv) \
	((PidginConversation *)(conv)->ui_data)

#define PIDGIN_IS_PIDGIN_CONVERSATION(conv) \
	(gaim_conversation_get_ui_ops(conv) == \
	 pidgin_conversations_get_conv_ui_ops())

#include "pidgin.h"
#include "conversation.h"
#include "gtkconvwin.h"

/**************************************************************************
 * @name Structures
 **************************************************************************/
/*@{*/

/**
 * A GTK+ representation of a graphical window containing one or more
 * conversations.
 */

/**
 * A GTK+ Instant Message pane.
 */
struct _PidginImPane
{
	GtkWidget *block;
	GtkWidget *send_file;
	GtkWidget *sep1;
	GtkWidget *sep2;
	GtkWidget *check;
	GtkWidget *progress;

	/* Buddy icon stuff */
	GtkWidget *icon_container;
	GtkWidget *icon;
	gboolean show_icon;
	gboolean animate;
	GdkPixbufAnimation *anim;
	GdkPixbufAnimationIter *iter;
	guint32 icon_timer;
};

/**
 * GTK+ Chat panes.
 */
struct _PidginChatPane
{
	GtkWidget *count;
	GtkWidget *list;
	GtkWidget *topic_text;
};

/**
 * A GTK+ conversation pane.
 */
struct _PidginConversation
{
	GaimConversation *active_conv;
	GList *convs;
	GList *send_history;

	PidginWindow *win;

	gboolean make_sound;

	GtkTooltips *tooltips;

	GtkWidget *tab_cont;
	GtkWidget *tabby;
	GtkWidget *menu_tabby;

	GtkWidget *imhtml;
	GtkTextBuffer *entry_buffer;
	GtkWidget *entry;
	gboolean auto_resize;   /* this is set to TRUE if the conversation
		 	 	 * is being resized by a non-user-initiated
		 		 * event, such as the buddy icon appearing
				 */
	gboolean entry_growing; /* True if the size of the entry was set
				 * automatically by typing too much to fit
				 * in one line */

	GtkWidget *close; /* "x" on the tab */
	GtkWidget *icon;
	GtkWidget *tab_label;
	GtkWidget *menu_icon;
	GtkWidget *menu_label;
	GtkSizeGroup *sg;

	GtkWidget *lower_hbox;

	GtkWidget *toolbar;

	GaimUnseenState unseen_state;
	guint unseen_count;

	union
	{
		PidginImPane   *im;
		PidginChatPane *chat;

	} u;

	time_t newday;
};

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation API
 **************************************************************************/
/*@{*/

/**
 * Returns the UI operations structure for GTK+ conversations.
 *
 * @return The GTK+ conversation operations structure.
 */
GaimConversationUiOps *pidgin_conversations_get_conv_ui_ops(void);

/**
 * Updates the buddy icon on a conversation.
 *
 * @param conv The conversation.
 */
void pidgin_conv_update_buddy_icon(GaimConversation *conv);

/**
 * Sets the active conversation within a GTK-conversation.
 *
 * @param conv The conversation
 */
void pidgin_conv_switch_active_conversation(GaimConversation *conv);

/**
 * Updates conversation buttons by protocol.
 *
 * @param conv The conversation.
 */
void pidgin_conv_update_buttons_by_protocol(GaimConversation *conv);

/**
 * Returns a list of conversations of the given type which have an unseen
 * state greater than or equal to the specified minimum state. Using the
 * hidden_only parameter, this search can be limited to hidden
 * conversations. The max_count parameter will limit the total number of
 * converations returned if greater than zero. The returned list should
 * be freed by the caller.
 *
 * @param type         The type of conversation.
 * @param min_state    The minimum unseen state.
 * @param hidden_only  If TRUE, only consider hidden conversations.
 * @param max_count    Maximum number of conversations to return, or 0 for
 *                     no maximum.
 * @return             List of GaimConversation matching criteria, or NULL.
 */
GList *
pidgin_conversations_find_unseen_list(GaimConversationType type,
										GaimUnseenState min_state,
										gboolean hidden_only,
										guint max_count);

/**
 * Fill a menu with a list of conversations. Clicking the conversation
 * menu item will present that conversation to the user.
 *
 * @param menu   Menu widget to add items to.
 * @param convs  List of GaimConversation to add to menu.
 * @return       Number of conversations added to menu.
 */
guint
pidgin_conversations_fill_menu(GtkWidget *menu, GList *convs);

/**
 * Presents a gaim conversation to the user.
 *
 * @param conv The conversation.
 */
void pidgin_conv_present_conversation(GaimConversation *conv);

PidginWindow *pidgin_conv_get_window(PidginConversation *gtkconv);
GdkPixbuf *pidgin_conv_get_tab_icon(GaimConversation *conv, gboolean small_icon);
void pidgin_conv_new(GaimConversation *conv);
int pidgin_conv_get_tab_at_xy(PidginWindow *win, int x, int y, gboolean *to_right);
gboolean pidgin_conv_is_hidden(PidginConversation *gtkconv);
/*@}*/

/**************************************************************************/
/** @name GTK+ Conversations Subsystem                                    */
/**************************************************************************/
/*@{*/

/**
 * Returns the gtk conversations subsystem handle.
 *
 * @return The conversations subsystem handle.
 */
void *pidgin_conversations_get_handle(void);

/**
 * Initializes the GTK+ conversations subsystem.
 */
void pidgin_conversations_init(void);

/**
 * Uninitialized the GTK+ conversation subsystem.
 */
void pidgin_conversations_uninit(void);

/*@}*/

#endif /* _PIDGIN_CONVERSATION_H_ */
