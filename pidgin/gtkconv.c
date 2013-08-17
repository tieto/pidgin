/**
 * @file gtkconv.c GTK+ Conversation API
 * @ingroup pidgin
 */

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
 *
 */
#define _PIDGIN_GTKCONV_C_

#include "internal.h"
#include "pidgin.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "cmds.h"
#include "core.h"
#include "debug.h"
#include "idle.h"
#include "imgstore.h"
#include "log.h"
#include "notify.h"
#include "plugins.h"
#include "protocol.h"
#include "request.h"
#include "theme-loader.h"
#include "theme-manager.h"
#include "util.h"
#include "version.h"

#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconvwin.h"
#include "gtkconv-theme.h"
#include "gtkconv-theme-loader.h"
#include "gtkdialogs.h"
#include "gtklog.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"
#include "pidgintooltip.h"
#include "smileyparser.h"

#include "gtknickcolors.h"

#define GTK_TOOLTIPS_VAR gtkconv->tooltips
#include "gtk3compat.h"

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
	guint32 typing_timer;

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

#define CLOSE_CONV_TIMEOUT_SECS  (10 * 60)

#define AUTO_RESPONSE "&lt;AUTO-REPLY&gt; : "

typedef enum
{
	PIDGIN_CONV_SET_TITLE			= 1 << 0,
	PIDGIN_CONV_BUDDY_ICON			= 1 << 1,
	PIDGIN_CONV_MENU			= 1 << 2,
	PIDGIN_CONV_TAB_ICON			= 1 << 3,
	PIDGIN_CONV_TOPIC			= 1 << 4,
	PIDGIN_CONV_SMILEY_THEME		= 1 << 5,
	PIDGIN_CONV_COLORIZE_TITLE		= 1 << 6
}PidginConvFields;

enum {
	CONV_ICON_COLUMN,
	CONV_TEXT_COLUMN,
	CONV_EMBLEM_COLUMN,
	CONV_PROTOCOL_ICON_COLUMN,
	CONV_NUM_COLUMNS
} PidginInfopaneColumns;

#define	PIDGIN_CONV_ALL	((1 << 7) - 1)

/* XXX: These color defines shouldn't really be here. But the nick-color
 * generation algorithm uses them, so keeping these around until we fix that. */
#define DEFAULT_SEND_COLOR "#204a87"
#define DEFAULT_HIGHLIGHT_COLOR "#AF7F00"

#define BUDDYICON_SIZE_MIN    32
#define BUDDYICON_SIZE_MAX    96

/* Undef this to turn off "custom-smiley" debug messages */
#define DEBUG_CUSTOM_SMILEY

#define LUMINANCE(c) (float)((0.3*(c.red))+(0.59*(c.green))+(0.11*(c.blue)))

/* From http://www.w3.org/TR/AERT#color-contrast */
#define MIN_BRIGHTNESS_CONTRAST 75
#define MIN_COLOR_CONTRAST 200

#define NICK_COLOR_GENERATE_COUNT 220
static GArray *generated_nick_colors = NULL;

/* These probably won't conflict with any WebKit values. */
#define PIDGIN_DRAG_BLIST_NODE (1337)
#define PIDGIN_DRAG_IM_CONTACT (31337)

static const GtkTargetEntry dnd_targets[] =
{
	{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, PIDGIN_DRAG_BLIST_NODE},
	{"application/x-im-contact", 0, PIDGIN_DRAG_IM_CONTACT}
};

static GtkTargetList *webkit_dnd_targets = NULL;

typedef struct {
	GtkWidget *window;

	GtkWidget *entry;
	GtkWidget *message;

	PurpleChatConversation *chat;

} InviteBuddyInfo;

static GtkWidget *invite_dialog = NULL;
static GtkWidget *warn_close_dialog = NULL;

static PidginWindow *hidden_convwin = NULL;
static GList *window_list = NULL;

/* Lists of status icons at all available sizes for use as window icons */
static GList *available_list = NULL;
static GList *away_list = NULL;
static GList *busy_list = NULL;
static GList *xa_list = NULL;
static GList *offline_list = NULL;
static GHashTable *protocol_lists = NULL;

static PurpleTheme *default_conv_theme = NULL;

static gboolean update_send_to_selection(PidginWindow *win);
static void generate_send_to_items(PidginWindow *win);

/* Prototypes. <-- because Paco-Paco hates this comment. */
static void load_conv_theme(PidginConversation *gtkconv);
static gboolean infopane_entry_activate(PidginConversation *gtkconv);
#if 0
static void got_typing_keypress(PidginConversation *gtkconv, gboolean first);
#endif
static void gray_stuff_out(PidginConversation *gtkconv);
static void add_chat_user_common(PurpleChatConversation *chat, PurpleChatUser *cb, const char *old_name);
static gboolean tab_complete(PurpleConversation *conv);
static void pidgin_conv_updated(PurpleConversation *conv, PurpleConversationUpdateType type);
static void conv_set_unseen(PurpleConversation *gtkconv, PidginUnseenState state);
static void gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state);
static void update_typing_icon(PidginConversation *gtkconv);
static void update_typing_message(PidginConversation *gtkconv, const char *message);
gboolean pidgin_conv_has_focus(PurpleConversation *conv);
static GArray* generate_nick_colors(guint numcolors, GdkColor background);
static gboolean color_is_visible(GdkColor foreground, GdkColor background, int color_contrast, int brightness_contrast);
static GtkTextTag *get_buddy_tag(PurpleChatConversation *chat, const char *who, PurpleMessageFlags flag, gboolean create);
static void pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields);
static void focus_out_from_menubar(GtkWidget *wid, PidginWindow *win);
static void pidgin_conv_tab_pack(PidginWindow *win, PidginConversation *gtkconv);
static gboolean infopane_press_cb(GtkWidget *widget, GdkEventButton *e, PidginConversation *conv);
static void hide_conv(PidginConversation *gtkconv, gboolean closetimer);

static void pidgin_conv_set_position_size(PidginWindow *win, int x, int y,
		int width, int height);
static gboolean pidgin_conv_xy_to_right_infopane(PidginWindow *win, int x, int y);

static const GdkColor *get_nick_color(PidginConversation *gtkconv, const char *name)
{
	static GdkColor col;
	GtkStyle *style = gtk_widget_get_style(gtkconv->webview);
	float scale;

	col = g_array_index(gtkconv->nick_colors, GdkColor, g_str_hash(name) % gtkconv->nick_colors->len);
	scale = ((1-(LUMINANCE(style->base[GTK_STATE_NORMAL]) / LUMINANCE(style->white))) *
		       (LUMINANCE(style->white)/MAX(MAX(col.red, col.blue), col.green)));

	/* The colors are chosen to look fine on white; we should never have to darken */
	if (scale > 1) {
		col.red   *= scale;
		col.green *= scale;
		col.blue  *= scale;
	}

	return &col;
}

static PurpleBlistNode *
get_conversation_blist_node(PurpleConversation *conv)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleBlistNode *node = NULL;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		node = PURPLE_BLIST_NODE(purple_blist_find_buddy(account, purple_conversation_get_name(conv)));
		node = node ? node->parent : NULL;
	} else {
		node = PURPLE_BLIST_NODE(purple_blist_find_chat(account, purple_conversation_get_name(conv)));
	}

	return node;
}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gboolean
close_this_sucker(gpointer data)
{
	PidginConversation *gtkconv = data;
	GList *list = g_list_copy(gtkconv->convs);
	g_list_foreach(list, (GFunc)g_object_unref, NULL);
	g_list_free(list);
	return FALSE;
}

static gboolean
close_conv_cb(GtkButton *button, PidginConversation *gtkconv)
{
	/* We are going to destroy the conversations immediately only if the 'close immediately'
	 * preference is selected. Otherwise, close the conversation after a reasonable timeout
	 * (I am going to consider 10 minutes as a 'reasonable timeout' here.
	 * For chats, close immediately if the chat is not in the buddylist, or if the chat is
	 * not marked 'Persistent' */
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account = purple_conversation_get_account(conv);
	const char *name = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately"))
			close_this_sucker(gtkconv);
		else
			hide_conv(gtkconv, TRUE);
	} else {
		PurpleChat *chat = purple_blist_find_chat(account, name);
		if (!chat ||
				!purple_blist_node_get_bool(&chat->node, "gtk-persistent"))
			close_this_sucker(gtkconv);
		else
			hide_conv(gtkconv, FALSE);
	}

	return TRUE;
}

static gboolean
lbox_size_allocate_cb(GtkWidget *w, GtkAllocation *allocation, gpointer data)
{
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", allocation->width == 1 ? 0 : allocation->width);

	return FALSE;
}

static void
default_formatize(PidginConversation *c)
{
	PurpleConversation *conv = c->active_conv;
	gtk_webview_setup_entry(GTK_WEBVIEW(c->entry), purple_conversation_get_features(conv));
}

static void
conversation_entry_clear(PidginConversation *gtkconv)
{
	GtkWebView *webview = GTK_WEBVIEW(gtkconv->entry);

	//XXX: hotfix for not focused entry after sending a message
	//gtk_webview_load_html_string(webview, "");
	gtk_webview_load_html_string_with_selection(webview, "<div id='caret'></div>");

#if 0
	/* TODO WebKit */
	gtk_source_undo_manager_begin_not_undoable_action(webview->undo_manager);
	gtk_webview_clear(webview);
	gtk_source_undo_manager_end_not_undoable_action(webview->undo_manager);
#endif
}

static void
clear_formatting_cb(GtkWebView *webview, PidginConversation *gtkconv)
{
	default_formatize(gtkconv);
}

static const char *
pidgin_get_cmd_prefix(void)
{
	return "/";
}

static PurpleCmdRet
say_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	purple_conversation_send(conv, args[0]);

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
me_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	char *tmp;

	tmp = g_strdup_printf("/me %s", args[0]);
	purple_conversation_send(conv, tmp);

	g_free(tmp);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
debug_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("Using Pidgin v%s with libpurple v%s.",
				DISPLAY_VERSION, purple_core_get_version());
	} else if (!g_ascii_strcasecmp(args[0], "plugins")) {
		/* Show all the loaded plugins, including the protocol plugins and plugin loaders.
		 * This is intentional, since third party prpls are often sources of bugs, and some
		 * plugin loaders (e.g. mono) can also be buggy.
		 */
		GString *str = g_string_new("Loaded Plugins: ");
		const GList *plugins = purple_plugins_get_loaded();
		if (plugins) {
			for (; plugins; plugins = plugins->next) {
				PurplePluginInfo *info = purple_plugin_get_info(PURPLE_PLUGIN(plugins->data));
				str = g_string_append(str, purple_plugin_info_get_name(info));

				if (plugins->next)
					str = g_string_append(str, ", ");
			}
		} else {
			str = g_string_append(str, "(none)");
		}

		tmp = g_string_free(str, FALSE);
	} else if (!g_ascii_strcasecmp(args[0], "unsafe")) {
		if (purple_debug_is_unsafe()) {
			purple_debug_set_unsafe(FALSE);
			purple_conversation_write(conv, NULL, _("Unsafe debugging is now disabled."),
			                          PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_SYSTEM, time(NULL));
		} else {
			purple_debug_set_unsafe(TRUE);
			purple_conversation_write(conv, NULL, _("Unsafe debugging is now enabled."),
			                          PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_SYSTEM, time(NULL));
		}

		return PURPLE_CMD_RET_OK;
	} else if (!g_ascii_strcasecmp(args[0], "verbose")) {
		if (purple_debug_is_verbose()) {
			purple_debug_set_verbose(FALSE);
			purple_conversation_write(conv, NULL, _("Verbose debugging is now disabled."),
			                          PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_SYSTEM, time(NULL));
		} else {
			purple_debug_set_verbose(TRUE);
			purple_conversation_write(conv, NULL, _("Verbose debugging is now enabled."),
			                          PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_SYSTEM, time(NULL));
		}

		return PURPLE_CMD_RET_OK;
	} else {
		purple_conversation_write(conv, NULL, _("Supported debug options are: plugins version unsafe verbose"),
		                        PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_ERROR, time(NULL));
		return PURPLE_CMD_RET_OK;
	}

	markup = g_markup_escape_text(tmp, -1);
	purple_conversation_send(conv, markup);

	g_free(tmp);
	g_free(markup);
	return PURPLE_CMD_RET_OK;
}

static void clear_conversation_scrollback_cb(PurpleConversation *conv,
                                             void *data)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	if (PIDGIN_CONVERSATION(conv)) {
		load_conv_theme(gtkconv);
		gtkconv->last_flags = 0;
	}
}

static PurpleCmdRet
clear_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	purple_conversation_clear_message_history(conv);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
clearall_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l;
	for (l = purple_conversations_get_all(); l != NULL; l = l->next)
		purple_conversation_clear_message_history(PURPLE_CONVERSATION(l->data));

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
help_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l, *text;
	GString *s;

	if (args[0] != NULL) {
		s = g_string_new("");
		text = purple_cmd_help(conv, args[0]);

		if (text) {
			for (l = text; l; l = l->next)
				if (l->next)
					g_string_append_printf(s, "%s\n", (char *)l->data);
				else
					g_string_append_printf(s, "%s", (char *)l->data);
		} else {
			g_string_append(s, _("No such command (in this context)."));
		}
	} else {
		s = g_string_new(_("Use \"/help &lt;command&gt;\" for help on a specific command.<br/>"
		                   "The following commands are available in this context:<br/>"));

		text = purple_cmd_list(conv);
		for (l = text; l; l = l->next)
			if (l->next)
				g_string_append_printf(s, "%s, ", (char *)l->data);
			else
				g_string_append_printf(s, "%s.", (char *)l->data);
		g_list_free(text);
	}

	purple_conversation_write(conv, NULL, s->str, PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_string_free(s, TRUE);

	return PURPLE_CMD_RET_OK;
}

static void
send_history_add(PidginConversation *gtkconv, const char *message)
{
	GList *first;

	first = g_list_first(gtkconv->send_history);
	g_free(first->data);
	first->data = g_strdup(message);
	gtkconv->send_history = g_list_prepend(first, NULL);
}

static gboolean
check_for_and_do_command(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	char *cmd;
	const char *prefix;
	gboolean retval = FALSE;

	gtkconv = PIDGIN_CONVERSATION(conv);
	prefix = pidgin_get_cmd_prefix();

	cmd = gtk_webview_get_body_text(GTK_WEBVIEW(gtkconv->entry));

	if (cmd && purple_str_has_prefix(cmd, prefix)) {
		PurpleCmdStatus status;
		char *error, *cmdline, *markup, *send_history;

		send_history = gtk_webview_get_body_html(GTK_WEBVIEW(gtkconv->entry));
		send_history_add(gtkconv, send_history);
		g_free(send_history);

		cmdline = cmd + strlen(prefix);

		if (strcmp(cmdline, "xyzzy") == 0) {
			purple_conversation_write(conv, "", "Nothing happens",
					PURPLE_MESSAGE_NO_LOG, time(NULL));
			g_free(cmd);
			return TRUE;
		}

		/* TODO WebKit: Cut out prefix for markup... */
		markup = gtk_webview_get_body_html(GTK_WEBVIEW(gtkconv->entry));
		status = purple_cmd_do_command(conv, cmdline, markup, &error);
		g_free(markup);

		switch (status) {
			case PURPLE_CMD_STATUS_OK:
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_NOT_FOUND:
				{
					PurpleProtocol *protocol = NULL;
					PurpleConnection *gc;

					if ((gc = purple_conversation_get_connection(conv)))
						protocol = purple_connection_get_protocol_info(gc);

					if ((protocol != NULL) && (protocol->options & OPT_PROTO_SLASH_COMMANDS_NATIVE)) {
						char *spaceslash;

						/* If the first word in the entered text has a '/' in it, then the user
						 * probably didn't mean it as a command. So send the text as message. */
						spaceslash = cmdline;
						while (*spaceslash && *spaceslash != ' ' && *spaceslash != '/')
							spaceslash++;

						if (*spaceslash != '/') {
							purple_conversation_write(conv, "", _("Unknown command."), PURPLE_MESSAGE_NO_LOG, time(NULL));
							retval = TRUE;
						}
					}
					break;
				}
			case PURPLE_CMD_STATUS_WRONG_ARGS:
				purple_conversation_write(conv, "", _("Syntax Error:  You typed the wrong number of arguments "
								    "to that command."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_FAILED:
				purple_conversation_write(conv, "", error ? error : _("Your command failed for an unknown reason."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				g_free(error);
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_WRONG_TYPE:
				if(PURPLE_IS_IM_CONVERSATION(conv))
					purple_conversation_write(conv, "", _("That command only works in chats, not IMs."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				else
					purple_conversation_write(conv, "", _("That command only works in IMs, not chats."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_WRONG_PROTOCOL:
				purple_conversation_write(conv, "", _("That command doesn't work on this protocol."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				retval = TRUE;
				break;
		}
	}

	g_free(cmd);

	return retval;
}

static void
send_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleMessageFlags flags = 0;
	char *buf, *clean;

	account = purple_conversation_get_account(conv);

	if (check_for_and_do_command(conv)) {
		conversation_entry_clear(gtkconv);
		return;
	}

	if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
		purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv)))
		return;

	if (!purple_account_is_connected(account))
		return;

	buf = gtk_webview_get_body_html(GTK_WEBVIEW(gtkconv->entry));
	clean = gtk_webview_get_body_text(GTK_WEBVIEW(gtkconv->entry));

	gtk_widget_grab_focus(gtkconv->entry);

	if (!*clean) {
		g_free(buf);
		g_free(clean);
		return;
	}

	purple_idle_touch();

#if 0
	/* TODO WebKit: Image stuff... */
	/* XXX: is there a better way to tell if the message has images? */
	if (GTK_WEBVIEW(gtkconv->entry)->im_images != NULL)
		flags |= PURPLE_MESSAGE_IMAGES;
#endif

	gc = NULL/*purple_account_get_connection(account)*/;
	if (gc && (purple_conversation_get_features(conv) & PURPLE_CONNECTION_FLAG_NO_NEWLINES)) {
#if 0
		/* TODO WebKit */
		char **bufs;
		int i;

		bufs = gtk_webview_get_markup_lines(GTK_WEBVIEW(gtkconv->entry));
		for (i = 0; bufs[i]; i++) {
			send_history_add(gtkconv, bufs[i]);
			purple_conversation_send_with_flags(conv, bufs[i], flags);
		}

		g_strfreev(bufs);
#endif

	} else {
		send_history_add(gtkconv, buf);
		purple_conversation_send_with_flags(conv, buf, flags);
	}

	g_free(clean);
	g_free(buf);

	conversation_entry_clear(gtkconv);
	gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
	gtk_widget_grab_focus(gtkconv->entry); // XXX: doesn't work
}

static void
add_remove_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleAccount *account;
	const char *name;
	PurpleConversation *conv = gtkconv->active_conv;

	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b;

		b = purple_blist_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_remove_buddy(b);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_buddy(account, (char *)name, NULL, NULL);
	} else {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_remove_chat(c);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_chat(account, NULL, NULL, name);
	}

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static void chat_do_info(PidginConversation *gtkconv, const char *who)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);
	PurpleConnection *gc;

	if ((gc = purple_conversation_get_connection(gtkconv->active_conv))) {
		pidgin_retrieve_user_info_in_chat(gc, who, purple_chat_conversation_get_id(chat));
	}
}


static void
info_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		pidgin_retrieve_user_info(purple_conversation_get_connection(conv),
					  purple_conversation_get_name(conv));
		gtk_widget_grab_focus(gtkconv->entry);
	} else {
		/* Get info of the person currently selected in the GtkTreeView */
		PidginChatPane *gtkchat;
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreeSelection *sel;
		char *name;

		gtkchat = gtkconv->u.chat;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

		if (gtk_tree_selection_get_selected(sel, NULL, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &name, -1);
		else
			return;

		chat_do_info(gtkconv, name);
		g_free(name);
	}
}

static void
block_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_block(account, purple_conversation_get_name(conv));

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static void
unblock_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_permit(account, purple_conversation_get_name(conv));

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static gboolean
chat_invite_filter(const PidginBuddyCompletionEntry *entry, gpointer data)
{
	PurpleAccount *filter_account = data;
	PurpleAccount *account = NULL;

	if (entry->is_buddy) {
		if (PURPLE_BUDDY_IS_ONLINE(entry->entry.buddy))
			account = purple_buddy_get_account(entry->entry.buddy);
		else
			return FALSE;
	} else {
		account = entry->entry.logged_buddy->account;
	}
	if (account == filter_account)
		return TRUE;
	return FALSE;
}

static void
do_invite(GtkWidget *w, int resp, InviteBuddyInfo *info)
{
	const char *buddy, *message;
	PurpleChatConversation *chat;

	chat = info->chat;

	if (resp == GTK_RESPONSE_OK) {
		buddy   = gtk_entry_get_text(GTK_ENTRY(info->entry));
		message = gtk_entry_get_text(GTK_ENTRY(info->message));

		if (!g_ascii_strcasecmp(buddy, ""))
			return;

		serv_chat_invite(purple_conversation_get_connection(PURPLE_CONVERSATION(chat)),
						 purple_chat_conversation_get_id(chat),
						 message, buddy);
	}

	gtk_widget_destroy(invite_dialog);
	invite_dialog = NULL;

	g_free(info);
}

static void
invite_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
				GtkSelectionData *sd, guint dnd_info, guint t, gpointer data)
{
	InviteBuddyInfo *info = (InviteBuddyInfo *)data;
	const char *convprotocol;
	gboolean success = TRUE;

	convprotocol = purple_account_get_protocol_id(
			purple_conversation_get_account(PURPLE_CONVERSATION(info->chat)));

	if (dnd_info == PIDGIN_DRAG_BLIST_NODE)
	{
		PurpleBlistNode *node = NULL;
		PurpleBuddy *buddy;
		const guchar *data = gtk_selection_data_get_data(sd);

		memcpy(&node, data, sizeof(node));

		if (PURPLE_IS_CONTACT(node))
			buddy = purple_contact_get_priority_buddy((PurpleContact *)node);
		else if (PURPLE_IS_BUDDY(node))
			buddy = (PurpleBuddy *)node;
		else
			return;

		if (strcmp(convprotocol, purple_account_get_protocol_id(purple_buddy_get_account(buddy))))
		{
			purple_notify_error(PIDGIN_CONVERSATION(PURPLE_CONVERSATION(info->chat)), NULL,
							  _("That buddy is not on the same protocol as this "
								"chat."), NULL);
			success = FALSE;
		}
		else
			gtk_entry_set_text(GTK_ENTRY(info->entry), purple_buddy_get_name(buddy));

		gtk_drag_finish(dc, success,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
	else if (dnd_info == PIDGIN_DRAG_IM_CONTACT)
	{
		char *protocol = NULL;
		char *username = NULL;
		PurpleAccount *account;

		if (pidgin_parse_x_im_contact((const char *) data, FALSE, &account,
										&protocol, &username, NULL))
		{
			if (account == NULL)
			{
				purple_notify_error(PIDGIN_CONVERSATION(PURPLE_CONVERSATION(info->chat)), NULL,
					_("You are not currently signed on with an account that "
					  "can invite that buddy."), NULL);
			}
			else if (strcmp(convprotocol, purple_account_get_protocol_id(account)))
			{
				purple_notify_error(PIDGIN_CONVERSATION(PURPLE_CONVERSATION(info->chat)), NULL,
								  _("That buddy is not on the same protocol as this "
									"chat."), NULL);
				success = FALSE;
			}
			else
			{
				gtk_entry_set_text(GTK_ENTRY(info->entry), username);
			}
		}

		g_free(username);
		g_free(protocol);

		gtk_drag_finish(dc, success,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
}

static void
invite_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);
	InviteBuddyInfo *info = NULL;

	if (invite_dialog == NULL) {
		PidginWindow *gtkwin;
		GtkWidget *label;
		GtkWidget *vbox, *hbox;
		GtkWidget *table;
		GtkWidget *img;

		img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
		                               gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));

		info = g_new0(InviteBuddyInfo, 1);
		info->chat = chat;

		gtkwin    = pidgin_conv_get_window(gtkconv);

		/* Create the new dialog. */
		invite_dialog = gtk_dialog_new_with_buttons(
			_("Invite Buddy Into Chat Room"),
			GTK_WINDOW(gtkwin->window), 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			PIDGIN_STOCK_INVITE, GTK_RESPONSE_OK, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(invite_dialog),
		                                GTK_RESPONSE_OK);
		gtk_container_set_border_width(GTK_CONTAINER(invite_dialog), PIDGIN_HIG_BOX_SPACE);
		gtk_window_set_resizable(GTK_WINDOW(invite_dialog), FALSE);

		info->window = GTK_WIDGET(invite_dialog);

		/* Setup the outside spacing. */
		vbox = gtk_dialog_get_content_area(GTK_DIALOG(invite_dialog));

		gtk_box_set_spacing(GTK_BOX(vbox), PIDGIN_HIG_BORDER);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), PIDGIN_HIG_BOX_SPACE);

		/* Setup the inner hbox and put the dialog's icon in it. */
		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		/* Setup the right vbox. */
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		/* Put our happy label in it. */
		label = gtk_label_new(_("Please enter the name of the user you wish "
								"to invite, along with an optional invite "
								"message."));
		gtk_widget_set_size_request(label, 350, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		/* hbox for the table, and to give it some spacing on the left. */
		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		/* Setup the table we're going to use to lay stuff out. */
		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);
		gtk_table_set_col_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);
		gtk_container_set_border_width(GTK_CONTAINER(table), PIDGIN_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		/* Now the Buddy label */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Buddy:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		/* Now the Buddy drop-down entry field. */
		info->entry = gtk_entry_new();
		pidgin_setup_screenname_autocomplete(info->entry, NULL, chat_invite_filter,
				purple_conversation_get_account(PURPLE_CONVERSATION(chat)));
		gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->entry);

		/* Now the label for "Message" */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Message:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


		/* And finally, the Message entry field. */
		info->message = gtk_entry_new();
		gtk_entry_set_activates_default(GTK_ENTRY(info->message), TRUE);

		gtk_table_attach_defaults(GTK_TABLE(table), info->message, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->message);

		/* Connect the signals. */
		g_signal_connect(G_OBJECT(invite_dialog), "response",
						 G_CALLBACK(do_invite), info);
		/* Setup drag-and-drop */
		gtk_drag_dest_set(info->window,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  dnd_targets,
						  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);
		gtk_drag_dest_set(info->entry,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  dnd_targets,
						  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);

		g_signal_connect(G_OBJECT(info->window), "drag_data_received",
						 G_CALLBACK(invite_dnd_recv), info);
		g_signal_connect(G_OBJECT(info->entry), "drag_data_received",
						 G_CALLBACK(invite_dnd_recv), info);
	}

	gtk_widget_show_all(invite_dialog);

	if (info != NULL)
		gtk_widget_grab_focus(info->entry);
}

static void
menu_new_conv_cb(GtkAction *action, gpointer data)
{
	pidgin_dialogs_im();
}

static void
menu_join_chat_cb(GtkAction *action, gpointer data)
{
	pidgin_blist_joinchat_show();
}

static void
savelog_writefile_cb(void *user_data, const char *filename)
{
	PurpleConversation *conv = (PurpleConversation *)user_data;
	GtkWebView *webview;
	FILE *fp;
	const char *name;
	gchar *text;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(PIDGIN_CONVERSATION(conv), NULL, _("Unable to open file."), NULL);
		return;
	}

	webview = GTK_WEBVIEW(PIDGIN_CONVERSATION(conv)->webview);
	name = purple_conversation_get_name(conv);
	fprintf(fp, "<html>\n");

	fprintf(fp, "<head>\n");
	fprintf(fp, "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n");
	fprintf(fp, "<title>%s</title>\n", name);
	text = gtk_webview_get_head_html(webview);
	fprintf(fp, "%s", text);
	g_free(text);
	fprintf(fp, "</head>\n");

	fprintf(fp, "<body>\n");
	fprintf(fp, _("<h1>Conversation with %s</h1>\n"), name);
	text = gtk_webview_get_body_html(webview);
	fprintf(fp, "%s", text);
	g_free(text);
	fprintf(fp, "\n</body>\n");

	fprintf(fp, "</html>\n");
	fclose(fp);
}

/*
 * It would be kinda cool if this gave the option of saving a
 * plaintext v. HTML file.
 */
static void
menu_save_as_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleBuddy *buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
	const char *name;
	gchar *buf;
	gchar *c;

	if (buddy != NULL)
		name = purple_buddy_get_contact_alias(buddy);
	else
		name = purple_normalize(account, purple_conversation_get_name(conv));

	buf = g_strdup_printf("%s.html", name);
	for (c = buf ; *c ; c++)
	{
		if (*c == '/' || *c == '\\')
			*c = ' ';
	}
	purple_request_file(PIDGIN_CONVERSATION(conv), _("Save Conversation"),
					  buf,
					  TRUE, G_CALLBACK(savelog_writefile_cb), NULL,
					  NULL, NULL, conv,
					  conv);

	g_free(buf);
}

static void
menu_view_log_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PurpleLogType type;
	PidginBuddyList *gtkblist;
	const char *name;
	PurpleAccount *account;
	GSList *buddies;
	GSList *cur;

	conv = pidgin_conv_window_get_active_conversation(win);
	type = PURPLE_IS_IM_CONVERSATION(conv) ? PURPLE_LOG_IM : PURPLE_LOG_CHAT;

	gtkblist = pidgin_blist_get_default_gtk_blist();

	pidgin_set_cursor(gtkblist->window, GDK_WATCH);
	pidgin_set_cursor(win->window, GDK_WATCH);

	name = purple_conversation_get_name(conv);
	account = purple_conversation_get_account(conv);

	buddies = purple_blist_find_buddies(account, name);
	for (cur = buddies; cur != NULL; cur = cur->next)
	{
		PurpleBlistNode *node = cur->data;
		if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
		{
			pidgin_log_show_contact((PurpleContact *)node->parent);
			g_slist_free(buddies);
			pidgin_clear_cursor(gtkblist->window);
			pidgin_clear_cursor(win->window);
			return;
		}
	}
	g_slist_free(buddies);

	pidgin_log_show(type, name, account);

	pidgin_clear_cursor(gtkblist->window);
	pidgin_clear_cursor(win->window);
}

static void
menu_clear_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);
	purple_conversation_clear_message_history(conv);
}

static void
menu_find_cb(GtkAction *action, gpointer data)
{
	PidginWindow *gtkwin = data;
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(gtkwin);
	gtk_widget_show_all(gtkconv->quickfind.container);
	gtk_widget_grab_focus(gtkconv->quickfind.entry);
}

#ifdef USE_VV
static void
menu_initiate_media_call_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = (PidginWindow *)data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);
	PurpleAccount *account = purple_conversation_get_account(conv);

	purple_protocol_initiate_media(account,
			purple_conversation_get_name(conv),
			action == win->menu.audio_call ? PURPLE_MEDIA_AUDIO :
			action == win->menu.video_call ? PURPLE_MEDIA_VIDEO :
			action == win->menu.audio_video_call ? PURPLE_MEDIA_AUDIO |
			PURPLE_MEDIA_VIDEO : PURPLE_MEDIA_NONE);
}
#endif

static void
menu_send_file_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		serv_send_file(purple_conversation_get_connection(conv), purple_conversation_get_name(conv), NULL);
	}

}

static void
menu_get_attention_cb(GObject *obj, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		int index;
		if ((GtkAction *)obj == win->menu.get_attention)
			index = 0;
		else
			index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(obj), "index"));
		purple_protocol_send_attention(purple_conversation_get_connection(conv),
			purple_conversation_get_name(conv), index);
	}
}

static void
menu_add_pounce_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_gtkconv(win)->active_conv;

	pidgin_pounce_editor_show(purple_conversation_get_account(conv),
								purple_conversation_get_name(conv), NULL);
}

static void
menu_insert_link_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PidginConversation *gtkconv;
	GtkWebView *entry;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	entry = GTK_WEBVIEW(gtkconv->entry);

	gtk_webview_activate_toolbar(entry, GTK_WEBVIEW_ACTION_LINK);
}

static void
menu_insert_image_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PidginConversation *gtkconv;
	GtkWebView *entry;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	entry = GTK_WEBVIEW(gtkconv->entry);

	gtk_webview_activate_toolbar(entry, GTK_WEBVIEW_ACTION_IMAGE);
}

static void
menu_alias_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PurpleAccount *account;
	const char *name;

	conv    = pidgin_conv_window_get_active_conversation(win);
	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b;

		b = purple_blist_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_alias_buddy(b);
	} else {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_alias_chat(c);
	}
}

static void
menu_get_info_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	info_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_invite_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	invite_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_block_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	block_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_unblock_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	unblock_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_add_remove_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	add_remove_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static gboolean
close_already(gpointer data)
{
	g_object_unref(data);
	return FALSE;
}

static void
hide_conv(PidginConversation *gtkconv, gboolean closetimer)
{
	GList *list;

	purple_signal_emit(pidgin_conversations_get_handle(),
			"conversation-hiding", gtkconv);

	for (list = g_list_copy(gtkconv->convs); list; list = g_list_delete_link(list, list)) {
		PurpleConversation *conv = list->data;
		if (closetimer) {
			guint timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
			if (timer)
				purple_timeout_remove(timer);
			timer = purple_timeout_add_seconds(CLOSE_CONV_TIMEOUT_SECS, close_already, conv);
			g_object_set_data(G_OBJECT(conv), "close-timer", GINT_TO_POINTER(timer));
		}
#if 0
		/* I will miss you */
		purple_conversation_set_ui_ops(conv, NULL);
#else
		pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
#endif
	}
}

static void
menu_close_conv_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;

	close_conv_cb(NULL, PIDGIN_CONVERSATION(pidgin_conv_window_get_active_conversation(win)));
}

static void
menu_logging_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	gboolean logging;
	PurpleBlistNode *node;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return;

	logging = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));

	if (logging == purple_conversation_is_logging(conv))
		return;

	node = get_conversation_blist_node(conv);

	if (logging)
	{
		/* Enable logging first so the message below can be logged. */
		purple_conversation_set_logging(conv, TRUE);

		purple_conversation_write(conv, NULL,
								_("Logging started. Future messages in this conversation will be logged."),
								PURPLE_MESSAGE_SYSTEM,
								time(NULL));
	}
	else
	{
		purple_conversation_write(conv, NULL,
								_("Logging stopped. Future messages in this conversation will not be logged."),
								PURPLE_MESSAGE_SYSTEM,
								time(NULL));

		/* Disable the logging second, so that the above message can be logged. */
		purple_conversation_set_logging(conv, FALSE);
	}

	/* Save the setting IFF it's different than the pref. */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (logging == purple_prefs_get_bool("/purple/logging/log_ims"))
			purple_blist_node_remove_setting(node, "enable-logging");
		else
			purple_blist_node_set_bool(node, "enable-logging", logging);
	} else {
		if (logging == purple_prefs_get_bool("/purple/logging/log_chats"))
			purple_blist_node_remove_setting(node, "enable-logging");
		else
			purple_blist_node_set_bool(node, "enable-logging", logging);
	}
}

static void
menu_toolbar_cb(GtkAction *action, gpointer data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
	                    gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)));
}

static void
menu_sounds_cb(GtkAction *action, gpointer data)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PurpleBlistNode *node;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->make_sound =
		gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
	node = get_conversation_blist_node(conv);
	if (node)
		purple_blist_node_set_bool(node, "gtk-mute-sound", !gtkconv->make_sound);
}

static void
chat_do_im(PidginConversation *gtkconv, const char *who)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;
	gchar *real_who = NULL;

	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	protocol = purple_connection_get_protocol_info(gc);

	if (protocol && protocol->get_cb_real_name)
		real_who = protocol->get_cb_real_name(gc,
				purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)), who);

	if(!who && !real_who)
		return;

	pidgin_dialogs_im_with_user(account, real_who ? real_who : who);

	g_free(real_who);
}

static void pidgin_conv_chat_update_user(PurpleChatUser *chatuser);

static void
ignore_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);
	const char *name;

	name = g_object_get_data(G_OBJECT(w), "user_data");

	if (name == NULL)
		return;

	if (purple_chat_conversation_is_ignored_user(chat, name))
		purple_chat_conversation_unignore(chat, name);
	else
		purple_chat_conversation_ignore(chat, name);

	pidgin_conv_chat_update_user(purple_chat_conversation_find_user(chat, name));
}

static void
menu_chat_im_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_im(gtkconv, who);
}

static void
menu_chat_send_file_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleProtocol *protocol;
	PurpleConversation *conv = gtkconv->active_conv;
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");
	PurpleConnection *gc  = purple_conversation_get_connection(conv);
	gchar *real_who = NULL;

	g_return_if_fail(gc != NULL);

	protocol = purple_connection_get_protocol_info(gc);

	if (protocol && protocol->get_cb_real_name)
		real_who = protocol->get_cb_real_name(gc,
				purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)), who);

	serv_send_file(gc, real_who ? real_who : who, NULL);
	g_free(real_who);
}

static void
menu_chat_info_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	char *who;

	who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_info(gtkconv, who);
}

static void
menu_chat_add_remove_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleBuddy *b;
	char *name;

	account = purple_conversation_get_account(conv);
	name    = g_object_get_data(G_OBJECT(w), "user_data");
	b       = purple_blist_find_buddy(account, name);

	if (b != NULL)
		pidgin_dialogs_remove_buddy(b);
	else if (account != NULL && purple_account_is_connected(account))
		purple_blist_request_add_buddy(account, name, NULL, NULL);

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static char *
get_class_for_user(const char *who)
{
	return g_strconcat("-pidgin-user:", who, NULL);
}

static WebKitDOMNode *
get_mark_for_user(PidginConversation *gtkconv, const char *who)
{
	WebKitDOMDocument *doc;
	WebKitDOMNodeList *nodes;
	WebKitDOMNode *node = NULL;
	gulong len;
	char *tmp;

	doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(gtkconv->webview));

	tmp = get_class_for_user(who);
	nodes = webkit_dom_document_get_elements_by_class_name(doc, tmp);
	g_free(tmp);

	if (nodes != NULL) {
		len = webkit_dom_node_list_get_length(nodes);
		if (len > 0)
			node = webkit_dom_node_list_item(nodes, len - 1);
	}

	g_object_unref(nodes);

	return node;
}

static void
menu_last_said_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	WebKitDOMNode *node;
	const char *who;

	who = g_object_get_data(G_OBJECT(w), "user_data");
	node = get_mark_for_user(gtkconv, who);

	if (node != NULL)
		webkit_dom_element_scroll_into_view(WEBKIT_DOM_ELEMENT(node), TRUE);
	else
		g_return_if_reached();
}

static GtkWidget *
create_chat_menu(PurpleChatConversation *chat, const char *who, PurpleConnection *gc)
{
	static GtkWidget *menu = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	PurpleAccount *account = purple_conversation_get_account(conv);
	gboolean is_me = FALSE;
	GtkWidget *button;
	PurpleBuddy *buddy = NULL;

	if (gc != NULL)
		protocol = purple_connection_get_protocol_info(gc);

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu)
		gtk_widget_destroy(menu);

	if (!strcmp(purple_chat_conversation_get_nick(chat), purple_normalize(account, who)))
		is_me = TRUE;

	menu = gtk_menu_new();

	if (!is_me) {
		button = pidgin_new_item_from_stock(menu, _("IM"), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW,
					G_CALLBACK(menu_chat_im_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);


		if (protocol && protocol->send_file)
		{
			gboolean can_receive_file = TRUE;

			button = pidgin_new_item_from_stock(menu, _("Send File"),
				PIDGIN_STOCK_TOOLBAR_SEND_FILE, G_CALLBACK(menu_chat_send_file_cb),
				PIDGIN_CONVERSATION(conv), 0, 0, NULL);

			if (gc == NULL || protocol == NULL)
				can_receive_file = FALSE;
			else {
				gchar *real_who = NULL;
				if (protocol->get_cb_real_name)
					real_who = protocol->get_cb_real_name(gc,
						purple_chat_conversation_get_id(chat), who);
				if (!(!protocol->can_receive_file || protocol->can_receive_file(gc, real_who ? real_who : who)))
					can_receive_file = FALSE;
				g_free(real_who);
			}

			if (!can_receive_file)
				gtk_widget_set_sensitive(button, FALSE);
			else
				g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
		}


		if (purple_chat_conversation_is_ignored_user(chat, who))
			button = pidgin_new_item_from_stock(menu, _("Un-Ignore"), PIDGIN_STOCK_IGNORE,
							G_CALLBACK(ignore_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
		else
			button = pidgin_new_item_from_stock(menu, _("Ignore"), PIDGIN_STOCK_IGNORE,
							G_CALLBACK(ignore_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (protocol && (protocol->get_info || protocol->get_cb_info)) {
		button = pidgin_new_item_from_stock(menu, _("Info"), PIDGIN_STOCK_TOOLBAR_USER_INFO,
						G_CALLBACK(menu_chat_info_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (!is_me && protocol && !(protocol->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		if ((buddy = purple_blist_find_buddy(account, who)) != NULL)
			button = pidgin_new_item_from_stock(menu, _("Remove"), GTK_STOCK_REMOVE,
						G_CALLBACK(menu_chat_add_remove_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
		else
			button = pidgin_new_item_from_stock(menu, _("Add"), GTK_STOCK_ADD,
						G_CALLBACK(menu_chat_add_remove_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	button = pidgin_new_item_from_stock(menu, _("Last Said"), GTK_STOCK_INDEX,
						G_CALLBACK(menu_last_said_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
	g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	if (!get_mark_for_user(PIDGIN_CONVERSATION(conv), who))
		gtk_widget_set_sensitive(button, FALSE);

	if (buddy != NULL)
	{
		if (purple_account_is_connected(account))
			pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(account),
												  (PurpleBlistNode *)buddy);
		pidgin_append_blist_node_extended_menu(menu, (PurpleBlistNode *)buddy);
		gtk_widget_show_all(menu);
	}

	return menu;
}


static gint
gtkconv_chat_popup_menu_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *menu;
	gchar *who;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = purple_account_get_connection(account);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);
	menu = create_chat_menu (PURPLE_CHAT_CONVERSATION(conv), who, gc);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
				   pidgin_treeview_popup_menu_position_func, widget,
				   0, GDK_CURRENT_TIME);
	g_free(who);

	return TRUE;
}


static gint
right_click_chat_cb(GtkWidget *widget, GdkEventButton *event,
					PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	gchar *who;
	int x, y;

	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = purple_account_get_connection(account);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(gtkchat->list),
								  event->x, event->y, &path, &column, &x, &y);

	if (path == NULL)
		return FALSE;

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list))), path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(gtkchat->list),
							 path, NULL, FALSE);
	gtk_widget_grab_focus(GTK_WIDGET(gtkchat->list));

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	/* emit chat-nick-clicked signal */
	if (event->type == GDK_BUTTON_PRESS) {
		gint plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
					pidgin_conversations_get_handle(), "chat-nick-clicked",
					conv, who, event->button));
		if (plugin_return)
			goto handled;
	}

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		chat_do_im(gtkconv, who);
	} else if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
		/* Move to user's anchor */
		WebKitDOMNode *node = get_mark_for_user(gtkconv, who);

		if (node != NULL)
			webkit_dom_element_scroll_into_view(WEBKIT_DOM_ELEMENT(node), TRUE);

	} else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		GtkWidget *menu = create_chat_menu (PURPLE_CHAT_CONVERSATION(conv), who, gc);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   event->button, event->time);
	}

handled:
	g_free(who);
	gtk_tree_path_free(path);

	return TRUE;
}

static void
activate_list_cb(GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *column, PidginConversation *gtkconv)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *who;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);
	chat_do_im(gtkconv, who);

	g_free(who);
}

static void
move_to_next_unread_tab(PidginConversation *gtkconv, gboolean forward)
{
	PidginConversation *next_gtkconv = NULL, *most_active = NULL;
	PidginUnseenState unseen_state = PIDGIN_UNSEEN_NONE;
	PidginWindow *win;
	int initial, i, total, diff;

	win   = gtkconv->win;
	initial = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
	                                gtkconv->tab_cont);
	total = pidgin_conv_window_get_gtkconv_count(win);
	/* By adding total here, the moduli calculated later will always have two
	 * positive arguments. x % y where x < 0 is not guaranteed to return a
	 * positive number.
	 */
	diff = (forward ? 1 : -1) + total;

	for (i = (initial + diff) % total; i != initial; i = (i + diff) % total) {
		next_gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, i);
		if (next_gtkconv->unseen_state > unseen_state) {
			most_active = next_gtkconv;
			unseen_state = most_active->unseen_state;
			if(PIDGIN_UNSEEN_NICK == unseen_state) /* highest possible state */
				break;
		}
	}

	if (most_active == NULL) { /* no new messages */
		i = (i + diff) % total;
		most_active = pidgin_conv_window_get_gtkconv_at_index(win, i);
	}

	if (most_active != NULL && most_active != gtkconv)
		pidgin_conv_window_switch_gtkconv(win, most_active);
}

static gboolean
gtkconv_cycle_focus(PidginConversation *gtkconv, GtkDirectionType dir)
{
	PurpleConversation *conv = gtkconv->active_conv;
	gboolean chat = PURPLE_IS_CHAT_CONVERSATION(conv);
	GtkWidget *next = NULL;
	struct {
		GtkWidget *from;
		GtkWidget *to;
	} transitions[] = {
		{gtkconv->entry, gtkconv->webview},
		{gtkconv->webview, chat ? gtkconv->u.chat->list : gtkconv->entry},
		{chat ? gtkconv->u.chat->list : NULL, gtkconv->entry},
		{NULL, NULL}
	}, *ptr;

	for (ptr = transitions; !next && ptr->from; ptr++) {
		GtkWidget *from, *to;
		if (dir == GTK_DIR_TAB_FORWARD) {
			from = ptr->from;
			to = ptr->to;
		} else {
			from = ptr->to;
			to = ptr->from;
		}
		if (gtk_widget_is_focus(from))
			next = to;
	}

	if (next)
		gtk_widget_grab_focus(next);
	return !!next;
}

static gboolean
conv_keypress_common(PidginConversation *gtkconv, GdkEventKey *event)
{
	PidginWindow *win;
	int curconv;

	win     = gtkconv->win;
	curconv = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));

	/* clear any tooltips */
	pidgin_tooltip_destroy();

	/* If CTRL was held down... */
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
			case GDK_KEY_Page_Down:
			case GDK_KEY_KP_Page_Down:
			case ']':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv + 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv + 1);
				return TRUE;
				break;

			case GDK_KEY_Page_Up:
			case GDK_KEY_KP_Page_Up:
			case '[':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv - 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), -1);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv - 1);
				return TRUE;
				break;

			case GDK_KEY_Tab:
			case GDK_KEY_KP_Tab:
			case GDK_KEY_ISO_Left_Tab:
				if (event->state & GDK_SHIFT_MASK) {
					move_to_next_unread_tab(gtkconv, FALSE);
				} else {
					move_to_next_unread_tab(gtkconv, TRUE);
				}

				return TRUE;
				break;

			case GDK_KEY_comma:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
						curconv - 1);
				return TRUE;
				break;

			case GDK_KEY_period:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
						(curconv + 1) % gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)));
				return TRUE;
				break;
			case GDK_KEY_F6:
				if (gtkconv_cycle_focus(gtkconv, event->state & GDK_SHIFT_MASK ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD))
					return TRUE;
				break;
		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (event->state & GDK_MOD1_MASK)
	{
		if (event->keyval > '0' && event->keyval <= '9')
		{
			guint switchto = event->keyval - '1';
			if (switchto < pidgin_conv_window_get_gtkconv_count(win))
				gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), switchto);

			return TRUE;
		}
	}

	/* If neither CTRL nor ALT were held down... */
	else
	{
		switch (event->keyval) {
		case GDK_KEY_F2:
			if (gtk_widget_is_focus(GTK_WIDGET(win->notebook))) {
				infopane_entry_activate(gtkconv);
				return TRUE;
			}
			break;
		case GDK_KEY_F6:
			if (gtkconv_cycle_focus(gtkconv, event->state & GDK_SHIFT_MASK ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD))
				return TRUE;
			break;
		}
	}
	return FALSE;
}

static gboolean
entry_key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	gtkconv  = (PidginConversation *)data;
	conv     = gtkconv->active_conv;

	if (conv_keypress_common(gtkconv, event))
		return TRUE;

	/* If CTRL was held down... */
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
			case GDK_KEY_Up:
				if (!gtkconv->send_history)
					break;

				if (gtkconv->entry != entry)
					break;

				if (!gtkconv->send_history->prev) {
					g_free(gtkconv->send_history->data);

					gtkconv->send_history->data =
						gtk_webview_get_body_html(GTK_WEBVIEW(gtkconv->entry));
				}

				if (gtkconv->send_history->next && gtkconv->send_history->next->data) {
					GObject *object;
#if 0
					/* TODO WebKit: maybe not necessary? */
					GtkTextIter iter;
					GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));
#endif

					gtkconv->send_history = gtkconv->send_history->next;

					/* Block the signal to prevent application of default formatting. */
					object = g_object_ref(G_OBJECT(gtkconv->entry));
					g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA,
					                                0, 0, NULL, NULL, gtkconv);
					/* Clear the formatting. */
					gtk_webview_clear_formatting(GTK_WEBVIEW(gtkconv->entry));
					/* Unblock the signal. */
					g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA,
					                                  0, 0, NULL, NULL, gtkconv);
					g_object_unref(object);

					gtk_webview_load_html_string(GTK_WEBVIEW(gtkconv->entry),
					                             gtkconv->send_history->data);
					/* this is mainly just a hack so the formatting at the
					 * cursor gets picked up. */
#if 0
					/* TODO WebKit: maybe not necessary? */
					gtk_text_buffer_get_end_iter(buffer, &iter);
					gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
#endif
				}

				return TRUE;
				break;

			case GDK_KEY_Down:
				if (!gtkconv->send_history)
					break;

				if (gtkconv->entry != entry)
					break;

				if (gtkconv->send_history->prev && gtkconv->send_history->prev->data) {
					GObject *object;
#if 0
					/* TODO WebKit: maybe not necessary? */
					GtkTextIter iter;
					GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));
#endif

					gtkconv->send_history = gtkconv->send_history->prev;

					/* Block the signal to prevent application of default formatting. */
					object = g_object_ref(G_OBJECT(gtkconv->entry));
					g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA,
					                                0, 0, NULL, NULL, gtkconv);
					/* Clear the formatting. */
					gtk_webview_clear_formatting(GTK_WEBVIEW(gtkconv->entry));
					/* Unblock the signal. */
					g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA,
					                                  0, 0, NULL, NULL, gtkconv);
					g_object_unref(object);

					gtk_webview_load_html_string(GTK_WEBVIEW(gtkconv->entry),
					                             gtkconv->send_history->data);
					/* this is mainly just a hack so the formatting at the
					 * cursor gets picked up. */
					if (*(char *)gtkconv->send_history->data) {
#if 0
					/* TODO WebKit: maybe not necessary? */
						gtk_text_buffer_get_end_iter(buffer, &iter);
						gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
#endif
					} else {
						/* Restore the default formatting */
						default_formatize(gtkconv);
					}
				}

				return TRUE;
				break;
		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (event->state & GDK_MOD1_MASK) 	{

	}

	/* If neither CTRL nor ALT were held down... */
	else {
		switch (event->keyval) {
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			if (gtkconv->entry != entry)
				break;
			{
				gint plugin_return;
				plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
							pidgin_conversations_get_handle(), "chat-nick-autocomplete",
							conv, event->state & GDK_SHIFT_MASK));
				return plugin_return ? TRUE : tab_complete(conv);
			}
			break;

		case GDK_KEY_Page_Up:
		case GDK_KEY_KP_Page_Up:
			gtk_webview_page_up(GTK_WEBVIEW(gtkconv->webview));
			return TRUE;
			break;

		case GDK_KEY_Page_Down:
		case GDK_KEY_KP_Page_Down:
			gtk_webview_page_down(GTK_WEBVIEW(gtkconv->webview));
			return TRUE;
			break;

		case GDK_KEY_KP_Enter:
		case GDK_KEY_Return:
			send_cb(entry, gtkconv);
			return TRUE;
			break;

		}
	}
	return FALSE;
}

/*
 * NOTE:
 *   This guy just kills a single right click from being propagated any
 *   further.  I  have no idea *why* we need this, but we do ...  It
 *   prevents right clicks on the GtkTextView in a convo dialog from
 *   going all the way down to the notebook.  I suspect a bug in
 *   GtkTextView, but I'm not ready to point any fingers yet.
 */
static gboolean
entry_stop_rclick_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* Right single click */
		g_signal_stop_emission_by_name(G_OBJECT(widget), "button_press_event");

		return TRUE;
	}

	return FALSE;
}

/*
 * If someone tries to type into the conversation backlog of a
 * conversation window then we yank focus from the conversation backlog
 * and give it to the text entry box so that people can type
 * all the live long day and it will get entered into the entry box.
 */
static gboolean
refocus_entry_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PidginConversation *gtkconv = data;

	/* If we have a valid key for the conversation display, then exit */
	if ((event->state & GDK_CONTROL_MASK) ||
		(event->keyval == GDK_KEY_F6) ||
		(event->keyval == GDK_KEY_F10) ||
		(event->keyval == GDK_KEY_Menu) ||
		(event->keyval == GDK_KEY_Shift_L) ||
		(event->keyval == GDK_KEY_Shift_R) ||
		(event->keyval == GDK_KEY_Control_L) ||
		(event->keyval == GDK_KEY_Control_R) ||
		(event->keyval == GDK_KEY_Escape) ||
		(event->keyval == GDK_KEY_Up) ||
		(event->keyval == GDK_KEY_Down) ||
		(event->keyval == GDK_KEY_Left) ||
		(event->keyval == GDK_KEY_Right) ||
		(event->keyval == GDK_KEY_Page_Up) ||
		(event->keyval == GDK_KEY_KP_Page_Up) ||
		(event->keyval == GDK_KEY_Page_Down) ||
		(event->keyval == GDK_KEY_KP_Page_Down) ||
		(event->keyval == GDK_KEY_Home) ||
		(event->keyval == GDK_KEY_End) ||
		(event->keyval == GDK_KEY_Tab) ||
		(event->keyval == GDK_KEY_KP_Tab) ||
		(event->keyval == GDK_KEY_ISO_Left_Tab))
	{
		if (event->type == GDK_KEY_PRESS)
			return conv_keypress_common(gtkconv, event);
		return FALSE;
	}

	gtk_widget_grab_focus(gtkconv->entry);
	gtk_widget_event(gtkconv->entry, (GdkEvent *)event);

	return TRUE;
}

static void
regenerate_options_items(PidginWindow *win);

void
pidgin_conv_switch_active_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PurpleConversation *old_conv;
	GtkWebView *entry;
	const char *protocol_name;
	PurpleConnectionFlags features;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	old_conv = gtkconv->active_conv;

	purple_debug_info("gtkconv", "setting active conversation on toolbar %p\n",
		conv);

	if (old_conv == conv)
		return;

	purple_conversation_close_logs(old_conv);
	gtkconv->active_conv = conv;

	purple_conversation_set_logging(conv,
		gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(gtkconv->win->menu.logging)));

	entry = GTK_WEBVIEW(gtkconv->entry);
	protocol_name = purple_account_get_protocol_name(purple_conversation_get_account(conv));
	gtk_webview_set_protocol_name(entry, protocol_name);
	gtk_webview_set_protocol_name(GTK_WEBVIEW(gtkconv->webview), protocol_name);

	features = purple_conversation_get_features(conv);
	if (!(features & PURPLE_CONNECTION_FLAG_HTML))
		gtk_webview_clear_formatting(GTK_WEBVIEW(gtkconv->entry));
	else if (features & PURPLE_CONNECTION_FLAG_FORMATTING_WBFO &&
	         !(purple_conversation_get_features(old_conv) & PURPLE_CONNECTION_FLAG_FORMATTING_WBFO))
	{
		/* The old conversation allowed formatting on parts of the
		 * buffer, but the new one only allows it on the whole
		 * buffer.  This code saves the formatting from the current
		 * position of the cursor, clears the formatting, then
		 * applies the saved formatting to the entire buffer. */

		gboolean bold;
		gboolean italic;
		gboolean underline;
		gboolean strike;
		char *fontface   = gtk_webview_get_current_fontface(entry);
		char *forecolor  = gtk_webview_get_current_forecolor(entry);
		char *backcolor  = gtk_webview_get_current_backcolor(entry);
#if 0
		/* TODO WebKit: Do we need this again? */
		char *background = gtk_webview_get_current_background(entry);
#endif
		gint fontsize    = gtk_webview_get_current_fontsize(entry);
		gboolean bold2;
		gboolean italic2;
		gboolean underline2;
		gboolean strike2;

		gtk_webview_get_current_format(entry, &bold, &italic, &underline, &strike);

		/* Clear existing formatting */
		gtk_webview_clear_formatting(entry);

		/* Apply saved formatting to the whole buffer. */

		gtk_webview_get_current_format(entry, &bold2, &italic2, &underline2, &strike2);

		if (bold != bold2)
			gtk_webview_toggle_bold(entry);

		if (italic != italic2)
			gtk_webview_toggle_italic(entry);

		if (underline != underline2)
			gtk_webview_toggle_underline(entry);

		if (strike != strike2)
			gtk_webview_toggle_strike(entry);

		gtk_webview_toggle_fontface(entry, fontface);

		if (!(features & PURPLE_CONNECTION_FLAG_NO_FONTSIZE))
			gtk_webview_font_set_size(entry, fontsize);

		gtk_webview_toggle_forecolor(entry, forecolor);

		if (!(features & PURPLE_CONNECTION_FLAG_NO_BGCOLOR))
		{
			gtk_webview_toggle_backcolor(entry, backcolor);
#if 0
			gtk_webview_toggle_background(entry, background);
#endif
		}

		g_free(fontface);
		g_free(forecolor);
		g_free(backcolor);
#if 0
		g_free(background);
#endif
	}
	else
	{
		/* This is done in default_formatize, which is called from clear_formatting_cb,
		 * which is (obviously) a clear_formatting signal handler.  However, if we're
		 * here, we didn't call gtk_imhtml_clear_formatting() (because we want to
		 * preserve the formatting exactly as it is), so we have to do this now. */
		gtk_webview_set_whole_buffer_formatting_only(entry,
			(features & PURPLE_CONNECTION_FLAG_FORMATTING_WBFO));
	}

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);

	gray_stuff_out(gtkconv);
	update_typing_icon(gtkconv);
	g_object_set_data(G_OBJECT(entry), "transient_buddy", NULL);
	regenerate_options_items(gtkconv->win);

	gtk_window_set_title(GTK_WINDOW(gtkconv->win->window),
	                     gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
}

static void
menu_conv_sel_send_cb(GObject *m, gpointer data)
{
	PurpleAccount *account = g_object_get_data(m, "purple_account");
	gchar *name = g_object_get_data(m, "purple_buddy_name");
	PurpleIMConversation *im;

	if (gtk_check_menu_item_get_active((GtkCheckMenuItem*) m) == FALSE)
		return;

	im = purple_im_conversation_new(account, name);
	pidgin_conv_switch_active_conversation(PURPLE_CONVERSATION(im));
}

#if 0
static void
insert_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *position,
			   gchar *new_text, gint new_text_length, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;

	g_return_if_fail(gtkconv != NULL);

	if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
		return;

	got_typing_keypress(gtkconv, (gtk_text_iter_is_start(position) &&
	                    gtk_text_iter_is_end(position)));
}

static void
delete_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *start_pos,
			   GtkTextIter *end_pos, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleConversation *conv;
	PurpleIMConversation *im;

	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;

	if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
		return;

	im = PURPLE_CONV_IM(conv);

	if (gtk_text_iter_is_start(start_pos) && gtk_text_iter_is_end(end_pos)) {

		/* We deleted all the text, so turn off typing. */
		purple_im_conversation_stop_send_typed_timeout(im);

		serv_send_typing(purple_conversation_get_connection(conv),
						 purple_conversation_get_name(conv),
						 PURPLE_IM_NOT_TYPING);
	}
	else {
		/* We're deleting, but not all of it, so it counts as typing. */
		got_typing_keypress(gtkconv, FALSE);
	}
}
#endif

/**************************************************************************
 * A bunch of buddy icon functions
 **************************************************************************/

static GList *get_protocol_icon_list(PurpleAccount *account)
{
	GList *l = NULL;
	PurpleProtocol *protocol =
			purple_find_protocol_info(purple_account_get_protocol_id(account));
	const char *prplname = protocol->list_icon(account, NULL);
	l = g_hash_table_lookup(protocol_lists, prplname);
	if (l)
		return l;

	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_LARGE));
	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_MEDIUM));
	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL));

	g_hash_table_insert(protocol_lists, g_strdup(prplname), l);
	return l;
}

static GList *
pidgin_conv_get_tab_icons(PurpleConversation *conv)
{
	PurpleAccount *account = NULL;
	const char *name = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			PurplePresence *p;
			p = purple_buddy_get_presence(b);
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY))
				return away_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE))
				return busy_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY))
				return xa_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE))
				return offline_list;
			else
				return available_list;
		}
	}

	return get_protocol_icon_list(account);
}

static const char *
pidgin_conv_get_icon_stock(PurpleConversation *conv)
{
	PurpleAccount *account = NULL;
	const char *stock = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	g_return_val_if_fail(account != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		const char *name = NULL;
		PurpleBuddy *b;
		name = purple_conversation_get_name(conv);
		b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			PurplePresence *p = purple_buddy_get_presence(b);
			PurpleStatus *active = purple_presence_get_active_status(p);
			PurpleStatusType *type = purple_status_get_status_type(active);
			PurpleStatusPrimitive prim = purple_status_type_get_primitive(type);
			stock = pidgin_stock_id_from_status_primitive(prim);
		} else {
			stock = PIDGIN_STOCK_STATUS_PERSON;
		}
	} else {
		stock = PIDGIN_STOCK_STATUS_CHAT;
	}

	return stock;
}

static GdkPixbuf *
pidgin_conv_get_icon(PurpleConversation *conv, GtkWidget *parent, const char *icon_size)
{
	PurpleAccount *account = NULL;
	const char *name = NULL;
	const char *stock = NULL;
	GdkPixbuf *status = NULL;
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	GtkIconSize size;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			/* I hate this hack.  It fixes a bug where the pending message icon
			 * displays in the conv tab even though it shouldn't.
			 * A better solution would be great. */
			if (ops && ops->update)
				ops->update(NULL, (PurpleBlistNode*)b);
		}
	}

	stock = pidgin_conv_get_icon_stock(conv);
	size = gtk_icon_size_from_name(icon_size);
	status = gtk_widget_render_icon (parent, stock, size, "GtkWidget");
	return status;
}

GdkPixbuf *
pidgin_conv_get_tab_icon(PurpleConversation *conv, gboolean small_icon)
{
	const char *icon_size = small_icon ? PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC : PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL;
	return pidgin_conv_get_icon(conv, PIDGIN_CONVERSATION(conv)->icon, icon_size);
}


static void
update_tab_icon(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PidginWindow *win;
	GList *l;
	GdkPixbuf *emblem = NULL;
	const char *status = NULL;
	const char *infopane_status = NULL;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	status = infopane_status = pidgin_conv_get_icon_stock(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
		if (b)
			emblem = pidgin_blist_get_emblem((PurpleBlistNode*)b);
	}

	g_return_if_fail(status != NULL);

	g_object_set(G_OBJECT(gtkconv->icon), "stock", status, NULL);
	g_object_set(G_OBJECT(gtkconv->menu_icon), "stock", status, NULL);

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_ICON_COLUMN, infopane_status, -1);

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_EMBLEM_COLUMN, emblem, -1);
	if (emblem)
		g_object_unref(emblem);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons")) {
		emblem = pidgin_create_protocol_icon(purple_conversation_get_account(gtkconv->active_conv), PIDGIN_PROTOCOL_ICON_SMALL);
	} else {
		emblem = NULL;
	}

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_PROTOCOL_ICON_COLUMN, emblem, -1);
	if (emblem)
		g_object_unref(emblem);

	/* XXX seanegan Why do I have to do this? */
	gtk_widget_queue_resize(gtkconv->infopane);
	gtk_widget_queue_draw(gtkconv->infopane);

	if (pidgin_conv_window_is_active_conversation(conv) &&
		(PURPLE_IS_CHAT_CONVERSATION(conv) || gtkconv->u.im->anim == NULL))
	{
		l = pidgin_conv_get_tab_icons(conv);

		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
	}
}

#if 0
/* This gets added as an idle handler when doing something that
 * redraws the icon. It sets the auto_resize gboolean to TRUE.
 * This way, when the size_allocate callback gets triggered, it notices
 * that this is an autoresize, and after the main loop iterates, it
 * gets set back to FALSE
 */
static gboolean reset_auto_resize_cb(gpointer data)
{
	PidginConversation *gtkconv = (PidginConversation *)data;
	gtkconv->auto_resize = FALSE;
	return FALSE;
}
#endif

static gboolean
redraw_icon(gpointer data)
{
	PidginConversation *gtkconv = (PidginConversation *)data;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	GdkPixbuf *buf;
	GdkPixbuf *scale;
	gint delay;
	int scale_width, scale_height;
	int size;

	gtkconv = PIDGIN_CONVERSATION(conv);
	account = purple_conversation_get_account(conv);

	if (!(account && purple_account_get_connection(account))) {
		gtkconv->u.im->icon_timer = 0;
		return FALSE;
	}

	gdk_pixbuf_animation_iter_advance(gtkconv->u.im->iter, NULL);
	buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);

	scale_width = gdk_pixbuf_get_width(buf);
	scale_height = gdk_pixbuf_get_height(buf);

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);
	size = MIN(size, MIN(scale_width, scale_height));
	size = CLAMP(size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);

	if (scale_width == scale_height) {
		scale_width = scale_height = size;
	} else if (scale_height > scale_width) {
		scale_width = size * scale_width / scale_height;
		scale_height = size;
	} else {
		scale_height = size * scale_height / scale_width;
		scale_width = size;
	}

	scale = gdk_pixbuf_scale_simple(buf, scale_width, scale_height,
		GDK_INTERP_BILINEAR);
	if (pidgin_gdk_pixbuf_is_opaque(scale))
		pidgin_gdk_pixbuf_make_round(scale);

	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->u.im->icon), scale);
	g_object_unref(G_OBJECT(scale));
	gtk_widget_queue_draw(gtkconv->u.im->icon);

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);

	return FALSE;
}

static void
start_anim(GtkWidget *widget, PidginConversation *gtkconv)
{
	int delay;

	if (gtkconv->u.im->anim == NULL)
		return;

	if (gtkconv->u.im->icon_timer != 0)
		return;

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim))
		return;

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);
}

static void
remove_icon(GtkWidget *widget, PidginConversation *gtkconv)
{
	GList *children;
	GtkWidget *event;
	PurpleConversation *conv = gtkconv->active_conv;

	g_return_if_fail(conv != NULL);

	gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, BUDDYICON_SIZE_MIN);
	children = gtk_container_get_children(GTK_CONTAINER(gtkconv->u.im->icon_container));
	if (children) {
		/* We know there's only one child here. It'd be nice to shortcut to the
		   event box, but we can't change the PidginConversation until 3.0 */
		event = (GtkWidget *)children->data;
		gtk_container_remove(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
		g_list_free(children);
	}

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->icon_timer = 0;
	gtkconv->u.im->icon = NULL;
	gtkconv->u.im->anim = NULL;
	gtkconv->u.im->iter = NULL;
	gtkconv->u.im->show_icon = FALSE;
}

static void
saveicon_writefile_cb(void *user_data, const char *filename)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(gtkconv->active_conv);
	PurpleBuddyIcon *icon;
	const void *data;
	size_t len;

	icon = purple_im_conversation_get_icon(im);
	data = purple_buddy_icon_get_data(icon, &len);

	if ((len <= 0) || (data == NULL) || !purple_util_write_data_to_file_absolute(filename, data, len)) {
		purple_notify_error(gtkconv, NULL, _("Unable to save icon file to disk."), NULL);
	}
}

static void
custom_icon_sel_cb(const char *filename, gpointer data)
{
	if (filename) {
		const gchar *name;
		PurpleBuddy *buddy;
		PurpleContact *contact;
		PidginConversation *gtkconv = data;
		PurpleConversation *conv = gtkconv->active_conv;
		PurpleAccount *account = purple_conversation_get_account(conv);

		name = purple_conversation_get_name(conv);
		buddy = purple_blist_find_buddy(account, name);
		if (!buddy) {
			purple_debug_info("custom-icon", "You can only set custom icons for people on your buddylist.\n");
			return;
		}
		contact = purple_buddy_get_contact(buddy);

		purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, filename);
	}
}

static void
set_custom_icon_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	GtkWidget *win = pidgin_buddy_icon_chooser_new(GTK_WINDOW(gtkconv->win->window),
						custom_icon_sel_cb, gtkconv);
	gtk_widget_show_all(win);
}

static void
change_size_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	int size = 0;
	PurpleConversation *conv = gtkconv->active_conv;
	GSList *buddies;

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);

	if (size == BUDDYICON_SIZE_MAX) {
		size = BUDDYICON_SIZE_MIN;
	} else {
		size = BUDDYICON_SIZE_MAX;
	}

	gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, size);
	pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));

	buddies = purple_blist_find_buddies(purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
	for (; buddies; buddies = g_slist_delete_link(buddies, buddies)) {
		PurpleBuddy *buddy = buddies->data;
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		purple_blist_node_set_int((PurpleBlistNode*)contact, "pidgin-infopane-iconsize", size);
	}
}

static void
remove_custom_icon_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	const gchar *name;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleContact *contact;
	PurpleConversation *conv = gtkconv->active_conv;

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);
	buddy = purple_blist_find_buddy(account, name);
	if (!buddy) {
		return;
	}
	contact = purple_buddy_get_contact(buddy);

	purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, NULL);
}

static void
icon_menu_save_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	const gchar *ext;
	gchar *buf;

	g_return_if_fail(conv != NULL);

	ext = purple_buddy_icon_get_extension(purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(conv)));

	buf = g_strdup_printf("%s.%s", purple_normalize(purple_conversation_get_account(conv), purple_conversation_get_name(conv)), ext);

	purple_request_file(gtkconv, _("Save Icon"), buf, TRUE,
					 G_CALLBACK(saveicon_writefile_cb), NULL,
					purple_conversation_get_account(conv), NULL, conv,
					gtkconv);

	g_free(buf);
}

static void
stop_anim(GtkWidget *widget, PidginConversation *gtkconv)
{
	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;
}


static void
toggle_icon_animate_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	gtkconv->u.im->animate =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));

	if (gtkconv->u.im->animate)
		start_anim(NULL, gtkconv);
	else
		stop_anim(NULL, gtkconv);
}

static gboolean
icon_menu(GtkWidget *widget, GdkEventButton *e, PidginConversation *gtkconv)
{
	static GtkWidget *menu = NULL;
	PurpleConversation *conv;
	PurpleBuddy *buddy;

	if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
		change_size_cb(NULL, gtkconv);
		return TRUE;
	}

	if (e->button != 3 || e->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu != NULL)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	if (gtkconv->u.im->anim &&
		!(gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)))
	{
		pidgin_new_check_item(menu, _("Animate"),
							G_CALLBACK(toggle_icon_animate_cb), gtkconv,
							gtkconv->u.im->icon_timer);
	}

	pidgin_new_item_from_stock(menu, _("Hide Icon"), NULL, G_CALLBACK(remove_icon),
							 gtkconv, 0, 0, NULL);

	pidgin_new_item_from_stock(menu, _("Save Icon As..."), GTK_STOCK_SAVE_AS,
							 G_CALLBACK(icon_menu_save_cb), gtkconv,
							 0, 0, NULL);

	pidgin_new_item_from_stock(menu, _("Set Custom Icon..."), NULL,
							 G_CALLBACK(set_custom_icon_cb), gtkconv,
							 0, 0, NULL);

	pidgin_new_item_from_stock(menu, _("Change Size"), NULL,
							 G_CALLBACK(change_size_cb), gtkconv,
							 0, 0, NULL);

	/* Is there a custom icon for this person? */
	conv = gtkconv->active_conv;
	buddy = purple_blist_find_buddy(purple_conversation_get_account(conv),
	                          purple_conversation_get_name(conv));
	if (buddy)
	{
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		if (contact && purple_buddy_icons_node_has_custom_icon((PurpleBlistNode*)contact))
		{
			pidgin_new_item_from_stock(menu, _("Remove Custom Icon"), NULL,
			                           G_CALLBACK(remove_custom_icon_cb), gtkconv,
			                           0, 0, NULL);
		}
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, e->button, e->time);

	return TRUE;
}

/**************************************************************************
 * End of the bunch of buddy icon functions
 **************************************************************************/
void
pidgin_conv_present_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	GdkModifierType state;

	pidgin_conv_attach_to_conversation(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);

	pidgin_conv_switch_active_conversation(conv);
	/* Switch the tab only if the user initiated the event by pressing
	 * a button or hitting a key. */
	if (gtk_get_current_event_state(&state))
		pidgin_conv_window_switch_gtkconv(gtkconv->win, gtkconv);
	gtk_window_present(GTK_WINDOW(gtkconv->win->window));
}

static GList *
pidgin_conversations_get_unseen(GList *l,
									PidginUnseenState min_state,
									gboolean hidden_only,
									guint max_count)
{
	GList *r = NULL;
	guint c = 0;

	for (; l != NULL && (max_count == 0 || c < max_count); l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		if(gtkconv == NULL || gtkconv->active_conv != conv)
			continue;

		if (gtkconv->unseen_state >= min_state
			&& (!hidden_only ||
				(hidden_only && gtkconv->win == hidden_convwin))) {

			r = g_list_prepend(r, conv);
			c++;
		}
	}

	return r;
}

GList *
pidgin_conversations_get_unseen_all(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_all(),
			min_state, hidden_only, max_count);
}

GList *
pidgin_conversations_get_unseen_ims(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_ims(),
			min_state, hidden_only, max_count);
}

GList *
pidgin_conversations_get_unseen_chats(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_chats(),
			min_state, hidden_only, max_count);
}

static void
unseen_conv_menu_cb(GtkMenuItem *item, PurpleConversation *conv)
{
	g_return_if_fail(conv != NULL);
	pidgin_conv_present_conversation(conv);
}

static void
unseen_all_conv_menu_cb(GtkMenuItem *item, GList *list)
{
	g_return_if_fail(list != NULL);
	/* Do not free the list from here. It will be freed from the
	 * 'destroy' callback on the menuitem. */
	while (list) {
		pidgin_conv_present_conversation(list->data);
		list = list->next;
	}
}

guint
pidgin_conversations_fill_menu(GtkWidget *menu, GList *convs)
{
	GList *l;
	guint ret=0;

	g_return_val_if_fail(menu != NULL, 0);
	g_return_val_if_fail(convs != NULL, 0);

	for (l = convs; l != NULL ; l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		GtkWidget *icon = gtk_image_new_from_stock(pidgin_conv_get_icon_stock(conv),
				gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC));
		GtkWidget *item;
		gchar *text = g_strdup_printf("%s (%d)",
				gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)),
				gtkconv->unseen_count);

		item = gtk_image_menu_item_new_with_label(text);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), icon);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(unseen_conv_menu_cb), conv);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_free(text);
		ret++;
	}

	if (convs->next) {
		/* There are more than one conversation. Add an option to show all conversations. */
		GtkWidget *item;
		GList *list = g_list_copy(convs);

		pidgin_separator(menu);

		item = gtk_menu_item_new_with_label(_("Show All"));
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(unseen_all_conv_menu_cb), list);
		g_signal_connect_swapped(G_OBJECT(item), "destroy", G_CALLBACK(g_list_free), list);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	return ret;
}

PidginWindow *
pidgin_conv_get_window(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);
	return gtkconv->win;
}

static GtkActionEntry menu_entries[] =
/* TODO: fill out tooltips... */
{
	/* Conversation menu */
	{ "ConversationMenu", NULL, N_("_Conversation"), NULL, NULL, NULL },
	{ "NewInstantMessage", PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, N_("New Instant _Message..."), "<control>M", NULL, G_CALLBACK(menu_new_conv_cb) },
	{ "JoinAChat", PIDGIN_STOCK_CHAT, N_("Join a _Chat..."), NULL, NULL, G_CALLBACK(menu_join_chat_cb) },
	{ "Find", GTK_STOCK_FIND, N_("_Find..."), NULL, NULL, G_CALLBACK(menu_find_cb) },
	{ "ViewLog", NULL, N_("View _Log"), NULL, NULL, G_CALLBACK(menu_view_log_cb) },
	{ "SaveAs", GTK_STOCK_SAVE_AS, N_("_Save As..."), NULL, NULL, G_CALLBACK(menu_save_as_cb) },
	{ "ClearScrollback", GTK_STOCK_CLEAR, N_("Clea_r Scrollback"), "<control>L", NULL, G_CALLBACK(menu_clear_cb) },

#ifdef USE_VV
	{ "MediaMenu", NULL, N_("M_edia"), NULL, NULL, NULL },
	{ "AudioCall", PIDGIN_STOCK_TOOLBAR_AUDIO_CALL, N_("_Audio Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
	{ "VideoCall", PIDGIN_STOCK_TOOLBAR_VIDEO_CALL, N_("_Video Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
	{ "AudioVideoCall", PIDGIN_STOCK_TOOLBAR_VIDEO_CALL, N_("Audio/Video _Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
#endif

	{ "SendFile", PIDGIN_STOCK_TOOLBAR_SEND_FILE, N_("Se_nd File..."), NULL, NULL, G_CALLBACK(menu_send_file_cb) },
	{ "GetAttention", PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION, N_("Get _Attention"), NULL, NULL, G_CALLBACK(menu_get_attention_cb) },
	{ "AddBuddyPounce", NULL, N_("Add Buddy _Pounce..."), NULL, NULL, G_CALLBACK(menu_add_pounce_cb) },
	{ "GetInfo", PIDGIN_STOCK_TOOLBAR_USER_INFO, N_("_Get Info"), "<control>O", NULL, G_CALLBACK(menu_get_info_cb) },
	{ "Invite", NULL, N_("In_vite..."), NULL, NULL, G_CALLBACK(menu_invite_cb) },
	{ "MoreMenu", NULL, N_("M_ore"), NULL, NULL, NULL },
	{ "Alias", NULL, N_("Al_ias..."), NULL, NULL, G_CALLBACK(menu_alias_cb) },
	{ "Block", PIDGIN_STOCK_TOOLBAR_BLOCK, N_("_Block..."), NULL, NULL, G_CALLBACK(menu_block_cb) },
	{ "Unblock", PIDGIN_STOCK_TOOLBAR_UNBLOCK, N_("_Unblock..."), NULL, NULL, G_CALLBACK(menu_unblock_cb) },
	{ "Add", GTK_STOCK_ADD, N_("_Add..."), NULL, NULL, G_CALLBACK(menu_add_remove_cb) },
	{ "Remove", GTK_STOCK_REMOVE, N_("_Remove..."), NULL, NULL, G_CALLBACK(menu_add_remove_cb) },
	{ "InsertLink", PIDGIN_STOCK_TOOLBAR_INSERT_LINK, N_("Insert Lin_k..."), NULL, NULL, G_CALLBACK(menu_insert_link_cb) },
	{ "InsertImage", PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE, N_("Insert Imag_e..."), NULL, NULL, G_CALLBACK(menu_insert_image_cb) },
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), NULL, NULL, G_CALLBACK(menu_close_conv_cb) },

	/* Options */
	{ "OptionsMenu", NULL, N_("_Options"), NULL, NULL, NULL },
};

/* Toggle items */
static const GtkToggleActionEntry menu_toggle_entries[] = {
	{ "EnableLogging", NULL, N_("Enable _Logging"), NULL, NULL, G_CALLBACK(menu_logging_cb), FALSE },
	{ "EnableSounds", NULL, N_("Enable _Sounds"), NULL, NULL, G_CALLBACK(menu_sounds_cb), FALSE },
	{ "ShowFormattingToolbars", NULL, N_("Show Formatting _Toolbars"), NULL, NULL, G_CALLBACK(menu_toolbar_cb), FALSE },
};

static const char *conversation_menu =
"<ui>"
	"<menubar name='Conversation'>"
		"<menu action='ConversationMenu'>"
			"<menuitem action='NewInstantMessage'/>"
			"<menuitem action='JoinAChat'/>"
			"<separator/>"
			"<menuitem action='Find'/>"
			"<menuitem action='ViewLog'/>"
			"<menuitem action='SaveAs'/>"
			"<menuitem action='ClearScrollback'/>"
			"<separator/>"
#ifdef USE_VV
			"<menu action='MediaMenu'>"
				"<menuitem action='AudioCall'/>"
				"<menuitem action='VideoCall'/>"
				"<menuitem action='AudioVideoCall'/>"
			"</menu>"
#endif
			"<menuitem action='SendFile'/>"
			"<menuitem action='GetAttention'/>"
			"<menuitem action='AddBuddyPounce'/>"
			"<menuitem action='GetInfo'/>"
			"<menuitem action='Invite'/>"
			"<menu action='MoreMenu'/>"
			"<separator/>"
			"<menuitem action='Alias'/>"
			"<menuitem action='Block'/>"
			"<menuitem action='Unblock'/>"
			"<menuitem action='Add'/>"
			"<menuitem action='Remove'/>"
			"<separator/>"
			"<menuitem action='InsertLink'/>"
			"<menuitem action='InsertImage'/>"
			"<separator/>"
			"<menuitem action='Close'/>"
		"</menu>"
		"<menu action='OptionsMenu'>"
			"<menuitem action='EnableLogging'/>"
			"<menuitem action='EnableSounds'/>"
			"<separator/>"
			"<menuitem action='ShowFormattingToolbars'/>"
		"</menu>"
	"</menubar>"
"</ui>";

static void
sound_method_pref_changed_cb(const char *name, PurplePrefType type,
							 gconstpointer value, gpointer data)
{
	PidginWindow *win = data;
	const char *method = value;

	if (!strcmp(method, "none"))
	{
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu.sounds),
		                             FALSE);
		gtk_action_set_sensitive(win->menu.sounds, FALSE);
	}
	else
	{
		PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

		if (gtkconv != NULL)
			gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu.sounds),
			                             gtkconv->make_sound);
		gtk_action_set_sensitive(win->menu.sounds, TRUE);
	}
}

/* Returns TRUE if some items were added to the menu, FALSE otherwise */
static gboolean
populate_menu_with_options(GtkWidget *menu, PidginConversation *gtkconv, gboolean all)
{
	GList *list;
	PurpleConversation *conv;
	PurpleAccount *account;
	PurpleBlistNode *node = NULL;
	PurpleChat *chat = NULL;
	PurpleBuddy *buddy = NULL;
	gboolean ret;

	conv = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);

	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		chat = purple_blist_find_chat(account, purple_conversation_get_name(conv));

		if ((chat == NULL) && (gtkconv->webview != NULL)) {
			chat = g_object_get_data(G_OBJECT(gtkconv->webview), "transient_chat");
		}

		if ((chat == NULL) && (gtkconv->webview != NULL)) {
			GHashTable *components;
			PurpleAccount *account = purple_conversation_get_account(conv);
			PurpleProtocol *protocol =
					purple_find_protocol_info(purple_account_get_protocol_id(account));
			if (purple_account_get_connection(account) != NULL &&
					PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(protocol, chat_info_defaults)) {
				components = protocol->chat_info_defaults(purple_account_get_connection(account),
						purple_conversation_get_name(conv));
			} else {
				components = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, g_free);
				g_hash_table_replace(components, g_strdup("channel"),
						g_strdup(purple_conversation_get_name(conv)));
			}
			chat = purple_chat_new(account, NULL, components);
			purple_blist_node_set_transient((PurpleBlistNode *)chat, TRUE);
			g_object_set_data_full(G_OBJECT(gtkconv->webview), "transient_chat",
					chat, (GDestroyNotify)purple_blist_remove_chat);
		}
	} else {
		if (!purple_account_is_connected(account))
			return FALSE;

		buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
		if (!buddy && gtkconv->webview) {
			buddy = g_object_get_data(G_OBJECT(gtkconv->webview), "transient_buddy");

			if (!buddy) {
				buddy = purple_buddy_new(account, purple_conversation_get_name(conv), NULL);
				purple_blist_node_set_transient((PurpleBlistNode *)buddy, TRUE);
				g_object_set_data_full(G_OBJECT(gtkconv->webview), "transient_buddy",
						buddy, (GDestroyNotify)g_object_unref);
			}
		}
	}

	if (chat)
		node = (PurpleBlistNode *)chat;
	else if (buddy)
		node = (PurpleBlistNode *)buddy;

	/* Now add the stuff */
	if (all) {
		if (buddy)
			pidgin_blist_make_buddy_menu(menu, buddy, TRUE);
		else if (chat) {
			/* XXX: */
		}
	} else if (node) {
		if (purple_account_is_connected(account))
			pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(account), node);
		pidgin_append_blist_node_extended_menu(menu, node);
	}

	if ((list = gtk_container_get_children(GTK_CONTAINER(menu))) == NULL) {
		ret = FALSE;
	} else {
		g_list_free(list);
		ret = TRUE;
	}
	return ret;
}

static void
regenerate_media_items(PidginWindow *win)
{
#ifdef USE_VV
	PurpleAccount *account;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL) {
		purple_debug_error("gtkconv", "couldn't get active conversation"
				" when regenerating media items\n");
		return;
	}

	account = purple_conversation_get_account(conv);

	if (account == NULL) {
		purple_debug_error("gtkconv", "couldn't get account when"
				" regenerating media items\n");
		return;
	}

	/*
	 * Check if account support voice and/or calls, and
	 * if the current buddy	supports it.
	 */
	if (account != NULL && PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleMediaCaps caps =
				purple_protocol_get_media_caps(account,
				purple_conversation_get_name(conv));

		gtk_action_set_sensitive(win->menu.audio_call,
				caps & PURPLE_MEDIA_CAPS_AUDIO
				? TRUE : FALSE);
		gtk_action_set_sensitive(win->menu.video_call,
				caps & PURPLE_MEDIA_CAPS_VIDEO
				? TRUE : FALSE);
		gtk_action_set_sensitive(win->menu.audio_video_call,
				caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO
				? TRUE : FALSE);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		/* for now, don't care about chats... */
		gtk_action_set_sensitive(win->menu.audio_call, FALSE);
		gtk_action_set_sensitive(win->menu.video_call, FALSE);
		gtk_action_set_sensitive(win->menu.audio_video_call, FALSE);
	} else {
		gtk_action_set_sensitive(win->menu.audio_call, FALSE);
		gtk_action_set_sensitive(win->menu.video_call, FALSE);
		gtk_action_set_sensitive(win->menu.audio_video_call, FALSE);
	}
#endif
}

static void
regenerate_attention_items(PidginWindow *win)
{
	GtkWidget *attention;
	GtkWidget *menu;
	PurpleConversation *conv;
	PurpleConnection *pc;
	PurpleProtocol *protocol = NULL;
	GList *list;

	conv = pidgin_conv_window_get_active_conversation(win);
	if (!conv)
		return;

	attention = gtk_ui_manager_get_widget(win->menu.ui,
	                                      "/Conversation/ConversationMenu/GetAttention");

	/* Remove the previous entries */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(attention), NULL);

	pc = purple_conversation_get_connection(conv);
	if (pc != NULL)
		protocol = purple_connection_get_protocol_info(pc);

	if (protocol && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(protocol, get_attention_types)) {
		list = protocol->get_attention_types(purple_connection_get_account(pc));

		/* Multiple attention types */
		if (list && list->next) {
			int index = 0;

			menu = gtk_menu_new();
			while (list) {
				PurpleAttentionType *type;
				GtkWidget *menuitem;

				type = list->data;

				menuitem = gtk_menu_item_new_with_label(purple_attention_type_get_name(type));
				g_object_set_data(G_OBJECT(menuitem), "index", GINT_TO_POINTER(index));
				g_signal_connect(G_OBJECT(menuitem), "activate",
				                 G_CALLBACK(menu_get_attention_cb),
				                 win);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

				index++;
				list = g_list_delete_link(list, list);
			}

			gtk_menu_item_set_submenu(GTK_MENU_ITEM(attention), menu);
			gtk_widget_show_all(menu);
		}
	}
}

static void
regenerate_options_items(PidginWindow *win)
{
	GtkWidget *menu;
	PidginConversation *gtkconv;
	GList *list;
	GtkWidget *more_menu;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	more_menu = gtk_ui_manager_get_widget(win->menu.ui,
	                                      "/Conversation/ConversationMenu/MoreMenu");
	gtk_widget_show(more_menu);
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(more_menu));

	/* Remove the previous entries */
	for (list = gtk_container_get_children(GTK_CONTAINER(menu)); list; )
	{
		GtkWidget *w = list->data;
		list = g_list_delete_link(list, list);
		gtk_widget_destroy(w);
	}

	if (!populate_menu_with_options(menu, gtkconv, FALSE))
	{
		GtkWidget *item = gtk_menu_item_new_with_label(_("No actions available"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_set_sensitive(item, FALSE);
	}

	gtk_widget_show_all(menu);
}

static void
remove_from_list(GtkWidget *widget, PidginWindow *win)
{
	GList *list = g_object_get_data(G_OBJECT(win->window), "plugin-actions");
	list = g_list_remove(list, widget);
	g_object_set_data(G_OBJECT(win->window), "plugin-actions", list);
}

static void
regenerate_plugins_items(PidginWindow *win)
{
	GList *action_items;
	GtkWidget *menu;
	GList *list;
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	GtkWidget *item;

	if (win->window == NULL || win == hidden_convwin)
		return;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	if (gtkconv == NULL)
		return;

	conv = gtkconv->active_conv;
	action_items = g_object_get_data(G_OBJECT(win->window), "plugin-actions");

	/* Remove the old menuitems */
	while (action_items) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(action_items->data),
					G_CALLBACK(remove_from_list), win);
		gtk_widget_destroy(action_items->data);
		action_items = g_list_delete_link(action_items, action_items);
	}

	item = gtk_ui_manager_get_widget(win->menu.ui, "/Conversation/OptionsMenu");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));

	list = purple_conversation_get_extended_menu(conv);
	if (list) {
		action_items = g_list_prepend(NULL, (item = pidgin_separator(menu)));
		g_signal_connect(G_OBJECT(item), "destroy", G_CALLBACK(remove_from_list), win);
	}

	for(; list; list = g_list_delete_link(list, list)) {
		PurpleMenuAction *act = (PurpleMenuAction *) list->data;
		item = pidgin_append_menu_action(menu, act, conv);
		action_items = g_list_prepend(action_items, item);
		gtk_widget_show_all(item);
		g_signal_connect(G_OBJECT(item), "destroy", G_CALLBACK(remove_from_list), win);
	}
	g_object_set_data(G_OBJECT(win->window), "plugin-actions", action_items);
}

static void menubar_activated(GtkWidget *item, gpointer data)
{
	PidginWindow *win = data;
	regenerate_media_items(win);
	regenerate_options_items(win);
	regenerate_plugins_items(win);
	regenerate_attention_items(win);

	/* The following are to make sure the 'More' submenu is not regenerated every time
	 * the focus shifts from 'Conversations' to some other menu and back. */
	g_signal_handlers_block_by_func(G_OBJECT(item), G_CALLBACK(menubar_activated), data);
	g_signal_connect(G_OBJECT(win->menu.menubar), "deactivate", G_CALLBACK(focus_out_from_menubar), data);
}

static void
focus_out_from_menubar(GtkWidget *wid, PidginWindow *win)
{
	/* The menubar has been deactivated. Make sure the 'More' submenu is regenerated next time
	 * the 'Conversation' menu pops up. */
	GtkWidget *menuitem = gtk_ui_manager_get_widget(win->menu.ui, "/Conversation/ConversationMenu");
	g_signal_handlers_unblock_by_func(G_OBJECT(menuitem), G_CALLBACK(menubar_activated), win);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->menu.menubar),
				G_CALLBACK(focus_out_from_menubar), win);
}

static GtkWidget *
setup_menubar(PidginWindow *win)
{
	GtkAccelGroup *accel_group;
	const char *method;
	GtkActionGroup *action_group;
	GError *error;
	GtkWidget *menuitem;

	action_group = gtk_action_group_new("ConversationActions");
#ifdef ENABLE_NLS
	gtk_action_group_set_translation_domain(action_group, PACKAGE);
#endif
	gtk_action_group_add_actions(action_group,
	                             menu_entries,
	                             G_N_ELEMENTS(menu_entries),
	                             win);
	gtk_action_group_add_toggle_actions(action_group,
	                                    menu_toggle_entries,
	                                    G_N_ELEMENTS(menu_toggle_entries),
	                                    win);

	win->menu.ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(win->menu.ui, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group(win->menu.ui);
	gtk_window_add_accel_group(GTK_WINDOW(win->window), accel_group);
	g_signal_connect(G_OBJECT(accel_group), "accel-changed",
	                 G_CALLBACK(pidgin_save_accels_cb), NULL);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string(win->menu.ui, conversation_menu, -1, &error))
	{
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		exit(EXIT_FAILURE);
	}

	win->menu.menubar =
		gtk_ui_manager_get_widget(win->menu.ui, "/Conversation");

	/* Make sure the 'Conversation -> More' menuitems are regenerated whenever
	 * the 'Conversation' menu pops up because the entries can change after the
	 * conversation is created. */
	menuitem = gtk_ui_manager_get_widget(win->menu.ui, "/Conversation/ConversationMenu");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(menubar_activated), win);

	win->menu.view_log =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/ViewLog");

#ifdef USE_VV
	win->menu.audio_call =
		gtk_ui_manager_get_action(win->menu.ui,
					    "/Conversation/ConversationMenu/MediaMenu/AudioCall");
	win->menu.video_call =
		gtk_ui_manager_get_action(win->menu.ui,
					    "/Conversation/ConversationMenu/MediaMenu/VideoCall");
	win->menu.audio_video_call =
		gtk_ui_manager_get_action(win->menu.ui,
					    "/Conversation/ConversationMenu/MediaMenu/AudioVideoCall");
#endif

	/* --- */

	win->menu.send_file =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/SendFile");

	win->menu.get_attention =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/GetAttention");

	win->menu.add_pounce =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/AddBuddyPounce");

	/* --- */

	win->menu.get_info =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/GetInfo");

	win->menu.invite =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/Invite");

	/* --- */

	win->menu.alias =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/Alias");

	win->menu.block =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/Block");

	win->menu.unblock =
		gtk_ui_manager_get_action(win->menu.ui,
					    "/Conversation/ConversationMenu/Unblock");

	win->menu.add =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/Add");

	win->menu.remove =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/ConversationMenu/Remove");

	/* --- */

	win->menu.insert_link =
		gtk_ui_manager_get_action(win->menu.ui,
				"/Conversation/ConversationMenu/InsertLink");

	win->menu.insert_image =
		gtk_ui_manager_get_action(win->menu.ui,
				"/Conversation/ConversationMenu/InsertImage");

	/* --- */

	win->menu.logging =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/OptionsMenu/EnableLogging");
	win->menu.sounds =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/OptionsMenu/EnableSounds");
	method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (method != NULL && !strcmp(method, "none"))
	{
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu.sounds),
		                               FALSE);
		gtk_action_set_sensitive(win->menu.sounds, FALSE);
	}
	purple_prefs_connect_callback(win, PIDGIN_PREFS_ROOT "/sound/method",
				    sound_method_pref_changed_cb, win);

	win->menu.show_formatting_toolbar =
		gtk_ui_manager_get_action(win->menu.ui,
		                          "/Conversation/OptionsMenu/ShowFormattingToolbars");

	win->menu.tray = pidgin_menu_tray_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(win->menu.menubar),
	                      win->menu.tray);
	gtk_widget_show(win->menu.tray);

	gtk_widget_show(win->menu.menubar);

	return win->menu.menubar;
}


/**************************************************************************
 * Utility functions
 **************************************************************************/

#if 0
static void
got_typing_keypress(PidginConversation *gtkconv, gboolean first)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleIMConversation *im;

	/*
	 * We know we got something, so we at least have to make sure we don't
	 * send PURPLE_IM_TYPED any time soon.
	 */

	im = PURPLE_CONV_IM(conv);

	purple_im_conversation_stop_send_typed_timeout(im);
	purple_im_conversation_start_send_typed_timeout(im);

	/* Check if we need to send another PURPLE_IM_TYPING message */
	if (first || (purple_im_conversation_get_type_again(im) != 0 &&
				  time(NULL) > purple_im_conversation_get_type_again(im)))
	{
		unsigned int timeout;
		timeout = serv_send_typing(purple_conversation_get_connection(conv),
								   purple_conversation_get_name(conv),
								   PURPLE_IM_TYPING);
		purple_im_conversation_set_type_again(im, timeout);
	}
}

static gboolean
typing_animation(gpointer data) {
	PidginConversation *gtkconv = data;
	PidginWindow *gtkwin = gtkconv->win;
	const char *stock_id = NULL;

	if(gtkconv != pidgin_conv_window_get_active_gtkconv(gtkwin)) {
		return FALSE;
	}

	switch (rand() % 5) {
	case 0:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING0;
		break;
	case 1:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING1;
		break;
	case 2:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING2;
		break;
	case 3:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING3;
		break;
	case 4:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING4;
		break;
	}
	if (gtkwin->menu.typing_icon == NULL) {
		gtkwin->menu.typing_icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
		pidgin_menu_tray_append(PIDGIN_MENU_TRAY(gtkwin->menu.tray),
		                        gtkwin->menu.typing_icon,
		                        _("User is typing..."));
	} else {
		gtk_image_set_from_stock(GTK_IMAGE(gtkwin->menu.typing_icon), stock_id, GTK_ICON_SIZE_MENU);
	}
	gtk_widget_show(gtkwin->menu.typing_icon);
	return TRUE;
}
#endif

static void
update_typing_message(PidginConversation *gtkconv, const char *message)
{
	/* TODO WEBKIT: this is not handled at all */
#if 0
	GtkTextBuffer *buffer;
	GtkTextMark *stmark, *enmark;

	if (g_object_get_data(G_OBJECT(gtkconv->imhtml), "disable-typing-notification"))
		return;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
	stmark = gtk_text_buffer_get_mark(buffer, "typing-notification-start");
	enmark = gtk_text_buffer_get_mark(buffer, "typing-notification-end");
	if (stmark && enmark) {
		GtkTextIter start, end;
		gtk_text_buffer_get_iter_at_mark(buffer, &start, stmark);
		gtk_text_buffer_get_iter_at_mark(buffer, &end, enmark);
		gtk_text_buffer_delete_mark(buffer, stmark);
		gtk_text_buffer_delete_mark(buffer, enmark);
		gtk_text_buffer_delete(buffer, &start, &end);
	} else if (message && *message == '\n' && message[1] == ' ' && message[2] == '\0')
		message = NULL;

#ifdef RESERVE_LINE
	if (!message)
		message = "\n ";   /* The blank space is required to avoid a GTK+/Pango bug */
#endif

	if (message) {
		GtkTextIter iter;
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-start", &iter, TRUE);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, message, -1, "TYPING-NOTIFICATION", NULL);
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-end", &iter, TRUE);
	}
#endif /* if 0 */
}

static void
update_typing_icon(PidginConversation *gtkconv)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(gtkconv->active_conv);
	char *message = NULL;

	if (im == NULL)
		return;

	if (purple_im_conversation_get_typing_state(im) == PURPLE_IM_NOT_TYPING) {
#ifdef RESERVE_LINE
		update_typing_message(gtkconv, NULL);
#else
		update_typing_message(gtkconv, "\n ");
#endif
		return;
	}

	if (purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPING) {
		message = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_title(PURPLE_CONVERSATION(im)));
	} else {
		message = g_strdup_printf(_("\n%s has stopped typing"), purple_conversation_get_title(PURPLE_CONVERSATION(im)));
	}

	update_typing_message(gtkconv, message);
	g_free(message);
}

static gboolean
update_send_to_selection(PidginWindow *win)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	GtkWidget *menu;
	GList *child;
	PurpleBuddy *b;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return FALSE;

	account = purple_conversation_get_account(conv);

	if (account == NULL)
		return FALSE;

	if (win->menu.send_to == NULL)
		return FALSE;

	if (!(b = purple_blist_find_buddy(account, purple_conversation_get_name(conv))))
		return FALSE;

	gtk_widget_show(win->menu.send_to);

	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(win->menu.send_to));

	for (child = gtk_container_get_children(GTK_CONTAINER(menu));
		 child != NULL;
		 child = g_list_delete_link(child, child)) {

		GtkWidget *item = child->data;
		PurpleBuddy *item_buddy;
		PurpleAccount *item_account = g_object_get_data(G_OBJECT(item), "purple_account");
		gchar *buddy_name = g_object_get_data(G_OBJECT(item),
		                                      "purple_buddy_name");
		item_buddy = purple_blist_find_buddy(item_account, buddy_name);

		if (b == item_buddy) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
			g_list_free(child);
			break;
		}
	}

	return FALSE;
}

static gboolean
send_to_item_enter_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), TRUE);
	return FALSE;
}

static gboolean
send_to_item_leave_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	return FALSE;
}

static void
create_sendto_item(GtkWidget *menu, GtkSizeGroup *sg, GSList **group, PurpleBuddy *buddy, PurpleAccount *account, const char *name)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *menuitem;
	GdkPixbuf *pixbuf;
	gchar *text;

	/* Create a pixmap for the protocol icon. */
	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);

	/* Now convert it to GtkImage */
	if (pixbuf == NULL)
		image = gtk_image_new();
	else
	{
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
	}

	gtk_size_group_add_widget(sg, image);

	/* Make our menu item */
	text = g_strdup_printf("%s (%s)", name, purple_account_get_name_for_display(account));
	menuitem = gtk_radio_menu_item_new_with_label(*group, text);
	g_free(text);
	*group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

	/* Do some evil, see some evil, speak some evil. */
	box = gtk_hbox_new(FALSE, 0);

	label = gtk_bin_get_child(GTK_BIN(menuitem));
	g_object_ref(label);
	gtk_container_remove(GTK_CONTAINER(menuitem), label);

	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 4);

	if (buddy != NULL &&
	    !purple_presence_is_online(purple_buddy_get_presence(buddy)))
	{
		gtk_widget_set_sensitive(label, FALSE);

		/* Set the label sensitive when the menuitem is highlighted and
		 * insensitive again when the mouse leaves it. This way, it
		 * doesn't appear weird from the highlighting of the embossed
		 * (insensitive style) text.*/
		g_signal_connect(menuitem, "enter-notify-event",
				 G_CALLBACK(send_to_item_enter_notify_cb), label);
		g_signal_connect(menuitem, "leave-notify-event",
				 G_CALLBACK(send_to_item_leave_notify_cb), label);
	}

	g_object_unref(label);

	gtk_container_add(GTK_CONTAINER(menuitem), box);

	gtk_widget_show(label);
	gtk_widget_show(image);
	gtk_widget_show(box);

	/* Set our data and callbacks. */
	g_object_set_data(G_OBJECT(menuitem), "purple_account", account);
	g_object_set_data_full(G_OBJECT(menuitem), "purple_buddy_name", g_strdup(name), g_free);

	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(menu_conv_sel_send_cb), NULL);

	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static gboolean
compare_buddy_presence(PurplePresence *p1, PurplePresence *p2)
{
	/* This is necessary because multiple PurpleBuddy's don't share the same
	 * PurplePresence anymore.
	 */
	PurpleBuddy *b1 = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(p1));
	PurpleBuddy *b2 = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(p2));
	if (purple_buddy_get_account(b1) == purple_buddy_get_account(b2) &&
			strcmp(purple_buddy_get_name(b1), purple_buddy_get_name(b2)) == 0)
		return FALSE;
	return TRUE;
}

static void
generate_send_to_items(PidginWindow *win)
{
	GtkWidget *menu;
	GSList *group = NULL;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	PidginConversation *gtkconv;
	GSList *l, *buds;

	g_return_if_fail(win != NULL);

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	g_return_if_fail(gtkconv != NULL);

	if (win->menu.send_to != NULL)
		gtk_widget_destroy(win->menu.send_to);

	/* Build the Send To menu */
	win->menu.send_to = gtk_menu_item_new_with_mnemonic(_("S_end To"));
	gtk_widget_show(win->menu.send_to);

	menu = gtk_menu_new();
	gtk_menu_shell_insert(GTK_MENU_SHELL(win->menu.menubar),
	                      win->menu.send_to, 2);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(win->menu.send_to), menu);

	gtk_widget_show(menu);

	if (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv)) {
		buds = purple_blist_find_buddies(purple_conversation_get_account(gtkconv->active_conv), purple_conversation_get_name(gtkconv->active_conv));

		if (buds == NULL)
		{
			/* The user isn't on the buddy list. So we don't create any sendto menu. */
		}
		else
		{
			GList *list = NULL, *iter;
			for (l = buds; l != NULL; l = l->next)
			{
				PurpleBlistNode *node;

				node = PURPLE_BLIST_NODE(purple_buddy_get_contact(PURPLE_BUDDY(l->data)));

				for (node = node->child; node != NULL; node = node->next)
				{
					PurpleBuddy *buddy = (PurpleBuddy *)node;
					PurpleAccount *account;

					if (!PURPLE_IS_BUDDY(node))
						continue;

					account = purple_buddy_get_account(buddy);
					/* TODO WEBKIT: (I'm not actually sure if this is webkit-related --Mark Doliner) */
					if (purple_account_is_connected(account) /*|| account == purple_conversation_get_account(gtkconv->active_conv)*/)
					{
						/* Use the PurplePresence to get unique buddies. */
						PurplePresence *presence = purple_buddy_get_presence(buddy);
						if (!g_list_find_custom(list, presence, (GCompareFunc)compare_buddy_presence))
							list = g_list_prepend(list, presence);
					}
				}
			}

			/* Create the sendto menu only if it has more than one item to show */
			if (list && list->next) {
				/* Loop over the list backwards so we get the items in the right order,
				 * since we did a g_list_prepend() earlier. */
				for (iter = g_list_last(list); iter != NULL; iter = iter->prev) {
					PurplePresence *pre = iter->data;
					PurpleBuddy *buddy = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(pre));
					create_sendto_item(menu, sg, &group, buddy,
							purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
				}
			}
			g_list_free(list);
			g_slist_free(buds);
		}
	}

	g_object_unref(sg);

	gtk_widget_show(win->menu.send_to);
	/* TODO: This should never be insensitive.  Possibly hidden or not. */
	if (!group)
		gtk_widget_set_sensitive(win->menu.send_to, FALSE);
	update_send_to_selection(win);
}

static const char *
get_chat_user_status_icon(PurpleChatConversation *chat, const char *name, PurpleChatUserFlags flags)
{
	const char *image = NULL;

	if (flags & PURPLE_CHAT_USER_FOUNDER) {
		image = PIDGIN_STOCK_STATUS_FOUNDER;
	} else if (flags & PURPLE_CHAT_USER_OP) {
		image = PIDGIN_STOCK_STATUS_OPERATOR;
	} else if (flags & PURPLE_CHAT_USER_HALFOP) {
		image = PIDGIN_STOCK_STATUS_HALFOP;
	} else if (flags & PURPLE_CHAT_USER_VOICE) {
		image = PIDGIN_STOCK_STATUS_VOICE;
	} else if ((!flags) && purple_chat_conversation_is_ignored_user(chat, name)) {
		image = PIDGIN_STOCK_STATUS_IGNORED;
	} else {
		return NULL;
	}
	return image;
}

static void
deleting_chat_user_cb(PurpleChatUser *cb)
{
	GtkTreeRowReference *ref = purple_chat_user_get_ui_data(cb);

	if (ref) {
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(cb, NULL);
	}
}

static void
add_chat_user_common(PurpleChatConversation *chat, PurpleChatUser *cb, const char *old_name)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	GtkTreeModel *tm;
	GtkListStore *ls;
	GtkTreePath *newpath;
	const char *stock;
	GtkTreeIter iter;
	gboolean is_me = FALSE;
	gboolean is_buddy;
	const gchar *name, *alias;
	gchar *tmp, *alias_key;
	PurpleChatUserFlags flags;
	GdkColor *color = NULL;

	alias = purple_chat_user_get_alias(cb);
	name  = purple_chat_user_get_name(cb);
	flags = purple_chat_user_get_flags(cb);

	conv    = PURPLE_CONVERSATION(chat);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	gc      = purple_conversation_get_connection(conv);

	if (!gc || !(protocol = purple_connection_get_protocol_info(gc)))
		return;

	tm = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
	ls = GTK_LIST_STORE(tm);

	stock = get_chat_user_status_icon(chat, name, flags);

	if (!strcmp(purple_chat_conversation_get_nick(chat), purple_normalize(purple_conversation_get_account(conv), old_name != NULL ? old_name : name)))
		is_me = TRUE;

	is_buddy = purple_chat_user_is_buddy(cb);

	tmp = g_utf8_casefold(alias, -1);
	alias_key = g_utf8_collate_key(tmp, -1);
	g_free(tmp);

	if (is_me) {
#if 0
		/* TODO WEBKIT: No tags in webkit stuff, yet. */
		GtkTextTag *tag = gtk_text_tag_table_lookup(
				gtk_text_buffer_get_tag_table(GTK_IMHTML(gtkconv->webview)->text_buffer),
				"send-name");
		g_object_get(tag, "foreground-gdk", &color, NULL);
#endif /* if 0 */
	} else {
		GtkTextTag *tag;
		if ((tag = get_buddy_tag(chat, name, 0, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_NORMAL, NULL);
		if ((tag = get_buddy_tag(chat, name, PURPLE_MESSAGE_NICK, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_NORMAL, NULL);
		color = (GdkColor*)get_nick_color(gtkconv, name);
	}

	gtk_list_store_insert_with_values(ls, &iter,
/*
* The GTK docs are mute about the effects of the "row" value for performance.
* X-Chat hardcodes their value to 0 (prepend) and -1 (append), so we will too.
* It *might* be faster to search the gtk_list_store and set row accurately,
* but no one in #gtk+ seems to know anything about it either.
* Inserting in the "wrong" location has no visible ill effects. - F.P.
*/
			-1, /* "row" */
			CHAT_USERS_ICON_STOCK_COLUMN,  stock,
			CHAT_USERS_ALIAS_COLUMN, alias,
			CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
			CHAT_USERS_NAME_COLUMN,  name,
			CHAT_USERS_FLAGS_COLUMN, flags,
			CHAT_USERS_COLOR_COLUMN, color,
			CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
			-1);

	if (purple_chat_user_get_ui_data(cb)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(cb);
		gtk_tree_row_reference_free(ref);
	}

	newpath = gtk_tree_model_get_path(tm, &iter);
	purple_chat_user_set_ui_data(cb, gtk_tree_row_reference_new(tm, newpath));
	gtk_tree_path_free(newpath);

	if (is_me && color)
		gdk_color_free(color);
	g_free(alias_key);
}

#if 0
/**
 * @param most_matched Used internally by this function.
 * @param entered The partial string that the user types before hitting the
 *        tab key.
 * @param entered_bytes The length of entered.
 * @param partial This is a return variable.  This will be set to a string
 *        containing the largest common string between all matches.  This will
 *        be inserted into the input box at the start of the word that the
 *        user is tab completing.  For example, if a chat room contains
 *        "AlfFan" and "AlfHater" and the user types "a<TAB>" then this will
 *        contain "Alf"
 * @param nick_partial Used internally by this function.  Shoudl be a
 *        temporary buffer that is entered_bytes+1 bytes long.
 * @param matches This is a return variable.  If the given name is a potential
 *        match for the entered string, then add a copy of the name to this
 *        list.  The caller is responsible for g_free'ing the data in this
 *        list.
 * @param name The buddy name or alias or slash command name that we're
 *        checking for a match.
 */
static void
tab_complete_process_item(int *most_matched, const char *entered, gsize entered_bytes, char **partial, char *nick_partial,
				  GList **matches, const char *name)
{
	memcpy(nick_partial, name, entered_bytes);
	if (purple_utf8_strcasecmp(nick_partial, entered))
		return;

	/* if we're here, it's a possible completion */

	if (*most_matched == -1) {
		/*
		 * this will only get called once, since from now
		 * on *most_matched is >= 0
		 */
		*most_matched = strlen(name);
		*partial = g_strdup(name);
	}
	else if (*most_matched) {
		char *tmp = g_strdup(name);

		while (purple_utf8_strcasecmp(tmp, *partial)) {
			(*partial)[*most_matched] = '\0';
			if (*most_matched < strlen(tmp))
				tmp[*most_matched] = '\0';
			(*most_matched)--;
		}
		(*most_matched)++;

		g_free(tmp);
	}

	*matches = g_list_insert_sorted(*matches, g_strdup(name),
								   (GCompareFunc)purple_utf8_strcasecmp);
}
#endif

static gboolean
tab_complete(PurpleConversation *conv)
{
#if 0
	/* TODO WebKit */
	PidginConversation *gtkconv;
	GtkTextIter cursor, word_start, start_buffer;
	int start;
	int most_matched = -1;
	char *entered, *partial = NULL;
	char *text;
	char *nick_partial;
	const char *prefix;
	GList *matches = NULL;
	gboolean command = FALSE;
	gsize entered_bytes = 0;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
	gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
			gtk_text_buffer_get_insert(gtkconv->entry_buffer));

	word_start = cursor;

	/* if there's nothing there just return */
	if (!gtk_text_iter_compare(&cursor, &start_buffer))
		return PURPLE_IS_CHAT_CONVERSATION(conv);

	text = gtk_text_buffer_get_text(gtkconv->entry_buffer, &start_buffer,
									&cursor, FALSE);

	/* if we're at the end of ": " we need to move back 2 spaces */
	start = strlen(text) - 1;

	if (start >= 1 && !strncmp(&text[start-1], ": ", 2)) {
		gtk_text_iter_backward_chars(&word_start, 2);
	}

	/* find the start of the word that we're tabbing.
	 * Using gtk_text_iter_backward_word_start won't work, because a nick can contain
	 * characters (e.g. '.', '/' etc.) that Pango may think are word separators. */
	while (gtk_text_iter_backward_char(&word_start)) {
		if (gtk_text_iter_get_char(&word_start) == ' ') {
			/* Reached the whitespace before the start of the word. Move forward once */
			gtk_text_iter_forward_char(&word_start);
			break;
		}
	}

	prefix = pidgin_get_cmd_prefix();
	if (gtk_text_iter_get_offset(&word_start) == 0 &&
			(strlen(text) >= strlen(prefix)) && !strncmp(text, prefix, strlen(prefix))) {
		command = TRUE;
		gtk_text_iter_forward_chars(&word_start, strlen(prefix));
	}

	g_free(text);

	entered = gtk_text_buffer_get_text(gtkconv->entry_buffer, &word_start,
									   &cursor, FALSE);
	entered_bytes = strlen(entered);

	if (!g_utf8_strlen(entered, -1)) {
		g_free(entered);
		return PURPLE_IS_CHAT_CONVERSATION(conv);
	}

	nick_partial = g_malloc0(entered_bytes + 1);

	if (command) {
		GList *list = purple_cmd_list(conv);
		GList *l;

		/* Commands */
		for (l = list; l != NULL; l = l->next) {
			tab_complete_process_item(&most_matched, entered, entered_bytes, &partial, nick_partial,
									  &matches, l->data);
		}
		g_list_free(list);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChatConversation *chat = PURPLE_CONV_CHAT(conv);
		GList *l = purple_chat_conversation_get_users(chat);
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(PIDGIN_CONVERSATION(conv)->u.chat->list));
		GtkTreeIter iter;
		int f;

		/* Users */
		for (; l != NULL; l = l->next) {
			tab_complete_process_item(&most_matched, entered, entered_bytes, &partial, nick_partial,
									  &matches, purple_chat_user_get_name((PurpleChatUser *)l->data));
		}


		/* Aliases */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		{
			do {
				char *name;
				char *alias;

				gtk_tree_model_get(model, &iter,
						   CHAT_USERS_NAME_COLUMN, &name,
						   CHAT_USERS_ALIAS_COLUMN, &alias,
						   -1);

				if (name && alias && strcmp(name, alias))
					tab_complete_process_item(&most_matched, entered, entered_bytes, &partial, nick_partial,
										  &matches, alias);
				g_free(name);
				g_free(alias);

				f = gtk_tree_model_iter_next(model, &iter);
			} while (f != 0);
		}
	} else {
		g_free(nick_partial);
		g_free(entered);
		return FALSE;
	}

	g_free(nick_partial);

	/* we're only here if we're doing new style */

	/* if there weren't any matches, return */
	if (!matches) {
		/* if matches isn't set partials won't be either */
		g_free(entered);
		return PURPLE_IS_CHAT_CONVERSATION(conv);
	}

	gtk_text_buffer_delete(gtkconv->entry_buffer, &word_start, &cursor);

	if (!matches->next) {
		/* there was only one match. fill it in. */
		gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
		gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
				gtk_text_buffer_get_insert(gtkconv->entry_buffer));

		if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
			char *tmp = g_strdup_printf("%s: ", (char *)matches->data);
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, tmp, -1);
			g_free(tmp);
		}
		else
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
											 matches->data, -1);

		g_free(matches->data);
		g_list_free(matches);
	}
	else {
		/*
		 * there were lots of matches, fill in as much as possible
		 * and display all of them
		 */
		char *addthis = g_malloc0(1);

		while (matches) {
			char *tmp = addthis;
			addthis = g_strconcat(tmp, matches->data, " ", NULL);
			g_free(tmp);
			g_free(matches->data);
			matches = g_list_remove(matches, matches->data);
		}

		purple_conversation_write(conv, NULL, addthis, PURPLE_MESSAGE_NO_LOG,
								time(NULL));
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, partial, -1);
		g_free(addthis);
	}

	g_free(entered);
	g_free(partial);
#endif
	return TRUE;
}

static void topic_callback(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	char *new_topic;
	const char *current_topic;

	gc      = purple_conversation_get_connection(conv);

	if(!gc || !(protocol = purple_connection_get_protocol_info(gc)))
		return;

	if(protocol->set_chat_topic == NULL)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	new_topic = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkchat->topic_text)));
	current_topic = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));

	if(current_topic && !g_utf8_collate(new_topic, current_topic)){
		g_free(new_topic);
		return;
	}

	if (current_topic)
		gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), current_topic);
	else
		gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), "");

	protocol->set_chat_topic(gc, purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)),
			new_topic);

	g_free(new_topic);
}

static gint
sort_chat_users(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata)
{
	PurpleChatUserFlags f1 = 0, f2 = 0;
	char *user1 = NULL, *user2 = NULL;
	gboolean buddy1 = FALSE, buddy2 = FALSE;
	gint ret = 0;

	gtk_tree_model_get(model, a,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user1,
	                   CHAT_USERS_FLAGS_COLUMN, &f1,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy1,
	                   -1);
	gtk_tree_model_get(model, b,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user2,
	                   CHAT_USERS_FLAGS_COLUMN, &f2,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy2,
	                   -1);

	/* Only sort by membership levels */
	f1 &= PURPLE_CHAT_USER_VOICE | PURPLE_CHAT_USER_HALFOP | PURPLE_CHAT_USER_OP |
			PURPLE_CHAT_USER_FOUNDER;
	f2 &= PURPLE_CHAT_USER_VOICE | PURPLE_CHAT_USER_HALFOP | PURPLE_CHAT_USER_OP |
			PURPLE_CHAT_USER_FOUNDER;

	if (user1 == NULL || user2 == NULL) {
		if (!(user1 == NULL && user2 == NULL))
			ret = (user1 == NULL) ? -1: 1;
	} else if (f1 != f2) {
		/* sort more important users first */
		ret = (f1 > f2) ? -1 : 1;
	} else if (buddy1 != buddy2) {
		ret = (buddy1 > buddy2) ? -1 : 1;
	} else {
		ret = strcmp(user1, user2);
	}

	g_free(user1);
	g_free(user2);

	return ret;
}

static void
update_chat_alias(PurpleBuddy *buddy, PurpleChatConversation *chat, PurpleConnection *gc, PurpleProtocol *protocol)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	PurpleAccount *account = purple_conversation_get_account(PURPLE_CONVERSATION(chat));
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(chat != NULL);

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkconv->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(account, purple_buddy_get_name(buddy)));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (!strcmp(normalized_name, purple_normalize(account, name))) {
			const char *alias = name;
			char *tmp;
			char *alias_key = NULL;
			PurpleBuddy *buddy2;

			if (strcmp(purple_chat_conversation_get_nick(chat), purple_normalize(account, name))) {
				/* This user is not me, so look into updating the alias. */

				if ((buddy2 = purple_blist_find_buddy(account, name)) != NULL) {
					alias = purple_buddy_get_contact_alias(buddy2);
				}

				tmp = g_utf8_casefold(alias, -1);
				alias_key = g_utf8_collate_key(tmp, -1);
				g_free(tmp);

				gtk_list_store_set(GTK_LIST_STORE(model), &iter,
								CHAT_USERS_ALIAS_COLUMN, alias,
								CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
								-1);
				g_free(alias_key);
			}
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);
}

static void
blist_node_aliased_cb(PurpleBlistNode *node, const char *old_alias, PurpleChatConversation *chat)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);

	g_return_if_fail(node != NULL);
	g_return_if_fail(conv != NULL);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	g_return_if_fail(purple_connection_get_protocol_info(gc) != NULL);
	protocol = purple_connection_get_protocol_info(gc);

	if (protocol->options & OPT_PROTO_UNIQUE_CHATNAME)
		return;

	if (PURPLE_IS_CONTACT(node))
	{
		PurpleBlistNode *bnode;

		for(bnode = node->child; bnode; bnode = bnode->next) {

			if(!PURPLE_IS_BUDDY(bnode))
				continue;

			update_chat_alias((PurpleBuddy *)bnode, chat, gc, protocol);
		}
	}
	else if (PURPLE_IS_BUDDY(node))
		update_chat_alias((PurpleBuddy *)node, chat, gc, protocol);
	else if (PURPLE_IS_CHAT(node) &&
			purple_conversation_get_account(conv) == purple_chat_get_account((PurpleChat*)node))
	{
		if (old_alias == NULL || g_utf8_collate(old_alias, purple_conversation_get_title(conv)) == 0)
			pidgin_conv_update_fields(conv, PIDGIN_CONV_SET_TITLE);
	}
}

static void
buddy_cb_common(PurpleBuddy *buddy, PurpleChatConversation *chat, gboolean is_buddy)
{
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	GtkTextTag *texttag;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(conv != NULL);

	/* Do nothing if the buddy does not belong to the conv's account */
	if (purple_buddy_get_account(buddy) != purple_conversation_get_account(conv))
		return;

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(PIDGIN_CONVERSATION(conv)->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(purple_conversation_get_account(conv), purple_buddy_get_name(buddy)));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (!strcmp(normalized_name, purple_normalize(purple_conversation_get_account(conv), name))) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			                   CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, -1);
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);

	blist_node_aliased_cb((PurpleBlistNode *)buddy, NULL, chat);

	texttag = get_buddy_tag(chat, purple_buddy_get_name(buddy), 0, FALSE); /* XXX: do we want the normalized name? */
	if (texttag) {
		g_object_set(texttag, "weight", is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);
	}
}

static void
buddy_added_cb(PurpleBlistNode *node, PurpleChatConversation *chat)
{
	if (!PURPLE_IS_BUDDY(node))
		return;

	buddy_cb_common(PURPLE_BUDDY(node), chat, TRUE);
}

static void
buddy_removed_cb(PurpleBlistNode *node, PurpleChatConversation *chat)
{
	if (!PURPLE_IS_BUDDY(node))
		return;

	/* If there's another buddy for the same "dude" on the list, do nothing. */
	if (purple_blist_find_buddy(purple_buddy_get_account(PURPLE_BUDDY(node)),
		                  purple_buddy_get_name(PURPLE_BUDDY(node))) != NULL)
		return;

	buddy_cb_common(PURPLE_BUDDY(node), chat, FALSE);
}

static void
entry_popup_menu_cb(GtkWebView *webview, GtkMenu *menu, gpointer data)
{
	GtkWidget *menuitem;
	PidginConversation *gtkconv = data;
	char *tmp;

	g_return_if_fail(menu != NULL);
	g_return_if_fail(gtkconv != NULL);

	menuitem = pidgin_new_item_from_stock(NULL, _("_Send"), NULL,
	                                      G_CALLBACK(send_cb), gtkconv,
	                                      0, 0, NULL);
	tmp = gtk_webview_get_body_text(webview);
	if (!tmp || !*tmp)
		gtk_widget_set_sensitive(menuitem, FALSE);
	g_free(tmp);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 0);

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 1);
}

static gboolean
resize_webview_cb(PidginConversation *gtkconv)
{
#if 0
	/* TODO WebKit: entry sizing */
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	int lines;
	GdkRectangle oneline;
	int height, diff;
	int pad_top, pad_inside, pad_bottom;
	int total_height;
	int max_height;
	int min_lines = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines");
	int min_height;
	gboolean interior_focus;
	int focus_width;
	GtkAllocation webview_allocation;
	GtkAllocation entry_allocation;
	GtkAllocation lower_hbox_allocation;

	gtk_widget_get_allocation(gtkconv->webview, &webview_allocation);
	gtk_widget_get_allocation(gtkconv->entry, &entry_allocation);
	gtk_widget_get_allocation(gtkconv->lower_hbox, &lower_hbox_allocation);
	total_height = webview_allocation.height + entry_allocation.height;
	max_height = total_height / 2;

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(gtkconv->entry));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(gtkconv->entry));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(gtkconv->entry));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(gtkconv->entry), &iter, &oneline);

	lines = gtk_text_buffer_get_line_count(buffer);

	height = 0;
	do {
		int lineheight = 0;
		gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(gtkconv->entry), &iter, NULL, &lineheight);
		height += lineheight;
		lines--;
	} while (gtk_text_iter_forward_line(&iter));
	height += lines * (oneline.height + pad_top + pad_bottom);

	/* Make sure there's enough room for at least min_lines. Allocate enough space to
	 * prevent scrolling when the second line is a continuation of the first line, or
	 * is the beginning of a new paragraph. */
	min_height = min_lines * (oneline.height + MAX(pad_inside, pad_top + pad_bottom));
	height = CLAMP(height, MIN(min_height, max_height), max_height);

	gtk_widget_style_get(gtkconv->entry,
	                     "interior-focus", &interior_focus,
	                     "focus-line-width", &focus_width,
	                     NULL);
	if (!interior_focus)
		height += 2 * focus_width;

	diff = height - entry_allocation.height;
	if (ABS(diff) < oneline.height / 2)
		return FALSE;

	purple_debug_info("pidgin", "resizing to %d, %d lines, diff %d\n",
	                  diff + lower_hbox_allocation.height, min_lines, diff);

	gtk_widget_set_size_request(gtkconv->lower_hbox, -1,
		diff + lower_hbox_allocation.height);
#endif
	gtk_widget_set_size_request(gtkconv->lower_hbox, -1, -1);

	return FALSE;
}

static void
minimum_entry_lines_pref_cb(const char *name,
                            PurplePrefType type,
                            gconstpointer value,
                            gpointer data)
{
	GList *l = purple_conversations_get_all();
	PurpleConversation *conv;
	while (l != NULL)
	{
		conv = (PurpleConversation *)l->data;

		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			resize_webview_cb(PIDGIN_CONVERSATION(conv));

		l = l->next;
	}
}

static void
setup_chat_topic(PidginConversation *gtkconv, GtkWidget *vbox)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	PurpleProtocol *protocol = purple_connection_get_protocol_info(gc);
	if (protocol->options & OPT_PROTO_CHAT_TOPIC)
	{
		GtkWidget *hbox, *label;
		PidginChatPane *gtkchat = gtkconv->u.chat;

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		label = gtk_label_new(_("Topic:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		gtkchat->topic_text = gtk_entry_new();
		gtk_widget_set_size_request(gtkchat->topic_text, -1, BUDDYICON_SIZE_MIN);

		if(protocol->set_chat_topic == NULL) {
			gtk_editable_set_editable(GTK_EDITABLE(gtkchat->topic_text), FALSE);
		} else {
			g_signal_connect(G_OBJECT(gtkchat->topic_text), "activate",
					G_CALLBACK(topic_callback), gtkconv);
		}

		gtk_box_pack_start(GTK_BOX(hbox), gtkchat->topic_text, TRUE, TRUE, 0);
		g_signal_connect(G_OBJECT(gtkchat->topic_text), "key_press_event",
			             G_CALLBACK(entry_key_press_cb), gtkconv);
	}
}

static gboolean
pidgin_conv_userlist_create_tooltip(GtkWidget *tipwindow, GtkTreePath *path,
		gpointer userdata, int *w, int *h)
{
	PidginConversation *gtkconv = userdata;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkconv->u.chat->list));
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleBlistNode *node;
	PurpleProtocol *protocol;
	PurpleAccount *account = purple_conversation_get_account(conv);
	char *who = NULL;

	if (purple_account_get_connection(account) == NULL)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	protocol = purple_connection_get_protocol_info(purple_account_get_connection(account));
	node = (PurpleBlistNode*)(purple_blist_find_buddy(purple_conversation_get_account(conv), who));
	if (node && protocol && (protocol->options & OPT_PROTO_UNIQUE_CHATNAME))
		pidgin_blist_draw_tooltip(node, gtkconv->infopane);

	g_free(who);
	return FALSE;
}

static void
setup_chat_userlist(PidginConversation *gtkconv, GtkWidget *hpaned)
{
	PidginChatPane *gtkchat = gtkconv->u.chat;
	GtkWidget *lbox, *list;
	GtkListStore *ls;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	int ul_width;
	void *blist_handle = purple_blist_get_handle();
	PurpleConversation *conv = gtkconv->active_conv;

	/* Build the right pane. */
	lbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, FALSE, TRUE);
	gtk_widget_show(lbox);

	/* Setup the label telling how many people are in the room. */
	gtkchat->count = gtk_label_new(_("0 people in room"));
	gtk_label_set_ellipsize(GTK_LABEL(gtkchat->count), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start(GTK_BOX(lbox), gtkchat->count, FALSE, FALSE, 0);
	gtk_widget_show(gtkchat->count);

	/* Setup the list of users. */

	ls = gtk_list_store_new(CHAT_USERS_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
							G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
							GDK_TYPE_COLOR, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(ls), CHAT_USERS_ALIAS_KEY_COLUMN,
									sort_chat_users, NULL, NULL);

	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	/* Allow a user to specify gtkrc settings for the chat userlist only */
	gtk_widget_set_name(list, "pidgin_conv_userlist");

	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(rend),
				 "stock-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL),
				 NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
			"stock-id", CHAT_USERS_ICON_STOCK_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);
	ul_width = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width");
	gtk_widget_set_size_request(lbox, ul_width, -1);

	/* Hack to prevent completely collapsed userlist coming back with a 1 pixel width.
	 * I would have liked to use the GtkPaned "max-position", but for some reason that didn't work */
	if (ul_width == 0)
		gtk_paned_set_position(GTK_PANED(hpaned), 999999);

	g_signal_connect(G_OBJECT(list), "button_press_event",
					 G_CALLBACK(right_click_chat_cb), gtkconv);
	g_signal_connect(G_OBJECT(list), "row-activated",
					 G_CALLBACK(activate_list_cb), gtkconv);
	g_signal_connect(G_OBJECT(list), "popup-menu",
			 G_CALLBACK(gtkconv_chat_popup_menu_cb), gtkconv);
	g_signal_connect(G_OBJECT(lbox), "size-allocate", G_CALLBACK(lbox_size_allocate_cb), gtkconv);

	pidgin_tooltip_setup_for_treeview(list, gtkconv,
			pidgin_conv_userlist_create_tooltip, NULL);

	rend = gtk_cell_renderer_text_new();
	g_object_set(rend,
				 "foreground-set", TRUE,
				 "weight-set", TRUE,
				 NULL);
	g_object_set(G_OBJECT(rend), "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
	                                               "text", CHAT_USERS_ALIAS_COLUMN,
	                                               "foreground-gdk", CHAT_USERS_COLOR_COLUMN,
	                                               "weight", CHAT_USERS_WEIGHT_COLUMN,
	                                               NULL);

	purple_signal_connect(blist_handle, "blist-node-added",
						gtkchat, PURPLE_CALLBACK(buddy_added_cb), conv);
	purple_signal_connect(blist_handle, "blist-node-removed",
						gtkchat, PURPLE_CALLBACK(buddy_removed_cb), conv);
	purple_signal_connect(blist_handle, "blist-node-aliased",
						gtkchat, PURPLE_CALLBACK(blist_node_aliased_cb), conv);

	gtk_tree_view_column_set_expand(col, TRUE);
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_widget_show(list);

	gtkchat->list = list;

	gtk_box_pack_start(GTK_BOX(lbox),
		pidgin_make_scrollable(list, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1),
		TRUE, TRUE, 0);
}

static gboolean
pidgin_conv_create_tooltip(GtkWidget *tipwindow, gpointer userdata, int *w, int *h)
{
	PurpleBlistNode *node = NULL;
	PurpleConversation *conv;
	PidginConversation *gtkconv = userdata;

	conv = gtkconv->active_conv;
	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		node = (PurpleBlistNode*)(purple_blist_find_chat(purple_conversation_get_account(conv), purple_conversation_get_name(conv)));
		if (!node)
			node = g_object_get_data(G_OBJECT(gtkconv->webview), "transient_chat");
	} else {
		node = (PurpleBlistNode*)(purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv)));
#if 0
		/* Using the transient blist nodes to show the tooltip doesn't quite work yet. */
		if (!node)
			node = g_object_get_data(G_OBJECT(gtkconv->webview), "transient_buddy");
#endif
	}

	if (node)
		pidgin_blist_draw_tooltip(node, gtkconv->infopane);
	return FALSE;
}

/* Quick Find {{{ */
static gboolean
pidgin_conv_end_quickfind(PidginConversation *gtkconv)
{
	gtk_widget_modify_base(gtkconv->quickfind.entry, GTK_STATE_NORMAL, NULL);

	webkit_web_view_unmark_text_matches(WEBKIT_WEB_VIEW(gtkconv->webview));
	gtk_widget_hide(gtkconv->quickfind.container);

	gtk_widget_grab_focus(gtkconv->entry);
	return TRUE;
}

static gboolean
quickfind_process_input(GtkWidget *entry, GdkEventKey *event, PidginConversation *gtkconv)
{
	switch (event->keyval) {
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			if (webkit_web_view_search_text(WEBKIT_WEB_VIEW(gtkconv->webview), gtk_entry_get_text(GTK_ENTRY(entry)), FALSE, TRUE, TRUE)) {
				gtk_widget_modify_base(gtkconv->quickfind.entry, GTK_STATE_NORMAL, NULL);
			} else {
				GdkColor col;
				col.red = 0xffff;
				col.green = 0xafff;
				col.blue = 0xafff;
				gtk_widget_modify_base(gtkconv->quickfind.entry, GTK_STATE_NORMAL, &col);
			}
			break;
		case GDK_KEY_Escape:
			pidgin_conv_end_quickfind(gtkconv);
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

static void
pidgin_conv_setup_quickfind(PidginConversation *gtkconv, GtkWidget *container)
{
	GtkWidget *widget = gtk_hbox_new(FALSE, 0);
	GtkWidget *label, *entry, *close;

	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);

	close = pidgin_create_small_button(gtk_label_new("×"));
	gtk_box_pack_start(GTK_BOX(widget), close, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(close, _("Close Find bar"));

	label = gtk_label_new(_("Find:"));
	gtk_box_pack_start(GTK_BOX(widget), label, FALSE, FALSE, 10);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(widget), entry, TRUE, TRUE, 0);

	gtkconv->quickfind.entry = entry;
	gtkconv->quickfind.container = widget;

	/* Hook to signals and stuff */
	g_signal_connect(G_OBJECT(entry), "key_press_event",
			G_CALLBACK(quickfind_process_input), gtkconv);
	g_signal_connect_swapped(G_OBJECT(close), "button-press-event",
			G_CALLBACK(pidgin_conv_end_quickfind), gtkconv);
}

/* }}} */

static char *
replace_header_tokens(PurpleConversation *conv, const char *text)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	GString *str;
	const char *cur = text;
	const char *prev = cur;
	time_t mtime;
	struct tm *tm = NULL;

	if (text == NULL || *text == '\0')
		return NULL;

	str = g_string_new(NULL);
	while ((cur = strchr(cur, '%'))) {
		char *freeval = NULL;
		const char *replace = NULL;
		const char *fin = NULL;

		if (g_str_has_prefix(cur, "%chatName%")) {
			replace = purple_conversation_get_name(conv);

		} else if (g_str_has_prefix(cur, "%sourceName%")) {
			replace = purple_account_get_private_alias(account);
			if (replace == NULL)
				replace = purple_account_get_username(account);

		} else if (g_str_has_prefix(cur, "%destinationName%")) {
			PurpleBuddy *buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
			if (buddy) {
				replace = purple_buddy_get_alias(buddy);
			} else {
				replace = purple_conversation_get_name(conv);
			}

		} else if (g_str_has_prefix(cur, "%incomingIconPath%")) {
			PurpleBuddyIcon *icon = purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(conv));
			if (icon)
				replace = purple_buddy_icon_get_full_path(icon);

		} else if (g_str_has_prefix(cur, "%outgoingIconPath%")) {
			replace = purple_account_get_buddy_icon_path(account);

		} else if (g_str_has_prefix(cur, "%timeOpened")) {
			const char *tmp = cur + strlen("%timeOpened");

			if (*tmp == '{') {
				const char *end;
				tmp++;
				end = strstr(tmp, "}%");
				if (!end) /* Invalid string */
					continue;
				if (!tm) {
					mtime = time(NULL);
					tm = localtime(&mtime);
				}
				replace = freeval = purple_uts35_to_str(tmp, end - tmp, tm);
				fin = end + 1;
			} else {
				if (!tm) {
					mtime = time(NULL);
					tm = localtime(&mtime);
				}

				replace = purple_utf8_strftime("%X", tm);
			}

		} else if (g_str_has_prefix(cur, "%dateOpened%")) {
			if (!tm) {
				mtime = time(NULL);
				tm = localtime(&mtime);
			}

			replace = purple_date_format_short(tm);

		} else {
			cur++;
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		if (replace)
			g_string_append(str, replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}
		g_free(freeval);
		freeval = NULL;
	}

	/* And wrap it up */
	g_string_append(str, prev);
	return g_string_free(str, FALSE);
}

static char *
replace_template_tokens(PidginConvTheme *theme, const char *header, const char *footer)
{
	GString *str;
	const char *text;
	char **ms;
	char *path;

	text = pidgin_conversation_theme_get_template(theme, PIDGIN_CONVERSATION_THEME_TEMPLATE_MAIN);
	if (text == NULL)
		return NULL;

	ms = g_strsplit(text, "%@", 6);
	if (ms[0] == NULL || ms[1] == NULL || ms[2] == NULL || ms[3] == NULL || ms[4] == NULL || ms[5] == NULL) {
		g_strfreev(ms);
		return NULL;
	}

	str = g_string_new(NULL);

	g_string_append(str, ms[0]);
	g_string_append(str, "file://");
	path = pidgin_conversation_theme_get_template_path(theme);
	g_string_append(str, path);
	g_free(path);

	g_string_append(str, ms[1]);

	text = pidgin_conversation_theme_get_template(theme, PIDGIN_CONVERSATION_THEME_TEMPLATE_BASESTYLE_CSS);
	g_string_append(str, text);

	g_string_append(str, ms[2]);

	g_string_append(str, "file://");
	path = pidgin_conversation_theme_get_css_path(theme);
	g_string_append(str, path);
	g_free(path);

	g_string_append(str, ms[3]);
	if (header)
		g_string_append(str, header);
	g_string_append(str, ms[4]);
	if (footer)
		g_string_append(str, footer);
	g_string_append(str, ms[5]);

	g_strfreev(ms);

	return g_string_free(str, FALSE);
}

static void
set_theme_webkit_settings(WebKitWebView *webview, PidginConvTheme *theme)
{
	WebKitWebSettings *settings;
	const GValue *val;
	
	g_object_get(G_OBJECT(webview), "settings", &settings, NULL);

	val = pidgin_conversation_theme_lookup(theme, "DefaultFontFamily", TRUE);
	if (val && G_VALUE_HOLDS_STRING(val))
	{
		const gchar *font_family = g_value_get_string(val);
#ifdef _WIN32
		/* XXX: a hack for not converting backslash to yen sign.
		 * See gtkwebview.c: gtk_webview_new.
		 */
		if (g_ascii_strcasecmp(font_family, "sans-serif") == 0)
			font_family = NULL;
#endif
		if (font_family)
			g_object_set(G_OBJECT(settings), "default-font-family", font_family, NULL);
	}

	val = pidgin_conversation_theme_lookup(theme, "DefaultFontSize", TRUE);
	if (val && G_VALUE_HOLDS_INT(val))
		g_object_set(G_OBJECT(settings), "default-font-size", GINT_TO_POINTER(g_value_get_int(val)), NULL);

	val = pidgin_conversation_theme_lookup(theme, "DefaultBackgroundIsTransparent", TRUE);
	if (val && G_VALUE_HOLDS_BOOLEAN(val))
		/* this does not work :( */
		webkit_web_view_set_transparent(webview, g_value_get_boolean(val));
}

static void
conv_variant_changed_cb(GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
	PidginConversation *gtkconv = user_data;
	char *path, *js;

	path = pidgin_conversation_theme_get_css_path(PIDGIN_CONV_THEME(gobject));
	js = g_strdup_printf("setStylesheet(\"mainStyle\", \"file://%s\");", path);
	g_free(path);
	gtk_webview_safe_execute_script(GTK_WEBVIEW(gtkconv->webview), js);
	g_free(js);
}

static void
load_conv_theme(PidginConversation *gtkconv)
{
	char *header, *footer;
	char *template;
	char *basedir, *baseuri;

	header = replace_header_tokens(gtkconv->active_conv,
		pidgin_conversation_theme_get_template(gtkconv->theme, PIDGIN_CONVERSATION_THEME_TEMPLATE_HEADER));
	footer = replace_header_tokens(gtkconv->active_conv,
		pidgin_conversation_theme_get_template(gtkconv->theme, PIDGIN_CONVERSATION_THEME_TEMPLATE_FOOTER));
	template = replace_template_tokens(gtkconv->theme, header, footer);
	g_free(header);
	g_free(footer);

	if (template == NULL)
		return;

	set_theme_webkit_settings(WEBKIT_WEB_VIEW(gtkconv->webview), gtkconv->theme);

	basedir = pidgin_conversation_theme_get_template_path(gtkconv->theme);
	baseuri = g_strdup_printf("file://%s", basedir);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(gtkconv->webview), template,
	                            "text/html", "UTF-8", baseuri);

	if (PURPLE_IS_CHAT_CONVERSATION(gtkconv->active_conv))
		gtk_webview_safe_execute_script(GTK_WEBVIEW(gtkconv->webview),
			"document.getElementById('Chat').className = 'groupchat'");

	g_signal_connect(G_OBJECT(gtkconv->theme), "notify::variant",
	                 G_CALLBACK(conv_variant_changed_cb), gtkconv);

	g_free(basedir);
	g_free(baseuri);
	g_free(template);
}

static GtkWidget *
setup_common_pane(PidginConversation *gtkconv)
{
	GtkWidget *vbox, *frame, *webview_sw, *event_box;
	GtkCellRenderer *rend;
	GtkTreePath *path;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleBuddy *buddy;
	gboolean chat = PURPLE_IS_CHAT_CONVERSATION(conv);
	int buddyicon_size = 0;

	/* Setup the top part of the pane */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(vbox);

	/* Setup the info pane */
	event_box = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
	gtk_widget_show(event_box);
	gtkconv->infopane_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), event_box, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event_box), gtkconv->infopane_hbox);
	gtk_widget_show(gtkconv->infopane_hbox);
	gtk_widget_add_events(event_box,
	                      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(event_box), "button-press-event",
	                 G_CALLBACK(infopane_press_cb), gtkconv);

	pidgin_tooltip_setup_for_widget(event_box, gtkconv,
		pidgin_conv_create_tooltip, NULL);

	gtkconv->infopane = gtk_cell_view_new();
	gtkconv->infopane_model = gtk_list_store_new(CONV_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF);
	gtk_cell_view_set_model(GTK_CELL_VIEW(gtkconv->infopane),
				GTK_TREE_MODEL(gtkconv->infopane_model));
	g_object_unref(gtkconv->infopane_model);
	gtk_list_store_append(gtkconv->infopane_model, &(gtkconv->infopane_iter));
	gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), gtkconv->infopane, TRUE, TRUE, 0);
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(gtkconv->infopane), path);
	gtk_tree_path_free(path);

	if (chat) {
		/* This empty widget is used to ensure that the infopane is consistently
		   sized for chat windows. The correct fix is to put an icon in the chat
		   window as well, because that would make "Set Custom Icon" consistent
		   for both the buddy list and the chat window, but PidginConversation
		   is pretty much stuck until 3.0. */
		GtkWidget *sizing_vbox;
		sizing_vbox = gtk_vbox_new(FALSE, 0);
		gtk_widget_set_size_request(sizing_vbox, -1, BUDDYICON_SIZE_MIN);
		gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), sizing_vbox, FALSE, FALSE, 0);
		gtk_widget_show(sizing_vbox);
	}
	else {
		gtkconv->u.im->icon_container = gtk_vbox_new(FALSE, 0);

		if ((buddy = purple_blist_find_buddy(purple_conversation_get_account(conv),
						purple_conversation_get_name(conv))) != NULL) {
			PurpleContact *contact = purple_buddy_get_contact(buddy);
			if (contact) {
				buddyicon_size = purple_blist_node_get_int((PurpleBlistNode*)contact, "pidgin-infopane-iconsize");
			}
		}
		buddyicon_size = CLAMP(buddyicon_size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);
		gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, buddyicon_size);

		gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox),
				   gtkconv->u.im->icon_container, FALSE, FALSE, 0);

		gtk_widget_show(gtkconv->u.im->icon_container);
	}

	gtk_widget_show(gtkconv->infopane);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "stock-id", CONV_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0,
			"stock-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL),
			NULL);

	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "markup", CONV_TEXT_COLUMN, NULL);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);

	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "pixbuf", CONV_PROTOCOL_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 3, "ypad", 0, NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "pixbuf", CONV_EMBLEM_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0, NULL);

	/* Setup the webkit widget */
	frame = pidgin_create_webview(FALSE, &gtkconv->webview, &webview_sw);
	gtk_widget_set_size_request(gtkconv->webview, -1, 0);

	load_conv_theme(gtkconv);

	if (chat) {
		GtkWidget *hpaned;

		/* Add the topic */
		setup_chat_topic(gtkconv, vbox);

		/* Add the gtkwebview frame */
		hpaned = gtk_hpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
		gtk_widget_show(hpaned);
		gtk_paned_pack1(GTK_PANED(hpaned), frame, TRUE, TRUE);

		/* Now add the userlist */
		setup_chat_userlist(gtkconv, hpaned);
	} else {
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	}
	gtk_widget_show_all(frame);

	gtk_widget_set_name(gtkconv->webview, "pidgin_conv_webview");
	g_object_set_data(G_OBJECT(gtkconv->webview), "gtkconv", gtkconv);

	g_signal_connect_after(G_OBJECT(gtkconv->webview), "button_press_event",
	                       G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->webview), "key_press_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->webview), "key_release_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);

	pidgin_conv_setup_quickfind(gtkconv, vbox);

	gtkconv->lower_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->lower_hbox, FALSE, FALSE, 0);
	gtk_widget_show(gtkconv->lower_hbox);

	/* Setup the entry widget and all signals */
	frame = pidgin_create_webview(TRUE, &gtkconv->entry, NULL);
	gtk_box_pack_start(GTK_BOX(gtkconv->lower_hbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	gtk_widget_set_name(gtkconv->entry, "pidgin_conv_entry");
	gtk_webview_set_protocol_name(GTK_WEBVIEW(gtkconv->entry),
			purple_account_get_protocol_name(purple_conversation_get_account(conv)));

	g_signal_connect(G_OBJECT(gtkconv->entry), "populate-popup",
	                 G_CALLBACK(entry_popup_menu_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->entry), "key-press-event",
	                 G_CALLBACK(entry_key_press_cb), gtkconv);
#if 0
	/* TODO WebKit: Why? */
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "button-press-event",
	                       G_CALLBACK(entry_stop_rclick_cb), NULL);
#endif

	if (!chat) {
		/* For sending typing notifications for IMs */
#if 0
		/* TODO WebKit */
		g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "insert_text",
						 G_CALLBACK(insert_text_cb), gtkconv);
		g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "delete_range",
						 G_CALLBACK(delete_text_cb), gtkconv);
#endif
		gtkconv->u.im->typing_timer = 0;
		gtkconv->u.im->animate = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons");
		gtkconv->u.im->show_icon = TRUE;
	}

#if 0
	/* TODO WebKit: sizing stuff? */
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry_buffer), "changed",
				 G_CALLBACK(resize_webview_cb), gtkconv);
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry), "size-allocate",
				 G_CALLBACK(resize_webview_cb), gtkconv);
#endif

	default_formatize(gtkconv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "format-cleared",
	                       G_CALLBACK(clear_formatting_cb), gtkconv);
	return vbox;
}

static void
conv_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
              GtkSelectionData *sd, guint info, guint t,
              PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginWindow *win = gtkconv->win;
	PurpleIMConversation *im;
	PurpleAccount *convaccount = purple_conversation_get_account(conv);
	PurpleConnection *gc = purple_account_get_connection(convaccount);
	PurpleProtocol *protocol = gc ? purple_connection_get_protocol_info(gc) : NULL;
	const guchar *data = gtk_selection_data_get_data(sd);

	if (info == PIDGIN_DRAG_BLIST_NODE)
	{
		PurpleBlistNode *n = NULL;
		PurpleBuddy *b;
		PidginConversation *gtkconv = NULL;
		PurpleAccount *buddyaccount;
		const char *buddyname;

		n = *(PurpleBlistNode **) data;

		if (PURPLE_IS_CONTACT(n))
			b = purple_contact_get_priority_buddy((PurpleContact*)n);
		else if (PURPLE_IS_BUDDY(n))
			b = (PurpleBuddy*)n;
		else
			return;

		buddyaccount = purple_buddy_get_account(b);
		buddyname = purple_buddy_get_name(b);
		/*
		 * If a buddy is dragged to a chat window of the same protocol,
		 * invite him to the chat.
		 */
		if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
				protocol && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(protocol, chat_invite) &&
				strcmp(purple_account_get_protocol_id(convaccount),
					purple_account_get_protocol_id(buddyaccount)) == 0) {
		    purple_chat_conversation_invite_user(PURPLE_CHAT_CONVERSATION(conv), buddyname, NULL, TRUE);
		} else {
			/*
			 * If we already have an open conversation with this buddy, then
			 * just move the conv to this window.  Otherwise, create a new
			 * conv and add it to this window.
			 */
			im = purple_conversations_find_im_with_account(buddyname, buddyaccount);
			if (im != NULL) {
				PidginWindow *oldwin;
				gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
				oldwin = gtkconv->win;
				if (oldwin != win) {
					pidgin_conv_window_remove_gtkconv(oldwin, gtkconv);
					pidgin_conv_window_add_gtkconv(win, gtkconv);
				}
			} else {
				im = purple_im_conversation_new(buddyaccount, buddyname);
				gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
				if (gtkconv->win != win) {
					pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
					pidgin_conv_window_add_gtkconv(win, gtkconv);
				}
			}

			/* Make this conversation the active conversation */
			pidgin_conv_window_switch_gtkconv(win, gtkconv);
		}

		gtk_drag_finish(dc, TRUE,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
	else if (info == PIDGIN_DRAG_IM_CONTACT)
	{
		char *protocol_id = NULL;
		char *username = NULL;
		PurpleAccount *account;
		PidginConversation *gtkconv;

		if (pidgin_parse_x_im_contact((const char *) data, FALSE, &account,
						&protocol_id, &username, NULL))
		{
			if (account == NULL)
			{
				purple_notify_error(win, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL);
			} else {
				/*
				 * If a buddy is dragged to a chat window of the same protocol,
				 * invite him to the chat.
				 */
				if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
						protocol && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(protocol, chat_invite) &&
						strcmp(purple_account_get_protocol_id(convaccount), protocol_id) == 0) {
					purple_chat_conversation_invite_user(PURPLE_CHAT_CONVERSATION(conv), username, NULL, TRUE);
				} else {
					im = purple_im_conversation_new(account, username);
					gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
					if (gtkconv->win != win) {
						pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
						pidgin_conv_window_add_gtkconv(win, gtkconv);
					}
				}
			}
		}

		g_free(username);
		g_free(protocol_id);

		gtk_drag_finish(dc, TRUE,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
	else if (info == WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST) {
		if (PURPLE_IS_IM_CONVERSATION(conv))
			pidgin_dnd_file_manage(sd, convaccount, purple_conversation_get_name(conv));
		gtk_drag_finish(dc, TRUE,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
	else
		gtk_drag_finish(dc, FALSE, FALSE, t);
}


static PidginConversation *
pidgin_conv_find_gtkconv(PurpleConversation * conv)
{
	PurpleBuddy *bud = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
	PurpleContact *c;
	PurpleBlistNode *cn, *bn;

	if (!bud)
		return NULL;

	if (!(c = purple_buddy_get_contact(bud)))
		return NULL;

	cn = PURPLE_BLIST_NODE(c);
	for (bn = purple_blist_node_get_first_child(cn); bn; bn = purple_blist_node_get_sibling_next(bn)) {
		PurpleBuddy *b = PURPLE_BUDDY(bn);
		PurpleIMConversation *im;
		if ((im = purple_conversations_find_im_with_account(purple_buddy_get_name(b), purple_buddy_get_account(b)))) {
			if (PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im)))
				return PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
		}
	}

	return NULL;
}

static void
buddy_update_cb(PurpleBlistNode *bnode, gpointer null)
{
	GList *list;

	g_return_if_fail(bnode);
	if (!PURPLE_IS_BUDDY(bnode))
		return;

	for (list = pidgin_conv_windows_get_list(); list; list = list->next)
	{
		PidginWindow *win = list->data;
		PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

		if (PURPLE_IS_CHAT_CONVERSATION(conv))
			continue;

		pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}
}

static gboolean
ignore_middle_click(GtkWidget *widget, GdkEventButton *e, gpointer null)
{
	/* A click on the pane is propagated to the notebook containing the pane.
	 * So if Stu accidentally aims high and middle clicks on the pane-handle,
	 * it causes a conversation tab to close. Let's stop that from happening.
	 */
	if (e->button == 2 && e->type == GDK_BUTTON_PRESS)
		return TRUE;
	return FALSE;
}

static void set_typing_font(GtkWidget *widget, GtkStyle *style, PidginConversation *gtkconv)
{
/* TODO WEBKIT */
#if 0
	static PangoFontDescription *font_desc = NULL;
	static GdkColor *color = NULL;
	static gboolean enable = TRUE;

	if (font_desc == NULL) {
		char *string = NULL;
		gtk_widget_style_get(widget,
				"typing-notification-font", &string,
				"typing-notification-color", &color,
				"typing-notification-enable", &enable,
				NULL);
		font_desc = pango_font_description_from_string(string);
		g_free(string);
		if (color == NULL) {
			GdkColor def = {0, 0x8888, 0x8888, 0x8888};
			color = gdk_color_copy(&def);
		}
	}

	gtk_text_buffer_create_tag(GTK_IMHTML(widget)->text_buffer, "TYPING-NOTIFICATION",
			"foreground-gdk", color,
			"font-desc", font_desc,
			NULL);

	if (!enable) {
		g_object_set_data(G_OBJECT(widget), "disable-typing-notification", GINT_TO_POINTER(TRUE));
		/* or may be 'gtkconv->disable_typing = TRUE;' instead? */
	}

	g_signal_handlers_disconnect_by_func(G_OBJECT(widget), set_typing_font, gtkconv);
#endif /* if 0 */
}

/**************************************************************************
 * Conversation UI operations
 **************************************************************************/
static void
private_gtkconv_new(PurpleConversation *conv, gboolean hidden)
{
	PidginConversation *gtkconv;
	const char *theme_name;
	PurpleTheme *theme = NULL;
	gboolean is_chat = PURPLE_IS_CHAT_CONVERSATION(conv);
	GtkWidget *pane = NULL;
	GtkWidget *tab_cont;
	PurpleBlistNode *convnode;
	GtkTargetList *targets;

	if (!is_chat && (gtkconv = pidgin_conv_find_gtkconv(conv))) {
		purple_conversation_set_ui_data(conv, gtkconv);
		if (!g_list_find(gtkconv->convs, conv))
			gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
		pidgin_conv_switch_active_conversation(conv);
		return;
	}

	gtkconv = g_new0(PidginConversation, 1);
	purple_conversation_set_ui_data(conv, gtkconv);
	gtkconv->active_conv = conv;
	gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
	gtkconv->send_history = g_list_append(NULL, NULL);

	/* Setup some initial variables. */
	gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	gtkconv->unseen_count = 0;
	theme_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/theme");
	if (theme_name && *theme_name)
		theme = purple_theme_manager_find_theme(theme_name, "conversation");
	if (!theme)
		theme = default_conv_theme;
	gtkconv->theme = PIDGIN_CONV_THEME(g_object_ref(theme));
	gtkconv->last_flags = 0;


	if (!is_chat) {
		gtkconv->u.im = g_malloc0(sizeof(PidginImPane));
	} else {
		gtkconv->u.chat = g_malloc0(sizeof(PidginChatPane));
	}
	pane = setup_common_pane(gtkconv);

	if (pane == NULL) {
		if (is_chat)
			g_free(gtkconv->u.chat);
		else
			g_free(gtkconv->u.im);

		g_free(gtkconv);
		purple_conversation_set_ui_data(conv, NULL);
		return;
	}

	/* Setup drag-and-drop */
	gtk_drag_dest_set(pane, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
	                  NULL, 0, GDK_ACTION_COPY);
	targets = gtk_target_list_new(dnd_targets, G_N_ELEMENTS(dnd_targets));
	gtk_target_list_add(targets, gdk_atom_intern("text/uri-list", FALSE), 0,
	                    WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST);
	gtk_drag_dest_set_target_list(pane, targets);

	if (webkit_dnd_targets) {
		targets = webkit_dnd_targets;
	} else {
		GtkTargetEntry *entries;
		gint count;

		targets = webkit_web_view_get_paste_target_list(WEBKIT_WEB_VIEW(gtkconv->webview));
		entries = gtk_target_table_new_from_list(targets, &count);
		targets = gtk_target_list_new(entries, count);
		gtk_target_table_free(entries, count);

		gtk_target_list_add_table(targets, dnd_targets, G_N_ELEMENTS(dnd_targets));
		webkit_dnd_targets = targets;
	}

	gtk_drag_dest_set(gtkconv->webview, 0, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_set_target_list(gtkconv->webview, targets);

	gtk_drag_dest_set(gtkconv->entry, 0, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_set_target_list(gtkconv->entry, targets);

	g_signal_connect(G_OBJECT(pane), "button_press_event",
	                 G_CALLBACK(ignore_middle_click), NULL);
	g_signal_connect(G_OBJECT(pane), "drag-data-received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);
#if 0
	/* FIXME: WebKit confuses the dnd source when this is enabled */
	g_signal_connect(G_OBJECT(gtkconv->webview), "drag-data-received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->entry), "drag-data-received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);
#endif

	g_signal_connect(gtkconv->webview, "style-set", G_CALLBACK(set_typing_font), gtkconv);

	/* Setup the container for the tab. */
	gtkconv->tab_cont = tab_cont = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	g_object_set_data(G_OBJECT(tab_cont), "PidginConversation", gtkconv);
	gtk_container_set_border_width(GTK_CONTAINER(tab_cont), PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(tab_cont), pane);
	gtk_widget_show(pane);

	convnode = get_conversation_blist_node(conv);
	if (convnode == NULL || !purple_blist_node_get_bool(convnode, "gtk-mute-sound"))
		gtkconv->make_sound = TRUE;

	if (convnode != NULL && purple_blist_node_has_setting(convnode, "enable-logging")) {
		gboolean logging = purple_blist_node_get_bool(convnode, "enable-logging");
		purple_conversation_set_logging(conv, logging);
	}

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar"))
		gtk_webview_show_toolbar(GTK_WEBVIEW(gtkconv->entry));
	else
		gtk_webview_hide_toolbar(GTK_WEBVIEW(gtkconv->entry));

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		gtk_widget_show(gtkconv->infopane_hbox);
	else
		gtk_widget_hide(gtkconv->infopane_hbox);


	g_signal_connect_swapped(G_OBJECT(pane), "focus",
	                         G_CALLBACK(gtk_widget_grab_focus),
	                         gtkconv->entry);

	if (hidden)
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
	else
		pidgin_conv_placement_place(gtkconv);

	if (generated_nick_colors == NULL) {
		generated_nick_colors = generate_nick_colors(NICK_COLOR_GENERATE_COUNT, gtk_widget_get_style(gtkconv->webview)->base[GTK_STATE_NORMAL]);
	}

	if(NULL == (gtkconv->nick_colors = pidgin_conversation_theme_get_nick_colors(gtkconv->theme)))
	{
		gtkconv->nick_colors = g_array_ref(generated_nick_colors);
	}

	if (purple_conversation_get_features(conv) & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY)
		pidgin_themes_smiley_themeize_custom(gtkconv->entry);
}

static void
pidgin_conv_new_hidden(PurpleConversation *conv)
{
	private_gtkconv_new(conv, TRUE);
}

void
pidgin_conv_new(PurpleConversation *conv)
{
	private_gtkconv_new(conv, FALSE);
	if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", PIDGIN_CONVERSATION(conv));
}

static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags)
{
	PurpleConversationUiOps *ui_ops = pidgin_conversations_get_conv_ui_ops();
	gboolean hide = FALSE;
	guint timer;

	/* create hidden conv if hide_new pref is always */
	if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always") == 0)
		hide = TRUE;

	/* create hidden conv if hide_new pref is away and account is away */
	if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away") == 0 &&
	    !purple_status_is_available(purple_account_get_active_status(account)))
		hide = TRUE;

	if (conv && PIDGIN_IS_PIDGIN_CONVERSATION(conv) && !hide) {
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv->win == hidden_convwin) {
			pidgin_conv_attach_to_conversation(gtkconv->active_conv);
		}
		return;
	}

	if (hide) {
		ui_ops->create_conversation = pidgin_conv_new_hidden;
		purple_im_conversation_new(account, sender);
		ui_ops->create_conversation = pidgin_conv_new;
	}

	/* Somebody wants to keep this conversation around, so don't time it out */
	if (conv) {
		timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
		if (timer) {
			purple_timeout_remove(timer);
			g_object_set_data(G_OBJECT(conv), "close-timer", GINT_TO_POINTER(0));
		}
	}
}

static void
pidgin_conv_destroy(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->convs = g_list_remove(gtkconv->convs, conv);
	/* Don't destroy ourselves until all our convos are gone */
	if (gtkconv->convs) {
		/* Make sure the destroyed conversation is not the active one */
		if (gtkconv->active_conv == conv) {
			gtkconv->active_conv = gtkconv->convs->data;
			purple_conversation_update(gtkconv->active_conv, PURPLE_CONVERSATION_UPDATE_FEATURES);
		}
		return;
	}

	pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);

	/* If the "Save Conversation" or "Save Icon" dialogs are open then close them */
	purple_request_close_with_handle(gtkconv);
	purple_notify_close_with_handle(gtkconv);

	gtk_widget_destroy(gtkconv->tab_cont);
	g_object_unref(gtkconv->tab_cont);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (gtkconv->u.im->icon_timer != 0)
			g_source_remove(gtkconv->u.im->icon_timer);

		if (gtkconv->u.im->anim != NULL)
			g_object_unref(G_OBJECT(gtkconv->u.im->anim));

		if (gtkconv->u.im->typing_timer != 0)
			g_source_remove(gtkconv->u.im->typing_timer);

		g_free(gtkconv->u.im);
	} else {
		purple_signals_disconnect_by_handle(gtkconv->u.chat);
		g_free(gtkconv->u.chat);
	}

	gtkconv->send_history = g_list_first(gtkconv->send_history);
	g_list_foreach(gtkconv->send_history, (GFunc)g_free, NULL);
	g_list_free(gtkconv->send_history);

	if (gtkconv->attach.timer) {
		g_source_remove(gtkconv->attach.timer);
	}

	g_array_unref(gtkconv->nick_colors);

	g_object_disconnect(G_OBJECT(gtkconv->theme), "any_signal::notify",
	                    conv_variant_changed_cb, gtkconv, NULL);
	g_object_unref(gtkconv->theme);

	g_free(gtkconv);
}


static void
pidgin_conv_write_im(PurpleIMConversation *im, const char *who,
					  const char *message, PurpleMessageFlags flags,
					  time_t mtime)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv = PURPLE_CONVERSATION(im);

	gtkconv = PIDGIN_CONVERSATION(conv);

	if (conv != gtkconv->active_conv &&
	    flags & PURPLE_MESSAGE_ACTIVE_ONLY)
	{
		/* Plugins that want these messages suppressed should be
		 * calling purple_im_conversation_write_message(), so they get suppressed here,
		 * before being written to the log. */
		purple_debug_info("gtkconv",
		                "Suppressing message for an inactive conversation in pidgin_conv_write_im()\n");
		return;
	}

	purple_conversation_write(conv, who, message, flags, mtime);
}

#if 0
static const char *
get_text_tag_color(GtkTextTag *tag)
{
	GdkColor *color = NULL;
	gboolean set = FALSE;
	static char colcode[] = "#XXXXXX";
	if (tag)
		g_object_get(G_OBJECT(tag), "foreground-set", &set, "foreground-gdk", &color, NULL);
	if (set && color)
		g_snprintf(colcode, sizeof(colcode), "#%02x%02x%02x",
				color->red >> 8, color->green >> 8, color->blue >> 8);
	else
		colcode[0] = '\0';
	if (color)
		gdk_color_free(color);
	return colcode;
}

/* The callback for an event on a link tag. */
static gboolean buddytag_event(GtkTextTag *tag, GObject *imhtml,
		GdkEvent *event, GtkTextIter *arg2, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS
			|| event->type == GDK_2BUTTON_PRESS) {
		GdkEventButton *btn_event = (GdkEventButton*) event;
		PurpleConversation *conv = data;
		char *buddyname;
		gchar *name;

		g_object_get(G_OBJECT(tag), "name", &name, NULL);

		/* strlen("BUDDY " or "HILIT ") == 6 */
		g_return_val_if_fail((name != NULL) && (strlen(name) > 6), FALSE);

		buddyname = name + 6;

		/* emit chat-nick-clicked signal */
		if (event->type == GDK_BUTTON_PRESS) {
			gint plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
						pidgin_conversations_get_handle(), "chat-nick-clicked",
						data, buddyname, btn_event->button));
			if (plugin_return) {
				g_free(name);
				return TRUE;
			}
		}

		if (btn_event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
			chat_do_im(PIDGIN_CONVERSATION(conv), buddyname);
			g_free(name);

			return TRUE;
		} else if (btn_event->button == 2 && event->type == GDK_2BUTTON_PRESS) {
			chat_do_info(PIDGIN_CONVERSATION(conv), buddyname);
			g_free(name);

			return TRUE;
		} else if (btn_event->button == 3 && event->type == GDK_BUTTON_PRESS) {
			GtkTextIter start, end;

			/* we shouldn't display the popup
			 * if the user has selected something: */
			if (!gtk_text_buffer_get_selection_bounds(
						gtk_text_iter_get_buffer(arg2),
						&start, &end)) {
				GtkWidget *menu = NULL;
				PurpleConnection *gc =
					purple_conversation_get_connection(conv);


				menu = create_chat_menu(conv, buddyname, gc);
				gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
						NULL, GTK_WIDGET(imhtml),
						btn_event->button,
						btn_event->time);

				g_free(name);

				/* Don't propagate the event any further */
				return TRUE;
			}
		}

		g_free(name);
	}

	return FALSE;
}
#endif

static GtkTextTag *get_buddy_tag(PurpleChatConversation *chat, const char *who, PurpleMessageFlags flag,
		gboolean create)
{
/* TODO WEBKIT */
#if 0
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	GtkTextTag *buddytag;
	gchar *str;
	gboolean highlight = (flag & PURPLE_MESSAGE_NICK);
	GtkTextBuffer *buffer = GTK_IMHTML(gtkconv->imhtml)->text_buffer;

	str = g_strdup_printf(highlight ? "HILIT %s" : "BUDDY %s", who);

	buddytag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(buffer), str);

	if (buddytag == NULL && create) {
		if (highlight)
			buddytag = gtk_text_buffer_create_tag(buffer, str,
					"foreground", get_text_tag_color(gtk_text_tag_table_lookup(
							gtk_text_buffer_get_tag_table(buffer), "highlight-name")),
					"weight", PANGO_WEIGHT_BOLD,
					NULL);
		else
			buddytag = gtk_text_buffer_create_tag(
					buffer, str,
					"foreground-gdk", get_nick_color(gtkconv, who),
					"weight", purple_blist_find_buddy(purple_conversation_get_account(conv), who) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
					NULL);

		g_object_set_data(G_OBJECT(buddytag), "cursor", "");
		g_signal_connect(G_OBJECT(buddytag), "event",
				G_CALLBACK(buddytag_event), conv);
	}

	g_free(str);

	return buddytag;
#endif /* if 0 */
	return NULL;
}

#if 0
static void pidgin_conv_calculate_newday(PidginConversation *gtkconv, time_t mtime)
{
	struct tm *tm = localtime(&mtime);

	tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
	tm->tm_mday++;

	gtkconv->newday = mktime(tm);
}

/* Detect string direction and encapsulate the string in RLE/LRE/PDF unicode characters
   str - pointer to string (string is re-allocated and the pointer updated) */
static void
str_embed_direction_chars(char **str)
{
#ifdef HAVE_PANGO14
	char pre_str[4];
	char post_str[10];
	char *ret;

	if (PANGO_DIRECTION_RTL == pango_find_base_dir(*str, -1))
	{
		sprintf(pre_str, "%c%c%c",
				0xE2, 0x80, 0xAB);	/* RLE */
		sprintf(post_str, "%c%c%c%c%c%c%c%c%c",
				0xE2, 0x80, 0xAC,	/* PDF */
				0xE2, 0x80, 0x8E,	/* LRM */
				0xE2, 0x80, 0xAC);	/* PDF */
	}
	else
	{
		sprintf(pre_str, "%c%c%c",
				0xE2, 0x80, 0xAA);	/* LRE */
		sprintf(post_str, "%c%c%c%c%c%c%c%c%c",
				0xE2, 0x80, 0xAC,	/* PDF */
				0xE2, 0x80, 0x8F,	/* RLM */
				0xE2, 0x80, 0xAC);	/* PDF */
	}

	ret = g_strconcat(pre_str, *str, post_str, NULL);

	g_free(*str);
	*str = ret;
#endif
}
#endif

static char *
replace_message_tokens(
	const char *text,
	PurpleConversation *conv,
	const char *name,
	const char *alias,
	const char *message,
	PurpleMessageFlags flags,
	time_t mtime)
{
	GString *str;
	const char *cur = text;
	const char *prev = cur;
	struct tm *tm = NULL;

	if (text == NULL || *text == '\0')
		return NULL;

	str = g_string_new(NULL);
	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		const char *fin = NULL;
		gpointer freeval = NULL;

		if (g_str_has_prefix(cur, "%message%")) {
			replace = message;

		} else if (g_str_has_prefix(cur, "%messageClasses%")) {
			char *user;
			GString *classes = g_string_new(NULL);
#define ADD_CLASS(f, class) \
			if (flags & f) \
				g_string_append(classes, class);
			ADD_CLASS(PURPLE_MESSAGE_SEND, "outgoing ");
			ADD_CLASS(PURPLE_MESSAGE_RECV, "incoming ");
			ADD_CLASS(PURPLE_MESSAGE_SYSTEM, "event ");
			ADD_CLASS(PURPLE_MESSAGE_AUTO_RESP, "autoreply ");
			ADD_CLASS(PURPLE_MESSAGE_DELAYED, "history ");
			ADD_CLASS(PURPLE_MESSAGE_NICK, "mention ");
#undef ADD_CLASS
			user = get_class_for_user(name);
			g_string_append(classes, user);
			g_free(user);

			replace = freeval = g_string_free(classes, FALSE);

		} else if (g_str_has_prefix(cur, "%time")) {
			const char *tmp = cur + strlen("%time");

			if (*tmp == '{') {
				char *end;
				tmp++;
				end = strstr(tmp, "}%");
				if (!end) /* Invalid string */
					continue;
				if (!tm)
					tm = localtime(&mtime);
				replace = freeval = purple_uts35_to_str(tmp, end - tmp, tm);
				fin = end + 1;
			} else {
				if (!tm)
					tm = localtime(&mtime);

				replace = purple_utf8_strftime("%X", tm);
			}

		} else if (g_str_has_prefix(cur, "%shortTime%")) {
			if (!tm)
				tm = localtime(&mtime);

			replace = purple_utf8_strftime("%H:%M", tm);

		} else if (g_str_has_prefix(cur, "%userIconPath%")) {
			if (flags & PURPLE_MESSAGE_SEND) {
				if (purple_account_get_bool(purple_conversation_get_account(conv), "use-global-buddyicon", TRUE)) {
					replace = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon");
				} else {
					PurpleStoredImage *img = purple_buddy_icons_find_account_icon(purple_conversation_get_account(conv));
					replace = purple_imgstore_get_filename(img);
				}
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = freeval = g_build_filename("Outgoing", "buddy_icon.png", NULL);
				}
			} else if (flags & PURPLE_MESSAGE_RECV) {
				PurpleBuddyIcon *icon = purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(conv));
				if (icon)
					replace = purple_buddy_icon_get_full_path(icon);
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = freeval = g_build_filename("Incoming", "buddy_icon.png", NULL);
				}
			}

		} else if (g_str_has_prefix(cur, "%senderScreenName%")) {
			replace = name;

		} else if (g_str_has_prefix(cur, "%sender%")) {
			replace = alias;

		} else if (g_str_has_prefix(cur, "%senderColor%")) {
			const GdkColor *color = get_nick_color(PIDGIN_CONVERSATION(conv), name);
			replace = freeval = g_strdup_printf("#%02x%02x%02x", (color->red >> 8), (color->green >> 8), (color->blue >> 8));

		} else if (g_str_has_prefix(cur, "%service%")) {
			replace = purple_account_get_protocol_name(purple_conversation_get_account(conv));

		} else if (g_str_has_prefix(cur, "%messageDirection%")) {
			replace = purple_markup_is_rtl(message) ? "rtl" : "ltr";

		} else if (g_str_has_prefix(cur, "%status%")) {
			GString *classes = g_string_new(NULL);

			if (flags & PURPLE_MESSAGE_ERROR)
				g_string_append(classes, "error ");

			replace = freeval = g_string_free(classes, FALSE);

		} else if (g_str_has_prefix(cur, "%variant%")) {
			replace = pidgin_conversation_theme_get_variant(PIDGIN_CONVERSATION(conv)->theme);
			replace = freeval = g_strdup(replace);
			purple_util_chrreplace(freeval, ' ', '_');

		} else {
			cur++;
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		if (replace)
			g_string_append(str, replace);
		g_free(freeval);
		replace = freeval = NULL;

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}

	}

	/* And wrap it up */
	g_string_append(str, prev);

	return g_string_free(str, FALSE);
}

static void
pidgin_conv_write_conv(PurpleConversation *conv, const char *name, const char *alias,
						const char *message, PurpleMessageFlags flags,
						time_t mtime)
{
	PidginConversation *gtkconv;
	PurpleConnection *gc;
	PurpleAccount *account;
#if 0
	int gtk_font_options = 0;
	int gtk_font_options_all = 0;
	char buf2[BUF_LONG];
	gboolean show_date;
	char *mdate;
	char *str;
	char *with_font_tag;
	char *sml_attrib = NULL;
	size_t length;
#endif
	char *displaying;
	gboolean plugin_return;
#if 0
	gboolean is_rtl_message = FALSE;
#endif

	const char *message_html;
	char *msg;
	char *escape;
	char *script;
	char *smileyed;
	PurpleMessageFlags old_flags;
	const char *func = "appendMessage";

	g_return_if_fail(conv != NULL);
	gtkconv = PIDGIN_CONVERSATION(conv);
	g_return_if_fail(gtkconv != NULL);

	if (gtkconv->attach.timer) {
		/* We are currently in the process of filling up the buffer with the message
		 * history of the conversation. So we do not need to add the message here.
		 * Instead, this message will be added to the message-list, which in turn will
		 * be processed and displayed by the attach-callback.
		 */
		return;
	}

	if (conv != gtkconv->active_conv)
	{
		if (flags & PURPLE_MESSAGE_ACTIVE_ONLY)
		{
			/* Unless this had PURPLE_MESSAGE_NO_LOG, this message
			 * was logged.  Plugin writers: if this isn't what
			 * you wanted, call purple_im_conversation_write_message() instead of
			 * purple_conversation_write(). */
			purple_debug_info("gtkconv",
			                "Suppressing message for an inactive conversation in pidgin_conv_write_conv()\n");
			return;
		}

		/* Set the active conversation to the one that just messaged us. */
		/* TODO: consider not doing this if the account is offline or something */
		if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV))
			pidgin_conv_switch_active_conversation(conv);
	}

	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);
	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL || !(flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)));

	/* Make sure URLs are clickable */
	if(flags & PURPLE_MESSAGE_NO_LINKIFY)
		displaying = g_strdup(message);
	else
		displaying = purple_markup_linkify(message);

	plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
							pidgin_conversations_get_handle(), (PURPLE_IS_IM_CONVERSATION(conv) ?
							"displaying-im-msg" : "displaying-chat-msg"),
							account, name, &displaying, conv, flags));
	if (plugin_return)
	{
		g_free(displaying);
		return;
	}
#if 0
	length = strlen(displaying) + 1;
#endif

	old_flags = gtkconv->last_flags;
	if ((flags & PURPLE_MESSAGE_SEND) && (old_flags & PURPLE_MESSAGE_SEND)) {
		message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
			PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_NEXT_CONTENT);
		func = "appendNextMessage";

	} else if (flags & PURPLE_MESSAGE_SEND) {
		message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
			PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_CONTENT);

	} else if ((flags & PURPLE_MESSAGE_RECV) && (old_flags & PURPLE_MESSAGE_RECV)) {
		GList *history = purple_conversation_get_message_history(gtkconv->last_conversed);
		PurpleConversationMessage *last_msg = history ? (PurpleConversationMessage *)history->data : NULL;

		g_assert(history != NULL);
		g_assert(last_msg != NULL);

		/* If the senders are the same, use appendNextMessage */
		if (purple_strequal(purple_conversation_message_get_sender(last_msg), name)) {
			message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
				PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_NEXT_CONTENT);
			func = "appendNextMessage";
		} else {
			message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
				PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTENT);
		}
	} else if (flags & PURPLE_MESSAGE_RECV) {
		message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
			PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTENT);

	} else {
		message_html = pidgin_conversation_theme_get_template(gtkconv->theme,
			PIDGIN_CONVERSATION_THEME_TEMPLATE_STATUS);
	}
	gtkconv->last_flags = flags;
	gtkconv->last_conversed = conv;

	smileyed = smiley_parse_markup(displaying, purple_account_get_protocol_id(account));
	msg = replace_message_tokens(message_html, conv, name, alias, smileyed, flags, mtime);
	escape = gtk_webview_quote_js_string(msg ? msg : "");
	script = g_strdup_printf("%s(%s)", func, escape);

	purple_debug_info("webkit", "JS: %s\n", script);
	gtk_webview_safe_execute_script(GTK_WEBVIEW(gtkconv->webview), script);

	g_free(script);
	g_free(smileyed);
	g_free(msg);
	g_free(escape);

#if 0
	/* if the buffer is not empty add a <br> */
	if (!gtk_webview_is_empty(GTK_WEBVIEW(gtkconv->webview)))
		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), "<br />");

	/* First message in a conversation. */
	if (gtkconv->newday == 0)
		pidgin_conv_calculate_newday(gtkconv, mtime);

	/* Show the date on the first message in a new day, or if the message is
	 * older than 20 minutes. */
	show_date = (mtime >= gtkconv->newday) || (time(NULL) > mtime + 20*60);

	mdate = purple_signal_emit_return_1(pidgin_conversations_get_handle(),
	                                  "conversation-timestamp",
	                                  conv, mtime, show_date);

	if (mdate == NULL)
	{
		struct tm *tm = localtime(&mtime);
		const char *tmp;
		if (show_date)
			tmp = purple_date_format_long(tm);
		else
			tmp = purple_time_format(tm);
		mdate = g_strdup_printf("(%s)", tmp);
	}

	/* Bi-Directional support - set timestamp direction using unicode characters */
	is_rtl_message = purple_markup_is_rtl(message);
	/* Enforce direction only if message is RTL - doesn't effect LTR users */
	if (is_rtl_message)
		str_embed_direction_chars(&mdate);

	if (mtime >= gtkconv->newday)
		pidgin_conv_calculate_newday(gtkconv, mtime);

	sml_attrib = g_strdup_printf("sml=\"%s\"", purple_account_get_protocol_name(account));

	gtk_font_options |= GTK_IMHTML_NO_COMMENTS;

	if ((flags & PURPLE_MESSAGE_RECV) &&
			!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting"))
		gtk_font_options |= GTK_IMHTML_NO_COLOURS | GTK_IMHTML_NO_FONTS | GTK_IMHTML_NO_SIZES | GTK_IMHTML_NO_FORMATTING;

	/* this is gonna crash one day, I can feel it. */
	if (purple_find_protocol_info(purple_account_get_protocol_id(purple_conversation_get_account(conv)))->options &
	    OPT_PROTO_USE_POINTSIZE) {
		gtk_font_options |= GTK_IMHTML_USE_POINTSIZE;
	}

	if (!(flags & PURPLE_MESSAGE_RECV) && (purple_conversation_get_features(conv) & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY))
	{
		/* We want to see our own smileys. Need to revert it after send*/
		pidgin_themes_smiley_themeize_custom(gtkconv->webview);
	}

	/* TODO: These colors should not be hardcoded so log.c can use them */
	if (flags & PURPLE_MESSAGE_RAW) {
		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), message);
	} else if (flags & PURPLE_MESSAGE_SYSTEM) {
		g_snprintf(buf2, sizeof(buf2),
			   "<font %s><font size=\"2\"><span class='timestamp'>%s</span></font><b>%s</b></font>",
			   sml_attrib ? sml_attrib : "", mdate, displaying);

		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), buf2);

	} else if (flags & PURPLE_MESSAGE_ERROR) {
		g_snprintf(buf2, sizeof(buf2),
			   "<font color=\"#ff0000\"><font %s><font size=\"2\"><span class='timestamp'>%s</span> </font><b>%s</b></font></font>",
			   sml_attrib ? sml_attrib : "", mdate, displaying);

		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), buf2);

	} else if (flags & PURPLE_MESSAGE_NO_LOG) {
		g_snprintf(buf2, BUF_LONG,
			   "<b><font %s color=\"#777777\">%s</font></b>",
			   sml_attrib ? sml_attrib : "", displaying);

		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), buf2);
	} else {
		char *new_message = g_memdup(displaying, length);
		char *alias_escaped = (alias ? g_markup_escape_text(alias, strlen(alias)) : g_strdup(""));
		/* The initial offset is to deal with
		 * escaped entities making the string longer */
		int tag_start_offset = 0;
		int tag_end_offset = 0;
		const char *tagname = NULL;

		/* Enforce direction on alias */
		if (is_rtl_message)
			str_embed_direction_chars(&alias_escaped);

		str = g_malloc(1024);
		if (flags & PURPLE_MESSAGE_WHISPER) {
			/* If we're whispering, it's not an autoresponse. */
			if (purple_message_meify(new_message, -1 )) {
				g_snprintf(str, 1024, "***%s", alias_escaped);
				tag_start_offset += 3;
				tagname = "whisper-action-name";
			}
			else {
				g_snprintf(str, 1024, "*%s*:", alias_escaped);
				tag_start_offset += 1;
				tag_end_offset = 2;
				tagname = "whisper-name";
			}
		} else {
			if (purple_message_meify(new_message, -1)) {
				if (flags & PURPLE_MESSAGE_AUTO_RESP) {
					g_snprintf(str, 1024, "%s ***%s", AUTO_RESPONSE, alias_escaped);
					tag_start_offset += strlen(AUTO_RESPONSE) - 6 + 4;
				} else {
					g_snprintf(str, 1024, "***%s", alias_escaped);
					tag_start_offset += 3;
				}

				if (flags & PURPLE_MESSAGE_NICK)
					tagname = "highlight-name";
				else
					tagname = "action-name";
			} else {
				if (flags & PURPLE_MESSAGE_AUTO_RESP) {
					g_snprintf(str, 1024, "%s %s", alias_escaped, AUTO_RESPONSE);
					tag_start_offset += strlen(AUTO_RESPONSE) - 6 + 1;
				} else {
					g_snprintf(str, 1024, "%s:", alias_escaped);
					tag_end_offset = 1;
				}

				if (flags & PURPLE_MESSAGE_NICK) {
					if (type == PURPLE_CONV_TYPE_IM) {
						tagname = "highlight-name";
					}
				} else if (flags & PURPLE_MESSAGE_RECV) {
					/* The tagname for chats is handled by get_buddy_tag */
					if (type == PURPLE_CONV_TYPE_IM) {
						tagname = "receive-name";
					}
				} else if (flags & PURPLE_MESSAGE_SEND) {
					tagname = "send-name";
				} else {
					purple_debug_error("gtkconv", "message missing flags\n");
				}
			}
		}

		g_free(alias_escaped);

		/* TODO WEBKIT */
#if 0
		if (tagname)
			tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), tagname);
		else
			tag = get_buddy_tag(conv, name, flags, TRUE);

		if (GTK_IMHTML(gtkconv->imhtml)->show_comments) {
		{
			/* The color for the timestamp has to be set in the font-tags, unfortunately.
			 * Applying the nick-tag to timestamps would work, but that can make it
			 * bold. I thought applying the "comment" tag again, which has "weight" set
			 * to PANGO_WEIGHT_NORMAL, would remove the boldness. But it doesn't. So
			 * this will have to do. I don't terribly like it.  -- sadrul */
			/* const char *color = get_text_tag_color(tag); */
			g_snprintf(buf2, BUF_LONG, "<FONT %s%s%s SIZE=\"2\"><!--%s --></FONT>",
					color ? "COLOR=\"" : "", color ? color : "", color ? "\"" : "", mdate);
			gtk_webview_append_html (GTK_WEBVIEW(gtkconv->webview), buf2);
		}
#endif /* if 0 */
		g_snprintf(buf2, BUF_LONG, "<font %s>%s</font> ", sml_attrib ? sml_attrib : "", str);
		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), buf2);

		g_free(str);

		if (gc) {
			char *pre = g_strdup_printf("<font %s>", sml_attrib ? sml_attrib : "");
			char *post = "</font>";
			with_font_tag = g_strdup_printf("%s%s%s", pre, new_message, post);
			g_free(pre);
		} else
			with_font_tag = g_memdup(new_message, length);

		gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview),
							 with_font_tag);

		g_free(with_font_tag);
		g_free(new_message);
	}

	g_free(mdate);
	g_free(sml_attrib);

#endif

	/* Tab highlighting stuff */
	if (!(flags & PURPLE_MESSAGE_SEND) && !pidgin_conv_has_focus(conv))
	{
		PidginUnseenState unseen = PIDGIN_UNSEEN_NONE;

		if ((flags & PURPLE_MESSAGE_NICK) == PURPLE_MESSAGE_NICK)
			unseen = PIDGIN_UNSEEN_NICK;
		else if (((flags & PURPLE_MESSAGE_SYSTEM) == PURPLE_MESSAGE_SYSTEM) ||
			  ((flags & PURPLE_MESSAGE_ERROR) == PURPLE_MESSAGE_ERROR))
			unseen = PIDGIN_UNSEEN_EVENT;
		else if ((flags & PURPLE_MESSAGE_NO_LOG) == PURPLE_MESSAGE_NO_LOG)
			unseen = PIDGIN_UNSEEN_NO_LOG;
		else
			unseen = PIDGIN_UNSEEN_TEXT;

		gtkconv_set_unseen(gtkconv, unseen);
	}

#if 0
	if (!(flags & PURPLE_MESSAGE_RECV) && (purple_conversation_get_features(conv) & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY))
	{
		/* Restore the smiley-data */
		pidgin_themes_smiley_themeize(gtkconv->webview);
	}
#endif


	/* on rejoin only request message history from after this message */
	if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV) &&
		PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *chat = purple_blist_find_chat(
			purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
		if (chat) {
			GHashTable *comps = purple_chat_get_components(chat);
			time_t now, history_since, prev_history_since = 0;
			struct tm *history_since_tm;
			const char *history_since_s, *prev_history_since_s;

			history_since = mtime + 1;

			prev_history_since_s = g_hash_table_lookup(comps,
				"history_since");
			if (prev_history_since_s != NULL)
				prev_history_since = purple_str_to_time(
					prev_history_since_s, TRUE, NULL, NULL,
					NULL);

			now = time(NULL);
			/* in case of incorrectly stored timestamps */
			if (prev_history_since > now)
				prev_history_since = now;
			/* in case of delayed messages */
			if (history_since < prev_history_since)
				history_since = prev_history_since;

			history_since_tm = gmtime(&history_since);
			history_since_s = purple_utf8_strftime(
				"%Y-%m-%dT%H:%M:%SZ", history_since_tm);
			if (g_strcmp0(prev_history_since_s,
				history_since_s) != 0)
				g_hash_table_replace(comps,
					g_strdup("history_since"),
					g_strdup(history_since_s));
		}
	}

	purple_signal_emit(pidgin_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "displayed-im-msg" : "displayed-chat-msg"),
		account, name, displaying, conv, flags);
	g_free(displaying);
	update_typing_message(gtkconv, NULL);
}

static gboolean get_iter_from_chatuser(PurpleChatUser *cb, GtkTreeIter *iter)
{
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	GtkTreeModel *model;

	g_return_val_if_fail(cb != NULL, FALSE);

	ref = purple_chat_user_get_ui_data(cb);
	if (!ref)
		return FALSE;

	if ((path = gtk_tree_row_reference_get_path(ref)) == NULL)
		return FALSE;

	model = gtk_tree_row_reference_get_model(ref);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtk_tree_path_free(path);
	return TRUE;
}

static void
pidgin_conv_chat_add_users(PurpleChatConversation *chat, GList *cbuddies, gboolean new_arrivals)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkListStore *ls;
	GList *l;

	char tmp[BUF_LONG];
	int num_users;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(purple_chat_conversation_get_users(chat));

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users),
			   num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);

	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
										 GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);

	l = cbuddies;
	while (l != NULL) {
		add_chat_user_common(chat, (PurpleChatUser *)l->data, NULL);
		l = l->next;
	}

	/* Currently GTK+ maintains our sorted list after it's in the tree.
	 * This may change if it turns out we can manage it faster ourselves.
	 */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  CHAT_USERS_ALIAS_KEY_COLUMN,
										 GTK_SORT_ASCENDING);
}

static void
pidgin_conv_chat_rename_user(PurpleChatConversation *chat, const char *old_name,
			      const char *new_name, const char *new_alias)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	PurpleChatUser *old_chatuser, *new_chatuser;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTextTag *tag;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	if ((tag = get_buddy_tag(chat, old_name, 0, FALSE)))
		g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
	if ((tag = get_buddy_tag(chat, old_name, PURPLE_MESSAGE_NICK, FALSE)))
		g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);

	old_chatuser = purple_chat_conversation_find_user(chat, old_name);
	if (!old_chatuser)
		return;

	if (get_iter_from_chatuser(old_chatuser, &iter)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(old_chatuser);

		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(old_chatuser, NULL);
	}

	g_return_if_fail(new_alias != NULL);

	new_chatuser = purple_chat_conversation_find_user(chat, new_name);

	add_chat_user_common(chat, new_chatuser, old_name);
}

static void
pidgin_conv_chat_remove_users(PurpleChatConversation *chat, GList *users)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *l;
	char tmp[BUF_LONG];
	int num_users;
	gboolean f;
	GtkTextTag *tag;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(purple_chat_conversation_get_users(chat));

	for (l = users; l != NULL; l = l->next) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

		if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
			/* XXX: Break? */
			continue;

		do {
			char *val;

			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
							   CHAT_USERS_NAME_COLUMN, &val, -1);

			if (!purple_utf8_strcasecmp((char *)l->data, val)) {
				f = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			}
			else
				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

			g_free(val);
		} while (f);

		if ((tag = get_buddy_tag(chat, l->data, 0, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
		if ((tag = get_buddy_tag(chat, l->data, PURPLE_MESSAGE_NICK, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
	}

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users), num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);
}

static void
pidgin_conv_chat_update_user(PurpleChatUser *chatuser)
{
	PurpleChatConversation *chat;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!chatuser)
		return;

	chat = purple_chat_user_get_chat(chatuser);
	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	if (get_iter_from_chatuser(chatuser, &iter)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(chatuser);
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(chatuser, NULL);
	}

	if (chatuser)
		add_chat_user_common(chat, chatuser, NULL);
}

gboolean
pidgin_conv_has_focus(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	PidginWindow *win;
	gboolean has_focus;

	win = gtkconv->win;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	if (has_focus && pidgin_conv_window_is_active_conversation(conv))
		return TRUE;

	return FALSE;
}

static gboolean
add_custom_smiley_for_webview(GtkWebView *webview, const char *sml, const char *smile)
{
	GtkWebViewSmiley *smiley;

	smiley = gtk_webview_smiley_find(webview, sml, smile);

	if (smiley) {
		if (!(gtk_webview_smiley_get_flags(smiley) & GTK_WEBVIEW_SMILEY_CUSTOM))
			return FALSE;

		gtk_webview_smiley_reload(smiley);
		return TRUE;
	}

	smiley = gtk_webview_smiley_create(NULL, smile, FALSE,
	                                   GTK_WEBVIEW_SMILEY_CUSTOM);
	gtk_webview_associate_smiley(webview, sml, smiley);
	g_signal_connect_swapped(webview, "destroy",
	                         G_CALLBACK(gtk_webview_smiley_destroy), smiley);

	return TRUE;
}

static gboolean
pidgin_conv_custom_smiley_add(PurpleConversation *conv, const char *smile, gboolean remote)
{
	PidginConversation *gtkconv;
	struct smiley_list *list;
	const char *sml = NULL, *conv_sml;

	if (!conv || !smile || !*smile) {
		return FALSE;
	}

	/* If smileys are off, return false */
	if (pidgin_themes_smileys_disabled())
		return FALSE;

	/* If possible add this smiley to the current theme.
	 * The addition is only temporary: custom smilies aren't saved to disk. */
	conv_sml = purple_account_get_protocol_name(purple_conversation_get_account(conv));
	gtkconv = PIDGIN_CONVERSATION(conv);

	for (list = (struct smiley_list *)current_smiley_theme->list; list; list = list->next) {
		if (!strcmp(list->sml, conv_sml)) {
			sml = list->sml;
			break;
		}
	}

	if (!add_custom_smiley_for_webview(GTK_WEBVIEW(gtkconv->webview), sml, smile))
		return FALSE;

	if (!remote)	/* If it's a local custom smiley, then add it for the entry */
		if (!add_custom_smiley_for_webview(GTK_WEBVIEW(gtkconv->entry), sml, smile))
			return FALSE;

	return TRUE;
}

static void
pidgin_conv_custom_smiley_write(PurpleConversation *conv, const char *smile,
                                      const guchar *data, gsize size)
{
/* TODO WEBKIT */
#if 0
	PidginConversation *gtkconv;
	GtkIMHtmlSmiley *smiley;
	const char *sml;
	GError *error = NULL;

	sml = purple_account_get_protocol_name(purple_conversation_get_account(conv));
	gtkconv = PIDGIN_CONVERSATION(conv);
	smiley = gtk_imhtml_smiley_get(GTK_IMHTML(gtkconv->imhtml), sml, smile);

	if (!smiley)
		return;

	smiley->data = g_realloc(smiley->data, smiley->datasize + size);
	g_memmove((guchar *)smiley->data + smiley->datasize, data, size);
	smiley->datasize += size;

	if (!smiley->loader)
		return;

	if (!gdk_pixbuf_loader_write(smiley->loader, data, size, &error) || error) {
		purple_debug_warning("gtkconv", "gdk_pixbuf_loader_write() "
				"failed with size=%" G_GSIZE_FORMAT ": %s\n", size,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		/* We must stop using the GdkPixbufLoader because trying to load
		   certain invalid GIFs with at least gdk-pixbuf 2.23.3 can return
		   a GdkPixbuf that will cause some operations (like
		   gdk_pixbuf_scale_simple()) to consume memory in an infinite loop.
		   But we also don't want to set smiley->loader to NULL because our
		   code might expect it to be set.  So create a new loader. */
		g_object_unref(G_OBJECT(smiley->loader));
		smiley->loader = gdk_pixbuf_loader_new();
	}
#endif /* if 0 */
}

static void
pidgin_conv_custom_smiley_close(PurpleConversation *conv, const char *smile)
{
/* TODO WEBKIT */
#if 0
	PidginConversation *gtkconv;
	GtkIMHtmlSmiley *smiley;
	const char *sml;
	GError *error = NULL;

	g_return_if_fail(conv  != NULL);
	g_return_if_fail(smile != NULL);

	sml = purple_account_get_protocol_name(purple_conversation_get_account(conv));
	gtkconv = PIDGIN_CONVERSATION(conv);
	smiley = gtk_imhtml_smiley_get(GTK_IMHTML(gtkconv->imhtml), sml, smile);

	if (!smiley)
		return;

	if (!smiley->loader)
		return;

	purple_debug_info("gtkconv", "About to close the smiley pixbuf\n");

	if (!gdk_pixbuf_loader_close(smiley->loader, &error) || error) {
		purple_debug_warning("gtkconv", "gdk_pixbuf_loader_close() "
				"failed: %s\n",
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		/* We must stop using the GdkPixbufLoader because if we tried to
		   load certain invalid GIFs with all current versions of GDK (as
		   of 2011-06-15) then it's possible the loader will contain data
		   that could cause some operations (like gdk_pixbuf_scale_simple())
		   to consume memory in an infinite loop.  But we also don't want
		   to set smiley->loader to NULL because our code might expect it
		   to be set.  So create a new loader. */
		g_object_unref(G_OBJECT(smiley->loader));
		smiley->loader = gdk_pixbuf_loader_new();
	}
#endif /* if 0 */
}

static void
pidgin_conv_send_confirm(PurpleConversation *conv, const char *message)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_webview_append_html(GTK_WEBVIEW(gtkconv->webview), message);
}

/*
 * Makes sure all the menu items and all the buttons are hidden/shown and
 * sensitive/insensitive.  This is called after changing tabs and when an
 * account signs on or off.
 */
static void
gray_stuff_out(PidginConversation *gtkconv)
{
	PidginWindow *win;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;
	GdkPixbuf *window_icon = NULL;
	GtkWebViewButtons buttons;
	PurpleAccount *account;

	win     = pidgin_conv_get_window(gtkconv);
	gc      = purple_conversation_get_connection(conv);
	account = purple_conversation_get_account(conv);

	if (gc != NULL)
		protocol = purple_connection_get_protocol_info(gc);

	if (win->menu.send_to != NULL)
		update_send_to_selection(win);

	/*
	 * Handle hiding and showing stuff based on what type of conv this is.
	 * Stuff that Purple IMs support in general should be shown for IM
	 * conversations.  Stuff that Purple chats support in general should be
	 * shown for chat conversations.  It doesn't matter whether the PRPL
	 * supports it or not--that only affects if the button or menu item
	 * is sensitive or not.
	 */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		/* Show stuff that applies to IMs, hide stuff that applies to chats */

		/* Deal with menu items */
		gtk_action_set_visible(win->menu.view_log, TRUE);
		gtk_action_set_visible(win->menu.send_file, TRUE);
		gtk_action_set_visible(win->menu.get_attention, TRUE);
		gtk_action_set_visible(win->menu.add_pounce, TRUE);
		gtk_action_set_visible(win->menu.get_info, TRUE);
		gtk_action_set_visible(win->menu.invite, FALSE);
		gtk_action_set_visible(win->menu.alias, TRUE);
		if (purple_account_privacy_check(account, purple_conversation_get_name(conv))) {
			gtk_action_set_visible(win->menu.unblock, FALSE);
			gtk_action_set_visible(win->menu.block, TRUE);
		} else {
			gtk_action_set_visible(win->menu.block, FALSE);
			gtk_action_set_visible(win->menu.unblock, TRUE);
		}

		if (purple_blist_find_buddy(account, purple_conversation_get_name(conv)) == NULL) {
			gtk_action_set_visible(win->menu.add, TRUE);
			gtk_action_set_visible(win->menu.remove, FALSE);
		} else {
			gtk_action_set_visible(win->menu.remove, TRUE);
			gtk_action_set_visible(win->menu.add, FALSE);
		}

		gtk_action_set_visible(win->menu.insert_link, TRUE);
		gtk_action_set_visible(win->menu.insert_image, TRUE);
	} else {
		/* Show stuff that applies to Chats, hide stuff that applies to IMs */

		/* Deal with menu items */
		gtk_action_set_visible(win->menu.view_log, TRUE);
		gtk_action_set_visible(win->menu.send_file, FALSE);
		gtk_action_set_visible(win->menu.get_attention, FALSE);
		gtk_action_set_visible(win->menu.add_pounce, FALSE);
		gtk_action_set_visible(win->menu.get_info, FALSE);
		gtk_action_set_visible(win->menu.invite, TRUE);
		gtk_action_set_visible(win->menu.alias, TRUE);
		gtk_action_set_visible(win->menu.block, FALSE);
		gtk_action_set_visible(win->menu.unblock, FALSE);

		if ((account == NULL) || purple_blist_find_chat(account, purple_conversation_get_name(conv)) == NULL) {
			/* If the chat is NOT in the buddy list */
			gtk_action_set_visible(win->menu.add, TRUE);
			gtk_action_set_visible(win->menu.remove, FALSE);
		} else {
			/* If the chat IS in the buddy list */
			gtk_action_set_visible(win->menu.add, FALSE);
			gtk_action_set_visible(win->menu.remove, TRUE);
		}

		gtk_action_set_visible(win->menu.insert_link, TRUE);
		gtk_action_set_visible(win->menu.insert_image, TRUE);
	}

	/*
	 * Handle graying stuff out based on whether an account is connected
	 * and what features that account supports.
	 */
	if ((gc != NULL) &&
		(PURPLE_IS_IM_CONVERSATION(conv) ||
		 !purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv)) ))
	{
		PurpleConnectionFlags features = purple_conversation_get_features(conv);
		/* Account is online */
		/* Deal with the toolbar */
		if (features & PURPLE_CONNECTION_FLAG_HTML)
		{
			buttons = GTK_WEBVIEW_ALL; /* Everything on */
			if (features & PURPLE_CONNECTION_FLAG_NO_BGCOLOR)
				buttons &= ~GTK_WEBVIEW_BACKCOLOR;
			if (features & PURPLE_CONNECTION_FLAG_NO_FONTSIZE)
			{
				buttons &= ~GTK_WEBVIEW_GROW;
				buttons &= ~GTK_WEBVIEW_SHRINK;
			}
			if (features & PURPLE_CONNECTION_FLAG_NO_URLDESC)
				buttons &= ~GTK_WEBVIEW_LINKDESC;
		} else {
			buttons = GTK_WEBVIEW_SMILEY | GTK_WEBVIEW_IMAGE;
		}

		if (!(protocol->options & OPT_PROTO_IM_IMAGE)
		 && !(features & PURPLE_CONNECTION_FLAG_NO_IMAGES)) {
			features |= PURPLE_CONNECTION_FLAG_NO_IMAGES;
			purple_conversation_set_features(conv, features);
		}

		if (features & PURPLE_CONNECTION_FLAG_NO_IMAGES)
			buttons &= ~GTK_WEBVIEW_IMAGE;

		if (features & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY)
			buttons |= GTK_WEBVIEW_CUSTOM_SMILEY;
		else
			buttons &= ~GTK_WEBVIEW_CUSTOM_SMILEY;

		gtk_webview_set_format_functions(GTK_WEBVIEW(gtkconv->entry), buttons);
		if (account != NULL)
			gtk_webview_set_protocol_name(GTK_WEBVIEW(gtkconv->entry), purple_account_get_protocol_id(account));

		/* Deal with menu items */
		gtk_action_set_sensitive(win->menu.view_log, TRUE);
		gtk_action_set_sensitive(win->menu.add_pounce, TRUE);
		gtk_action_set_sensitive(win->menu.get_info, (protocol->get_info != NULL));
		gtk_action_set_sensitive(win->menu.invite, (protocol->chat_invite != NULL));
		gtk_action_set_sensitive(win->menu.insert_link, (features & PURPLE_CONNECTION_FLAG_HTML));
		gtk_action_set_sensitive(win->menu.insert_image, !(features & PURPLE_CONNECTION_FLAG_NO_IMAGES));

		if (PURPLE_IS_IM_CONVERSATION(conv))
		{
			gtk_action_set_sensitive(win->menu.add, (protocol->add_buddy != NULL));
			gtk_action_set_sensitive(win->menu.remove, (protocol->remove_buddy != NULL));
			gtk_action_set_sensitive(win->menu.send_file,
									 (protocol->send_file != NULL && (!protocol->can_receive_file ||
									  protocol->can_receive_file(gc, purple_conversation_get_name(conv)))));
			gtk_action_set_sensitive(win->menu.get_attention, (protocol->send_attention != NULL));
			gtk_action_set_sensitive(win->menu.alias,
									 (account != NULL) &&
									 (purple_blist_find_buddy(account, purple_conversation_get_name(conv)) != NULL));
		}
		else
		{
			gtk_action_set_sensitive(win->menu.add, (protocol->join_chat != NULL));
			gtk_action_set_sensitive(win->menu.remove, (protocol->join_chat != NULL));
			gtk_action_set_sensitive(win->menu.alias,
									 (account != NULL) &&
									 (purple_blist_find_chat(account, purple_conversation_get_name(conv)) != NULL));
		}

	} else {
		/* Account is offline */
		/* Or it's a chat that we've left. */

		/* Then deal with menu items */
		gtk_action_set_sensitive(win->menu.view_log, TRUE);
		gtk_action_set_sensitive(win->menu.send_file, FALSE);
		gtk_action_set_sensitive(win->menu.get_attention, FALSE);
		gtk_action_set_sensitive(win->menu.add_pounce, TRUE);
		gtk_action_set_sensitive(win->menu.get_info, FALSE);
		gtk_action_set_sensitive(win->menu.invite, FALSE);
		gtk_action_set_sensitive(win->menu.alias, FALSE);
		gtk_action_set_sensitive(win->menu.add, FALSE);
		gtk_action_set_sensitive(win->menu.remove, FALSE);
		gtk_action_set_sensitive(win->menu.insert_link, TRUE);
		gtk_action_set_sensitive(win->menu.insert_image, FALSE);
	}

	/*
	 * Update the window's icon
	 */
	if (pidgin_conv_window_is_active_conversation(conv))
	{
		GList *l = NULL;
		if (PURPLE_IS_IM_CONVERSATION(conv) &&
				(gtkconv->u.im->anim))
		{
			PurpleBuddy *buddy = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
			window_icon =
				gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);

			if (buddy &&  !PURPLE_BUDDY_IS_ONLINE(buddy))
				gdk_pixbuf_saturate_and_pixelate(window_icon, window_icon, 0.0, FALSE);

			g_object_ref(window_icon);
			l = g_list_append(l, window_icon);
		} else {
			l = pidgin_conv_get_tab_icons(conv);
		}
		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
		if (window_icon != NULL) {
			g_object_unref(G_OBJECT(window_icon));
			g_list_free(l);
		}
	}
}

static void
pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields)
{
	PidginConversation *gtkconv;
	PidginWindow *win;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return;
	win = pidgin_conv_get_window(gtkconv);
	if (!win)
		return;

	if (fields & PIDGIN_CONV_SET_TITLE)
	{
		purple_conversation_autoset_title(conv);
	}

	if (fields & PIDGIN_CONV_BUDDY_ICON)
	{
		if (PURPLE_IS_IM_CONVERSATION(conv))
			pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
	}

	if (fields & PIDGIN_CONV_MENU)
	{
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
		generate_send_to_items(win);
		regenerate_plugins_items(win);
	}

	if (fields & PIDGIN_CONV_TAB_ICON)
	{
		update_tab_icon(conv);
		generate_send_to_items(win);		/* To update the icons in SendTo menu */
	}

	if ((fields & PIDGIN_CONV_TOPIC) &&
				PURPLE_IS_CHAT_CONVERSATION(conv))
	{
		const char *topic;
		PidginChatPane *gtkchat = gtkconv->u.chat;

		if (gtkchat->topic_text != NULL)
		{
			topic = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));

			gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), topic ? topic : "");
			gtk_widget_set_tooltip_text(gtkchat->topic_text,
			                            topic ? topic : "");
		}
	}

#if 0
	if (fields & PIDGIN_CONV_SMILEY_THEME)
		pidgin_themes_smiley_themeize(PIDGIN_CONVERSATION(conv)->webview);
#endif

	if ((fields & PIDGIN_CONV_COLORIZE_TITLE) ||
			(fields & PIDGIN_CONV_SET_TITLE) ||
			(fields & PIDGIN_CONV_TOPIC))
	{
		char *title;
		PurpleIMConversation *im = NULL;
		PurpleAccount *account = purple_conversation_get_account(conv);
		PurpleBuddy *buddy = NULL;
		char *markup = NULL;
		AtkObject *accessibility_obj;
		/* I think this is a little longer than it needs to be but I'm lazy. */
		char *style;

		if (PURPLE_IS_IM_CONVERSATION(conv))
			im = PURPLE_IM_CONVERSATION(conv);

		if ((account == NULL) ||
			!purple_account_is_connected(account) ||
			(PURPLE_IS_CHAT_CONVERSATION(conv)
				&& purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv))))
			title = g_strdup_printf("(%s)", purple_conversation_get_title(conv));
		else
			title = g_strdup(purple_conversation_get_title(conv));

		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
			if (buddy) {
				markup = pidgin_blist_get_name_markup(buddy, FALSE, FALSE);
			} else {
				markup = title;
			}
		} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
			const char *topic = gtkconv->u.chat->topic_text
				? gtk_entry_get_text(GTK_ENTRY(gtkconv->u.chat->topic_text))
				: NULL;
			const char *title = purple_conversation_get_title(conv);
			const char *name = purple_conversation_get_name(conv);

			char *topic_esc, *unaliased, *unaliased_esc, *title_esc;

			topic_esc = topic ? g_markup_escape_text(topic, -1) : NULL;
			unaliased = g_utf8_collate(title, name) ? g_strdup_printf("(%s)", name) : NULL;
			unaliased_esc = unaliased ? g_markup_escape_text(unaliased, -1) : NULL;
			title_esc = g_markup_escape_text(title, -1);

			markup = g_strdup_printf("%s%s<span size='smaller'>%s</span>%s<span color='%s' size='smaller'>%s</span>",
						title_esc,
						unaliased_esc ? " " : "",
						unaliased_esc ? unaliased_esc : "",
						topic_esc  && *topic_esc ? "\n" : "",
						pidgin_get_dim_grey_string(gtkconv->infopane),
						topic_esc ? topic_esc : "");

			g_free(title_esc);
			g_free(topic_esc);
			g_free(unaliased);
			g_free(unaliased_esc);
		}
		gtk_list_store_set(gtkconv->infopane_model, &(gtkconv->infopane_iter),
				CONV_TEXT_COLUMN, markup, -1);
	        /* XXX seanegan Why do I have to do this? */
		gtk_widget_queue_draw(gtkconv->infopane);

		if (title != markup)
			g_free(markup);

		if (!gtk_widget_get_realized(gtkconv->tab_label))
			gtk_widget_realize(gtkconv->tab_label);

		accessibility_obj = gtk_widget_get_accessible(gtkconv->tab_cont);
		if (im != NULL &&
		    purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPING) {
			atk_object_set_description(accessibility_obj, _("Typing"));
			style = "tab-label-typing";
		} else if (im != NULL &&
		         purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPED) {
			atk_object_set_description(accessibility_obj, _("Stopped Typing"));
			style = "tab-label-typed";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_NICK)	{
			atk_object_set_description(accessibility_obj, _("Nick Said"));
			style = "tab-label-attention";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_TEXT)	{
			atk_object_set_description(accessibility_obj, _("Unread Messages"));
			if (PURPLE_IS_CHAT_CONVERSATION(gtkconv->active_conv))
				style = "tab-label-unreadchat";
			else
				style = "tab-label-attention";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_EVENT) {
			atk_object_set_description(accessibility_obj, _("New Event"));
			style = "tab-label-event";
		} else {
			style = "tab-label";
		}

		gtk_widget_set_name(gtkconv->tab_label, style);
		gtk_label_set_text(GTK_LABEL(gtkconv->tab_label), title);
		gtk_widget_set_state(gtkconv->tab_label, GTK_STATE_ACTIVE);

		if (gtkconv->unseen_state == PIDGIN_UNSEEN_TEXT ||
				gtkconv->unseen_state == PIDGIN_UNSEEN_NICK ||
				gtkconv->unseen_state == PIDGIN_UNSEEN_EVENT) {
			PangoAttrList *list = pango_attr_list_new();
			PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
			attr->start_index = 0;
			attr->end_index = -1;
			pango_attr_list_insert(list, attr);
			gtk_label_set_attributes(GTK_LABEL(gtkconv->tab_label), list);
			pango_attr_list_unref(list);
		} else
			gtk_label_set_attributes(GTK_LABEL(gtkconv->tab_label), NULL);

		if (pidgin_conv_window_is_active_conversation(conv))
			update_typing_icon(gtkconv);

		gtk_label_set_text(GTK_LABEL(gtkconv->menu_label), title);
		if (pidgin_conv_window_is_active_conversation(conv)) {
			const char* current_title = gtk_window_get_title(GTK_WINDOW(win->window));
			if (current_title == NULL || strcmp(current_title, title) != 0)
				gtk_window_set_title(GTK_WINDOW(win->window), title);
		}

		g_free(title);
	}
}

static void
pidgin_conv_updated(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	PidginConvFields flags = 0;

	g_return_if_fail(conv != NULL);

	if (type == PURPLE_CONVERSATION_UPDATE_ACCOUNT)
	{
		flags = PIDGIN_CONV_ALL;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_TYPING ||
	         type == PURPLE_CONVERSATION_UPDATE_UNSEEN ||
	         type == PURPLE_CONVERSATION_UPDATE_TITLE)
	{
		flags = PIDGIN_CONV_COLORIZE_TITLE;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_TOPIC)
	{
		flags = PIDGIN_CONV_TOPIC;
	}
	else if (type == PURPLE_CONVERSATION_ACCOUNT_ONLINE ||
	         type == PURPLE_CONVERSATION_ACCOUNT_OFFLINE)
	{
		flags = PIDGIN_CONV_MENU | PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_SET_TITLE;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_AWAY)
	{
		flags = PIDGIN_CONV_TAB_ICON;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_ADD ||
	         type == PURPLE_CONVERSATION_UPDATE_REMOVE ||
	         type == PURPLE_CONVERSATION_UPDATE_CHATLEFT)
	{
		flags = PIDGIN_CONV_SET_TITLE | PIDGIN_CONV_MENU;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_ICON)
	{
		flags = PIDGIN_CONV_BUDDY_ICON;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_FEATURES)
	{
		flags = PIDGIN_CONV_MENU;
	}

	pidgin_conv_update_fields(conv, flags);
}

static void
wrote_msg_update_unseen_cb(PurpleAccount *account, const char *who, const char *message,
		PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	PidginConversation *gtkconv = conv ? PIDGIN_CONVERSATION(conv) : NULL;
	if (conv == NULL || (gtkconv && gtkconv->win != hidden_convwin))
		return;
	if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)) {
		PidginUnseenState unseen = PIDGIN_UNSEEN_NONE;

		if ((flags & PURPLE_MESSAGE_NICK) == PURPLE_MESSAGE_NICK)
			unseen = PIDGIN_UNSEEN_NICK;
		else if (((flags & PURPLE_MESSAGE_SYSTEM) == PURPLE_MESSAGE_SYSTEM) ||
			  ((flags & PURPLE_MESSAGE_ERROR) == PURPLE_MESSAGE_ERROR))
			unseen = PIDGIN_UNSEEN_EVENT;
		else if ((flags & PURPLE_MESSAGE_NO_LOG) == PURPLE_MESSAGE_NO_LOG)
			unseen = PIDGIN_UNSEEN_NO_LOG;
		else
			unseen = PIDGIN_UNSEEN_TEXT;

		conv_set_unseen(conv, unseen);
	}
}

static PurpleConversationUiOps conversation_ui_ops =
{
	pidgin_conv_new,
	pidgin_conv_destroy,              /* destroy_conversation */
	NULL,                              /* write_chat           */
	pidgin_conv_write_im,             /* write_im             */
	pidgin_conv_write_conv,           /* write_conv           */
	pidgin_conv_chat_add_users,       /* chat_add_users       */
	pidgin_conv_chat_rename_user,     /* chat_rename_user     */
	pidgin_conv_chat_remove_users,    /* chat_remove_users    */
	pidgin_conv_chat_update_user,     /* chat_update_user     */
	pidgin_conv_present_conversation, /* present              */
	pidgin_conv_has_focus,            /* has_focus            */
	pidgin_conv_custom_smiley_add,    /* custom_smiley_add    */
	pidgin_conv_custom_smiley_write,  /* custom_smiley_write  */
	pidgin_conv_custom_smiley_close,  /* custom_smiley_close  */
	pidgin_conv_send_confirm,         /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleConversationUiOps *
pidgin_conversations_get_conv_ui_ops(void)
{
	return &conversation_ui_ops;
}

/**************************************************************************
 * Public conversation utility functions
 **************************************************************************/
void
pidgin_conv_update_buddy_icon(PurpleIMConversation *im)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PidginWindow *win;

	PurpleBuddy *buddy;

	PurpleStoredImage *custom_img = NULL;
	gconstpointer data = NULL;
	size_t len;

	GdkPixbuf *buf;

	GList *children;
	GtkWidget *event;
	GdkPixbuf *scale;
	int scale_width, scale_height;
	int size = 0;

	PurpleAccount *account;

	PurpleBuddyIcon *icon;

	conv = PURPLE_CONVERSATION(im);

	g_return_if_fail(conv != NULL);
	g_return_if_fail(PIDGIN_IS_PIDGIN_CONVERSATION(conv));

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	if (!gtkconv->u.im->show_icon)
		return;

	account = purple_conversation_get_account(conv);

	/* Remove the current icon stuff */
	children = gtk_container_get_children(GTK_CONTAINER(gtkconv->u.im->icon_container));
	if (children) {
		/* We know there's only one child here. It'd be nice to shortcut to the
		   event box, but we can't change the PidginConversation until 3.0 */
		event = (GtkWidget *)children->data;
		gtk_container_remove(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
		g_list_free(children);
	}

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	gtkconv->u.im->anim = NULL;

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->iter = NULL;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	if (purple_conversation_get_connection(conv) == NULL)
		return;

	buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
	if (buddy)
	{
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		if (contact) {
			custom_img = purple_buddy_icons_node_find_custom_icon((PurpleBlistNode*)contact);
			if (custom_img) {
				/* There is a custom icon for this user */
				data = purple_imgstore_get_data(custom_img);
				len = purple_imgstore_get_size(custom_img);
			}
		}
	}

	if (data == NULL) {
		icon = purple_im_conversation_get_icon(im);
		if (icon == NULL)
		{
			gtk_widget_set_size_request(gtkconv->u.im->icon_container,
			                            -1, BUDDYICON_SIZE_MIN);
			return;
		}

		data = purple_buddy_icon_get_data(icon, &len);
		if (data == NULL)
		{
			gtk_widget_set_size_request(gtkconv->u.im->icon_container,
			                            -1, BUDDYICON_SIZE_MIN);
			return;
		}
	}

	gtkconv->u.im->anim = pidgin_pixbuf_anim_from_data(data, len);
	purple_imgstore_unref(custom_img);

	if (!gtkconv->u.im->anim) {
		purple_debug_error("gtkconv", "Couldn't load icon for conv %s\n",
				purple_conversation_get_name(conv));
		return;
	}

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)) {
		GdkPixbuf *stat;
		gtkconv->u.im->iter = NULL;
		stat = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
		buf = gdk_pixbuf_add_alpha(stat, FALSE, 0, 0, 0);
	} else {
		GdkPixbuf *stat;
		gtkconv->u.im->iter =
			gdk_pixbuf_animation_get_iter(gtkconv->u.im->anim, NULL); /* LEAK */
		stat = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);
		buf = gdk_pixbuf_add_alpha(stat, FALSE, 0, 0, 0);
		if (gtkconv->u.im->animate)
			start_anim(NULL, gtkconv);
	}

	scale_width = gdk_pixbuf_get_width(buf);
	scale_height = gdk_pixbuf_get_height(buf);

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);
	size = MIN(size, MIN(scale_width, scale_height));

	/* Some sanity checks */
	size = CLAMP(size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);
	if (scale_width == scale_height) {
		scale_width = scale_height = size;
	} else if (scale_height > scale_width) {
		scale_width = size * scale_width / scale_height;
		scale_height = size;
	} else {
		scale_height = size * scale_height / scale_width;
		scale_width = size;
	}
	scale = gdk_pixbuf_scale_simple(buf, scale_width, scale_height,
				GDK_INTERP_BILINEAR);
	g_object_unref(buf);
	if (pidgin_gdk_pixbuf_is_opaque(scale))
		pidgin_gdk_pixbuf_make_round(scale);

	event = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(event), FALSE);
	gtk_widget_add_events(event,
	                      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(event), "button-press-event",
					 G_CALLBACK(icon_menu), gtkconv);

	pidgin_tooltip_setup_for_widget(event, gtkconv, pidgin_conv_create_tooltip, NULL);
	gtk_widget_show(event);

	gtkconv->u.im->icon = gtk_image_new_from_pixbuf(scale);
	gtk_container_add(GTK_CONTAINER(event), gtkconv->u.im->icon);
	gtk_widget_show(gtkconv->u.im->icon);

	g_object_unref(G_OBJECT(scale));

	/* The buddy icon code needs badly to be fixed. */
	if(pidgin_conv_window_is_active_conversation(conv))
	{
		buf = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
		if (buddy && !PURPLE_BUDDY_IS_ONLINE(buddy))
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);
		gtk_window_set_icon(GTK_WINDOW(win->window), buf);
	}
}

void
pidgin_conv_update_buttons_by_protocol(PurpleConversation *conv)
{
	PidginWindow *win;

	if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		return;

	win = PIDGIN_CONVERSATION(conv)->win;

	if (win != NULL && pidgin_conv_window_is_active_conversation(conv))
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
}

static gboolean
pidgin_conv_xy_to_right_infopane(PidginWindow *win, int x, int y)
{
	gint pane_x, pane_y, x_rel;
	PidginConversation *gtkconv;
	GtkAllocation allocation;

	gdk_window_get_origin(gtk_widget_get_window(win->notebook),
	                      &pane_x, &pane_y);
	x_rel = x - pane_x;
	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	gtk_widget_get_allocation(gtkconv->infopane, &allocation);
	return (x_rel > allocation.x + allocation.width / 2);
}

int
pidgin_conv_get_tab_at_xy(PidginWindow *win, int x, int y, gboolean *to_right)
{
	gint nb_x, nb_y, x_rel, y_rel;
	GtkNotebook *notebook;
	GtkWidget *page, *tab;
	gint i, page_num = -1;
	gint count;
	gboolean horiz;

	if (to_right)
		*to_right = FALSE;

	notebook = GTK_NOTEBOOK(win->notebook);

	gdk_window_get_origin(gtk_widget_get_window(win->notebook), &nb_x, &nb_y);
	x_rel = x - nb_x;
	y_rel = y - nb_y;

	horiz = (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(notebook) == GTK_POS_BOTTOM);

	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

	for (i = 0; i < count; i++) {
		GtkAllocation allocation;

		page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
		tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), page);
		gtk_widget_get_allocation(tab, &allocation);

		/* Make sure the tab is not hidden beyond an arrow */
		if (!gtk_widget_is_drawable(tab) && gtk_notebook_get_show_tabs(notebook))
			continue;

		if (horiz) {
			if (x_rel >= allocation.x - PIDGIN_HIG_BOX_SPACE &&
					x_rel <= allocation.x + allocation.width + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && x_rel >= allocation.x + allocation.width/2)
					*to_right = TRUE;

				break;
			}
		} else {
			if (y_rel >= allocation.y - PIDGIN_HIG_BOX_SPACE &&
					y_rel <= allocation.y + allocation.height + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && y_rel >= allocation.y + allocation.height/2)
					*to_right = TRUE;

				break;
			}
		}
	}

	if (page_num == -1) {
		/* Add after the last tab */
		page_num = count - 1;
	}

	return page_num;
}

static void
close_on_tabs_pref_cb(const char *name, PurplePrefType type,
					  gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		if (value)
			gtk_widget_show(gtkconv->close);
		else
			gtk_widget_hide(gtkconv->close);
	}
}

static void
spellcheck_pref_cb(const char *name, PurplePrefType type,
				   gconstpointer value, gpointer data)
{
	GList *cl;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	for (cl = purple_conversations_get_all(); cl != NULL; cl = cl->next) {

		conv = (PurpleConversation *)cl->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		pidgin_webview_set_spellcheck(GTK_WEBVIEW(gtkconv->entry),
		                              (gboolean)GPOINTER_TO_INT(value));
	}
}

static void
tab_side_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *gtkwins, *gtkconvs;
	GtkPositionType pos;
	PidginWindow *gtkwin;

	pos = GPOINTER_TO_INT(value);

	for (gtkwins = pidgin_conv_windows_get_list(); gtkwins != NULL; gtkwins = gtkwins->next) {
		gtkwin = gtkwins->data;
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gtkwin->notebook), pos&~8);
		for (gtkconvs = gtkwin->gtkconvs; gtkconvs != NULL; gtkconvs = gtkconvs->next) {
			pidgin_conv_tab_pack(gtkwin, gtkconvs->data);
		}
	}
}

static void
show_formatting_toolbar_pref_cb(const char *name, PurplePrefType type,
								gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	for (l = purple_conversations_get_all(); l != NULL; l = l->next)
	{
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);
		win     = gtkconv->win;

		gtk_toggle_action_set_active(
		        GTK_TOGGLE_ACTION(win->menu.show_formatting_toolbar),
		        (gboolean)GPOINTER_TO_INT(value));

		if ((gboolean)GPOINTER_TO_INT(value))
			gtk_webview_show_toolbar(GTK_WEBVIEW(gtkconv->entry));
		else
			gtk_webview_hide_toolbar(GTK_WEBVIEW(gtkconv->entry));

		g_idle_add((GSourceFunc)resize_webview_cb, gtkconv);
	}
}

static void
animate_buddy_icons_pref_cb(const char *name, PurplePrefType type,
							gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	/* Set the "animate" flag for each icon based on the new preference */
	for (l = purple_conversations_get_ims(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;
		gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv)
			gtkconv->u.im->animate = GPOINTER_TO_INT(value);
	}

	/* Now either stop or start animation for the active conversation in each window */
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;
		conv = pidgin_conv_window_get_active_conversation(win);
		pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
	}
}

static void
show_buddy_icons_pref_cb(const char *name, PurplePrefType type,
						 gconstpointer value, gpointer data)
{
	GList *l;

	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		PurpleConversation *conv = l->data;
		if (!PIDGIN_CONVERSATION(conv))
			continue;
		if (GPOINTER_TO_INT(value))
			gtk_widget_show(PIDGIN_CONVERSATION(conv)->infopane_hbox);
		else
			gtk_widget_hide(PIDGIN_CONVERSATION(conv)->infopane_hbox);

		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
		}
	}

	/* Make the tabs show/hide correctly */
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		PidginWindow *win = l->data;
		if (pidgin_conv_window_get_gtkconv_count(win) == 1)
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
						   GPOINTER_TO_INT(value) == 0);
	}
}

static void
show_protocol_icons_pref_cb(const char *name, PurplePrefType type,
						gconstpointer value, gpointer data)
{
	GList *l;
	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		PurpleConversation *conv = l->data;
		if (PIDGIN_CONVERSATION(conv))
			update_tab_icon(conv);
	}
}

static void
conv_placement_usetabs_cb(const char *name, PurplePrefType type,
						  gconstpointer value, gpointer data)
{
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
}

static void
account_status_changed_cb(PurpleAccount *account, PurpleStatus *oldstatus,
                          PurpleStatus *newstatus)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away")!=0)
		return;

	if(purple_status_is_available(oldstatus) || !purple_status_is_available(newstatus))
		return;

	for (l = hidden_convwin->gtkconvs; l; ) {
		gtkconv = l->data;
		l = l->next;

		conv = gtkconv->active_conv;
		if (PURPLE_IS_CHAT_CONVERSATION(conv) ||
				account != purple_conversation_get_account(conv))
			continue;

		pidgin_conv_attach_to_conversation(conv);

		/* TODO: do we need to do anything for any other conversations that are in the same gtkconv here?
		 * I'm a little concerned that not doing so will cause the "pending" indicator in the gtkblist not to be cleared. -DAA*/
		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
	}
}

static void
hide_new_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;
	gboolean when_away = FALSE;

	if(!hidden_convwin)
		return;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always")==0)
		return;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away")==0)
		when_away = TRUE;

	for (l = hidden_convwin->gtkconvs; l; )
	{
		gtkconv = l->data;
		l = l->next;

		conv = gtkconv->active_conv;

		if (PURPLE_IS_CHAT_CONVERSATION(conv) ||
				gtkconv->unseen_count == 0 ||
				(when_away && !purple_status_is_available(
							purple_account_get_active_status(
							purple_conversation_get_account(conv)))))
			continue;

		pidgin_conv_attach_to_conversation(conv);
	}
}


static void
conv_placement_pref_cb(const char *name, PurplePrefType type,
					   gconstpointer value, gpointer data)
{
	PidginConvPlacementFunc func;

	if (strcmp(name, PIDGIN_PREFS_ROOT "/conversations/placement"))
		return;

	func = pidgin_conv_placement_get_fnc(value);

	if (func == NULL)
		return;

	pidgin_conv_placement_set_current_func(func);
}

static PidginConversation *
get_gtkconv_with_contact(PurpleContact *contact)
{
	PurpleBlistNode *node;

	node = ((PurpleBlistNode*)contact)->child;

	for (; node; node = node->next)
	{
		PurpleBuddy *buddy = (PurpleBuddy*)node;
		PurpleIMConversation *im;
		im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
		if (im)
			return PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
	}
	return NULL;
}

static void
account_signed_off_cb(PurpleConnection *gc, gpointer event)
{
	GList *iter;

	for (iter = purple_conversations_get_all(); iter; iter = iter->next)
	{
		PurpleConversation *conv = iter->data;

		/* This seems fine in theory, but we also need to cover the
		 * case of this account matching one of the other buddies in
		 * one of the contacts containing the buddy corresponding to
		 * a conversation.  It's easier to just update them all. */
		/* if (purple_conversation_get_account(conv) == account) */
			pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON |
							PIDGIN_CONV_MENU | PIDGIN_CONV_COLORIZE_TITLE);

		if (PURPLE_CONNECTION_IS_CONNECTED(gc) &&
				PURPLE_IS_CHAT_CONVERSATION(conv) &&
				purple_conversation_get_account(conv) == purple_connection_get_account(gc) &&
				g_object_get_data(G_OBJECT(conv), "want-to-rejoin")) {
			GHashTable *comps = NULL;
			PurpleChat *chat = purple_blist_find_chat(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
			if (chat == NULL) {
				PurpleProtocol *protocol = purple_connection_get_protocol_info(gc);
				
				if (protocol->chat_info_defaults != NULL)
					comps = protocol->chat_info_defaults(gc, purple_conversation_get_name(conv));
			} else {
				comps = purple_chat_get_components(chat);
			}
			serv_join_chat(gc, comps);
			if (chat == NULL && comps != NULL)
				g_hash_table_destroy(comps);
		}
	}
}

static void
account_signing_off(PurpleConnection *gc)
{
	GList *list = purple_conversations_get_chats();
	PurpleAccount *account = purple_connection_get_account(gc);

	/* We are about to sign off. See which chats we are currently in, and mark
	 * them for rejoin on reconnect. */
	while (list) {
		PurpleConversation *conv = list->data;
		if (!purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv)) &&
				purple_conversation_get_account(conv) == account) {
			g_object_set_data(G_OBJECT(conv), "want-to-rejoin", GINT_TO_POINTER(TRUE));
			purple_conversation_write(conv, NULL, _("The account has disconnected and you are no "
						"longer in this chat. You will automatically rejoin the chat when "
						"the account reconnects."),
					PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
		}
		list = list->next;
	}
}

static void
update_buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old, PurpleStatus *newstatus)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv)
	{
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON
		                              | PIDGIN_CONV_COLORIZE_TITLE
		                              | PIDGIN_CONV_BUDDY_ICON);
		if ((purple_status_is_online(old) ^ purple_status_is_online(newstatus)) != 0)
			pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}
}

static void
update_buddy_privacy_changed(PurpleBuddy *buddy)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv) {
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_MENU);
	}
}

static void
update_buddy_idle_changed(PurpleBuddy *buddy, gboolean old, gboolean newidle)
{
	PurpleIMConversation *im;

	im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
	if (im)
		pidgin_conv_update_fields(PURPLE_CONVERSATION(im), PIDGIN_CONV_TAB_ICON);
}

static void
update_buddy_icon(PurpleBuddy *buddy)
{
	PurpleIMConversation *im;

	im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
	if (im)
		pidgin_conv_update_fields(PURPLE_CONVERSATION(im), PIDGIN_CONV_BUDDY_ICON);
}

static void
update_buddy_sign(PurpleBuddy *buddy, const char *which)
{
	PurplePresence *presence;
	PurpleStatus *on, *off;

	presence = purple_buddy_get_presence(buddy);
	if (!presence)
		return;
	off = purple_presence_get_status(presence, "offline");
	on = purple_presence_get_status(presence, "available");

	if (*(which+1) == 'f')
		update_buddy_status_changed(buddy, on, off);
	else
		update_buddy_status_changed(buddy, off, on);
}

static void
update_conversation_switched(PurpleConversation *conv)
{
	pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_SET_TITLE |
					PIDGIN_CONV_MENU | PIDGIN_CONV_BUDDY_ICON);
}

static void
update_buddy_typing(PurpleAccount *account, const char *who)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(who, account));
	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (gtkconv && gtkconv->active_conv == conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_COLORIZE_TITLE);
}

static void
update_chat(PurpleChatConversation *chat)
{
	pidgin_conv_update_fields(PURPLE_CONVERSATION(chat), PIDGIN_CONV_TOPIC |
					PIDGIN_CONV_MENU | PIDGIN_CONV_SET_TITLE);
}

static void
update_chat_topic(PurpleChatConversation *chat, const char *old, const char *new)
{
	pidgin_conv_update_fields(PURPLE_CONVERSATION(chat), PIDGIN_CONV_TOPIC);
}

/* Message history stuff */

/* Compare two PurpleConversationMessage's, according to time in ascending order. */
static int
message_compare(gconstpointer p1, gconstpointer p2)
{
	const PurpleConversationMessage *m1 = p1, *m2 = p2;
	return (purple_conversation_message_get_timestamp(m1) > purple_conversation_message_get_timestamp(m2));
}

/* Adds some message history to the gtkconv. This happens in a idle-callback. */
static gboolean
add_message_history_to_gtkconv(gpointer data)
{
	PidginConversation *gtkconv = data;
	GtkWebView *webview = GTK_WEBVIEW(gtkconv->webview);
	int count = 0;
	int timer = gtkconv->attach.timer;
	time_t when = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtkconv->entry), "attach-start-time"));
	gboolean im = (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv));

	gtkconv->attach.timer = 0;
	while (gtkconv->attach.current && count < 100) {  /* XXX: 100 is a random value here */
		PurpleConversationMessage *msg = gtkconv->attach.current->data;
		if (!im && when && when < purple_conversation_message_get_timestamp(msg)) {
			gtk_webview_append_html(webview, "<BR><HR>");
			gtk_webview_scroll_to_end(webview, TRUE);
			g_object_set_data(G_OBJECT(gtkconv->entry), "attach-start-time", NULL);
		}
		pidgin_conv_write_conv(
				purple_conversation_message_get_conversation(msg),
				purple_conversation_message_get_sender(msg),
				purple_conversation_message_get_alias(msg),
				purple_conversation_message_get_message(msg),
				purple_conversation_message_get_flags(msg),
				purple_conversation_message_get_timestamp(msg));
		if (im) {
			gtkconv->attach.current = g_list_delete_link(gtkconv->attach.current, gtkconv->attach.current);
		} else {
			gtkconv->attach.current = gtkconv->attach.current->prev;
		}
		count++;
	}
	gtkconv->attach.timer = timer;
	if (gtkconv->attach.current)
		return TRUE;

	g_source_remove(gtkconv->attach.timer);
	gtkconv->attach.timer = 0;
	if (im) {
		/* Print any message that was sent while the old history was being added back. */
		GList *msgs = NULL;
		GList *iter = gtkconv->convs;
		for (; iter; iter = iter->next) {
			PurpleConversation *conv = iter->data;
			GList *history = purple_conversation_get_message_history(conv);
			for (; history; history = history->next) {
				PurpleConversationMessage *msg = history->data;
				if (purple_conversation_message_get_timestamp(msg) > when)
					msgs = g_list_prepend(msgs, msg);
			}
		}
		msgs = g_list_sort(msgs, message_compare);
		for (; msgs; msgs = g_list_delete_link(msgs, msgs)) {
			PurpleConversationMessage *msg = msgs->data;
			pidgin_conv_write_conv(
					purple_conversation_message_get_conversation(msg),
					purple_conversation_message_get_sender(msg),
					purple_conversation_message_get_alias(msg),
					purple_conversation_message_get_message(msg),
					purple_conversation_message_get_flags(msg),
					purple_conversation_message_get_timestamp(msg));
		}
		gtk_webview_append_html(webview, "<BR><HR>");
		gtk_webview_scroll_to_end(webview, TRUE);
		g_object_set_data(G_OBJECT(gtkconv->entry), "attach-start-time", NULL);
	}

	g_object_set_data(G_OBJECT(gtkconv->entry), "attach-start-time", NULL);
	purple_signal_emit(pidgin_conversations_get_handle(),
			"conversation-displayed", gtkconv);
	return FALSE;
}

static void
pidgin_conv_attach(PurpleConversation *conv)
{
	int timer;
	g_object_set_data(G_OBJECT(conv), "unseen-count", NULL);
	g_object_set_data(G_OBJECT(conv), "unseen-state", NULL);
	purple_conversation_set_ui_ops(conv, pidgin_conversations_get_conv_ui_ops());
	purple_conversation_set_ui_data(conv, NULL);
	private_gtkconv_new(conv, FALSE);
	timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
	if (timer) {
		purple_timeout_remove(timer);
		g_object_set_data(G_OBJECT(conv), "close-timer", NULL);
	}
}

gboolean pidgin_conv_attach_to_conversation(PurpleConversation *conv)
{
	GList *list;
	PidginConversation *gtkconv;

	if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
		/* This is pretty much always the case now. */
		gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv->win != hidden_convwin)
			return FALSE;
		pidgin_conv_window_remove_gtkconv(hidden_convwin, gtkconv);
		pidgin_conv_placement_place(gtkconv);
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", gtkconv);
		list = gtkconv->convs;
		while (list) {
			pidgin_conv_attach(list->data);
			list = list->next;
		}
		return TRUE;
	}

	pidgin_conv_attach(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);

	list = purple_conversation_get_message_history(conv);
	if (list) {
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			GList *convs;
			list = g_list_copy(list);
			for (convs = purple_conversations_get_ims(); convs; convs = convs->next)
				if (convs->data != conv &&
						pidgin_conv_find_gtkconv(convs->data) == gtkconv) {
					pidgin_conv_attach(convs->data);
					list = g_list_concat(list, g_list_copy(purple_conversation_get_message_history(convs->data)));
				}
			list = g_list_sort(list, message_compare);
			gtkconv->attach.current = list;
			list = g_list_last(list);
		} else {
			gtkconv->attach.current = g_list_last(list);
		}
	
		g_object_set_data(G_OBJECT(gtkconv->entry), "attach-start-time",
				GINT_TO_POINTER(purple_conversation_message_get_timestamp((PurpleConversationMessage*)(list->data))));
		gtkconv->attach.timer = g_idle_add(add_message_history_to_gtkconv, gtkconv);
	} else {
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", gtkconv);
	}

	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(conv);
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TOPIC);
		pidgin_conv_chat_add_users(chat, purple_chat_conversation_get_users(chat), TRUE);
	}

	return TRUE;
}

PurpleTheme *
pidgin_conversations_get_default_theme(void)
{
	return default_conv_theme;
}

void *
pidgin_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

void
pidgin_conversations_init(void)
{
	void *handle = pidgin_conversations_get_handle();
	void *blist_handle = purple_blist_get_handle();
	char *theme_dir;

	/* Conversations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/themes");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size", 96);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines", 2);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/placement", "last");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/placement_number", 1);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/font_size", 3);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/tabs", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/tab_side", GTK_POS_TOP);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/scrollback_lines", 4000);

#ifdef _WIN32
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font", TRUE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/custom_font", "");
#endif

	/* Conversations -> Chat */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/chat");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height", 54);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", 80);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/x", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/y", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/width", 340);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/height", 390);

	/* Conversations -> IM */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/im");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/x", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/y", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/width", 340);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/height", 390);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons", TRUE);

	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height", 54);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "never");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately", TRUE);

#ifdef _WIN32
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/win32/minimize_new_convs", FALSE);
#endif

	/* Connect callbacks. */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/close_on_tabs",
								close_on_tabs_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
								show_formatting_toolbar_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/spellcheck",
								spellcheck_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tab_side",
								tab_side_pref_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tabs",
								conv_placement_usetabs_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/placement",
								conv_placement_pref_cb, NULL);
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		minimum_entry_lines_pref_cb, NULL);

	/* IM callbacks */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons",
								animate_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons",
								show_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_protocol_icons",
								show_protocol_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
								hide_new_pref_cb, NULL);



	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	purple_signal_register(handle, "conversation-dragging",
	                     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
	                     G_TYPE_POINTER, /* pointer to a (PidginWindow *) */
	                     G_TYPE_POINTER); /* pointer to a (PidginWindow *) */

	purple_signal_register(handle, "conversation-timestamp",
#if SIZEOF_TIME_T == 4
	                     purple_marshal_POINTER__POINTER_INT_BOOLEAN,
#elif SIZEOF_TIME_T == 8
			     purple_marshal_POINTER__POINTER_INT64_BOOLEAN,
#else
#error Unkown size of time_t
#endif
	                     G_TYPE_STRING, 3, PURPLE_TYPE_CONVERSATION,
#if SIZEOF_TIME_T == 4
	                     G_TYPE_INT,
#elif SIZEOF_TIME_T == 8
	                     G_TYPE_INT64,
#else
# error Unknown size of time_t
#endif
	                     G_TYPE_BOOLEAN);

	purple_signal_register(handle, "displaying-im-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION, G_TYPE_INT);

	purple_signal_register(handle, "displayed-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_INT);

	purple_signal_register(handle, "displaying-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_POINTER, /* pointer to a string */
						 PURPLE_TYPE_CONVERSATION, G_TYPE_INT);

	purple_signal_register(handle, "displayed-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 G_TYPE_STRING, PURPLE_TYPE_CONVERSATION, G_TYPE_INT);

	purple_signal_register(handle, "conversation-switched",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "conversation-hiding",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 G_TYPE_POINTER); /* (PidginConversation *) */

	purple_signal_register(handle, "conversation-displayed",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 G_TYPE_POINTER); /* (PidginConversation *) */

	purple_signal_register(handle, "chat-nick-autocomplete",
						 purple_marshal_BOOLEAN__POINTER_BOOLEAN,
						 G_TYPE_BOOLEAN, 1, PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "chat-nick-clicked",
						 purple_marshal_BOOLEAN__POINTER_POINTER_UINT,
						 G_TYPE_BOOLEAN, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_UINT);


	/**********************************************************************
	 * Register commands
	 **********************************************************************/
	purple_cmd_register("say", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  say_command_cb, _("say &lt;message&gt;:  Send a message normally as if you weren't using a command."), NULL);
	purple_cmd_register("me", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  me_command_cb, _("me &lt;action&gt;:  Send an IRC style action to a buddy or chat."), NULL);
	purple_cmd_register("debug", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  debug_command_cb, _("debug &lt;option&gt;:  Send various debug information to the current conversation."), NULL);
	purple_cmd_register("clear", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  clear_command_cb, _("clear: Clears the conversation scrollback."), NULL);
	purple_cmd_register("clearall", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  clearall_command_cb, _("clear: Clears all conversation scrollbacks."), NULL);
	purple_cmd_register("help", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  help_command_cb, _("help &lt;command&gt;:  Help on a specific command."), NULL);

	/**********************************************************************
	 * UI operations
	 **********************************************************************/

	purple_signal_connect(purple_connections_get_handle(), "signed-on", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONVERSATION_ACCOUNT_ONLINE));
	purple_signal_connect(purple_connections_get_handle(), "signed-off", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONVERSATION_ACCOUNT_OFFLINE));
	purple_signal_connect(purple_connections_get_handle(), "signing-off", handle,
						G_CALLBACK(account_signing_off), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg",
						handle, G_CALLBACK(received_im_msg_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "cleared-message-history",
	                      handle, G_CALLBACK(clear_conversation_scrollback_cb), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "deleting-chat-user",
	                      handle, G_CALLBACK(deleting_chat_user_cb), NULL);

	purple_conversations_set_ui_ops(&conversation_ui_ops);

	hidden_convwin = pidgin_conv_window_new();
	window_list = g_list_remove(window_list, hidden_convwin);

	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed",
						handle, PURPLE_CALLBACK(account_status_changed_cb), NULL);

	/* Callbacks to update a conversation */
	purple_signal_connect(blist_handle, "blist-node-added", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "blist-node-removed", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-on",
						handle, PURPLE_CALLBACK(update_buddy_sign), "on");
	purple_signal_connect(blist_handle, "buddy-signed-off",
						handle, PURPLE_CALLBACK(update_buddy_sign), "off");
	purple_signal_connect(blist_handle, "buddy-status-changed",
						handle, PURPLE_CALLBACK(update_buddy_status_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-privacy-changed",
						handle, PURPLE_CALLBACK(update_buddy_privacy_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-idle-changed",
						handle, PURPLE_CALLBACK(update_buddy_idle_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-icon-changed",
						handle, PURPLE_CALLBACK(update_buddy_icon), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing-stopped",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-switched",
						handle, PURPLE_CALLBACK(update_conversation_switched), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-left", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-joined", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", handle,
						PURPLE_CALLBACK(update_chat_topic), NULL);
	purple_signal_connect_priority(purple_conversations_get_handle(), "conversation-updated", handle,
						PURPLE_CALLBACK(pidgin_conv_updated), NULL,
						PURPLE_SIGNAL_PRIORITY_LOWEST);
	purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg", handle,
			PURPLE_CALLBACK(wrote_msg_update_unseen_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "wrote-chat-msg", handle,
			PURPLE_CALLBACK(wrote_msg_update_unseen_cb), NULL);

	purple_theme_manager_register_type(g_object_new(PIDGIN_TYPE_CONV_THEME_LOADER, "type", "conversation", NULL));
#ifdef _WIN32
	theme_dir = g_build_filename(DATADIR, "theme", NULL);
#else
	theme_dir = g_build_filename(DATADIR, "pidgin", "theme", NULL);
#endif
	default_conv_theme = purple_theme_manager_load_theme(theme_dir, "conversation");
	g_free(theme_dir);

#if !GTK_CHECK_VERSION(3,0,0)
	{
		/* Set default tab colors */
		GString *str = g_string_new(NULL);
		GtkSettings *settings = gtk_settings_get_default();
		GtkStyle *parent = gtk_rc_get_style_by_paths(settings, "tab-container.tab-label*", NULL, G_TYPE_NONE), *now;
		struct {
			const char *stylename;
			const char *labelname;
			const char *color;
		} styles[] = {
			{"pidgin_tab_label_typing_default", "tab-label-typing", "#4e9a06"},
			{"pidgin_tab_label_typed_default", "tab-label-typed", "#c4a000"},
			{"pidgin_tab_label_attention_default", "tab-label-attention", "#006aff"},
			{"pidgin_tab_label_unreadchat_default", "tab-label-unreadchat", "#cc0000"},
			{"pidgin_tab_label_event_default", "tab-label-event", "#888a85"},
			{NULL, NULL, NULL}
		};
		int iter;
		for (iter = 0; styles[iter].stylename; iter++) {
			now = gtk_rc_get_style_by_paths(settings, styles[iter].labelname, NULL, G_TYPE_NONE);
			if (parent == now ||
					(parent && now && parent->rc_style == now->rc_style)) {
				g_string_append_printf(str, "style \"%s\" {\n"
						"fg[ACTIVE] = \"%s\"\n"
						"}\n"
						"widget \"*%s\" style \"%s\"\n",
						styles[iter].stylename,
						styles[iter].color,
						styles[iter].labelname, styles[iter].stylename);
			}
		}
		gtk_rc_parse_string(str->str);
		g_string_free(str, TRUE);
		gtk_rc_reset_styles(settings);
	}
#endif
}

void
pidgin_conversations_uninit(void)
{
	purple_prefs_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_unregister_by_instance(pidgin_conversations_get_handle());
}
















/* down here is where gtkconvwin.c ought to start. except they share like every freaking function,
 * and touch each others' private members all day long */

/**
 * @file gtkconvwin.c GTK+ Conversation Window API
 * @ingroup pidgin
 *
 * pidgin
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
 *
 */
#include "internal.h"
#include "pidgin.h"


#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "cmds.h"
#include "debug.h"
#include "imgstore.h"
#include "log.h"
#include "notify.h"
#include "protocol.h"
#include "request.h"
#include "util.h"

#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkutils.h"
#include "pidginstock.h"

static void
do_close(GtkWidget *w, int resp, PidginWindow *win)
{
	gtk_widget_destroy(warn_close_dialog);
	warn_close_dialog = NULL;

	if (resp == GTK_RESPONSE_OK)
		pidgin_conv_window_destroy(win);
}

static void
build_warn_close_dialog(PidginWindow *gtkwin)
{
	GtkWidget *label, *vbox, *hbox, *img;

	g_return_if_fail(warn_close_dialog == NULL);

	warn_close_dialog = gtk_dialog_new_with_buttons(_("Confirm close"),
							GTK_WINDOW(gtkwin->window), GTK_DIALOG_MODAL,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(warn_close_dialog),
	                                GTK_RESPONSE_OK);

	gtk_container_set_border_width(GTK_CONTAINER(warn_close_dialog),
	                               6);
	gtk_window_set_resizable(GTK_WINDOW(warn_close_dialog), FALSE);

	/* Setup the outside spacing. */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(warn_close_dialog));

	gtk_box_set_spacing(GTK_BOX(vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_WARNING,
	                               gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	/* Setup the inner hbox and put the dialog's icon in it. */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	/* Setup the right vbox. */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("You have unread messages. Are you sure you want to close the window?"));
	gtk_widget_set_size_request(label, 350, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* Connect the signals. */
	g_signal_connect(G_OBJECT(warn_close_dialog), "response",
	                 G_CALLBACK(do_close), gtkwin);

}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gboolean
close_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginWindow *win = d;
	GList *l;

	/* If there are unread messages then show a warning dialog */
	for (l = pidgin_conv_window_get_gtkconvs(win);
	     l != NULL; l = l->next)
	{
		PidginConversation *gtkconv = l->data;
		if (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv) &&
				gtkconv->unseen_state >= PIDGIN_UNSEEN_TEXT)
		{
			build_warn_close_dialog(win);
			gtk_widget_show_all(warn_close_dialog);

			return TRUE;
		}
	}

	pidgin_conv_window_destroy(win);

	return TRUE;
}

static void
conv_set_unseen(PurpleConversation *conv, PidginUnseenState state)
{
	int unseen_count = 0;
	PidginUnseenState unseen_state = PIDGIN_UNSEEN_NONE;

	if(g_object_get_data(G_OBJECT(conv), "unseen-count"))
		unseen_count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "unseen-count"));

	if(g_object_get_data(G_OBJECT(conv), "unseen-state"))
		unseen_state = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "unseen-state"));

	if (state == PIDGIN_UNSEEN_NONE)
	{
		unseen_count = 0;
		unseen_state = PIDGIN_UNSEEN_NONE;
	}
	else
	{
		if (state >= PIDGIN_UNSEEN_TEXT)
			unseen_count++;

		if (state > unseen_state)
			unseen_state = state;
	}

	g_object_set_data(G_OBJECT(conv), "unseen-count", GINT_TO_POINTER(unseen_count));
	g_object_set_data(G_OBJECT(conv), "unseen-state", GINT_TO_POINTER(unseen_state));

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
}

static void
gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state)
{
	if (state == PIDGIN_UNSEEN_NONE)
	{
		gtkconv->unseen_count = 0;
		gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	}
	else
	{
		if (state >= PIDGIN_UNSEEN_TEXT)
			gtkconv->unseen_count++;

		if (state > gtkconv->unseen_state)
			gtkconv->unseen_state = state;
	}

	g_object_set_data(G_OBJECT(gtkconv->active_conv), "unseen-count", GINT_TO_POINTER(gtkconv->unseen_count));
	g_object_set_data(G_OBJECT(gtkconv->active_conv), "unseen-state", GINT_TO_POINTER(gtkconv->unseen_state));

	purple_conversation_update(gtkconv->active_conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
}

/*
 * When a conversation window is focused, we know the user
 * has looked at it so we know there are no longer unseen
 * messages.
 */
static gboolean
focus_win_cb(GtkWidget *w, GdkEventFocus *e, gpointer d)
{
	PidginWindow *win = d;
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	if (gtkconv)
		gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);

	return FALSE;
}

static void
notebook_init_grab(PidginWindow *gtkwin, GtkWidget *widget, GdkEvent *event)
{
	static GdkCursor *cursor = NULL;
#if GTK_CHECK_VERSION(3,0,0)
	GdkDevice *device;
#endif

	gtkwin->in_drag = TRUE;

	if (gtkwin->drag_leave_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            gtkwin->drag_leave_signal);
		gtkwin->drag_leave_signal = 0;
	}

	if (cursor == NULL)
		cursor = gdk_cursor_new(GDK_FLEUR);

	/* Grab the pointer */
	gtk_grab_add(gtkwin->notebook);
#if GTK_CHECK_VERSION(3,0,0)
	device = gdk_event_get_device(event);
	if (!gdk_display_device_is_grabbed(gdk_device_get_display(device), device))
		gdk_device_grab(device, gtk_widget_get_window(gtkwin->notebook),
		                GDK_OWNERSHIP_WINDOW, FALSE,
		                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
		                cursor, gdk_event_get_time(event));
#else
#ifndef _WIN32
	/* Currently for win32 GTK+ (as of 2.2.1), gdk_pointer_is_grabbed will
	   always be true after a button press. */
	if (!gdk_pointer_is_grabbed())
#endif
		gdk_pointer_grab(gtk_widget_get_window(gtkwin->notebook), FALSE,
		                 GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
		                 NULL, cursor, gdk_event_get_time(event));
#endif
}

static gboolean
notebook_motion_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{

	/*
	* Make sure the user moved the mouse far enough for the
	* drag to be initiated.
	*/
	if (win->in_predrag) {
		if (e->x_root <  win->drag_min_x ||
		    e->x_root >= win->drag_max_x ||
		    e->y_root <  win->drag_min_y ||
		    e->y_root >= win->drag_max_y) {

			    win->in_predrag = FALSE;
			    notebook_init_grab(win, widget, (GdkEvent *)e);
		    }
	}
	else { /* Otherwise, draw the arrows. */
		PidginWindow *dest_win;
		GtkNotebook *dest_notebook;
		GtkWidget *tab;
		gint page_num;
		gboolean horiz_tabs = FALSE;
		gboolean to_right = FALSE;

		/* Get the window that the cursor is over. */
		dest_win = pidgin_conv_window_get_at_event((GdkEvent *)e);

		if (dest_win == NULL) {
			dnd_hints_hide_all();

			return TRUE;
		}

		dest_notebook = GTK_NOTEBOOK(dest_win->notebook);

		if (gtk_notebook_get_show_tabs(dest_notebook)) {
			page_num = pidgin_conv_get_tab_at_xy(dest_win,
			                                      e->x_root, e->y_root, &to_right);
			to_right = to_right && (win != dest_win);
			tab = pidgin_conv_window_get_gtkconv_at_index(dest_win, page_num)->tabby;
		} else {
			page_num = 0;
			to_right = pidgin_conv_xy_to_right_infopane(dest_win, e->x_root, e->y_root);
			tab = pidgin_conv_window_get_gtkconv_at_index(dest_win, page_num)->infopane_hbox;
		}

		if (gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_TOP ||
				gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_BOTTOM) {
			horiz_tabs = TRUE;
		}

		if (gtk_notebook_get_show_tabs(dest_notebook) == FALSE && win == dest_win)
		{
			/* dragging a tab from a single-tabbed window over its own window */
			dnd_hints_hide_all();
			return TRUE;
		} else if (horiz_tabs) {
			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				dnd_hints_show_relative(HINT_ARROW_DOWN, tab, HINT_POSITION_RIGHT, HINT_POSITION_TOP);
				dnd_hints_show_relative(HINT_ARROW_UP, tab, HINT_POSITION_RIGHT, HINT_POSITION_BOTTOM);
			} else {
				dnd_hints_show_relative(HINT_ARROW_DOWN, tab, HINT_POSITION_LEFT, HINT_POSITION_TOP);
				dnd_hints_show_relative(HINT_ARROW_UP, tab, HINT_POSITION_LEFT, HINT_POSITION_BOTTOM);
			}
		} else {
			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				dnd_hints_show_relative(HINT_ARROW_RIGHT, tab, HINT_POSITION_LEFT, HINT_POSITION_BOTTOM);
				dnd_hints_show_relative(HINT_ARROW_LEFT, tab, HINT_POSITION_RIGHT, HINT_POSITION_BOTTOM);
			} else {
				dnd_hints_show_relative(HINT_ARROW_RIGHT, tab, HINT_POSITION_LEFT, HINT_POSITION_TOP);
				dnd_hints_show_relative(HINT_ARROW_LEFT, tab, HINT_POSITION_RIGHT, HINT_POSITION_TOP);
			}
		}
	}

	return TRUE;
}

static gboolean
notebook_leave_cb(GtkWidget *widget, GdkEventCrossing *e, PidginWindow *win)
{
	if (win->in_drag)
		return FALSE;

	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		win->in_predrag = FALSE;
		notebook_init_grab(win, widget, (GdkEvent *)e);
	}

	return TRUE;
}

/*
 * THANK YOU GALEON!
 */

static gboolean
infopane_press_cb(GtkWidget *widget, GdkEventButton *e, PidginConversation *gtkconv)
{
	if (e->type == GDK_2BUTTON_PRESS && e->button == 1) {
		if (infopane_entry_activate(gtkconv))
			return TRUE;
	}

	if (e->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (e->button == 1) {
		int nb_x, nb_y;
		GtkAllocation allocation;

		gtk_widget_get_allocation(gtkconv->infopane_hbox, &allocation);

		if (gtkconv->win->in_drag)
			return TRUE;

		gtkconv->win->in_predrag = TRUE;
		gtkconv->win->drag_tab = gtk_notebook_page_num(GTK_NOTEBOOK(gtkconv->win->notebook), gtkconv->tab_cont);

		gdk_window_get_origin(gtk_widget_get_window(gtkconv->infopane_hbox), &nb_x, &nb_y);

		gtkconv->win->drag_min_x = allocation.x + nb_x;
		gtkconv->win->drag_min_y = allocation.y + nb_y;
		gtkconv->win->drag_max_x = allocation.width + gtkconv->win->drag_min_x;
		gtkconv->win->drag_max_y = allocation.height + gtkconv->win->drag_min_y;

		gtkconv->win->drag_motion_signal = g_signal_connect(G_OBJECT(gtkconv->win->notebook), "motion_notify_event",
								    G_CALLBACK(notebook_motion_cb), gtkconv->win);
		gtkconv->win->drag_leave_signal = g_signal_connect(G_OBJECT(gtkconv->win->notebook), "leave_notify_event",
								    G_CALLBACK(notebook_leave_cb), gtkconv->win);
		return FALSE;
	}

	if (e->button == 3) {
		/* Right click was pressed. Popup the context menu. */
		GtkWidget *menu = gtk_menu_new(), *sub;
		gboolean populated = populate_menu_with_options(menu, gtkconv, TRUE);

		sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(gtkconv->win->menu.send_to));
		if (sub && gtk_widget_is_sensitive(gtkconv->win->menu.send_to)) {
			GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("S_end To"));
			if (populated)
				pidgin_separator(menu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub);
			gtk_widget_show(item);
			gtk_widget_show_all(sub);
		} else if (!populated) {
			gtk_widget_destroy(menu);
			return FALSE;
		}

		gtk_widget_show_all(menu);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, e->button, e->time);
		return TRUE;
	}
	return FALSE;
}

static gboolean
notebook_press_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{
	gint nb_x, nb_y;
	int tab_clicked;
	GtkWidget *page;
	GtkWidget *tab;
	GtkAllocation allocation;

	if (e->button == 2 && e->type == GDK_BUTTON_PRESS) {
		PidginConversation *gtkconv;
		tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

		if (tab_clicked == -1)
			return FALSE;

		gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, tab_clicked);
		close_conv_cb(NULL, gtkconv);
		return TRUE;
	}


	if (e->button != 1 || e->type != GDK_BUTTON_PRESS)
		return FALSE;


	if (win->in_drag) {
		purple_debug(PURPLE_DEBUG_WARNING, "gtkconv",
		           "Already in the middle of a window drag at tab_press_cb\n");
		return TRUE;
	}

	/*
	* Make sure a tab was actually clicked. The arrow buttons
	* mess things up.
	*/
	tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

	if (tab_clicked == -1)
		return FALSE;

	/*
	* Get the relative position of the press event, with regards to
	* the position of the notebook.
	*/
	gdk_window_get_origin(gtk_widget_get_window(win->notebook), &nb_x, &nb_y);

	/* Reset the min/max x/y */
	win->drag_min_x = 0;
	win->drag_min_y = 0;
	win->drag_max_x = 0;
	win->drag_max_y = 0;

	/* Find out which tab was dragged. */
	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), tab_clicked);
	tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(win->notebook), page);

	gtk_widget_get_allocation(tab, &allocation);

	win->drag_min_x = allocation.x      + nb_x;
	win->drag_min_y = allocation.y      + nb_y;
	win->drag_max_x = allocation.width  + win->drag_min_x;
	win->drag_max_y = allocation.height + win->drag_min_y;

	/* Make sure the click occurred in the tab. */
	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		return FALSE;
	}

	win->in_predrag = TRUE;
	win->drag_tab = tab_clicked;

	/* Connect the new motion signals. */
	win->drag_motion_signal =
		g_signal_connect(G_OBJECT(widget), "motion_notify_event",
		                 G_CALLBACK(notebook_motion_cb), win);

	win->drag_leave_signal =
		g_signal_connect(G_OBJECT(widget), "leave_notify_event",
		                 G_CALLBACK(notebook_leave_cb), win);

	return FALSE;
}

static gboolean
notebook_release_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{
	PidginWindow *dest_win;
	GtkNotebook *dest_notebook;
	PidginConversation *active_gtkconv;
	PidginConversation *gtkconv;
	gint dest_page_num = 0;
	gboolean new_window = FALSE;
	gboolean to_right = FALSE;
#if GTK_CHECK_VERSION(3,0,0)
	GdkDevice *device;
#endif

	/*
	* Don't check to make sure that the event's window matches the
	* widget's, because we may be getting an event passed on from the
	* close button.
	*/
	if (e->button != 1 && e->type != GDK_BUTTON_RELEASE)
		return FALSE;

#if GTK_CHECK_VERSION(3,0,0)
	device = gdk_event_get_device((GdkEvent *)e);
	if (gdk_display_device_is_grabbed(gdk_device_get_display(device), device)) {
		gdk_device_ungrab(device, gdk_event_get_time((GdkEvent *)e));
		gtk_grab_remove(widget);
	}
#else
	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(gdk_event_get_time((GdkEvent *)e));
		gtk_grab_remove(widget);
	}
#endif

	if (!win->in_predrag && !win->in_drag)
		return FALSE;

	/* Disconnect the motion signal. */
	if (win->drag_motion_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            win->drag_motion_signal);

		win->drag_motion_signal = 0;
	}

	/*
	* If we're in a pre-drag, we'll also need to disconnect the leave
	* signal.
	*/
	if (win->in_predrag) {
		win->in_predrag = FALSE;

		if (win->drag_leave_signal) {
			g_signal_handler_disconnect(G_OBJECT(widget),
			                            win->drag_leave_signal);

			win->drag_leave_signal = 0;
		}
	}

	/* If we're not in drag...        */
	/* We're perfectly normal people! */
	if (!win->in_drag)
		return FALSE;

	win->in_drag = FALSE;

	dnd_hints_hide_all();

	dest_win = pidgin_conv_window_get_at_event((GdkEvent *)e);

	active_gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	if (dest_win == NULL) {
		/* If the current window doesn't have any other conversations,
		* there isn't much point transferring the conv to a new window. */
		if (pidgin_conv_window_get_gtkconv_count(win) > 1) {
			/* Make a new window to stick this to. */
			dest_win = pidgin_conv_window_new();
			new_window = TRUE;
		}
	}

	if (dest_win == NULL)
		return FALSE;

	purple_signal_emit(pidgin_conversations_get_handle(),
	                 "conversation-dragging", win, dest_win);

	/* Get the destination page number. */
	if (!new_window) {
		dest_notebook = GTK_NOTEBOOK(dest_win->notebook);
		if (gtk_notebook_get_show_tabs(dest_notebook)) {
			dest_page_num = pidgin_conv_get_tab_at_xy(dest_win,
			                                           e->x_root, e->y_root, &to_right);
		} else {
			dest_page_num = 0;
			to_right = pidgin_conv_xy_to_right_infopane(dest_win, e->x_root, e->y_root);
		}
	}

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, win->drag_tab);

	if (win == dest_win) {
		gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, dest_page_num);
	} else {
		pidgin_conv_window_remove_gtkconv(win, gtkconv);
		pidgin_conv_window_add_gtkconv(dest_win, gtkconv);
		gtk_notebook_reorder_child(GTK_NOTEBOOK(dest_win->notebook), gtkconv->tab_cont, dest_page_num + to_right);
		pidgin_conv_window_switch_gtkconv(dest_win, gtkconv);
		if (new_window) {
			gint win_width, win_height;

			gtk_window_get_size(GTK_WINDOW(dest_win->window),
			                    &win_width, &win_height);
#ifdef _WIN32  /* only override window manager placement on Windows */
			gtk_window_move(GTK_WINDOW(dest_win->window),
			                e->x_root - (win_width  / 2),
			                e->y_root - (win_height / 2));
#endif

			pidgin_conv_window_show(dest_win);
		}
	}

	gtk_widget_grab_focus(active_gtkconv->entry);

	return TRUE;
}


static void
before_switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
                      gpointer user_data)
{
	PidginWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	win = user_data;
	conv = pidgin_conv_window_get_active_conversation(win);

	g_return_if_fail(conv != NULL);

	if (PURPLE_IS_CHAT_CONVERSATION(conv))
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	if (gtkconv->u.im->typing_timer != 0) {
		g_source_remove(gtkconv->u.im->typing_timer);
		gtkconv->u.im->typing_timer = 0;
	}

	stop_anim(NULL, gtkconv);
}

static void
close_window(GtkWidget *w, PidginWindow *win)
{
	close_win_cb(w, NULL, win);
}

static void
detach_tab_cb(GtkWidget *w, PidginWindow *win)
{
	PidginWindow *new_window;
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (!gtkconv)
		return;

	/* Nothing to do if there's only one tab in the window */
	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		return;

	pidgin_conv_window_remove_gtkconv(win, gtkconv);

	new_window = pidgin_conv_window_new();
	pidgin_conv_window_add_gtkconv(new_window, gtkconv);
	pidgin_conv_window_show(new_window);
}

static void
close_others_cb(GtkWidget *w, PidginWindow *win)
{
	GList *iter;
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (!gtkconv)
		return;

	for (iter = pidgin_conv_window_get_gtkconvs(win); iter; )
	{
		PidginConversation *gconv = iter->data;
		iter = iter->next;

		if (gconv != gtkconv)
		{
			close_conv_cb(NULL, gconv);
		}
	}
}

static void
close_tab_cb(GtkWidget *w, PidginWindow *win)
{
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (gtkconv)
		close_conv_cb(NULL, gtkconv);
}

static void
notebook_menu_switch_cb(GtkWidget *item, GtkWidget *child)
{
	GtkNotebook *notebook;
	int index;

	notebook = GTK_NOTEBOOK(gtk_widget_get_parent(child));
	index = gtk_notebook_page_num(notebook, child);
	gtk_notebook_set_current_page(notebook, index);
}

static void
notebook_menu_update_label_cb(GtkWidget *child, GParamSpec *pspec,
                              GtkNotebook *notebook)
{
	GtkWidget *item;
	GtkWidget *label;

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	label = gtk_bin_get_child(GTK_BIN(item));
	if (label)
		gtk_container_remove(GTK_CONTAINER(item), label);

	label = gtk_notebook_get_menu_label(notebook, child);
	if (label) {
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(item), label);
		gtk_widget_show(item);
	} else {
		gtk_widget_hide(item);
	}
}

static void
notebook_add_tab_to_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                            guint page_num, PidginWindow *win)
{
	GtkWidget *item;
	GtkWidget *label;

	item = gtk_menu_item_new();
	label = gtk_notebook_get_menu_label(notebook, child);
	if (label) {
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(item), label);
		gtk_widget_show(item);
	}

	g_signal_connect(child, "child-notify::menu-label",
	                 G_CALLBACK(notebook_menu_update_label_cb), notebook); 
	g_signal_connect(item, "activate",
	                 G_CALLBACK(notebook_menu_switch_cb), child);
	g_object_set_data(G_OBJECT(child), "popup-menu-item", item);

	gtk_menu_shell_insert(GTK_MENU_SHELL(win->notebook_menu), item, page_num);
}

static void
notebook_remove_tab_from_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                                 guint page_num, PidginWindow *win)
{
	GtkWidget *item;

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	gtk_container_remove(GTK_CONTAINER(win->notebook_menu), item);
}


static void
notebook_reorder_tab_in_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                                guint page_num, PidginWindow *win)
{
	GtkWidget *item;

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	gtk_menu_reorder_child(GTK_MENU(win->notebook_menu), item, page_num);
}

static gboolean
notebook_right_click_menu_cb(GtkNotebook *notebook, GdkEventButton *event,
                             PidginWindow *win)
{
	GtkWidget *menu;
	PidginConversation *gtkconv;

	if (event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win,
			pidgin_conv_get_tab_at_xy(win, event->x_root, event->y_root, NULL));

	win->clicked_tab = gtkconv;

	menu = win->notebook_menu;

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);

	return TRUE;
}

static void
remove_edit_entry(PidginConversation *gtkconv, GtkWidget *entry)
{
	g_signal_handlers_disconnect_matched(G_OBJECT(entry), G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, gtkconv);
	gtk_widget_show(gtkconv->infopane);
	gtk_widget_grab_focus(gtkconv->entry);
	gtk_widget_destroy(entry);
}

static gboolean
alias_focus_cb(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	remove_edit_entry(user_data, widget);
	return FALSE;
}

static gboolean
alias_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Escape) {
		remove_edit_entry(user_data, widget);
		return TRUE;
	}
	return FALSE;
}

static void
alias_cb(GtkEntry *entry, gpointer user_data)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PurpleAccount *account;
	const char *name;

	gtkconv = (PidginConversation *)user_data;
	if (gtkconv == NULL) {
		return;
	}
	conv    = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *buddy;
		buddy = purple_blist_find_buddy(account, name);
		if (buddy != NULL) {
			purple_buddy_set_local_alias(buddy, gtk_entry_get_text(entry));
		}
		serv_alias_buddy(buddy);
	} else {
		gtk_entry_set_text(GTK_ENTRY(gtkconv->u.chat->topic_text), gtk_entry_get_text(entry));
		topic_callback(NULL, gtkconv);
	}
	remove_edit_entry(user_data, GTK_WIDGET(entry));
}

static gboolean
infopane_entry_activate(PidginConversation *gtkconv)
{
	GtkWidget *entry = NULL;
	PurpleConversation *conv = gtkconv->active_conv;
	const char *text = NULL;

	if (!gtk_widget_get_visible(gtkconv->infopane)) {
		/* There's already an entry for alias. Let's not create another one. */
		return FALSE;
	}

	if (!purple_account_is_connected(purple_conversation_get_account(gtkconv->active_conv))) {
		/* Do not allow aliasing someone on a disconnected account. */
		return FALSE;
	}

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *buddy = purple_blist_find_buddy(purple_conversation_get_account(gtkconv->active_conv), purple_conversation_get_name(gtkconv->active_conv));
		if (!buddy)
			/* This buddy isn't in your buddy list, so we can't alias him */
			return FALSE;

		text = purple_buddy_get_contact_alias(buddy);
	} else {
		PurpleConnection *gc;
		PurpleProtocol *protocol = NULL;

		gc = purple_conversation_get_connection(conv);
		if (gc != NULL)
			protocol = purple_connection_get_protocol_info(gc);
		if (protocol && protocol->set_chat_topic == NULL)
			/* This protocol doesn't support setting the chat room topic */
			return FALSE;

		text = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));
	}

	/* alias label */
	entry = gtk_entry_new();
	gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
	gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5);

	gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), entry, TRUE, TRUE, 0);
	/* after the tab label */
	gtk_box_reorder_child(GTK_BOX(gtkconv->infopane_hbox), entry, 0);

	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(alias_cb), gtkconv);
	g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(alias_focus_cb), gtkconv);
	g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(alias_key_press_cb), gtkconv);

	if (text != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_widget_show(entry);
	gtk_widget_hide(gtkconv->infopane);
	gtk_widget_grab_focus(entry);

	return TRUE;
}

static gboolean
window_keypress_cb(GtkWidget *widget, GdkEventKey *event, PidginWindow *win)
{
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	return conv_keypress_common(gtkconv, event);
}

static void
switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
               gpointer user_data)
{
	PidginWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	const char *sound_method;

	win = user_data;
	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, page_num);
	conv = gtkconv->active_conv;

	g_return_if_fail(conv != NULL);

	/* clear unseen flag if conversation is not hidden */
	if(!pidgin_conv_is_hidden(gtkconv)) {
		gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
	}

	/* Update the menubar */

	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(gtkconv->win->menu.logging),
	                             purple_conversation_is_logging(conv));

	generate_send_to_items(win);
	regenerate_options_items(win);
	regenerate_plugins_items(win);

	pidgin_conv_switch_active_conversation(conv);

	sound_method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (strcmp(sound_method, "none") != 0)
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu.sounds),
		                             gtkconv->make_sound);

	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu.show_formatting_toolbar),
	                             purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar"));

	/*
	 * We pause icons when they are not visible.  If this icon should
	 * be animated then start it back up again.
	 */
	if (PURPLE_IS_IM_CONVERSATION(conv) &&
	    (gtkconv->u.im->animate))
		start_anim(NULL, gtkconv);

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);
}

/**************************************************************************
 * GTK+ window ops
 **************************************************************************/

GList *
pidgin_conv_windows_get_list()
{
	return window_list;
}

static GList*
make_status_icon_list(const char *stock, GtkWidget *w)
{
	GList *l = NULL;
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MEDIUM), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE), "GtkWindow"));
	return l;
}

static void
create_icon_lists(GtkWidget *w)
{
	available_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AVAILABLE, w);
	busy_list = make_status_icon_list(PIDGIN_STOCK_STATUS_BUSY, w);
	xa_list = make_status_icon_list(PIDGIN_STOCK_STATUS_XA, w);
	offline_list = make_status_icon_list(PIDGIN_STOCK_STATUS_OFFLINE, w);
	away_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AWAY, w);
	protocol_lists = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
plugin_changed_cb(PurplePlugin *p, gpointer data)
{
	regenerate_plugins_items(data);
}

static gboolean gtk_conv_configure_cb(GtkWidget *w, GdkEventConfigure *event, gpointer data) {
	int x, y;

	if (gtk_widget_get_visible(w))
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);
	else
		return FALSE; /* carry on normally */

	/* Workaround for GTK+ bug # 169811 - "configure_event" is fired
	* when the window is being maximized */
	if (gdk_window_get_state(gtk_widget_get_window(w)) & GDK_WINDOW_STATE_MAXIMIZED)
		return FALSE;

	/* don't save off-screen positioning */
	if (x + event->width < 0 ||
	    y + event->height < 0 ||
	    x > gdk_screen_width() ||
	    y > gdk_screen_height())
		return FALSE; /* carry on normally */

	/* store the position */
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/x", x);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/y", y);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/width",  event->width);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/height", event->height);

	/* continue to handle event normally */
	return FALSE;

}

static void
pidgin_conv_set_position_size(PidginWindow *win, int conv_x, int conv_y,
		int conv_width, int conv_height)
{
	/* if the window exists, is hidden, we're saving positions, and the
	 * position is sane... */
	if (win && win->window &&
			!gtk_widget_get_visible(win->window) && conv_width != 0) {

#ifdef _WIN32  /* only override window manager placement on Windows */
		/* ...check position is on screen... */
		if (conv_x >= gdk_screen_width())
			conv_x = gdk_screen_width() - 100;
		else if (conv_x + conv_width < 0)
			conv_x = 100;

		if (conv_y >= gdk_screen_height())
			conv_y = gdk_screen_height() - 100;
		else if (conv_y + conv_height < 0)
			conv_y = 100;

		/* ...and move it back. */
		gtk_window_move(GTK_WINDOW(win->window), conv_x, conv_y);
#endif
		gtk_window_resize(GTK_WINDOW(win->window), conv_width, conv_height);
	}
}

static void
pidgin_conv_restore_position(PidginWindow *win) {
	pidgin_conv_set_position_size(win,
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/x"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/y"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/width"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/height"));
}

PidginWindow *
pidgin_conv_window_new()
{
	PidginWindow *win;
	GtkPositionType pos;
	GtkWidget *testidea;
	GtkWidget *menubar;
	GtkWidget *menu;
	GtkWidget *item;
	GdkModifierType state;

	win = g_malloc0(sizeof(PidginWindow));

	window_list = g_list_append(window_list, win);

	/* Create the window. */
	win->window = pidgin_create_window(NULL, 0, "conversation", TRUE);
	if (!gtk_get_current_event_state(&state))
		gtk_window_set_focus_on_map(GTK_WINDOW(win->window), FALSE);

	/* Etan: I really think this entire function call should happen only
	 * when we are on Windows but I was informed that back before we used
	 * to save the window position we stored the window size, so I'm
	 * leaving it for now. */
#if TRUE || defined(_WIN32)
	pidgin_conv_restore_position(win);
#endif

	if (available_list == NULL) {
		create_icon_lists(win->window);
	}

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(close_win_cb), win);
	g_signal_connect(G_OBJECT(win->window), "focus_in_event",
	                 G_CALLBACK(focus_win_cb), win);

	/* Intercept keystrokes from the menu items */
	g_signal_connect(G_OBJECT(win->window), "key_press_event",
					 G_CALLBACK(window_keypress_cb), win);


	/* Create the notebook. */
	win->notebook = gtk_notebook_new();

	pos = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side");

#if 0
	gtk_notebook_set_tab_hborder(GTK_NOTEBOOK(win->notebook), 0);
	gtk_notebook_set_tab_vborder(GTK_NOTEBOOK(win->notebook), 0);
#endif
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(win->notebook), pos);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(win->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook), TRUE);

	menu = win->notebook_menu = gtk_menu_new();

	pidgin_separator(GTK_WIDGET(menu));

	item = gtk_menu_item_new_with_label(_("Close other tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_others_cb), win);

	item = gtk_menu_item_new_with_label(_("Close all tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_window), win);

	pidgin_separator(menu);

	item = gtk_menu_item_new_with_label(_("Detach this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(detach_tab_cb), win);

	item = gtk_menu_item_new_with_label(_("Close this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_tab_cb), win);

	g_signal_connect(G_OBJECT(win->notebook), "page-added",
	                 G_CALLBACK(notebook_add_tab_to_menu_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "page-removed",
	                 G_CALLBACK(notebook_remove_tab_from_menu_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "page-reordered",
	                 G_CALLBACK(notebook_reorder_tab_in_menu_cb), win);

	g_signal_connect(G_OBJECT(win->notebook), "button-press-event",
					G_CALLBACK(notebook_right_click_menu_cb), win);

	gtk_widget_show(win->notebook);

	g_signal_connect(G_OBJECT(win->notebook), "switch_page",
	                 G_CALLBACK(before_switch_conv_cb), win);
	g_signal_connect_after(G_OBJECT(win->notebook), "switch_page",
	                       G_CALLBACK(switch_conv_cb), win);

	/* Setup the tab drag and drop signals. */
	gtk_widget_add_events(win->notebook,
	                      GDK_BUTTON1_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(win->notebook), "button_press_event",
	                 G_CALLBACK(notebook_press_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "button_release_event",
	                 G_CALLBACK(notebook_release_cb), win);

	testidea = gtk_vbox_new(FALSE, 0);

	/* Setup the menubar. */
	menubar = setup_menubar(win);
	gtk_box_pack_start(GTK_BOX(testidea), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(testidea), win->notebook, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(win->window), testidea);

	gtk_widget_show(testidea);

	/* Update the plugin actions when plugins are (un)loaded */
	purple_signal_connect(purple_plugins_get_handle(), "plugin-load",
			win, PURPLE_CALLBACK(plugin_changed_cb), win);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload",
			win, PURPLE_CALLBACK(plugin_changed_cb), win);


#ifdef _WIN32
	g_signal_connect(G_OBJECT(win->window), "show",
	                 G_CALLBACK(winpidgin_ensure_onscreen), win->window);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/win32/minimize_new_convs")
			&& !gtk_get_current_event_state(&state))
		gtk_window_iconify(GTK_WINDOW(win->window));
#endif

	return win;
}

void
pidgin_conv_window_destroy(PidginWindow *win)
{
	if (win->gtkconvs) {
		GList *iter = win->gtkconvs;
		while (iter)
		{
			PidginConversation *gtkconv = iter->data;
			iter = iter->next;
			close_conv_cb(NULL, gtkconv);
		}
		return;
	}

	purple_prefs_disconnect_by_handle(win);
	window_list = g_list_remove(window_list, win);

	gtk_widget_destroy(win->notebook_menu);
	gtk_widget_destroy(win->window);

	g_object_unref(G_OBJECT(win->menu.ui));

	purple_notify_close_with_handle(win);
	purple_signals_disconnect_by_handle(win);

	g_free(win);
}

void
pidgin_conv_window_show(PidginWindow *win)
{
	gtk_widget_show(win->window);
}

void
pidgin_conv_window_hide(PidginWindow *win)
{
	gtk_widget_hide(win->window);
}

void
pidgin_conv_window_raise(PidginWindow *win)
{
	gdk_window_raise(GDK_WINDOW(gtk_widget_get_window(win->window)));
}

void
pidgin_conv_window_switch_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
	                              gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
		                              gtkconv->tab_cont));
}

static gboolean
gtkconv_tab_set_tip(GtkWidget *widget, GdkEventCrossing *event, PidginConversation *gtkconv)
{
/* PANGO_VERSION_CHECK macro was introduced in 1.15. So we need this double check. */
#ifndef PANGO_VERSION_CHECK
#define pango_layout_is_ellipsized(l) TRUE
#elif !PANGO_VERSION_CHECK(1,16,0)
#define pango_layout_is_ellipsized(l) TRUE
#endif
	PangoLayout *layout;

	layout = gtk_label_get_layout(GTK_LABEL(gtkconv->tab_label));
	if (pango_layout_is_ellipsized(layout))
		gtk_widget_set_tooltip_text(widget, gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
	else
		gtk_widget_set_tooltip_text(widget, NULL);

	return FALSE;
}

#if GTK_CHECK_VERSION(3,0,0)
static void
set_default_tab_colors(GtkWidget *widget)
{
	GString *str;
	GtkCssProvider *provider;
	GError *error = NULL;
	int iter;

	struct {
		const char *labelname;
		const char *color;
	} styles[] = {
		{"tab-label-typing", "#4e9a06"},
		{"tab-label-typed", "#c4a000"},
		{"tab-label-attention", "#006aff"},
		{"tab-label-unreadchat", "#cc0000"},
		{"tab-label-event", "#888a85"},
		{NULL, NULL}
	};

	str = g_string_new(NULL);

	for (iter = 0; styles[iter].labelname; iter++) {
		g_string_append_printf(str,
		                       "#%s {\n"
		                       "	color: %s;\n"
		                       "}\n",
		                       styles[iter].labelname,
		                       styles[iter].color);
	}

	provider = gtk_css_provider_new();

	gtk_css_provider_load_from_data(provider, str->str, str->len, &error);

	gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
	                               GTK_STYLE_PROVIDER(provider),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	if (error)
		g_error_free(error);
	g_string_free(str, TRUE);
}
#endif

void
pidgin_conv_window_add_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginConversation *focus_gtkconv;
	GtkWidget *tab_cont = gtkconv->tab_cont;
	const gchar *tmp_lab;

	win->gtkconvs = g_list_append(win->gtkconvs, gtkconv);
	gtkconv->win = win;

	if (win->gtkconvs && win->gtkconvs->next && win->gtkconvs->next->next == NULL)
		pidgin_conv_tab_pack(win, ((PidginConversation*)win->gtkconvs->data));


	/* Close button. */
	gtkconv->close = pidgin_create_small_button(gtk_label_new("×"));
	gtk_widget_set_tooltip_text(gtkconv->close, _("Close conversation"));

	g_signal_connect(gtkconv->close, "clicked", G_CALLBACK (close_conv_cb), gtkconv);

	/* Status icon. */
	gtkconv->icon = gtk_image_new();
	gtkconv->menu_icon = gtk_image_new();
	g_object_set(G_OBJECT(gtkconv->icon),
			"icon-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC),
			NULL);
	g_object_set(G_OBJECT(gtkconv->menu_icon),
			"icon-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC),
			NULL);
	gtk_widget_show(gtkconv->icon);
	update_tab_icon(conv);

	/* Tab label. */
	gtkconv->tab_label = gtk_label_new(tmp_lab = purple_conversation_get_title(conv));
#if GTK_CHECK_VERSION(3,0,0)
	set_default_tab_colors(gtkconv->tab_label);
#endif
	gtk_widget_set_name(gtkconv->tab_label, "tab-label");

	gtkconv->menu_tabby = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtkconv->menu_label = gtk_label_new(tmp_lab);
	gtk_box_pack_start(GTK_BOX(gtkconv->menu_tabby), gtkconv->menu_icon, FALSE, FALSE, 0);

	gtk_widget_show_all(gtkconv->menu_icon);

	gtk_box_pack_start(GTK_BOX(gtkconv->menu_tabby), gtkconv->menu_label, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->menu_label);
	gtk_misc_set_alignment(GTK_MISC(gtkconv->menu_label), 0, 0);

	gtk_widget_show(gtkconv->menu_tabby);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));

	/* Build and set conversations tab */
	pidgin_conv_tab_pack(win, gtkconv);

	gtk_notebook_set_menu_label(GTK_NOTEBOOK(win->notebook), tab_cont, gtkconv->menu_tabby);

	gtk_widget_show(tab_cont);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1) {
		/* Er, bug in notebooks? Switch to the page manually. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
	} else {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), TRUE);
	}

	focus_gtkconv = g_list_nth_data(pidgin_conv_window_get_gtkconvs(win),
	                             gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook)));
	gtk_widget_grab_focus(focus_gtkconv->entry);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		update_send_to_selection(win);
}

static void
pidgin_conv_tab_pack(PidginWindow *win, PidginConversation *gtkconv)
{
	gboolean tabs_side = FALSE;
	gint angle = 0;
	GtkWidget *first, *third, *ebox;

	if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_LEFT ||
	    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_RIGHT)
		tabs_side = TRUE;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_LEFT|8))
		angle = 90;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_RIGHT|8))
		angle = 270;

	if (!angle) {
		g_object_set(G_OBJECT(gtkconv->tab_label), "ellipsize", PANGO_ELLIPSIZE_END,  NULL);
		gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), 4);
	} else {
		g_object_set(G_OBJECT(gtkconv->tab_label), "ellipsize", PANGO_ELLIPSIZE_NONE, NULL);
		gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), -1);
	}

	if (tabs_side) {
		gtk_label_set_width_chars(
			GTK_LABEL(gtkconv->tab_label),
			MIN(g_utf8_strlen(gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)), -1), 12)
		);
	}

	gtk_label_set_angle(GTK_LABEL(gtkconv->tab_label), angle);

#if 0
	gtk_misc_set_alignment(GTK_MISC(gtkconv->tab_label), 0.00, 0.5);
	gtk_misc_set_padding(GTK_MISC(gtkconv->tab_label), 4, 0);
#endif

	if (angle)
		gtkconv->tabby = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	else
		gtkconv->tabby = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_set_name(gtkconv->tabby, "tab-container");

	/* select the correct ordering for verticle tabs */
	if (angle == 90) {
		first = gtkconv->close;
		third = gtkconv->icon;
	} else {
		first = gtkconv->icon;
		third = gtkconv->close;
	}

	ebox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), FALSE);
	gtk_container_add(GTK_CONTAINER(ebox), gtkconv->tabby);
	g_signal_connect(G_OBJECT(ebox), "enter-notify-event",
			G_CALLBACK(gtkconv_tab_set_tip), gtkconv);

	if (gtk_widget_get_parent(gtkconv->tab_label) == NULL) {
		/* Pack if it's a new widget */
		gtk_box_pack_start(GTK_BOX(gtkconv->tabby), first,              FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(gtkconv->tabby), gtkconv->tab_label, TRUE,  TRUE,  0);
		gtk_box_pack_start(GTK_BOX(gtkconv->tabby), third,              FALSE, FALSE, 0);

		/* Add this pane to the conversation's notebook. */
		gtk_notebook_append_page(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, ebox);
	} else {
		/* reparent old widgets on preference changes */
		gtk_widget_reparent(first,              gtkconv->tabby);
		gtk_widget_reparent(gtkconv->tab_label, gtkconv->tabby);
		gtk_widget_reparent(third,              gtkconv->tabby);
		gtk_box_set_child_packing(GTK_BOX(gtkconv->tabby), first,              FALSE, FALSE, 0, GTK_PACK_START);
		gtk_box_set_child_packing(GTK_BOX(gtkconv->tabby), gtkconv->tab_label, TRUE,  TRUE,  0, GTK_PACK_START);
		gtk_box_set_child_packing(GTK_BOX(gtkconv->tabby), third,              FALSE, FALSE, 0, GTK_PACK_START);

		/* Reset the tabs label to the new version */
		gtk_notebook_set_tab_label(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, ebox);
	}

	gtk_container_child_set(GTK_CONTAINER(win->notebook), gtkconv->tab_cont,
	                        "tab-expand", !tabs_side && !angle,
	                        "tab-fill", TRUE, NULL);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
					   purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs") &&
                                           (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons") ||
                                           purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") != GTK_POS_TOP));

	/* show the widgets */
/*	gtk_widget_show(gtkconv->icon); */
	gtk_widget_show(gtkconv->tab_label);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs"))
		gtk_widget_show(gtkconv->close);
	gtk_widget_show(gtkconv->tabby);
	gtk_widget_show(ebox);
}

void
pidgin_conv_window_remove_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	unsigned int index;

	index = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont);

	g_object_ref_sink(G_OBJECT(gtkconv->tab_cont));

	gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), index);

	win->gtkconvs = g_list_remove(win->gtkconvs, gtkconv);

	g_signal_handlers_disconnect_matched(win->window, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, gtkconv);

	if (win->gtkconvs && win->gtkconvs->next == NULL)
		pidgin_conv_tab_pack(win, win->gtkconvs->data);

	if (!win->gtkconvs && win != hidden_convwin)
		pidgin_conv_window_destroy(win);
}

PidginConversation *
pidgin_conv_window_get_gtkconv_at_index(const PidginWindow *win, int index)
{
	GtkWidget *tab_cont;

	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	return tab_cont ? g_object_get_data(G_OBJECT(tab_cont), "PidginConversation") : NULL;
}

PidginConversation *
pidgin_conv_window_get_active_gtkconv(const PidginWindow *win)
{
	int index;
	GtkWidget *tab_cont;

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));
	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	if (!tab_cont)
		return NULL;
	return g_object_get_data(G_OBJECT(tab_cont), "PidginConversation");
}


PurpleConversation *
pidgin_conv_window_get_active_conversation(const PidginWindow *win)
{
	PidginConversation *gtkconv;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	return gtkconv ? gtkconv->active_conv : NULL;
}

gboolean
pidgin_conv_window_is_active_conversation(const PurpleConversation *conv)
{
	return conv == pidgin_conv_window_get_active_conversation(PIDGIN_CONVERSATION(conv)->win);
}

gboolean
pidgin_conv_window_has_focus(PidginWindow *win)
{
	gboolean has_focus = FALSE;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}

PidginWindow *
pidgin_conv_window_get_at_event(GdkEvent *event)
{
	PidginWindow *win;
	GdkWindow *gdkwin;
	GList *l;
	int x, y;

#if GTK_CHECK_VERSION(3,0,0)
	gdkwin = gdk_device_get_window_at_position(gdk_event_get_device(event),
	                                           &x, &y);
#else
	gdkwin = gdk_window_at_pointer(&x, &y);
#endif

	if (gdkwin)
		gdkwin = gdk_window_get_toplevel(gdkwin);

	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;

		if (gdkwin == gtk_widget_get_window(win->window))
			return win;
	}

	return NULL;
}

GList *
pidgin_conv_window_get_gtkconvs(PidginWindow *win)
{
	return win->gtkconvs;
}

guint
pidgin_conv_window_get_gtkconv_count(PidginWindow *win)
{
	return g_list_length(win->gtkconvs);
}

PidginWindow *
pidgin_conv_window_first_im(void)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginWindow *
pidgin_conv_window_last_im(void)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	for (wins = g_list_last(pidgin_conv_windows_get_list());
	     wins != NULL;
	     wins = wins->prev) {

		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginWindow *
pidgin_conv_window_first_chat(void)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginWindow *
pidgin_conv_window_last_chat(void)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	for (wins = g_list_last(pidgin_conv_windows_get_list());
	     wins != NULL;
	     wins = wins->prev) {

		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}


/**************************************************************************
 * Conversation placement functions
 **************************************************************************/
typedef struct
{
	char *id;
	char *name;
	PidginConvPlacementFunc fnc;

} ConvPlacementData;

static GList *conv_placement_fncs = NULL;
static PidginConvPlacementFunc place_conv = NULL;

/* This one places conversations in the last made window. */
static void
conv_placement_last_created_win(PidginConversation *conv)
{
	PidginWindow *win;

	GList *l = g_list_last(pidgin_conv_windows_get_list());
	win = l ? l->data : NULL;;

	if (win == NULL) {
		win = pidgin_conv_window_new();

		g_signal_connect(G_OBJECT(win->window), "configure_event",
				G_CALLBACK(gtk_conv_configure_cb), NULL);

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);
	} else {
		pidgin_conv_window_add_gtkconv(win, conv);
	}
}

/* This one places conversations in the last made window of the same type. */
static gboolean
conv_placement_last_created_win_type_configured_cb(GtkWidget *w,
		GdkEventConfigure *event, PidginConversation *conv)
{
	int x, y;
	GList *all;

	if (gtk_widget_get_visible(w))
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);
	else
		return FALSE; /* carry on normally */

	/* Workaround for GTK+ bug # 169811 - "configure_event" is fired
	* when the window is being maximized */
	if (gdk_window_get_state(gtk_widget_get_window(w)) & GDK_WINDOW_STATE_MAXIMIZED)
		return FALSE;

	/* don't save off-screen positioning */
	if (x + event->width < 0 ||
	    y + event->height < 0 ||
	    x > gdk_screen_width() ||
	    y > gdk_screen_height())
		return FALSE; /* carry on normally */

	for (all = conv->convs; all != NULL; all = all->next) {
		if (PURPLE_IS_IM_CONVERSATION(conv->active_conv) != PURPLE_IS_IM_CONVERSATION(all->data)) {
			/* this window has different types of conversation, don't save */
			return FALSE;
		}
	}

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/x", x);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/y", y);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/height", event->height);
	} else {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/x", x);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/y", y);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/height", event->height);
	}

	return FALSE;
}

static void
conv_placement_last_created_win_type(PidginConversation *conv)
{
	PidginWindow *win;

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
		win = pidgin_conv_window_last_im();
	else
		win = pidgin_conv_window_last_chat();

	if (win == NULL) {
		win = pidgin_conv_window_new();

		if (PURPLE_IS_IM_CONVERSATION(conv->active_conv) ||
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/width") == 0) {
			pidgin_conv_set_position_size(win,
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/x"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/y"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/width"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/height"));
		} else if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv)) {
			pidgin_conv_set_position_size(win,
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/x"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/y"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/width"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/height"));
		}

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);

		g_signal_connect(G_OBJECT(win->window), "configure_event",
				G_CALLBACK(conv_placement_last_created_win_type_configured_cb), conv);
	} else
		pidgin_conv_window_add_gtkconv(win, conv);
}

/* This one places each conversation in its own window. */
static void
conv_placement_new_window(PidginConversation *conv)
{
	PidginWindow *win;

	win = pidgin_conv_window_new();

	g_signal_connect(G_OBJECT(win->window), "configure_event",
			G_CALLBACK(gtk_conv_configure_cb), NULL);

	pidgin_conv_window_add_gtkconv(win, conv);

	pidgin_conv_window_show(win);
}

static PurpleGroup *
conv_get_group(PidginConversation *conv)
{
	PurpleGroup *group = NULL;

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv)) {
		PurpleBuddy *buddy;

		buddy = purple_blist_find_buddy(purple_conversation_get_account(conv->active_conv),
		                        purple_conversation_get_name(conv->active_conv));

		if (buddy != NULL)
			group = purple_buddy_get_group(buddy);

	} else {
		PurpleChat *chat;

		chat = purple_blist_find_chat(purple_conversation_get_account(conv->active_conv),
		                            purple_conversation_get_name(conv->active_conv));

		if (chat != NULL)
			group = purple_chat_get_group(chat);
	}

	return group;
}

/*
 * This groups things by, well, group. Buddies from groups will always be
 * grouped together, and a buddy from a group not belonging to any currently
 * open windows will get a new window.
 */
static void
conv_placement_by_group(PidginConversation *conv)
{
	PurpleGroup *group = NULL;
	GList *wl, *cl;

	group = conv_get_group(conv);

	/* Go through the list of IMs and find one with this group. */
	for (wl = pidgin_conv_windows_get_list(); wl != NULL; wl = wl->next) {
		PidginWindow *win2;
		PidginConversation *conv2;
		PurpleGroup *group2 = NULL;

		win2 = wl->data;

		for (cl = win2->gtkconvs;
		     cl != NULL;
		     cl = cl->next) {
			conv2 = cl->data;

			group2 = conv_get_group(conv2);

			if (group == group2) {
				pidgin_conv_window_add_gtkconv(win2, conv);

				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

/* This groups things by account.  Otherwise, the same semantics as above */
static void
conv_placement_by_account(PidginConversation *conv)
{
	GList *wins, *convs;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv->active_conv);

	/* Go through the list of IMs and find one with this group. */
	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		PidginWindow *win2;
		PidginConversation *conv2;

		win2 = wins->data;

		for (convs = win2->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {
			conv2 = convs->data;

			if (account == purple_conversation_get_account(conv2->active_conv)) {
				pidgin_conv_window_add_gtkconv(win2, conv);
				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

static ConvPlacementData *
get_conv_placement_data(const char *id)
{
	ConvPlacementData *data = NULL;
	GList *n;

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		if (!strcmp(data->id, id))
			return data;
	}

	return NULL;
}

static void
add_conv_placement_fnc(const char *id, const char *name,
                       PidginConvPlacementFunc fnc)
{
	ConvPlacementData *data;

	data = g_new(ConvPlacementData, 1);

	data->id = g_strdup(id);
	data->name = g_strdup(name);
	data->fnc  = fnc;

	conv_placement_fncs = g_list_append(conv_placement_fncs, data);
}

static void
ensure_default_funcs(void)
{
	if (conv_placement_fncs == NULL) {
		add_conv_placement_fnc("last", _("Last created window"),
		                       conv_placement_last_created_win);
		add_conv_placement_fnc("im_chat", _("Separate IM and Chat windows"),
		                       conv_placement_last_created_win_type);
		add_conv_placement_fnc("new", _("New window"),
		                       conv_placement_new_window);
		add_conv_placement_fnc("group", _("By group"),
		                       conv_placement_by_group);
		add_conv_placement_fnc("account", _("By account"),
		                       conv_placement_by_account);
	}
}

GList *
pidgin_conv_placement_get_options(void)
{
	GList *n, *list = NULL;
	ConvPlacementData *data;

	ensure_default_funcs();

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		list = g_list_append(list, data->name);
		list = g_list_append(list, data->id);
	}

	return list;
}


void
pidgin_conv_placement_add_fnc(const char *id, const char *name,
                            PidginConvPlacementFunc fnc)
{
	g_return_if_fail(id   != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fnc  != NULL);

	ensure_default_funcs();

	add_conv_placement_fnc(id, name, fnc);
}

void
pidgin_conv_placement_remove_fnc(const char *id)
{
	ConvPlacementData *data = get_conv_placement_data(id);

	if (data == NULL)
		return;

	conv_placement_fncs = g_list_remove(conv_placement_fncs, data);

	g_free(data->id);
	g_free(data->name);
	g_free(data);
}

const char *
pidgin_conv_placement_get_name(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->name;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_fnc(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->fnc;
}

void
pidgin_conv_placement_set_current_func(PidginConvPlacementFunc func)
{
	g_return_if_fail(func != NULL);

	/* If tabs are enabled, set the function, otherwise, NULL it out. */
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"))
		place_conv = func;
	else
		place_conv = NULL;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_current_func(void)
{
	return place_conv;
}

void
pidgin_conv_placement_place(PidginConversation *gtkconv)
{
	if (place_conv)
		place_conv(gtkconv);
	else
		conv_placement_new_window(gtkconv);
}

gboolean
pidgin_conv_is_hidden(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, FALSE);

	return (gtkconv->win == hidden_convwin);
}


/* Algorithm from http://www.w3.org/TR/AERT#color-contrast */
static gboolean
color_is_visible(GdkColor foreground, GdkColor background, int color_contrast, int brightness_contrast)
{
	gulong fg_brightness;
	gulong bg_brightness;
	gulong br_diff;
	gulong col_diff;
	int fred, fgreen, fblue, bred, bgreen, bblue;

	/* this algorithm expects colors between 0 and 255 for each of red green and blue.
	 * GTK on the other hand has values between 0 and 65535
	 * Err suggested I >> 8, which grabbed the high bits.
	 */

	fred = foreground.red >> 8 ;
	fgreen = foreground.green >> 8 ;
	fblue = foreground.blue >> 8 ;


	bred = background.red >> 8 ;
	bgreen = background.green >> 8 ;
	bblue = background.blue >> 8 ;

	fg_brightness = (fred * 299 + fgreen * 587 + fblue * 114) / 1000;
	bg_brightness = (bred * 299 + bgreen * 587 + bblue * 114) / 1000;
	br_diff = abs(fg_brightness - bg_brightness);

	col_diff = abs(fred - bred) + abs(fgreen - bgreen) + abs(fblue - bblue);

	return ((col_diff > color_contrast) && (br_diff > brightness_contrast));
}


static GArray*
generate_nick_colors(guint numcolors, GdkColor background)
{
	guint i = 0, j = 0;
	GArray *colors = g_array_new(FALSE, FALSE, sizeof(GdkColor));
	GdkColor nick_highlight;
	GdkColor send_color;
	time_t breakout_time;

	gdk_color_parse(DEFAULT_HIGHLIGHT_COLOR, &nick_highlight);
	gdk_color_parse(DEFAULT_SEND_COLOR, &send_color);

	srand(background.red + background.green + background.blue + 1);

	breakout_time = time(NULL) + 3;

	/* first we look through the list of "good" colors: colors that differ from every other color in the
	 * list.  only some of them will differ from the background color though. lets see if we can find
	 * numcolors of them that do
	 */
	while (i < numcolors && j < NUM_NICK_SEED_COLORS && time(NULL) < breakout_time)
	{
		GdkColor color = nick_seed_colors[j];

		if (color_is_visible(color, background,     MIN_COLOR_CONTRAST,     MIN_BRIGHTNESS_CONTRAST) &&
			color_is_visible(color, nick_highlight, MIN_COLOR_CONTRAST / 2, 0) &&
			color_is_visible(color, send_color,     MIN_COLOR_CONTRAST / 4, 0))
		{
			g_array_append_val(colors, color);
			i++;
		}
		j++;
	}

	/* we might not have found numcolors in the last loop.  if we did, we'll never enter this one.
	 * if we did not, lets just find some colors that don't conflict with the background.  its
	 * expensive to find colors that not only don't conflict with the background, but also do not
	 * conflict with each other.
	 */
	while(i < numcolors && time(NULL) < breakout_time)
	{
		GdkColor color = { 0, rand() % 65536, rand() % 65536, rand() % 65536 };

		if (color_is_visible(color, background,     MIN_COLOR_CONTRAST,     MIN_BRIGHTNESS_CONTRAST) &&
			color_is_visible(color, nick_highlight, MIN_COLOR_CONTRAST / 2, 0) &&
			color_is_visible(color, send_color,     MIN_COLOR_CONTRAST / 4, 0))
		{
			g_array_append_val(colors, color);
			i++;
		}
	}

	if (i < numcolors) {
		purple_debug_warning("gtkconv", "Unable to generate enough random colors before timeout. %u colors found.\n", i);
	}

	return colors;
}
