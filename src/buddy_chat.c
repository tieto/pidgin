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
#include "../config.h"
#endif
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gtkhtml.h"
#include "gtkspell.h"
#include <gdk/gdkkeysyms.h>

#include "convo.h"
#include "prpl.h"

#include "pixmaps/tb_forward.xpm"
#include "pixmaps/join.xpm"
#include "pixmaps/close.xpm"

static GtkWidget *joinchat;
static struct gaim_connection *joinchatgc;
static GtkWidget *entry;
static GtkWidget *invite;
static GtkWidget *inviteentry;
static GtkWidget *invitemess;
static int community;
extern int state_lock;

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


static void do_join_chat()
{
	char *group;

	group = gtk_entry_get_text(GTK_ENTRY(entry));

	if (joinchat) {
		serv_join_chat(joinchatgc, community + 4, group);
		gtk_widget_destroy(joinchat);
	}
	joinchat = NULL;
}

static void joinchat_choose(GtkWidget *w, struct gaim_connection *g)
{
	joinchatgc = g;
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

	while (c) {
		g = (struct gaim_connection *)c->data;
		g_snprintf(buf, sizeof buf, "%s (%s)", g->username, (*g->prpl->name)());
		opt = gtk_menu_item_new_with_label(buf);
		gtk_object_set_user_data(GTK_OBJECT(opt), g);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(joinchat_choose), g);
		gtk_menu_append(GTK_MENU(menu), opt);
		gtk_widget_show(opt);
		c = c->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);

	joinchatgc = connections->data;
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
	GtkWidget *opt;

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

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);

		label = gtk_label_new(_("Join what group:"));
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		entry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(rowbox), entry, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(entry), "activate",
				   GTK_SIGNAL_FUNC(do_join_chat), joinchat);
		gtk_window_set_focus(GTK_WINDOW(joinchat), entry);

#ifndef NO_MULTI
		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);

		label = gtk_label_new(_("Join Chat As:"));
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		create_joinchat_menu(rowbox);
#else
		joinchatgc = connections->data;
#endif

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);

		community = 0;
		opt = gtk_radio_button_new_with_label(NULL, _("AIM Private Chats"));
		gtk_box_pack_start(GTK_BOX(rowbox), opt, TRUE, TRUE, 0);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
		gtk_signal_connect(GTK_OBJECT(opt), "clicked", set_option, &community);
		gtk_widget_show(opt);

		opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(opt)),
						      _("AOL Community Chats"));
		gtk_box_pack_start(GTK_BOX(rowbox), opt, TRUE, TRUE, 0);

		/* buttons */

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		cancel = picture_button(joinchat, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
				   GTK_SIGNAL_FUNC(destroy_join_chat), joinchat);

		join = picture_button(joinchat, _("Join"), join_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), join, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(join), "clicked", GTK_SIGNAL_FUNC(do_join_chat), joinchat);
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

	buddy = gtk_entry_get_text(GTK_ENTRY(inviteentry));
	mess = gtk_entry_get_text(GTK_ENTRY(invitemess));

	if (invite) {
		serv_chat_invite(b->gc, b->id, mess, buddy);
		gtk_widget_destroy(invite);
	}
	invite = NULL;
}



void invite_callback(GtkWidget *w, struct conversation *b)
{
	GtkWidget *cancel;
	GtkWidget *invite_btn;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;
	if (!invite) {
		invite = gtk_window_new(GTK_WINDOW_DIALOG);
		cancel = gtk_button_new_with_label(_("Cancel"));
		invite_btn = gtk_button_new_with_label(_("Invite"));
		bbox = gtk_hbox_new(TRUE, 10);
		topbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);
		inviteentry = gtk_entry_new();
		invitemess = gtk_entry_new();

		if (display_options & OPT_DISP_COOL_LOOK) {
			gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(invite_btn), GTK_RELIEF_NONE);
		}

		/* Put the buttons in the box */
		gtk_box_pack_start(GTK_BOX(bbox), invite_btn, TRUE, TRUE, 10);
		gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

		label = gtk_label_new(_("Invite who?"));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(topbox), inviteentry, FALSE, FALSE, 5);
		label = gtk_label_new(_("With message:"));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(topbox), invitemess, FALSE, FALSE, 5);

		/* And the boxes in the box */
		gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

		/* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(invite), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_invite), invite);

		gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_invite), b);
		gtk_signal_connect(GTK_OBJECT(invite_btn), "clicked", GTK_SIGNAL_FUNC(do_invite), b);
		gtk_signal_connect(GTK_OBJECT(inviteentry), "activate", GTK_SIGNAL_FUNC(do_invite), b);
		/* Finish up */
		gtk_widget_show(invite_btn);
		gtk_widget_show(cancel);
		gtk_widget_show(inviteentry);
		gtk_widget_show(invitemess);
		gtk_widget_show(topbox);
		gtk_widget_show(bbox);
		gtk_widget_show(vbox);
		gtk_window_set_title(GTK_WINDOW(invite), _("Invite to Buddy Chat"));
		gtk_window_set_focus(GTK_WINDOW(invite), inviteentry);
		gtk_container_add(GTK_CONTAINER(invite), vbox);
		gtk_widget_realize(invite);
		aol_icon(invite->window);

	}
	gtk_widget_show(invite);
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
	if (!strncmp(c, "/me ", 4)) {
		sprintf(c, "%s", c + 4);
		return TRUE;
	} else
		return FALSE;
}

void chat_write(struct conversation *b, char *who, int flag, char *message)
{
	GList *ignore = b->ignored;
	char *str;

	if (!b->is_chat) {
		debug_printf("chat_write: expecting chat, got IM\n");
		return;
	}

	while (ignore) {
		if (!strcasecmp(who, ignore->data))
			return;
		ignore = ignore->next;
	}


	if (!(flag & WFLAG_WHISPER)) {
		str = g_strdup(normalize(who));
		if (!strcasecmp(str, normalize(b->gc->username))) {
			debug_printf("%s %s\n", normalize(who), normalize(b->gc->username));
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

	write_to_conv(b, message, flag, who);
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

	escape_text(buf);	/* it's ok to leave this here because oscar can't whisper */
	serv_chat_whisper(b->gc, b->id, who, buf);

	g_snprintf(buf2, sizeof(buf2), "%s->%s", b->gc->username, who);

	chat_write(b, buf2, WFLAG_WHISPER, buf);

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


void add_chat_buddy(struct conversation *b, char *buddy)
{
	char *name = g_strdup(buddy);
	char tmp[BUF_LONG];
	GtkWidget *list_item;
	int pos;
	GList *ignored;

	plugin_event(event_chat_buddy_join, b->gc, b->name, name, 0);
	b->in_room = g_list_insert_sorted(b->in_room, name, insertname);
	pos = g_list_index(b->in_room, name);

	ignored = b->ignored;
	while (ignored) {
		if (!strcasecmp(name, ignored->data))
			break;
		ignored = ignored->next;
	}

	if (ignored) {
		g_snprintf(tmp, sizeof(tmp), "X %s", name);
		list_item = gtk_list_item_new_with_label(tmp);
	} else
		list_item = gtk_list_item_new_with_label(name);

	gtk_object_set_user_data(GTK_OBJECT(list_item), name);
	gtk_list_insert_items(GTK_LIST(b->list), g_list_append(NULL, list_item), pos);
	gtk_widget_show(list_item);

	g_snprintf(tmp, sizeof(tmp), _("%d people in room"), g_list_length(b->in_room));
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_JOIN))
		play_sound(CHAT_JOIN);

	if (display_options & OPT_DISP_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("<B>%s entered the room.</B>"), name);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL);
	}
}




void remove_chat_buddy(struct conversation *b, char *buddy)
{
	GList *names = b->in_room;
	GList *items = GTK_LIST(b->list)->children;

	char tmp[BUF_LONG];

	plugin_event(event_chat_buddy_leave, b->gc, b->name, buddy, 0);

	while (names) {
		if (!strcasecmp((char *)names->data, buddy)) {
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

	g_snprintf(tmp, sizeof(tmp), _("%d people in room"), g_list_length(b->in_room));
	gtk_label_set_text(GTK_LABEL(b->count), tmp);

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_PART))
		play_sound(CHAT_LEAVE);

	if (display_options & OPT_DISP_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("<B>%s left the room.</B>"), buddy);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL);
	}
}


void im_callback(GtkWidget *w, struct conversation *b)
{
	char *name;
	GList *i;
	struct conversation *c;

	i = GTK_LIST(b->list)->selection;
	if (i)
		name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
	else
		return;

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
	if (i)
		name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
	else
		return;

	pos = gtk_list_child_position(GTK_LIST(b->list), i->data);

	ignored = b->ignored;
	while (ignored) {
		if (!strcasecmp(name, ignored->data))
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
}



void show_new_buddy_chat(struct conversation *b)
{
	GtkWidget *win;
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

	int dispstyle = set_dispstyle(1);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	b->window = win;
	gtk_object_set_user_data(GTK_OBJECT(win), b);
	gtk_window_set_wmclass(GTK_WINDOW(win), "buddy_chat", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);
	gtk_container_border_width(GTK_CONTAINER(win), 10);
	gtk_window_set_title(GTK_WINDOW(win), b->name);
	gtk_signal_connect(GTK_OBJECT(win), "destroy", GTK_SIGNAL_FUNC(close_callback), b);
	gtk_widget_realize(win);
	aol_icon(win->window);

	vpaned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(vpaned), 15);
	gtk_container_add(GTK_CONTAINER(win), vpaned);
	gtk_widget_show(vpaned);

	hpaned = gtk_hpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(hpaned), 15);
	gtk_paned_pack1(GTK_PANED(vpaned), hpaned, TRUE, FALSE);
	gtk_widget_show(hpaned);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_paned_pack1(GTK_PANED(hpaned), sw, TRUE, TRUE);
	gtk_widget_set_usize(sw, 320, 160);
	gtk_widget_show(sw);

	text = gtk_html_new(NULL, NULL);
	b->text = text;
	gtk_container_add(GTK_CONTAINER(sw), text);
	gtk_widget_show(text);
	GTK_HTML(text)->hadj->step_increment = 10.0;
	GTK_HTML(text)->vadj->step_increment = 10.0;

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
	if (general_options & OPT_GEN_CHECK_SPELLING)
		gtkspell_attach(GTK_TEXT(chatentry));
	gtk_box_pack_start(GTK_BOX(vbox), chatentry, TRUE, TRUE, 0);
	if (display_options & OPT_DISP_CHAT_BIG_ENTRY)
		gtk_widget_set_usize(chatentry, 320, 50);
	else
		gtk_widget_set_usize(chatentry, 320, 25);
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
	b->bgcol = bgcolor;
	b->hasbg = 0;
	b->fgcol = fgcolor;
	b->hasfg = 0;

	update_buttons_by_protocol(b);

	gtk_widget_show(win);
}



void handle_click_chat(GtkWidget *widget, GdkEventButton * event, struct chat_room *cr)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		/* FIXME : double click on chat in buddy list */
		serv_join_chat(connections->data, cr->exchange, cr->name);
	}
}


void setup_buddy_chats()
{
	GList *list;
	struct chat_room *cr;
	GList *crs = chat_rooms;
	GtkWidget *w;
	GtkWidget *item;
	GtkWidget *tree;

	if (buddies == NULL)
		return;

	list = GTK_TREE(buddies)->children;

	while (list) {
		w = (GtkWidget *)list->data;
		if (!strcmp(GTK_LABEL(GTK_BIN(w)->child)->label, _("Buddy Chat"))) {
			gtk_tree_remove_items(GTK_TREE(buddies), list);
			list = GTK_TREE(buddies)->children;
			if (!list)
				break;
		}
		list = list->next;
	}

	if (crs == NULL)
		return;

	item = gtk_tree_item_new_with_label(_("Buddy Chat"));
	tree = gtk_tree_new();
	gtk_widget_show(item);
	gtk_widget_show(tree);
	gtk_tree_append(GTK_TREE(buddies), item);
	gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), tree);
	gtk_tree_item_expand(GTK_TREE_ITEM(item));

	while (crs) {
		cr = (struct chat_room *)crs->data;

		item = gtk_tree_item_new_with_label(cr->name);
		gtk_object_set_user_data(GTK_OBJECT(item), cr);
		gtk_tree_append(GTK_TREE(tree), item);
		gtk_widget_show(item);
		gtk_signal_connect(GTK_OBJECT(item), "button_press_event",
				   GTK_SIGNAL_FUNC(handle_click_chat), cr);

		crs = crs->next;

	}

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
