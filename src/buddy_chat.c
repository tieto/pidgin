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

#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "gtkhtml.h"

static GtkWidget *joinchat;
static GtkWidget *entry;
static GtkWidget *invite;
static GtkWidget *inviteentry;
static GtkWidget *invitemess;

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
	if (!joinchat) {
		joinchat = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		cancel = gtk_button_new_with_label("Cancel");
		join = gtk_button_new_with_label("Join");
		bbox = gtk_hbox_new(TRUE, 10);
		topbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);
		entry = gtk_entry_new();

		/* Put the buttons in the box */
		gtk_box_pack_start(GTK_BOX(bbox), join, TRUE, TRUE, 10);
		gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);
		
		label = gtk_label_new("Join what group:");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(topbox), entry, FALSE, FALSE, 5);
	
		/* And the boxes in the box */
		gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
		
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
		gtk_window_set_title(GTK_WINDOW(joinchat), "Join Chat");
		gtk_window_set_focus(GTK_WINDOW(joinchat), entry);
                gtk_container_add(GTK_CONTAINER(joinchat), vbox);
                gtk_widget_realize(joinchat);
		aol_icon(joinchat->window);

	}
	gtk_widget_show(joinchat);
}


static void do_invite(GtkWidget *w, struct buddy_chat *b)
{
	char *buddy;
	char *mess;

	buddy = gtk_entry_get_text(GTK_ENTRY(inviteentry));
	mess = gtk_entry_get_text(GTK_ENTRY(invitemess));

        if (invite) {
                serv_chat_invite(b->id, mess, buddy);
		gtk_widget_destroy(invite);
	}
	invite=NULL;
}



static void invite_callback(GtkWidget *w, struct buddy_chat *b)
{
	GtkWidget *cancel;
	GtkWidget *invite_btn;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;
	if (!invite) {
		invite = gtk_window_new(GTK_WINDOW_DIALOG);
		cancel = gtk_button_new_with_label("Cancel");
		invite_btn = gtk_button_new_with_label("Invite");
		bbox = gtk_hbox_new(TRUE, 10);
		topbox = gtk_hbox_new(FALSE, 5);
		vbox = gtk_vbox_new(FALSE, 5);
		inviteentry = gtk_entry_new();
		invitemess = gtk_entry_new();

		/* Put the buttons in the box */
		gtk_box_pack_start(GTK_BOX(bbox), invite_btn, TRUE, TRUE, 10);
		gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);
		
		label = gtk_label_new("Invite who?");
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(topbox), inviteentry, FALSE, FALSE, 5);
		label = gtk_label_new("With message:");
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
		gtk_window_set_title(GTK_WINDOW(invite), "Invite to Buddy Chat");
		gtk_window_set_focus(GTK_WINDOW(invite), inviteentry);
                gtk_container_add(GTK_CONTAINER(invite), vbox);
                gtk_widget_realize(invite);
		aol_icon(invite->window);

	}
	gtk_widget_show(invite);
}

void chat_write(struct buddy_chat *b, char *who, int flag, char *message)
{
        char *buf;
        GList *ignore = b->ignored;
        char *str;
        char colour[10];


        while(ignore) {
                if (!strcasecmp(who, ignore->data))
                        return;
                ignore = ignore->next;
        }
        
        buf = g_malloc(BUF_LONG);
        
        if (flag & WFLAG_WHISPER) {
                str = g_malloc(64);
                g_snprintf(str, 62, "*%s*", who);
                strcpy(colour, "#00ff00\0");
        } else {
                str = g_strdup(normalize(who));
                if (!strcasecmp(str, normalize(current_user->username)))
                        strcpy(colour, "#0000ff\0");
                else
                        strcpy(colour, "#ff0000\0");
                g_free(str);
                str = who;
        }



	if (display_options & OPT_DISP_SHOW_TIME)
                g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s %s: </B></FONT>", colour, date(), str);
	else
                g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"%s\"><B>%s: </B></FONT>", colour, str);

        gtk_html_freeze(GTK_HTML(b->text));
                        
        gtk_html_append_text(GTK_HTML(b->text), buf, 0);
        gtk_html_append_text(GTK_HTML(b->text), message, (display_options & OPT_DISP_IGNORE_COLOUR) ? HTML_OPTION_NO_COLOURS : 0);
        gtk_html_append_text(GTK_HTML(b->text), "<BR>", 0);
        
        gtk_html_thaw(GTK_HTML(b->text));
        
        if (flag & WFLAG_WHISPER)
                g_free(str);

        g_free(buf);
}

static void close_callback(GtkWidget *widget, struct buddy_chat *b)
{
        serv_chat_leave(b->id);
        
        if (b->window)
                gtk_widget_destroy(b->window);
        b->window = NULL;
}


static void whisper_callback(GtkWidget *widget, struct buddy_chat *b)
{
	char buf[BUF_LEN*4];
	char buf2[BUF_LONG];
	GList *selected;
	char *who;

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(b->entry)), sizeof(buf)/2);
	if (!strlen(buf))
		return;

	selected = GTK_LIST(b->list)->selection;

	if (!selected)
		return;
	

	who = GTK_LABEL(gtk_container_children(GTK_CONTAINER(selected->data))->data)->label;

	if (!who)
		return;

	gtk_entry_set_text(GTK_ENTRY(b->entry), "");

        escape_text(buf);
        serv_chat_whisper(b->id, who, buf);
                          
	g_snprintf(buf2, sizeof(buf2), "%s->%s", current_user->username, who);

	chat_write(b, buf2, WFLAG_WHISPER, buf);

	gtk_widget_grab_focus(GTK_WIDGET(b->entry));


}


static void send_callback(GtkWidget *widget, struct buddy_chat *b)
{
	char buf[BUF_LEN*4];

	strncpy(buf, gtk_editable_get_chars(GTK_EDITABLE(b->entry), 0, -1), sizeof(buf)/2);
	if (!strlen(buf))
		return;

	gtk_editable_delete_text(GTK_EDITABLE(b->entry), 0, -1);

	if (general_options & OPT_GEN_SEND_LINKS) {
		linkify_text(buf);
	}

        escape_text(buf);
        serv_chat_send(b->id, buf);
        
	gtk_widget_grab_focus(GTK_WIDGET(b->entry));

	serv_set_idle(0);
}


void update_chat_list(struct buddy_chat *b)
{
        GtkWidget *list_item;
        char name[80];
        char *tmp;
        GList *names = b->in_room;


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



void add_chat_buddy(struct buddy_chat *b, char *buddy)
{
        char *name = g_strdup(buddy);

        b->in_room = g_list_append(b->in_room, name);

        update_chat_list(b);

}




void remove_chat_buddy(struct buddy_chat *b, char *buddy)
{	
        GList *names = b->in_room;

        while(names) {
                if (!strcasecmp((char *)names->data, buddy)) {
                        b->in_room = g_list_remove(b->in_room, names->data);
                        update_chat_list(b);
                        break;
                }
                names = names->next;
        }
}


static void im_callback(GtkWidget *w, struct buddy_chat *b)
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

static void ignore_callback(GtkWidget *w, struct buddy_chat *b)
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

static void info_callback(GtkWidget *w, struct buddy_chat *b)
{
        char *name;
        GList *i;

        i = GTK_LIST(b->list)->selection;
        if (i)
                name = (char *)gtk_object_get_user_data(GTK_OBJECT(i->data));
        else
                return;

        serv_get_info(name);
}



void show_new_buddy_chat(struct buddy_chat *b)
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
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	b->window = win;

	vpaned = gtk_vpaned_new();
	hpaned = gtk_hpaned_new();

	gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, TRUE);

	close = gtk_button_new_with_label("Close");
	invite_btn = gtk_button_new_with_label("Invite");
	whisper = gtk_button_new_with_label("Whisper");
        send = gtk_button_new_with_label("Send");

        im = gtk_button_new_with_label("IM");
        ignore = gtk_button_new_with_label("Ignore");
        info = gtk_button_new_with_label("Info");

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


	b->makesound=1;

	gtk_object_set_user_data(GTK_OBJECT(chatentry), b);
	b->entry = chatentry;
	
	/* Hack something so we know have an entry click event */

	gtk_signal_connect(GTK_OBJECT(chatentry), "activate", GTK_SIGNAL_FUNC(send_callback),b);
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
                if (!strcmp(GTK_LABEL(GTK_BIN(w)->child)->label, "Buddy Chat")) {
                        gtk_tree_remove_items(GTK_TREE(buddies), list);
                        list = GTK_TREE(buddies)->children;
                        if (!list)
                        	break;
                }
                list = list->next;
        }

        if (crs == NULL)
                return;

        item = gtk_tree_item_new_with_label("Buddy Chat");
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
