/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PIDGIN_CONVERSATION_WINDOW_H_
#define _PIDGIN_CONVERSATION_WINDOW_H_
/**
 * SECTION:gtkconvwin
 * @section_id: pidgin-gtkconvwin
 * @short_description: <filename>gtkconvwin.h</filename>
 * @title: Conversation Window API
 */

#define PIDGIN_TYPE_WINDOW (pidgin_window_get_type())

typedef struct _PidginWindowMenu   PidginWindowMenu;
typedef struct _PidginWindow       PidginWindow;


/**************************************************************************
 * Structures
 **************************************************************************/

struct _PidginWindowMenu
{
	GtkUIManager *ui;
	GtkWidget *menubar;

	GtkAction *view_log;

	GtkAction *audio_call;
	GtkAction *video_call;
	GtkAction *audio_video_call;

	GtkAction *send_file;
	GtkAction *get_attention;
	GtkAction *add_pounce;
	GtkAction *get_info;
	GtkAction *invite;

	GtkAction *alias;
	GtkAction *block;
	GtkAction *unblock;
	GtkAction *add;
	GtkAction *remove;

	GtkAction *insert_link;
	GtkAction *insert_image;

	GtkAction *logging;
	GtkAction *sounds;
	GtkAction *show_formatting_toolbar;

	GtkWidget *send_to;
	GtkWidget *e2ee;

	GtkWidget *tray;

	GtkWidget *typing_icon;
};

/**
 * PidginWindow:
 * @window:        The window.
 * @notebook:      The notebook of conversations.
 * @notebook_menu: The menu on the notebook.
 * @clicked_tab:   The menu currently clicked.
 *
 * A GTK+ representation of a graphical window containing one or more
 * conversations.
 */
struct _PidginWindow
{
	/*< private >*/
	gint box_count;

	/*< public >*/
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *notebook_menu;
	PidginConversation *clicked_tab;
	GList *gtkconvs;

	PidginWindowMenu *menu;

	/* Tab dragging stuff. */
	gboolean in_drag;
	gboolean in_predrag;

	gint drag_tab;

	gint drag_min_x, drag_max_x, drag_min_y, drag_max_y;

	gint drag_motion_signal;
	gint drag_leave_signal;
};

G_BEGIN_DECLS

/**************************************************************************
 * GTK+ Conversation Window API
 **************************************************************************/

/**
 * pidgin_window_get_type:
 *
 * Returns: The #GType for the #PidginWindow boxed structure.
 */
GType pidgin_window_get_type(void);

/**
 * pidgin_conv_window_new:
 *
 * Returns: A new #PidginWindow.
 */
PidginWindow * pidgin_conv_window_new(void);

/**
 * pidgin_conv_window_destroy:
 * @win: The conversation window to destroy
 */
void pidgin_conv_window_destroy(PidginWindow *win);

/**
 * pidgin_conv_windows_get_list:
 *
 * Returns: (transfer none) (element-type Pidgin.Window): The list of windows.
 */
GList *pidgin_conv_windows_get_list(void);

/**
 * pidgin_conv_window_show:
 * @win: The conversation window to show
 */
void pidgin_conv_window_show(PidginWindow *win);

/**
 * pidgin_conv_window_hide:
 * @win: The conversation window to hide
 */
void pidgin_conv_window_hide(PidginWindow *win);

/**
 * pidgin_conv_window_raise:
 * @win: The conversation window to raise
 */
void pidgin_conv_window_raise(PidginWindow *win);

/**
 * pidgin_conv_window_switch_gtkconv:
 * @win:     The conversation window
 * @gtkconv: The pidgin conversation to switch to
 */
void pidgin_conv_window_switch_gtkconv(PidginWindow *win, PidginConversation *gtkconv);

/**
 * pidgin_conv_window_add_gtkconv:
 * @win:     The conversation window
 * @gtkconv: The pidgin conversation to add
 */
void pidgin_conv_window_add_gtkconv(PidginWindow *win, PidginConversation *gtkconv);

/**
 * pidgin_conv_window_remove_gtkconv:
 * @win:     The conversation window
 * @gtkconv: The pidgin conversation to remove
 */
void pidgin_conv_window_remove_gtkconv(PidginWindow *win, PidginConversation *gtkconv);

/**
 * pidgin_conv_window_get_gtkconv_at_index:
 * @win:   The conversation window
 * @index: The index in the window to get the conversation from
 *
 * Returns: The conversation in @win at @index
 */
PidginConversation *pidgin_conv_window_get_gtkconv_at_index(const PidginWindow *win, int index);

/**
 * pidgin_conv_window_get_active_gtkconv:
 * @win: The conversation window
 *
 * Returns: The active #PidginConversation in @win.
 */
PidginConversation *pidgin_conv_window_get_active_gtkconv(const PidginWindow *win);

/**
 * pidgin_conv_window_get_active_conversation:
 * @win: The conversation window
 *
 * Returns: The active #PurpleConversation in @win.
 */
PurpleConversation *pidgin_conv_window_get_active_conversation(const PidginWindow *win);

/**
 * pidgin_conv_window_is_active_conversation:
 * @conv: The conversation
 *
 * Returns: %TRUE if @conv is the active conversation, %FALSE otherwise.
 */
gboolean pidgin_conv_window_is_active_conversation(const PurpleConversation *conv);

/**
 * pidgin_conv_window_has_focus:
 * @win: The conversation window
 *
 * Returns: %TRUE if @win has focus, %FALSE otherwise.
 */
gboolean pidgin_conv_window_has_focus(PidginWindow *win);

/**
 * pidgin_conv_window_get_at_event:
 * @event: The event
 *
 * Returns: The #PidginWindow on which @event occured.
 */
PidginWindow *pidgin_conv_window_get_at_event(GdkEvent *event);

/**
 * pidgin_conv_window_get_gtkconvs:
 * @win: The conversation window
 *
 * Returns: (transfer none) (element-type Pidgin.Conversation): A list of
 *          #PidginConversation's in @win.
 */
GList *pidgin_conv_window_get_gtkconvs(PidginWindow *win);

/**
 * pidgin_conv_window_get_gtkconv_count:
 * @win: The conversation window
 *
 * Returns: The number of conversations in @win
 */
guint pidgin_conv_window_get_gtkconv_count(PidginWindow *win);

/**
 * pidgin_conv_window_first_im:
 *
 * Returns: The window which has the first IM, %NULL if no IM is found.
 */
PidginWindow *pidgin_conv_window_first_im(void);

/**
 * pidgin_conv_window_last_im:
 *
 * Returns: The window which has the last IM, %NULL if no IM is found.
 */
PidginWindow *pidgin_conv_window_last_im(void);

/**
 * pidgin_conv_window_first_chat:
 *
 * Returns: The window which has the first chat, %NULL if no chat is found.
 */
PidginWindow *pidgin_conv_window_first_chat(void);

/**
 * pidgin_conv_window_last_chat:
 *
 * Returns: The window which has the last chat, %NULL if no chat is found.
 */
PidginWindow *pidgin_conv_window_last_chat(void);

/**************************************************************************
 * GTK+ Conversation Placement API
 **************************************************************************/

typedef void (*PidginConvPlacementFunc)(PidginConversation *);

GList *pidgin_conv_placement_get_options(void);
void pidgin_conv_placement_add_fnc(const char *id, const char *name, PidginConvPlacementFunc fnc);
void pidgin_conv_placement_remove_fnc(const char *id);
const char *pidgin_conv_placement_get_name(const char *id);
PidginConvPlacementFunc pidgin_conv_placement_get_fnc(const char *id);
void pidgin_conv_placement_set_current_func(PidginConvPlacementFunc func);
PidginConvPlacementFunc pidgin_conv_placement_get_current_func(void);
void pidgin_conv_placement_place(PidginConversation *gtkconv);

G_END_DECLS

#endif /* _PIDGIN_CONVERSATION_WINDOW_H_ */
