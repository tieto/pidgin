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
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <aim.h>
extern int gaim_caps;
#include "gaim.h"

static int idle_timer = -1;
static time_t lastsent = 0;
static time_t login_time = 0;
static struct timeval lag_tv;
static int is_idle = 0;

int correction_time = 0;

int serv_login(char *username, char *password)
{
	if (!(general_options & OPT_GEN_USE_OSCAR)) {
		USE_OSCAR = 0;
	        return toc_login(username, password);
	} else {
		USE_OSCAR = 1;
		debug_print("Logging in using Oscar. Expect problems.\n");
		return oscar_login(username, password);
	}
}

void serv_close()
{
	if (!USE_OSCAR)
		toc_close();
	else
		oscar_close();

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
	struct conversation *cnv = find_conversation(name);
	if (cnv && cnv->is_direct) {
		if (!USE_OSCAR) {
			/* FIXME */
		} else {
			sprintf(debug_buff, "Sending DirectIM to %s\n", name);
			debug_print(debug_buff);
			aim_send_im_direct(gaim_sess, cnv->conn, message);
		}
	} else {
		if (!USE_OSCAR) {
			char buf[MSG_LEN - 7];

		        g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
		                   message, ((away) ? " auto" : ""));
			sflap_send(buf, strlen(buf), TYPE_DATA);
		} else {
			if (away)
				aim_send_im(gaim_sess, gaim_conn, name, AIM_IMFLAGS_AWAY, message);
			else
				aim_send_im(gaim_sess, gaim_conn, name, AIM_IMFLAGS_ACK, message);
		}
	}
        if (!away)
                serv_touch_idle();
}

void serv_get_info(char *name)
{
	if (!USE_OSCAR) {
        	char buf[MSG_LEN];
	        g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
	        sflap_send(buf, -1, TYPE_DATA);
	} else {
		aim_getinfo(gaim_sess, gaim_conn, name, AIM_GETINFO_GENERALINFO);
	}
}

void serv_get_away_msg(char *name)
{
	if (!USE_OSCAR) {
		/* HAHA! TOC doesn't have this yet */
	} else {
		aim_getinfo(gaim_sess, gaim_conn, name, AIM_GETINFO_AWAYMESSAGE);
	}
}

void serv_get_dir(char *name)
{
	if (!USE_OSCAR) {
		char buf[MSG_LEN];
		g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
		sflap_send(buf, -1, TYPE_DATA);
	}
}

void serv_set_dir(char *first, char *middle, char *last, char *maiden,
		  char *city, char *state, char *country, int web)
{
	if (!USE_OSCAR) {
	char buf2[BUF_LEN*4], buf[BUF_LEN];
	g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
		   middle, last, maiden, city, state, country,
		   (web == 1) ? "Y" : "");
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
	sflap_send(buf, -1, TYPE_DATA);
	}
}

void serv_dir_search(char *first, char *middle, char *last, char *maiden,
		     char *city, char *state, char *country, char *email)
{
	if (!USE_OSCAR) {
		char buf[BUF_LONG];
		g_snprintf(buf, sizeof(buf)/2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle, last, maiden, city, state, country, email);
		sprintf(debug_buff,"Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden, city, state, country);
		debug_print(debug_buff);
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		if (strlen(email))
			aim_usersearch_address(gaim_sess, gaim_conn, email);
	}
}


void serv_set_away(char *message)
{
	if (!USE_OSCAR) {
	        char buf[MSG_LEN];
	        if (message)
	                g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", message);
	        else
	                g_snprintf(buf, MSG_LEN, "toc_set_away \"\"");
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		aim_bos_setprofile(gaim_sess, gaim_conn, current_user->user_info,
					message, gaim_caps);
	}
}

void serv_set_info(char *info)
{
	if (!USE_OSCAR) {
		char buf[MSG_LEN];
		g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", info);
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		if (awaymessage)
			aim_bos_setprofile(gaim_sess, gaim_conn, info,
						awaymessage->message, gaim_caps);
		else
			aim_bos_setprofile(gaim_sess, gaim_conn, info,
						NULL, gaim_caps);
	}
}

void serv_change_passwd(char *orig, char *new) {
	if (!USE_OSCAR) {
		char *buf = g_malloc(BUF_LONG); 
		g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new);
		sflap_send(buf, strlen(buf), TYPE_DATA);
		g_free(buf);
	} else {
		/* FIXME */
	}
}

void serv_add_buddy(char *name)
{
	if (!USE_OSCAR) {
		char buf[1024];
		g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		aim_add_buddy(gaim_sess, gaim_conn, name);
	}
}

void serv_add_buddies(GList *buddies)
{
	if (!USE_OSCAR) {
		char buf[MSG_LEN];
	        int n, num = 0;

	        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	        while(buddies) {
			/* i don't know why we choose 8, it just seems good */
	                if (strlen(normalize(buddies->data)) > MSG_LEN - n - 8) {
	                        sflap_send(buf, -1, TYPE_DATA);
	                        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	                        num = 0;
	                }
	                ++num;
	                n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
	                buddies = buddies->next;
	        }
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		char buf[MSG_LEN];
		int n = 0;
		while(buddies) {
			if (n > MSG_LEN - 18) {
				aim_bos_setbuddylist(gaim_sess, gaim_conn, buf);
				n = 0;
			}
			n += g_snprintf(buf + n, sizeof(buf) - n, "%s&",
					(char *)buddies->data);
			buddies = buddies->next;
		}
		aim_bos_setbuddylist(gaim_sess, gaim_conn, buf);
	}
}


void serv_remove_buddy(char *name)
{
	if (!USE_OSCAR) {
		char buf[1024];
		g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		aim_remove_buddy(gaim_sess, gaim_conn, name);
	}
}

void serv_add_permit(char *name)
{
	permdeny = 3;
	build_permit_tree();
}



void serv_add_deny(char *name)
{
	permdeny = 4;
	build_permit_tree();
}



void serv_set_permit_deny()
{
	if (!USE_OSCAR) {
		char buf[MSG_LEN];
		int at;
		GList *list;

		switch (permdeny) {
		case PERMIT_ALL:
			sprintf(buf, "toc_add_permit %s", current_user->username);
			sflap_send(buf, -1, TYPE_DATA);
			sprintf(buf, "toc_add_deny");
			sflap_send(buf, -1, TYPE_DATA);
			break;
		case PERMIT_NONE:
			sprintf(buf, "toc_add_deny %s", current_user->username);
			sflap_send(buf, -1, TYPE_DATA);
			sprintf(buf, "toc_add_permit");
			sflap_send(buf, -1, TYPE_DATA);
			break;
		case PERMIT_SOME:
			at = g_snprintf(buf, sizeof(buf), "toc_add_permit");
			list = permit;
			while (list) {
				at += g_snprintf(&buf[at], sizeof(buf) - at, " %s", normalize(list->data));
				list = list->next;
			}
			buf[at] = 0; /* is this necessary? */
			sflap_send(buf, -1, TYPE_DATA);
			break;
		case DENY_SOME:
			/* you'll still see people as being online, but they won't see you, and you
			 * won't get updates for them. that's why i thought this was broken. */
			at = g_snprintf(buf, sizeof(buf), "toc_add_deny");
			list = deny;
			while (list) {
				at += g_snprintf(&buf[at], sizeof(buf) - at, " %s", normalize(list->data));
				list = list->next;
			}
			buf[at] = 0; /* is this necessary? */
			sflap_send(buf, -1, TYPE_DATA);
			break;
		}
	} else {
/*
		int at;
		GList *list;
		char buf[MSG_LEN];

		switch (permdeny) {
		case PERMIT_ALL:
			aim_bos_changevisibility(gaim_sess, gaim_conn,
			   AIM_VISIBILITYCHANGE_DENYADD, current_user->username);
			break;
		case PERMIT_NONE:
			aim_bos_changevisibility(gaim_sess, gaim_conn,
			   AIM_VISIBILITYCHANGE_PERMITADD, current_user->username);
			break;
		case PERMIT_SOME:
			at = g_snprintf(buf, sizeof(buf), "%s", current_user->username);
			list = permit;
			while (list) {
				at += g_snprintf(&buf[at], sizeof(buf) - at, "&");
				at += g_snprintf(&buf[at], sizeof(buf) - at, "%s", list->data);
				list = list->next;
			}
			aim_bos_changevisibility(gaim_sess, gaim_conn,
			   AIM_VISIBILITYCHANGE_PERMITADD, buf);
			break;
		case DENY_SOME:
			if (deny) {
				at = 0;
				list = deny;
				while (list) {
					at += g_snprintf(&buf[at], sizeof(buf) - at, "%s", list->data);
					list = list->next;
					if (list)
						at += g_snprintf(&buf[at], sizeof(buf) - at, "&");
				}
				sprintf(debug_buff, "denying %s\n", buf);
				debug_print(debug_buff);
				aim_bos_changevisibility(gaim_sess, gaim_conn,
				   AIM_VISIBILITYCHANGE_DENYADD, buf);
			} else {
				aim_bos_changevisibility(gaim_sess, gaim_conn,
				   AIM_VISIBILITYCHANGE_DENYADD, current_user->username);
			}
			break;
		}
*/
	}
}

void serv_set_idle(int time)
{
	if (!USE_OSCAR) {
		char buf[256];
		g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
		sflap_send(buf, -1, TYPE_DATA);
	} else {
		aim_bos_setidle(gaim_sess, gaim_conn, time);
	}
}


void serv_warn(char *name, int anon)
{
	if (!USE_OSCAR) {
		char *send = g_malloc(256);
		g_snprintf(send, 255, "toc_evil %s %s", name,
			   ((anon) ? "anon" : "norm"));
		sflap_send(send, -1, TYPE_DATA);
		g_free(send);
	}
}

void serv_build_config(char *buf, int len, gboolean show) {
	toc_build_config(buf, len, show);
}


void serv_save_config()
{
	if (!USE_OSCAR) {
		char *buf = g_malloc(BUF_LONG);
		char *buf2 = g_malloc(MSG_LEN);
		serv_build_config(buf, BUF_LONG / 2, FALSE);
		g_snprintf(buf2, MSG_LEN, "toc_set_config {%s}", buf);
	        sflap_send(buf2, -1, TYPE_DATA);
		g_free(buf2);
		g_free(buf);
	}
}


void serv_accept_chat(int i)
{
	if (!USE_OSCAR) {
	        char *buf = g_malloc(256);
	        g_snprintf(buf, 255, "toc_chat_accept %d",  i);
	        sflap_send(buf, -1, TYPE_DATA);
	        g_free(buf);
	} else {
	/* this should never get called because libfaim doesn't use the id
	 * (i'm not even sure Oscar does). go through serv_join_chat instead */
	}
}

void serv_join_chat(int exchange, char *name)
{
	if (!USE_OSCAR) {
	        char buf[BUF_LONG];
	        g_snprintf(buf, sizeof(buf)/2, "toc_chat_join %d \"%s\"", exchange, name);
	        sflap_send(buf, -1, TYPE_DATA);
	} else {
		sprintf(debug_buff, "Attempting to join chat room %s.\n", name);
		debug_print(debug_buff);
		aim_chat_join(gaim_sess, gaim_conn, exchange, name);
	}
}

void serv_chat_invite(int id, char *message, char *name)
{
	if (!USE_OSCAR) {
	        char buf[BUF_LONG];
	        g_snprintf(buf, sizeof(buf)/2, "toc_chat_invite %d \"%s\" %s", id, message, normalize(name));
	        sflap_send(buf, -1, TYPE_DATA);
	} else {
		GList *bcs = buddy_chats;
		struct conversation *b = NULL;

		while (bcs) {
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;
			bcs = bcs->next;
			b = NULL;
		}

		if (!b)
			return;
		
		aim_chat_invite(gaim_sess, gaim_conn, name, message, 0x4, b->name, 0x1);
	}
}

void serv_chat_leave(int id)
{
	if (!USE_OSCAR) {
	        char *buf = g_malloc(256);
	        g_snprintf(buf, 255, "toc_chat_leave %d",  id);
	        sflap_send(buf, -1, TYPE_DATA);
	        g_free(buf);
	} else {
		GList *bcs = buddy_chats;
		struct conversation *b = NULL;
		struct chat_connection *c = NULL;
		int count = 0;

		while (bcs) {
			count++;
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;
			bcs = bcs->next;
			b = NULL;
		}

		if (!b)
			return;

		sprintf(debug_buff, "Attempting to leave room %s (currently in %d rooms)\n",
					b->name, count);
		debug_print(debug_buff);

//		aim_chat_leaveroom(gaim_sess, b->name);
		c = find_oscar_chat(b->name);
		if (c != NULL) {
			oscar_chats = g_list_remove(oscar_chats, c);
			gdk_input_remove(c->inpa);
			aim_conn_kill(gaim_sess, &c->conn);
			g_free(c->name);
			g_free(c);
		}
		/* we do this because with Oscar it doesn't tell us we left */
		serv_got_chat_left(b->id);
	}
}

void serv_chat_whisper(int id, char *who, char *message)
{
	if (!USE_OSCAR) {
	        char buf2[MSG_LEN];
	        g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, who, message);
	        sflap_send(buf2, -1, TYPE_DATA);
	} else {
		do_error_dialog("Sorry, Oscar doesn't whisper. Send an IM. (The last message was not received.)",
				"Gaim - Chat");
	}
}

void serv_chat_send(int id, char *message)
{
	if (!USE_OSCAR) {
	        char buf[MSG_LEN];
		escape_text(message);
	        g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"",id, message);
	        sflap_send(buf, -1, TYPE_DATA);
	} else {
		struct aim_conn_t *cn;
		GList *bcs = buddy_chats;
		struct conversation *b = NULL;

		while (bcs) {
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;
			bcs = bcs->next;
			b = NULL;
		}
		if (!b)
			return;

		cn = aim_chat_getconn(gaim_sess, b->name);
		aim_chat_send_im(gaim_sess, cn, message);
	}
	serv_touch_idle();
}



void serv_got_im(char *name, char *message, int away)
{
	struct conversation *cnv;
	int is_idle = -1;
	int new_conv = 0;

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char **, char **, void *);
	char *buffy = g_strdup(message);
	char *angel = g_strdup(name);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_im_recv && g->function != NULL) {
			function = g->function;
			(*function)(&angel, &buffy, g->data);
		}
		c = c->next;
	}
	if (!buffy || !angel)
		return;
	g_snprintf(message, strlen(message) + 1, "%s", buffy);
	g_free(buffy);
	g_snprintf(name, strlen(name) + 1, "%s", angel);
	g_free(angel);
#endif
	
	if ((general_options & OPT_GEN_TIK_HACK) && awaymessage &&
	    !strcmp(message, ">>>Automated Message: Getting Away Message<<<"))
	{
	    	serv_send_im(name, awaymessage->message, 1);
	    	return;
	}
	
        cnv = find_conversation(name);

	if (general_options & OPT_GEN_SEND_LINKS) {
		linkify_text(message);
	}
	
	if (away)
		away = WFLAG_AUTO;

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
			write_to_conv(cnv, message, away | WFLAG_RECV, NULL);
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
		write_to_conv(cnv, message, away | WFLAG_RECV, NULL);
	}




	if (awaymessage != NULL) {
		time_t t;
		char *tmpmsg;

		time(&t);


		if ((cnv == NULL) || (t - cnv->sent_away) < 120)
			return;

		cnv->sent_away = t;

		if (is_idle)
			is_idle = -1;

		/* apply default fonts and colors */
		tmpmsg = stylize(awaymessage->message, MSG_LEN);
		
		escape_text(tmpmsg);
		escape_message(tmpmsg);
		serv_send_im(name, away_subs(tmpmsg, name), 1);

		if (is_idle == -1)
			is_idle = 1;
		
		if (cnv != NULL)
			write_to_conv(cnv, away_subs(tmpmsg, name), WFLAG_SEND | WFLAG_AUTO, NULL);
		g_free(tmpmsg);
	}
}



void serv_got_update(char *name, int loggedin, int evil, time_t signon, time_t idle, int type, u_short caps)
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
#ifdef GAIM_PLUGINS
	if ((b->uc & UC_UNAVAILABLE) && !(type & UC_UNAVAILABLE)) {
		GList *c = callbacks;
		struct gaim_callback *g;
		void (*function)(char *, void *);
		while (c) {
			g = (struct gaim_callback *)c->data;
			if (g->event == event_buddy_back &&
					g->function != NULL) { 
				function = g->function;
				(*function)(b->name, g->data);
			}
			c = c->next;
		}
	}
#endif
        b->uc = type;
	if (caps) b->caps = caps;
        
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


#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, int, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_warned && g->function != NULL) {
			function = g->function;
			(*function)(name, lev, g->data);
		}
		c = c->next;
	}
#endif

        g_snprintf(buf2, 1023, "You have just been warned by %s.\nYour new warning level is %d%%",
                   ((name == NULL) ? "an anonymous person" : name) , lev);


        d = gtk_dialog_new();
        gtk_widget_realize(d);
        aol_icon(d->window);

        label = gtk_label_new(buf2);
        gtk_widget_show(label);
        close = gtk_button_new_with_label("Close");
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
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
	if (!USE_OSCAR) {
	        int i = (int)gtk_object_get_user_data(GTK_OBJECT(w2));
	        serv_accept_chat(i);
		gtk_widget_destroy(w2);
	} else {
		char *i = (char *)gtk_object_get_user_data(GTK_OBJECT(w2));
		int id = (int)gtk_object_get_user_data(GTK_OBJECT(w));
		serv_join_chat(id, i);
		g_free(i);
		gtk_widget_destroy(w2);
	}
}



void serv_got_chat_invite(char *name, int id, char *who, char *message)
{
        GtkWidget *d;
        GtkWidget *label;
        GtkWidget *yesbtn;
        GtkWidget *nobtn;

        char buf2[BUF_LONG];


#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, char *, char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_invited && g->function != NULL) {
			function = g->function;
			(*function)(who, name, message, g->data);
		}
		c = c->next;
	}
#endif

	if (message)
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites you to buddy chat room: '%s'\n%s", who, name, message);
	else
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites you to buddy chat room: '%s'\n", who, name);

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

	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(yesbtn), GTK_RELIEF_NONE);
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(nobtn), GTK_RELIEF_NONE);

        /*		gtk_widget_set_usize(d, 200, 110); */

	if (!USE_OSCAR)
	        gtk_object_set_user_data(GTK_OBJECT(d), (void *)id);
	else {
		gtk_object_set_user_data(GTK_OBJECT(d), (void *)g_strdup(name));
		gtk_object_set_user_data(GTK_OBJECT(yesbtn), (void *)id);
	}


        gtk_window_set_title(GTK_WINDOW(d), "Buddy chat invite");
        gtk_signal_connect(GTK_OBJECT(nobtn), "clicked", GTK_SIGNAL_FUNC(close_invite), d);
        gtk_signal_connect(GTK_OBJECT(yesbtn), "clicked", GTK_SIGNAL_FUNC(chat_invite_callback), d);


        gtk_widget_show(d);
}

void serv_got_joined_chat(int id, char *name)
{
        struct conversation *b;

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_join && g->function != NULL) {
			function = g->function;
			(*function)(name, g->data);
		}
		c = c->next;
	}
#endif

        b = (struct conversation *)g_new0(struct conversation, 1);
        buddy_chats = g_list_append(buddy_chats, b);

	b->is_chat = TRUE;
        b->ignored = NULL;
        b->in_room = NULL;
        b->id = id;
        g_snprintf(b->name, 80, "%s", name);
        show_new_buddy_chat(b);
}

void serv_got_chat_left(int id)
{
        GList *bcs = buddy_chats;
        struct conversation *b = NULL;


        while(bcs) {
                b = (struct conversation *)bcs->data;
                if (id == b->id) {
                        break;
                        }
                b = NULL;
                bcs = bcs->next;
        }

        if (!b)
                return;

#ifdef GAIM_PLUGINS
	{
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_leave && g->function != NULL) {
			function = g->function;
			(*function)(b->name, g->data);
		}
		c = c->next;
	}
	}
#endif

	sprintf(debug_buff, "Leaving room %s.\n", b->name);
	debug_print(debug_buff);

        buddy_chats = g_list_remove(buddy_chats, b);

        g_free(b);
}

void serv_got_chat_in(int id, char *who, int whisper, char *message)
{
        int w;
        GList *bcs = buddy_chats;
        struct conversation *b = NULL;

        while(bcs) {
                b = (struct conversation *)bcs->data;
                if (id == b->id)
                        break;
                bcs = bcs->next;
                b = NULL;

        }
        if (!b)
                return;
        
#ifdef GAIM_PLUGINS
	{
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(char *, char *, char *, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_chat_recv && g->function != NULL) {
			function = g->function;
			(*function)(b->name, who, message, g->data);
		}
		c = c->next;
	}
	}
#endif

        if (whisper)
                w = WFLAG_WHISPER;
        else
                w = 0;

        chat_write(b, who, w, message);
}

void serv_rvous_accept(char *name, char *cookie, char *uid)
{
	/* Oscar doesn't matter here because this won't ever be called for it */
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_rvous_accept %s %s %s", normalize(name),
			cookie, uid);
	sflap_send(buf, strlen(buf), TYPE_DATA);
}

void serv_rvous_cancel(char *name, char *cookie, char *uid)
{
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_rvous_cancel %s %s %s", normalize(name),
			cookie, uid);
	sflap_send(buf, strlen(buf), TYPE_DATA);
}

void serv_do_imimage(GtkWidget *w, char *name) {
	struct conversation *cnv = find_conversation(name);
	if (!cnv) cnv = new_conversation(name);

	if (!USE_OSCAR) {
		/* FIXME */
	} else {
		oscar_do_directim(name);
	}
}

void serv_got_imimage(char *name, char *cookie, char *ip, struct aim_conn_t *conn, int watcher)
{
	if (!USE_OSCAR) {
		/* FIXME */
	} else {
		struct conversation *cnv = find_conversation(name);
		if (!cnv) cnv = new_conversation(name);
		make_direct(cnv, TRUE, conn, watcher);
	}
}
