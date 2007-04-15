/**
 * @file gntconv.c GNT Conversation API
 * @ingroup gntui
 *
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <idle.h>
#include <prefs.h>
#include <util.h>

#include "finch.h"
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

#define PREF_ROOT	"/purple/gnt/conversations"

#include "config.h"

static void
send_typing_notification(GntWidget *w, FinchConv *ggconv)
{
	const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
	gboolean empty = (!text || !*text);
	if (purple_prefs_get_bool("/purple/gnt/conversations/notify_typing")) {
		PurpleConversation *conv = ggconv->active_conv;
		PurpleConvIm *im = PURPLE_CONV_IM(conv);
		if (!empty) {
			gboolean send = (purple_conv_im_get_send_typed_timeout(im) == 0);

			purple_conv_im_stop_send_typed_timeout(im);
			purple_conv_im_start_send_typed_timeout(im);
			if (send || (purple_conv_im_get_type_again(im) != 0 &&
						  time(NULL) > purple_conv_im_get_type_again(im))) {
				unsigned int timeout;
				timeout = serv_send_typing(purple_conversation_get_gc(conv),
										   purple_conversation_get_name(conv),
										   PURPLE_TYPING);
				purple_conv_im_set_type_again(im, timeout);
			}
		} else {
			purple_conv_im_stop_send_typed_timeout(im);

			serv_send_typing(purple_conversation_get_gc(conv),
							 purple_conversation_get_name(conv),
							 PURPLE_NOT_TYPING);
		}
	}
}

static gboolean
entry_key_pressed(GntWidget *w, const char *key, FinchConv *ggconv)
{
	if (key[0] == '\r' && key[1] == 0)
	{
		const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
		if (*text == '/')
		{
			PurpleConversation *conv = ggconv->active_conv;
			PurpleCmdStatus status;
			const char *cmdline = text + 1;
			char *error = NULL, *escape;

			escape = g_markup_escape_text(cmdline, -1);
			status = purple_cmd_do_command(conv, cmdline, escape, &error);
			g_free(escape);

			switch (status)
			{
				case PURPLE_CMD_STATUS_OK:
					break;
				case PURPLE_CMD_STATUS_NOT_FOUND:
					purple_conversation_write(conv, "", _("No such command."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
					break;
				case PURPLE_CMD_STATUS_WRONG_ARGS:
					purple_conversation_write(conv, "", _("Syntax Error:  You typed the wrong number of arguments "
										"to that command."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
					break;
				case PURPLE_CMD_STATUS_FAILED:
					purple_conversation_write(conv, "", error ? error : _("Your command failed for an unknown reason."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
					break;
				case PURPLE_CMD_STATUS_WRONG_TYPE:
					if(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
						purple_conversation_write(conv, "", _("That command only works in chats, not IMs."),
								PURPLE_MESSAGE_NO_LOG, time(NULL));
					else
						purple_conversation_write(conv, "", _("That command only works in IMs, not chats."),
								PURPLE_MESSAGE_NO_LOG, time(NULL));
					break;
				case PURPLE_CMD_STATUS_WRONG_PRPL:
					purple_conversation_write(conv, "", _("That command doesn't work on this protocol."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
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
			char *apos = purple_strreplace(escape, "&apos;", "'");
			g_free(escape);
			escape = apos;
			switch (purple_conversation_get_type(ggconv->active_conv))
			{
				case PURPLE_CONV_TYPE_IM:
					purple_conv_im_send_with_flags(PURPLE_CONV_IM(ggconv->active_conv), escape, PURPLE_MESSAGE_SEND);
					break;
				case PURPLE_CONV_TYPE_CHAT:
					purple_conv_chat_send(PURPLE_CONV_CHAT(ggconv->active_conv), escape);
					break;
				default:
					g_free(escape);
					g_return_val_if_reached(FALSE);
			}
			g_free(escape);
			purple_idle_touch();
		}
		gnt_entry_add_to_history(GNT_ENTRY(ggconv->entry), text);
		gnt_entry_clear(GNT_ENTRY(ggconv->entry));
		return TRUE;
	}

	return FALSE;
}

static void
closing_window(GntWidget *window, FinchConv *ggconv)
{
	GList *list = ggconv->list;
	ggconv->window = NULL;
	while (list) {
		PurpleConversation *conv = list->data;
		list = list->next;
		purple_conversation_destroy(conv);
	}
}

static void
size_changed_cb(GntWidget *widget, int width, int height)
{
	int w, h;
	gnt_widget_get_size(widget, &w, &h);
	purple_prefs_set_int(PREF_ROOT "/size/width", w);
	purple_prefs_set_int(PREF_ROOT "/size/height", h);
}

static void
save_position_cb(GntWidget *w, int x, int y)
{
	purple_prefs_set_int(PREF_ROOT "/position/x", x);
	purple_prefs_set_int(PREF_ROOT "/position/y", y);
}

static PurpleConversation *
find_conv_with_contact(PurpleConversation *conv)
{
	PurpleBlistNode *node;
	PurpleBuddy *buddy = purple_find_buddy(conv->account, conv->name);
	PurpleConversation *ret = NULL;

	if (!buddy)
		return NULL;

	for (node = ((PurpleBlistNode*)buddy)->parent->child; node; node = node->next) {
		if (node == (PurpleBlistNode*)buddy)
			continue;
		if ((ret = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
				((PurpleBuddy*)node)->name, ((PurpleBuddy*)node)->account)) != NULL)
			break;
	}
	return ret;
}

static char *
get_conversation_title(PurpleConversation *conv, PurpleAccount *account)
{
	return g_strdup_printf(_("%s (%s -- %s)"), purple_conversation_get_title(conv),
		purple_account_get_username(account), purple_account_get_protocol_name(account));
}

static void
update_buddy_typing(PurpleAccount *account, const char *who, gpointer null)
{
	PurpleConversation *conv;
	FinchConv *ggc;
	PurpleConvIm *im = NULL;
	char *title, *str;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, account);

	if (!conv)
		return;

	im = PURPLE_CONV_IM(conv);
	ggc = conv->ui_data;

	if (purple_conv_im_get_typing_state(im) == PURPLE_TYPING) {
		int scroll;
		str = get_conversation_title(conv, account);
		title = g_strdup_printf(_("%s [%s]"), str,
			gnt_ascii_only() ? "T" : "\342\243\277");
		g_free(str);

		scroll = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggc->tv));
		str = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_name(conv));
		/* Updating is a little buggy. So just remove and add a new one */
		gnt_text_view_tag_change(GNT_TEXT_VIEW(ggc->tv), "typing", NULL, TRUE);
		gnt_text_view_append_text_with_tag(GNT_TEXT_VIEW(ggc->tv),
					str, GNT_TEXT_FLAG_DIM, "typing");
		g_free(str);
		if (scroll <= 1)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggc->tv), 0);
 	} else {
		title = get_conversation_title(conv, account);
		gnt_text_view_tag_change(GNT_TEXT_VIEW(ggc->tv), "typing", NULL, TRUE);
	}
	gnt_screen_rename_widget(ggc->window, title);
	g_free(title);
}

static gpointer
finch_conv_get_handle()
{
	static int handle;
	return &handle;
}

static void
finch_create_conversation(PurpleConversation *conv)
{
	FinchConv *ggc = conv->ui_data;
	char *title;
	PurpleConversationType type;
	PurpleConversation *cc;
	PurpleAccount *account;

	if (ggc)
		return;

	cc = find_conv_with_contact(conv);
	if (cc && cc->ui_data)
		ggc = cc->ui_data;
	else
		ggc = g_new0(FinchConv, 1);

	ggc->list = g_list_prepend(ggc->list, conv);
	ggc->active_conv = conv;
	conv->ui_data = ggc;

	if (cc && cc->ui_data) {
		finch_conversation_set_active(conv);
		return;
	}

	account = purple_conversation_get_account(conv);
	type = purple_conversation_get_type(conv);
	title = get_conversation_title(conv, account);

	ggc->window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_title(GNT_BOX(ggc->window), title);
	gnt_box_set_toplevel(GNT_BOX(ggc->window), TRUE);
	gnt_box_set_pad(GNT_BOX(ggc->window), 0);
	gnt_widget_set_name(ggc->window, "conversation-window");

	ggc->tv = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->tv);
	gnt_widget_set_name(ggc->tv, "conversation-window-textview");
	gnt_widget_set_size(ggc->tv, purple_prefs_get_int(PREF_ROOT "/size/width"),
			purple_prefs_get_int(PREF_ROOT "/size/height"));

	ggc->entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->entry);
	gnt_widget_set_name(ggc->entry, "conversation-window-entry");
	gnt_entry_set_history_length(GNT_ENTRY(ggc->entry), -1);
	gnt_entry_set_word_suggest(GNT_ENTRY(ggc->entry), TRUE);
	gnt_entry_set_always_suggest(GNT_ENTRY(ggc->entry), FALSE);

	gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(ggc->tv), ggc->entry);
	g_signal_connect_after(G_OBJECT(ggc->entry), "key_pressed", G_CALLBACK(entry_key_pressed), ggc);
	g_signal_connect(G_OBJECT(ggc->window), "destroy", G_CALLBACK(closing_window), ggc);

	gnt_widget_set_position(ggc->window, purple_prefs_get_int(PREF_ROOT "/position/x"),
			purple_prefs_get_int(PREF_ROOT "/position/y"));
	gnt_widget_show(ggc->window);

	g_signal_connect(G_OBJECT(ggc->tv), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggc->window), "position_set", G_CALLBACK(save_position_cb), NULL);

	if (type == PURPLE_CONV_TYPE_IM) {
		g_signal_connect(G_OBJECT(ggc->entry), "text_changed", G_CALLBACK(send_typing_notification), ggc);
		purple_signal_connect(purple_conversations_get_handle(), "buddy-typing", finch_conv_get_handle(),
						PURPLE_CALLBACK(update_buddy_typing), NULL);
		purple_signal_connect(purple_conversations_get_handle(), "buddy-typing-stopped", finch_conv_get_handle(),
						PURPLE_CALLBACK(update_buddy_typing), NULL);
	}

	g_free(title);
}

static void
finch_destroy_conversation(PurpleConversation *conv)
{
	/* do stuff here */
	FinchConv *ggc = conv->ui_data;
	ggc->list = g_list_remove(ggc->list, conv);
	if (ggc->list && conv == ggc->active_conv)
		ggc->active_conv = ggc->list->data;
	
	if (ggc->list == NULL) {
		gnt_widget_destroy(ggc->window);
		g_free(ggc);
	}
}

static void
finch_write_common(PurpleConversation *conv, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	FinchConv *ggconv = conv->ui_data;
	char *strip, *newline;
	GntTextFormatFlags fl = 0;
	int pos;
	gboolean notify;

	g_return_if_fail(ggconv != NULL);

	if (ggconv->active_conv != conv) {
		if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV))
			finch_conversation_set_active(conv);
		else
			return;
	}

	pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggconv->tv));

	notify = !!gnt_text_view_tag_change(GNT_TEXT_VIEW(ggconv->tv), "typing", NULL, TRUE);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv), "\n", GNT_TEXT_FLAG_NORMAL);

	/* Unnecessary to print the timestamp for delayed message */
	if (!(flags & PURPLE_MESSAGE_DELAYED) &&
			purple_prefs_get_bool("/purple/gnt/conversations/timestamps"))
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					purple_utf8_strftime("(%H:%M:%S) ", localtime(&mtime)), GNT_TEXT_FLAG_DIM);

	if (flags & PURPLE_MESSAGE_AUTO_RESP)
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					_("<AUTO-REPLY> "), GNT_TEXT_FLAG_BOLD);

	if (who && *who && (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)))
	{
		char * name = NULL;

		if (purple_message_meify((char*)message, -1))
			name = g_strdup_printf("*** %s ", who);
		else
			name =  g_strdup_printf("%s: ", who);

		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				name, GNT_TEXT_FLAG_BOLD);
		g_free(name);
	}
	else
		fl = GNT_TEXT_FLAG_DIM;

	if (flags & PURPLE_MESSAGE_ERROR)
		fl |= GNT_TEXT_FLAG_BOLD;
	if (flags & PURPLE_MESSAGE_NICK)
		fl |= GNT_TEXT_FLAG_UNDERLINE;

	/* XXX: Remove this workaround when textview can parse messages. */
	newline = purple_strdup_withhtml(message);
	strip = purple_markup_strip_html(newline);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				strip, fl);

	g_free(newline);
	g_free(strip);

	if (notify) {
		strip = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_name(conv));
		gnt_text_view_append_text_with_tag(GNT_TEXT_VIEW(ggconv->tv),
					strip, GNT_TEXT_FLAG_DIM, "typing");
		g_free(strip);
	}

	if (pos <= 1)
		gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 0);

	if (flags & (PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_NICK | PURPLE_MESSAGE_ERROR))
		gnt_widget_set_urgent(ggconv->tv);
}

static void
finch_write_chat(PurpleConversation *conv, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	purple_conversation_write(conv, who, message, flags, mtime);
}

static void
finch_write_im(PurpleConversation *conv, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	if (flags & PURPLE_MESSAGE_SEND)
	{
		who = purple_connection_get_display_name(purple_account_get_connection(account));
		if (!who)
			who = purple_account_get_alias(account);
		if (!who)
			who = purple_account_get_username(account);
	}
	else if (flags & PURPLE_MESSAGE_RECV)
	{
		PurpleBuddy *buddy;
		who = purple_conversation_get_name(conv);
		buddy = purple_find_buddy(account, who);
		if (buddy)
			who = purple_buddy_get_contact_alias(buddy);
	}

	purple_conversation_write(conv, who, message, flags, mtime);
}

static void
finch_write_conv(PurpleConversation *conv, const char *who, const char *alias,
		const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const char *name;
	if (alias && *alias)
		name = alias;
	else if (who && *who)
		name = who;
	else
		name = NULL;

	finch_write_common(conv, name, message, flags, mtime);
}

static void
finch_chat_add_users(PurpleConversation *conv, GList *users, gboolean new_arrivals)
{
	FinchConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);

	if (!new_arrivals)
	{
		/* Print the list of users in the room */
		GString *string = g_string_new(_("List of users:\n"));
		GList *iter;

		for (iter = users; iter; iter = iter->next)
		{
			PurpleConvChatBuddy *cbuddy = iter->data;
			char *str;

			if ((str = cbuddy->alias) == NULL)
				str = cbuddy->name;
			g_string_append_printf(string, "[ %s ]", str);
		}

		purple_conversation_write(conv, NULL, string->str,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_string_free(string, TRUE);
	}

	for (; users; users = users->next)
	{
		PurpleConvChatBuddy *cbuddy = users->data;
		gnt_entry_add_suggest(entry, cbuddy->name);
		gnt_entry_add_suggest(entry, cbuddy->alias);
	}
}

static void
finch_chat_rename_user(PurpleConversation *conv, const char *old, const char *new_n, const char *new_a)
{
	/* Update the name for string completion */
	FinchConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	gnt_entry_remove_suggest(entry, old);
	gnt_entry_add_suggest(entry, new_n);
	gnt_entry_add_suggest(entry, new_a);
}

static void
finch_chat_remove_user(PurpleConversation *conv, GList *list)
{
	/* Remove the name from string completion */
	FinchConv *ggc = conv->ui_data;
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	for (; list; list = list->next)
		gnt_entry_remove_suggest(entry, list->data);
}

static void
finch_chat_update_user(PurpleConversation *conv, const char *user)
{
}

static PurpleConversationUiOps conv_ui_ops = 
{
	.create_conversation = finch_create_conversation,
	.destroy_conversation = finch_destroy_conversation,
	.write_chat = finch_write_chat,
	.write_im = finch_write_im,
	.write_conv = finch_write_conv,
	.chat_add_users = finch_chat_add_users,
	.chat_rename_user = finch_chat_rename_user,
	.chat_remove_users = finch_chat_remove_user,
	.chat_update_user = finch_chat_update_user,
	.present = NULL,
	.has_focus = NULL,
	.custom_smiley_add = NULL,
	.custom_smiley_write = NULL,
	.custom_smiley_close = NULL
};

PurpleConversationUiOps *finch_conv_get_ui_ops()
{
	return &conv_ui_ops;
}

/* Xerox */
static PurpleCmdRet
say_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		purple_conv_im_send(PURPLE_CONV_IM(conv), args[0]);
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		purple_conv_chat_send(PURPLE_CONV_CHAT(conv), args[0]);

	return PURPLE_CMD_RET_OK;
}

/* Xerox */
static PurpleCmdRet
me_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	char *tmp;

	tmp = g_strdup_printf("/me %s", args[0]);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		purple_conv_im_send(PURPLE_CONV_IM(conv), tmp);
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		purple_conv_chat_send(PURPLE_CONV_CHAT(conv), tmp);

	g_free(tmp);
	return PURPLE_CMD_RET_OK;
}

/* Xerox */
static PurpleCmdRet
debug_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;
	PurpleCmdStatus status;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("me is using %s.", VERSION);
		markup = g_markup_escape_text(tmp, -1);

		status = purple_cmd_do_command(conv, tmp, markup, error);

		g_free(tmp);
		g_free(markup);
		return status;
	} else {
		purple_conversation_write(conv, NULL, _("Supported debug options are:  version"),
		                        PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_ERROR, time(NULL));
		return PURPLE_CMD_STATUS_OK;
	}
}

/* Xerox */
static PurpleCmdRet
clear_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	FinchConv *ggconv = conv->ui_data;
	gnt_text_view_clear(GNT_TEXT_VIEW(ggconv->tv));
	return PURPLE_CMD_STATUS_OK;
}

/* Xerox */
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
		s = g_string_new(_("Use \"/help &lt;command&gt;\" for help on a specific command.\n"
											 "The following commands are available in this context:\n"));

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

	return PURPLE_CMD_STATUS_OK;
}

static PurpleCmdRet
cmd_show_window(PurpleConversation *conv, const char *cmd, char **args, char **error, gpointer data)
{
	void (*callback)() = data;
	callback();
	return PURPLE_CMD_STATUS_OK;
}

void finch_conversation_init()
{
	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_none(PREF_ROOT "/size");
	purple_prefs_add_int(PREF_ROOT "/size/width", 70);
	purple_prefs_add_int(PREF_ROOT "/size/height", 20);
	purple_prefs_add_none(PREF_ROOT "/position");
	purple_prefs_add_int(PREF_ROOT "/position/x", 0);
	purple_prefs_add_int(PREF_ROOT "/position/y", 0);

	/* Xerox the commands */
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
	purple_cmd_register("help", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  help_command_cb, _("help &lt;command&gt;:  Help on a specific command."), NULL);

	/* Now some commands to bring up some other windows */
	purple_cmd_register("plugins", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("plugins: Show the plugins window."), finch_plugins_show_all);
	purple_cmd_register("buddylist", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("buddylist: Show the buddylist."), finch_blist_show);
	purple_cmd_register("accounts", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("accounts: Show the accounts window."), finch_accounts_show_all);
	purple_cmd_register("debugwin", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("debugwin: Show the debug window."), finch_debug_window_show);
	purple_cmd_register("prefs", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("prefs: Show the preference window."), finch_prefs_show_all);
	purple_cmd_register("status", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  cmd_show_window, _("statuses: Show the savedstatuses window."), finch_savedstatus_show_all);
}

void finch_conversation_uninit()
{
}

void finch_conversation_set_active(PurpleConversation *conv)
{
	FinchConv *ggconv = conv->ui_data;
	PurpleAccount *account;
	char *title;

	g_return_if_fail(ggconv);
	g_return_if_fail(g_list_find(ggconv->list, conv));

	ggconv->active_conv = conv;
	account = purple_conversation_get_account(conv);
	title = get_conversation_title(conv, account);
	gnt_screen_rename_widget(ggconv->window, title);
	g_free(title);
}

