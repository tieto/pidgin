/**
 * @file gntconv.c GNT Conversation API
 * @ingroup finch
 */

/* finch
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <internal.h>
#include "finch.h"

#include <cmds.h>
#include <core.h>
#include <idle.h>
#include <prefs.h>
#include <util.h>

#include "gntaccount.h"
#include "gntblist.h"
#include "gntconv.h"
#include "gntdebug.h"
#include "gntlog.h"
#include "gntplugin.h"
#include "gntpounce.h"
#include "gntprefs.h"
#include "gntrequest.h"
#include "gntsound.h"
#include "gntstatus.h"

#include "gnt.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gntlabel.h"
#include "gntmenu.h"
#include "gntmenuitem.h"
#include "gntmenuitemcheck.h"
#include "gntmenuutil.h"
#include "gntstyle.h"
#include "gnttextview.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

#define PREF_ROOT	"/finch/conversations"
#define PREF_CHAT   PREF_ROOT "/chats"
#define PREF_USERLIST PREF_CHAT "/userlist"

#include "config.h"

static void finch_write_common(PurpleConversation *conv, const char *who,
		const char *message, PurpleMessageFlags flags, time_t mtime);
static void generate_send_to_menu(FinchConv *ggc);

static int color_message_receive;
static int color_message_send;
static int color_message_highlight;
static int color_message_action;
static int color_timestamp;

static PurpleBuddy *
find_buddy_for_conversation(PurpleConversation *conv)
{
	return purple_find_buddy(purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
}

static PurpleChat *
find_chat_for_conversation(PurpleConversation *conv)
{
	return purple_blist_find_chat(purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
}

static PurpleBListNode *
get_conversation_blist_node(PurpleConversation *conv)
{
	PurpleBListNode *node = NULL;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		node = (PurpleBListNode*)find_buddy_for_conversation(conv);
		node = node ? purple_blist_node_get_parent(node) : NULL;
	} else {
		node = (PurpleBListNode*)find_chat_for_conversation(conv);
	}

	return node;
}

static void
send_typing_notification(GntWidget *w, FinchConv *ggconv)
{
	const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
	gboolean empty = (!text || !*text || (*text == '/'));
	if (purple_prefs_get_bool("/finch/conversations/notify_typing")) {
		PurpleConversation *conv = ggconv->active_conv;
		PurpleIMConversation *im = PURPLE_IM_CONVERSATION(conv);
		if (!empty) {
			gboolean send = (purple_im_conversation_get_send_typed_timeout(im) == 0);

			purple_im_conversation_stop_send_typed_timeout(im);
			purple_im_conversation_start_send_typed_timeout(im);
			if (send || (purple_im_conversation_get_type_again(im) != 0 &&
						  time(NULL) > purple_im_conversation_get_type_again(im))) {
				unsigned int timeout;
				timeout = serv_send_typing(purple_conversation_get_connection(conv),
										   purple_conversation_get_name(conv),
										   PURPLE_IM_TYPING);
				purple_im_conversation_set_type_again(im, timeout);
			}
		} else {
			purple_im_conversation_stop_send_typed_timeout(im);

			serv_send_typing(purple_conversation_get_connection(conv),
							 purple_conversation_get_name(conv),
							 PURPLE_IM_NOT_TYPING);
		}
	}
}

static void
entry_key_pressed(GntWidget *w, FinchConv *ggconv)
{
	const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
	if (*text == '/' && *(text + 1) != '/')
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
				if(PURPLE_IS_IM_CONVERSATION(conv))
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
	}
	else if (!purple_account_is_connected(purple_conversation_get_account(ggconv->active_conv)))
	{
		purple_conversation_write(ggconv->active_conv, "", _("Message was not sent, because you are not signed on."),
				PURPLE_MESSAGE_ERROR | PURPLE_MESSAGE_NO_LOG, time(NULL));
	}
	else
	{
		char *escape = purple_markup_escape_text((*text == '/' ? text + 1 : text), -1);
		purple_conversation_send(ggconv->active_conv, escape);
		g_free(escape);
		purple_idle_touch();
	}
	gnt_entry_add_to_history(GNT_ENTRY(ggconv->entry), text);
	gnt_entry_clear(GNT_ENTRY(ggconv->entry));
}

static void
closing_window(GntWidget *window, FinchConv *ggconv)
{
	GList *list = ggconv->list;
	ggconv->window = NULL;
	while (list) {
		PurpleConversation *conv = list->data;
		list = list->next;
		g_object_unref(conv);
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

static PurpleIMConversation *
find_im_with_contact(PurpleAccount *account, const char *name)
{
	PurpleBListNode *node;
	PurpleBuddy *buddy = purple_find_buddy(account, name);
	PurpleIMConversation *im = NULL;

	if (!buddy)
		return NULL;

	for (node = purple_blist_node_get_first_child(purple_blist_node_get_parent((PurpleBListNode*)buddy));
				node; node = purple_blist_node_get_sibling_next(node)) {
		if (node == (PurpleBListNode*)buddy)
			continue;
		if ((im = purple_conversations_find_im_with_account(
				purple_buddy_get_name((PurpleBuddy*)node), purple_buddy_get_account((PurpleBuddy*)node))) != NULL)
			break;
	}
	return im;
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
	FinchConv *ggc;
	PurpleIMConversation *im;
	PurpleConversation *conv;
	char *title, *str;

	im = purple_conversations_find_im_with_account(who, account);

	if (!im)
		return;

	conv = PURPLE_CONVERSATION(im);
	ggc = FINCH_CONV(conv);

	if (purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPING) {
		int scroll;
		str = get_conversation_title(conv, account);
		title = g_strdup_printf(_("%s [%s]"), str,
			gnt_ascii_only() ? "T" : "\342\243\277");
		g_free(str);

		scroll = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggc->tv));
		str = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_title(conv));
		/* Updating is a little buggy. So just remove and add a new one */
		gnt_text_view_tag_change(GNT_TEXT_VIEW(ggc->tv), "typing", NULL, TRUE);
		gnt_text_view_append_text_with_tag(GNT_TEXT_VIEW(ggc->tv),
					str, GNT_TEXT_FLAG_DIM, "typing");
		g_free(str);
		if (scroll <= 1)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggc->tv), 0);
 	} else {
		title = get_conversation_title(conv, account);
		gnt_text_view_tag_change(GNT_TEXT_VIEW(ggc->tv), "typing", " ", TRUE);
	}
	gnt_screen_rename_widget(ggc->window, title);
	g_free(title);
}

static void
chat_left_cb(PurpleConversation *conv, gpointer null)
{
	finch_write_common(conv, NULL, _("You have left this chat."),
			PURPLE_MESSAGE_SYSTEM, time(NULL));
}

static void
buddy_signed_on_off(PurpleBuddy *buddy, gpointer null)
{
	PurpleIMConversation *im = find_im_with_contact(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
	if (im == NULL)
		return;
	generate_send_to_menu(FINCH_CONV(PURPLE_CONVERSATION(im)));
}

static void
account_signed_on_off(PurpleConnection *gc, gpointer null)
{
	GList *list = purple_conversations_get_ims();
	while (list) {
		PurpleConversation *conv = list->data;
		PurpleIMConversation *cc = find_im_with_contact(
				purple_conversation_get_account(conv), purple_conversation_get_name(conv));
		if (cc)
			generate_send_to_menu(FINCH_CONV(PURPLE_CONVERSATION(cc)));
		list = list->next;
	}

	if (PURPLE_CONNECTION_IS_CONNECTED(gc)) {
		/* We just signed on. Let's see if there's any chat that we have open,
		 * and hadn't left before the disconnect. */
		list = purple_conversations_get_chats();
		while (list) {
			PurpleConversation *conv = list->data;
			PurpleChat *chat;
			GHashTable *comps = NULL;

			list = list->next;
			if (purple_conversation_get_account(conv) != purple_connection_get_account(gc) ||
					!g_object_get_data(G_OBJECT(conv), "want-to-rejoin"))
				continue;

			chat = find_chat_for_conversation(conv);
			if (chat == NULL) {
				PurplePluginProtocolInfo *info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
				if (info->chat_info_defaults != NULL)
					comps = info->chat_info_defaults(gc, purple_conversation_get_name(conv));
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
						"longer in this chat. You will be automatically rejoined in the chat when "
						"the account reconnects."),
					PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, time(NULL));
		}
		list = list->next;
	}
}

static gpointer
finch_conv_get_handle(void)
{
	static int handle;
	return &handle;
}

static void
cleared_message_history_cb(PurpleConversation *conv, gpointer data)
{
	FinchConv *ggc = FINCH_CONV(conv);
	if (ggc)
		gnt_text_view_clear(GNT_TEXT_VIEW(ggc->tv));
}

static void
gg_extended_menu(FinchConv *ggc)
{
	GntWidget *sub;
	GList *list;

	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(ggc->plugins, GNT_MENU(sub));

	for (list = purple_conversation_get_extended_menu(ggc->active_conv);
			list; list = g_list_delete_link(list, list))
	{
		gnt_append_menu_action(GNT_MENU(sub), list->data, ggc->active_conv);
	}
}

static void
conv_updated(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	if (type == PURPLE_CONVERSATION_UPDATE_FEATURES) {
		gg_extended_menu(purple_conversation_get_ui_data(conv));
	}
}

static void
clear_scrollback_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *ggc = ggconv;
	purple_conversation_clear_message_history(ggc->active_conv);
}

static void
send_file_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *ggc = ggconv;
	serv_send_file(purple_conversation_get_connection(ggc->active_conv),
			purple_conversation_get_name(ggc->active_conv), NULL);
}

static void
add_pounce_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *ggc = ggconv;
	finch_pounce_editor_show(
			purple_conversation_get_account(ggc->active_conv),
			purple_conversation_get_name(ggc->active_conv), NULL);
}

static void
get_info_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *ggc = ggconv;
	finch_retrieve_user_info(purple_conversation_get_connection(ggc->active_conv),
			purple_conversation_get_name(ggc->active_conv));
}

static void
toggle_timestamps_cb(GntMenuItem *item, gpointer ggconv)
{
	purple_prefs_set_bool(PREF_ROOT "/timestamps",
		!purple_prefs_get_bool(PREF_ROOT "/timestamps"));
}

static void
toggle_logging_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *fc = ggconv;
	PurpleConversation *conv = fc->active_conv;
	gboolean logging = gnt_menuitem_check_get_checked(GNT_MENU_ITEM_CHECK(item));
	GList *iter;

	if (logging == purple_conversation_is_logging(conv))
		return;

	/* Xerox */
	if (logging) {
		/* Enable logging first so the message below can be logged. */
		purple_conversation_set_logging(conv, TRUE);

		purple_conversation_write(conv, NULL,
				_("Logging started. Future messages in this conversation will be logged."),
				PURPLE_MESSAGE_SYSTEM, time(NULL));
	} else {
		purple_conversation_write(conv, NULL,
				_("Logging stopped. Future messages in this conversation will not be logged."),
				PURPLE_MESSAGE_SYSTEM, time(NULL));

		/* Disable the logging second, so that the above message can be logged. */
		purple_conversation_set_logging(conv, FALSE);
	}

	/* Each conversation with the same person will have the same logging setting */
	for (iter = fc->list; iter; iter = iter->next) {
		if (iter->data == conv)
			continue;
		purple_conversation_set_logging(iter->data, logging);
	}
}

static void
toggle_sound_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *fc = ggconv;
	PurpleBListNode *node = get_conversation_blist_node(fc->active_conv);
	fc->flags ^= FINCH_CONV_NO_SOUND;
	if (node)
		purple_blist_node_set_bool(node, "gnt-mute-sound", !!(fc->flags & FINCH_CONV_NO_SOUND));
}

static void
send_to_cb(GntMenuItem *m, gpointer n)
{
	PurpleAccount *account = g_object_get_data(G_OBJECT(m), "purple_account");
	gchar *buddy = g_object_get_data(G_OBJECT(m), "purple_buddy_name");
	PurpleIMConversation *im = purple_im_conversation_new(account, buddy);
	finch_conversation_set_active(PURPLE_CONVERSATION(im));
}

static void
view_log_cb(GntMenuItem *n, gpointer ggc)
{
	FinchConv *fc;
	PurpleConversation *conv;
	PurpleLogType type;
	const char *name;
	PurpleAccount *account;
	GSList *buddies;
	GSList *cur;

	fc = ggc;
	conv = fc->active_conv;

	if (PURPLE_IS_IM_CONVERSATION(conv))
		type = PURPLE_LOG_IM;
	else
		type = PURPLE_LOG_CHAT;

	name = purple_conversation_get_name(conv);
	account = purple_conversation_get_account(conv);

	buddies = purple_find_buddies(account, name);
	for (cur = buddies; cur != NULL; cur = cur->next) {
		PurpleBListNode *node = cur->data;
		if ((node != NULL) &&
				(purple_blist_node_get_sibling_prev(node) || purple_blist_node_get_sibling_next(node))) {
			finch_log_show_contact((PurpleContact *)purple_blist_node_get_parent(node));
			g_slist_free(buddies);
			return;
		}
	}
	g_slist_free(buddies);

	finch_log_show(type, name, account);
}

static void
generate_send_to_menu(FinchConv *ggc)
{
	GntWidget *sub, *menu = ggc->menu;
	GntMenuItem *item;
	GSList *buds;
	GList *list = NULL;

	buds = purple_find_buddies(purple_conversation_get_account(ggc->active_conv),
			purple_conversation_get_name(ggc->active_conv));
	if (!buds)
		return;

	if ((item = ggc->u.im->sendto) == NULL) {
		item = gnt_menuitem_new(_("Send To"));
		gnt_menu_add_item(GNT_MENU(menu), item);
		ggc->u.im->sendto = item;
	}
	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(sub));

	for (; buds; buds = g_slist_delete_link(buds, buds)) {
		PurpleBListNode *node = PURPLE_BLIST_NODE(purple_buddy_get_contact(PURPLE_BUDDY(buds->data)));
		for (node = purple_blist_node_get_first_child(node); node != NULL;
				node = purple_blist_node_get_sibling_next(node)) {
			PurpleBuddy *buddy = (PurpleBuddy *)node;
			PurpleAccount *account = purple_buddy_get_account(buddy);
			if (purple_account_is_connected(account)) {
				/* Use the PurplePresence to get unique buddies. */
				PurplePresence *presence = purple_buddy_get_presence(buddy);
				if (!g_list_find(list, presence))
					list = g_list_prepend(list, presence);
			}
		}
	}
	for (list = g_list_reverse(list); list != NULL; list = g_list_delete_link(list, list)) {
		PurplePresence *pre = list->data;
		PurpleBuddy *buddy = purple_presence_get_buddy(pre);
		PurpleAccount *account = purple_buddy_get_account(buddy);
		gchar *name = g_strdup(purple_buddy_get_name(buddy));
		gchar *text = g_strdup_printf("%s (%s)", purple_buddy_get_name(buddy), purple_account_get_username(account));
		item = gnt_menuitem_new(text);
		g_free(text);
		gnt_menu_add_item(GNT_MENU(sub), item);
		gnt_menuitem_set_callback(item, send_to_cb, NULL);
		g_object_set_data(G_OBJECT(item), "purple_account", account);
		g_object_set_data_full(G_OBJECT(item), "purple_buddy_name", name, g_free);
	}
}

static void
invite_cb(GntMenuItem *item, gpointer ggconv)
{
	FinchConv *fc = ggconv;
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(fc->active_conv);
	purple_chat_conversation_invite_user(chat, NULL, NULL, TRUE);
}

static void
plugin_changed_cb(PurplePlugin *p, gpointer data)
{
	gg_extended_menu(data);
}

static void
gg_create_menu(FinchConv *ggc)
{
	GntWidget *menu, *sub;
	GntMenuItem *item;

	ggc->menu = menu = gnt_menu_new(GNT_MENU_TOPLEVEL);
	gnt_window_set_menu(GNT_WINDOW(ggc->window), GNT_MENU(menu));

	item = gnt_menuitem_new(_("Conversation"));
	gnt_menu_add_item(GNT_MENU(menu), item);

	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(sub));

	item = gnt_menuitem_new(_("Clear Scrollback"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(item, clear_scrollback_cb, ggc);

	item = gnt_menuitem_check_new(_("Show Timestamps"));
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(item),
		purple_prefs_get_bool(PREF_ROOT "/timestamps"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(item, toggle_timestamps_cb, ggc);

	if (PURPLE_IS_IM_CONVERSATION(ggc->active_conv)) {
		PurpleAccount *account = purple_conversation_get_account(ggc->active_conv);
		PurpleConnection *gc = purple_account_get_connection(account);
		PurplePluginProtocolInfo *pinfo =
			gc ? PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc)) : NULL;

		if (pinfo && pinfo->get_info) {
			item = gnt_menuitem_new(_("Get Info"));
			gnt_menu_add_item(GNT_MENU(sub), item);
			gnt_menuitem_set_callback(item, get_info_cb, ggc);
		}

		item = gnt_menuitem_new(_("Add Buddy Pounce..."));
		gnt_menu_add_item(GNT_MENU(sub), item);
		gnt_menuitem_set_callback(item, add_pounce_cb, ggc);

		if (pinfo && pinfo->send_file &&
				(!pinfo->can_receive_file ||
					pinfo->can_receive_file(gc, purple_conversation_get_name(ggc->active_conv)))) {
			item = gnt_menuitem_new(_("Send File"));
			gnt_menu_add_item(GNT_MENU(sub), item);
			gnt_menuitem_set_callback(item, send_file_cb, ggc);
		}

		generate_send_to_menu(ggc);
	} else {
		item = gnt_menuitem_new(_("Invite..."));
		gnt_menu_add_item(GNT_MENU(sub), item);
		gnt_menuitem_set_callback(item, invite_cb, ggc);
	}

	item = gnt_menuitem_new(_("View Log..."));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(item, view_log_cb, ggc);

	item = gnt_menuitem_check_new(_("Enable Logging"));
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(item),
			purple_conversation_is_logging(ggc->active_conv));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(item, toggle_logging_cb, ggc);

	item = gnt_menuitem_check_new(_("Enable Sounds"));
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(item),
			!(ggc->flags & FINCH_CONV_NO_SOUND));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(item, toggle_sound_cb, ggc);

	item = gnt_menuitem_new(_("Plugins"));
	gnt_menu_add_item(GNT_MENU(menu), item);
	ggc->plugins = item;

	gg_extended_menu(ggc);
}

static void
create_conv_from_userlist(GntWidget *widget, FinchConv *fc)
{
	PurpleAccount *account = purple_conversation_get_account(fc->active_conv);
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePluginProtocolInfo *prpl_info = NULL;
	char *name, *realname;

	if (!gc) {
		purple_conversation_write(fc->active_conv, NULL, _("You are not connected."),
				PURPLE_MESSAGE_SYSTEM, time(NULL));
		return;
	}

	name = gnt_tree_get_selection_data(GNT_TREE(widget));

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	if (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, get_cb_real_name))
		realname = prpl_info->get_cb_real_name(gc, purple_chat_conversation_get_id(
				PURPLE_CHAT_CONVERSATION(fc->active_conv)), name);
	else
		realname = NULL;
	purple_im_conversation_new(account, realname ? realname : name);
	g_free(realname);
}

static void
gained_focus_cb(GntWindow *window, FinchConv *fc)
{
	GList *iter;
	for (iter = fc->list; iter; iter = iter->next) {
		g_object_set_data(G_OBJECT(iter->data), "unseen-count", 0);
		purple_conversation_update(iter->data, PURPLE_CONVERSATION_UPDATE_UNSEEN);
	}
}

static void
completion_cb(GntEntry *entry, const char *start, const char *end)
{
	if (start == entry->start && *start != '/')
		gnt_widget_key_pressed(GNT_WIDGET(entry), ": ");
}

static void
gg_setup_commands(FinchConv *fconv, gboolean remove_first)
{
	GList *commands;
	char command[256] = "/";

	if (remove_first) {
		commands = purple_cmd_list(NULL);
		for (; commands; commands = g_list_delete_link(commands, commands)) {
			g_strlcpy(command + 1, commands->data, sizeof(command) - 1);
			gnt_entry_remove_suggest(GNT_ENTRY(fconv->entry), command);
		}
	}

	commands = purple_cmd_list(fconv->active_conv);
	for (; commands; commands = g_list_delete_link(commands, commands)) {
		g_strlcpy(command + 1, commands->data, sizeof(command) - 1);
		gnt_entry_add_suggest(GNT_ENTRY(fconv->entry), command);
	}
}

static void
cmd_added_cb(const char *cmd, PurpleCmdPriority prior, PurpleCmdFlag flags,
		FinchConv *fconv)
{
	gg_setup_commands(fconv, TRUE);
}

static void
cmd_removed_cb(const char *cmd, FinchConv *fconv)
{
	char command[256] = "/";
	g_strlcpy(command + 1, cmd, sizeof(command) - 1);
	gnt_entry_remove_suggest(GNT_ENTRY(fconv->entry), command);
	gg_setup_commands(fconv, TRUE);
}

static void
finch_create_conversation(PurpleConversation *conv)
{
	FinchConv *ggc = FINCH_CONV(conv);
	char *title;
	PurpleConversation *cc;
	PurpleAccount *account;
	PurpleBListNode *convnode = NULL;

	if (ggc) {
		gnt_window_present(ggc->window);
		return;
	}

	account = purple_conversation_get_account(conv);
	cc = PURPLE_CONVERSATION(find_im_with_contact(account, purple_conversation_get_name(conv)));
	if (cc && FINCH_CONV(cc))
		ggc = FINCH_CONV(cc);
	else
		ggc = g_new0(FinchConv, 1);

	/* Each conversation with the same person will have the same logging setting */
	if (ggc->list) {
		purple_conversation_set_logging(conv,
				purple_conversation_is_logging(ggc->list->data));
	}

	ggc->list = g_list_prepend(ggc->list, conv);
	ggc->active_conv = conv;
	purple_conversation_set_ui_data(conv, ggc);

	if (cc && FINCH_CONV(cc) && cc != conv) {
		finch_conversation_set_active(conv);
		return;
	}

	title = get_conversation_title(conv, account);

	ggc->window = gnt_vwindow_new(FALSE);
	gnt_box_set_title(GNT_BOX(ggc->window), title);
	gnt_box_set_toplevel(GNT_BOX(ggc->window), TRUE);
	gnt_box_set_pad(GNT_BOX(ggc->window), 0);

	gnt_widget_set_name(ggc->window,
			PURPLE_IS_IM_CONVERSATION(conv) ? "conversation-window-im" : "conversation-window-chat");

	ggc->tv = gnt_text_view_new();
	gnt_widget_set_name(ggc->tv, "conversation-window-textview");
	gnt_widget_set_size(ggc->tv, purple_prefs_get_int(PREF_ROOT "/size/width"),
			purple_prefs_get_int(PREF_ROOT "/size/height"));

	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		GntWidget *hbox, *tree;
		FinchConvChat *fc = ggc->u.chat = g_new0(FinchConvChat, 1);
		hbox = gnt_hbox_new(FALSE);
		gnt_box_set_pad(GNT_BOX(hbox), 0);
		tree = fc->userlist = gnt_tree_new_with_columns(2);
		gnt_tree_set_col_width(GNT_TREE(tree), 0, 1);   /* The flag column */
		gnt_tree_set_compare_func(GNT_TREE(tree), (GCompareFunc)g_utf8_collate);
		gnt_tree_set_hash_fns(GNT_TREE(tree), g_str_hash, g_str_equal, g_free);
		gnt_tree_set_search_column(GNT_TREE(tree), 1);
		GNT_WIDGET_SET_FLAGS(tree, GNT_WIDGET_NO_BORDER);
		gnt_box_add_widget(GNT_BOX(hbox), ggc->tv);
		gnt_box_add_widget(GNT_BOX(hbox), tree);
		gnt_box_add_widget(GNT_BOX(ggc->window), hbox);
		g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(create_conv_from_userlist), ggc);
		gnt_widget_set_visible(tree, purple_prefs_get_bool(PREF_USERLIST));
	} else {
		ggc->u.im = g_new0(FinchConvIm, 1);
		gnt_box_add_widget(GNT_BOX(ggc->window), ggc->tv);
	}

	ggc->info = gnt_vbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->info);

	ggc->entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->entry);
	gnt_widget_set_name(ggc->entry, "conversation-window-entry");
	gnt_entry_set_history_length(GNT_ENTRY(ggc->entry), -1);
	gnt_entry_set_word_suggest(GNT_ENTRY(ggc->entry), TRUE);
	gnt_entry_set_always_suggest(GNT_ENTRY(ggc->entry), FALSE);

	gnt_text_view_attach_scroll_widget(GNT_TEXT_VIEW(ggc->tv), ggc->entry);
	gnt_text_view_attach_pager_widget(GNT_TEXT_VIEW(ggc->tv), ggc->entry);

	g_signal_connect_after(G_OBJECT(ggc->entry), "activate", G_CALLBACK(entry_key_pressed), ggc);
	g_signal_connect(G_OBJECT(ggc->entry), "completion", G_CALLBACK(completion_cb), NULL);
	g_signal_connect(G_OBJECT(ggc->window), "destroy", G_CALLBACK(closing_window), ggc);

	gnt_widget_set_position(ggc->window, purple_prefs_get_int(PREF_ROOT "/position/x"),
			purple_prefs_get_int(PREF_ROOT "/position/y"));
	gnt_widget_show(ggc->window);

	g_signal_connect(G_OBJECT(ggc->tv), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggc->window), "position_set", G_CALLBACK(save_position_cb), NULL);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		g_signal_connect(G_OBJECT(ggc->entry), "text_changed", G_CALLBACK(send_typing_notification), ggc);

	convnode = get_conversation_blist_node(conv);
	if ((convnode && purple_blist_node_get_bool(convnode, "gnt-mute-sound")) ||
			!finch_sound_is_enabled())
		ggc->flags |= FINCH_CONV_NO_SOUND;

	gg_create_menu(ggc);
	gg_setup_commands(ggc, FALSE);

	purple_signal_connect(purple_cmds_get_handle(), "cmd-added", ggc,
			G_CALLBACK(cmd_added_cb), ggc);
	purple_signal_connect(purple_cmds_get_handle(), "cmd-removed", ggc,
			G_CALLBACK(cmd_removed_cb), ggc);

	purple_signal_connect(purple_plugins_get_handle(), "plugin-load", ggc,
				PURPLE_CALLBACK(plugin_changed_cb), ggc);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload", ggc,
				PURPLE_CALLBACK(plugin_changed_cb), ggc);

	g_free(title);
	gnt_box_give_focus_to_child(GNT_BOX(ggc->window), ggc->entry);
	g_signal_connect(G_OBJECT(ggc->window), "gained-focus", G_CALLBACK(gained_focus_cb), ggc);
}

static void
finch_destroy_conversation(PurpleConversation *conv)
{
	/* do stuff here */
	FinchConv *ggc = FINCH_CONV(conv);
	ggc->list = g_list_remove(ggc->list, conv);
	if (ggc->list && conv == ggc->active_conv) {
		ggc->active_conv = ggc->list->data;
		gg_setup_commands(ggc, TRUE);
	}

	if (ggc->list == NULL) {
		g_free(ggc->u.chat);
		purple_signals_disconnect_by_handle(ggc);
		if (ggc->window)
			gnt_widget_destroy(ggc->window);
		g_free(ggc);
	}
}

static void
finch_write_common(PurpleConversation *conv, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	FinchConv *ggconv = FINCH_CONV(conv);
	char *strip, *newline;
	GntTextFormatFlags fl = 0;
	int pos;

	g_return_if_fail(ggconv != NULL);

	if ((flags & PURPLE_MESSAGE_SYSTEM) && !(flags & PURPLE_MESSAGE_NOTIFY)) {
		flags &= ~(PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV);
	}

	if (ggconv->active_conv != conv) {
		if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV))
			finch_conversation_set_active(conv);
		else
			return;
	}

	pos = gnt_text_view_get_lines_below(GNT_TEXT_VIEW(ggconv->tv));

	gnt_text_view_tag_change(GNT_TEXT_VIEW(ggconv->tv), "typing", NULL, TRUE);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv), "\n", GNT_TEXT_FLAG_NORMAL);

	/* Unnecessary to print the timestamp for delayed message */
	if (purple_prefs_get_bool("/finch/conversations/timestamps")) {
		if (!mtime)
			time(&mtime);
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					purple_utf8_strftime("(%H:%M:%S)", localtime(&mtime)), gnt_color_pair(color_timestamp));
	}

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv), " ", GNT_TEXT_FLAG_NORMAL);

	if (flags & PURPLE_MESSAGE_AUTO_RESP)
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
					_("<AUTO-REPLY> "), GNT_TEXT_FLAG_BOLD);

	if (who && *who && (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)) &&
			!(flags & PURPLE_MESSAGE_NOTIFY))
	{
		char * name = NULL;
		GntTextFormatFlags msgflags = GNT_TEXT_FLAG_NORMAL;
		gboolean me = FALSE;

		if (purple_message_meify((char*)message, -1)) {
			name = g_strdup_printf("*** %s", who);
			if (!(flags & PURPLE_MESSAGE_SEND) &&
					(flags & PURPLE_MESSAGE_NICK))
				msgflags = gnt_color_pair(color_message_highlight);
			else
				msgflags = gnt_color_pair(color_message_action);
			me = TRUE;
		} else {
			name =  g_strdup_printf("%s", who);
			if (flags & PURPLE_MESSAGE_SEND)
				msgflags = gnt_color_pair(color_message_send);
			else if (flags & PURPLE_MESSAGE_NICK)
				msgflags = gnt_color_pair(color_message_highlight);
			else
				msgflags = gnt_color_pair(color_message_receive);
		}
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				name, msgflags);
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv), me ? " " : ": ", GNT_TEXT_FLAG_NORMAL);
		g_free(name);
	} else
		fl = GNT_TEXT_FLAG_DIM;

	if (flags & PURPLE_MESSAGE_ERROR)
		fl |= GNT_TEXT_FLAG_BOLD;

	/* XXX: Remove this workaround when textview can parse messages. */
	newline = purple_strdup_withhtml(message);
	strip = purple_markup_strip_html(newline);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				strip, fl);

	g_free(newline);
	g_free(strip);

	if (PURPLE_IS_IM_CONVERSATION(conv) && purple_im_conversation_get_typing_state(
			PURPLE_IM_CONVERSATION(conv)) == PURPLE_IM_TYPING) {
		strip = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_title(conv));
		gnt_text_view_append_text_with_tag(GNT_TEXT_VIEW(ggconv->tv),
					strip, GNT_TEXT_FLAG_DIM, "typing");
		g_free(strip);
	}

	if (pos <= 1)
		gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 0);

	if (flags & (PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_NICK | PURPLE_MESSAGE_ERROR))
		gnt_widget_set_urgent(ggconv->tv);
	if (flags & PURPLE_MESSAGE_RECV && !gnt_widget_has_focus(ggconv->window)) {
		int count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "unseen-count"));
		g_object_set_data(G_OBJECT(conv), "unseen-count", GINT_TO_POINTER(count + 1));
		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
	}
}

static void
finch_write_chat(PurpleChatConversation *chat, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	purple_conversation_write(PURPLE_CONVERSATION(chat), who, message, flags, mtime);
}

static void
finch_write_im(PurpleIMConversation *im, const char *who, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(im);
	PurpleAccount *account = purple_conversation_get_account(conv);
	if (flags & PURPLE_MESSAGE_SEND)
	{
		who = purple_connection_get_display_name(purple_account_get_connection(account));
		if (!who)
			who = purple_account_get_private_alias(account);
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

static const char *
chat_flag_text(PurpleChatUserFlags flags)
{
	if (flags & PURPLE_CHAT_USER_FOUNDER)
		return "~";
	if (flags & PURPLE_CHAT_USER_OP)
		return "@";
	if (flags & PURPLE_CHAT_USER_HALFOP)
		return "%";
	if (flags & PURPLE_CHAT_USER_VOICE)
		return "+";
	return " ";
}

static void
finch_chat_add_users(PurpleChatConversation *chat, GList *users, gboolean new_arrivals)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	FinchConv *ggc = FINCH_CONV(conv);
	GntEntry *entry = GNT_ENTRY(ggc->entry);

	if (!new_arrivals)
	{
		/* Print the list of users in the room */
		GString *string = g_string_new(NULL);
		GList *iter;
		int count = g_list_length(users);

		g_string_printf(string,
				ngettext("List of %d user:\n", "List of %d users:\n", count), count);
		for (iter = users; iter; iter = iter->next)
		{
			PurpleChatUser *chatuser = iter->data;
			const char *str;

			if ((str = purple_chat_user_get_alias(chatuser)) == NULL)
				str = purple_chat_user_get_name(chatuser);
			g_string_append_printf(string, "[ %s ]", str);
		}

		purple_conversation_write(conv, NULL, string->str,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_string_free(string, TRUE);
	}

	for (; users; users = users->next)
	{
		PurpleChatUser *chatuser = users->data;
		GntTree *tree = GNT_TREE(ggc->u.chat->userlist);
		gnt_entry_add_suggest(entry, purple_chat_user_get_name(chatuser));
		gnt_entry_add_suggest(entry, purple_chat_user_get_alias(chatuser));
		gnt_tree_add_row_after(tree, g_strdup(purple_chat_user_get_name(chatuser)),
				gnt_tree_create_row(tree, chat_flag_text(purple_chat_user_get_flags(chatuser)), purple_chat_user_get_alias(chatuser)), NULL, NULL);
	}
}

static void
finch_chat_rename_user(PurpleChatConversation *chat, const char *old, const char *new_n, const char *new_a)
{
	/* Update the name for string completion */
	FinchConv *ggc = FINCH_CONV(PURPLE_CONVERSATION(chat));
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	GntTree *tree = GNT_TREE(ggc->u.chat->userlist);
	PurpleChatUser *cb = purple_chat_conversation_find_user(chat, new_n);

	gnt_entry_remove_suggest(entry, old);
	gnt_tree_remove(tree, (gpointer)old);

	gnt_entry_add_suggest(entry, new_n);
	gnt_entry_add_suggest(entry, new_a);
	gnt_tree_add_row_after(tree, g_strdup(new_n),
			gnt_tree_create_row(tree, chat_flag_text(purple_chat_user_get_flags(cb)), new_a), NULL, NULL);
}

static void
finch_chat_remove_users(PurpleChatConversation *chat, GList *list)
{
	/* Remove the name from string completion */
	FinchConv *ggc = FINCH_CONV(PURPLE_CONVERSATION(chat));
	GntEntry *entry = GNT_ENTRY(ggc->entry);
	for (; list; list = list->next) {
		GntTree *tree = GNT_TREE(ggc->u.chat->userlist);
		gnt_entry_remove_suggest(entry, list->data);
		gnt_tree_remove(tree, list->data);
	}
}

static void
finch_chat_update_user(PurpleChatUser *cb)
{
	PurpleChatConversation *chat;
	FinchConv *ggc;
	if (!cb)
		return;

	chat = purple_chat_user_get_chat(cb);
	ggc = FINCH_CONV(PURPLE_CONVERSATION(chat));
	gnt_tree_change_text(GNT_TREE(ggc->u.chat->userlist),
			(gpointer)purple_chat_user_get_name(cb), 0,
			chat_flag_text(purple_chat_user_get_flags(cb)));
}

static void
finch_conv_present(PurpleConversation *conv)
{
	FinchConv *fc = FINCH_CONV(conv);
	if (fc && fc->window)
		gnt_window_present(fc->window);
}

static gboolean
finch_conv_has_focus(PurpleConversation *conv)
{
	FinchConv *fc = FINCH_CONV(conv);
	if (fc && fc->window)
		return gnt_widget_has_focus(fc->window);
	return FALSE;
}

static PurpleConversationUiOps conv_ui_ops =
{
	finch_create_conversation,
	finch_destroy_conversation,
	finch_write_chat,
	finch_write_im,
	finch_write_conv,
	finch_chat_add_users,
	finch_chat_rename_user,
	finch_chat_remove_users,
	finch_chat_update_user,
	finch_conv_present, /* present */
	finch_conv_has_focus, /* has_focus */
	NULL, /* custom_smiley_add */
	NULL, /* custom_smiley_write */
	NULL, /* custom_smiley_close */
	NULL, /* send_confirm */
	NULL,
	NULL,
	NULL,
	NULL
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
	purple_conversation_send(conv, args[0]);

	return PURPLE_CMD_RET_OK;
}

/* Xerox */
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

/* Xerox */
static PurpleCmdRet
debug_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("Using Finch v%s with libpurple v%s.",
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
				str = g_string_append(str, purple_plugin_get_name(plugins->data));
				if (plugins->next)
					str = g_string_append(str, ", ");
			}
		} else {
			str = g_string_append(str, "(none)");
		}

		tmp = g_string_free(str, FALSE);
	} else {
		purple_conversation_write(conv, NULL, _("Supported debug options are: plugins version"),
		                        PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_ERROR, time(NULL));
		return PURPLE_CMD_RET_OK;
	}

	markup = g_markup_escape_text(tmp, -1);
	purple_conversation_send(conv, markup);

	g_free(tmp);
	g_free(markup);
	return PURPLE_CMD_RET_OK;
}

/* Xerox */
static PurpleCmdRet
clear_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	purple_conversation_clear_message_history(conv);
	return PURPLE_CMD_RET_OK;
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

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
cmd_show_window(PurpleConversation *conv, const char *cmd, char **args, char **error, gpointer data)
{
	void (*callback)(void) = data;
	callback();
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
cmd_message_color(PurpleConversation *conv, const char *cmd, char **args, char **error, gpointer data)
{
	int *msgclass  = NULL;
	int fg, bg;

	if (strcmp(args[0], "receive") == 0)
		msgclass = &color_message_receive;
	else if (strcmp(args[0], "send") == 0)
		msgclass = &color_message_send;
	else if (strcmp(args[0], "highlight") == 0)
		msgclass = &color_message_highlight;
	else if (strcmp(args[0], "action") == 0)
		msgclass = &color_message_action;
	else if (strcmp(args[0], "timestamp") == 0)
		msgclass = &color_timestamp;
	else {
		if (error)
			*error = g_strdup_printf(_("%s is not a valid message class. See '/help msgcolor' for valid message classes."), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	fg = gnt_colors_get_color(args[1]);
	if (fg == -EINVAL) {
		if (error)
			*error = g_strdup_printf(_("%s is not a valid color. See '/help msgcolor' for valid colors."), args[1]);
		return PURPLE_CMD_RET_FAILED;
	}

	bg = gnt_colors_get_color(args[2]);
	if (bg == -EINVAL) {
		if (error)
			*error = g_strdup_printf(_("%s is not a valid color. See '/help msgcolor' for valid colors."), args[2]);
		return PURPLE_CMD_RET_FAILED;
	}

	init_pair(*msgclass, fg, bg);

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
users_command_cb(PurpleConversation *conv, const char *cmd, char **args, char **error, gpointer data)
{
	FinchConv *fc = FINCH_CONV(conv);
	FinchConvChat *ch;
	if (!fc)
		return PURPLE_CMD_RET_FAILED;

	ch = fc->u.chat;
	gnt_widget_set_visible(ch->userlist,
			(GNT_WIDGET_IS_FLAG_SET(ch->userlist, GNT_WIDGET_INVISIBLE)));
	gnt_box_readjust(GNT_BOX(fc->window));
	gnt_box_give_focus_to_child(GNT_BOX(fc->window), fc->entry);
	purple_prefs_set_bool(PREF_USERLIST, !(GNT_WIDGET_IS_FLAG_SET(ch->userlist, GNT_WIDGET_INVISIBLE)));
	return PURPLE_CMD_RET_OK;
}

void finch_conversation_init()
{
	color_message_send = gnt_style_get_color(NULL, "color-message-sent");
	if (!color_message_send)
		color_message_send = gnt_color_add_pair(COLOR_CYAN, -1);
	color_message_receive = gnt_style_get_color(NULL, "color-message-received");
	if (!color_message_receive)
		color_message_receive = gnt_color_add_pair(COLOR_RED, -1);
	color_message_highlight = gnt_style_get_color(NULL, "color-message-highlight");
	if (!color_message_highlight)
		color_message_highlight = gnt_color_add_pair(COLOR_GREEN, -1);
	color_timestamp = gnt_style_get_color(NULL, "color-timestamp");
	if (!color_timestamp)
		color_timestamp = gnt_color_add_pair(COLOR_BLUE, -1);
	color_message_action = gnt_style_get_color(NULL, "color-message-action");
	if (!color_message_action)
		color_message_action = gnt_color_add_pair(COLOR_YELLOW, -1);
	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_none(PREF_ROOT "/size");
	purple_prefs_add_int(PREF_ROOT "/size/width", 70);
	purple_prefs_add_int(PREF_ROOT "/size/height", 20);
	purple_prefs_add_none(PREF_ROOT "/position");
	purple_prefs_add_int(PREF_ROOT "/position/x", 0);
	purple_prefs_add_int(PREF_ROOT "/position/y", 0);
	purple_prefs_add_none(PREF_CHAT);
	purple_prefs_add_bool(PREF_USERLIST, FALSE);

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
	purple_cmd_register("users", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  users_command_cb, _("users:  Show the list of users in the chat."), NULL);

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

	/* Allow customizing the message colors using a command during run-time */
	purple_cmd_register("msgcolor", "www", PURPLE_CMD_P_DEFAULT,
			PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
			cmd_message_color, _("msgcolor &lt;class&gt; &lt;foreground&gt; &lt;background&gt;: "
				                 "Set the color for different classes of messages in the conversation window.<br>"
				                 "    &lt;class&gt;: receive, send, highlight, action, timestamp<br>"
				                 "    &lt;foreground/background&gt;: black, red, green, blue, white, gray, darkgray, magenta, cyan, default<br><br>"
								 "EXAMPLE:<br>    msgcolor send cyan default"),
			NULL);
	purple_cmd_register("msgcolour", "www", PURPLE_CMD_P_DEFAULT,
			PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
			cmd_message_color, _("msgcolor &lt;class&gt; &lt;foreground&gt; &lt;background&gt;: "
				                 "Set the color for different classes of messages in the conversation window.<br>"
				                 "    &lt;class&gt;: receive, send, highlight, action, timestamp<br>"
				                 "    &lt;foreground/background&gt;: black, red, green, blue, white, gray, darkgray, magenta, cyan, default<br><br>"
								 "EXAMPLE:<br>    msgcolor send cyan default"),
			NULL);

	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing", finch_conv_get_handle(),
					PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing-stopped", finch_conv_get_handle(),
					PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-left", finch_conv_get_handle(),
					PURPLE_CALLBACK(chat_left_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "cleared-message-history", finch_conv_get_handle(),
					PURPLE_CALLBACK(cleared_message_history_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", finch_conv_get_handle(),
					PURPLE_CALLBACK(conv_updated), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on", finch_conv_get_handle(),
					PURPLE_CALLBACK(buddy_signed_on_off), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off", finch_conv_get_handle(),
					PURPLE_CALLBACK(buddy_signed_on_off), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-on", finch_conv_get_handle(),
					PURPLE_CALLBACK(account_signed_on_off), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", finch_conv_get_handle(),
					PURPLE_CALLBACK(account_signed_on_off), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-off", finch_conv_get_handle(),
					PURPLE_CALLBACK(account_signing_off), NULL);
}

void finch_conversation_uninit()
{
	purple_signals_disconnect_by_handle(finch_conv_get_handle());
}

void finch_conversation_set_active(PurpleConversation *conv)
{
	FinchConv *ggconv = FINCH_CONV(conv);
	PurpleAccount *account;
	char *title;

	g_return_if_fail(ggconv);
	g_return_if_fail(g_list_find(ggconv->list, conv));
	if (ggconv->active_conv == conv)
		return;

	ggconv->active_conv = conv;
	gg_setup_commands(ggconv, TRUE);
	account = purple_conversation_get_account(conv);
	title = get_conversation_title(conv, account);
	gnt_screen_rename_widget(ggconv->window, title);
	g_free(title);
}

void finch_conversation_set_info_widget(PurpleConversation *conv, GntWidget *widget)
{
	FinchConv *fc = FINCH_CONV(conv);
	int height, width;

	gnt_box_remove_all(GNT_BOX(fc->info));

	if (widget) {
		gnt_box_add_widget(GNT_BOX(fc->info), widget);
		gnt_box_readjust(GNT_BOX(fc->info));
	}

	gnt_widget_get_size(fc->window, &width, &height);
	gnt_box_readjust(GNT_BOX(fc->window));
	gnt_screen_resize_widget(fc->window, width, height);
	gnt_box_give_focus_to_child(GNT_BOX(fc->window), fc->entry);
}

