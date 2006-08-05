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

#include "gnt.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gnttextview.h"

#define PREF_ROOT	"/gaim/gnt/conversations"

GHashTable *ggconvs;

typedef struct _GGConv GGConv;
typedef struct _GGConvChat GGConvChat;
typedef struct _GGConvIm GGConvIm;

struct _GGConv
{
	GaimConversation *conv;

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
			GaimConversation *conv = ggconv->conv;
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
			switch (gaim_conversation_get_type(ggconv->conv))
			{
				case GAIM_CONV_TYPE_IM:
					gaim_conv_im_send_with_flags(GAIM_CONV_IM(ggconv->conv), text, GAIM_MESSAGE_SEND);
					break;
				case GAIM_CONV_TYPE_CHAT:
					gaim_conv_chat_send(GAIM_CONV_CHAT(ggconv->conv), text);
					break;
				default:
					g_return_val_if_reached(FALSE);
			}
		}
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
	ggconv->window = NULL;
	gaim_conversation_destroy(ggconv->conv);
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

static void
gg_create_conversation(GaimConversation *conv)
{
	GGConv *ggc = g_hash_table_lookup(ggconvs, conv);
	char *title;
	GaimConversationType type;

	if (ggc)
		return;

	ggc = g_new0(GGConv, 1);
	g_hash_table_insert(ggconvs, conv, ggc);

	ggc->conv = conv;
	conv->ui_data = ggc;

	type = gaim_conversation_get_type(conv);
	title = g_strdup_printf(_("%s"), gaim_conversation_get_title(conv));
	ggc->window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_title(GNT_BOX(ggc->window), title);
	gnt_box_set_toplevel(GNT_BOX(ggc->window), TRUE);
	gnt_box_set_pad(GNT_BOX(ggc->window), 0);
	gnt_widget_set_name(ggc->window, title);

	ggc->tv = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->tv);
	gnt_widget_set_name(ggc->tv, "conversation-window-textview");
	gnt_widget_set_size(ggc->tv, gaim_prefs_get_int(PREF_ROOT "/size/width"),
			gaim_prefs_get_int(PREF_ROOT "/size/height"));

	ggc->entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->entry);
	gnt_widget_set_name(ggc->entry, "conversation-window-entry");

	g_signal_connect(G_OBJECT(ggc->entry), "key_pressed", G_CALLBACK(entry_key_pressed), ggc);
	g_signal_connect(G_OBJECT(ggc->window), "destroy", G_CALLBACK(closing_window), ggc);

	gnt_widget_set_position(ggc->window, gaim_prefs_get_int(PREF_ROOT "/position/x"),
			gaim_prefs_get_int(PREF_ROOT "/position/y"));
	gnt_widget_show(ggc->window);

	g_signal_connect(G_OBJECT(ggc->tv), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggc->window), "position_set", G_CALLBACK(save_position_cb), NULL);

	g_free(title);
}

static void
gg_destroy_conversation(GaimConversation *conv)
{
	g_hash_table_remove(ggconvs, conv);
}

static void
gg_write_common(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	GGConv *ggconv = g_hash_table_lookup(ggconvs, conv); /* XXX: ggconv = conv->ui_data; should do */
	char *strip, *newline;
	GntTextFormatFlags fl = 0;
	int pos;

	g_return_if_fail(ggconv != NULL);

	if (who && *who && (flags & (GAIM_MESSAGE_SEND | GAIM_MESSAGE_RECV)))
	{
		char * name = g_strdup_printf("%s: ", who);
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

	pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggconv->tv));

	/* XXX: Remove this workaround when textview can parse messages. */
	newline = gaim_strdup_withhtml(message);
	strip = gaim_markup_strip_html(newline);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				strip, fl);
	gnt_text_view_next_line(GNT_TEXT_VIEW(ggconv->tv));
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
	gg_write_common(conv, who, message, flags, mtime);
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

	gg_write_common(conv, who, message, flags, mtime);
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
	/* XXX: Add the names for string completion */
}

static void
gg_chat_rename_user(GaimConversation *conv, const char *old, const char *new_n, const char *new_a)
{
	/* XXX: Update the name for string completion */
}

static void
gg_chat_remove_user(GaimConversation *conv, GList *list)
{
	/* XXX: Remove the name from string completion */
}

static void
gg_chat_update_user(GaimConversation *conv, const char *user)
{
	/* XXX: This probably will not require updating the string completion  */
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

static void
destroy_ggconv(gpointer data)
{
	GGConv *ggconv = data;
	gnt_widget_destroy(ggconv->window);
	g_free(ggconv);
}

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
	ggconvs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, destroy_ggconv);

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
}

void gg_conversation_uninit()
{
	g_hash_table_destroy(ggconvs);
	ggconvs = NULL;
}

