/*
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
#include "internal.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "signals.h"
#include "util.h"

/* XXX CORE/UI, waiting for away splittage */
#include "gtkinternal.h"
#include "ui.h"

typedef struct
{
	char *id;
	char *name;
	GaimConvPlacementFunc fnc;

} ConvPlacementData;

#define SEND_TYPED_TIMEOUT 5000

static GaimConvWindowUiOps *win_ui_ops = NULL;

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static GList *windows = NULL;
static GList *conv_placement_fncs = NULL;
static GaimConvPlacementFunc place_conv = NULL;

static void ensure_default_funcs(void);
static void conv_placement_last_created_win(GaimConversation *conv);

static gint
insertname_compare(gconstpointer one, gconstpointer two)
{
	const char *a = (const char *)one;
	const char *b = (const char *)two;

	if (*a == '@') {
		if (*b != '@') return -1;

		return strcasecmp(a + 1, b + 1);

	} else if (*a == '%') {
		if (*b != '%') return -1;

		return strcasecmp(a + 1, b + 1);

	} else if (*a == '+') {
		if (*b == '@') return  1;
		if (*b != '+') return -1;

		return strcasecmp(a + 1, b + 1);

	} else if (*b == '@' || *b == '%' || *b == '+')
		return 1;

	return strcasecmp(a, b);
}

static gboolean
find_nick(GaimConnection *gc, const char *message)
{
	GaimAccount *account;
	char *msg, *who, *p;
	const char *disp;
	int n;

	account = gaim_connection_get_account(gc);

	msg = g_utf8_strdown(message, -1);

	who = g_utf8_strdown(gaim_account_get_username(account), -1);
	n = strlen(who);

	if ((p = strstr(msg, who)) != NULL) {
		if ((p == msg || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
			g_free(who);
			g_free(msg);

			return TRUE;
		}
	}

	g_free(who);

	disp = gaim_connection_get_display_name(gc);


	if(disp)  {
		if (!gaim_utf8_strcasecmp(gaim_account_get_username(account), disp)) {
			g_free(msg);

			return FALSE;
		}

		who = g_utf8_strdown(disp, -1);
		n = who ? strlen(who) : 0;

		if (n > 0 && (p = strstr(msg, who)) != NULL) {
			if ((p == msg || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
				g_free(who);
				g_free(msg);

				return TRUE;
			}
		}

		g_free(who);
	}

	g_free(msg);

	return FALSE;
}

static gboolean
reset_typing(gpointer data)
{
	GaimConversation *c = (GaimConversation *)data;
	GaimConvIm *im;

	if (!g_list_find(conversations, c))
		return FALSE;

	im = GAIM_CONV_IM(c);

	gaim_conv_im_set_typing_state(im, GAIM_NOT_TYPING);
	gaim_conv_im_update_typing(im);
	gaim_conv_im_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed(gpointer data)
{
	GaimConversation *conv = (GaimConversation *)data;
	GaimConnection *gc;
	const char *name;

	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (conv != NULL && gc != NULL && name != NULL) {
		gaim_conv_im_set_type_again(GAIM_CONV_IM(conv), TRUE);

		serv_send_typing(gc, name, GAIM_TYPED);

		gaim_debug(GAIM_DEBUG_MISC, "conversation", "typed...\n");
	}

	return FALSE;
}

static void
common_send(GaimConversation *conv, const char *message)
{
	GaimConversationType type;
	GaimConnection *gc;
	GaimConversationUiOps *ops;
	char *buf, *buf2, *buffy = NULL;
	int plugin_return;
	int limit;
	int err = 0;
	GList *first;

	gc = gaim_conversation_get_gc(conv);

	g_return_if_fail(gc != NULL);

	type = gaim_conversation_get_type(conv);
	ops  = gaim_conversation_get_ui_ops(conv);

	limit = 32 * 1024; /* You shouldn't be sending more than 32K in your
						  messages. That's a book. */

	buf = g_malloc(limit);
	strncpy(buf, message, limit);

	if (strlen(buf) == 0) {
		g_free(buf);

		return;
	}

	first = g_list_first(conv->send_history);

	if (first->data)
		g_free(first->data);

	first->data = g_strdup(buf);

	conv->send_history = g_list_prepend(first, NULL);

	buf2 = g_malloc(limit);

	if ((gc->flags & GAIM_CONNECTION_HTML) &&
		gaim_prefs_get_bool("/core/conversations/send_urls_as_links")) {

		buffy = gaim_markup_linkify(buf);
	}
	else
		buffy = g_strdup(buf);

	plugin_return =
		GPOINTER_TO_INT(gaim_signal_emit_return_1(
			gaim_conversations_get_handle(),
			(type == GAIM_CONV_IM
			 ? "displaying-im-msg" : "displaying-chat-msg"),
			gaim_conversation_get_account(conv), conv, &buffy));

	if (buffy == NULL) {
		g_free(buf2);
		g_free(buf);
		return;
	}

	if (plugin_return) {
		g_free(buffy);
		g_free(buf2);
		g_free(buf);
		return;
	}

	strncpy(buf, buffy, limit);
	g_free(buffy);

	gaim_signal_emit(gaim_conversations_get_handle(),
		(type == GAIM_CONV_IM ? "displayed-im-msg" : "displayed-chat-msg"),
		gaim_conversation_get_account(conv), conv, buffy);

	if (type == GAIM_CONV_IM) {
		GaimConvIm *im = GAIM_CONV_IM(conv);

		buffy = g_strdup(buf);
		gaim_signal_emit(gaim_conversations_get_handle(), "sending-im-msg",
						 gaim_conversation_get_account(conv),
						 gaim_conversation_get_name(conv), &buffy);

		if (buffy != NULL) {
			GaimConvImFlags imflags = 0;
			GaimMessageFlags msgflags = GAIM_MESSAGE_SEND;

			if (im->images != NULL) {
				imflags |= GAIM_CONV_IM_IMAGES;
				msgflags |= GAIM_MESSAGE_IMAGES;
			}

			err = serv_send_im(gc, gaim_conversation_get_name(conv),
							    buffy, imflags);

			if (err > 0)
				gaim_conv_im_write(im, NULL, buf, msgflags, time(NULL));

			if (im->images != NULL) {
				GSList *tempy;
				int image;

				for (tempy = im->images;
					 tempy != NULL;
					 tempy = tempy->next) {

					image = GPOINTER_TO_INT(tempy->data);
					gaim_imgstore_unref(image);
				}

				g_slist_free(im->images);
				im->images = NULL;
			}

			gaim_signal_emit(gaim_conversations_get_handle(), "sent-im-msg",
							 gaim_conversation_get_account(conv),
							 gaim_conversation_get_name(conv), buffy);

			g_free(buffy);
		}
	}
	else {
		buffy = g_strdup(buf);

		gaim_signal_emit(gaim_conversations_get_handle(), "sending-chat-msg",
						 gaim_conversation_get_account(conv), &buffy,
						 gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));

		if (buffy != NULL) {
			err = serv_chat_send(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)), buffy);

			gaim_signal_emit(gaim_conversations_get_handle(), "sent-chat-msg",
							 gaim_conversation_get_account(conv), buf,
							 gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));

			g_free(buffy);
		}
	}

	g_free(buf2);
	g_free(buf);

	if (err < 0) {
		if (err == -E2BIG) {
			gaim_notify_error(NULL, NULL,
							  _("Unable to send message. The message is "
								"too large."), NULL);
		}
		else if (err == -ENOTCONN) {
			gaim_debug(GAIM_DEBUG_ERROR, "conversation",
					   "Not yet connected.\n");
		}
		else {
			gaim_notify_error(NULL, NULL, _("Unable to send message."), NULL);
		}
	}
	else {
		if (err > 0 &&
			gaim_prefs_get_bool("/core/conversations/away_back_on_send")) {

			if (awaymessage != NULL) {
				do_im_back(NULL, NULL);
			}
			else if (gc->away) {
				serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
			}
		}
	}
}

static void
update_conv_indexes(GaimConvWindow *win)
{
	GList *l;
	int i;

	for (l = gaim_conv_window_get_conversations(win), i = 0;
		 l != NULL;
		 l = l->next, i++) {

		GaimConversation *conv = (GaimConversation *)l->data;

		conv->conversation_pos = i;
	}
}

GaimConvWindow *
gaim_conv_window_new(void)
{
	GaimConvWindow *win;

	win = g_new0(GaimConvWindow, 1);

	windows = g_list_append(windows, win);

	win->ui_ops = gaim_conversations_get_win_ui_ops();

	if (win->ui_ops != NULL && win->ui_ops->new_window != NULL)
		win->ui_ops->new_window(win);

	return win;
}

void
gaim_conv_window_destroy(GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;
	GList *node;

	g_return_if_fail(win != NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	/*
	 * If there are any conversations in this, destroy them all. The last
	 * conversation will call gaim_conv_window_destroy(), but this time, this
	 * check will fail and the window will actually be destroyed.
	 *
	 * This is needed because chats may not close right away. They may
	 * wait for notification first. When they get that, the window is
	 * already destroyed, and gaim either crashes or spits out gtk warnings.
	 * The problem is fixed with this check.
	 */
	if (gaim_conv_window_get_conversation_count(win) > 0) {

		node = g_list_first(gaim_conv_window_get_conversations(win));
		while(node != NULL)
		{
			GaimConversation *conv = node->data;

			node = g_list_next(node);

			gaim_conversation_destroy(conv);
		}
	}
	else
	{
		if (ops != NULL && ops->destroy_window != NULL)
			ops->destroy_window(win);

		g_list_free(gaim_conv_window_get_conversations(win));

		windows = g_list_remove(windows, win);

		g_free(win);
	}
}

void
gaim_conv_window_show(GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops == NULL || ops->show == NULL)
		return;

	ops->show(win);
}

void
gaim_conv_window_hide(GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops == NULL || ops->hide == NULL)
		return;

	ops->hide(win);
}

void
gaim_conv_window_raise(GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops == NULL || ops->raise == NULL)
		return;

	ops->raise(win);
}

void
gaim_conv_window_flash(GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops == NULL || ops->flash == NULL)
		return;

	ops->flash(win);
}

void
gaim_conv_window_set_ui_ops(GaimConvWindow *win, GaimConvWindowUiOps *ops)
{
	GaimConversationUiOps *conv_ops = NULL;
	GList *l;

	g_return_if_fail(win != NULL);

	if (win->ui_ops == ops)
		return;

	if (ops != NULL && ops->get_conversation_ui_ops != NULL)
		conv_ops = ops->get_conversation_ui_ops();

	if (win->ui_ops != NULL && win->ui_ops->destroy_window != NULL)
		win->ui_ops->destroy_window(win);

	win->ui_ops = ops;

	if (win->ui_ops != NULL && win->ui_ops->new_window != NULL)
		win->ui_ops->new_window(win);

	for (l = gaim_conv_window_get_conversations(win);
		 l != NULL;
		 l = l->next) {

		GaimConversation *conv = (GaimConversation *)l;

		gaim_conversation_set_ui_ops(conv, conv_ops);

		if (win->ui_ops != NULL && win->ui_ops->add_conversation != NULL)
			win->ui_ops->add_conversation(win, conv);
	}
}

GaimConvWindowUiOps *
gaim_conv_window_get_ui_ops(const GaimConvWindow *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->ui_ops;
}

int
gaim_conv_window_add_conversation(GaimConvWindow *win, GaimConversation *conv)
{
	GaimConvWindowUiOps *ops;

	g_return_val_if_fail(win  != NULL, -1);
	g_return_val_if_fail(conv != NULL, -1);

	if (gaim_conversation_get_window(conv) != NULL) {
		gaim_conv_window_remove_conversation(
			gaim_conversation_get_window(conv),
			gaim_conversation_get_index(conv));
	}

	ops = gaim_conv_window_get_ui_ops(win);

	win->conversations = g_list_append(win->conversations, conv);
	win->conversation_count++;

	conv->conversation_pos = win->conversation_count - 1;

	if (ops != NULL) {
		conv->window = win;

		if (ops->get_conversation_ui_ops != NULL)
			gaim_conversation_set_ui_ops(conv, ops->get_conversation_ui_ops());

		if (ops->add_conversation != NULL)
			ops->add_conversation(win, conv);
	}

	return win->conversation_count - 1;
}

GaimConversation *
gaim_conv_window_remove_conversation(GaimConvWindow *win, unsigned int index)
{
	GaimConvWindowUiOps *ops;
	GaimConversation *conv;
	GList *node;

	g_return_val_if_fail(win != NULL, NULL);
	g_return_val_if_fail(index < gaim_conv_window_get_conversation_count(win), NULL);

	ops = gaim_conv_window_get_ui_ops(win);

	node = g_list_nth(gaim_conv_window_get_conversations(win), index);
	conv = (GaimConversation *)node->data;

	if (ops != NULL && ops->remove_conversation != NULL)
		ops->remove_conversation(win, conv);

	win->conversations = g_list_remove_link(win->conversations, node);

	g_list_free_1(node);

	win->conversation_count--;

	conv->window = NULL;

	if (gaim_conv_window_get_conversation_count(win) == 0)
		gaim_conv_window_destroy(win);
	else {
		/* Change all the indexes. */
		update_conv_indexes(win);
	}

	return conv;
}

void
gaim_conv_window_move_conversation(GaimConvWindow *win, unsigned int index,
							  unsigned int new_index)
{
	GaimConvWindowUiOps *ops;
	GaimConversation *conv;
	GList *l;

	g_return_if_fail(win != NULL);
	g_return_if_fail(index < gaim_conv_window_get_conversation_count(win));
	g_return_if_fail(index != new_index);

	/* We can't move this past the last index. */
	if (new_index > gaim_conv_window_get_conversation_count(win))
		new_index = gaim_conv_window_get_conversation_count(win);

	/* Get the list item for this conversation at its current index. */
	l = g_list_nth(gaim_conv_window_get_conversations(win), index);

	if (l == NULL) {
		/* Should never happen. */
		gaim_debug(GAIM_DEBUG_ERROR, "conversation",
				   "Misordered conversations list in window %p\n", win);

		return;
	}

	conv = (GaimConversation *)l->data;

	/* Update the UI part of this. */
	ops = gaim_conv_window_get_ui_ops(win);

	if (ops != NULL && ops->move_conversation != NULL)
		ops->move_conversation(win, conv, new_index);

	if (new_index > index)
		new_index--;

	/* Remove the old one. */
	win->conversations = g_list_delete_link(win->conversations, l);

	/* Insert it where it should go. */
	win->conversations = g_list_insert(win->conversations, conv, new_index);

	update_conv_indexes(win);
}

GaimConversation *
gaim_conv_window_get_conversation_at(const GaimConvWindow *win, unsigned int index)
{
	g_return_val_if_fail(win != NULL, NULL);
	g_return_val_if_fail(index >= 0 &&
						 index < gaim_conv_window_get_conversation_count(win),
						 NULL);

	return (GaimConversation *)g_list_nth_data(
		gaim_conv_window_get_conversations(win), index);
}

size_t
gaim_conv_window_get_conversation_count(const GaimConvWindow *win)
{
	g_return_val_if_fail(win != NULL, 0);

	return win->conversation_count;
}

void
gaim_conv_window_switch_conversation(GaimConvWindow *win, unsigned int index)
{
	GaimConvWindowUiOps *ops;
	GaimConversation *old_conv, *conv;

	g_return_if_fail(win != NULL);
	g_return_if_fail(index >= 0 &&
					 index < gaim_conv_window_get_conversation_count(win));

	old_conv = gaim_conv_window_get_active_conversation(win);
	conv = gaim_conv_window_get_conversation_at(win, index);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "conversation-switching", old_conv, conv);

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops != NULL && ops->switch_conversation != NULL)
		ops->switch_conversation(win, index);

	gaim_conversation_set_unseen(conv, GAIM_UNSEEN_NONE);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "conversation-switched", old_conv, conv);
}

GaimConversation *
gaim_conv_window_get_active_conversation(const GaimConvWindow *win)
{
	GaimConvWindowUiOps *ops;

	g_return_val_if_fail(win != NULL, NULL);

	if (gaim_conv_window_get_conversation_count(win) == 0)
		return NULL;

	ops = gaim_conv_window_get_ui_ops(win);

	if (ops != NULL && ops->get_active_index != NULL)
		return gaim_conv_window_get_conversation_at(win, ops->get_active_index(win));

	return NULL;
}

GList *
gaim_conv_window_get_conversations(const GaimConvWindow *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->conversations;
}

GList *
gaim_get_windows(void)
{
	return windows;
}

GaimConvWindow *
gaim_get_first_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	GaimConvWindow *win;
	GaimConversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		win = (GaimConvWindow *)wins->data;

		for (convs = gaim_conv_window_get_conversations(win);
			 convs != NULL;
			 convs = convs->next) {

			conv = (GaimConversation *)convs->data;

			if (gaim_conversation_get_type(conv) == type)
				return win;
		}
	}

	return NULL;
}

GaimConvWindow *
gaim_get_last_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	GaimConvWindow *win;
	GaimConversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = g_list_last(gaim_get_windows());
		 wins != NULL;
		 wins = wins->prev) {

		win = (GaimConvWindow *)wins->data;

		for (convs = gaim_conv_window_get_conversations(win);
			 convs != NULL;
			 convs = convs->next) {

			conv = (GaimConversation *)convs->data;

			if (gaim_conversation_get_type(conv) == type)
				return win;
		}
	}

	return NULL;
}

/**************************************************************************
 * Conversation API
 **************************************************************************/
GaimConversation *
gaim_conversation_new(GaimConversationType type, GaimAccount *account,
					  const char *name)
{
	GaimConversation *conv;

	g_return_val_if_fail(type    != GAIM_CONV_UNKNOWN, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name    != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((conv = gaim_find_conversation_with_account(name, account)) != NULL)
		return conv;

	conv = g_new0(GaimConversation, 1);

	conv->type         = type;
	conv->account      = account;
	conv->name         = g_strdup(name);
	conv->title        = g_strdup(name);
	conv->send_history = g_list_append(NULL, NULL);
	conv->history      = g_string_new("");
	conv->data         = g_hash_table_new_full(g_str_hash, g_str_equal,
											   g_free, NULL);
	conv->log          = gaim_log_new(type == GAIM_CONV_IM ? GAIM_LOG_IM :
					  type == GAIM_CONV_CHAT ? GAIM_LOG_CHAT : GAIM_LOG_IM, name, account, time(NULL));


	if (type == GAIM_CONV_IM)
	{
		conv->u.im = g_new0(GaimConvIm, 1);
		conv->u.im->conv = conv;

		ims = g_list_append(ims, conv);

		gaim_conversation_set_logging(conv,
				gaim_prefs_get_bool("/core/logging/log_ims"));
	}
	else if (type == GAIM_CONV_CHAT)
	{
		conv->u.chat = g_new0(GaimConvChat, 1);
		conv->u.chat->conv = conv;

		chats = g_list_append(chats, conv);

		gaim_conversation_set_logging(conv,
				gaim_prefs_get_bool("/core/logging/log_chats"));
	}

	conversations = g_list_append(conversations, conv);

	/* Auto-set the title. */
	gaim_conversation_autoset_title(conv);

	/*
	 * Create a window if one does not exist. If it does, use the last
	 * created window.
	 */
	if (windows == NULL) {
		GaimConvWindow *win;

		win = gaim_conv_window_new();
		gaim_conv_window_add_conversation(win, conv);

		/* Ensure the window is visible. */
		gaim_conv_window_show(win);
	}
	else {
		if (place_conv == NULL)
		{
			ensure_default_funcs();

			place_conv = conv_placement_last_created_win;
		}

		if (place_conv == NULL)
			gaim_debug(GAIM_DEBUG_ERROR, "conversation",
					   "This is about to suck.\n");

		place_conv(conv);
	}

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "conversation-created", conv);

	return conv;
}

void
gaim_conversation_destroy(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConvWindow *win;
	GaimConversationUiOps *ops;
	GaimConnection *gc;
	const char *name;
	GList *node;

	g_return_if_fail(conv != NULL);

	win  = gaim_conversation_get_window(conv);
	ops  = gaim_conversation_get_ui_ops(conv);
	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (gc != NULL) {
		/* Still connected */
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
			if (gaim_prefs_get_bool("/core/conversations/im/send_typing"))
				serv_send_typing(gc, name, GAIM_NOT_TYPING);

			if (gc && prpl_info->convo_closed != NULL)
				prpl_info->convo_closed(gc, name);
		}
		else if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) {
			/*
			 * This is unfortunately necessary, because calling
			 * serv_chat_leave() calls this gaim_conversation_destroy(),
			 * which leads to two calls here.. We can't just return after
			 * this, because then it'll return on the next pass. So, since
			 * serv_got_chat_left(), which is eventually called from the
			 * prpl that serv_chat_leave() calls, removes this conversation
			 * from the gc's buddy_chats list, we're going to check to see
			 * if this exists in the list. If so, we want to return after
			 * calling this, because it'll be called again. If not, fall
			 * through, because it'll have already been removed, and we'd
			 * be on the 2nd pass.
			 *
			 * Long paragraph. <-- Short sentence.
			 *
			 *   -- ChipX86
			 */

			if (gc && g_slist_find(gc->buddy_chats, conv) != NULL) {
				serv_chat_leave(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));

				return;
			}
		}
	}

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "deleting-conversation", conv);

	if (conv->name  != NULL) g_free(conv->name);
	if (conv->title != NULL) g_free(conv->title);

	for (node = g_list_first(conv->send_history);
		 node != NULL;
		 node = g_list_next(node)) {

		if (node->data != NULL)
			g_free(node->data);
	}

	g_list_free(g_list_first(conv->send_history));

	if (conv->history != NULL)
		g_string_free(conv->history, TRUE);

	conversations = g_list_remove(conversations, conv);

	if (conv->type == GAIM_CONV_IM) {
		GSList *tempy;
		int image;

		gaim_conv_im_stop_typing_timeout(conv->u.im);
		gaim_conv_im_stop_type_again_timeout(conv->u.im);

		for (tempy = conv->u.im->images;
			 tempy != NULL;
			 tempy = tempy->next) {

			image = GPOINTER_TO_INT(tempy->data);
			gaim_imgstore_unref(image);
		}

		g_slist_free(conv->u.im->images);

		if (conv->u.im->icon != NULL)
			gaim_buddy_icon_unref(conv->u.im->icon);

		g_free(conv->u.im);

		ims = g_list_remove(ims, conv);
	}
	else if (conv->type == GAIM_CONV_CHAT) {

		for (node = conv->u.chat->in_room; node != NULL; node = node->next) {
			if (node->data != NULL)
				g_free(node->data);
		}

		for (node = conv->u.chat->ignored; node != NULL; node = node->next) {
			if (node->data != NULL)
				g_free(node->data);
		}

		g_list_free(conv->u.chat->in_room);
		g_list_free(conv->u.chat->ignored);

		if (conv->u.chat->who != NULL)
			g_free(conv->u.chat->who);

		if (conv->u.chat->topic != NULL)
			g_free(conv->u.chat->topic);

		g_free(conv->u.chat);

		chats = g_list_remove(chats, conv);
	}

	g_hash_table_destroy(conv->data);

	if (win != NULL) {
		gaim_conv_window_remove_conversation(win,
			gaim_conversation_get_index(conv));
	}

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

	gaim_log_free(conv->log);
	g_free(conv);
}

GaimConversationType
gaim_conversation_get_type(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, GAIM_CONV_UNKNOWN);

	return conv->type;
}

void
gaim_conversation_set_ui_ops(GaimConversation *conv,
							 GaimConversationUiOps *ops)
{
	g_return_if_fail(conv != NULL);

	if (conv->ui_ops == ops)
		return;

	if (conv->ui_ops != NULL && conv->ui_ops->destroy_conversation != NULL)
		conv->ui_ops->destroy_conversation(conv);

	conv->ui_data = NULL;

	conv->ui_ops = ops;
}

GaimConversationUiOps *
gaim_conversation_get_ui_ops(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->ui_ops;
}

void
gaim_conversation_set_account(GaimConversation *conv, GaimAccount *account)
{
	g_return_if_fail(conv != NULL);

	if (account == gaim_conversation_get_account(conv))
		return;

	conv->account = account;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_ACCOUNT);
}

GaimAccount *
gaim_conversation_get_account(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->account;
}

GaimConnection *
gaim_conversation_get_gc(const GaimConversation *conv)
{
	GaimAccount *account;

	g_return_val_if_fail(conv != NULL, NULL);

	account = gaim_conversation_get_account(conv);

	if (account == NULL)
		return NULL;

	return account->gc;
}

void
gaim_conversation_set_title(GaimConversation *conv, const char *title)
{
	GaimConversationUiOps *ops;

	g_return_if_fail(conv  != NULL);
	g_return_if_fail(title != NULL);

	if (conv->title != NULL)
		g_free(conv->title);

	conv->title = g_strdup(title);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->set_title != NULL)
		ops->set_title(conv, conv->title);
}

const char *
gaim_conversation_get_title(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->title;
}

void
gaim_conversation_autoset_title(GaimConversation *conv)
{
	GaimAccount *account;
	GaimBuddy *b;
	const char *text, *name;

	g_return_if_fail(conv != NULL);

	account = gaim_conversation_get_account(conv);
	name = gaim_conversation_get_name(conv);

	if (gaim_prefs_get_bool("/core/conversations/use_alias_for_title") &&
		account != NULL && ((b = gaim_find_buddy(account, name)) != NULL)) {

		text = gaim_get_buddy_alias(b);
	}
	else
		text = name;

	gaim_conversation_set_title(conv, text);
}

int
gaim_conversation_get_index(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, 0);

	return conv->conversation_pos;
}

void
gaim_conversation_set_unseen(GaimConversation *conv, GaimUnseenState state)
{
	g_return_if_fail(conv != NULL);

	conv->unseen = state;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_UNSEEN);
}

void
gaim_conversation_foreach(void (*func)(GaimConversation *conv))
{
	GaimConversation *conv;
	GList *l;

	g_return_if_fail(func != NULL);

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		func(conv);
	}
}

GaimUnseenState
gaim_conversation_get_unseen(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, 0);

	return conv->unseen;
}

void
gaim_conversation_set_name(GaimConversation *conv, const char *name)
{
	g_return_if_fail(conv != NULL);

	if (conv->name != NULL)
		g_free(conv->name);

	conv->name = (name == NULL ? NULL : g_strdup(name));

	gaim_conversation_autoset_title(conv);
}

const char *
gaim_conversation_get_name(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->name;
}

void
gaim_conversation_set_logging(GaimConversation *conv, gboolean log)
{
	g_return_if_fail(conv != NULL);

	conv->logging = log;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_LOGGING);
}

gboolean
gaim_conversation_is_logging(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, FALSE);

	return conv->logging;
}

GList *
gaim_conversation_get_send_history(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->send_history;
}

void
gaim_conversation_set_history(GaimConversation *conv, GString *history)
{
	g_return_if_fail(conv != NULL);

	conv->history = history;
}

GString *
gaim_conversation_get_history(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->history;
}

GaimConvWindow *
gaim_conversation_get_window(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->window;
}

GaimConvIm *
gaim_conversation_get_im_data(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	if (gaim_conversation_get_type(conv) != GAIM_CONV_IM)
		return NULL;

	return conv->u.im;
}

GaimConvChat *
gaim_conversation_get_chat_data(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	if (gaim_conversation_get_type(conv) != GAIM_CONV_CHAT)
		return NULL;

	return conv->u.chat;
}

void
gaim_conversation_set_data(GaimConversation *conv, const char *key,
						   gpointer data)
{
	g_return_if_fail(conv != NULL);
	g_return_if_fail(key  != NULL);

	g_hash_table_replace(conv->data, g_strdup(key), data);
}

gpointer
gaim_conversation_get_data(GaimConversation *conv, const char *key)
{
	g_return_val_if_fail(conv != NULL, NULL);
	g_return_val_if_fail(key  != NULL, NULL);

	return g_hash_table_lookup(conv->data, key);
}

GList *
gaim_get_conversations(void)
{
	return conversations;
}

GList *
gaim_get_ims(void)
{
	return ims;
}

GList *
gaim_get_chats(void)
{
	return chats;
}


/* This is deprecated, right? */
GaimConversation *
gaim_find_conversation(const char *name)
{
	GaimConversation *c = NULL;
	char *cuser;
	GList *cnv;

	g_return_val_if_fail(name != NULL, NULL);

	cuser = g_strdup(gaim_normalize(NULL, name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (GaimConversation *)cnv->data;

		if (!gaim_utf8_strcasecmp(cuser, gaim_normalize(NULL, gaim_conversation_get_name(c))))
			break;

		c = NULL;
	}

	g_free(cuser);

	return c;
}

GaimConversation *
gaim_find_conversation_with_account(const char *name,
									const GaimAccount *account)
{
	GaimConversation *c = NULL;
	char *cuser;
	GList *cnv;

	g_return_val_if_fail(name != NULL, NULL);

	cuser = g_strdup(gaim_normalize(account, name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (GaimConversation *)cnv->data;

		if (!gaim_utf8_strcasecmp(cuser,
								  gaim_normalize(account, gaim_conversation_get_name(c))) &&
			account == gaim_conversation_get_account(c)) {

			break;
		}

		c = NULL;
	}

	g_free(cuser);

	return c;
}

void
gaim_conversation_write(GaimConversation *conv, const char *who,
						const char *message, GaimMessageFlags flags,
						time_t mtime)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc = NULL;
	GaimAccount *account;
	GaimConversationUiOps *ops;
	GaimConvWindow *win;
	GaimBuddy *b;
	GaimUnseenState unseen;
	/* int logging_font_options = 0; */

	g_return_if_fail(conv    != NULL);
	g_return_if_fail(message != NULL);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops == NULL || ops->write_conv == NULL)
		return;

	account = gaim_conversation_get_account(conv);

	if (account != NULL)
		gc = gaim_account_get_connection(account);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		(gc == NULL || !g_slist_find(gc->buddy_chats, conv)))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		!g_list_find(gaim_get_conversations(), conv))
		return;

	if (gc != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (gaim_conversation_get_type(conv) == GAIM_CONV_IM ||
			!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (who == NULL) {
				if (flags & GAIM_MESSAGE_SEND) {
					b = gaim_find_buddy(account,
							    gaim_account_get_username(account));

					if (b != NULL && strcmp(b->name, gaim_get_buddy_alias(b)))
						who = gaim_get_buddy_alias(b);
					else if (gaim_account_get_alias(account) != NULL)
						who = account->alias;
					else if (gaim_connection_get_display_name(gc) != NULL)
						who = gaim_connection_get_display_name(gc);
					else
						who = gaim_account_get_username(account);
				}
				else {
					b = gaim_find_buddy(account,
							    gaim_conversation_get_name(conv));

					if (b != NULL)
						who = gaim_get_buddy_alias(b);
					else
						who = gaim_conversation_get_name(conv);
				}
			}
			else {
				b = gaim_find_buddy(account, who);

				if (b != NULL)
					who = gaim_get_buddy_alias(b);
			}
		}
	}
	
	if (gaim_conversation_is_logging(conv))
		gaim_log_write(conv->log, flags, who, mtime, message);
	ops->write_conv(conv, who, message, flags, mtime);

	win = gaim_conversation_get_window(conv);

	/* Tab highlighting */
	if (!(flags & GAIM_MESSAGE_RECV) && !(flags & GAIM_MESSAGE_SYSTEM))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if ((flags & GAIM_MESSAGE_RECV) == GAIM_MESSAGE_RECV)
			gaim_conv_im_set_typing_state(GAIM_CONV_IM(conv), GAIM_NOT_TYPING);
	}

	if (gaim_conv_window_get_active_conversation(win) != conv) {
		if ((flags & GAIM_MESSAGE_NICK) == GAIM_MESSAGE_NICK ||
				gaim_conversation_get_unseen(conv) == GAIM_UNSEEN_NICK)
			unseen = GAIM_UNSEEN_NICK;
		else if ((flags & GAIM_MESSAGE_SYSTEM) == GAIM_MESSAGE_SYSTEM &&
				 gaim_conversation_get_unseen(conv) != GAIM_UNSEEN_TEXT)
			unseen = GAIM_UNSEEN_EVENT;
		else
			unseen = GAIM_UNSEEN_TEXT;
	}
	else
		unseen = GAIM_UNSEEN_NONE;

	gaim_conversation_set_unseen(conv, unseen);
}

void
gaim_conversation_update_progress(GaimConversation *conv, float percent)
{
	GaimConversationUiOps *ops;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(percent >= 0 && percent <= 1);

	/*
	 * NOTE: A percent == 1 indicates that the progress bar should be
	 *       closed.
	 */
	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->update_progress != NULL)
		ops->update_progress(conv, percent);
}

void
gaim_conversation_update(GaimConversation *conv, GaimConvUpdateType type)
{
	GaimConversationUiOps *ops;

	g_return_if_fail(conv != NULL);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->updated != NULL)
		ops->updated(conv, type);
}

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
GaimConversation *
gaim_conv_im_get_conversation(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->conv;
}

void
gaim_conv_im_set_icon(GaimConvIm *im, GaimBuddyIcon *icon)
{
	g_return_if_fail(im != NULL);

	if (im->icon == icon)
		return;

	if (im->icon != NULL)
		gaim_buddy_icon_unref(im->icon);

	im->icon = (icon == NULL ? NULL : gaim_buddy_icon_ref(icon));

	gaim_conversation_update(gaim_conv_im_get_conversation(im),
							 GAIM_CONV_UPDATE_ICON);
}

GaimBuddyIcon *
gaim_conv_im_get_icon(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->icon;
}

void
gaim_conv_im_set_typing_state(GaimConvIm *im, int state)
{
	g_return_if_fail(im != NULL);

	im->typing_state = state;
}

int
gaim_conv_im_get_typing_state(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_state;
}

void
gaim_conv_im_start_typing_timeout(GaimConvIm *im, int timeout)
{
	GaimConversation *conv;
	const char *name;

	g_return_if_fail(im != NULL);

	if (im->typing_timeout > 0)
		gaim_conv_im_stop_typing_timeout(im);

	conv = gaim_conv_im_get_conversation(im);
	name = gaim_conversation_get_name(conv);

	im->typing_timeout = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
		timeout * 1000, reset_typing, conv, NULL);
}

void
gaim_conv_im_stop_typing_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->typing_timeout == 0)
		return;

	g_source_remove(im->typing_timeout);
	im->typing_timeout = 0;
}

guint
gaim_conv_im_get_typing_timeout(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_timeout;
}

void
gaim_conv_im_set_type_again(GaimConvIm *im, time_t val)
{
	g_return_if_fail(im != NULL);

	im->type_again = val;
}

time_t
gaim_conv_im_get_type_again(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again;
}

void
gaim_conv_im_start_type_again_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	im->type_again_timeout = g_timeout_add(SEND_TYPED_TIMEOUT, send_typed,
										   gaim_conv_im_get_conversation(im));
}

void
gaim_conv_im_stop_type_again_timeout(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->type_again_timeout == 0)
		return;

	g_source_remove(im->type_again_timeout);
	im->type_again_timeout = 0;
}

guint
gaim_conv_im_get_type_again_timeout(const GaimConvIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again_timeout;
}

void
gaim_conv_im_update_typing(GaimConvIm *im)
{
	g_return_if_fail(im != NULL);

	gaim_conversation_update(gaim_conv_im_get_conversation(im),
							 GAIM_CONV_UPDATE_TYPING);
}

void
gaim_conv_im_write(GaimConvIm *im, const char *who, const char *message,
			  GaimMessageFlags flags, time_t mtime)
{
	GaimConversation *c;

	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	c = gaim_conv_im_get_conversation(im);

	/* Raise the window, if specified in prefs. */
	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, flags, mtime);
	else
		gaim_conversation_write(c, who, message, flags, mtime);
}

void
gaim_conv_im_send(GaimConvIm *im, const char *message)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_conv_im_get_conversation(im), message);
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/

GaimConversation *
gaim_conv_chat_get_conversation(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->conv;
}

GList *
gaim_conv_chat_set_users(GaimConvChat *chat, GList *users)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->in_room = users;

	return users;
}

GList *
gaim_conv_chat_get_users(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->in_room;
}

void
gaim_conv_chat_ignore(GaimConvChat *chat, const char *name)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user isn't already ignored. */
	if (gaim_conv_chat_is_user_ignored(chat, name))
		return;

	gaim_conv_chat_set_ignored(chat,
		g_list_append(gaim_conv_chat_get_ignored(chat), g_strdup(name)));
}

void
gaim_conv_chat_unignore(GaimConvChat *chat, const char *name)
{
	GList *item;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user is actually ignored. */
	if (!gaim_conv_chat_is_user_ignored(chat, name))
		return;

	item = g_list_find(gaim_conv_chat_get_ignored(chat),
					   gaim_conv_chat_get_ignored_user(chat, name));

	gaim_conv_chat_set_ignored(chat,
		g_list_remove_link(gaim_conv_chat_get_ignored(chat), item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
gaim_conv_chat_set_ignored(GaimConvChat *chat, GList *ignored)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->ignored = ignored;

	return ignored;
}

GList *
gaim_conv_chat_get_ignored(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->ignored;
}

const char *
gaim_conv_chat_get_ignored_user(const GaimConvChat *chat, const char *user)
{
	GList *ignored;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(user != NULL, NULL);

	for (ignored = gaim_conv_chat_get_ignored(chat);
		 ignored != NULL;
		 ignored = ignored->next) {

		const char *ign = (const char *)ignored->data;

		if (!gaim_utf8_strcasecmp(user, ign) ||
			((*ign == '+' || *ign == '%') && !gaim_utf8_strcasecmp(user, ign + 1)))
			return ign;

		if (*ign == '@') {
			ign++;

			if ((*ign == '+' && !gaim_utf8_strcasecmp(user, ign + 1)) ||
				(*ign != '+' && !gaim_utf8_strcasecmp(user, ign)))
				return ign;
		}
	}

	return NULL;
}

gboolean
gaim_conv_chat_is_user_ignored(const GaimConvChat *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (gaim_conv_chat_get_ignored_user(chat, user) != NULL);
}

void
gaim_conv_chat_set_topic(GaimConvChat *chat, const char *who, const char *topic)
{
	g_return_if_fail(chat != NULL);

	if (chat->who   != NULL) g_free(chat->who);
	if (chat->topic != NULL) g_free(chat->topic);

	chat->who   = (who   == NULL ? NULL : g_strdup(who));
	chat->topic = (topic == NULL ? NULL : g_strdup(topic));

	gaim_conversation_update(gaim_conv_chat_get_conversation(chat),
							 GAIM_CONV_UPDATE_TOPIC);
}

const char *
gaim_conv_chat_get_topic(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->topic;
}

void
gaim_conv_chat_set_id(GaimConvChat *chat, int id)
{
	g_return_if_fail(chat != NULL);

	chat->id = id;
}

int
gaim_conv_chat_get_id(const GaimConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, -1);

	return chat->id;
}

void
gaim_conv_chat_write(GaimConvChat *chat, const char *who, const char *message,
				GaimMessageFlags flags, time_t mtime)
{
	GaimAccount *account;
	GaimConversation *conv;
	GaimConnection *gc;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	conv    = gaim_conv_chat_get_conversation(chat);
	gc      = gaim_conversation_get_gc(conv);
	account = gaim_connection_get_account(gc);

	/* Don't display this if the person who wrote it is ignored. */
	if (gaim_conv_chat_is_user_ignored(chat, who))
		return;

	if (!(flags & GAIM_MESSAGE_WHISPER)) {
		char *str;
		const char *disp;

		str = g_strdup(gaim_normalize(account, who));
		disp = gaim_connection_get_display_name(gc);

		if (!gaim_utf8_strcasecmp(str, gaim_normalize(account, gaim_account_get_username(account))) ||
			(disp && !gaim_utf8_strcasecmp(str, gaim_normalize(account, disp)))) {

			flags |= GAIM_MESSAGE_SEND;
		}
		else {
			flags |= GAIM_MESSAGE_RECV;

			if (find_nick(gc, message))
				flags |= GAIM_MESSAGE_NICK;
		}

		g_free(str);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (conv->ui_ops != NULL && conv->ui_ops->write_chat != NULL)
		conv->ui_ops->write_chat(conv, who, message, flags, mtime);
	else
		gaim_conversation_write(conv, who, message, flags, mtime);
}

void
gaim_conv_chat_send(GaimConvChat *chat, const char *message)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_conv_chat_get_conversation(chat), message);
}

void
gaim_conv_chat_add_user(GaimConvChat *chat, const char *user, const char *extra_msg)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "chat-buddy-joining", conv, user);

	gaim_conv_chat_set_users(chat,
		g_list_insert_sorted(gaim_conv_chat_get_users(chat), g_strdup(user),
							 insertname_compare));

	if (ops != NULL && ops->chat_add_user != NULL)
		ops->chat_add_user(conv, user);

	if (gaim_prefs_get_bool("/core/conversations/chat/show_join")) {
		if (extra_msg == NULL)
			g_snprintf(tmp, sizeof(tmp), _("%s entered the room."), user);
		else
			g_snprintf(tmp, sizeof(tmp),
					   _("%s [<I>%s</I>] entered the room."),
					   user, extra_msg);

		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	}

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "chat-buddy-joined", conv, user);
}

void
gaim_conv_chat_add_users(GaimConvChat *chat, GList *users)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GList *l;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;

		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-joining", conv, user);

		gaim_conv_chat_set_users(chat,
				g_list_insert_sorted(gaim_conv_chat_get_users(chat),
									 g_strdup((char *)l->data),
									 insertname_compare));

		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-joined", conv, user);
	}

	if (ops != NULL && ops->chat_add_users != NULL)
		ops->chat_add_users(conv, users);
}

void
gaim_conv_chat_rename_user(GaimConvChat *chat, const char *old_user,
					  const char *new_user)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];
	GList *names;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_conv_chat_set_users(chat,
		g_list_insert_sorted(gaim_conv_chat_get_users(chat), g_strdup(new_user),
							 insertname_compare));

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user);

	for (names = gaim_conv_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		if (!gaim_utf8_strcasecmp((char *)names->data, old_user)) {
			gaim_conv_chat_set_users(chat,
					g_list_remove(gaim_conv_chat_get_users(chat), names->data));
			break;
		}
	}

	if (gaim_conv_chat_is_user_ignored(chat, old_user)) {
		gaim_conv_chat_unignore(chat, old_user);
		gaim_conv_chat_ignore(chat, new_user);
	}
	else if (gaim_conv_chat_is_user_ignored(chat, new_user))
		gaim_conv_chat_unignore(chat, new_user);

	if (gaim_prefs_get_bool("/core/conversations/chat/show_nick_change")) {
		g_snprintf(tmp, sizeof(tmp),
				   _("%s is now known as %s"), old_user, new_user);

		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	}
}

void
gaim_conv_chat_remove_user(GaimConvChat *chat, const char *user, const char *reason)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];
	GList *names;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-buddy-leaving",
					 conv, user, reason);

	if (ops != NULL && ops->chat_remove_user != NULL)
		ops->chat_remove_user(conv, user);

	for (names = gaim_conv_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		if (!gaim_utf8_strcasecmp((char *)names->data, user)) {
			gaim_conv_chat_set_users(chat,
					g_list_remove(gaim_conv_chat_get_users(chat), names->data));
			break;
		}
	}

	/* NOTE: Don't remove them from ignored in case they re-enter. */

	if (gaim_prefs_get_bool("/core/conversations/chat/show_leave")) {
		if (reason != NULL && *reason != '\0')
			g_snprintf(tmp, sizeof(tmp),
					   _("%s left the room (%s)."), user, reason);
		else
			g_snprintf(tmp, sizeof(tmp), _("%s left the room."), user);

		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	}

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-buddy-left",
					 conv, user, reason);
}

void
gaim_conv_chat_remove_users(GaimConvChat *chat, GList *users, const char *reason)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];
	GList *names, *l;

	g_return_if_fail(chat  != NULL);
	g_return_if_fail(users != NULL);

	conv = gaim_conv_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;

		gaim_signal_emit(gaim_conversations_get_handle(), "chat-buddy-leaving",
						 conv, user, reason);
	}

	if (ops != NULL && ops->chat_remove_users != NULL)
		ops->chat_remove_users(conv, users);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;

		for (names = gaim_conv_chat_get_users(chat);
			 names != NULL;
			 names = names->next) {

			if (!gaim_utf8_strcasecmp((char *)names->data, user))
			{
				gaim_conv_chat_set_users(chat,
					g_list_remove(gaim_conv_chat_get_users(chat), names->data));

				break;
			}
		}

		gaim_signal_emit(gaim_conversations_get_handle(), "chat-buddy-left",
						 conv, user, reason);
	}

	/* NOTE: Don't remove them from ignored in case they re-enter. */

	if (gaim_prefs_get_bool("/core/conversations/chat/show_leave")) {
		if (reason != NULL && *reason != '\0') {
			int i;
			int size = g_list_length(users);
			int max = MIN(10, size);
			GList *l;

			*tmp = '\0';

			for (l = users, i = 0; i < max; i++, l = l->next) {
				g_strlcat(tmp, (char *)l->data, sizeof(tmp));

				if (i < max - 1)
					g_strlcat(tmp, ", ", sizeof(tmp));
			}

			if (size > 10)
				g_snprintf(tmp, sizeof(tmp),
						   _("(+%d more)"), size - 10);

			g_snprintf(tmp, sizeof(tmp), _(" left the room (%s)."), reason);

			gaim_conversation_write(conv, NULL, tmp,
									GAIM_MESSAGE_SYSTEM, time(NULL));
		}
	}
}

void
gaim_conv_chat_clear_users(GaimConvChat *chat)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	GList *users;
	GList *l, *l_next;

	g_return_if_fail(chat != NULL);

	conv  = gaim_conv_chat_get_conversation(chat);
	ops   = gaim_conversation_get_ui_ops(conv);
	users = gaim_conv_chat_get_users(chat);

	if (ops != NULL && ops->chat_remove_users != NULL)
		ops->chat_remove_users(conv, users);

	for (l = users; l != NULL; l = l_next)
	{
		char *user = (char *)l->data;

		l_next = l->next;

		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-leaving", conv, user, NULL);
		gaim_signal_emit(gaim_conversations_get_handle(),
						 "chat-buddy-left", conv, user, NULL);

		g_free(user);
	}

	g_list_free(users);
	gaim_conv_chat_set_users(chat, NULL);
}

GaimConversation *
gaim_find_chat(const GaimConnection *gc, int id)
{
	GList *l;
	GaimConversation *conv;

	for (l = gaim_get_chats(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)) == id &&
			gaim_conversation_get_gc(conv) == gc)
			return conv;
	}

	return NULL;
}

/**************************************************************************
 * Conversation placement functions
 **************************************************************************/
/* This one places conversations in the last made window. */
static void
conv_placement_last_created_win(GaimConversation *conv)
{
	GaimConvWindow *win;

	if (gaim_prefs_get_bool("/core/conversations/combine_chat_im"))
		win = g_list_last(gaim_get_windows())->data;
	else
		win = gaim_get_last_window_with_type(gaim_conversation_get_type(conv));

	if (win == NULL) {
		win = gaim_conv_window_new();

		gaim_conv_window_add_conversation(win, conv);
		gaim_conv_window_show(win);
	}
	else
		gaim_conv_window_add_conversation(win, conv);
}

/* This one places each conversation in its own window. */
static void
conv_placement_new_window(GaimConversation *conv)
{
	GaimConvWindow *win;

	win = gaim_conv_window_new();

	gaim_conv_window_add_conversation(win, conv);

	gaim_conv_window_show(win);
}

/*
 * This groups things by, well, group. Buddies from groups will always be
 * grouped together, and a buddy from a group not belonging to any currently
 * open windows will get a new window.
 */
static void
conv_placement_by_group(GaimConversation *conv)
{
	GaimConvWindow *win;
	GaimConversationType type;

	type = gaim_conversation_get_type(conv);

	if (type == GAIM_CONV_IM) {
		GaimBuddy *b;
		GaimGroup *grp = NULL;
		GList *wins, *convs;

		b = gaim_find_buddy(gaim_conversation_get_account(conv),
					   gaim_conversation_get_name(conv));

		if (b != NULL)
			grp = gaim_find_buddys_group(b);

		/* Go through the list of IMs and find one with this group. */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			GaimConvWindow *win2;
			GaimConversation *conv2;
			GaimBuddy *b2;
			GaimGroup *g2 = NULL;

			win2 = (GaimConvWindow *)wins->data;

			for (convs = gaim_conv_window_get_conversations(win2);
				 convs != NULL;
				 convs = convs->next) {

				conv2 = (GaimConversation *)convs->data;

				b2 = gaim_find_buddy(gaim_conversation_get_account(conv2),
								gaim_conversation_get_name(conv2));

				if (b2 != NULL)
					g2 = gaim_find_buddys_group(b2);

				if (grp == g2) {
					gaim_conv_window_add_conversation(win2, conv);

					return;
				}
			}
		}

		/* Make a new window. */
		conv_placement_new_window(conv);
	}
	else if (type == GAIM_CONV_CHAT) {
		GaimChat *chat;
		GaimGroup *group = NULL;
		GList *wins, *convs;

		chat = gaim_blist_find_chat(gaim_conversation_get_account(conv),
									gaim_conversation_get_name(conv));

		if (chat != NULL)
			group = gaim_chat_get_group(chat);

		/* Go through the list of chats and find one with this group. */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			GaimConvWindow *win2;
			GaimConversation *conv2;
			GaimChat *chat2;
			GaimGroup *group2 = NULL;

			win2 = (GaimConvWindow *)wins->data;

			for (convs = gaim_conv_window_get_conversations(win2);
				 convs != NULL;
				 convs = convs->next) {

				conv2 = (GaimConversation *)convs->data;

				chat2 = gaim_blist_find_chat(
					gaim_conversation_get_account(conv2),
					gaim_conversation_get_name(conv2));

				if (chat2 != NULL)
					group2 = gaim_chat_get_group(chat2);

				if (group == group2) {
					gaim_conv_window_add_conversation(win2, conv);

					return;
				}
			}
		}

		/* Make a new window. */
		conv_placement_new_window(conv);
	}
	else {
		win = gaim_get_last_window_with_type(type);

		if (win == NULL)
			conv_placement_new_window(conv);
		else
			gaim_conv_window_add_conversation(win, conv);
	}
}

/* This groups things by account.  Otherwise, the same semantics as above */
static void
conv_placement_by_account(GaimConversation *conv)
{
	GaimConversationType type;
	GList *wins, *convs;
	GaimAccount *account;

	account = gaim_conversation_get_account(conv);
	type = gaim_conversation_get_type(conv);


	/* Go through the list of IMs and find one with this group. */
	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		GaimConvWindow *win2;
		GaimConversation *conv2;

		win2 = (GaimConvWindow *)wins->data;

		for (convs = gaim_conv_window_get_conversations(win2);
				convs != NULL;
				convs = convs->next) {

			conv2 = (GaimConversation *)convs->data;

			if ((gaim_prefs_get_bool("/core/conversations/combine_chat_im") ||
				 type == gaim_conversation_get_type(conv2)) &&
				account == gaim_conversation_get_account(conv2)) {

				gaim_conv_window_add_conversation(win2, conv);
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

	for(n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		if(!strcmp(data->id, id))
			return data;
	}

	return NULL;
}

static void
add_conv_placement_fnc(const char *id, const char *name,
					   GaimConvPlacementFunc fnc)
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
	if (conv_placement_fncs == NULL)
	{
		add_conv_placement_fnc("last", _("Last created window"),
							   conv_placement_last_created_win);
		add_conv_placement_fnc("new", _("New window"),
							   conv_placement_new_window);
		add_conv_placement_fnc("group", _("By group"),
							   conv_placement_by_group);
		add_conv_placement_fnc("account", _("By account"),
							   conv_placement_by_account);
	}
}

GList *
gaim_conv_placement_get_options(void)
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
gaim_conv_placement_add_fnc(const char *id, const char *name,
							GaimConvPlacementFunc fnc)
{
	g_return_if_fail(id   != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fnc  != NULL);

	ensure_default_funcs();

	add_conv_placement_fnc(id, name, fnc);
}

void
gaim_conv_placement_remove_fnc(const char *id)
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
gaim_conv_placement_get_name(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->name;
}

GaimConvPlacementFunc
gaim_conv_placement_get_fnc(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->fnc;
}

void
gaim_conv_placement_set_current_func(GaimConvPlacementFunc func)
{
	g_return_if_fail(func != NULL);

	place_conv = func;
}

GaimConvPlacementFunc
gaim_conv_placement_get_current_func(void)
{
	return place_conv;
}

static void
update_titles_pref_cb(const char *name, GaimPrefType type,
					  gpointer value, gpointer data)
{
	/*
	 * If the use_server_alias option was changed, and use_alias_for_title
	 * is false, then we don't have to do anything here.
	 */
	if (!strcmp(name, "/core/buddies/use_server_alias") &&
		!gaim_prefs_get_bool("/core/conversations/use_alias_for_title"))
		return;

	gaim_conversation_foreach(gaim_conversation_autoset_title);
}

void
gaim_conversations_set_win_ui_ops(GaimConvWindowUiOps *ops)
{
	win_ui_ops = ops;
}

GaimConvWindowUiOps *
gaim_conversations_get_win_ui_ops(void)
{
	return win_ui_ops;
}

void *
gaim_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_conversations_init(void)
{
	void *handle = gaim_conversations_get_handle();

	/**********************************************************************
	 * Register preferences
	 **********************************************************************/

	/* Conversations */
	gaim_prefs_add_none("/core/conversations");
	gaim_prefs_add_bool("/core/conversations/send_urls_as_links", TRUE);
	gaim_prefs_add_bool("/core/conversations/away_back_on_send", TRUE);
	gaim_prefs_add_bool("/core/conversations/use_alias_for_title", TRUE);
	gaim_prefs_add_bool("/core/conversations/combine_chat_im", FALSE);

	/* Conversations -> Chat */
	gaim_prefs_add_none("/core/conversations/chat");
	gaim_prefs_add_bool("/core/conversations/chat/show_join", TRUE);
	gaim_prefs_add_bool("/core/conversations/chat/show_leave", TRUE);
	gaim_prefs_add_bool("/core/conversations/chat/show_nick_change", TRUE);

	/* Conversations -> IM */
	gaim_prefs_add_none("/core/conversations/im");
	gaim_prefs_add_bool("/core/conversations/im/show_login", TRUE);
	gaim_prefs_add_bool("/core/conversations/im/send_typing", TRUE);

	gaim_prefs_connect_callback("/core/conversations/use_alias_for_title",
			update_titles_pref_cb, NULL);
	gaim_prefs_connect_callback("/core/buddies/use_server_alias",
			update_titles_pref_cb, NULL);


	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	gaim_signal_register(handle, "displaying-im-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "displayed-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "sending-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "sent-im-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "received-im-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 4,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "displaying-chat-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "displayed-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER,
						 NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "sending-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "sent-chat-msg",
						 gaim_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_UINT));

	gaim_signal_register(handle, "received-chat-msg",
						 gaim_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
						 gaim_value_new(GAIM_TYPE_BOOLEAN), 4,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "conversation-switching",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "conversation-switched",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "conversation-created",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "deleting-conversation",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "buddy-typing",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "buddy-typing-stopped",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "chat-buddy-joining",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-buddy-joined",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-buddy-leaving",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-buddy-left",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-inviting-user",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-invited-user",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-invited",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER_POINTER,
						 NULL, 4,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new(GAIM_TYPE_STRING));

	gaim_signal_register(handle, "chat-joined",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));

	gaim_signal_register(handle, "chat-left",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_CONVERSATION));
}

void
gaim_conversations_uninit(void)
{
	gaim_signals_unregister_by_instance(gaim_conversations_get_handle());
}
