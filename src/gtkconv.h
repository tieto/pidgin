/**
 * @file gtkconv.h GTK+ Conversation API
 * @ingroup gtkui
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

#ifndef _GAIM_GTK_CONVERSATION_H_
#define _GAIM_GTK_CONVERSATION_H_

#include "conversation.h"

/**************************************************************************
 * @name Structures
 **************************************************************************/
/*@{*/

typedef struct _GaimGtkWindow       GaimGtkWindow;
typedef struct _GaimGtkImPane       GaimGtkImPane;
typedef struct _GaimGtkChatPane     GaimGtkChatPane;
typedef struct _GaimGtkConversation GaimGtkConversation;

/**
 * A GTK+ representation of a graphical window containing one or more
 * conversations.
 */
struct _GaimGtkWindow
{
	GtkWidget *window;           /**< The window.                      */
	GtkWidget *notebook;         /**< The notebook of conversations.   */

	struct
	{
		GtkWidget *menubar;

		GtkWidget *view_log;
		GtkWidget *insert_link;
		GtkWidget *insert_image;
		GtkWidget *logging;
		GtkWidget *sounds;
		GtkWidget *send_as;
		GtkWidget *typing_icon;

		GtkItemFactory *item_factory;

	} menu;

	/* Tab dragging stuff. */
	gboolean in_drag;
	gboolean in_predrag;

	gint drag_min_x, drag_max_x, drag_min_y, drag_max_y;

	gint drag_motion_signal;
	gint drag_leave_signal;
};

/**
 * GTK+ Instant Message panes.
 */
struct _GaimGtkImPane
{
	GtkWidget *warn;
	GtkWidget *block;
	GtkWidget *add;
	GtkWidget *sep1;
	GtkWidget *sep2;
	GtkWidget *check;
	GtkWidget *progress;

	gboolean a_virgin;

	/* Buddy icon stuff */
	GtkWidget *icon;
	GdkPixbufAnimation *anim;
	GdkPixbufAnimationIter *iter;
	guint32 icon_timer;
	GtkWidget *save_icon;
};

/**
 * GTK+ Chat panes.
 */
struct _GaimGtkChatPane
{
	GtkWidget *count;
	GtkWidget *list;
	GtkWidget *whisper;
	GtkWidget *invite;
	GtkWidget *topic_text;
};

/**
 * A GTK+ conversation pane.
 */
struct _GaimGtkConversation
{
	gboolean make_sound;
	gboolean has_font;
	char fontface[128];
	GdkColor fg_color;
	GdkColor bg_color;

	GtkTooltips *tooltips;

	GtkWidget *tab_cont;
	GtkWidget *tabby;

	GtkWidget *imhtml;
	GtkTextBuffer *entry_buffer;
	GtkWidget *entry;

	GtkWidget *send;
	GtkWidget *info;
	GtkWidget *close;
	GtkWidget *tab_label;
	GtkSizeGroup *sg;

	GtkWidget *bbox;
	GtkWidget *sw;

	struct
	{
		GtkWidget *toolbar;

		GtkWidget *bold;
		GtkWidget *italic;
		GtkWidget *underline;

		GtkWidget *larger_size;
		GtkWidget *normal_size;
		GtkWidget *smaller_size;

		GtkWidget *font;
		GtkWidget *fgcolor;
		GtkWidget *bgcolor;

		GtkWidget *image;
		GtkWidget *link;
		GtkWidget *smiley;
		GtkWidget *log;

	} toolbar;

	struct
	{
		GtkWidget *fg_color;
		GtkWidget *bg_color;
		GtkWidget *font;
		GtkWidget *smiley;
		GtkWidget *link;
		GtkWidget *image;
		GtkWidget *log;

	} dialogs;

	union
	{
		GaimGtkImPane   *im;
		GaimGtkChatPane *chat;

	} u;
};

#define GAIM_GTK_WINDOW(win) \
	((GaimGtkWindow *)(win)->ui_data)

#define GAIM_GTK_CONVERSATION(conv) \
	((GaimGtkConversation *)(conv)->ui_data)

#define GAIM_IS_GTK_WINDOW(win) \
	(gaim_window_get_ui_ops(win) == gaim_get_gtk_window_ui_ops())

#define GAIM_IS_GTK_CONVERSATION(conv) \
	(gaim_conversation_get_ui_ops(conv) == gaim_get_gtk_conversation_ui_ops())

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation API
 **************************************************************************/
/*@{*/

/**
 * Initializes the GTK+ conversation system.
 */
void gaim_gtk_conversation_init(void);

/**
 * Returns the UI operations structure for GTK windows.
 *
 * @return The GTK window operations structure.
 */
GaimWindowUiOps *gaim_get_gtk_window_ui_ops(void);

/**
 * Returns the UI operations structure for GTK conversations.
 *
 * @return The GTK conversation operations structure.
 */
GaimConversationUiOps *gaim_get_gtk_conversation_ui_ops(void);

/**
 * Updates the buddy icon on a conversation.
 *
 * @param conv The conversation.
 */
void gaim_gtkconv_update_buddy_icon(GaimConversation *conv);

/**
 * Updates the font buttons on all conversations to reflect any changed
 * preferences.
 */
void gaim_gtkconv_update_font_buttons(void);

/**
 * Updates the font colors of each conversation to the new colors
 * chosen in the prefs dialog.
 *
 * @param conv The conversation to update.
 */
void gaim_gtkconv_update_font_colors(GaimConversation *conv);

/**
 * Updates the font faces of each conversation to the new font
 * face chosen in the prefs dialog.
 *
 * @param conv The conversation to update.
 */
void gaim_gtkconv_update_font_face(GaimConversation *conv);

/**
 * Updates the tab positions on all conversation windows to reflect any
 * changed preferences.
 */
void gaim_gtkconv_update_tabs(void);

/**
 * Updates the button style on chat windows to reflect any
 * changed preferences.
 */
void gaim_gtkconv_update_chat_button_style();

/**
 * Updates the button style on IM windows to reflect any
 * changed preferences.
 */
void gaim_gtkconv_update_im_button_style();

/**
 * Updates conversation buttons by protocol.
 *
 * @param conv The conversation.
 */
void gaim_gtkconv_update_buttons_by_protocol(GaimConversation *conv);

/**
 * Returns the window at the specified X, Y location.
 *
 * If the window is not a GTK+ window, @c NULL is returned.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 *
 * @return The GTK+ window at the location, if it exists, or @c NULL otherwise.
 */
GaimWindow *gaim_gtkwin_get_at_xy(int x, int y);

/**
 * Returns the index of the tab at the specified X, Y location in a notebook.
 *
 * @param win The GTK+ window containing the notebook.
 * @param x   The X coordinate.
 * @param y   The Y coordinate.
 *
 * @return The index of the tab at the location.
 */
int gaim_gtkconv_get_tab_at_xy(GaimWindow *win, int x, int y);

/**
 * Returns the index of the destination tab at the
 * specified X, Y location in a notebook.
 *
 * This is used for drag-and-drop functions when the tab at the index
 * is a destination tab.
 *
 * @param win The GTK+ window containing the notebook.
 * @param x   The X coordinate.
 * @param y   The Y coordinate.
 *
 * @return The index of the tab at the location.
 */
int gaim_gtkconv_get_dest_tab_at_xy(GaimWindow *win, int x, int y);

/*@}*/

#endif /* _GAIM_GTK_CONVERSATION_H_ */
