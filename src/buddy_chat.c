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
#include "gtkspell.h"
#include <gdk/gdkkeysyms.h>

#include "convo.h"
#include "prpl.h"

#include "pixmaps/tb_forward.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/close.xpm"

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

	tmp = list = (*joinchatgc->prpl->chat_info)(joinchatgc);
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
			gtk_signal_connect(GTK_OBJECT(entry), "activate",
					   GTK_SIGNAL_FUNC(do_join_chat), NULL);
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
		g_snprintf(buf, sizeof buf, "%s (%s)", g->username, (*g->prpl->name)());
		opt = gtk_menu_item_new_with_label(buf);
		gtk_object_set_user_data(GTK_OBJECT(opt), g);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(joinchat_choose), g);
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
				"the ability to chat.", "Unable to chat");
		return;
	}

	if (!joinchat) {
		joinchat = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_wmclass(GTK_WINDOW(joinchat), "joinchat", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(joinchat), FALSE, TRUE, TRUE);
		gtk_widget_realize(joinchat);
		gtk_signal_connect(GTK_OBJECT(joinchat), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_join_chat), joinchat);
		gtk_window_set_title(GTK_WINDOW(joinchat), _("Join Chat"));
		aol_icon(joinchat->window);

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

		cancel = picture_button(joinchat, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
				   GTK_SIGNAL_FUNC(destroy_join_chat), joinchat);

		join = picture_button(joinchat, _("Join"), join_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), join, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(join), "clicked", GTK_SIGNAL_FUNC(do_join_chat), NULL);
	}
	gtk_widget_show_all(joinchat);
}


static void do_invite(GtkWidget *w, struct conversation *b)
{
	char *buddy;
	char *mess;

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
		invite = gtk_window_new(GTK_WINDOW_DIALOG);
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
		gtk_signal_connect(GTK_OBJECT(invite), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_invite), invite);

		gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_invite), b);
		gtk_signal_connect(GTK_OBJECT(invite_btn), "clicked", GTK_SIGNAL_FUNC(do_invite), b);
		gtk_signal_connect(GTK_OBJECT(GTK_ENTRY(GTK_COMBO(inviteentry)->entry)), "activate",
				   GTK_SIGNAL_FUNC(do_invite), b);

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

		aol_icon(invite->window);

	}
	gtk_widget_show(invite);
}

void tab_complete(struct conversation *c)
{
	int pos = GTK_EDITABLE(c->entry)->current_pos;
	int start = pos;
	int most_matched = -1;
	char *entered, *partial = NULL;
	char *text;
	GList *matches = NULL;
	GList *nicks = c->in_room;

	/* if there's nothing there just return */
	if (!start)
		return;
	
	text = gtk_editable_get_chars(GTK_EDITABLE(c->entry), 0, pos);

	/* if we're at the end of ": " we need to move back 2 spaces */
	if (start >= 2 && text[start - 1] == ' ' && text[start - 2] == ':')
		start -= 2;
	
	/* find the start of the word that we're tabbing */
	while (start > 0 && text[start - 1] != ' ')
		start--;

	entered = text + start;
	if (chat_options & OPT_CHAT_OLD_STYLE_TAB) {
		if (strlen(entered) >= 2 && !strncmp(": ", entered + strlen(entered) - 2, 2))
			entered[strlen(entered) - 2] = 0;
	}
		
	if (!strlen(entered)) {
		g_free(text);
		return;
	}

	debug_printf("checking tab-completion for %s\n", entered);

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
		        gtk_editable_delete_text(GTK_EDITABLE(c->entry), start, pos);
			if (strlen(nick) == strlen(entered)) {
				nicks = nicks->next ? nicks->next : c->in_room;
				nick = nicks->data;
				if (*nick == '@')
					nick++;
				if (*nick == '+')
					nick++;
			}

			if (start == 0) {
				char *tmp = g_strdup_printf("%s: ", nick);
				int t = start;
				gtk_editable_insert_text(GTK_EDITABLE(c->entry), tmp, strlen(tmp), &start);
				if (t == start) {
					t = start + strlen(tmp);
					gtk_editable_set_position(GTK_EDITABLE(c->entry), t);
				}
				g_free(tmp);
			} else {
				int t = start;
				gtk_editable_insert_text(GTK_EDITABLE(c->entry), nick, strlen(nick), &start);
				if (t == start) {
					t = start + strlen(nick);
					gtk_editable_set_position(GTK_EDITABLE(c->entry), t);
				}
			}
			g_free(text);
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
		g_free(text);
		return;
	}
	
	gtk_editable_delete_text(GTK_EDITABLE(c->entry), start, pos);
	if (!matches->next) {
		/* there was only one match. fill it in. */
		if (start == 0) {
			char *tmp = g_strdup_printf("%s: ", (char *)matches->data);
			int t = start;
			gtk_editable_insert_text(GTK_EDITABLE(c->entry), tmp, strlen(tmp), &start);
			if (t == start) {
				t = start + strlen(tmp);
				gtk_editable_set_position(GTK_EDITABLE(c->entry), t);
			}
			g_free(tmp);
		} else {
			gtk_editable_insert_text(GTK_EDITABLE(c->entry), matches->data, strlen(matches->data), &start);
		}
		matches = g_list_remove(matches, matches->data);
	} else {
		/* there were lots of matches, fill in as much as possible and display all of them */
		char *addthis = g_malloc0(1);
		int t = start;
		while (matches) {
			char *tmp = addthis;
			addthis = g_strconcat(tmp, matches->data, " ", NULL);
			g_free(tmp);
			matches = g_list_remove(matches, matches->data);
		}
		write_to_conv(c, addthis, WFLAG_NOLOG, NULL, time(NULL));
		gtk_editable_insert_text(GTK_EDITABLE(c->entry), partial, strlen(partial), &start);
		if (t == start) {
			t = start + strlen(partial);
			gtk_editable_set_position(GTK_EDITABLE(c->entry), t);
		}
		g_free(addthis);
	}
	
	g_free(text);
	g_free(partial);
}

gboolean meify(char *message)
{
	/* read /me-ify : if the message (post-HTML) starts with /me, remove
	 * the "/me " part of it (including that space) and return TRUE */
	char *c = message;
	int inside_HTML = 0;	/* i really don't like descriptive names */
	if (!c)
		return FALSE;	/* um... this would be very bad if this happens */
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
	}
	/* k, so now we've gotten past all the HTML crap. */
	if (!*c)
		return FALSE;
	if (!g_strncasecmp(c, "/me ", 4)) {
		sprintf(c, "%s", c + 4);
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

	if ((p = strstr(msg, who)) != NULL) {
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
	GList *ignore = b->ignored;
	char *str;

	if (!b->is_chat) {
		debug_printf("chat_write: expecting chat, got IM\n");
		return;
	}

	while (ignore) {
		if (!g_strcasecmp(who, ignore->data))
			return;
		ignore = ignore->next;
	}


	if (!(flag & WFLAG_WHISPER)) {
		str = g_strdup(normalize (who));
		if (!g_strcasecmp(str, normalize(b->gc->username))) {
			if (b->makesound && (sound_options & OPT_SOUND_CHAT_YOU_SAY))
				play_sound(CHAT_YOU_SAY);
			flag |= WFLAG_SEND;
		} else if (!g_strcasecmp(str, normalize(b->gc->displayname))) {
			if (b->makesound && (sound_options & OPT_SOUND_CHAT_YOU_SAY))
				play_sound(CHAT_YOU_SAY);
			flag |= WFLAG_SEND;
		} else {
			if (b->makesound && (sound_options & OPT_SOUND_CHAT_SAY))
				play_sound(CHAT_SAY);
			flag |= WFLAG_RECV;
		}
		g_free(str);
	}

	if ((flag & WFLAG_RECV) && find_nick(b->gc, message))
		flag |= WFLAG_NICK;

	write_to_conv(b, message, flag, who, mtime);
}



void whisper_callback(GtkWidget *widget, struct conversation *b)
{
	char buf[BUF_LEN * 4];
	char buf2[BUF_LONG];
	GList *selected;
	char *who;

	strncpy(buf, gtk_editable_get_chars(GTK_EDITABLE(b->entry), 0, -1), sizeof(buf) / 2);
	if (!strlen(buf))
		return;

	selected = GTK_LIST(b->list)->selection;

	if (!selected)
		return;


	who = GTK_LABEL(gtk_container_children(GTK_CONTAINER(selected->data))->data)->label;

	if (!who)
		return;

	gtk_editable_delete_text(GTK_EDITABLE(b->entry), 0, -1);

	serv_chat_whisper(b->gc, b->id, who, buf);

	g_snprintf(buf2, sizeof(buf2), "%s->%s", b->gc->username, who);

	chat_write(b, buf2, WFLAG_WHISPER, buf, time(NULL));

	gtk_widget_grab_focus(GTK_WIDGET(b->entry));


}

static gint insertname(gconstpointer one, gconstpointer two)
{
	const char *a = (const char *)one;
	const char *b = (const char *)two;

	if (*a == '@') {
		if (*b != '@')
			return -1;
		return (strcmp(a + 1, b + 1));
	} else if (*a == '+') {
		if (*b == '@')
			return 1;
		if (*b != '+')
			return -1;
		return (strcmp(a + 1, b + 1));
	} else {
		if (*b == '@' || *b == '+')
			return 1;
		return strcmp(a, b);
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
	gtk_list_select_child(GTK_LIST(b->list), gtk_object_get_user_data(GTK_OBJECT(obj)));
	ignore_callback(obj, b);
}

static void chat_press_info(GtkObject *obj, struct conversation *b)
{
	if (b->gc)
		(*b->gc->prpl->get_info)(b->gc, gtk_object_get_user_data(obj));
}

static gint right_click_chat(GtkObject *obj, GdkEventButton *event, struct conversation *b)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		GtkWidget *menu;
		GtkWidget *button;

		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(chat_press_im), b);
		gtk_object_set_user_data(GTK_OBJECT(button), gtk_object_get_user_data(obj));
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (g_list_find_custom(b->ignored, gtk_object_get_user_data(obj), (GCompareFunc)strcmp))
			button = gtk_menu_item_new_with_label(_("Un-Ignore"));
		else
			button = gtk_menu_item_new_with_label(_("Ignore"));
		gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(chat_press_ign), b);
		gtk_object_set_user_data(GTK_OBJECT(button), obj);
		gtk_menu_append(GTK_MENU(menu), button);
		gtk_widget_show(button);

		if (b->gc && b->gc->prpl->get_info) {
			button = gtk_menu_item_new_with_label(_("Info"));
			gtk_signal_connect(GTK_OBJECT(button), "activate",
					   GTK_SIGNAL_FUNC(chat_press_info), b);
			gtk_object_set_user_data(GTK_OBJECT(button), gtk_object_get_user_data(obj));
			gtk_menu_append(GTK_MENU(menu), button);
			gtk_widget_show(button);
		}

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}
	return TRUE;
}

void add_chat_buddy(struct conversation *b, char *buddy)
{
	char *name = g_strdup(buddy);
	char tmp[BUF_LONG];
	GtkWidget *list_item;
	int pos;
	GList *ignored;

	plugin_event(event_chat_buddy_join, b->gc, (void *)b->id, name, 0);
	b->in_room = g_list_insert_sorted(b->in_room, name, insertname);
	pos = g_list_index(b->in_room, name);

	ignored = b->ignored;
	while (ignored) {
		if (!g_strcasecmp(name, ignored->data))
			 break;
		ignored = ignored->next;
	}

	if (ignored) {
		g_snprintf(tmp, sizeof(tmp), "X %s", name);
		list_item = gtk_list_item_new_with_label(tmp);
	} else
		list_item = gtk_list_item_new_with_label(name);

	gtk_object_set_user_data(GTK_OBJECT(list_item), name);
	gtk_signal_connect(GTK_OBJECT(list_item), "button_press_event",
			   GTK_SIGNAL_FUNC(right_click_chat), b);
	gtk_list_insert_items(GTK_LIST(b->list), g_list_append(NULL, list_item), pos);
	gtk_widget_show(list_item);

	g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(b->in_room),
		   g_list_length(b->in_room) == 1 ? "person" : "people");
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_JOIN))
		play_sound(CHAT_JOIN);

	if (chat_options & OPT_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("%s entered the room."), name);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL));
	}
}


void rename_chat_buddy(struct conversation *b, char *old, char *new)
{
	GList *names = b->in_room;
	GList *items = GTK_LIST(b->list)->children;

	char *name = g_strdup(new);
	GtkWidget *list_item;
	int pos;
	GList *ignored = b->ignored;

	char tmp[BUF_LONG];

	while (names) {
		if (!g_strcasecmp((char *)names->data, old)) {
			char *tmp2 = names->data;
			b->in_room = g_list_remove(b->in_room, names->data);
			while (items) {
				if (tmp2 == gtk_object_get_user_data(items->data)) {
					gtk_list_remove_items(GTK_LIST(b->list),
							      g_list_append(NULL, items->data));
					break;
				}
				items = items->next;
			}
			g_free(tmp2);
			break;
		}
		names = names->next;
	}

	if (!names)
		return;

	b->in_room = g_list_insert_sorted(b->in_room, name, insertname);
	pos = g_list_index(b->in_room, name);

	while (ignored) {
		if (!g_strcasecmp(old, ignored->data))
			break;
		ignored = ignored->next;
	}

	if (ignored) {
		b->ignored = g_list_remove(b->ignored, ignored->data);
		b->ignored = g_list_append(b->ignored, name);
		g_snprintf(tmp, sizeof(tmp), "X %s", name);
		list_item = gtk_list_item_new_with_label(tmp);
	} else
		list_item = gtk_list_item_new_with_label(name);

	gtk_object_set_user_data(GTK_OBJECT(list_item), name);
	gtk_signal_connect(GTK_OBJECT(list_item), "button_press_event",
			   GTK_SIGNAL_FUNC(right_click_chat), b);
	gtk_list_insert_items(GTK_LIST(b->list), g_list_append(NULL, list_item), pos);
	gtk_widget_show(list_item);

	if (chat_options & OPT_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("%s is now known as %s"), old, new);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL));
	}
}


void remove_chat_buddy(struct conversation *b, char *buddy)
{
	GList *names = b->in_room;
	GList *items = GTK_LIST(b->list)->children;

	char tmp[BUF_LONG];

	plugin_event(event_chat_buddy_leave, b->gc, (void *)b->id, buddy, 0);

	while (names) {
		if (!g_strcasecmp((char *)names->data, buddy)) {
			char *tmp = names->data;
			b->in_room = g_list_remove(b->in_room, names->data);
			while (items) {
				if (tmp == gtk_object_get_user_data(items->data)) {
					gtk_list_remove_items(GTK_LIST(b->list),
							      g_list_append(NULL, items->data));
					break;
				}
				items = items->next;
			}
			g_free(tmp);
			break;
		}
		names = names->next;
	}

	if (!names)
		return;

	g_snprintf(tmp, sizeof(tmp), _("%d %s in room"), g_list_length(b->in_room),
		   g_list_length(b->in_room) == 1 ? "person" : "people");
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_PART))
		play_sound(CHAT_LEAVE);

	if (chat_options & OPT_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("%s left the room."), buddy);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL, time(NULL));
	}
}


void im_callback(GtkWidget *w, struct conversation *b)
{
	char *name;
	GList *i;
	struct conversation *c;

	i = GTK_LIST(b->list)->selection;
	if (i) {
		name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
	} else {
		return;
	}

	c = find_conversation(name);

	if (c != NULL) {
		gdk_window_raise(c->window->window);
	} else {
		c = new_conversation(name);
	}


}

void ignore_callback(GtkWidget *w, struct conversation *b)
{
	char *name;
	GList *i, *ignored;
	int pos;
	GtkWidget *list_item;
	char tmp[80];

	i = GTK_LIST(b->list)->selection;
	if (i) {
		name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
	} else {
		return;
	}

	pos = gtk_list_child_position(GTK_LIST(b->list), i->data);

	ignored = b->ignored;
	while (ignored) {
		if (!g_strcasecmp(name, ignored->data))
			 break;
		ignored = ignored->next;
	}

	if (ignored) {
		b->ignored = g_list_remove(b->ignored, ignored->data);
		g_snprintf(tmp, sizeof tmp, "%s", name);
	} else {
		b->ignored = g_list_append(b->ignored, g_strdup(name));
		g_snprintf(tmp, sizeof tmp, "X %s", name);
	}

	list_item = gtk_list_item_new_with_label(tmp);
	gtk_object_set_user_data(GTK_OBJECT(list_item), name);
	gtk_list_insert_items(GTK_LIST(b->list), g_list_append(NULL, list_item), pos);
	gtk_widget_destroy(i->data);
	gtk_widget_show(list_item);
	gtk_signal_connect(GTK_OBJECT(list_item), "button_press_event",
			   GTK_SIGNAL_FUNC(right_click_chat), b);
}

static gint delete_all_chats(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	while (chats) {
		struct conversation *c = chats->data;
		close_callback(c->close, c);
	}
	return FALSE;
}

static void chat_switch(GtkNotebook *notebook, GtkWidget *page, gint page_num, gpointer data)
{
	GtkWidget *label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(chat_notebook),
						      gtk_notebook_get_nth_page(GTK_NOTEBOOK
										(chat_notebook),
										page_num));
	GtkStyle *style;
	struct conversation *b = g_list_nth_data(chats, page_num);
	if (b && b->window && b->entry)
		gtk_window_set_focus(GTK_WINDOW(b->window), b->entry);
	if (!GTK_WIDGET_REALIZED(label))
		return;
	style = gtk_style_new();
	gdk_font_unref(style->font);
	style->font = gdk_font_ref(label->style->font);
	gtk_widget_set_style(label, style);
	gtk_style_unref(style);
	b->unseen = FALSE;
}

void show_new_buddy_chat(struct conversation *b)
{
	GtkWidget *win;
	GtkWidget *cont;
	GtkWidget *text;
	GtkWidget *send;
	GtkWidget *list;
	GtkWidget *invite_btn;
	GtkWidget *whisper;
	GtkWidget *close;
	GtkWidget *chatentry;
	GtkWidget *lbox;
	GtkWidget *bbox;
	GtkWidget *bbox2;
	GtkWidget *im, *ignore, *info;
	GtkWidget *sw;
	GtkWidget *sw2;
	GtkWidget *vbox;
	GtkWidget *vpaned;
	GtkWidget *hpaned;
	GtkWidget *toolbar;
	char buf[BUF_LONG];

	int dispstyle = set_dispstyle(1);

	if (chat_options & OPT_CHAT_ONE_WINDOW) {
		if (!all_chats) {
			win = all_chats = b->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			gtk_window_set_wmclass(GTK_WINDOW(win), "buddy_chat", "Gaim");
			gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
			gtk_container_border_width(GTK_CONTAINER(win), 0);
			gtk_widget_realize(win);
			aol_icon(win->window);
			gtk_window_set_title(GTK_WINDOW(win), _("Gaim - Group Chats"));
			gtk_signal_connect(GTK_OBJECT(win), "delete_event",
					   GTK_SIGNAL_FUNC(delete_all_chats), NULL);

			chat_notebook = gtk_notebook_new();
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
			gtk_notebook_set_scrollable(GTK_NOTEBOOK(chat_notebook), TRUE);
			gtk_notebook_popup_enable(GTK_NOTEBOOK(chat_notebook));
			gtk_container_add(GTK_CONTAINER(win), chat_notebook);
			gtk_signal_connect(GTK_OBJECT(chat_notebook), "switch-page",
					   GTK_SIGNAL_FUNC(chat_switch), NULL);
			gtk_widget_show(chat_notebook);
		} else
			win = b->window = all_chats;

		cont = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(cont), 5);
		gtk_notebook_append_page(GTK_NOTEBOOK(chat_notebook), cont, gtk_label_new(b->name));
		gtk_widget_show(cont);
	} else {
		win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		b->window = win;
		gtk_object_set_user_data(GTK_OBJECT(win), b);
		gtk_window_set_wmclass(GTK_WINDOW(win), "buddy_chat", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
		gtk_container_border_width(GTK_CONTAINER(win), 10);
		gtk_widget_realize(win);
		aol_icon(win->window);
		g_snprintf(buf, sizeof(buf), "Gaim - %s (chat)", b->name);
		gtk_window_set_title(GTK_WINDOW(win), buf);
		gtk_signal_connect(GTK_OBJECT(win), "destroy", GTK_SIGNAL_FUNC(close_callback), b);

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
	GTK_LAYOUT(text)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(text)->vadjustment->step_increment = 10.0;
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

	list = gtk_list_new();
	b->list = list;
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list);
	gtk_widget_set_usize(list, 150, -1);
	gtk_widget_show(list);

	bbox2 = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(lbox), bbox2, FALSE, FALSE, 0);
	gtk_widget_show(bbox2);

	im = picture_button2(win, _("IM"), tmp_send_xpm, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox2), im, dispstyle, dispstyle, 0);
	gtk_signal_connect(GTK_OBJECT(im), "clicked", GTK_SIGNAL_FUNC(im_callback), b);

	ignore = picture_button2(win, _("Ignore"), close_xpm, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox2), ignore, dispstyle, dispstyle, 0);
	gtk_signal_connect(GTK_OBJECT(ignore), "clicked", GTK_SIGNAL_FUNC(ignore_callback), b);

	info = picture_button2(win, _("Info"), tb_search_xpm, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox2), info, dispstyle, dispstyle, 0);
	gtk_signal_connect(GTK_OBJECT(info), "clicked", GTK_SIGNAL_FUNC(info_callback), b);
	b->info = info;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox, TRUE, FALSE);
	gtk_widget_show(vbox);

	chatentry = gtk_text_new(NULL, NULL);
	b->entry = chatentry;
	if (!(chat_options & OPT_CHAT_ONE_WINDOW))
		gtk_window_set_focus(GTK_WINDOW(b->window), b->entry);

	toolbar = build_conv_toolbar(b);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	gtk_object_set_user_data(GTK_OBJECT(chatentry), b);
	gtk_text_set_editable(GTK_TEXT(chatentry), TRUE);
	gtk_text_set_word_wrap(GTK_TEXT(chatentry), TRUE);
	gtk_signal_connect(GTK_OBJECT(chatentry), "activate", GTK_SIGNAL_FUNC(send_callback), b);
	gtk_signal_connect(GTK_OBJECT(chatentry), "key_press_event", GTK_SIGNAL_FUNC(keypress_callback),
			   b);
	gtk_signal_connect(GTK_OBJECT(chatentry), "key_press_event", GTK_SIGNAL_FUNC(entry_key_pressed),
			   chatentry);
	if (convo_options & OPT_CONVO_CHECK_SPELLING)
		gtkspell_attach(GTK_TEXT(chatentry));
	gtk_box_pack_start(GTK_BOX(vbox), chatentry, TRUE, TRUE, 0);
	gtk_widget_set_usize(chatentry, buddy_chat_size.width, buddy_chat_size.entry_height);
	gtk_window_set_focus(GTK_WINDOW(win), chatentry);
	gtk_widget_show(chatentry);

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	close = picture_button2(win, _("Close"), cancel_xpm, dispstyle);
	b->close = close;
	gtk_object_set_user_data(GTK_OBJECT(close), b);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_callback), b);
	gtk_box_pack_end(GTK_BOX(bbox), close, dispstyle, dispstyle, 0);

	invite_btn = picture_button2(win, _("Invite"), join_xpm, dispstyle);
	b->invite = invite_btn;
	gtk_signal_connect(GTK_OBJECT(invite_btn), "clicked", GTK_SIGNAL_FUNC(invite_callback), b);
	gtk_box_pack_end(GTK_BOX(bbox), invite_btn, dispstyle, dispstyle, 0);

	whisper = picture_button2(win, _("Whisper"), tb_forward_xpm, dispstyle);
	b->whisper = whisper;
	gtk_signal_connect(GTK_OBJECT(whisper), "clicked", GTK_SIGNAL_FUNC(whisper_callback), b);
	gtk_box_pack_end(GTK_BOX(bbox), whisper, dispstyle, dispstyle, 0);

	send = picture_button2(win, _("Send"), tmp_send_xpm, dispstyle);
	b->send = send;
	gtk_signal_connect(GTK_OBJECT(send), "clicked", GTK_SIGNAL_FUNC(send_callback), b);
	gtk_box_pack_end(GTK_BOX(bbox), send, dispstyle, dispstyle, 0);

	b->font_dialog = NULL;
	b->fg_color_dialog = NULL;
	b->bg_color_dialog = NULL;
	b->smiley_dialog = NULL;
	b->link_dialog = NULL;
	b->log_dialog = NULL;
	sprintf(b->fontface, "%s", fontface);
	b->hasfont = 0;
	b->fontsize = fontsize;
	b->hassize = 0;
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

static GtkWidget *change_text(GtkWidget *win, char *text, GtkWidget *button, char **xpm, int chat)
{
	int dispstyle = set_dispstyle(chat);
	GtkWidget *parent = button->parent;
	gtk_widget_destroy(button);
	button = picture_button2(win, text, xpm, dispstyle);
	if (chat == 1)
		gtk_box_pack_start(GTK_BOX(parent), button, dispstyle, dispstyle, 5);
	else
		gtk_box_pack_end(GTK_BOX(parent), button, dispstyle, dispstyle, 0);
	gtk_widget_show(button);
	return button;
}

void update_chat_button_pix()
{
	GSList *C = connections;
	struct gaim_connection *g;

	while (C) {
		GSList *bcs;
		struct conversation *c;
		int opt = 1;
		g = (struct gaim_connection *)C->data;
		bcs = g->buddy_chats;

		while (bcs) {
			c = (struct conversation *)bcs->data;
			c->send = change_text(c->window, _("Send"), c->send, tmp_send_xpm, opt);
			c->whisper =
			    change_text(c->window, _("Whisper"), c->whisper, tb_forward_xpm, opt);
			c->invite = change_text(c->window, _("Invite"), c->invite, join_xpm, opt);
			c->close = change_text(c->window, _("Close"), c->close, cancel_xpm, opt);
			gtk_object_set_user_data(GTK_OBJECT(c->close), c);
			gtk_signal_connect(GTK_OBJECT(c->close), "clicked",
					   GTK_SIGNAL_FUNC(close_callback), c);
			gtk_signal_connect(GTK_OBJECT(c->send), "clicked",
					   GTK_SIGNAL_FUNC(send_callback), c);
			gtk_signal_connect(GTK_OBJECT(c->invite), "clicked",
					   GTK_SIGNAL_FUNC(invite_callback), c);
			gtk_signal_connect(GTK_OBJECT(c->whisper), "clicked",
					   GTK_SIGNAL_FUNC(whisper_callback), c);
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
	int dispstyle = set_dispstyle(0);

	while (bcs) {
		c = (struct conversation *)bcs->data;
		parent = c->close->parent;
		c->close = change_text(c->window, _("Close"), c->close, cancel_xpm, opt);
		gtk_box_reorder_child(GTK_BOX(parent), c->close, 0);
		gtk_box_set_child_packing(GTK_BOX(parent), c->sep1, dispstyle, dispstyle, 0,
					  GTK_PACK_END);
		if (find_buddy(c->gc, c->name) == NULL)
			 c->add = change_text(c->window, _("Add"), c->add, gnome_add_xpm, opt);
		else
			c->add = change_text(c->window, _("Remove"), c->add, gnome_remove_xpm, opt);
		gtk_box_reorder_child(GTK_BOX(parent), c->add, 2);
		c->block = change_text(c->window, _("Block"), c->block, block_xpm, opt);
		gtk_box_reorder_child(GTK_BOX(parent), c->block, 3);
		c->warn = change_text(c->window, _("Warn"), c->warn, warn_xpm, opt);
		gtk_box_reorder_child(GTK_BOX(parent), c->warn, 4);
		c->info = change_text(c->window, _("Info"), c->info, tb_search_xpm, opt);
		gtk_box_reorder_child(GTK_BOX(parent), c->info, 5);
		c->send = change_text(c->window, _("Send"), c->send, tmp_send_xpm, opt);
		gtk_box_set_child_packing(GTK_BOX(parent), c->sep2, dispstyle, dispstyle, 0,
					  GTK_PACK_END);
		gtk_box_reorder_child(GTK_BOX(parent), c->send, 7);
		gtk_object_set_user_data(GTK_OBJECT(c->close), c);
		gtk_signal_connect(GTK_OBJECT(c->close), "clicked", GTK_SIGNAL_FUNC(close_callback), c);
		gtk_signal_connect(GTK_OBJECT(c->send), "clicked", GTK_SIGNAL_FUNC(send_callback), c);
		gtk_signal_connect(GTK_OBJECT(c->add), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
		gtk_signal_connect(GTK_OBJECT(c->info), "clicked", GTK_SIGNAL_FUNC(info_callback), c);
		gtk_signal_connect(GTK_OBJECT(c->warn), "clicked", GTK_SIGNAL_FUNC(warn_callback), c);
		gtk_signal_connect(GTK_OBJECT(c->block), "clicked", GTK_SIGNAL_FUNC(block_callback), c);
		bcs = bcs->next;
	}
}

void chat_tabize()
{
	int pos = 0;
	/* evil, evil i tell you! evil! */
	if (chat_options & OPT_CHAT_ONE_WINDOW) {
		GList *x = chats;
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
						      GTK_SIGNAL_FUNC(close_callback), c);
			gtk_widget_destroy(win);

			if (c->topic)
				gtk_entry_set_text(GTK_ENTRY(c->topic_text), c->topic);

			while (r) {
				char *name = r->data;
				GtkWidget *list_item;
				GList *ignored = c->ignored;
				char tmp[BUF_LONG];

				while (ignored) {
					if (!g_strcasecmp(name, ignored->data))
						 break;
					ignored = ignored->next;
				}

				if (ignored) {
					g_snprintf(tmp, sizeof(tmp), "X %s", name);
					list_item = gtk_list_item_new_with_label(tmp);
				} else
					list_item = gtk_list_item_new_with_label(name);

				gtk_object_set_user_data(GTK_OBJECT(list_item), name);
				gtk_signal_connect(GTK_OBJECT(list_item), "button_press_event",
						   GTK_SIGNAL_FUNC(right_click_chat), c);
				gtk_list_insert_items(GTK_LIST(c->list),
						      g_list_append(NULL, list_item), pos);
				gtk_widget_show(list_item);

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

			while (r) {
				char *name = r->data;
				GtkWidget *list_item;
				GList *ignored = c->ignored;
				char tmp[BUF_LONG];

				while (ignored) {
					if (!g_strcasecmp(name, ignored->data))
						 break;
					ignored = ignored->next;
				}

				if (ignored) {
					g_snprintf(tmp, sizeof(tmp), "X %s", name);
					list_item = gtk_list_item_new_with_label(tmp);
				} else
					list_item = gtk_list_item_new_with_label(name);

				gtk_object_set_user_data(GTK_OBJECT(list_item), name);
				gtk_signal_connect(GTK_OBJECT(list_item), "button_press_event",
						   GTK_SIGNAL_FUNC(right_click_chat), c);
				gtk_list_insert_items(GTK_LIST(c->list),
						      g_list_append(NULL, list_item), pos);
				gtk_widget_show(list_item);

				r = r->next;
				pos++;
			}

			x = x->next;
		}
		if (all_chats)
			gtk_widget_destroy(all_chats);
		all_chats = NULL;
		chat_notebook = NULL;
		chats = m;
	}
}
