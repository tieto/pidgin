/**
 * @file gtkconv.h GTK+ Conversation API
 * @ingroup gtkui
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
 */
#ifndef _GAIM_GTKCONVERSATION_H_
#define _GAIM_GTKCONVERSATION_H_

typedef struct _GaimGtkImPane       GaimGtkImPane;
typedef struct _GaimGtkChatPane     GaimGtkChatPane;
typedef struct _GaimGtkConversation GaimGtkConversation;

/**
 * Unseen text states.
 */
typedef enum
{
	GAIM_UNSEEN_NONE  = 0, /**< No unseen text in the conversation. */
	GAIM_UNSEEN_EVENT = 1, /**< Unseen events in the conversation.  */
	GAIM_UNSEEN_NOLOG = 2, /**< Unseen text with NO_LOG flag.       */
	GAIM_UNSEEN_TEXT  = 3, /**< Unseen text in the conversation.    */
	GAIM_UNSEEN_NICK  = 4  /**< Unseen text and the nick was said.  */
} GaimUnseenState;

enum {
	CHAT_USERS_ICON_COLUMN,
	CHAT_USERS_ALIAS_COLUMN,
	CHAT_USERS_NAME_COLUMN,
	CHAT_USERS_FLAGS_COLUMN,
	CHAT_USERS_COLOR_COLUMN,
	CHAT_USERS_BUDDY_COLUMN,
	CHAT_USERS_COLUMNS
};

#define GAIM_GTK_CONVERSATION(conv) \
	((GaimGtkConversation *)(conv)->ui_data)

#define GAIM_IS_GTK_CONVERSATION(conv) \
	(gaim_conversation_get_ui_ops(conv) == \
	 gaim_gtk_conversations_get_conv_ui_ops())

#include "gtkgaim.h"
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
struct _GaimGtkImPane
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
struct _GaimGtkChatPane
{
	GtkWidget *count;
	GtkWidget *list;
	GtkWidget *topic_text;
	GtkWidget *userlist_im;
	GtkWidget *userlist_ignore;
	GtkWidget *userlist_info;
};

/**
 * A GTK+ conversation pane.
 */
struct _GaimGtkConversation
{
	GaimConversation *active_conv;
	GList *convs;

	GaimGtkWindow *win;

	gboolean make_sound;
	gboolean show_formatting_toolbar;
	gboolean show_timestamps;

	GtkTooltips *tooltips;

	GtkWidget *tab_cont;
	GtkWidget *tabby;
	GtkWidget *menu_tabby;

	GtkWidget *imhtml;
	GtkTextBuffer *entry_buffer;
	GtkWidget *entry;

	GtkWidget *close; /* "x" on the tab */
	GtkWidget *icon;
	GtkWidget *tab_label;
	GtkWidget *menu_icon;
	GtkWidget *menu_label;
	GtkSizeGroup *sg;

	GtkWidget *lower_hbox;

	GtkWidget *toolbar;

	GaimUnseenState unseen_state;

	struct
	{
		GtkWidget *image;
		GtkWidget *search;

	} dialogs;

	union
	{
		GaimGtkImPane   *im;
		GaimGtkChatPane *chat;

	} u;
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
GaimConversationUiOps *gaim_gtk_conversations_get_conv_ui_ops(void);

/**
 * Updates the buddy icon on a conversation.
 *
 * @param conv The conversation.
 */
void gaim_gtkconv_update_buddy_icon(GaimConversation *conv);

/**
 * Sets the active conversation within a GTK-conversation.
 *
 * @param conv The conversation
 */
void gaim_gtkconv_switch_active_conversation(GaimConversation *conv);

/**
 * Updates the tab positions on all conversation windows to reflect any
 * changed preferences.
 */
void gaim_gtkconv_update_tabs(void);

/**
 * Updates conversation buttons by protocol.
 *
 * @param conv The conversation.
 */
void gaim_gtkconv_update_buttons_by_protocol(GaimConversation *conv);

/**
 * Finds the first conversation of the given type which has an unseen
 * state greater than or equal to the specified minimum state.
 *
 * @param type      The type of conversation.
 * @param min_state The minimum unseen state.
 * @return          First conversation matching criteria, or NULL.
 */
GaimConversation *
gaim_gtk_conversations_get_first_unseen(GaimConversationType type,
                                        GaimUnseenState min_state);

/**
 * Presents a gaim conversation to the user.
 *
 * @param conv The conversation.
 */
void gaim_gtkconv_present_conversation(GaimConversation *conv);

/**
 * Finds the first conversations of the given type which has an
 * unseen state greater than or equal to the given minimum
 * state.
 *
 * @param type      The type of conversation.
 * @param min_state The minimum unseen state.
 * @return          First conversation matching critera, or NULL.
 */
GaimConversation * gaim_gtk_conversations_get_first_unseen(
		GaimConversationType type, GaimUnseenState min_state);

GaimGtkWindow *gaim_gtkconv_get_window(GaimGtkConversation *gtkconv);
GdkPixbuf *gaim_gtkconv_get_tab_icon(GaimConversation *conv, gboolean small_icon);
void gaim_gtkconv_new(GaimConversation *conv);
int gaim_gtkconv_get_tab_at_xy(GaimGtkWindow *win, int x, int y, gboolean *to_right);
/*@}*/

/**************************************************************************/
/** @name GTK+ Conversations Subsystem                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns the gtk conversations subsystem handle.
 *
 * @return The conversations subsystem handle.
 */
void *gaim_gtk_conversations_get_handle(void);

/**
 * Initializes the GTK+ conversations subsystem.
 */
void gaim_gtk_conversations_init(void);

/**
 * Uninitialized the GTK+ conversation subsystem.
 */
void gaim_gtk_conversations_uninit(void);

/*@}*/

#endif /* _GAIM_GTKCONVERSATION_H_ */
