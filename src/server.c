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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#ifdef USE_OSCAR
#include "../libfaim/aim.h"
#endif
#include "gaim.h"

static int idle_timer = -1;
static time_t lastsent = 0;
static time_t login_time = 0;
static struct timeval lag_tv;
static int is_idle = 0;

int correction_time = 0;

int serv_login(char *username, char *password)
{
#ifndef USE_OSCAR
        return toc_login(username, password);
#else
	return oscar_login(username, password);
#endif
}

void serv_close()
{
#ifndef USE_OSCAR
	toc_close();
#else
        oscar_close();
#endif
        gtk_timeout_remove(idle_timer);
        idle_timer = -1;
}


void serv_touch_idle()
{
	/* Are we idle?  If so, not anymore */
	if (is_idle > 0) {
		is_idle = 0;
                serv_set_idle(0);
        }
        time(&lastsent);
}


static gint check_idle()
{
	time_t t;

        /* Not idle, really...  :) */
        update_all_buddies();

	time(&t);

        gettimeofday(&lag_tv, NULL);
	if (!(general_options & OPT_GEN_SHOW_LAGMETER))
		serv_send_im(current_user->username, LAGOMETER_STR, 0);

	if (report_idle != IDLE_GAIM)
                return TRUE;

	
	if (is_idle)
		return TRUE;

	if ((t - lastsent) > 600) { /* 15 minutes! */
		serv_set_idle((int)t - lastsent);
		is_idle = 1;
        }

#ifdef GAIM_PLUGINS
	{
		GList *c = callbacks;
		struct gaim_callback *g;
		void (*function)(void *);
		while (c) {
			g = (struct gaim_callback *)c->data;
			if (g->event == event_blist_update &&
					g->function != NULL) { 
				function = g->function;
				(*function)(g->data);
			}
			c = c->next;
		}
	}
#endif
        
	return TRUE;

}


void serv_finish_login()
{
        char *buf;

	if (strlen(current_user->user_info)) {
		buf = g_malloc(strlen(current_user->user_info) * 4);
		strcpy(buf, current_user->user_info);
		escape_text(buf);
		serv_set_info(buf);
		g_free(buf);
	}

        if (idle_timer != -1)
                gtk_timeout_remove(idle_timer);
        
        idle_timer = gtk_timeout_add(20000, (GtkFunction)check_idle, NULL);
        serv_touch_idle();

        time(&login_time);

        serv_add_buddy(current_user->username);
        
	if (!(general_options & OPT_GEN_REGISTERED))
	{
		show_register_dialog();
		save_prefs();
	}
}



void serv_send_im(char *name, char *message, int away)
{
	char buf[MSG_LEN - 7];

#ifndef USE_OSCAR
        g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
                   message, ((away) ? " auto" : ""));
	sflap_send(buf, strlen(buf), TYPE_DATA);
#else
	aim_send_im(NULL, normalize(name), ((away) ? 0 : AIM_IMFLAGS_AWAY), message);
#endif
        if (!away)
                serv_touch_idle();
}

void serv_get_info(char *name)
{
#ifndef USE_OSCAR
        char buf[MSG_LEN];
        g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
        sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_get_dir(char *name)
{
#ifndef USE_OSCAR
        char buf[MSG_LEN];
        g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
        sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_set_dir(char *first, char *middle, char *last, char *maiden,
		  char *city, char *state, char *country, int web)
{
#ifndef USE_OSCAR
	char buf2[BUF_LEN*4], buf[BUF_LEN];
	g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
		   middle, last, maiden, city, state, country,
		   (web == 1) ? "Y" : "");
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
	sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_dir_search(char *first, char *middle, char *last, char *maiden,
		     char *city, char *state, char *country, char *email)
{
#ifndef USE_OSCAR
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf)/2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle, last, maiden, city, state, country, email);
	sprintf(debug_buff,"Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden, city, state, country);
	debug_print(debug_buff);
	sflap_send(buf, -1, TYPE_DATA);
#endif
}


void serv_set_away(char *message)
{
#ifndef USE_OSCAR
        char buf[MSG_LEN];
        if (message)
                g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", message);
        else
                g_snprintf(buf, MSG_LEN, "toc_set_away");
	sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_set_info(char *info)
{
	char buf[MSG_LEN];
#ifndef USE_OSCAR
	g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", info);
	sflap_send(buf, -1, TYPE_DATA);
#else
	g_snprintf(buf, sizeof(buf), "%s\n", info);
	aim_bos_setprofile(gaim_conn, buf);
#endif
}

void serv_add_buddy(char *name)
{
#ifndef USE_OSCAR
	char buf[1024];
	g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
	sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_add_buddies(GList *buddies)
{
	char buf[MSG_LEN];
        int n, num = 0;
#ifndef USE_OSCAR
	
        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
        while(buddies) {
                if (num == 20) {
                        sflap_send(buf, -1, TYPE_DATA);
                        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
                        num = 0;
                }
                ++num;
                n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
                buddies = buddies->next;
        }
	sflap_send(buf, -1, TYPE_DATA);
#else
	while(buddies) {
		if (num == 20) {
                        aim_bos_setbuddylist(gaim_conn, buf);
                        num = 0;
                }
                ++num;
                n += g_snprintf(buf + n, sizeof(buf) - n, "%s&", normalize(buddies->data));
                buddies = buddies->next;
	}
	aim_bos_setbuddylist(gaim_conn, buf);
#endif
}


void serv_remove_buddy(char *name)
{
#ifndef USE_OSCAR
	char buf[1024];
	g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
	sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_add_permit(char *name)
{
#ifndef USE_OSCAR
	char buf[1024];
	g_snprintf(buf, sizeof(buf), "toc_add_permit %s", normalize(name));
	sflap_send(buf, -1, TYPE_DATA);
#endif
}



void serv_add_deny(char *name)
{
#ifndef USE_OSCAR
	char buf[1024];
	g_snprintf(buf, sizeof(buf), "toc_add_deny %s", normalize(name));
	sflap_send(buf, -1, TYPE_DATA);
#endif
}



void serv_set_permit_deny()
{
#ifndef USE_OSCAR
	char buf[MSG_LEN];
	int at;
	GList *list;
        /* FIXME!  We flash here. */
        if (permdeny == 1 || permdeny == 3) {
        	g_snprintf(buf, sizeof(buf), "toc_add_permit");
                sflap_send(buf, -1, TYPE_DATA);
        } else {
                g_snprintf(buf, sizeof(buf), "toc_add_deny");
                sflap_send(buf, -1, TYPE_DATA);
        }


	at = g_snprintf(buf, sizeof(buf), "toc_add_permit");
	list = permit;
	while(list) {
                at += g_snprintf(&buf[at], sizeof(buf) - at, " %s", normalize(list->data));
                list = list->next;
	}
	buf[at] = 0;
	sflap_send(buf, -1, TYPE_DATA);

	at = g_snprintf(buf, sizeof(buf), "toc_add_deny");
	list = deny;
	while(list) {
                at += g_snprintf(&buf[at], sizeof(buf) - at, " %s", normalize(list->data));
		list = list->next;
	}
	buf[at] = 0;
	sflap_send(buf, -1, TYPE_DATA);



#endif
}

void serv_set_idle(int time)
{
#ifndef USE_OSCAR
	char buf[256];
	g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
	sflap_send(buf, -1, TYPE_DATA);
#endif
}


void serv_warn(char *name, int anon)
{
#ifndef USE_OSCAR
	char *send = g_malloc(256);
	g_snprintf(send, 255, "toc_evil %s %s", name,
		   ((anon) ? "anon" : "norm"));
	sflap_send(send, -1, TYPE_DATA);
	g_free(send);
#endif
}


void serv_save_config()
{
#ifndef USE_OSCAR
	char *buf = g_malloc(BUF_LONG);
	char *buf2 = g_malloc(MSG_LEN);
	toc_build_config(buf, BUF_LONG / 2);
	g_snprintf(buf2, MSG_LEN, "toc_set_config {%s}", buf);
        sflap_send(buf2, -1, TYPE_DATA);
	g_free(buf2);
	g_free(buf);
#else
	FILE *f;
	char *buf = g_malloc(BUF_LONG);
	char file[1024];

	g_snprintf(file, sizeof(file), "%s/.gaimbuddy", getenv("HOME"));

        if ((f = fopen(file,"w"))) {
                build_config(buf, BUF_LONG - 1);
                fprintf(f, "%s\n", buf);
                fclose(f);
                chmod(buf, S_IRUSR | S_IWUSR);
        } else {
                g_snprintf(buf, BUF_LONG / 2, "Error writing file %s", file);
                do_error_dialog(buf, "Error");
        }
        
        g_free(buf);

#endif
	
}


void serv_accept_chat(int i)
{
#ifndef USE_OSCAR
        char *buf = g_malloc(256);
        g_snprintf(buf, 255, "toc_chat_accept %d",  i);
        sflap_send(buf, -1, TYPE_DATA);
        g_free(buf);
#endif
}

void serv_join_chat(int exchange, char *name)
{
#ifndef USE_OSCAR
        char buf[BUF_LONG];
        g_snprintf(buf, sizeof(buf)/2, "toc_chat_join %d \"%s\"", exchange, name);
        sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_chat_invite(int id, char *message, char *name)
{
#ifndef USE_OSCAR
        char buf[BUF_LONG];
        g_snprintf(buf, sizeof(buf)/2, "toc_chat_invite %d \"%s\" %s", id, message, normalize(name));
        sflap_send(buf, -1, TYPE_DATA);
#endif
}

void serv_chat_leave(int id)
{
#ifndef USE_OSCAR
        char *buf = g_malloc(256);
        g_snprintf(buf, 255, "toc_chat_leave %d",  id);
        sflap_send(buf, -1, TYPE_DATA);
        g_free(buf);
#endif
}

void serv_chat_whisper(int id, char *who, char *message)
{
#ifndef USE_OSCAR
        char buf2[MSG_LEN];
        g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, who, message);
        sflap_send(buf2, -1, TYPE_DATA);
#endif
}

void serv_chat_send(int id, char *message)
{
#ifndef USE_OSCAR
        char buf[MSG_LEN];
        g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"",id, message);
        sflap_send(buf, -1, TYPE_DATA);
#endif
}




void serv_got_im(char *name, char *message, int away)
{
	struct conversation *cnv;
        int is_idle = -1;
        int new_conv = 0;
	char *nname;

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char **, char **, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_im_recv && g->function != NULL) {
			function = g->function;
			/* I can guarantee you this is wrong */
			(*function)(&name, &message, g->data);
		}
		c = c->next;
	}
	/* make sure no evil plugin is trying to crash gaim */
	if (message == NULL)
		return;
#endif

	nname = g_strdup(normalize(name));

	if (!strcasecmp(normalize(name), nname)) {
	if (!(general_options & OPT_GEN_SHOW_LAGMETER))
	{
		if (!strcmp(message, LAGOMETER_STR)) {
			struct timeval tv;
                        int ms;

			gettimeofday(&tv, NULL);

			ms = 1000000 * (tv.tv_sec - lag_tv.tv_sec);

			ms += tv.tv_usec - lag_tv.tv_usec;

                        update_lagometer(ms);
			g_free(nname);
                        return;
		}
	}
	}
	g_free(nname);
	
        cnv = find_conversation(name);

	if (awaymessage != NULL) {
		if (!(general_options & OPT_GEN_DISCARD_WHEN_AWAY)) {
			if (cnv == NULL) {
				new_conv = 1;
				cnv = new_conversation(name);
			}
		}
		if (cnv != NULL) {
			if (sound_options & OPT_SOUND_WHEN_AWAY)
				play_sound(AWAY);
			write_to_conv(cnv, message, WFLAG_AUTO | WFLAG_RECV);
		}

	} else {
		if (cnv == NULL) {
			new_conv = 1;
			cnv = new_conversation(name);
		}
		if (new_conv && (sound_options & OPT_SOUND_FIRST_RCV)) {
			play_sound(FIRST_RECEIVE);
		} else {
			if (cnv->makesound && (sound_options & OPT_SOUND_RECV))
				play_sound(RECEIVE);
		}
		write_to_conv(cnv, message, WFLAG_RECV);
	}




        if (awaymessage != NULL) {
                time_t t;

                time(&t);


                if ((cnv == NULL) || (t - cnv->sent_away) < 120)
                        return;

                cnv->sent_away = t;

                if (is_idle)
                        is_idle = -1;

                serv_send_im(name, awaymessage->message, 1);

                if (is_idle == -1)
			is_idle = 1;
		
                if (cnv != NULL)
			write_to_conv(cnv, awaymessage->message, WFLAG_SEND | WFLAG_AUTO);
        }
}



void serv_got_update(char *name, int loggedin, int evil, time_t signon, time_t idle, int type)
{
        struct buddy *b;
        char *nname;
                     
        b = find_buddy(name);

        nname = g_strdup(normalize(name));
        if (!strcasecmp(nname, normalize(current_user->username))) {
                correction_time = (int)(signon - login_time);
                update_all_buddies();
                if (!b) {
			g_free(nname);
                        return;
		}
        }

	g_free(nname);
        
        if (!b) {
                sprintf(debug_buff,"Error, no such person\n");
				debug_print(debug_buff);
                return;
        }

        /* This code will 'align' the name from the TOC */
        /* server with what's in our record.  We want to */
        /* store things how THEY want it... */
        if (strcmp(name, b->name)) {
                GList  *cnv = conversations;
                struct conversation *cv;

                char *who = g_malloc(80);

                strcpy(who, normalize(name));

                while(cnv) {
                        cv = (struct conversation *)cnv->data;
                        if (!strcasecmp(who, normalize(cv->name))) {
                                g_snprintf(cv->name, sizeof(cv->name), "%s", name);
                                if (find_log_info(name) || (general_options & OPT_GEN_LOG_ALL))
                                        g_snprintf(who, 63, LOG_CONVERSATION_TITLE, name);
                                else
                                        g_snprintf(who, 63, CONVERSATION_TITLE, name);
                                gtk_window_set_title(GTK_WINDOW(cv->window), who);
				/* was g_free(buf), but break gives us that
				 * and freeing twice is not good --Sumner */
                                break;
                        }
                        cnv = cnv->next;
                }
		g_free(who); 
                g_snprintf(b->name, sizeof(b->name), "%s", name);
                /*gtk_label_set_text(GTK_LABEL(b->label), b->name);*/

                /* okay lets save the new config... */

        }

        b->idle = idle;
        b->evil = evil;
        b->uc = type;
        
        b->signon = signon;

        if (loggedin) {
                if (!b->present) {
                        b->present = 1;
                        do_pounce(b->name);
                }
        } else
                b->present = 0;

        set_buddy(b);
}

static
void close_warned(GtkWidget *w, GtkWidget *w2)
{
        gtk_widget_destroy(w2);
}



void serv_got_eviled(char *name, int lev)
{
        char *buf2 = g_malloc(1024);
        GtkWidget *d, *label, *close;


        g_snprintf(buf2, 1023, "You have just been warned by %s.\nYour new warning level is %d./%%",
                   ((name == NULL) ? "an anonymous person" : name) , lev);


        d = gtk_dialog_new();
        gtk_widget_realize(d);
        aol_icon(d->window);

        label = gtk_label_new(buf2);
        gtk_widget_show(label);
        close = gtk_button_new_with_label("Close");
        gtk_widget_show(close);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),
                           label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area),
                           close, FALSE, FALSE, 5);

        gtk_window_set_title(GTK_WINDOW(d), "Warned");
        gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(close_warned), d);
        gtk_widget_show(d);
}



static void close_invite(GtkWidget *w, GtkWidget *w2)
{
	gtk_widget_destroy(w2);
}

static void chat_invite_callback(GtkWidget *w, GtkWidget *w2)
{
        int i = (int)gtk_object_get_user_data(GTK_OBJECT(w2));
        serv_accept_chat(i);
	gtk_widget_destroy(w2);
}



void serv_got_chat_invite(char *name, int id, char *who, char *message)
{
        GtkWidget *d;
        GtkWidget *label;
        GtkWidget *yesbtn;
        GtkWidget *nobtn;

        char buf2[BUF_LONG];


        g_snprintf(buf2, sizeof(buf2), "User '%s' invites you to buddy chat room: '%s'\n%s", who, name, message);

        d = gtk_dialog_new();
        gtk_widget_realize(d);
        aol_icon(d->window);


        label = gtk_label_new(buf2);
        gtk_widget_show(label);
        yesbtn = gtk_button_new_with_label("Yes");
        gtk_widget_show(yesbtn);
        nobtn = gtk_button_new_with_label("No");
        gtk_widget_show(nobtn);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),
                           label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area),
                           yesbtn, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area),
                           nobtn, FALSE, FALSE, 5);


        /*		gtk_widget_set_usize(d, 200, 110); */
        gtk_object_set_user_data(GTK_OBJECT(d), (void *)id);


        gtk_window_set_title(GTK_WINDOW(d), "Buddy chat invite");
        gtk_signal_connect(GTK_OBJECT(nobtn), "clicked", GTK_SIGNAL_FUNC(close_invite), d);
        gtk_signal_connect(GTK_OBJECT(yesbtn), "clicked", GTK_SIGNAL_FUNC(chat_invite_callback), d);


        gtk_widget_show(d);
}

void serv_got_joined_chat(int id, char *name)
{
        struct buddy_chat *b;

        b = (struct buddy_chat *)g_new0(struct buddy_chat, 1);
        buddy_chats = g_list_append(buddy_chats, b);

        b->ignored = NULL;
        b->in_room = NULL;
        b->id = id;
        g_snprintf(b->name, 80, "%s", name);
        show_new_buddy_chat(b);
}

void serv_got_chat_left(int id)
{
        GList *bcs = buddy_chats;
        struct buddy_chat *b = NULL;


        while(bcs) {
                b = (struct buddy_chat *)bcs->data;
                if (id == b->id) {
                        break;
                        }
                b = NULL;
                bcs = bcs->next;
        }

        if (!b)
                return;

        if (b->window)
                gtk_widget_destroy(GTK_WIDGET(b->window));

        buddy_chats = g_list_remove(buddy_chats, b);

        g_free(b);
}

void serv_got_chat_in(int id, char *who, int whisper, char *message)
{
        int w;
        GList *bcs = buddy_chats;
        struct buddy_chat *b = NULL;

        while(bcs) {
                b = (struct buddy_chat *)bcs->data;
                if (id == b->id)
                        break;
                bcs = bcs->next;
                b = NULL;

        }
        if (!b)
                return;
        
        if (whisper)
                w = WFLAG_WHISPER;
        else
                w = 0;

        chat_write(b, who, w, message);
}
        

