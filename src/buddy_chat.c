/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "convo.h"
#include "prpl.h"

/*#include "pixmaps/tb_forward.xpm"*/
#include "pixmaps/join.xpm"
/*#include "pixmaps/close.xpm"*/
#include "pixmaps/cancel.xpm"

GtkWidget *joinchat;
static struct gaim_connection *joinchatgc;
static GtkWidget *invite;
static GtkWidget *inviteentry;
static GtkWidget *invitemess;
static GtkWidget *jc_vbox = NULL;
static GList *chatentries = NULL;
extern int state_lock;

GList *chats = NULL;
GtkWidget *all_chats = NULL;
GtkWidget *chat_notebook = NULL;

static char *ignored(struct conversation *b, char *who)
{
	GList *ignore = b->ignored;

	while (ignore) {
		char *ign = ignore->data;
		if (!g_strcasecmp(who, ign))
			return ign;
		if (*ign == '+' && !g_strcasecmp(who, ign + 1))
			return ign;
		if (*ign == '@') {
			ign++;
			if (*ign == '+' && !g_strcasecmp(who, ign + 1))
				return ign;
			if (*ign != '+' && !g_strcasecmp(who, ign))
				return ign;
		}
		ignore = ignore->next;
	}
	return NULL;
}


static void destroy_join_chat()
{
	if (joinchat)
		gtk_widget_destroy(joinchat);
	joinchat = NULL;
}

static void destroy_invite()
{
	if (invite)
		gtk_widget_destroy(invite);
	invite = NULL;
}


void do_join_chat()
{
	if (joinchat) {
		GList *data = NULL;
		GList *tmp = chatentries;
		int *ival;
		char *sval;
		while (tmp) {
			if (gtk_object_get_user_data(tmp->data)) {
				ival = g_new0(int, 1);
				*ival = gtk_spin_button_get_value_as_int(tmp->data);
				data = g_list_append(data, ival);
			} else {
				sval = g_strdup(gtk_entry_get_text(tmp->data));
				data = g_list_append(data, sval);
			}
			tmp = tmp->next;
		}
		serv_join_chat(joinchatgc, data);

		tmp = data;
		while (tmp) {
			g_free(tmp->data);
			tmp = tmp->next;
		}
		g_list_free(data);

		gtk_widget_destroy(joinchat);
		if (chatentries)
			g_list_free(chatentries);
		chatentries = NULL;
	}
	joinchat = NULL;
}

static void rebuild_jc()
{
	GList *list, *tmp;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	if (!joinchatgc)
		return;

	while (GTK_BOX(jc_vbox)->children)
		gtk_container_remove(GTK_CONTAINER(jc_vbox),
				     ((GtkBoxChild *)GTK_BOX(jc_vbox)->children->data)->widget);
	if (chatentries)
		g_list_free(chatentries);
	chatentries = NULL;

	tmp = list = joinchatgc->prpl->chat_info(joinchatgc);
	while (list) {
		GtkWidget *label;
		GtkWidget *rowbox;
		pce = list->data;

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(jc_vbox), rowbox, TRUE, TRUE, 0);
		gtk_widget_show(rowbox);

		label = gtk_label_new(pce->label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		if (pce->is_int) {
			GtkObject *adjust;
			GtkWidget *spin;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max, 1, 10, 10);
			spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			gtk_object_set_user_data(GTK_OBJECT(spin), (void *)1);
			chatentries = g_list_append(chatentries, spin);
			gtk_widget_set_usize(spin, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), spin, FALSE, FALSE, 0);
			gtk_widget_show(spin);
		} else {
			GtkWidget *entry;
			entry = gtk_entry_new();
			chatentries = g_list_append(chatentries, entry);
			gtk_box_pack_end(GTK_BOX(rowbox), entry, FALSE, FALSE, 0);
			if (pce->def)
				gtk_entry_set_text(GTK_ENTRY(entry), pce->def);
			if (focus) {
				gtk_widget_grab_focus(entry);
				focus = FALSE;
			}
			g_signal_connect(GTK_OBJECT(entry), "activate",
					   G_CALLBACK(do_join_chat), NULL);
			gtk_widget_show(entry);
		}

		g_free(pce);
		list = list->next;
	}
	g_list_free(tmp);
}

static void joinchat_choose(GtkWidget *w, struct gaim_connection *g)
{
	if (joinchatgc == g)
		return;
	joinchatgc = g;
	rebuild_jc();
}

static void create_joinchat_menu(GtkWidget *box)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *c = connections;
	struct gaim_connection *g;
	char buf[2048];

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(box), optmenu, FALSE, FALSE, 0);

	menu = gtk_menu_new();
	joinchatgc = NULL;

	while (c) {
		g = (struct gaim_connection *)c->data;
		c = c->next;
		if (!g->prpl->join_chat)
			continue;
		if (!joinchatgc)
			joinchatgc = g;
		g_snprintf(buf, sizeof buf, "%s (%s)", g->username, g->prpl->name);
		opt = gtk_menu_item_new_with_label(buf);
		gtk_object_set_user_data(GTK_OBJECT(opt), g);
		g_signal_connect(GTK_OBJECT(opt), "activate", G_CALLBACK(joinchat_choose), g);
		gtk_menu_append(GTK_MENU(menu), opt);
		gtk_widget_show(opt);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
}

void join_chat()
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *rowbox;
	GtkWidget *bbox;
	GtkWidget *join;
	GtkWidget *cancel;
	GtkWidget *label;
	GSList *c = connections;
	struct gaim_connection *gc = NULL;

	while (c) {
		gc = c->data;
		if (gc->prpl->join_chat)
			break;
		gc = NULL;
		c = c->next;
	}
	if (gc == NULL) {
		do_error_dialog("You are not currently signed on with any protocols that have "
				"the ability to chat.", NULL, GAIM_ERROR);
		return;
	}

	if (!joinchat) {
		GAIM_DIALOG(joinchat);
		gtk_window_set_role(GTK_WINDOW(joinchat), "joinchat");
		gtk_window_set_policy(GTK_WINDOW(joinchat), FALSE, TRUE, TRUE);
		gtk_widget_realize(joinchat);
		g_signal_connect(GTK_OBJECT(joinchat), "delete_event",
				   G_CALLBACK(destroy_join_chat), joinchat);
		gtk_window_set_title(GTK_WINDOW(joinchat), _("Join Chat"));

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(joinchat), mainbox);

		frame = gtk_frame_new(_("Buddy Chat"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

#ifndef NO_MULTI
		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);

		label = gtk_label_new(_("Join Chat As:"));
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		create_joinchat_menu(rowbox);

		{
			GtkWidget *tmp = fbox;
			fbox = gtk_vbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(tmp), fbox);
			gtk_container_set_border_width(GTK_CONTAINER(fbox), 0);
			jc_vbox = fbox;
		}
#else
		joinchatgc = connections->data;
#endif
		rebuild_jc();
		/* buttons */

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		join = picture_button(joinchat, _("Join"), join_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), join, FALSE, FALSE, 0);
		g_signal_connect(GTK_OBJECT(join), "clicked", G_CALLBACK(do_join_chat), NULL);

		cancel = picture_button(joinchat, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
		g_signal_connect(GTK_OBJECT(cancel), "clicked",
				   G_CALLBACK(destroy_join_chat), joinchat);
	}
	gtk_widget_show_all(joinchat);
}


static void do_invite(GtkWidget *w, struct conversation *b)
{
	const char *buddy;
	const char *mess;

	if (!b->is_chat) {
		debug_printf("do_invite: expecting chat, got IM\n");
		return;
	}

	buddy = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(inviteentry)->entry));
	mess = gtk_entry_get_text(GTK_ENTRY(invitemess));

	if (invite) {
		serv_chat_invite(b->gc, b->id, mess, buddy);
		gtk_widget_destroy(invite);
	}
	invite = NULL;
}


GList *generate_invite_user_names(struct gaim_connection *gc)
{
	GSList *grp;
	GSList *bl;
	struct group *g;
	struct buddy *buddy;

	static GList *tmp = NULL;

	if (tmp)
		g_list_free(tmp);
	tmp = NULL;

	tmp = g_list_append(tmp, "");

	if (gc) {
		grp = gc->groups;

		while (grp) {
			g = (struct group *)grp->data;

			bl = g->members;

			while (bl) {
				buddy = (struct buddy *)bl->data;

				if (buddy->present)
					tmp = g_list_append(tmp, buddy->name);

				bl = g_slist_next(bl);
			}

			grp = g_slist_next(grp);
		}
	}

	return tmp;

}

void invite_callback(GtkWidget *w, struct conversation *b)
{
	GtkWidget *cancel;
	GtkWidget *invite_btn;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *frame;

	if (!invite) {
		GAIM_DIALOG(invite);
		gtk_widget_realize(invite);

		cancel = picture_button(invite, _("Cancel"), cancel_xpm);
		invite_btn = picture_button(invite, _("Invite"), join_xpm);
		inviteentry = gtk_combo_new();
		invitemess = gtk_entry_new();
		frame = gtk_frame_new(_("Invite"));
		table = gtk_table_new(2, 2, FALSE);

		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		gtk_container_set_border_width(GTK_CONTAINER(table), 5);

		gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

		/* Now we should fill out all of the names */
		gtk_combo_set_popdown_strings(GTK_COMBO(inviteentry), generate_invite_user_names(b->gc));

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
		gtk_container_add(GTK_CONTAINER(frame), table);

		label = gtk_label_new(_("Buddy"));
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

		label = gtk_label_new(_("Message"));
		gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

		/* Now the right side of the table */
		gtk_table_attach(GTK_TABLE(table), inviteentry, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0,
				 0);
		gtk_table_attach(GTK_TABLE(table), invitemess, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0,
				 0);

		/* And now for the button box */
		bbox = gtk_hbox_new(FALSE, 10);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

		gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(bbox), invite_btn, FALSE, FALSE, 0);

		/* Handle closes right */
		g_signal_connect(GTK_OBJECT(invite), "delete_event",
				   G_CALLBACK(destroy_invite), invite);

		g_signal_connect(GTK_OBJECT(cancel), "clicked", G_CALLBACK(destroy_invite), b);
		g_signal_connect(GTK_OBJECT(invite_btn), "clicked", G_CALLBACK(do_invite), b);
		g_signal_connect(GTK_OBJECT(GTK_ENTRY(GTK_COMBO(inviteentry)->entry)), "activate",
				   G_CALLBACK(do_invite), b);

		/* Finish up */
		gtk_widget_set_usize(GTK_WIDGET(invite), 550, 115);
		gtk_widget_show(invite_btn);
		gtk_widget_show(cancel);
		gtk_widget_show(inviteentry);
		gtk_widget_show(invitemess);
		gtk_widget_show(vbox);
		gtk_widget_show(bbox);
		gtk_widget_show(table);
		gtk_widget_show(frame);
		gtk_window_set_title(GTK_WINDOW(invite), _("Gaim - Invite Buddy Into Chat Room"));
		gtk_window_set_focus(GTK_WINDOW(invite), GTK_WIDGET(GTK_COMBO(inviteentry)->entry));
		gtk_container_add(GTK_CONTAINER(invite), vbox);


	}
	gtk_widget_show(invite);
}

void tab_complete(struct conversation *c)
{
	GtkTextIter cursor, word_start, start_buffer;
	int start;
	int most_matched = -1;
	char *entered, *partial = NULL;
	char *text;
	GList *matches = NULL;
	GList *nicks = NULL;

	gtk_text_buffer_get_start_iter(c->entry_buffer, &start_buffer);
	gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &cursor,
					 gtk_text_buffer_get_insert(c->entry_buffer));
	word_start = cursor;

	/* if there's nothing there just return */
	if (!gtk_text_iter_compare(&cursor, &start_buffer))
		return;
	
	text = gtk_text_buffer_get_text(c->entry_buffer, &start_buffer, &cursor, FALSE);

	/* if we're at the end of ": " we need to move back 2 spaces */
	start = strlen(text)-1;
	if (strlen(text)>=2 && !strncmp(&text[start-1], ": ", 2)) {
		gtk_text_iter_backward_chars(&word_start, 2);
	}

	/* find the start of the word that we're tabbing */
	while (start >= 0 && text[start] != ' ') {
		gtk_text_iter_backward_char(&word_start);
		start--;
	}
	g_free(text);

	entered = gtk_text_buffer_get_text(c->entry_buffer, &word_start, &cursor, FALSE);
	if (chat_options & OPT_CHAT_OLD_STYLE_TAB) {
		if (strlen(entered) >= 2 && !strncmp(": ", entered + strlen(entered) - 2, 2))
			entered[strlen(entered) - 2] = 0;
	}

	if (!strlen(entered)) {
		g_free(entered);
		return;
	}

	debug_printf("checking tab-completion for %s\n", entered);

	nicks = c->in_room;
	while (nicks) {
		char *nick = nicks->data;
		/* this checks to see if the current nick could be a completion */
		if (g_strncasecmp(nick, entered, strlen(entered))) {
			if (nick[0] != '+' && nick[0] != '@') {
				nicks = nicks->next;
				continue;
			}
			if (g_strncasecmp(nick + 1, entered, strlen(entered))) {
				if (nick[0] != '@' || nick[1] != '+') {
					nicks = nicks->next;
					continue;
				}
				if (g_strncasecmp(nick + 2, entered, strlen(entered))) {
					nicks = nicks->next;
					continue;
				}
				else
					nick += 2;
			} else
				nick++;
		}
		/* if we're here, it's a possible completion */
		debug_printf("possible completion: %s\n", nick);

		/* if we're doing old-style, just fill in the completion */
		if (chat_options & OPT_CHAT_OLD_STYLE_TAB) {
		        gtk_text_buffer_delete(c->entry_buffer, &word_start, &cursor);
			if (strlen(nick) == strlen(entered)) {
				nicks = nicks->next ? nicks->next : c->in_room;
				nick = nicks->data;
				if (*nick == '@')
					nick++;
				if (*nick == '+')
					nick++;
			}

			gtk_text_buffer_get_start_iter(c->entry_buffer, &start_buffer);
			gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &cursor,
							 gtk_text_buffer_get_insert(c->entry_buffer));
			if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
				char *tmp = g_strdup_printf("%s: ", nick);
				gtk_text_buffer_insert_at_cursor(c->entry_buffer, tmp, -1);
				g_free(tmp);
			} else {
				gtk_text_buffer_insert_at_cursor(c->entry_buffer, nick, -1);
			}
			g_free(entered);
			return;
		}

		/* we're only here if we're doing new style */
		if (most_matched == -1) {
			/* this will only get called once, since from now on most_matched is >= 0 */
			most_matched = strlen(nick);
			partial = g_strdup(nick);
		} else if (most_matched) {
			while (g_strncasecmp(nick, partial, most_matched))
				most_matched--;
			partial[most_matched] = 0;
		}
		matches = g_list_append(matches, nick);

		nicks = nicks->next;
	}
	/* we're only here if we're doing new style */
	
	/* if there weren't any matches, return */
	if (!matches) {
		/* if matches isn't set partials won't be either */
		g_free(entered);
		return;
	}
	
	gtk_text_buffer_delete(c->entry_buffer, &word_start, &cursor);
	if (!matches->next) {
		/* there was only one match. fill it in. */
		gtk_text_buffer_get_start_iter(c->entry_buffer, &start_buffer);
		gtk_text_buffer_get_iter_at_mark(c->entry_buffer, &cursor,
						 gtk_text_buffer_get_insert(c->entry_buffer));
		if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
			char *tmp = g_strdup_printf("%s: ", (char *)matches->data);
			gtk_text_buffer_insert_at_cursor(c->entry_buffer, tmp, -1);
			g_free(tmp);
		} else {
			gtk_text_buffer_insert_at_cursor(c->entry_buffer, matches->data, -1);
		}
		matches = g_list_remove(matches, matches->data);
	} else {
		/* there were lots of matches, fill in as much as possible and display all of them */
		char *addthis = g_malloc0(1);
		while (matches) {
			char *tmp = addthis;
			addthis = g_strconcat(tmp, matches->data, " ", NULL);
			g_free(tmp);
			matches = g_list_remove(matches, matches->data);
		}
		write_to_conv(c, addthis, WFLAG_NOLOG, NULL, time(NULL), -1);
		gtk_text_buffer_insert_at_cursor(c->entry_buffer, partial, -1);
		g_free(addthis);
	}

	g_free(entered);
	g_free(partial);
}

gboolean meify(char *message, int len)
{
	/* read /me-ify : if the message (post-HTML) starts with /me, remove
	 * the "/me " part of it (including that space) and return TRUE */
	char *c = message;
	int inside_HTML = 0;	/* i really don't like descriptive names */
	if (!c)
		return FALSE;	/* um... this would be very bad if this happens */
	if (len == -1)
		len = strlen(message);
	while (*c) {
		if (inside_HTML) {
			if (*c == '>')
				inside_HTML = 0;
		} else {
			if (*c == '<')
				inside_HTML = 1;
			else
				break;
		}
		c++;		/* i really don't like c++ either */
		len--;
	}
	/* k, so now we've gotten past all the HTML who. */
	if (!*c)
		return FALSE;
	if (!g_strncasecmp(c, "/me ", 4)) {
		memmove(c, c + 4, len - 3);
		return TRUE;
	} else
		return FALSE;
}

static gboolean find_nick(struct gaim_connection *gc, char *message)
{
	char *msg = g_strdup(message), *who, *p;
	int n;
	g_strdown(msg);

	who = g_strdup(gc->username);
	n = strlen(who);
	g_strdown(who);

	if ((p = strstr(msg, who)) != NULL) {
		if (((p == msg) || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
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
	n = strlen(who);
	g_strdown(who);

	if (n > 0 && (p = strstr(msg, who)) != NULL) {
		if (((p == msg) || !isalnum(*(p - 1))) && !isalnum(*(p + n))) {
			g_free(who);
			g_free(msg);
			return TRUE;
		}
	}
	g_free(who);
	g_free(msg);
	return FALSE;
}

void chat_write(struct conversation *b, char *who, int flag, char *message, time_t mtime)
{
	char *str;

	if (!b->is_chat) {
		debug_printf("chat_write: expecting chat, got IM\n");
		return;
	}

	if (ignored(b, who))
		return;

	if (!(flag & WFLAG_WHISPER)) {
		str = g_strdup(normalize (who));
		if (!g_strcasecmp(str, normalize(b->gc->username))) {
			if (b->makesound)
				play_sound(SND_CHAT_YOU_SAY);
			flag |= WFLAG_SEND;
		} else if (!g_strcasecmp(str, normalize(b->gc->displayname))) {
			if (b->makesound)
				play_sound(SND_CHAT_YOU_SAY);
			flag |= WFLAG_SEND;
		} else {
		       	flag |= WFLAG_RECV;
			if (find_nick(b->gc, message))
				flag |= WFLAG_NICK;
		}
		g_free(str);
	}

	if (flag & WFLAG_RECV && b->makesound) {
		if (flag & WFLAG_NICK && (sound_options & OPT_SOUND_CHAT_NICK)) {
			play_sound(SND_CHAT_NICK);
		} else {
			play_sound(SND_CHAT_SAY);
		}
	}

	if (chat_options & OPT_CHAT_COLORIZE) 
		flag |= WFLAG_COLORIZE;
	write_to_conv(b, message, flag, who, mtime, -1);
}

static gint insertname(gconstpointer one, gconstpointer two)
{
	const char *a = (const char *)one;
	const char *b = (const char *)two;

	if (*a == '@') {
		if (*b != '@')
			return -1;
		return (strcasecmp(a + 1, b + 1));
	} else if (*a == '+') {
		if (*b == '@')
			return 1;
		if (*b != '+')
			return -1;
		return (strcasecmp(a + 1, b + 1));
	} else {
		if (*b == '@' || *b == '+')
			return 1;
		return strcasecmp(a, b);
	}
}

static void chat_press_im(GtkObject *obj, struct conversation *b)
{
	struct conversation *c;

	c = find_conversation(gtk_object_get_user_data(obj));

	if (c != NULL)
		gdk_window_show(c->window->window);
	else {
		c = new_conversation(gtk_object_get_user_data(obj));
		set_convo_gc(c, b->gc);
	}
}

static void chat_press_ign(GtkWidget *obj, struct conversation *b)
{
	ignore_callback(obj, b);
}

static void chat_press_info(GtkObject *obj, struct conversation *b)
{
	if (b->gc) {
		/*
		 * If there are special needs for getting info on users in
		 * buddy chat "rooms"...
		 */
		if(b->gc->prpl->get_cb_info != NULL) {
			b->gc->prpl->get_cb_info(b->gc, b->id, gtk_object_get_user_data(obj));
		} else {
			b->gc->prpl->get_info(b->gc, gtk_object_get_user_data(obj));
		}
	}
}


static void chat_press_away(GtkObject *obj, struct conversation *b)
{
	if (b->gc) {
		/*
		 * May want to expand this to work similarly to chat_press_info?
		 */
		if(b->gc->prpl->get_cb_away != NULL) {
			b->gc->prpl->get_cb_away(b->gc, b->id, gtk_object_get_user_data(obj));
		}
	}
}

/* Added by Jonas <jonas@birme.se> */
static void chat_press_add(GtkObject *obj, struct conversation *c)
{
	char *name = gtk_object_get_user_data(obj);
	struct buddy *b = find_buddy(c->gc, name);

	if (b) {
		show_confirm_del(c->gc, name);
	} else if (c->gc)
		show_add_buddy(c->gc, name, NULL, NULL);
	
	gtk_widget_grab_focus(c->entry);	
}
/* End Jonas */

static gint right_click_chat(GtkObject *obj, GdkEventButton *event, struct conversation *b)
{
	GtkTreePath *path;
	int x;
	int y;
	GtkTreeIter iter;
	GtkTreeModel *mod;
	GtkTreeViewColumn *column;
	gchar *who;

	
	mod = gtk_tree_view_get_model(GTK_TREE_VIEW(b->list));
	
	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(b->list), event->x, event->y, &path, &column, &x, &y);
	
	if (path == NULL)
			return FALSE;

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(gtk_tree_view_get_selection(GTK_TREE_VIEW(b->list))), path);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(mod), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &who, -1);

	if ((event->button == 1) && (event->type == GDK_2BUTTON_PRESS)) {
		struct conversation *c;

		if ((c = find_conversation(who)) == NULL)
			c = new_conversation(who);

		set_convo_gc(c, b->gc);
	} else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		static GtkWidget *menu = NULL;
		GtkWidget *button;

		/*
		 * If a menu already exists, destroy it before creating a new one,
		 * thus freeing-up the memory it occupied.
		 */

		if(menu)
			gtk_widget_destroy(menu);

		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		g_signal_connect(GTK_OBJECT(button), "activate", G_CALLBACK(chat_press_im), b);
		gtk_object_set_user_data(GTK_OBJECT(button), who);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (ignored(b, gtk_object_get_user_data(obj)))
			button = gtk_menu_item_new_with_label(_("Un-Ignore"));
		else
			button = gtk_menu_item_new_with_label(_("Ignore"));
		
		g_signal_connect(GTK_OBJECT(button), "activate", G_CALLBACK(chat_press_ign), b);
		gtk_object_set_user_data(GTK_OBJECT(button), who);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (b->gc && b->gc->prpl->get_info) {
			button = gtk_menu_item_new_with_label(_("Info"));
			g_signal_connect(GTK_OBJECT(button), "activate",
					   G_CALLBACK(chat_press_info), b);
			gtk_object_set_user_data(GTK_OBJECT(button), who);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		if (b->gc && b->gc->prpl->get_cb_away) {
			button = gtk_menu_item_new_with_label(_("Get Away Msg"));
			g_signal_connect(GTK_OBJECT(button), "activate",
					   G_CALLBACK(chat_press_away), b);
			gtk_object_set_user_data(GTK_OBJECT(button), who);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		/* Added by Jonas <jonas@birme.se> */
		if (b->gc) {
			if (find_buddy(b->gc, who))
				button = gtk_menu_item_new_with_label(_("Remove"));
			else
				button = gtk_menu_item_new_with_label(_("Add"));
			g_signal_connect(GTK_OBJECT(button), "activate",
					   G_CALLBACK(chat_press_add), b);
			gtk_object_set_user_data(GTK_OBJECT(button), 
						 who);
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}
		/* End Jonas */
		
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}

	return TRUE;
}

/*
 * Common code for adding a chat buddy to the list
 */
static void add_chat_buddy_common(struct conversation *b, char *name, int pos)
{
	GtkTreeIter iter;
	GtkListStore *ls;


	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(b->list)));

	gtk_list_store_append(ls, &iter);
	gtk_list_store_set(ls, &iter, 0, ignored(b, name) ? "X" : " ", 1, name, -1);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls), 1, GTK_SORT_ASCENDING);
}

void add_chat_buddy(struct conversation *b, char *buddy, char *extra_msg)
{
	char *name = g_strdup(buddy);
	char tmp[BUF_LONG];
	int pos;

	plugin_event(event_chat_buddy_join, b->gc, b->id, name);
	b->in_room = g_list_insert_sorted(b->in_room, name, insertname);
	pos = g_list_index(b->in_room, name);

	add_chat_buddy_common(b, name, pos);

	g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(b->in_room),
		   g_list_length(b->in_room) == 1 ? "person" : "people");
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound)
		play_sound(SND_CHAT_JOIN);

	if (chat_options & OPT_CHAT_LOGON) {
		if (!extra_msg)
			g_snprintf(tmp, sizeof(tmp), _("%s entered the room."), name);
		else
			g_snprintf(tmp, sizeof(tmp), _("%s [<I>%s</I>] entered the room."), name, 
				   extra_msg);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL), -1);
	}
}


void rename_chat_buddy(struct conversation *b, char *old, char *new)
{
	GList *names = b->in_room;
	char *name = g_strdup(new);
	char *ign;
	int pos;
	char tmp[BUF_LONG];
	GtkTreeIter iter;
	GtkTreeModel *mod;
	int f = 1;

	while (names) {
		if (!g_strcasecmp((char *)names->data, old)) {
			char *tmp2 = names->data;
			b->in_room = g_list_remove(b->in_room, names->data);
			
			mod = gtk_tree_view_get_model(GTK_TREE_VIEW(b->list));	
			
			if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mod), &iter))
				break;

			while (f != 0) {
				gchar *val;

				gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &val, -1);

			
				if (!g_strcasecmp(old, val)) 
					gtk_list_store_remove(GTK_LIST_STORE(mod), &iter);

				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(mod), &iter);

				g_free(val);
			}

			g_free(tmp2);
			break;
		}
		names = names->next;
	}

	if (!names) {
		g_free(name);
		return;
	}

	b->in_room = g_list_insert_sorted(b->in_room, name, insertname);
	pos = g_list_index(b->in_room, name);

	ign = ignored(b, old);

	if (ign) {
		g_free(ign);
		b->ignored = g_list_remove(b->ignored, ign);

		/* we haven't added them yet. if we find them, don't add them again */
		if (!ignored(b, new))
			b->ignored = g_list_append(b->ignored, g_strdup(name));

	} else {
		if ((ign = ignored(b, new)) != NULL) {
			/* if they weren't ignored and change to someone who is ignored,
			 * assume it's someone else. that may seem a little backwards but
			 * it's better when it *is* actually someone else. Sorry Sean. */
			g_free(ign);
			b->ignored = g_list_remove(b->ignored, ign);
		}
	}

	add_chat_buddy_common(b, name, pos);

	if (chat_options & OPT_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("%s is now known as %s"), old, new);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL), -1);
	}
}


void remove_chat_buddy(struct conversation *b, char *buddy, char *reason)
{
	GList *names = b->in_room;
	char tmp[BUF_LONG];
	GtkTreeIter iter;
	GtkTreeModel *mod;
	int f = 1;

	plugin_event(event_chat_buddy_leave, b->gc, b->id, buddy);

	while (names) {
		if (!g_strcasecmp((char *)names->data, buddy)) {
			b->in_room = g_list_remove(b->in_room, names->data);

			mod = gtk_tree_view_get_model(GTK_TREE_VIEW(b->list));	
			
			if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mod), &iter))
				break;

			while (f != 0) {
				gchar *val;

				gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &val, -1);

			
				if (!g_strcasecmp(buddy, val)) 
					gtk_list_store_remove(GTK_LIST_STORE(mod), &iter);

				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(mod), &iter);

				g_free(val);
			}

			break;
		}

		names = names->next;
	}

	if (!names)
		return;

	/* don't remove them from ignored in case they re-enter */
	g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(b->in_room),
		   g_list_length(b->in_room) == 1 ? "person" : "people");
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound)
		play_sound(SND_CHAT_LEAVE);

	if (chat_options & OPT_CHAT_LOGON) {
		if (reason && *reason)
			g_snprintf(tmp, sizeof(tmp), _("%s left the room (%s)."), buddy, reason);
		else
			g_snprintf(tmp, sizeof(tmp), _("%s left the room."), buddy);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL), -1);
	}
}


void im_callback(GtkWidget *w, struct conversation *b)
{
	gchar *name;
	struct conversation *c;
	GtkTreeIter iter;
	GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(b->list));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(b->list));

	if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &name, -1);
	} else {
		return;
	}

	if (*name == '@')
		name++;
	if (*name == '+')
		name++;

	c = find_conversation(name);

	if (c != NULL) {
		gdk_window_raise(c->window->window);
	} else {
		c = new_conversation(name);
	}

	set_convo_gc(c, b->gc);
}

void ignore_callback(GtkWidget *w, struct conversation *b)
{
	GtkTreeIter iter;
	GtkTreeModel *mod = gtk_tree_view_get_model(GTK_TREE_VIEW(b->list));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(b->list));
	char *name;
	char *ign;
	int pos;

	if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(mod), &iter, 1, &name, -1);
		gtk_list_store_remove(GTK_LIST_STORE(mod), &iter);
	} else {
		return;
	}

	pos = g_list_index(b->in_room, name);

	ign = ignored(b, name);

	if (ign) {
		g_free(ign);
		b->ignored = g_list_remove(b->ignored, ign);
	} else {
		b->ignored = g_list_append(b->ignored, g_strdup(name));
	}

	add_chat_buddy_common(b, name, pos);
}

void show_new_buddy_chat(struct conversation *b)
{
	GtkWidget *win;
	GtkWidget *cont;
	GtkWidget *text;
	/*GtkWidget *close;*/
	GtkWidget *frame;
	GtkWidget *chatentry;
	GtkWidget *lbox;
	GtkWidget *bbox;
	GtkWidget *bbox2;
	GtkWidget *button;
	GtkWidget *sw;
	GtkWidget *sw2;
	GtkWidget *vbox;
	GtkWidget *vpaned;
	GtkWidget *hpaned;
	GtkWidget *toolbar;
	GtkWidget *sep;
	GtkListStore *ls;
	GtkWidget *list;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkWidget *tabby;

	char buf[BUF_LONG];

	int dispstyle = set_dispstyle(1);

	if (chat_options & OPT_CHAT_ONE_WINDOW) {
		if (!all_chats) {
			GtkWidget *testidea;

			win = all_chats = b->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			if ((convo_options & OPT_CONVO_COMBINE) && (im_options & OPT_IM_ONE_WINDOW))
				all_convos = all_chats;
			gtk_window_set_role(GTK_WINDOW(win), "buddy_chat");
			gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, FALSE);
			gtk_container_border_width(GTK_CONTAINER(win), 0);
			gtk_widget_realize(win);
			gtk_window_set_title(GTK_WINDOW(win), _("Gaim - Group Chats"));
			g_signal_connect(GTK_OBJECT(win), "delete_event",
					   G_CALLBACK(delete_all_convo), NULL);

			chat_notebook = gtk_notebook_new();
			if ((convo_options & OPT_CONVO_COMBINE) && (im_options & OPT_IM_ONE_WINDOW))
				convo_notebook = chat_notebook;
			if (chat_options & OPT_CHAT_SIDE_TAB) {
				if (chat_options & OPT_CHAT_BR_TAB) {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook),
								 GTK_POS_RIGHT);
				} else {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook),
								 GTK_POS_LEFT);
				}
			} else {
				if (chat_options & OPT_CHAT_BR_TAB) {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook),
								 GTK_POS_BOTTOM);
				} else {
					gtk_notebook_set_tab_pos(GTK_NOTEBOOK(chat_notebook),
								 GTK_POS_TOP);
				}
			}
			
			testidea = gtk_vbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(testidea), chat_notebook, TRUE, TRUE, 0);
			gtk_widget_show(testidea);

			gtk_notebook_set_scrollable(GTK_NOTEBOOK(chat_notebook), TRUE);
			gtk_notebook_popup_enable(GTK_NOTEBOOK(chat_notebook));
			gtk_container_add(GTK_CONTAINER(win), testidea);
			g_signal_connect_after(GTK_OBJECT(chat_notebook), "switch-page",
					   G_CALLBACK(convo_switch), NULL);
			gtk_widget_show(chat_notebook);
		} else
			win = b->window = all_chats;

		cont = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(cont), 5);

		tabby = gtk_hbox_new(FALSE, 5);
		b->close = gtk_button_new();
		gtk_widget_set_size_request(GTK_WIDGET(b->close), 16, 16);
		gtk_container_add(GTK_CONTAINER(b->close), gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
		gtk_button_set_relief(GTK_BUTTON(b->close), GTK_RELIEF_NONE);
		b->tab_label = gtk_label_new(b->name);

		g_signal_connect(GTK_OBJECT(b->close), "clicked", G_CALLBACK(close_callback), b);

		gtk_box_pack_start(GTK_BOX(tabby), b->tab_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(tabby), b->close, FALSE, FALSE, 0);
		gtk_widget_show_all(tabby);
		gtk_notebook_append_page(GTK_NOTEBOOK(chat_notebook), cont, tabby);
		gtk_notebook_set_menu_label_text(GTK_NOTEBOOK(chat_notebook), cont,
				b->name);
		gtk_widget_show(cont);
	} else {
		win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		b->window = win;
		gtk_object_set_user_data(GTK_OBJECT(win), b);
		gtk_window_set_role(GTK_WINDOW(win), "buddy_chat");
		gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
		gtk_container_border_width(GTK_CONTAINER(win), 10);
		gtk_widget_realize(win);
		g_snprintf(buf, sizeof(buf), "Gaim - %s (chat)", b->name);
		gtk_window_set_title(GTK_WINDOW(win), buf);
		g_signal_connect(GTK_OBJECT(win), "destroy", G_CALLBACK(close_callback), b);

		cont = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(win), cont);
		gtk_widget_show(cont);
	}

	if (b->gc->prpl->options & OPT_PROTO_CHAT_TOPIC) {
		GtkWidget *hbox;
		GtkWidget *label;

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(cont), hbox, FALSE, FALSE, 5);
		gtk_widget_show(hbox);

		label = gtk_label_new(_("Topic:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		b->topic_text = gtk_entry_new();
		gtk_entry_set_editable(GTK_ENTRY(b->topic_text), FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), b->topic_text, TRUE, TRUE, 5);
		gtk_widget_show(b->topic_text);
	}

	vpaned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(vpaned), 15);
	gtk_container_add(GTK_CONTAINER(cont), vpaned);
	gtk_widget_show(vpaned);

	hpaned = gtk_hpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(hpaned), 15);
	gtk_paned_pack1(GTK_PANED(vpaned), hpaned, TRUE, FALSE);
	gtk_widget_show(hpaned);

	sw = gtk_scrolled_window_new(NULL, NULL);
	b->sw = sw;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_paned_pack1(GTK_PANED(hpaned), sw, TRUE, TRUE);
	gtk_widget_set_usize(sw, buddy_chat_size.width, buddy_chat_size.height);
	gtk_widget_show(sw);

	text = gtk_imhtml_new(NULL, NULL);
	b->text = text;
	gtk_container_add(GTK_CONTAINER(sw), text);
	if (convo_options & OPT_CONVO_SHOW_TIME)
		gtk_imhtml_show_comments(GTK_IMHTML(text), TRUE);
	gaim_setup_imhtml(text);
	gtk_widget_show(text);

	lbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, TRUE, TRUE);
	gtk_widget_show(lbox);

	b->count = gtk_label_new(_("0 people in room"));
	gtk_box_pack_start(GTK_BOX(lbox), b->count, FALSE, FALSE, 0);
	gtk_widget_show(b->count);

	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(lbox), sw2, TRUE, TRUE, 0);
	gtk_widget_show(sw2);

	ls = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls), 1, GTK_SORT_ASCENDING);
	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL, rend, "text", 0, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
	g_signal_connect(GTK_OBJECT(list), "button_press_event",
		G_CALLBACK(right_click_chat), b);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	col = gtk_tree_view_column_new_with_attributes(NULL, rend, "text", 1, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
	g_signal_connect(GTK_OBJECT(list), "button_press_event",
		G_CALLBACK(right_click_chat), b);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	gtk_widget_set_usize(list, 150, -1);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_widget_show(list);
	b->list = list;
	
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list);

	bbox2 = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(lbox), bbox2, FALSE, FALSE, 0);
	gtk_widget_show(bbox2);

	button = gaim_pixbuf_button_from_stock(NULL, "gtk-redo", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox2), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(im_callback), b);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(NULL, "gtk-dialog-error", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox2), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(ignore_callback), b);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(NULL, "gtk-find", GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox2), button, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(info_callback), b);
	gtk_widget_show(button);
	b->info = button;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox, TRUE, FALSE);
	gtk_widget_show(vbox);

	toolbar = build_conv_toolbar(b);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
	gtk_widget_show(frame);
	
	b->entry_buffer = gtk_text_buffer_new(NULL);
	g_object_set_data(G_OBJECT(b->entry_buffer), "user_data", b);
	chatentry = gtk_text_view_new_with_buffer(b->entry_buffer);
	b->entry = chatentry;
	if (!(chat_options & OPT_CHAT_ONE_WINDOW)
			|| ((gtk_notebook_get_current_page(GTK_NOTEBOOK(chat_notebook)) == 0)
				&& (b == g_list_nth_data(chats, 0))))
		gtk_widget_grab_focus(b->entry);


	b->makesound = 1; /* Need to do this until we get a menu */

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(b->entry), GTK_WRAP_WORD);
	g_signal_connect(G_OBJECT(b->entry), "key_press_event", G_CALLBACK(keypress_callback), b);
	g_signal_connect_after(G_OBJECT(b->entry), "button_press_event", 
			       G_CALLBACK(stop_rclick_callback), NULL);
	g_signal_connect_swapped(G_OBJECT(chatentry), "key_press_event", 
				 G_CALLBACK(entry_key_pressed), chatentry);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(chatentry));
	gtk_widget_set_usize(chatentry, buddy_chat_size.width, MAX(buddy_chat_size.entry_height, 25));
	gtk_window_set_focus(GTK_WINDOW(win), chatentry);
	gtk_widget_show(chatentry);
#ifdef USE_GTKSPELL
	if (convo_options & OPT_CONVO_CHECK_SPELLING)
		gtkspell_attach(GTK_TEXT_VIEW(chatentry));
#endif
	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	/* 
	close = picture_button2(win, _("Close"), cancel_xpm, dispstyle);
	b->close = close;
	gtk_object_set_user_data(GTK_OBJECT(close), b);
	g_signal_connect(GTK_OBJECT(close), "clicked", G_CALLBACK(close_callback), b);
	gtk_box_pack_end(GTK_BOX(bbox), close, dispstyle, dispstyle, 0);
	*/

	/* Send */
	button = gaim_pixbuf_button_from_stock(
				(dispstyle == 0 ? NULL : _("Send")),
				(dispstyle == 1 ? NULL : "gtk-convert"),
				GAIM_BUTTON_VERTICAL);

	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(send_callback), b);
	gtk_widget_show(button);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	b->send = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(bbox), sep, FALSE, FALSE, 0);

	/* Invite */
	button = gaim_pixbuf_button_from_stock(
				(dispstyle == 0 ? NULL : _("Invite")),
				(dispstyle == 1 ? NULL : "gtk-jump-to"),
				GAIM_BUTTON_VERTICAL);

	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(invite_callback), b);
	gtk_widget_show(button);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	b->invite = button;


	b->font_dialog = NULL;
	b->fg_color_dialog = NULL;
	b->bg_color_dialog = NULL;
	b->smiley_dialog = NULL;
	b->link_dialog = NULL;
	b->log_dialog = NULL;
	sprintf(b->fontface, "%s", fontface);
	b->hasfont = 0;
	b->bgcol = bgcolor;
	b->hasbg = 0;
	b->fgcol = fgcolor;
	b->hasfg = 0;

	update_buttons_by_protocol(b);

	gtk_widget_show(win);
}

void chat_set_topic(struct conversation *b, char *who, char *topic)
{
	gtk_entry_set_text(GTK_ENTRY(b->topic_text), topic);
	if (b->topic)
		g_free(b->topic);
	b->topic = g_strdup(topic);
}



void delete_chat(struct conversation *b)
{
	while (b->in_room) {
		g_free(b->in_room->data);
		b->in_room = g_list_remove(b->in_room, b->in_room->data);
	}
	while (b->ignored) {
		g_free(b->ignored->data);
		b->ignored = g_list_remove(b->ignored, b->ignored->data);
	}
	g_string_free(b->history, TRUE);
	if (b->topic)
		g_free(b->topic);
	g_free(b);
}

static GtkWidget *change_text(GtkWidget *win, char *text, GtkWidget *button, char *stock, int chat)
{
	int dispstyle = set_dispstyle(chat);
	gtk_widget_destroy(button);
	/* XXX button = picture_button2(win, text, xpm, dispstyle); */
	button = gaim_pixbuf_button_from_stock((dispstyle == 0 ? NULL : text),
										   (dispstyle == 1 ? NULL : stock),
										   GAIM_BUTTON_VERTICAL);
#if 0
	if (chat == 1)
		gtk_box_pack_start(GTK_BOX(parent), button, dispstyle, dispstyle, 5);
	else
		gtk_box_pack_end(GTK_BOX(parent), button, dispstyle, dispstyle, 0);

	gtk_box_pack_start(GTK_BOX(parent), button, dispstyle, dispstyle, 0);
#endif

	gtk_widget_show(button);
	return button;
}

void update_chat_button_pix()
{
	GSList *C = connections;
	struct gaim_connection *g;
	GtkWidget *parent;

	while (C) {
		GSList *bcs;
		struct conversation *c;
		int opt = 1;
		g = (struct gaim_connection *)C->data;
		bcs = g->buddy_chats;

		while (bcs) {
			c = (struct conversation *)bcs->data;
			parent = c->send->parent;

			c->send = change_text(c->window, _("Send"), c->send, "gtk-convert", opt);
			c->invite = change_text(c->window, _("Invite"), c->invite, "gtk-jump-to", opt);
			gtk_box_pack_end(GTK_BOX(parent), c->send, FALSE, FALSE, 0);
			gtk_box_pack_end(GTK_BOX(parent), c->invite, FALSE, FALSE, 0);

			g_signal_connect(GTK_OBJECT(c->send), "clicked",
					   G_CALLBACK(send_callback), c);
			g_signal_connect(GTK_OBJECT(c->invite), "clicked",
					   G_CALLBACK(invite_callback), c);

			gtk_button_set_relief(GTK_BUTTON(c->send), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(c->invite), GTK_RELIEF_NONE);

			update_buttons_by_protocol(c);

			bcs = bcs->next;
		}
		C = C->next;
	}
}

void update_im_button_pix()
{
	GList *bcs = conversations;
	struct conversation *c;
	GtkWidget *parent;
	int opt = 0;

	while (bcs) {
		c = (struct conversation *)bcs->data;
		parent = c->send->parent;

		c->send = change_text(c->window, _("Send"), c->send, "gtk-convert", opt);
		gtk_box_pack_end(GTK_BOX(parent), c->send, FALSE, FALSE, 0);

		gtk_widget_destroy(c->sep2);
		c->sep2 = gtk_vseparator_new();
		gtk_box_pack_end(GTK_BOX(parent), c->sep2, FALSE, TRUE, 0);
		gtk_widget_show(c->sep2);

		if (find_buddy(c->gc, c->name) == NULL)
			c->add = change_text(c->window, _("Add"), c->add, "gtk-add", opt);
		else
			c->add = change_text(c->window, _("Remove"), c->add, "gtk-remove", opt);

		gtk_box_pack_start(GTK_BOX(parent), c->add, FALSE, FALSE, 0);

		c->warn = change_text(c->window, _("Warn"), c->warn, "gtk-dialog-warning", opt);
		gtk_box_pack_start(GTK_BOX(parent), c->warn, FALSE, FALSE, 0);

		c->info = change_text(c->window, _("Info"), c->info, "gtk-find", opt);
		gtk_box_pack_start(GTK_BOX(parent), c->info, FALSE, FALSE, 0);

		c->block = change_text(c->window, _("Block"), c->block, "gtk-stop", opt);
		gtk_box_pack_start(GTK_BOX(parent), c->block, FALSE, FALSE, 0);

		gtk_button_set_relief(GTK_BUTTON(c->info), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(c->add), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(c->warn), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(c->send), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(c->block), GTK_RELIEF_NONE);

		gtk_size_group_add_widget(c->sg, c->info);
		gtk_size_group_add_widget(c->sg, c->add);
		gtk_size_group_add_widget(c->sg, c->warn);
		gtk_size_group_add_widget(c->sg, c->send);
		gtk_size_group_add_widget(c->sg, c->block);

		gtk_box_reorder_child(GTK_BOX(parent), c->warn, 1);
		gtk_box_reorder_child(GTK_BOX(parent), c->block, 2);
		gtk_box_reorder_child(GTK_BOX(parent), c->info, 4);


		update_buttons_by_protocol(c);

		g_signal_connect(GTK_OBJECT(c->send), "clicked", G_CALLBACK(send_callback), c);
		g_signal_connect(GTK_OBJECT(c->info), "clicked", G_CALLBACK(info_callback), c);
		g_signal_connect(GTK_OBJECT(c->warn), "clicked", G_CALLBACK(warn_callback), c);
		g_signal_connect(GTK_OBJECT(c->block), "clicked", G_CALLBACK(block_callback), c);
		bcs = bcs->next;
	}
}

void chat_tabize()
{
	int pos = 0;
	char tmp[BUF_LONG];
	/* evil, evil i tell you! evil! */
	if (chat_options & OPT_CHAT_ONE_WINDOW) {
		GList *x = chats;
		if ((convo_options & OPT_CONVO_COMBINE) && (im_options & OPT_IM_ONE_WINDOW)) {
			all_chats = all_convos;
			chat_notebook = convo_notebook;
		}
		while (x) {
			struct conversation *c = x->data;
			GtkWidget *imhtml, *win;
			GList *r = c->in_room;

			imhtml = c->text;
			win = c->window;
			show_new_buddy_chat(c);
			gtk_widget_destroy(c->text);
			gtk_widget_reparent(imhtml, c->sw);
			c->text = imhtml;
			gtk_signal_disconnect_by_func(GTK_OBJECT(win),
						      G_CALLBACK(close_callback), c);
			gtk_widget_destroy(win);

			if (c->topic)
				gtk_entry_set_text(GTK_ENTRY(c->topic_text), c->topic);

			g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(c->in_room),
				   g_list_length(c->in_room) == 1 ? "person" : "people");
			gtk_label_set_text(GTK_LABEL(c->count), tmp);

			while (r) {
				char *name = r->data;

				add_chat_buddy_common(c, name, pos);

				r = r->next;
				pos++;
			}

			x = x->next;
		}
	} else {
		GList *x, *m;
		x = m = chats;
		chats = NULL;
		while (x) {
			struct conversation *c = x->data;
			GtkWidget *imhtml;
			GList *r = c->in_room;

			imhtml = c->text;
			show_new_buddy_chat(c);
			gtk_widget_destroy(c->text);
			gtk_widget_reparent(imhtml, c->sw);
			c->text = imhtml;

			if (c->topic)
				gtk_entry_set_text(GTK_ENTRY(c->topic_text), c->topic);

			g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(c->in_room),
				   g_list_length(c->in_room) == 1 ? "person" : "people");
			gtk_label_set_text(GTK_LABEL(c->count), tmp);

			while (r) {
				char *name = r->data;

				add_chat_buddy_common(c, name, pos);

				r = r->next;
				pos++;
			}

			x = x->next;
		}
		chats = m;
		if ((convo_options & OPT_CONVO_COMBINE) &&
		    (im_options & OPT_IM_ONE_WINDOW) && conversations) {
			while (m) {
				gtk_notebook_remove_page(GTK_NOTEBOOK(chat_notebook),
							 g_list_length(conversations));
				m = m->next;
			}
		} else if (all_chats)
			gtk_widget_destroy(all_chats);
		all_chats = NULL;
		chat_notebook = NULL;
	}
}
