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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include "conversation.h"
#include "gaim.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

struct ConvPlacementData
{
	char *name;
	gaim_conv_placement_fnc fnc;

};

#define SEND_TYPED_TIMEOUT 5000

static struct gaim_window_ui_ops *win_ui_ops = NULL;

static GList *conversations = NULL;
static GList *ims = NULL;
static GList *chats = NULL;
static GList *windows = NULL;
static GList *conv_placement_fncs = NULL;
static gaim_conv_placement_fnc place_conv = NULL;
static int place_conv_index = -1;

static gint
insertname_compare(gconstpointer one, gconstpointer two)
{
	const char *a = (const char *)one;
	const char *b = (const char *)two;

	if (*a == '@') {
		if (*b != '@') return -1;

		return (strcasecmp(a + 1, b + 1));

	} else if (*a == '+') {
		if (*b == '@') return  1;
		if (*b != '+') return -1;
		
		return (strcasecmp(a + 1, b + 1));

	} else if (*a == '@' || *b == '+')
		return 1;

	return (strcasecmp(a, b));
}

static gboolean
find_nick(struct gaim_connection *gc, const char *message)
{
	char *msg, *who, *p;
	int n;

	msg = g_strdup(message);
	g_strdown(msg);

	who = g_strdup(gc->username);
	g_strdown(who);
	n = strlen(who);

	if ((p = strstr(msg, who)) != NULL) {
		if ((p == msg || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
			g_free(who);
			g_free(msg);

			return TRUE;
		}
	}

	g_free(who);

	if (!g_strcasecmp(gc->username, gc->displayname)) {
		g_free(msg);

		return FALSE;
	}

	who = g_strdup(gc->displayname);
	g_strdown(who);
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
	struct gaim_conversation *c = gaim_find_conversation(name);
	struct gaim_im *im;

	if (!c)
		return FALSE;

	im = GAIM_IM(c);

	gaim_im_set_typing_state(im, NOT_TYPING);
	gaim_im_update_typing(im);
	gaim_im_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed(gpointer data)
{
	struct gaim_conversation *conv = (struct gaim_conversation *)data;
	struct gaim_connection *gc;
	const char *name;

	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (conv != NULL && gc != NULL && name != NULL) {
		gaim_im_set_type_again(GAIM_IM(conv), TRUE);

		/* XXX Somebody add const stuff! */
		serv_send_typing(gc, (char *)name, TYPED);

		debug_printf("typed...\n");
	}

	return FALSE;
}

static void
common_send(struct gaim_conversation *conv, const char *message)
{
	GaimConversationType type;
	struct gaim_connection *gc;
	struct gaim_conversation_ui_ops *ops;
	char *buf, *buf2, *buffy;
	gulong length = 0;
	gboolean binary = FALSE;
	int plugin_return;
	int limit;
	int err = 0;
	GList *first;

	if ((gc = gaim_conversation_get_gc(conv)) == NULL)
		return;

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

	if (gc->flags & OPT_CONN_HTML) {
		if (convo_options & OPT_CONVO_SEND_LINKS)
			linkify_text(buf);
	}

	buffy = g_strdup(buf);
	plugin_return = plugin_event(
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
		struct gaim_im *im = GAIM_IM(conv);

		buffy = g_strdup(buf);
		plugin_event(event_im_displayed_sent, gc,
					 gaim_conversation_get_name(conv), &buffy);

		if (buffy != NULL) {
			int imflags = 0;

			if (conv->u.im->images != NULL) {
				int id = 1, offset = 0;
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

					if (stat(img_filename, &st) != 0) {
						debug_printf("Could not stat %s\n",
									 (char *)img_filename);
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
						debug_printf("Not sending image: %s\n", img_filename);
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
						debug_printf("Could not open %s\n", img_filename);
						continue;
					}

					strncpy(bigbuf + offset, imgtag, strlen(imgtag) + 1);

					offset += strlen(imgtag);
					offset += fread(bigbuf + offset, 1, st.st_size, imgfile);

					fclose(imgfile);

					strncpy(bigbuf + offset, "</DATA>",
							strlen("</DATA>") + 1);

					offset += strlen("</DATA>");
					id++;
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

					g_slist_free(tempy);
					conv->u.im->images = NULL;

					if (binary)
						gaim_im_write(im, NULL, bigbuf, length,
									  WFLAG_SEND, time(NULL));
					else
						gaim_im_write(im, NULL, buffy, -1, WFLAG_SEND,
									  time(NULL));

					if (im_options & OPT_IM_POPDOWN)
						gaim_window_hide(gaim_conversation_get_window(conv));
				}

				if (binary)
					g_free(bigbuf);
			}
			else {
				err = serv_send_im(gc, (char *)gaim_conversation_get_name(conv),
								   buffy, -1, imflags);

				if (err > 0) {
					gaim_im_write(im, NULL, buf, -1, WFLAG_SEND, time(NULL));

					if (im_options & OPT_IM_POPDOWN)
						gaim_window_hide(gaim_conversation_get_window(conv));
				}
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
		if (err == -E2BIG)
			do_error_dialog(_("Unable to send message. "
							  "The message is too large."), NULL,
							GAIM_ERROR);
		else if (err == -ENOTCONN)
			debug_printf("Not yet connected.\n");
		else
			do_error_dialog(_("Unable to send message."), NULL, GAIM_ERROR);
	}
	else {
		if (err > 0 && (away_options & OPT_AWAY_BACK_ON_IM)) {
			if (awaymessage != NULL) {
				do_im_back();
			}
			else if (gc->away) {
				serv_set_away(gc, GAIM_AWAY_CUSTOM, NULL);
			}
		}
	}

}

static void
update_conv_indexes(struct gaim_window *win)
{
	GList *l;
	int i;

	for (l = gaim_window_get_conversations(win), i = 0;
		 l != NULL;
		 l = l->next, i++) {

		struct gaim_conversation *conv = (struct gaim_conversation *)l->data;

		conv->conversation_pos = i;
	}
}

struct gaim_window *
gaim_window_new(void)
{
	struct gaim_window *win;

	win = g_malloc0(sizeof(struct gaim_window));

	win->ui_ops = gaim_get_win_ui_ops();

	if (win->ui_ops != NULL && win->ui_ops->new_window != NULL)
		win->ui_ops->new_window(win);

	windows = g_list_append(windows, win);

	return win;
}

void
gaim_window_destroy(struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;
	GList *node;

	if (win == NULL)
		return;

	ops = gaim_window_get_ui_ops(win);

	for (node = g_list_first(gaim_window_get_conversations(win));
		 node != NULL;
		 node = g_list_next(node))
	{
		struct gaim_conversation *conv;
		
		conv = (struct gaim_conversation *)node->data;

		conv->window = NULL;
		gaim_conversation_destroy(conv);

		node->data = NULL;
	}

	if (ops != NULL && ops->destroy_window != NULL)
		ops->destroy_window(win);

	g_list_free(gaim_window_get_conversations(win));

	windows = g_list_remove(windows, win);

	g_free(win);
}

void
gaim_window_show(struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL)
		return;

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->show == NULL)
		return;

	ops->show(win);
}

void
gaim_window_hide(struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL)
		return;

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->hide == NULL)
		return;

	ops->hide(win);
}

void
gaim_window_raise(struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL)
		return;

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->raise == NULL)
		return;

	ops->raise(win);
}

void
gaim_window_flash(struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL)
		return;

	ops = gaim_window_get_ui_ops(win);

	if (ops == NULL || ops->flash == NULL)
		return;

	ops->flash(win);
}

void
gaim_window_set_ui_ops(struct gaim_window *win, struct gaim_window_ui_ops *ops)
{
	struct gaim_conversation_ui_ops *conv_ops = NULL;
	GList *l;

	if (win == NULL || win->ui_ops == ops)
		return;

	if (ops != NULL) {
		if (ops->get_conversation_ui_ops != NULL)
			conv_ops = ops->get_conversation_ui_ops();
	}

	if (win->ui_ops != NULL) {
		if (win->ui_ops->destroy_window != NULL)
			win->ui_ops->destroy_window(win);
	}

	win->ui_ops = ops;

	if (win->ui_ops != NULL) {
		if (win->ui_ops->new_window != NULL)
			win->ui_ops->new_window(win);
	}

	for (l = gaim_window_get_conversations(win);
		 l != NULL;
		 l = l->next) {

		struct gaim_conversation *conv = (struct gaim_conversation *)l;

		gaim_conversation_set_ui_ops(conv, conv_ops);

		if (win->ui_ops != NULL && win->ui_ops->add_conversation != NULL)
			win->ui_ops->add_conversation(win, conv);
	}
}

struct gaim_window_ui_ops *
gaim_window_get_ui_ops(const struct gaim_window *win)
{
	if (win == NULL)
		return NULL;

	return win->ui_ops;
}

int
gaim_window_add_conversation(struct gaim_window *win,
							 struct gaim_conversation *conv)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL || conv == NULL)
		return -1;

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

struct gaim_conversation *
gaim_window_remove_conversation(struct gaim_window *win, unsigned int index)
{
	struct gaim_window_ui_ops *ops;
	struct gaim_conversation *conv;
	GList *node;

	if (win == NULL || index >= gaim_window_get_conversation_count(win))
		return NULL;

	ops = gaim_window_get_ui_ops(win);

	node = g_list_nth(gaim_window_get_conversations(win), index);
	conv = (struct gaim_conversation *)node->data;

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
gaim_window_move_conversation(struct gaim_window *win, unsigned int index,
							  unsigned int new_index)
{
	struct gaim_window_ui_ops *ops;
	struct gaim_conversation *conv;
	GList *l;

	if (win == NULL || index >= gaim_window_get_conversation_count(win) ||
		index == new_index)
		return;

	/* We can't move this past the last index. */
	if (new_index > gaim_window_get_conversation_count(win))
		new_index = gaim_window_get_conversation_count(win);

	/* Get the list item for this conversation at its current index. */
	l = g_list_nth(gaim_window_get_conversations(win), index);

	if (l == NULL) {
		/* Should never happen. */
		debug_printf("Misordered conversations list in window %p\n", win);

		return;
	}

	conv = (struct gaim_conversation *)l->data;

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

struct gaim_conversation *
gaim_window_get_conversation_at(const struct gaim_window *win,
								unsigned int index)
{
	if (win == NULL || index >= gaim_window_get_conversation_count(win))
		return NULL;

	return (struct gaim_conversation *)g_list_nth_data(
		gaim_window_get_conversations(win), index);
}

size_t
gaim_window_get_conversation_count(const struct gaim_window *win)
{
	if (win == NULL)
		return 0;

	return win->conversation_count;
}

void
gaim_window_switch_conversation(struct gaim_window *win, unsigned int index)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL || index < 0 ||
		index >= gaim_window_get_conversation_count(win))
		return;

	ops = gaim_window_get_ui_ops(win);

	if (ops != NULL && ops->switch_conversation != NULL)
		ops->switch_conversation(win, index);

	gaim_conversation_set_unseen(
		gaim_window_get_conversation_at(win, index), 0);
}

struct gaim_conversation *
gaim_window_get_active_conversation(const struct gaim_window *win)
{
	struct gaim_window_ui_ops *ops;

	if (win == NULL)
		return NULL;

	ops = gaim_window_get_ui_ops(win);

	if (ops != NULL && ops->get_active_index != NULL)
		return gaim_window_get_conversation_at(win, ops->get_active_index(win));

	return NULL;
}

GList *
gaim_window_get_conversations(const struct gaim_window *win)
{
	if (win == NULL)
		return NULL;

	return win->conversations;
}

GList *
gaim_get_windows(void)
{
	return windows;
}

struct gaim_window *
gaim_get_first_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	struct gaim_window *win;
	struct gaim_conversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
		win = (struct gaim_window *)wins->data;

		for (convs = gaim_window_get_conversations(win);
			 convs != NULL;
			 convs = convs->next) {

			conv = (struct gaim_conversation *)convs->data;

			if (gaim_conversation_get_type(conv) == type)
				return win;
		}
	}

	return NULL;
}

struct gaim_window *
gaim_get_last_window_with_type(GaimConversationType type)
{
	GList *wins, *convs;
	struct gaim_window *win;
	struct gaim_conversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	for (wins = g_list_last(gaim_get_windows());
		 wins != NULL;
		 wins = wins->prev) {

		win = (struct gaim_window *)wins->data;

		for (convs = gaim_window_get_conversations(win);
			 convs != NULL;
			 convs = convs->next) {

			conv = (struct gaim_conversation *)convs->data;

			if (gaim_conversation_get_type(conv) == type)
				return win;
		}
	}

	return NULL;
}

/**************************************************************************
 * Conversation API
 **************************************************************************/
struct gaim_conversation *
gaim_conversation_new(GaimConversationType type, struct aim_user *user,
					  const char *name)
{
	struct gaim_conversation *conv;

	if (type == GAIM_CONV_UNKNOWN)
		return NULL;

	/* Check if this conversation already exists. */
	if ((conv = gaim_find_conversation_with_user(name, user)) != NULL)
		return conv;

	conv = g_malloc0(sizeof(struct gaim_conversation));

	conv->type         = type;
	conv->user         = user;
	conv->name         = g_strdup(name);
	conv->title        = g_strdup(name);
	conv->send_history = g_list_append(NULL, NULL);
	conv->history      = g_string_new("");

	if (type == GAIM_CONV_IM)
	{
		conv->u.im = g_malloc0(sizeof(struct gaim_im));
		conv->u.im->conv = conv;

		ims = g_list_append(ims, conv);

		gaim_conversation_set_logging(conv,
									  (logging_options & OPT_LOG_CONVOS));
	}
	else if (type == GAIM_CONV_CHAT)
	{
		conv->u.chat = g_malloc0(sizeof(struct gaim_chat));
		conv->u.chat->conv = conv;

		chats = g_list_append(chats, conv);

		gaim_conversation_set_logging(conv, (logging_options & OPT_LOG_CHATS));
	}

	conversations = g_list_append(conversations, conv);

	/* Auto-set the title. */
	gaim_conversation_autoset_title(conv);

	/*
	 * Create a window if one does not exist. If it does, use the last
	 * created window.
	 */
	if (windows == NULL) {
		struct gaim_window *win;

		win = gaim_window_new();
		gaim_window_add_conversation(win, conv);

		/* Ensure the window is visible. */
		gaim_window_show(win);
	}
	else {
		if (place_conv == NULL)
			gaim_conv_placement_set_active(0);

		place_conv(conv);
	}

	plugin_event(event_new_conversation, name);

	return conv;
}

void
gaim_conversation_destroy(struct gaim_conversation *conv)
{
	struct gaim_window *win;
	struct gaim_conversation_ui_ops *ops;
	struct gaim_connection *gc;
	const char *name;
	GList *node;

	if (conv == NULL)
		return;

	win  = gaim_conversation_get_window(conv);
	ops  = gaim_conversation_get_ui_ops(conv);
	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if (!(misc_options & OPT_MISC_STEALTH_TYPING))
			serv_send_typing(gc, (char *)name, NOT_TYPING);

		if (gc && gc->prpl->convo_closed != NULL)
			gc->prpl->convo_closed(gc, (char *)name);
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

	plugin_event(event_del_conversation, conv);

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
		gaim_im_stop_typing_timeout(conv->u.im);
		gaim_im_stop_type_again_timeout(conv->u.im);

		g_slist_free(conv->u.im->images);

		g_free(conv->u.im);

		ims = g_list_remove(ims, conv);
	}
	else if (conv->type == GAIM_CONV_CHAT) {
		g_list_free(conv->u.chat->in_room);
		g_list_free(conv->u.chat->ignored);

		if (conv->u.chat->who != NULL)
			g_free(conv->u.chat->who);

		if (conv->u.chat->topic != NULL)
			g_free(conv->u.chat->topic);

		g_free(conv->u.chat);

		chats = g_list_remove(chats, conv);
	}

	if (win != NULL) {
		gaim_window_remove_conversation(win,
			gaim_conversation_get_index(conv));
	}

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

	g_free(conv);
}

GaimConversationType
gaim_conversation_get_type(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return GAIM_CONV_UNKNOWN;

	return conv->type;
}

void
gaim_conversation_set_ui_ops(struct gaim_conversation *conv,
							 struct gaim_conversation_ui_ops *ops)
{
	if (conv == NULL || conv->ui_ops == ops)
		return;

	if (conv->ui_ops != NULL && conv->ui_ops->destroy_conversation != NULL)
		conv->ui_ops->destroy_conversation(conv);

	conv->ui_data = NULL;

	conv->ui_ops = ops;
}

struct gaim_conversation_ui_ops *
gaim_conversation_get_ui_ops(struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->ui_ops;
}

void
gaim_conversation_set_user(struct gaim_conversation *conv,
						   struct aim_user *user)
{
	if (conv == NULL || user == gaim_conversation_get_user(conv))
		return;

	conv->user = user;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_USER);
}

struct aim_user *
gaim_conversation_get_user(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->user;
}

struct gaim_connection *
gaim_conversation_get_gc(const struct gaim_conversation *conv)
{
	struct aim_user *user;

	if (conv == NULL)
		return NULL;

	user = gaim_conversation_get_user(conv);

	if (user == NULL)
		return NULL;

	return user->gc;
}

void
gaim_conversation_set_title(struct gaim_conversation *conv, const char *title)
{
	struct gaim_conversation_ui_ops *ops;

	if (conv == NULL || title == NULL)
		return;

	if (conv->title != NULL)
		g_free(conv->title);

	conv->title = g_strdup(title);

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->set_title != NULL)
		ops->set_title(conv, conv->title);
}

const char *
gaim_conversation_get_title(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->title;
}

void
gaim_conversation_autoset_title(struct gaim_conversation *conv)
{
	struct aim_user *user;
	struct buddy *b;
	const char *text, *name;

	if (conv == NULL)
		return;

	user = gaim_conversation_get_user(conv);
	name = gaim_conversation_get_name(conv);

	if (((im_options & OPT_IM_ALIAS_TAB) == OPT_IM_ALIAS_TAB) &&
		user != NULL && ((b = find_buddy(user, name)) != NULL)) {

		text = get_buddy_alias(b);
	}
	else
		text = name;

	gaim_conversation_set_title(conv, text);
}

int
gaim_conversation_get_index(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return 0;

	return conv->conversation_pos;
}

void
gaim_conversation_set_unseen(struct gaim_conversation *conv,
							 GaimUnseenState state)
{
	if (conv == NULL)
		return;

	conv->unseen = state;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_UNSEEN);
}

void
gaim_conversation_foreach(void (*func)(struct gaim_conversation *conv))
{
	struct gaim_conversation *conv;
	GList *l;

	if (func == NULL)
		return;

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (struct gaim_conversation *)l->data;

		func(conv);
	}
}

GaimUnseenState
gaim_conversation_get_unseen(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return 0;

	return conv->unseen;
}

const char *
gaim_conversation_get_name(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->name;
}

void
gaim_conversation_set_logging(struct gaim_conversation *conv, gboolean log)
{
	if (conv == NULL)
		return;

	conv->logging = log;

	gaim_conversation_update(conv, GAIM_CONV_UPDATE_LOGGING);
}

gboolean
gaim_conversation_is_logging(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return FALSE;

	return conv->logging;
}

GList *
gaim_conversation_get_send_history(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->send_history;
}

void
gaim_conversation_set_history(struct gaim_conversation *conv,
							  GString *history)
{
	if (conv == NULL)
		return;

	conv->history = history;
}

GString *
gaim_conversation_get_history(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->history;
}

struct gaim_window *
gaim_conversation_get_window(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	return conv->window;
}

struct gaim_im *
gaim_conversation_get_im_data(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	if (gaim_conversation_get_type(conv) != GAIM_CONV_IM)
		return NULL;

	return conv->u.im;
}

struct gaim_chat *
gaim_conversation_get_chat_data(const struct gaim_conversation *conv)
{
	if (conv == NULL)
		return NULL;

	if (gaim_conversation_get_type(conv) != GAIM_CONV_CHAT)
		return NULL;

	return conv->u.chat;
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

struct gaim_conversation *
gaim_find_conversation(const char *name)
{
	struct gaim_conversation *c = NULL;
	char *cuser;
	GList *cnv;

	if (name == NULL)
		return NULL;

	cuser = g_strdup(normalize(name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (struct gaim_conversation *)cnv->data;

		if (!g_strcasecmp(cuser, normalize(gaim_conversation_get_name(c))))
			break;

		c = NULL;
	}

	g_free(cuser);

	return c;
}

struct gaim_conversation *
gaim_find_conversation_with_user(const char *name, const struct aim_user *user)
{
	struct gaim_conversation *c = NULL;
	char *cuser;
	GList *cnv;

	if (name == NULL)
		return NULL;

	cuser = g_strdup(normalize(name));

	for (cnv = gaim_get_conversations(); cnv != NULL; cnv = cnv->next) {
		c = (struct gaim_conversation *)cnv->data;

		if (!g_strcasecmp(cuser, normalize(gaim_conversation_get_name(c))) &&
			user == gaim_conversation_get_user(c)) {

			break;
		}

		c = NULL;
	}

	g_free(cuser);

	return c;
}

void
gaim_conversation_write(struct gaim_conversation *conv, const char *who,
						const char *message, size_t length, int flags,
						time_t mtime)
{
	struct gaim_connection *gc;
	struct gaim_conversation_ui_ops *ops;
	struct gaim_window *win;
	struct buddy *b;
	GaimUnseenState unseen;
	/* int logging_font_options = 0; */

	if (conv == NULL || message == NULL)
		return;

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops == NULL || ops->write_conv == NULL)
		return;

	gc = gaim_conversation_get_gc(conv);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		(gc == NULL || !g_slist_find(gc->buddy_chats, conv)))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		!g_list_find(gaim_get_conversations(), conv))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM ||
		!(gc->prpl->options & OPT_PROTO_UNIQUE_CHATNAME)) {

		if (who == NULL) {
			if ((flags & WFLAG_SEND) == WFLAG_SEND) {
				b = find_buddy(gc->user, gc->username);

				if (b != NULL && strcmp(b->name, get_buddy_alias(b)))
					who = get_buddy_alias(b);
				else if (*gc->user->alias)
					who = gc->user->alias;
				else if (*gc->displayname)
					who = gc->displayname;
				else
					who = gc->username;
			}
			else {
				b = find_buddy(gc->user, gaim_conversation_get_name(conv));

				if (b != NULL)
					who = get_buddy_alias(b);
				else
					who = gaim_conversation_get_name(conv);
			}
		}
		else {
			b = find_buddy(gc->user, who);

			if (b != NULL)
				who = get_buddy_alias(b);
		}
	}

	ops->write_conv(conv, who, message, length, flags, mtime);

	win = gaim_conversation_get_window(conv);

	if (!(flags & WFLAG_NOLOG) &&
		((gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		  (chat_options & OPT_CHAT_POPUP)) ||
		 (gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		  (im_options & OPT_IM_POPUP)))) {

		gaim_window_show(win);
	}

	/* Tab highlighting */
	if (!(flags & WFLAG_RECV) && !(flags & WFLAG_SYSTEM))
		return;

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if ((flags & WFLAG_RECV) == WFLAG_RECV)
			gaim_im_set_typing_state(GAIM_IM(conv), NOT_TYPING);
	}

	if (gaim_window_get_active_conversation(win) != conv) {
		if ((flags & WFLAG_NICK) == WFLAG_NICK)
			unseen = GAIM_UNSEEN_NICK;
		else
			unseen = GAIM_UNSEEN_TEXT;
	}
	else
		unseen = GAIM_UNSEEN_NONE;

	gaim_conversation_set_unseen(conv, unseen);
}

void
gaim_conversation_update_progress(struct gaim_conversation *conv,
								  float percent)
{
	struct gaim_conversation_ui_ops *ops;

	if (conv == NULL)
		return;

	if (percent < 0)
		percent = 0;

	/*
	 * NOTE: A percent >= 1 indicates that the progress bar should be
	 *       closed.
	 */
	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->update_progress != NULL)
		ops->update_progress(conv, percent);
}

void
gaim_conversation_update(struct gaim_conversation *conv,
						 GaimConvUpdateType type)
{
	struct gaim_conversation_ui_ops *ops;

	if (conv == NULL)
		return;

	ops = gaim_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->updated != NULL)
		ops->updated(conv, type);
}

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
struct gaim_conversation *
gaim_im_get_conversation(struct gaim_im *im)
{
	if (im == NULL)
		return NULL;

	return im->conv;
}

void
gaim_im_set_typing_state(struct gaim_im *im, int state)
{
	if (im == NULL)
		return;

	im->typing_state = state;
}

int
gaim_im_get_typing_state(const struct gaim_im *im)
{
	if (im == NULL)
		return 0;

	return im->typing_state;
}

void
gaim_im_start_typing_timeout(struct gaim_im *im, int timeout)
{
	struct gaim_conversation *conv;
	const char *name;

	if (im == NULL)
		return;

	if (im->typing_timeout > 0)
		gaim_im_stop_typing_timeout(im);

	conv = gaim_im_get_conversation(im);
	name = gaim_conversation_get_name(conv);

	im->typing_timeout = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
		timeout * 1000, reset_typing, g_strdup(name), g_free);
}

void
gaim_im_stop_typing_timeout(struct gaim_im *im)
{
	if (im == NULL)
		return;

	if (im->typing_timeout == 0)
		return;

	g_source_remove(im->typing_timeout);
	im->typing_timeout = 0;
}

guint
gaim_im_get_typing_timeout(const struct gaim_im *im)
{
	if (im == NULL)
		return 0;

	return im->typing_timeout;
}

void
gaim_im_set_type_again(struct gaim_im *im, time_t val)
{
	if (im == NULL)
		return;

	im->type_again = val;
}

time_t
gaim_im_get_type_again(const struct gaim_im *im)
{
	if (im == NULL)
		return 0;

	return im->type_again;
}

void
gaim_im_start_type_again_timeout(struct gaim_im *im)
{
	if (im == NULL)
		return;

	im->type_again_timeout = g_timeout_add(SEND_TYPED_TIMEOUT, send_typed,
										   gaim_im_get_conversation(im));
}

void
gaim_im_stop_type_again_timeout(struct gaim_im *im)
{
	if (im == NULL)
		return;

	if (im->type_again_timeout == 0)
		return;

	g_source_remove(im->type_again_timeout);
	im->type_again_timeout = 0;
}

guint
gaim_im_get_type_again_timeout(const struct gaim_im *im)
{
	if (im == NULL)
		return 0;

	return im->type_again_timeout;
}

void
gaim_im_update_typing(struct gaim_im *im)
{
	if (im == NULL)
		return;

	gaim_conversation_update(gaim_im_get_conversation(im),
							 GAIM_CONV_UPDATE_TYPING);
}

void
gaim_im_write(struct gaim_im *im, const char *who, const char *message,
			  size_t len, int flags, time_t mtime)
{
	struct gaim_conversation *c;

	if (im == NULL || message == NULL)
		return;

	c = gaim_im_get_conversation(im);

	/* Raise the window, if specified in prefs. */
	if (!(flags & WFLAG_NOLOG) & (im_options & OPT_IM_POPUP))
		gaim_window_raise(gaim_conversation_get_window(c));

	if (c->ui_ops != NULL && c->ui_ops->write_im != NULL)
		c->ui_ops->write_im(c, who, message, len, flags, mtime);
	else
		gaim_conversation_write(c, who, message, -1, flags, mtime);
}

void
gaim_im_send(struct gaim_im *im, const char *message)
{
	if (im == NULL || message == NULL)
		return;

	common_send(gaim_im_get_conversation(im), message);
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/

struct gaim_conversation *
gaim_chat_get_conversation(struct gaim_chat *chat)
{
	if (chat == NULL)
		return NULL;

	return chat->conv;
}

GList *
gaim_chat_set_users(struct gaim_chat *chat, GList *users)
{
	if (chat == NULL)
		return NULL;

	chat->in_room = users;

	return users;
}

GList *
gaim_chat_get_users(const struct gaim_chat *chat)
{
	if (chat == NULL)
		return NULL;

	return chat->in_room;
}

void
gaim_chat_ignore(struct gaim_chat *chat, const char *name)
{
	if (chat == NULL || name == NULL)
		return;

	/* Make sure the user isn't already ignored. */
	if (gaim_chat_is_user_ignored(chat, name))
		return;

	gaim_chat_set_ignored(chat,
		g_list_append(gaim_chat_get_ignored(chat), g_strdup(name)));
}

void
gaim_chat_unignore(struct gaim_chat *chat, const char *name)
{
	GList *item;

	if (chat == NULL || name == NULL)
		return;

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
gaim_chat_set_ignored(struct gaim_chat *chat, GList *ignored)
{
	if (chat == NULL)
		return NULL;

	chat->ignored = ignored;

	return ignored;
}

GList *
gaim_chat_get_ignored(const struct gaim_chat *chat)
{
	if (chat == NULL)
		return NULL;

	return chat->ignored;
}

const char *
gaim_chat_get_ignored_user(const struct gaim_chat *chat, const char *user)
{
	GList *ignored;

	if (chat == NULL || user == NULL)
		return NULL;

	for (ignored = gaim_chat_get_ignored(chat);
		 ignored != NULL;
		 ignored = ignored->next) {

		const char *ign = (const char *)ignored->data;

		if (!g_strcasecmp(user, ign) ||
			(*ign == '+' && !g_strcasecmp(user, ign + 1)))
			return ign;

		if (*ign == '@') {
			ign++;

			if ((*ign == '+' && !g_strcasecmp(user, ign + 1)) ||
				(*ign != '+' && !g_strcasecmp(user, ign)))
				return ign;
		}
	}

	return NULL;
}

gboolean
gaim_chat_is_user_ignored(const struct gaim_chat *chat, const char *user)
{
	if (chat == NULL || user == NULL)
		return FALSE;

	return (gaim_chat_get_ignored_user(chat, user) != NULL);
}

void
gaim_chat_set_topic(struct gaim_chat *chat, const char *who, const char *topic)
{
	if (chat == NULL)
		return;

	if (chat->who   != NULL) free(chat->who);
	if (chat->topic != NULL) free(chat->topic);

	chat->who   = (who   == NULL ? NULL : g_strdup(who));
	chat->topic = (topic == NULL ? NULL : g_strdup(topic));

	gaim_conversation_update(gaim_chat_get_conversation(chat),
							 GAIM_CONV_UPDATE_TOPIC);
}

const char *
gaim_chat_get_topic(const struct gaim_chat *chat)
{
	if (chat == NULL)
		return NULL;

	return chat->topic;
}

void
gaim_chat_set_id(struct gaim_chat *chat, int id)
{
	if (chat == NULL)
		return;

	chat->id = id;
}

int
gaim_chat_get_id(const struct gaim_chat *chat)
{
	if (chat == NULL)
		return -1;

	return chat->id;
}

void
gaim_chat_write(struct gaim_chat *chat, const char *who,
				const char *message, int flags, time_t mtime)
{
	struct gaim_conversation *conv;
	struct gaim_connection *gc;

	if (chat == NULL || who == NULL || message == NULL)
		return;

	conv = gaim_chat_get_conversation(chat);
	gc   = gaim_conversation_get_gc(conv);

	/* Don't display this if the person who wrote it is ignored. */
	if (gaim_chat_is_user_ignored(chat, who))
		return;

	/* Raise the window, if specified in prefs. */
	if (!(flags & WFLAG_NOLOG) & (chat_options & OPT_CHAT_POPUP))
		gaim_window_raise(gaim_conversation_get_window(conv));

	if (!(flags & WFLAG_WHISPER)) {
		char *str;

		str = g_strdup(normalize(who));

		if (!g_strcasecmp(str, normalize(gc->username)) ||
			!g_strcasecmp(str, normalize(gc->displayname))) {

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
gaim_chat_send(struct gaim_chat *chat, const char *message)
{
	if (chat == NULL || message == NULL)
		return;

	common_send(gaim_chat_get_conversation(chat), message);
}

void
gaim_chat_add_user(struct gaim_chat *chat, const char *user,
				   const char *extra_msg)
{
	struct gaim_conversation *conv;
	struct gaim_conversation_ui_ops *ops;
	char tmp[BUF_LONG];

	if (chat == NULL || user == NULL)
		return;

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_chat_set_users(chat,
		g_list_insert_sorted(gaim_chat_get_users(chat), g_strdup(user),
							 insertname_compare));

	plugin_event(event_chat_buddy_join,
				 gaim_conversation_get_gc(conv), gaim_chat_get_id(chat),
				 user);

	if (ops != NULL && ops->chat_add_user != NULL)
		ops->chat_add_user(conv, user);

	if (chat_options & OPT_CHAT_LOGON) {
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
gaim_chat_rename_user(struct gaim_chat *chat, const char *old_user,
					  const char *new_user)
{
	struct gaim_conversation *conv;
	struct gaim_conversation_ui_ops *ops;
	char tmp[BUF_LONG];

	if (chat == NULL || old_user == NULL || new_user == NULL)
		return;

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	gaim_chat_set_users(chat,
		g_list_insert_sorted(gaim_chat_get_users(chat), g_strdup(new_user),
							 insertname_compare));

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(conv, old_user, new_user);

	gaim_chat_set_users(chat, g_list_remove(gaim_chat_get_users(chat),
											old_user));

	if (gaim_chat_is_user_ignored(chat, old_user)) {
		gaim_chat_unignore(chat, old_user);
		gaim_chat_ignore(chat, new_user);
	}
	else if (gaim_chat_is_user_ignored(chat, new_user))
		gaim_chat_unignore(chat, new_user);

	if (chat_options & OPT_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp),
				   _("%s is now known as %s"), old_user, new_user);

		gaim_conversation_write(conv, NULL, tmp, -1, WFLAG_SYSTEM, time(NULL));
	}
}

void
gaim_chat_remove_user(struct gaim_chat *chat, const char *user,
					  const char *reason)
{
	struct gaim_conversation *conv;
	struct gaim_conversation_ui_ops *ops;
	char tmp[BUF_LONG];
	GList *names;

	if (chat == NULL || user == NULL)
		return;

	conv = gaim_chat_get_conversation(chat);
	ops  = gaim_conversation_get_ui_ops(conv);

	plugin_event(event_chat_buddy_leave, gaim_conversation_get_gc(conv),
				 gaim_chat_get_id(chat), user);

	if (ops != NULL && ops->chat_remove_user != NULL)
		ops->chat_remove_user(conv, user);

	for (names = gaim_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		if (!g_strcasecmp((char *)names->data, user)) {
			gaim_chat_set_users(chat,
					g_list_remove(gaim_chat_get_users(chat), names->data));
			break;
		}
	}

	/* NOTE: Don't remove them from ignored in case they re-enter. */

	if (chat_options & OPT_CHAT_LOGON) {
		if (reason != NULL && *reason != '\0')
			g_snprintf(tmp, sizeof(tmp),
					   _("%s left the room (%s)."), user, reason);
		else
			g_snprintf(tmp, sizeof(tmp), _("%s left the room."), user);

		gaim_conversation_write(conv, NULL, tmp, -1, WFLAG_SYSTEM, time(NULL));
	}
}

struct gaim_conversation *
gaim_find_chat(struct gaim_connection *gc, int id)
{
	GList *l;
	struct gaim_conversation *conv;

	for (l = gaim_get_chats(); l != NULL; l = l->next) {
		conv = (struct gaim_conversation *)l->data;

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
conv_placement_last_created_win(struct gaim_conversation *conv)
{
	struct gaim_window *win;

	if (convo_options & OPT_CONVO_COMBINE)
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
conv_placement_new_window(struct gaim_conversation *conv)
{
	struct gaim_window *win;

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
conv_placement_by_group(struct gaim_conversation *conv)
{
	struct gaim_window *win;
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

		b = find_buddy(gaim_conversation_get_user(conv),
					   gaim_conversation_get_name(conv));

		if (b != NULL)
			grp = find_group_by_buddy(b);

		/* Go through the list of IMs and find one with this group. */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			struct gaim_window *win2;
			struct gaim_conversation *conv2;
			struct buddy *b2;
			struct group *g2 = NULL;

			win2 = (struct gaim_window *)wins->data;

			for (convs = gaim_window_get_conversations(win2);
				 convs != NULL;
				 convs = convs->next) {

				conv2 = (struct gaim_conversation *)convs->data;

				b2 = find_buddy(gaim_conversation_get_user(conv2),
								gaim_conversation_get_name(conv2));

				if (b2 != NULL)
					g2 = find_group_by_buddy(b2);

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

static int
add_conv_placement_fnc(const char *name, gaim_conv_placement_fnc fnc)
{
	struct ConvPlacementData *data;

	data = g_malloc0(sizeof(struct ConvPlacementData));

	data->name = g_strdup(name);
	data->fnc  = fnc;

	conv_placement_fncs = g_list_append(conv_placement_fncs, data);

	return gaim_conv_placement_get_fnc_count() - 1;
}

static void
ensure_default_funcs(void)
{
	if (conv_placement_fncs == NULL) {
		add_conv_placement_fnc(_("Last created window"),
							   conv_placement_last_created_win);
		add_conv_placement_fnc(_("New window"),
							   conv_placement_new_window);
		add_conv_placement_fnc(_("By group"),
							   conv_placement_by_group);
	}
}

int
gaim_conv_placement_add_fnc(const char *name, gaim_conv_placement_fnc fnc)
{
	if (name == NULL || fnc == NULL)
		return -1;

	if (conv_placement_fncs == NULL)
		ensure_default_funcs();

	return add_conv_placement_fnc(name, fnc);
}

void
gaim_conv_placement_remove_fnc(int index)
{
	struct ConvPlacementData *data;
	GList *node;

	if (index < 0 || index > g_list_length(conv_placement_fncs))
		return;

	node = g_list_nth(conv_placement_fncs, index);
	data = (struct ConvPlacementData *)node->data;

	g_free(data->name);
	g_free(data);

	conv_placement_fncs = g_list_remove_link(conv_placement_fncs, node);
	g_list_free_1(node);
}

int
gaim_conv_placement_get_fnc_count(void)
{
	ensure_default_funcs();

	return g_list_length(conv_placement_fncs);
}

const char *
gaim_conv_placement_get_name(int index)
{
	struct ConvPlacementData *data;

	ensure_default_funcs();

	if (index < 0 || index > g_list_length(conv_placement_fncs))
		return NULL;

	data = g_list_nth_data(conv_placement_fncs, index);

	if (data == NULL)
		return NULL;

	return data->name;
}

gaim_conv_placement_fnc
gaim_conv_placement_get_fnc(int index)
{
	struct ConvPlacementData *data;

	ensure_default_funcs();

	if (index < 0 || index > g_list_length(conv_placement_fncs))
		return NULL;

	data = g_list_nth_data(conv_placement_fncs, index);

	if (data == NULL)
		return NULL;

	return data->fnc;
}

int
gaim_conv_placement_get_fnc_index(gaim_conv_placement_fnc fnc)
{
	struct ConvPlacementData *data;
	GList *node;
	int i;

	ensure_default_funcs();

	for (node = conv_placement_fncs, i = 0;
		 node != NULL;
		 node = node->next, i++) {

		data = (struct ConvPlacementData *)node->data;

		if (data->fnc == fnc)
			return i;
	}

	return -1;
}

int
gaim_conv_placement_get_active(void)
{
	return place_conv_index;
}

void
gaim_conv_placement_set_active(int index)
{
	gaim_conv_placement_fnc fnc;

	ensure_default_funcs();

	fnc = gaim_conv_placement_get_fnc(index);

	if (fnc == NULL)
		return;

	place_conv = fnc;
	place_conv_index = index;
}

void
gaim_set_win_ui_ops(struct gaim_window_ui_ops *ops)
{
	win_ui_ops = ops;
}

struct gaim_window_ui_ops *
gaim_get_win_ui_ops(void)
{
	return win_ui_ops;
}
