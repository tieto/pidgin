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
 *
 */
#include "internal.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "util.h"

/* XXX */
#include "ui.h"

typedef struct
{
	char *id;
	char *name;
	GaimConvPlacementFunc fnc;

} ConvPlacementData;

#define SEND_TYPED_TIMEOUT 5000

static GaimWindowUiOps *win_ui_ops = NULL;

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static GList *windows = NULL;
static GList *conv_placement_fncs = NULL;
static GaimConvPlacementFunc place_conv = NULL;

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

	if (!gaim_utf8_strcasecmp(gaim_account_get_username(account),
							  gaim_connection_get_display_name(gc))) {
		g_free(msg);

		return FALSE;
	}

	who = g_utf8_strdown(gaim_connection_get_display_name(gc), -1);
	n = strlen(who);

	if (n > 0 && (p = strstr(msg, who)) != NULL) {
		if ((p == msg || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
			g_free(who);
			g_free(msg);

			return TRUE;
		}
	}

	g_free(who);
	g_free(msg);

	return FALSE;
}

static gboolean
reset_typing(gpointer data)
{
	char *name = (char *)data;
	GaimConversation *c = gaim_find_conversation(name);
	GaimIm *im;

	if (!c)
		return FALSE;

	im = GAIM_IM(c);

	gaim_im_set_typing_state(im, GAIM_NOT_TYPING);
	gaim_im_update_typing(im);
	gaim_im_stop_typing_timeout(im);

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
		gaim_im_set_type_again(GAIM_IM(conv), TRUE);

		/* XXX Somebody add const stuff! */
		serv_send_typing(gc, (char *)name, GAIM_TYPED);

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
	gulong length = 0;
	gboolean binary = FALSE;
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

	if ((gc->flags & OPT_CONN_HTML) &&
		gaim_prefs_get_bool("/core/conversations/send_urls_as_links")) {

		buffy = linkify_text(buf);
	}
	else
		buffy = g_strdup(buf);

	plugin_return = gaim_event_broadcast(
			(type == GAIM_CONV_IM ? event_im_send : event_chat_send),
			gc,
			(type == GAIM_CONV_IM
			 ? gaim_conversation_get_name(conv)
			 : (void *)gaim_chat_get_id(GAIM_CHAT(conv))),
			&buffy);

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

	if (type == GAIM_CONV_IM) {
		GaimIm *im = GAIM_IM(conv);

		buffy = g_strdup(buf);
		gaim_event_broadcast(event_im_displayed_sent, gc,
					 gaim_conversation_get_name(conv), &buffy);

		if (buffy != NULL) {
			int imflags = 0;

			if (conv->u.im->images != NULL) {
				int id = 0, offset = 0;
				char *bigbuf = NULL;
				GSList *tmplist;

				for (tmplist = conv->u.im->images;
					 tmplist != NULL;
					 tmplist = tmplist->next) {

					char *img_filename = (char *)tmplist->data;
					FILE *imgfile;
					char *filename, *c;
					struct stat st;
					char imgtag[1024];

					id++;

					if (stat(img_filename, &st) != 0) {
						gaim_debug(GAIM_DEBUG_ERROR, "conversation",
								   "Could not stat image %s\n",
								   img_filename);

						continue;
					}

					/*
					 * Here we check to make sure the user still wants to send
					 * the image. He may have deleted the <img> tag in which
					 * case we don't want to send the binary data.
					 */
					filename = img_filename;

					while ((c = strchr(filename, '/')) != NULL)
						filename = c + 1;
					
					g_snprintf(imgtag, sizeof(imgtag),
							   "<IMG SRC=\"file://%s\" ID=\"%d\" "
							   "DATASIZE=\"%d\">",
							   filename, id, (int)st.st_size);

					if (strstr(buffy, imgtag) == 0) {
						gaim_debug(GAIM_DEBUG_ERROR, "conversation",
								   "Not sending image: %s\n", img_filename);
						continue;
					}

					if (!binary) {
						length = strlen(buffy) + strlen("<BINARY></BINARY>");
						bigbuf = g_malloc(length + 1);
						g_snprintf(bigbuf,
								   strlen(buffy) + strlen("<BINARY> ") + 1,
								   "%s<BINARY>", buffy);

						offset = strlen(buffy) + strlen("<BINARY>");
						binary = TRUE;
					}

					g_snprintf(imgtag, sizeof(imgtag),
							   "<DATA ID=\"%d\" SIZE=\"%d\">",
							   id, (int)st.st_size);

					length += strlen(imgtag) + st.st_size + strlen("</DATA>");

					bigbuf = g_realloc(bigbuf, length + 1);

					if ((imgfile = fopen(img_filename, "r")) == NULL) {
						gaim_debug(GAIM_DEBUG_ERROR, "conversation",
								   "Could not open image %s\n", img_filename);
						continue;
					}

					strncpy(bigbuf + offset, imgtag, strlen(imgtag) + 1);

					offset += strlen(imgtag);
					offset += fread(bigbuf + offset, 1, st.st_size, imgfile);

					fclose(imgfile);

					strncpy(bigbuf + offset, "</DATA>",
							strlen("</DATA>") + 1);

					offset += strlen("</DATA>");
				}

				if (binary) {
					strncpy(bigbuf + offset, "</BINARY>",
							strlen("<BINARY>") + 1);

					err = serv_send_im(gc,
						(char *)gaim_conversation_get_name(conv),
						bigbuf, length, imflags);
				}
				else
					err = serv_send_im(gc,
						(char *)gaim_conversation_get_name(conv),
						buffy, -1, imflags);

				if (err > 0) {
					GSList *tempy;

					for (tempy = conv->u.im->images;
						 tempy != NULL;
						 tempy = tempy->next) {

						g_free(tempy->data);
					}

					g_slist_free(conv->u.im->images);
					conv->u.im->images = NULL;

					if (binary)
						gaim_im_write(im, NULL, bigbuf, length,
									  WFLAG_SEND, time(NULL));
					else
						gaim_im_write(im, NULL, buffy, -1, WFLAG_SEND,
									  time(NULL));
				}

				if (binary)
					g_free(bigbuf);
			}
			else {
				err = serv_send_im(gc, (char *)gaim_conversation_get_name(conv),
								   buffy, -1, imflags);

				if (err > 0)
					gaim_im_write(im, NULL, buf, -1, WFLAG_SEND, time(NULL));
			}

			g_free(buffy);
		}
	}
	else {
		err = serv_chat_send(gc, gaim_chat_get_id(GAIM_CHAT(conv)), buf);
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
update_conv_indexes(GaimWindow *win)
{
	GList *l;
	int i;

	for (l = gaim_window_get_conversations(win), i = 0;
		 l != NULL;
		 l = l->next, i++) {

		GaimConversation *conv = (GaimConversation *)l->data;

		conv->conversation_pos = i;
	}
}

GaimWindow *
gaim_window_new(void)
{
	GaimWindow *win;

	win = g_malloc0(sizeof(GaimWindow));

	windows = g_list_append(windows, win);

	win->ui_ops = gaim_get_win_ui_ops();

	if (win->ui_ops != NULL && win->ui_ops->new_window != NULL)
		win->ui_ops->new_window(win);

	return win;
}

void
gaim_window_destroy(GaimWindow *win)
{
	GaimWindowUiOps *ops;
	GList *node;

	g_return_if_fail(win != NULL);

	ops = gaim_window_get_ui_ops(win);

	/*
	 * If there are any conversations in this, destroy them all. The last
	 * conversation will call gaim_window_destroy(), but this time, this
	 * check will fail and the window will actually be destroyed.
	 *
	 * This is needed because chats may not close right away. They may
	 * wait for notification first. When they get that, the window is
	 * already destroyed, and gaim either crashes or spits out gtk warnings.
	 * The problem is fixed with this check.
	 */
	if (gaim_window_get_conversation_count(win) > 0) {

		node = g_list_first(gaim_window_get_conversations(win));
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

		g_list_free(gaim_window_get_conversations(win));

		windows = g_list_remove(windows, win);

		g_free(win);
	}
}

void
gaim_window_show(GaimWindow *win)
{
	GaimWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->show == NULL)
		return;

	ops->show(win);
}

void
gaim_window_hide(GaimWindow *win)
{
	GaimWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->hide == NULL)
		return;

	ops->hide(win);
}

void
gaim_window_raise(GaimWindow *win)
{
	GaimWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->raise == NULL)
		return;

	ops->raise(win);
}

void
gaim_window_flash(GaimWindow *win)
{
	GaimWindowUiOps *ops;

	g_return_if_fail(win != NULL);

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->flash == NULL)
		return;

	ops->flash(win);
}

void
gaim_window_set_ui_ops(GaimWindow *win, GaimWindowUiOps *ops)
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

	for (l = gaim_window_get_conversations(win);
		 l != NULL;
		 l = l->next) {

		GaimConversation *conv = (GaimConversation *)l;

		gaim_conversation_set_ui_ops(conv, conv_ops);

		if (win->ui_ops != NULL && win->ui_ops->add_conversation != NULL)
			win->ui_ops->add_conversation(win, conv);
	}
}

GaimWindowUiOps *
gaim_window_get_ui_ops(const GaimWindow *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->ui_ops;
}

int
gaim_window_add_conversation(GaimWindow *win, GaimConversation *conv)
{
	GaimWindowUiOps *ops;

	g_return_val_if_fail(win  != NULL, -1);
	g_return_val_if_fail(conv != NULL, -1);

	if (gaim_conversation_get_window(conv) != NULL) {
		gaim_window_remove_conversation(
			gaim_conversation_get_window(conv),
			gaim_conversation_get_index(conv));
	}

	ops = gaim_window_get_ui_ops(win);

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
gaim_window_remove_conversation(GaimWindow *win, unsigned int index)
{
	GaimWindowUiOps *ops;
	GaimConversation *conv;
	GList *node;

	g_return_val_if_fail(win != NULL, NULL);
	g_return_val_if_fail(index < gaim_window_get_conversation_count(win), NULL);

	ops = gaim_window_get_ui_ops(win);

	node = g_list_nth(gaim_window_get_conversations(win), index);
	conv = (GaimConversation *)node->data;

	if (ops != NULL && ops->remove_conversation != NULL)
		ops->remove_conversation(win, conv);

	win->conversations = g_list_remove_link(win->conversations, node);

	g_list_free_1(node);

	win->conversation_count--;

	conv->window = NULL;

	if (gaim_window_get_conversation_count(win) == 0)
		gaim_window_destroy(win);
	else {
		/* Change all the indexes. */
		update_conv_indexes(win);
	}

	return conv;
}

void
gaim_window_move_conversation(GaimWindow *win, unsigned int index,
							  unsigned int new_index)
{
	GaimWindowUiOps *ops;
	GaimConversation *conv;
	GList *l;

	g_return_if_fail(win != NULL);
	g_return_if_fail(index < gaim_window_get_conversation_count(win));
	g_return_if_fail(index != new_index);

	/* We can't move this past the last index. */
	if (new_index > gaim_window_get_conversation_count(win))
		new_index = gaim_window_get_conversation_count(win);

	/* Get the list item for this conversation at its current index. */
	l = g_list_nth(gaim_window_get_conversations(win), index);

	if (l == NULL) {
		/* Should never happen. */
		gaim_debug(GAIM_DEBUG_ERROR, "conversation",
				   "Misordered conversations list in window %p\n", win);

		return;
	}

	conv = (GaimConversation *)l->data;

	/* Update the UI part of this. */
	ops = gaim_window_get_ui_ops(win);

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
gaim_window_get_conversation_at(const GaimWindow *win, unsigned int index)
{
	g_return_val_if_fail(win != NULL, NULL);
	g_return_val_if_fail(index >= 0 &&
						 index < gaim_window_get_conversation_count(win),
						 NULL);

	return (GaimConversation *)g_list_nth_data(
		gaim_window_get_conversations(win), index);
}

size_t
gaim_window_get_conversation_count(const GaimWindow *win)
{
	g_return_val_if_fail(win != NULL, 0);

	return win->conversation_count;
}

void
gaim_window_switch_conversation(GaimWindow *win, unsigned int index)
{
	GaimWindowUiOps *ops;

	g_return_if_fail(win != NULL);
	g_return_if_fail(index >= 0 &&
					 index < gaim_window_get_conversation_count(win));

	ops = gaim_window_get_ui_ops(win);

	if (ops != NULL && ops->switch_conversation != NULL)
		ops->switch_conversation(win, index);

	gaim_conversation_set_unseen(
		gaim_window_get_conversation_at(win, index), 0);
}

GaimConversation *
gaim_window_get_active_conversation(const GaimWindow *win)
{
	GaimWindowUiOps *ops;

	g_return_val_if_fail(win != NULL, NULL);

	if (gaim_window_get_conversation_count(win) == 0)
		return NULL;

	ops = gaim_window_get_ui_ops(win);

	if (ops != NULL && ops->get_active_index != NULL)
		return gaim_window_get_conversation_at(win, ops->get_active_index(win));

	return NULL;
}

GList *
gaim_window_get_conversations(const GaimWindow *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	return win->conversations;
}

GList *
gaim_get_windows(void)
{
	return windows;
}

GaimWindow *
gaim_get_first_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	GaimWindow *win;
	GaimConversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		win = (GaimWindow *)wins->data;

		for (convs = gaim_window_get_conversations(win);
			 convs != NULL;
			 convs = convs->next) {

			conv = (GaimConversation *)convs->data;

			if (gaim_conversation_get_type(conv) == type)
				return win;
		}
	}

	return NULL;
}

GaimWindow *
gaim_get_last_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	GaimWindow *win;
	GaimConversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = g_list_last(gaim_get_windows());
		 wins != NULL;
		 wins = wins->prev) {

		win = (GaimWindow *)wins->data;

		for (convs = gaim_window_get_conversations(win);
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

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	/* Check if this conversation already exists. */
	if ((conv = gaim_find_conversation_with_account(name, account)) != NULL)
		return conv;

	conv = g_malloc0(sizeof(GaimConversation));

	conv->type         = type;
	conv->account      = account;
	conv->name         = g_strdup(name);
	conv->title        = g_strdup(name);
	conv->send_history = g_list_append(NULL, NULL);
	conv->history      = g_string_new("");
	conv->data         = g_hash_table_new_full(g_str_hash, g_str_equal,
											   g_free, NULL);

	if (type == GAIM_CONV_IM)
	{
		conv->u.im = g_malloc0(sizeof(GaimIm));
		conv->u.im->conv = conv;

		ims = g_list_append(ims, conv);

		gaim_conversation_set_logging(conv,
				gaim_prefs_get_bool("/gaim/gtk/logging/log_ims"));
	}
	else if (type == GAIM_CONV_CHAT)
	{
		conv->u.chat = g_malloc0(sizeof(GaimChat));
		conv->u.chat->conv = conv;

		chats = g_list_append(chats, conv);

		gaim_conversation_set_logging(conv,
				gaim_prefs_get_bool("/gaim/gtk/logging/log_chats"));
	}

	conversations = g_list_append(conversations, conv);

	/* Auto-set the title. */
	gaim_conversation_autoset_title(conv);

	/*
	 * Create a window if one does not exist. If it does, use the last
	 * created window.
	 */
	if (windows == NULL ||
		!gaim_prefs_get_bool("/gaim/gtk/conversations/tabs")) {

		GaimWindow *win;

		win = gaim_window_new();
		gaim_window_add_conversation(win, conv);

		/* Ensure the window is visible. */
		gaim_window_show(win);
	}
	else {
		if (place_conv == NULL)
			gaim_prefs_set_string("/core/conversations/placement", "last");

		if (!place_conv)
			gaim_debug(GAIM_DEBUG_ERROR, "conversation", "This is about to suck.\n");

		place_conv(conv);
	}

	gaim_event_broadcast(event_new_conversation, name);

	return conv;
}

void
gaim_conversation_destroy(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimWindow *win;
	GaimConversationUiOps *ops;
	GaimConnection *gc;
	const char *name;
	GList *node;

	g_return_if_fail(conv != NULL);

	win  = gaim_conversation_get_window(conv);
	ops  = gaim_conversation_get_ui_ops(conv);
	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (gc) {
		/* Still connected */
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
			if (gaim_prefs_get_bool("/core/conversations/im/send_typing"))
				serv_send_typing(gc, (char *)name, GAIM_NOT_TYPING);

			if (gc && prpl_info->convo_closed != NULL)
				prpl_info->convo_closed(gc, (char *)name);
		}
		else if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) {
			/*
			 * This is unfortunately necessary, because calling serv_chat_leave()
			 * calls this gaim_conversation_destroy(), which leads to two calls
			 * here.. We can't just return after this, because then it'll return
			 * on the next pass. So, since serv_got_chat_left(), which is
			 * eventually called from the prpl that serv_chat_leave() calls,
			 * removes this conversation from the gc's buddy_chats list, we're
			 * going to check to see if this exists in the list. If so, we want
			 * to return after calling this, because it'll be called again. If not,
			 * fall through, because it'll have already been removed, and we'd
			 * be on the 2nd pass.
			 *
			 * Long paragraph. <-- Short sentence.
			 *
			 *   -- ChipX86
			 */

			if (gc && g_slist_find(gc->buddy_chats, conv) != NULL) {
				serv_chat_leave(gc, gaim_chat_get_id(GAIM_CHAT(conv)));

				return;
			}
		}
	}

	gaim_event_broadcast(event_del_conversation, conv);

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
		GSList *snode;

		gaim_im_stop_typing_timeout(conv->u.im);
		gaim_im_stop_type_again_timeout(conv->u.im);

		for (snode = conv->u.im->images; snode != NULL; snode = snode->next) {
			if (snode->data != NULL)
				g_free(snode->data);
		}

		g_slist_free(conv->u.im->images);

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
		gaim_window_remove_conversation(win,
			gaim_conversation_get_index(conv));
	}

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

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
	struct buddy *b;
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

GaimWindow *
gaim_conversation_get_window(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	return conv->window;
}

GaimIm *
gaim_conversation_get_im_data(const GaimConversation *conv)
{
	g_return_val_if_fail(conv != NULL, NULL);

	if (gaim_conversation_get_type(conv) != GAIM_CONV_IM)
		return NULL;

	return conv->u.im;
}

GaimChat *
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

GaimConversation *
gaim_find_conversation(const char *name)
{
	GaimConversation *c = NULL;
	char *cuser;
	GList *cnv;

	g_return_val_if_fail(name != NULL, NULL);

	cuser = g_strdup(normalize(name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (GaimConversation *)cnv->data;

		if (!gaim_utf8_strcasecmp(cuser, normalize(gaim_conversation_get_name(c))))
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

	cuser = g_strdup(normalize(name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (GaimConversation *)cnv->data;

		if (!gaim_utf8_strcasecmp(cuser,
								  normalize(gaim_conversation_get_name(c))) &&
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
						const char *message, size_t length, int flags,
						time_t mtime)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc;
	GaimAccount *account;
	GaimConversationUiOps *ops;
	GaimWindow *win;
	struct buddy *b;
	GaimUnseenState unseen;
	/* int logging_font_options = 0; */

	g_return_if_fail(conv    != NULL);
	g_return_if_fail(message != NULL);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops == NULL || ops->write_conv == NULL)
		return;

	account = gaim_conversation_get_account(conv);
	gc      = gaim_account_get_connection(account);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		(account->gc == NULL || !g_slist_find(account->gc->buddy_chats, conv)))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		!g_list_find(gaim_get_conversations(), conv))
		return;

	if (account->gc != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

		if (gaim_conversation_get_type(conv) == GAIM_CONV_IM ||
			!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (who == NULL) {
				if (flags & WFLAG_SEND) {
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

	ops->write_conv(conv, who, message, length, flags, mtime);

	win = gaim_conversation_get_window(conv);

	/* Tab highlighting */
	if (!(flags & WFLAG_RECV) && !(flags & WFLAG_SYSTEM))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if ((flags & WFLAG_RECV) == WFLAG_RECV)
			gaim_im_set_typing_state(GAIM_IM(conv), GAIM_NOT_TYPING);
	}

	if (gaim_window_get_active_conversation(win) != conv) {
		if ((flags & WFLAG_NICK) == WFLAG_NICK ||
				gaim_conversation_get_unseen(conv) == GAIM_UNSEEN_NICK)
			unseen = GAIM_UNSEEN_NICK;
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
gaim_im_get_conversation(const GaimIm *im)
{
	g_return_val_if_fail(im != NULL, NULL);

	return im->conv;
}

void
gaim_im_set_typing_state(GaimIm *im, int state)
{
	g_return_if_fail(im != NULL);

	im->typing_state = state;
}

int
gaim_im_get_typing_state(const GaimIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_state;
}

void
gaim_im_start_typing_timeout(GaimIm *im, int timeout)
{
	GaimConversation *conv;
	const char *name;

	g_return_if_fail(im != NULL);

	if (im->typing_timeout > 0)
		gaim_im_stop_typing_timeout(im);

	conv = gaim_im_get_conversation(im);
	name = gaim_conversation_get_name(conv);

	im->typing_timeout = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
		timeout * 1000, reset_typing, g_strdup(name), g_free);
}

void
gaim_im_stop_typing_timeout(GaimIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->typing_timeout == 0)
		return;

	g_source_remove(im->typing_timeout);
	im->typing_timeout = 0;
}

guint
gaim_im_get_typing_timeout(const GaimIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->typing_timeout;
}

void
gaim_im_set_type_again(GaimIm *im, time_t val)
{
	g_return_if_fail(im != NULL);

	im->type_again = val;
}

time_t
gaim_im_get_type_again(const GaimIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again;
}

void
gaim_im_start_type_again_timeout(GaimIm *im)
{
	g_return_if_fail(im != NULL);

	im->type_again_timeout = g_timeout_add(SEND_TYPED_TIMEOUT, send_typed,
										   gaim_im_get_conversation(im));
}

void
gaim_im_stop_type_again_timeout(GaimIm *im)
{
	g_return_if_fail(im != NULL);

	if (im->type_again_timeout == 0)
		return;

	g_source_remove(im->type_again_timeout);
	im->type_again_timeout = 0;
}

guint
gaim_im_get_type_again_timeout(const GaimIm *im)
{
	g_return_val_if_fail(im != NULL, 0);

	return im->type_again_timeout;
}

void
gaim_im_update_typing(GaimIm *im)
{
	g_return_if_fail(im != NULL);

	gaim_conversation_update(gaim_im_get_conversation(im),
							 GAIM_CONV_UPDATE_TYPING);
}

void
gaim_im_write(GaimIm *im, const char *who, const char *message,
			  size_t len, int flags, time_t mtime)
{
	GaimConversation *c;

	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	c = gaim_im_get_conversation(im);

	/* Raise the window, if specified in prefs. */
	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, len, flags, mtime);
	else
		gaim_conversation_write(c, who, message, -1, flags, mtime);
}

void
gaim_im_send(GaimIm *im, const char *message)
{
	g_return_if_fail(im != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_im_get_conversation(im), message);
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/

GaimConversation *
gaim_chat_get_conversation(const GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->conv;
}

GList *
gaim_chat_set_users(GaimChat *chat, GList *users)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->in_room = users;

	return users;
}

GList *
gaim_chat_get_users(const GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->in_room;
}

void
gaim_chat_ignore(GaimChat *chat, const char *name)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user isn't already ignored. */
	if (gaim_chat_is_user_ignored(chat, name))
		return;

	gaim_chat_set_ignored(chat,
		g_list_append(gaim_chat_get_ignored(chat), g_strdup(name)));
}

void
gaim_chat_unignore(GaimChat *chat, const char *name)
{
	GList *item;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user is actually ignored. */
	if (!gaim_chat_is_user_ignored(chat, name))
		return;

	item = g_list_find(gaim_chat_get_ignored(chat),
					   gaim_chat_get_ignored_user(chat, name));

	gaim_chat_set_ignored(chat,
		g_list_remove_link(gaim_chat_get_ignored(chat), item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
gaim_chat_set_ignored(GaimChat *chat, GList *ignored)
{
	g_return_val_if_fail(chat != NULL, NULL);

	chat->ignored = ignored;

	return ignored;
}

GList *
gaim_chat_get_ignored(const GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->ignored;
}

const char *
gaim_chat_get_ignored_user(const GaimChat *chat, const char *user)
{
	GList *ignored;

	g_return_val_if_fail(chat != NULL, NULL);
	g_return_val_if_fail(user != NULL, NULL);

	for (ignored = gaim_chat_get_ignored(chat);
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
gaim_chat_is_user_ignored(const GaimChat *chat, const char *user)
{
	g_return_val_if_fail(chat != NULL, FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (gaim_chat_get_ignored_user(chat, user) != NULL);
}

void
gaim_chat_set_topic(GaimChat *chat, const char *who, const char *topic)
{
	g_return_if_fail(chat != NULL);

	if (chat->who   != NULL) free(chat->who);
	if (chat->topic != NULL) free(chat->topic);

	chat->who   = (who   == NULL ? NULL : g_strdup(who));
	chat->topic = (topic == NULL ? NULL : g_strdup(topic));

	gaim_conversation_update(gaim_chat_get_conversation(chat),
							 GAIM_CONV_UPDATE_TOPIC);
}

const char *
gaim_chat_get_topic(const GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return chat->topic;
}

void
gaim_chat_set_id(GaimChat *chat, int id)
{
	g_return_if_fail(chat != NULL);

	chat->id = id;
}

int
gaim_chat_get_id(const GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, -1);

	return chat->id;
}

void
gaim_chat_write(GaimChat *chat, const char *who, const char *message,
				int flags, time_t mtime)
{
	GaimAccount *account;
	GaimConversation *conv;
	GaimConnection *gc;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	conv    = gaim_chat_get_conversation(chat);
	gc      = gaim_conversation_get_gc(conv);
	account = gaim_connection_get_account(gc);

	/* Don't display this if the person who wrote it is ignored. */
	if (gaim_chat_is_user_ignored(chat, who))
		return;

	if (!(flags & WFLAG_WHISPER)) {
		char *str;

		str = g_strdup(normalize(who));

		if (!gaim_utf8_strcasecmp(str, normalize(gaim_account_get_username(account))) ||
			!gaim_utf8_strcasecmp(str, normalize(gaim_connection_get_display_name(gc)))) {

			flags |= WFLAG_SEND;
		}
		else {
			flags |= WFLAG_RECV;

			if (find_nick(gc, message))
				flags |= WFLAG_NICK;
		}
		
		g_free(str);
	}

	/* Pass this on to either the ops structure or the default write func. */
	if (conv->ui_ops != NULL && conv->ui_ops->write_chat != NULL)
		conv->ui_ops->write_chat(conv, who, message, flags, mtime);
	else
		gaim_conversation_write(conv, who, message, -1, flags, mtime);
}

void
gaim_chat_send(GaimChat *chat, const char *message)
{
	g_return_if_fail(chat != NULL);
	g_return_if_fail(message != NULL);

	common_send(gaim_chat_get_conversation(chat), message);
}

void
gaim_chat_add_user(GaimChat *chat, const char *user, const char *extra_msg)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_chat_set_users(chat,
		g_list_insert_sorted(gaim_chat_get_users(chat), g_strdup(user),
							 insertname_compare));

	gaim_event_broadcast(event_chat_buddy_join,
				 gaim_conversation_get_gc(conv), gaim_chat_get_id(chat),
				 user);

	if (ops != NULL && ops->chat_add_user != NULL)
		ops->chat_add_user(conv, user);

	if (gaim_prefs_get_bool("/core/conversations/chat/show_join")) {
		if (extra_msg == NULL)
			g_snprintf(tmp, sizeof(tmp), _("%s entered the room."), user);
		else
			g_snprintf(tmp, sizeof(tmp),
					   _("%s [<I>%s</I>] entered the room."),
					   user, extra_msg);

		gaim_conversation_write(conv, NULL, tmp, -1, WFLAG_SYSTEM, time(NULL));
	}
}

void
gaim_chat_rename_user(GaimChat *chat, const char *old_user,
					  const char *new_user)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];
	GList *names;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_chat_set_users(chat,
		g_list_insert_sorted(gaim_chat_get_users(chat), g_strdup(new_user),
							 insertname_compare));

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user);

	for (names = gaim_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		if (!gaim_utf8_strcasecmp((char *)names->data, old_user)) {
			gaim_chat_set_users(chat,
					g_list_remove(gaim_chat_get_users(chat), names->data));
			break;
		}
	}

	if (gaim_chat_is_user_ignored(chat, old_user)) {
		gaim_chat_unignore(chat, old_user);
		gaim_chat_ignore(chat, new_user);
	}
	else if (gaim_chat_is_user_ignored(chat, new_user))
		gaim_chat_unignore(chat, new_user);

	if (gaim_prefs_get_bool("/core/conversations/chat/show_nick_change")) {
		g_snprintf(tmp, sizeof(tmp),
				   _("%s is now known as %s"), old_user, new_user);

		gaim_conversation_write(conv, NULL, tmp, -1, WFLAG_SYSTEM, time(NULL));
	}
}

void
gaim_chat_remove_user(GaimChat *chat, const char *user, const char *reason)
{
	GaimConversation *conv;
	GaimConversationUiOps *ops;
	char tmp[BUF_LONG];
	GList *names;

	g_return_if_fail(chat != NULL);
	g_return_if_fail(user != NULL);

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_event_broadcast(event_chat_buddy_leave, gaim_conversation_get_gc(conv),
				 gaim_chat_get_id(chat), user);

	if (ops != NULL && ops->chat_remove_user != NULL)
		ops->chat_remove_user(conv, user);

	for (names = gaim_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		if (!gaim_utf8_strcasecmp((char *)names->data, user)) {
			gaim_chat_set_users(chat,
					g_list_remove(gaim_chat_get_users(chat), names->data));
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

		gaim_conversation_write(conv, NULL, tmp, -1, WFLAG_SYSTEM, time(NULL));
	}
}

GaimConversation *
gaim_find_chat(const GaimConnection *gc, int id)
{
	GList *l;
	GaimConversation *conv;

	for (l = gaim_get_chats(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (gaim_chat_get_id(GAIM_CHAT(conv)) == id &&
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
	GaimWindow *win;

	if (gaim_prefs_get_bool("/core/conversations/combine_chat_im"))
		win = g_list_last(gaim_get_windows())->data;
	else
		win = gaim_get_last_window_with_type(gaim_conversation_get_type(conv));

	if (win == NULL) {
		win = gaim_window_new();

		gaim_window_add_conversation(win, conv);
		gaim_window_show(win);
	}
	else
		gaim_window_add_conversation(win, conv);
}

/* This one places each conversation in its own window. */
static void
conv_placement_new_window(GaimConversation *conv)
{
	GaimWindow *win;

	win = gaim_window_new();

	gaim_window_add_conversation(win, conv);

	gaim_window_show(win);
}

/*
 * This groups things by, well, group. Buddies from groups will always be
 * grouped together, and a buddy from a group not belonging to any currently
 * open windows will get a new window.
 */
static void
conv_placement_by_group(GaimConversation *conv)
{
	GaimWindow *win;
	GaimConversationType type;

	type = gaim_conversation_get_type(conv);

	if (type != GAIM_CONV_IM) {
		win = gaim_get_last_window_with_type(type);

		if (win == NULL)
			conv_placement_new_window(conv);
		else
			gaim_window_add_conversation(win, conv);
	}
	else {
		struct buddy *b;
		struct group *grp = NULL;
		GList *wins, *convs;

		b = gaim_find_buddy(gaim_conversation_get_account(conv),
					   gaim_conversation_get_name(conv));

		if (b != NULL)
			grp = gaim_find_buddys_group(b);

		/* Go through the list of IMs and find one with this group. */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			GaimWindow *win2;
			GaimConversation *conv2;
			struct buddy *b2;
			struct group *g2 = NULL;

			win2 = (GaimWindow *)wins->data;

			for (convs = gaim_window_get_conversations(win2);
				 convs != NULL;
				 convs = convs->next) {

				conv2 = (GaimConversation *)convs->data;

				b2 = gaim_find_buddy(gaim_conversation_get_account(conv2),
								gaim_conversation_get_name(conv2));

				if (b2 != NULL)
					g2 = gaim_find_buddys_group(b2);

				if (grp == g2) {
					gaim_window_add_conversation(win2, conv);

					return;
				}
			}
		}

		/* Make a new window. */
		conv_placement_new_window(conv);
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
		GaimWindow *win2;
		GaimConversation *conv2;

		win2 = (GaimWindow *)wins->data;

		for (convs = gaim_window_get_conversations(win2);
				convs != NULL;
				convs = convs->next) {

			conv2 = (GaimConversation *)convs->data;

			if ((gaim_prefs_get_bool("/core/conversations/combine_chat_im") ||
				 type == gaim_conversation_get_type(conv2)) &&
				account == gaim_conversation_get_account(conv2)) {

				gaim_window_add_conversation(win2, conv);
				return;
			}

		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

static ConvPlacementData *get_conv_placement_data(const char *id)
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
	if (conv_placement_fncs == NULL) {
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

GList *gaim_conv_placement_get_options(void)
{
	GList *n, *list = NULL;
	ConvPlacementData *data;

	ensure_default_funcs();

	for(n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		list = g_list_append(list, data->name);
		list = g_list_append(list, data->id);
	}

	return list;
}


void
gaim_conv_placement_add_fnc(const char *id, const char *name, GaimConvPlacementFunc fnc)
{
	g_return_if_fail(id != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fnc  != NULL);

	ensure_default_funcs();

	add_conv_placement_fnc(id, name, fnc);
}

void
gaim_conv_placement_remove_fnc(const char *id)
{
	ConvPlacementData *data = get_conv_placement_data(id);

	if(!data)
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

	if (!data)
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

static void
conv_placement_pref_cb(const char *name, GaimPrefType type, gpointer value,
		gpointer data)
{
	GaimConvPlacementFunc fnc;

	if(strcmp(name, "/core/conversations/placement"))
		return;

	ensure_default_funcs();

	fnc = gaim_conv_placement_get_fnc(value);

	if (fnc == NULL)
		return;

	place_conv = fnc;
}

static void
update_titles_pref_cb(const char *name, GaimPrefType type, gpointer value,
		gpointer data)
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
gaim_set_win_ui_ops(GaimWindowUiOps *ops)
{
	win_ui_ops = ops;
}

GaimWindowUiOps *
gaim_get_win_ui_ops(void)
{
	return win_ui_ops;
}

void
gaim_conversation_init(void)
{
	/* Conversations */
	gaim_prefs_add_none("/core/conversations");
	gaim_prefs_add_bool("/core/conversations/send_urls_as_links", TRUE);
	gaim_prefs_add_bool("/core/conversations/away_back_on_send", TRUE);
	gaim_prefs_add_bool("/core/conversations/use_alias_for_title", TRUE);
	gaim_prefs_add_bool("/core/conversations/combine_chat_im", FALSE);
	gaim_prefs_add_string("/core/conversations/placement", "last");

	/* Conversations -> Chat */
	gaim_prefs_add_none("/core/conversations/chat");
	gaim_prefs_add_bool("/core/conversations/chat/show_join", TRUE);
	gaim_prefs_add_bool("/core/conversations/chat/show_leave", TRUE);
	gaim_prefs_add_bool("/core/conversations/chat/show_nick_change", TRUE);

	/* Conversations -> IM */
	gaim_prefs_add_none("/core/conversations/im");
	gaim_prefs_add_bool("/core/conversations/im/show_login", TRUE);
	gaim_prefs_add_bool("/core/conversations/im/send_typing", TRUE);

	gaim_prefs_connect_callback("/core/conversations/placement",
			conv_placement_pref_cb, NULL);
	gaim_prefs_trigger_callback("/core/conversations/placement");
	gaim_prefs_connect_callback("/core/conversations/use_alias_for_title",
			update_titles_pref_cb, NULL);
	gaim_prefs_connect_callback("/core/buddies/use_server_alias",
			update_titles_pref_cb, NULL);
}
