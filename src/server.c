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
#include "multi.h"
#include "gaim.h"

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"

int correction_time = 0;

struct gaim_connection *serv_login(char *username, char *password)
{
	struct aim_user *u = find_user(username);

	if (u->protocol == PROTO_TOC) {
	        return toc_login(username, password);
	} else if (u->protocol == PROTO_OSCAR) {
		debug_print("Logging in using Oscar. Expect problems.\n");
		return oscar_login(username, password);
	} else {
		/* PRPL */
		return NULL;
	}
}

void serv_close(struct gaim_connection *gc)
{
	if (gc->protocol == PROTO_TOC)
		toc_close(gc);
	else if (gc->protocol == PROTO_OSCAR)
		oscar_close(gc);
	else /* PRPL */ ;

	account_offline(gc);
	destroy_gaim_conn(gc);

	if (connections) return;

	if (gc->idle_timer > 0)
		gtk_timeout_remove(gc->idle_timer);
        gc->idle_timer = -1;
}


void serv_touch_idle(struct gaim_connection *gc)
{
	/* Are we idle?  If so, not anymore */
	if (gc->is_idle > 0) {
		gc->is_idle = 0;
                serv_set_idle(gc, 0);
        }
        time(&gc->lastsent);
}


static gint check_idle(struct gaim_connection *gc)
{
	time_t t;

        /* Not idle, really...  :) */
        update_all_buddies();

	plugin_event(event_blist_update, 0, 0, 0);
        
	time(&t);

	if (report_idle != IDLE_GAIM)
                return TRUE;

	
	if (gc->is_idle)
		return TRUE;

	if ((t - gc->lastsent) > 600) { /* 15 minutes! */
		serv_set_idle(gc, (int)t - gc->lastsent);
		gc->is_idle = 1;
        }

	return TRUE;

}


void serv_finish_login(struct gaim_connection *gc)
{
        char *buf;

	if (strlen(gc->user_info)) {
		buf = g_malloc(strlen(gc->user_info) * 4);
		strcpy(buf, gc->user_info);
		serv_set_info(gc, buf);
		g_free(buf);
	}

        if (gc->idle_timer > 0)
                gtk_timeout_remove(gc->idle_timer);
        
        gc->idle_timer = gtk_timeout_add(20000, (GtkFunction)check_idle, gc);
        serv_touch_idle(gc);

        time(&gc->login_time);

        serv_add_buddy(gc->username);
}



void serv_send_im(char *name, char *message, int away)
{
	struct conversation *cnv = find_conversation(name);
	if (cnv && cnv->is_direct) {
		if (cnv->gc->protocol == PROTO_OSCAR) {
			debug_printf("Sending DirectIM to %s\n", name);
			aim_send_im_direct(cnv->gc->oscar_sess, cnv->conn, message);
		} else {
			/* Direct IM TOC FIXME */
		}
	} else {
		if (cnv->gc->protocol == PROTO_TOC) {
			char buf[MSG_LEN - 7];

			escape_text(message);
		        g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
		                   message, ((away) ? " auto" : ""));
			sflap_send(cnv->gc, buf, -1, TYPE_DATA);
		} else if (cnv->gc->protocol == PROTO_OSCAR) {
			if (away)
				aim_send_im(cnv->gc->oscar_sess, cnv->gc->oscar_conn,
						name, AIM_IMFLAGS_AWAY, message);
			else
				aim_send_im(cnv->gc->oscar_sess, cnv->gc->oscar_conn,
						name, AIM_IMFLAGS_ACK, message);
		}
	}
        if (!away)
                serv_touch_idle(cnv->gc);
}

void serv_get_info(char *name)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
        	char buf[MSG_LEN];
	        g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
	        sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_getinfo(g->oscar_sess, g->oscar_conn, name, AIM_GETINFO_GENERALINFO);
	}
}

void serv_get_away_msg(char *name)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		/* HAHA! TOC doesn't have this yet */
	} else if (g->protocol == PROTO_OSCAR) {
		aim_getinfo(g->oscar_sess, g->oscar_conn, name, AIM_GETINFO_AWAYMESSAGE);
	}
}

void serv_get_dir(char *name)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[MSG_LEN];
		g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
		sflap_send(g, buf, -1, TYPE_DATA);
	}
}

void serv_set_dir(char *first, char *middle, char *last, char *maiden,
		  char *city, char *state, char *country, int web)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf2[BUF_LEN*4], buf[BUF_LEN];
		g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
			   middle, last, maiden, city, state, country,
			   (web == 1) ? "Y" : "");
		escape_text(buf2);
		g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		/* FIXME : some of these things are wrong, but i'm lazy */
		aim_setdirectoryinfo(g->oscar_sess, g->oscar_conn, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
	}
}

void serv_dir_search(char *first, char *middle, char *last, char *maiden,
		     char *city, char *state, char *country, char *email)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[BUF_LONG];
		g_snprintf(buf, sizeof(buf)/2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle, last, maiden, city, state, country, email);
		sprintf(debug_buff,"Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden, city, state, country);
		debug_print(debug_buff);
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		if (strlen(email))
			aim_usersearch_address(g->oscar_sess, g->oscar_conn, email);
	}
}


void serv_set_away(char *message)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
	        char buf[MSG_LEN];
	        if (message) {
			escape_text(message);
	                g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", message);
		} else
	                g_snprintf(buf, MSG_LEN, "toc_set_away \"\"");
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_bos_setprofile(g->oscar_sess, g->oscar_conn, g->user_info, message, gaim_caps);
	}
}

void serv_set_info(struct gaim_connection *g, char *info)
{
	if (g->protocol == PROTO_TOC) {
		char buf[MSG_LEN];
		escape_text(info);
		g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", info);
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		if (awaymessage)
			aim_bos_setprofile(g->oscar_sess, g->oscar_conn, info,
						awaymessage->message, gaim_caps);
		else
			aim_bos_setprofile(g->oscar_sess, g->oscar_conn, info,
						NULL, gaim_caps);
	}
}

void serv_change_passwd(char *orig, char *new) {
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char *buf = g_malloc(BUF_LONG); 
		g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new);
		sflap_send(g, buf, strlen(buf), TYPE_DATA);
		g_free(buf);
	} else if (g->protocol == PROTO_OSCAR) {
		/* Oscar change_passwd FIXME */
	}
}

void serv_add_buddy(char *name)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[1024];
		g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_add_buddy(g->oscar_sess, g->oscar_conn, name);
	}
}

void serv_add_buddies(GList *buddies)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[MSG_LEN];
	        int n, num = 0;

	        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	        while(buddies) {
			/* i don't know why we choose 8, it just seems good */
	                if (strlen(normalize(buddies->data)) > MSG_LEN - n - 8) {
	                        sflap_send(g, buf, -1, TYPE_DATA);
	                        n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	                        num = 0;
	                }
	                ++num;
	                n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
	                buddies = buddies->next;
	        }
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		char buf[MSG_LEN];
		int n = 0;
		while(buddies) {
			if (n > MSG_LEN - 18) {
				aim_bos_setbuddylist(g->oscar_sess, g->oscar_conn, buf);
				n = 0;
			}
			n += g_snprintf(buf + n, sizeof(buf) - n, "%s&",
					(char *)buddies->data);
			buddies = buddies->next;
		}
		aim_bos_setbuddylist(g->oscar_sess, g->oscar_conn, buf);
	}
}


void serv_remove_buddy(char *name)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[1024];
		g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_remove_buddy(g->oscar_sess, g->oscar_conn, name);
	}
}

void serv_add_permit(char *name)
{
	permdeny = 3;
	build_permit_tree();
	serv_set_permit_deny();
}



void serv_add_deny(char *name)
{
	permdeny = 4;
	build_permit_tree();
	serv_set_permit_deny();
}



void serv_set_permit_deny()
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char buf[MSG_LEN];
		int at;
		GList *list;

		switch (permdeny) {
		case PERMIT_ALL:
			sprintf(buf, "toc_add_permit %s", g->username);
			sflap_send(g, buf, -1, TYPE_DATA);
			sprintf(buf, "toc_add_deny");
			sflap_send(g, buf, -1, TYPE_DATA);
			break;
		case PERMIT_NONE:
			sprintf(buf, "toc_add_deny %s", g->username);
			sflap_send(g, buf, -1, TYPE_DATA);
			sprintf(buf, "toc_add_permit");
			sflap_send(g, buf, -1, TYPE_DATA);
			break;
		case PERMIT_SOME:
			at = g_snprintf(buf, sizeof(buf), "toc_add_permit");
			list = permit;
			while (list) {
				at += g_snprintf(&buf[at], sizeof(buf) - at, " %s", normalize(list->data));
				list = list->next;
			}
			buf[at] = 0; /* is this necessary? */
			sflap_send(g, buf, -1, TYPE_DATA);
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
			sflap_send(g, buf, -1, TYPE_DATA);
			break;
		}
	} else if (g->protocol == PROTO_OSCAR) {
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

void serv_set_idle(struct gaim_connection *g, int time)
{
	if (g->protocol == PROTO_TOC) {
		char buf[256];
		g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
		sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_bos_setidle(g->oscar_sess, g->oscar_conn, time);
	}
}


void serv_warn(char *name, int anon)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char *send = g_malloc(256);
		g_snprintf(send, 255, "toc_evil %s %s", name,
			   ((anon) ? "anon" : "norm"));
		sflap_send(g, send, -1, TYPE_DATA);
		g_free(send);
	} else if (g->protocol == PROTO_OSCAR) {
		aim_send_warning(g->oscar_sess, g->oscar_conn, name, anon);
	}
}

void serv_build_config(char *buf, int len, gboolean show) {
	toc_build_config(buf, len, show);
}


void serv_save_config()
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	if (g->protocol == PROTO_TOC) {
		char *buf = g_malloc(BUF_LONG);
		char *buf2 = g_malloc(MSG_LEN);
		serv_build_config(buf, BUF_LONG / 2, FALSE);
		g_snprintf(buf2, MSG_LEN, "toc_set_config {%s}", buf);
	        sflap_send(g, buf2, -1, TYPE_DATA);
		g_free(buf2);
		g_free(buf);
	}
}


void serv_accept_chat(struct gaim_connection *g, int i)
{
	if (g->protocol == PROTO_TOC) {
	        char *buf = g_malloc(256);
	        g_snprintf(buf, 255, "toc_chat_accept %d",  i);
	        sflap_send(g, buf, -1, TYPE_DATA);
	        g_free(buf);
	} else if (g->protocol == PROTO_OSCAR) {
		/* this should never get called because libfaim doesn't use the id
		 * (i'm not even sure Oscar does). go through serv_join_chat instead */
	}
}

void serv_join_chat(struct gaim_connection *g, int exchange, char *name)
{
	if (g->protocol == PROTO_TOC) {
	        char buf[BUF_LONG];
	        g_snprintf(buf, sizeof(buf)/2, "toc_chat_join %d \"%s\"", exchange, name);
	        sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		struct aim_conn_t *cur = NULL;
		sprintf(debug_buff, "Attempting to join chat room %s.\n", name);
		debug_print(debug_buff);
		if ((cur = aim_getconn_type(g->oscar_sess, AIM_CONN_TYPE_CHATNAV))) {
			debug_print("chatnav exists, creating room\n");
			aim_chatnav_createroom(g->oscar_sess, cur, name, exchange);
		} else {
			/* this gets tricky */
			debug_print("chatnav does not exist, opening chatnav\n");
			g->create_exchange = exchange;
			g->create_name = g_strdup(name);
			aim_bos_reqservice(g->oscar_sess, g->oscar_conn, AIM_CONN_TYPE_CHATNAV);
		}
	}
}

void serv_chat_invite(struct gaim_connection *g, int id, char *message, char *name)
{
	if (g->protocol == PROTO_TOC) {
	        char buf[BUF_LONG];
	        g_snprintf(buf, sizeof(buf)/2, "toc_chat_invite %d \"%s\" %s", id, message, normalize(name));
	        sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		GSList *bcs = g->buddy_chats;
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
		
		aim_chat_invite(g->oscar_sess, g->oscar_conn, name,
				message ? message : "", 0x4, b->name, 0x0);
	}
}

void serv_chat_leave(struct gaim_connection *g, int id)
{
	if (g->protocol == PROTO_TOC) {
	        char *buf = g_malloc(256);
	        g_snprintf(buf, 255, "toc_chat_leave %d",  id);
	        sflap_send(g, buf, -1, TYPE_DATA);
	        g_free(buf);
	} else if (g->protocol == PROTO_OSCAR) {
		GSList *bcs = g->buddy_chats;
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

		c = find_oscar_chat(g, b->name);
		if (c != NULL) {
			g->oscar_chats = g_slist_remove(g->oscar_chats, c);
			gdk_input_remove(c->inpa);
			if (g && g->oscar_sess)
				aim_conn_kill(g->oscar_sess, &c->conn);
			g_free(c->name);
			g_free(c);
		}
		/* we do this because with Oscar it doesn't tell us we left */
		serv_got_chat_left(g, b->id);
	}
}

void serv_chat_whisper(struct gaim_connection *g, int id, char *who, char *message)
{
	if (g->protocol == PROTO_TOC) {
	        char buf2[MSG_LEN];
	        g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, who, message);
	        sflap_send(g, buf2, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		do_error_dialog("Sorry, Oscar doesn't whisper. Send an IM. (The last message was not received.)",
				"Gaim - Chat");
	}
}

void serv_chat_send(struct gaim_connection *g, int id, char *message)
{
	if (g->protocol == PROTO_TOC) {
	        char buf[MSG_LEN];
		escape_text(message);
	        g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"",id, message);
	        sflap_send(g, buf, -1, TYPE_DATA);
	} else if (g->protocol == PROTO_OSCAR) {
		struct aim_conn_t *cn;
		GSList *bcs = g->buddy_chats;
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

		cn = aim_chat_getconn(g->oscar_sess, b->name);
		aim_chat_send_im(g->oscar_sess, cn, message);
	}
	serv_touch_idle(g);
}



void serv_got_im(struct gaim_connection *gc, char *name, char *message, int away)
{
	struct conversation *cnv;
	int is_idle = -1;
	int new_conv = 0;

	char *buffy = g_strdup(message);
	char *angel = g_strdup(name);
	plugin_event(event_im_recv, &angel, &buffy, 0);
	if (!buffy || !angel)
		return;
	g_snprintf(message, strlen(message) + 1, "%s", buffy);
	g_free(buffy);
	g_snprintf(name, strlen(name) + 1, "%s", angel);
	g_free(angel);
	
	if ((general_options & OPT_GEN_TIK_HACK) && awaymessage &&
	    !strcmp(message, ">>>Automated Message: Getting Away Message<<<"))
	{
		char *tmpmsg = stylize(awaymessage->message, MSG_LEN);
	    	serv_send_im(name, tmpmsg, 1);
		g_free(tmpmsg);
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


	cnv->gc = gc;
	gtk_option_menu_set_history(GTK_OPTION_MENU(cnv->menu), g_slist_index(connections, gc));


	if (awaymessage != NULL) {
		time_t t;
		char *tmpmsg;
		struct buddy *b = find_buddy(name);
		char *alias = b ? b->show : name;

		time(&t);


		if ((cnv == NULL) || (t - cnv->sent_away) < 120)
			return;

		cnv->sent_away = t;

		if (is_idle)
			is_idle = -1;

		/* apply default fonts and colors */
		tmpmsg = stylize(awaymessage->message, MSG_LEN);
		
		if (gc->protocol == PROTO_TOC) {
			escape_text(tmpmsg);
			escape_message(tmpmsg);
		}
		serv_send_im(name, away_subs(tmpmsg, alias), 1);
		g_free(tmpmsg);
		tmpmsg = stylize(awaymessage->message, MSG_LEN);

		if (is_idle == -1)
			is_idle = 1;
		
		if (cnv != NULL)
			write_to_conv(cnv, away_subs(tmpmsg, alias), WFLAG_SEND | WFLAG_AUTO, NULL);
		g_free(tmpmsg);
	}
}



void serv_got_update(char *name, int loggedin, int evil, time_t signon, time_t idle, int type, u_short caps)
{
        struct buddy *b = find_buddy(name);
	struct gaim_connection *gc = find_gaim_conn_by_name(name);
                     
        if (gc) {
                correction_time = (int)(signon - gc->login_time);
                update_all_buddies();
                if (!b) {
                        return;
		}
        }
        
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

	if ((b->uc & UC_UNAVAILABLE) && !(type & UC_UNAVAILABLE)) {
		plugin_event(event_buddy_back, b->name, 0, 0);
	} else if (!(b->uc & UC_UNAVAILABLE) && (type & UC_UNAVAILABLE)) {
		plugin_event(event_buddy_away, b->name, 0, 0);
	}

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


	plugin_event(event_warned, name, (void *)lev, 0);

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
	struct gaim_connection *g = (struct gaim_connection *)
					gtk_object_get_user_data(GTK_OBJECT(GTK_DIALOG(w2)->vbox));
	if (g->protocol == PROTO_TOC) {
	        int i = (int)gtk_object_get_user_data(GTK_OBJECT(w2));
	        serv_accept_chat(g, i);
		gtk_widget_destroy(w2);
	} else if (g->protocol == PROTO_OSCAR) {
		char *i = (char *)gtk_object_get_user_data(GTK_OBJECT(w2));
		int id = (int)gtk_object_get_user_data(GTK_OBJECT(w));
		serv_join_chat(g, id, i);
		g_free(i);
		gtk_widget_destroy(w2);
	}
}



void serv_got_chat_invite(struct gaim_connection *g, char *name, int id, char *who, char *message)
{
        GtkWidget *d;
        GtkWidget *label;
        GtkWidget *yesbtn;
        GtkWidget *nobtn;

        char buf2[BUF_LONG];


	plugin_event(event_chat_invited, who, name, message);

	if (message)
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites %s to buddy chat room: '%s'\n%s", who, g->username, name, message);
	else
		g_snprintf(buf2, sizeof(buf2), "User '%s' invites %s to buddy chat room: '%s'\n", who, g->username, name);

        d = gtk_dialog_new();
        gtk_widget_realize(d);
        aol_icon(d->window);


        label = gtk_label_new(buf2);
        gtk_widget_show(label);
        yesbtn = picture_button(d, _("Yes"), ok_xpm);
        nobtn = picture_button(d, _("No"), cancel_xpm);
        gtk_widget_show(nobtn);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),
                           label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area),
                           yesbtn, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area),
                           nobtn, FALSE, FALSE, 5);

	gtk_object_set_user_data(GTK_OBJECT(GTK_DIALOG(d)->vbox), g);
	if (g->protocol == PROTO_TOC) {
	        gtk_object_set_user_data(GTK_OBJECT(d), (void *)id);
		gtk_object_set_user_data(GTK_OBJECT(GTK_DIALOG(d)->vbox), g);
	} else if (g->protocol == PROTO_OSCAR) {
		gtk_object_set_user_data(GTK_OBJECT(d), (void *)g_strdup(name));
		gtk_object_set_user_data(GTK_OBJECT(yesbtn), (void *)id);
	} else {
		/* PRPL */
	}


        gtk_window_set_title(GTK_WINDOW(d), "Buddy chat invite");
        gtk_signal_connect(GTK_OBJECT(nobtn), "clicked", GTK_SIGNAL_FUNC(close_invite), d);
        gtk_signal_connect(GTK_OBJECT(yesbtn), "clicked", GTK_SIGNAL_FUNC(chat_invite_callback), d);


        gtk_widget_show(d);
}

void serv_got_joined_chat(struct gaim_connection *gc, int id, char *name)
{
        struct conversation *b;

	plugin_event(event_chat_join, name, 0, 0);

        b = (struct conversation *)g_new0(struct conversation, 1);
        gc->buddy_chats = g_slist_append(gc->buddy_chats, b);

	b->is_chat = TRUE;
        b->ignored = NULL;
        b->in_room = NULL;
        b->id = id;
	b->gc = gc;
        g_snprintf(b->name, 80, "%s", name);

	if ((general_options & OPT_GEN_LOG_ALL) || find_log_info(b->name)) {
		FILE *fd;
		char *filename;

		filename = (char *)malloc(100);
		snprintf(filename, 100, "%s.chat", b->name);
		
		fd = open_log_file(filename);
		if (!(general_options & OPT_GEN_STRIP_HTML))
			fprintf(fd, "<HR><BR><H3 Align=Center> ---- New Conversation @ %s ----</H3><BR>\n", full_date());
		else
			fprintf(fd, "---- New Conversation @ %s ----\n", full_date());
		
		fclose(fd);
		free(filename);
	}
	
        show_new_buddy_chat(b);
}

void serv_got_chat_left(struct gaim_connection *g, int id)
{
        GSList *bcs = g->buddy_chats;
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

	plugin_event(event_chat_leave, b->name, 0, 0);

	sprintf(debug_buff, "Leaving room %s.\n", b->name);
	debug_print(debug_buff);

        g->buddy_chats = g_slist_remove(g->buddy_chats, b);

        g_free(b);
}

void serv_got_chat_in(struct gaim_connection *g, int id, char *who, int whisper, char *message)
{
        int w;
        GSList *bcs = g->buddy_chats;
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
        
	plugin_event(event_chat_recv, b->name, who, message);

        if (whisper)
                w = WFLAG_WHISPER;
        else
                w = 0;

        chat_write(b, who, w, message);
}

void serv_rvous_accept(char *name, char *cookie, char *uid)
{
	/* Oscar doesn't matter here because this won't ever be called for it */
	/* FIXME */
	struct gaim_connection *g = connections->data;
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_rvous_accept %s %s %s", normalize(name),
			cookie, uid);
	sflap_send(g, buf, -1, TYPE_DATA);
}

void serv_rvous_cancel(char *name, char *cookie, char *uid)
{
	/* FIXME */
	struct gaim_connection *g = connections->data;
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_rvous_cancel %s %s %s", normalize(name),
			cookie, uid);
	sflap_send(g, buf, -1, TYPE_DATA);
}

void serv_do_imimage(GtkWidget *w, char *name) {
	struct conversation *cnv = find_conversation(name);
	if (!cnv) cnv = new_conversation(name);

	if (cnv->gc->protocol == PROTO_TOC) {
		/* Direct IM TOC FIXME */
	} else if (cnv->gc->protocol == PROTO_OSCAR) {
		oscar_do_directim(cnv->gc, name);
	}
}

void serv_got_imimage(struct gaim_connection *gc, char *name, char *cookie, char *ip,
			struct aim_conn_t *conn, int watcher)
{
	if (gc->protocol == PROTO_TOC) {
		/* Direct IM TOC FIXME */
	} else if (gc->protocol == PROTO_OSCAR) {
		struct conversation *cnv = find_conversation(name);
		if (!cnv) cnv = new_conversation(name);
		make_direct(cnv, TRUE, conn, watcher);
	}
}
