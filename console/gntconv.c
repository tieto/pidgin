/**
 * @file gntconv.c GNT Conversation API
 * @ingroup gntui
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
#include <string.h>

#include <cmds.h>
#include <prefs.h>
#include <util.h>

#include "gntgaim.h"
#include "gntaccount.h"
#include "gntblist.h"
#include "gntconv.h"
#include "gntdebug.h"
#include "gntplugin.h"
#include "gntprefs.h"
#include "gntstatus.h"

#include "gnt.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gnttextview.h"

#define PREF_ROOT	"/gaim/gnt/conversations"

#include "config.h"

typedef struct _GGConv GGConv;
typedef struct _GGConvChat GGConvChat;
typedef struct _GGConvIm GGConvIm;

struct _GGConv
{
	GList *list;
	GaimConversation *active_conv;
	/*GaimConversation *conv;*/

	GntWidget *window;        /* the container */
	GntWidget *entry;         /* entry */
	GntWidget *tv;            /* text-view */

	union
	{
		GGConvChat *chat;
		GGConvIm *im;
	} u;
};

struct _GGConvChat
{
	GntWidget *userlist;       /* the userlist */
};

struct _GGConvIm
{
	void *nothing_for_now;
};

static gboolean
entry_key_pressed(GntWidget *w, const char *key, GGConv *ggconv)
{
	if (key[0] == '\r' && key[1] == 0)
	{
		const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
		if (*text == '/')
		{
			GaimConversation *conv = ggconv->active_conv;
			GaimCmdStatus status;
			const char *cmdline = text + 1;
			char *error = NULL, *escape;

			escape = g_markup_escape_text(cmdline, -1);
			status = gaim_cmd_do_command(conv, cmdline, escape, &error);
			g_free(escape);

			switch (status)
			{
				case GAIM_CMD_STATUS_OK:
					break;
				case GAIM_CMD_STATUS_NOT_FOUND:
					gaim_conversation_write(conv, "", _("No such command."),
							GAIM_MESSAGE_NO_LOG, time(NULL));
					break;
				case GAIM_CMD_STATUS_WRONG_ARGS:
					gaim_conversation_write(conv, "", _("Syntax Error:  You typed the wrong number of arguments "
										"to that command."),
							GAIM_MESSAGE_NO_LOG, time(NULL));
					break;
				case GAIM_CMD_STATUS_FAILED:
					gaim_conversation_write(conv, "", error ? error : _("Your command failed for an unknown reason."),
							GAIM_MESSAGE_NO_LOG, time(NULL));
					break;
				case GAIM_CMD_STATUS_WRONG_TYPE:
					if(gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM)
						gaim_conversation_write(conv, "", _("That command only works in chats, not IMs."),
								GAIM_MESSAGE_NO_LOG, time(NULL));
					else
						gaim_conversation_write(conv, "", _("That command only works in IMs, not chats."),
								GAIM_MESSAGE_NO_LOG, time(NULL));
					break;
				case GAIM_CMD_STATUS_WRONG_PRPL:
					gaim_conversation_write(conv, "", _("That command doesn't work on this protocol."),
							GAIM_MESSAGE_NO_LOG, time(NULL));
					break;
			}
			g_free(error);
#if 0
			gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					_("Commands are not supported yet. Message was NOT sent."),
					GNT_TEXT_FLAG_DIM | GNT_TEXT_FLAG_UNDERLINE);
			gnt_text_view_next_line(GNT_TEXT_VIEW(ggconv->tv));
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 0);
#endif
		}
		else
		{
			char *escape = g_markup_escape_text(text, -1);
			switch (gaim_conversation_get_type(ggconv->active_conv))
			{
				case GAIM_CONV_TYPE_IM:
					gaim_conv_im_send_with_flags(GAIM_CONV_IM(ggconv->active_conv), escape, GAIM_MESSAGE_SEND);
					break;
				case GAIM_CONV_TYPE_CHAT:
					gaim_conv_chat_send(GAIM_CONV_CHAT(ggconv->active_conv), escape);
					break;
				default:
					g_free(escape);
					g_return_val_if_reached(FALSE);
			}
			g_free(escape);
		}
		gnt_entry_add_to_history(GNT_ENTRY(ggconv->entry), text);
		gnt_entry_clear(GNT_ENTRY(ggconv->entry));
		return TRUE;
	}
	else if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 1);
		else if (strcmp(key+1, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), -1);
		else if (strcmp(key+1, GNT_KEY_PGDOWN) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), ggconv->tv->priv.height - 2);
		else if (strcmp(key+1, GNT_KEY_PGUP) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), -(ggconv->tv->priv.height - 2));
		else
			return FALSE;
		return TRUE;
	}

	return FALSE;
}

static void
closing_window(GntWidget *window, GGConv *ggconv)
{
	GList *list = ggconv->list;
	ggconv->window = NULL;
	while (list) {
		GaimConversation *conv = list->data;
		list = list->next;
		gaim_conversation_destroy(conv);
	}
}

static void
size_changed_cb(GntWidget *w, int width, int height)
{
	gaim_prefs_set_int(PREF_ROOT "/size/width", width);
	gaim_prefs_set_int(PREF_ROOT "/size/height", height);
}

static void
save_position_cb(GntWidget *w, int x, int y)
{
	gaim_prefs_set_int(PREF_ROOT "/position/x", x);
	gaim_prefs_set_int(PREF_ROOT "/position/y", y);
}

static GaimConversation *
find_conv_with_contact(GaimConversation *conv)
{
	GaimBlistNode *node;
	GaimBuddy *buddy = gaim_find_buddy(conv->account, conv->name);
	GaimConversation *ret = NULL;

	if (!buddy)
		return NULL;

	for (node = ((GaimBlistNode*)buddy)->parent->child; node; node = node->next) {
		if (node == (GaimBlistNode*)buddy)
			continue;
		if ((ret = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM,
				((GaimBuddy*)node)->name, ((GaimBuddy*)node)->account)) != NULL)
			break;
	}
	return ret;
}

static char *
get_conversation_title(GaimConversation *conv, GaimAccount *account)
{
	return g_strdup_printf(_("%s (%s -- %s)"), gaim_conversation_get_title(conv),
		gaim_account_get_username(account), gaim_account_get_protocol_name(account));
}

static void
update_buddy_typing(GaimAccount *account, const char *who, gpointer null)
{
	GaimConversation *conv;
	GGConv *ggc;
	GaimConvIm *im = NULL;
	char *title, *old_title;

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, who, account);

	if (!conv)
		return;

	im = GAIM_CONV_IM(conv);

	if (gaim_conv_im_get_typing_state(im) == GAIM_TYPING) {
		old_title = get_conversation_title(conv, account);
		title = g_strdup_printf(_("%s [%s]"), old_title,
			gnt_ascii_only() ? "T" : "\342\243\277");
		g_free(old_title);
 	} else
		title = get_conversation_title(conv, account);
	ggc = conv->ui_data;
	gnt_screen_rename_widget(ggc->window, title);
	g_free(title);
}

static gpointer
gg_conv_get_handle()
{
	static int handle;
	return &handle;
}

static void
gg_create_conversation(GaimConversation *conv)
{
	GGConv *ggc = conv->ui_data;
	char *title;
	GaimConversationType type;
	GaimConversation *cc;
	GaimAccount *account;

	if (ggc)
		return;

	cc = find_conv_with_contact(conv);
	if (cc && cc->ui_data)
		ggc = cc->ui_data;
	else
		ggc = g_new0(GGConv, 1);

	ggc->list = g_list_prepend(ggc->list, conv);
	ggc->active_conv = conv;
	conv->ui_data = ggc;

	if (cc && cc->ui_data) {
		gg_conversation_set_active(conv);
		return;
	}

	account = gaim_conversation_get_account(conv);
	type = gaim_conversation_get_type(conv);
	title = get_conversation_title(conv, account);

	ggc->window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_title(GNT_BOX(ggc->window), title);
	gnt_box_set_toplevel(GNT_BOX(ggc->window), TRUE);
	gnt_box_set_pad(GNT_BOX(ggc->window), 0);
	gnt_widget_set_name(ggc->window, "conversation-window");

	ggc->tv = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->tv);
	gnt_widget_set_name(ggc->tv, "conversation-window-textview");
	gnt_widget_set_size(ggc->tv, gaim_prefs_get_int(PREF_ROOT "/size/width"),
			gaim_prefs_get_int(PREF_ROOT "/size/height"));

	ggc->entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->entry);
	gnt_widget_set_name(ggc->entry, "conversation-window-entry");
	gnt_entry_set_history_length(GNT_ENTRY(ggc->entry), -1);
	gnt_entry_set_word_suggest(GNT_ENTRY(ggc->entry), TRUE);
	gnt_entry_set_always_suggest(GNT_ENTRY(ggc->entry), FALSE);

	g_signal_connect_after(G_OBJECT(ggc->entry), "key_pressed", G_CALLBACK(entry_key_pressed), ggc);
	g_signal_connect(G_OBJECT(ggc->window), "destroy", G_CALLBACK(closing_window), ggc);

	gnt_widget_set_position(ggc->window, gaim_prefs_get_int(PREF_ROOT "/position/x"),
			gaim_prefs_get_int(PREF_ROOT "/position/y"));
	gnt_widget_show(ggc->window);

	g_signal_connect(G_OBJECT(ggc->tv), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggc->window), "position_set", G_CALLBACK(save_position_cb), NULL);

	gaim_signal_connect(gaim_conversations_get_handle(), "buddy-typing", gg_conv_get_handle(),
	                GAIM_CALLBACK(update_buddy_typing), NULL);
	gaim_signal_connect(gaim_conversations_get_handle(), "buddy-typing-stopped", gg_conv_get_handle(),
	                GAIM_CALLBACK(update_buddy_typing), NULL);

	g_free(title);
}

static void
gg_destroy_conversation(GaimConversation *conv)
{
	/* do stuff here */
	GGConv *ggc = conv->ui_data;
	ggc->list = g_list_remove(ggc->list, conv);
	if (ggc->list && conv == ggc->active_conv)
		ggc->active_conv = ggc->list->data;
	
	if (ggc->list == NULL) {
		gnt_widget_destroy(ggc->window);
		g_free(ggc);
	}
}

static void
gg_write_common(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	GGConv *ggconv = conv->ui_data;
	char *strip, *newline;
	GntTextFormatFlags fl = 0;
	int pos;

	g_return_if_fail(ggconv != NULL);

	if (ggconv->active_conv != conv) {
		if (flags & (GAIM_MESSAGE_SEND | GAIM_MESSAGE_RECV))
			gg_conversation_set_active(conv);
		else
			return;
	}

	pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggconv->tv));

	gnt_text_view_next_line(GNT_TEXT_VIEW(ggconv->tv));

	/* Unnecessary to print the timestamp for delayed message */
	if (!(flags & GAIM_MESSAGE_DELAYED) &&
			gaim_prefs_get_bool("/gaim/gnt/conversations/timestamps"))
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					gaim_utf8_strftime("(%H:%M:%S) ", localtime(&mtime)), GNT_TEXT_FLAG_DIM);

	if (flags & GAIM_MESSAGE_AUTO_RESP)
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					_("<AUTO-REPLY> "), GNT_TEXT_FLAG_BOLD);

	if (who && *who && (flags & (GAIM_MESSAGE_SEND | GAIM_MESSAGE_RECV)))
	{
		char * name = NULL;

		if (gaim_message_meify((char*)message, -1))
			name = g_strdup_printf("*** %s ", who);
		else
			name =  g_strdup_printf("%s: ", who);

		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				name, GNT_TEXT_FLAG_BOLD);
		g_free(name);
	}
	else
		fl = GNT_TEXT_FLAG_DIM;

	if (flags & GAIM_MESSAGE_ERROR)
		fl |= GNT_TEXT_FLAG_BOLD;
	if (flags & GAIM_MESSAGE_NICK)
		fl |= GNT_TEXT_FLAG_UNDERLINE;

	/* XXX: Remove this workaround when textview can parse messages. */
	newline = gaim_strdup_withhtml(message);
	strip = gaim_markup_strip_html(newline);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				strip, fl);
	if (pos <= 1)
		gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 0);

	g_free(newline);
	g_free(strip);

	if (flags & (GAIM_MESSAGE_RECV | GAIM_MESSAGE_NICK | GAIM_MESSAGE_ERROR))
		gnt_widget_set_urgent(ggconv->tv);
}

static void
gg_write_chat(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	gaim_conversation_write(conv, who, message, flags, mtime);
}

static void
gg_write_im(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	GaimAccount *account = gaim_conversation_get_account(conv);
	if (flags & GAIM_MESSAGE_SEND)
	{
		who = gaim_connection_get_display_name(gaim_account_get_connection(account));
		if (!who)
			who = gaim_account_get_alias(account);
		if (!who)
			who = gaim_account_get_username(account);
	}
	else if (flags & GAIM_MESSAGE_RECV)
	{
		GaimBuddy *buddy;
		who = gaim_conversation_get_name(conv);
		buddy = gaim_find_buddy(account, who);
		if (buddy)
			who = gaim_buddy_get_contact_alias(buddy);
	}

	gaim_conversation_write(conv, who, message, flags, mtime);
}

static void
gg_write_conv(GaimConversation *conv, const char *who, const char *alias,
		const char *message, GaimMessageFlags flags, time_t mtime)
{
	const char *name;
	if (alias && *alias)
		name = alias;
	else if (who && *who)
		name = who;
	else
		name = NULL;

	gg_write_common(conv, name, message, flags, mtime);
}

static void
gg_chat_add_users(GaimConversation *conv, GList *users, gboolean new_arrivals)
{
	GGConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);

	if (!new_arrivals)
	{
		/* Print the list of users in the room */
		GString *string = g_string_new(_("List of users:\n"));
		GList *iter;

		for (iter = users; iter; iter = iter->next)
		{
			GaimConvChatBuddy *cbuddy = iter->data;
			char *str;

			if ((str = cbuddy->alias) == NULL)
				str = cbuddy->name;
			g_string_append_printf(string, "[ %s ]", str);
		}

		gaim_conversation_write(conv, NULL, string->str,
				GAIM_MESSAGE_SYSTEM, time(NULL));
		g_string_free(string, TRUE);
	}

	for (; users; users = users->next)
	{
		GaimConvChatBuddy *cbuddy = users->data;
		gnt_entry_add_suggest(entry, cbuddy->name);
		gnt_entry_add_suggest(entry, cbuddy->alias);
	}
}

static void
gg_chat_rename_user(GaimConversation *conv, const char *old, const char *new_n, const char *new_a)
{
	/* Update the name for string completion */
	GGConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	gnt_entry_remove_suggest(entry, old);
	gnt_entry_add_suggest(entry, new_n);
	gnt_entry_add_suggest(entry, new_a);
}

static void
gg_chat_remove_user(GaimConversation *conv, GList *list)
{
	/* Remove the name from string completion */
	GGConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	for (; list; list = list->next)
		gnt_entry_remove_suggest(entry, list->data);
}

static void
gg_chat_update_user(GaimConversation *conv, const char *user)
{
}

static GaimConversationUiOps conv_ui_ops = 
{
	.create_conversation = gg_create_conversation,
	.destroy_conversation = gg_destroy_conversation,
	.write_chat = gg_write_chat,
	.write_im = gg_write_im,
	.write_conv = gg_write_conv,
	.chat_add_users = gg_chat_add_users,
	.chat_rename_user = gg_chat_rename_user,
	.chat_remove_users = gg_chat_remove_user,
	.chat_update_user = gg_chat_update_user,
	.present = NULL,
	.has_focus = NULL,
	.custom_smiley_add = NULL,
	.custom_smiley_write = NULL,
	.custom_smiley_close = NULL
};

GaimConversationUiOps *gg_conv_get_ui_ops()
{
	return &conv_ui_ops;
}

/* Xerox */
static GaimCmdRet
say_command_cb(GaimConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM)
		gaim_conv_im_send(GAIM_CONV_IM(conv), args[0]);
	else if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT)
		gaim_conv_chat_send(GAIM_CONV_CHAT(conv), args[0]);

	return GAIM_CMD_RET_OK;
}

/* Xerox */
static GaimCmdRet
me_command_cb(GaimConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	char *tmp;

	tmp = g_strdup_printf("/me %s", args[0]);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM)
		gaim_conv_im_send(GAIM_CONV_IM(conv), tmp);
	else if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT)
		gaim_conv_chat_send(GAIM_CONV_CHAT(conv), tmp);

	g_free(tmp);
	return GAIM_CMD_RET_OK;
}

/* Xerox */
static GaimCmdRet
debug_command_cb(GaimConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;
	GaimCmdStatus status;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("me is using %s.", VERSION);
		markup = g_markup_escape_text(tmp, -1);

		status = gaim_cmd_do_command(conv, tmp, markup, error);

		g_free(tmp);
		g_free(markup);
		return status;
	} else {
		gaim_conversation_write(conv, NULL, _("Supported debug options are:  version"),
		                        GAIM_MESSAGE_NO_LOG|GAIM_MESSAGE_ERROR, time(NULL));
		return GAIM_CMD_STATUS_OK;
	}
}

/* Xerox */
static GaimCmdRet
clear_command_cb(GaimConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GGConv *ggconv = conv->ui_data;
	gnt_text_view_clear(GNT_TEXT_VIEW(ggconv->tv));
	return GAIM_CMD_STATUS_OK;
}

/* Xerox */
static GaimCmdRet
help_command_cb(GaimConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l, *text;
	GString *s;

	if (args[0] != NULL) {
		s = g_string_new("");
		text = gaim_cmd_help(conv, args[0]);

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
		s = g_string_new(_("Use \"/help &lt;command&gt;\" for help on a specific command.\n"
											 "The following commands are available in this context:\n"));

		text = gaim_cmd_list(conv);
		for (l = text; l; l = l->next)
			if (l->next)
				g_string_append_printf(s, "%s, ", (char *)l->data);
			else
				g_string_append_printf(s, "%s.", (char *)l->data);
		g_list_free(text);
	}

	gaim_conversation_write(conv, NULL, s->str, GAIM_MESSAGE_NO_LOG, time(NULL));
	g_string_free(s, TRUE);

	return GAIM_CMD_STATUS_OK;
}

static GaimCmdRet
cmd_show_window(GaimConversation *conv, const char *cmd, char **args, char **error, gpointer data)
{
	void (*callback)() = data;
	callback();
	return GAIM_CMD_STATUS_OK;
}

void gg_conversation_init()
{
	gaim_prefs_add_none(PREF_ROOT);
	gaim_prefs_add_none(PREF_ROOT "/size");
	gaim_prefs_add_int(PREF_ROOT "/size/width", 70);
	gaim_prefs_add_int(PREF_ROOT "/size/height", 20);
	gaim_prefs_add_none(PREF_ROOT "/position");
	gaim_prefs_add_int(PREF_ROOT "/position/x", 0);
	gaim_prefs_add_int(PREF_ROOT "/position/y", 0);

	/* Xerox the commands */
	gaim_cmd_register("say", "S", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  say_command_cb, _("say &lt;message&gt;:  Send a message normally as if you weren't using a command."), NULL);
	gaim_cmd_register("me", "S", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  me_command_cb, _("me &lt;action&gt;:  Send an IRC style action to a buddy or chat."), NULL);
	gaim_cmd_register("debug", "w", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  debug_command_cb, _("debug &lt;option&gt;:  Send various debug information to the current conversation."), NULL);
	gaim_cmd_register("clear", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  clear_command_cb, _("clear: Clears the conversation scrollback."), NULL);
	gaim_cmd_register("help", "w", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  help_command_cb, _("help &lt;command&gt;:  Help on a specific command."), NULL);

	/* Now some commands to bring up some other windows */
	gaim_cmd_register("plugins", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("plugins: Show the plugins window."), gg_plugins_show_all);
	gaim_cmd_register("buddylist", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("buddylist: Show the buddylist."), gg_blist_show);
	gaim_cmd_register("accounts", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("accounts: Show the accounts window."), gg_accounts_show_all);
	gaim_cmd_register("debugwin", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("debugwin: Show the debug window."), gg_debug_window_show);
	gaim_cmd_register("prefs", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("prefs: Show the preference window."), gg_prefs_show_all);
	gaim_cmd_register("status", "", GAIM_CMD_P_DEFAULT,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("statuses: Show the savedstatuses window."), gg_savedstatus_show_all);
}

void gg_conversation_uninit()
{
}

void gg_conversation_set_active(GaimConversation *conv)
{
	GGConv *ggconv = conv->ui_data;
	GaimAccount *account;
	char *title;

	g_return_if_fail(ggconv);
	g_return_if_fail(g_list_find(ggconv->list, conv));

	ggconv->active_conv = conv;
	account = gaim_conversation_get_account(conv);
	title = get_conversation_title(conv, account);
	gnt_screen_rename_widget(ggconv->window, title);
	g_free(title);
}

