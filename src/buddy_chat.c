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
#include <gdk/gdkkeysyms.h>

#include "convo.h"

#include "pixmaps/join.xpm"
#include "pixmaps/cancel.xpm"


static GtkWidget *joinchat;
static GtkWidget *entry;
static GtkWidget *invite;
static GtkWidget *inviteentry;
static GtkWidget *invitemess;
extern int state_lock;

static void destroy_join_chat()
{
	if (joinchat)
		gtk_widget_destroy(joinchat);
	joinchat=NULL;
}

static void destroy_invite()
{
	if (invite)
		gtk_widget_destroy(invite);
	invite=NULL;
}




static void do_join_chat()
{
	char *group;

	group = gtk_entry_get_text(GTK_ENTRY(entry));

        if (joinchat) {
                serv_join_chat(4, group);
		gtk_widget_destroy(joinchat);
	}
	joinchat=NULL;
}



void join_chat()
{
	GtkWidget *cancel;
	GtkWidget *join;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;
	GtkWidget *button_box;
	GtkWidget *icon_i;
	GdkBitmap *mask;
	GdkPixmap *icon;
	GtkWidget *frame;

	if (!joinchat) {
		joinchat = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_widget_set_usize(joinchat, 300, 100);
		gtk_window_set_policy(GTK_WINDOW(joinchat), FALSE, FALSE, TRUE);
		gtk_widget_show(joinchat);
		bbox = gtk_hbox_new(TRUE, 10);
		topbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);
		entry = gtk_entry_new();

		frame = gtk_frame_new(_("Buddy Chat"));
       /* Build Join Button */

        join = gtk_button_new();

        button_box = gtk_hbox_new(FALSE, 5);
        icon = gdk_pixmap_create_from_xpm_d (joinchat->window, &mask, NULL, join_xpm);
        icon_i = gtk_pixmap_new(icon, mask);

        label = gtk_label_new(_("Join"));

        gtk_box_pack_start(GTK_BOX(button_box), icon_i, FALSE, FALSE, 2);
        gtk_box_pack_end(GTK_BOX(button_box), label, FALSE, FALSE, 2);

        gtk_widget_show(label);
        gtk_widget_show(icon_i);

        gtk_widget_show(button_box);

        gtk_container_add(GTK_CONTAINER(join), button_box);
	gtk_widget_set_usize(join, 75, 30);

        /* End of OK Button */

        /* Build Cancel Button */

        cancel = gtk_button_new();

        button_box = gtk_hbox_new(FALSE, 5);
        icon = gdk_pixmap_create_from_xpm_d ( joinchat->window, &mask, NULL, cancel_xpm);
        icon_i = gtk_pixmap_new(icon, mask);

        label = gtk_label_new(_("Cancel"));

        gtk_box_pack_start(GTK_BOX(button_box), icon_i, FALSE, FALSE, 2);
        gtk_box_pack_end(GTK_BOX(button_box), label, FALSE, FALSE, 2);

        gtk_widget_show(label);
        gtk_widget_show(icon_i);

        gtk_widget_show(button_box);

        gtk_container_add(GTK_CONTAINER(cancel), button_box);

	gtk_widget_set_usize(cancel, 75, 30);

        /* End of Cancel Button */
        
	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(join), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
	}

	gtk_box_pack_start(GTK_BOX(bbox), join, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

		label = gtk_label_new(_("Join what group:"));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(topbox), entry, FALSE, FALSE, 5);
	
		/* And the boxes in the box */
		gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);
		
		/* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(joinchat), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_join_chat), joinchat);

		gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(destroy_join_chat), joinchat);
		gtk_signal_connect(GTK_OBJECT(join), "clicked",
			   GTK_SIGNAL_FUNC(do_join_chat), joinchat);
		gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(do_join_chat), joinchat);
		/* Finish up */
		gtk_widget_show(join);
		gtk_widget_show(cancel);
		gtk_widget_show(entry);
		gtk_widget_show(topbox);
		gtk_widget_show(bbox);
		gtk_widget_show(vbox);
		gtk_widget_show(frame);
		gtk_container_add(GTK_CONTAINER(frame), vbox);
		gtk_window_set_title(GTK_WINDOW(joinchat), _("Join Chat"));
		gtk_window_set_focus(GTK_WINDOW(joinchat), entry);
                gtk_container_add(GTK_CONTAINER(joinchat), frame);
		gtk_container_set_border_width(GTK_CONTAINER(joinchat), 5);
                gtk_widget_realize(joinchat);
		aol_icon(joinchat->window);

	}
	gtk_widget_show(joinchat);
}


static void do_invite(GtkWidget *w, struct conversation *b)
{
	char *buddy;
	char *mess;

	if (!b->is_chat) {
		debug_print("do_invite: expecting chat, got IM\n");
		return;
	}

	buddy = gtk_entry_get_text(GTK_ENTRY(inviteentry));
	mess = gtk_entry_get_text(GTK_ENTRY(invitemess));

        if (invite) {
                serv_chat_invite(b->id, mess, buddy);
		gtk_widget_destroy(invite);
	}
	invite=NULL;
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

		if (display_options & OPT_DISP_COOL_LOOK)
		{
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

		gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(destroy_invite), b);
		gtk_signal_connect(GTK_OBJECT(invite_btn), "clicked",
			   GTK_SIGNAL_FUNC(do_invite), b);
		gtk_signal_connect(GTK_OBJECT(inviteentry), "activate",
			   GTK_SIGNAL_FUNC(do_invite), b);
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

gboolean meify(char *message) {
	/* read /me-ify : if the message (post-HTML) starts with /me, remove
	 * the "/me " part of it (including that space) and return TRUE */
	char *c = message;
	int inside_HTML = 0; /* i really don't like descriptive names */
	if (!c) return FALSE; /* um... this would be very bad if this happens */
	while (*c) {
		if (inside_HTML) {
			if (*c == '>') inside_HTML = 0;
		} else {
			if (*c == '<') inside_HTML = 1;
			else break;
		}
		c++; /* i really don't like c++ either */
	}
	/* k, so now we've gotten past all the HTML crap. */
	if (!*c) return FALSE;
	if (!strncmp(c, "/me ", 4)) {
		sprintf(c, "%s", c+4);
		return TRUE;
	} else
		return FALSE;
}

void chat_write(struct conversation *b, char *who, int flag, char *message)
{
        GList *ignore = b->ignored;
	char *str;

	if (!b->is_chat) {
		debug_print("chat_write: expecting chat, got IM\n");
		return;
	}

        while(ignore) {
                if (!strcasecmp(who, ignore->data))
                        return;
                ignore = ignore->next;
        }

        
        if (!(flag & WFLAG_WHISPER)) {
		str = g_strdup(normalize(who));
		if (!strcasecmp(str, normalize(current_user->username))) {
			sprintf(debug_buff, "%s %s\n", normalize(who), normalize(current_user->username));
			debug_print(debug_buff);
			if (b->makesound && (sound_options & OPT_SOUND_CHAT_SAY))
				play_sound(SEND);
			flag |= WFLAG_SEND;
		} else {
			if (b->makesound && (sound_options & OPT_SOUND_CHAT_SAY))
				play_sound(RECEIVE);
			flag |= WFLAG_RECV;
		}
		g_free(str);
        }

	write_to_conv(b, message, flag, who);
}



void whisper_callback(GtkWidget *widget, struct conversation *b)
{
	char buf[BUF_LEN*4];
	char buf2[BUF_LONG];
	GList *selected;
	char *who;

	strncpy(buf, gtk_editable_get_chars(GTK_EDITABLE(b->entry), 0, -1), sizeof(buf)/2);
	if (!strlen(buf))
		return;

	selected = GTK_LIST(b->list)->selection;

	if (!selected)
		return;
	

	who = GTK_LABEL(gtk_container_children(GTK_CONTAINER(selected->data))->data)->label;

	if (!who)
		return;

	gtk_editable_delete_text(GTK_EDITABLE(b->entry), 0, -1);

        escape_text(buf);
        serv_chat_whisper(b->id, who, buf);
                          
	g_snprintf(buf2, sizeof(buf2), "%s->%s", current_user->username, who);

	chat_write(b, buf2, WFLAG_WHISPER, buf);

	gtk_widget_grab_focus(GTK_WIDGET(b->entry));


}



void update_chat_list(struct conversation *b)
{
        GtkWidget *list_item;
        char name[80];
        char *tmp;
        GList *names = b->in_room;

	if (!b->is_chat) {
		debug_print("update_chat_list: expecting chat, got IM\n");
		return;
	}

        gtk_list_clear_items(GTK_LIST(b->list), 0, -1);


        while(names) {
                tmp = (char *)names->data;
                if (g_list_index(b->ignored, names->data) != -1)
                        g_snprintf(name, sizeof(name), "X %s", tmp);
                else
                        g_snprintf(name, sizeof(name), "%s", tmp);

                list_item = gtk_list_item_new_with_label(name);
                gtk_widget_show(list_item);
                gtk_object_set_user_data(GTK_OBJECT(list_item), tmp);

                gtk_list_append_items(GTK_LIST(b->list), g_list_append(NULL, list_item));

                names = names->next;
        }

}



void add_chat_buddy(struct conversation *b, char *buddy)
{
        char *name = g_strdup(buddy);
	char tmp[BUF_LONG];

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_buddy_join && g->function != NULL) {
			function = g->function;
			(*function)(b->name, name, g->data);
		}
		c = c->next;
	}
#endif
        b->in_room = g_list_append(b->in_room, name);

        update_chat_list(b);

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_JOIN))
		play_sound(BUDDY_ARRIVE);

	if (display_options & OPT_DISP_CHAT_LOGON) {
		g_snprintf(tmp, sizeof(tmp), _("<B>%s entered the room.</B>"), name);
		write_to_conv(b, tmp, WFLAG_SYSTEM, NULL);
	}
}




void remove_chat_buddy(struct conversation *b, char *buddy)
{	
        GList *names = b->in_room;
	char tmp[BUF_LONG];

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_buddy_leave && g->function != NULL) {
			function = g->function;
			(*function)(b->name, buddy, g->data);
		}
		c = c->next;
	}
#endif

        while(names) {
                if (!strcasecmp((char *)names->data, buddy)) {
                        b->in_room = g_list_remove(b->in_room, names->data);
                        update_chat_list(b);
                        break;
                }
                names = names->next;
        }

	if (b->makesound && (sound_options & OPT_SOUND_CHAT_JOIN))
		play_sound(BUDDY_LEAVE);

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
        GList *i;

        i = GTK_LIST(b->list)->selection;
        if (i)
                name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
        else
                return;

        if (g_list_index(b->ignored, (gpointer)name) == -1)
                b->ignored = g_list_append(b->ignored, name);
        else
                b->ignored = g_list_remove(b->ignored, name);

        update_chat_list(b);
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
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	b->window = win;

	vpaned = gtk_vpaned_new();
	hpaned = gtk_hpaned_new();

	gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);

	close = gtk_button_new_with_label(_("Close"));
	invite_btn = gtk_button_new_with_label(_("Invite"));
	whisper = gtk_button_new_with_label(_("Whisper"));
        send = gtk_button_new_with_label(_("Send"));

        im = gtk_button_new_with_label(_("IM"));
        ignore = gtk_button_new_with_label(_("Ignore"));
        info = gtk_button_new_with_label(_("Info"));

	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(invite_btn), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(whisper), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(send), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(im), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(ignore), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(info), GTK_RELIEF_NONE);
	}
	
	text = gtk_html_new(NULL, NULL);
	
	b->text = text;

	list = gtk_list_new();
	b->list = list;

        bbox = gtk_hbox_new(TRUE, 0);
        bbox2 = gtk_hbox_new(TRUE, 0);
        vbox = gtk_vbox_new(FALSE, 0);
        lbox = gtk_vbox_new(FALSE, 4);
	chatentry = gtk_text_new( NULL, NULL );
	gtk_text_set_editable(GTK_TEXT(chatentry), TRUE);

	gtk_text_set_word_wrap(GTK_TEXT(chatentry), TRUE);

	gtk_widget_realize(win);

	toolbar = build_conv_toolbar(b);

	gtk_object_set_user_data(GTK_OBJECT(chatentry), b);
	b->entry = chatentry;
	
	/* Hack something so we know have an entry click event */

	gtk_signal_connect(GTK_OBJECT(chatentry), "activate", GTK_SIGNAL_FUNC(send_callback),b);
	gtk_signal_connect(GTK_OBJECT(chatentry), "key_press_event", GTK_SIGNAL_FUNC(keypress_callback), b);
        /* Text box */

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_ALWAYS);
        gtk_widget_show(sw);
        gtk_container_add(GTK_CONTAINER(sw), text);
        gtk_widget_show(text);


        GTK_HTML (text)->hadj->step_increment = 10.0;
        GTK_HTML (text)->vadj->step_increment = 10.0;
        gtk_widget_set_usize(sw, 320, 150);

	gtk_paned_pack1(GTK_PANED(hpaned), sw, TRUE, TRUE);

        sw2 = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
                                       GTK_POLICY_NEVER,
                                       GTK_POLICY_AUTOMATIC);
        gtk_widget_show(sw2);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list);

        gtk_box_pack_start(GTK_BOX(lbox), sw2, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(lbox), bbox2, FALSE, FALSE, 5);
                                       
        
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, TRUE, TRUE);
	gtk_widget_show(list);


	gtk_widget_set_usize(list, 150, 150);


	/* Ready and pack buttons */
	gtk_object_set_user_data(GTK_OBJECT(win), b);
	gtk_object_set_user_data(GTK_OBJECT(close), b);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_callback),b);
	gtk_signal_connect(GTK_OBJECT(send), "clicked", GTK_SIGNAL_FUNC(send_callback),b);
	gtk_signal_connect(GTK_OBJECT(invite_btn), "clicked", GTK_SIGNAL_FUNC(invite_callback), b);
	gtk_signal_connect(GTK_OBJECT(whisper), "clicked", GTK_SIGNAL_FUNC(whisper_callback), b);

        gtk_signal_connect(GTK_OBJECT(im), "clicked", GTK_SIGNAL_FUNC(im_callback), b);
        gtk_signal_connect(GTK_OBJECT(ignore), "clicked", GTK_SIGNAL_FUNC(ignore_callback), b);
        gtk_signal_connect(GTK_OBJECT(info), "clicked", GTK_SIGNAL_FUNC(info_callback), b);

	gtk_box_pack_start(GTK_BOX(bbox), send, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), whisper, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(bbox), invite_btn, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(bbox), close, TRUE, TRUE, 5);

        gtk_box_pack_start(GTK_BOX(bbox2), im, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(bbox2), ignore, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(bbox2), info, TRUE, TRUE, 5);
	
	/* pack and fill the rest */
	
	
	gtk_paned_pack1(GTK_PANED(vpaned), hpaned, TRUE, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), chatentry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox, TRUE, FALSE);

	gtk_widget_show(send);
	gtk_widget_show(invite_btn);
	gtk_widget_show(whisper);
        gtk_widget_show(close);
        gtk_widget_show(im);
        gtk_widget_show(ignore);
        gtk_widget_show(info);
        gtk_widget_show(bbox);
        gtk_widget_show(lbox);
        gtk_widget_show(bbox2);
	gtk_widget_show(vbox);
	gtk_widget_show( vpaned );
	gtk_widget_show( hpaned );
	gtk_widget_show(chatentry);
       	gtk_widget_set_usize(chatentry, 320, 25);
	
	gtk_container_add(GTK_CONTAINER(win),vpaned);
	gtk_container_border_width(GTK_CONTAINER(win), 10);

	gtk_window_set_title(GTK_WINDOW(win), b->name);
	gtk_window_set_focus(GTK_WINDOW(win), chatentry);

	gtk_signal_connect(GTK_OBJECT(win), "destroy", GTK_SIGNAL_FUNC(close_callback),b);

	if (fontface)
		strncpy(b->current_fontface, fontface, sizeof(b->current_fontface));
	if (fontname)
		strncpy(b->current_fontname, fontname, sizeof(b->current_fontname));
		
	gtk_widget_realize(win);
	aol_icon(win->window);

	gtk_widget_show(win);
}



void handle_click_chat(GtkWidget *widget, GdkEventButton *event, struct chat_room *cr)
{
        if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
                serv_join_chat(cr->exchange, cr->name);
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

        while(list) {
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
                                   GTK_SIGNAL_FUNC(handle_click_chat),
                                   cr);

                crs = crs->next;

        }

}
