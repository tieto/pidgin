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
#include <netdb.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include "multi.h"
#include "gaim.h"
#include "gnome_applet_mgr.h"

#define REVISION "gaim:$Revision: 976 $"


static unsigned int peer_ver=0;
#ifdef _WIN32
static int win32_r;
#endif

static int toc_signon(struct gaim_connection *);



/* ok. this function used to take username/password, and return 0 on success.
 * now, it takes username/password, and returns NULL on error or a new gaim_connection
 * on success. FIXME: should this modify the UI? or just sign in? */
struct gaim_connection *toc_login(char *username, char *password)
{
	char *config;
        struct in_addr *sin;
        struct aim_user *u;
	struct gaim_connection *gc;
	char buf[80];
	char buf2[2048];

	g_snprintf(buf, sizeof(buf), "Looking up %s", aim_host);	
	set_login_progress(1, buf);

	sin = (struct in_addr *)get_address(aim_host);
	if (!sin) {
	        
#ifdef USE_APPLET
		set_user_state(offline);
#endif /* USE_APPLET */
		g_snprintf(buf, sizeof(buf), "Unable to lookup %s", aim_host);
		hide_login_progress(buf);
		return NULL;
	}
	
	g_snprintf(toc_addy, sizeof(toc_addy), "%s", inet_ntoa(*sin));
	g_snprintf(buf, sizeof(buf), "Connecting to %s", inet_ntoa(*sin));
	
	set_login_progress(2, buf);


	gc = new_gaim_conn(PROTO_TOC, username, password);
	
	gc->toc_fd = connect_address(sin->s_addr, aim_port);

        if (gc->toc_fd < 0) {
#ifdef USE_APPLET
		set_user_state(offline);
#endif /* USE_APPLET */
		g_snprintf(buf, sizeof(buf), "Connect to %s failed",
			 inet_ntoa(*sin));
		hide_login_progress(buf);
		destroy_gaim_conn(gc);
		return NULL;
        }

        g_free(sin);
	
	g_snprintf(buf, sizeof(buf), "Signon: %s",username);
	
	set_login_progress(3, buf);
	
	if (toc_signon(gc) < 0) {
#ifdef USE_APPLET
		set_user_state(offline);
#endif /* USE_APPLET */
		hide_login_progress("Disconnected.");
		destroy_gaim_conn(gc);
		return NULL;
	}

	g_snprintf(buf, sizeof(buf), "Waiting for reply...");
	set_login_progress(4, buf);
	if (toc_wait_signon(gc) < 0) {
#ifdef USE_APPLET
		set_user_state(offline);
#endif /* USE_APPLET */
		hide_login_progress("Authentication Failed");
		destroy_gaim_conn(gc);
		return NULL;
	}

        u = find_user(username);
	sprintf(gc->user_info, "%s", u->user_info);
	gc->options = u->options;
	save_prefs();

	g_snprintf(buf, sizeof(buf), "Retrieving config...");
	set_login_progress(5, buf);
	config = toc_wait_config(gc);
	gc->state = STATE_ONLINE;

	if (mainwindow)
		gtk_widget_hide(mainwindow);
	show_buddy_list();
#ifdef USE_APPLET
	if (general_options & OPT_GEN_APP_BUDDY_SHOW) {
		refresh_buddy_window();
		createOnlinePopup();
                applet_buddy_show = TRUE;
        } else {
                gtk_widget_hide(blist);
                applet_buddy_show = FALSE;
        }

	set_user_state(online);
#else
	refresh_buddy_window();
#endif
	if (config != NULL)
		parse_toc_buddy_list(gc, config, 0);
	else
		do_import(0, gc);
        
	setup_buddy_chats();

	g_snprintf(buf2, sizeof(buf2), "toc_init_done");
	sflap_send(gc, buf2, -1, TYPE_DATA);

	g_snprintf(buf2, sizeof(buf2), "toc_set_caps %s %s %s %s %s",
		   FILE_SEND_UID, FILE_GET_UID, B_ICON_UID, IMAGE_UID,
		   VOICE_UID);
	sflap_send(gc, buf2, -1, TYPE_DATA);

	if (gc->keepalive < 0)
		update_keepalive(gc, gc->options & OPT_USR_KEEPALV);

        serv_finish_login(gc);
	gaim_setup(gc);
	return 0;
}

void toc_close(struct gaim_connection *gc)
{
	if (gc->protocol != PROTO_TOC) return; /* how did this happen? */
#ifdef USE_APPLET
	set_user_state(offline);
#endif /* USE_APPLET */
        if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	gc->inpa = -1;
	close(gc->toc_fd);
}

unsigned char *roast_password(char *pass)
{
	/* Trivial "encryption" */
	static char rp[256];
	static char *roast = ROAST;
	int pos=2;
	int x;
	strcpy(rp, "0x");
	for (x=0;(x<150) && pass[x]; x++) 
		pos+=sprintf(&rp[pos],"%02x", pass[x] ^ roast[x % strlen(roast)]);
	rp[pos]='\0';
        return rp;
}


char *print_header(void *hdr_v)
{
	static char s[80];
	struct sflap_hdr *hdr = (struct sflap_hdr *)hdr_v;
	g_snprintf(s,sizeof(s), "[ ast: %c, type: %d, seqno: %d, len: %d ]",
		hdr->ast, hdr->type, ntohs(hdr->seqno), ntohs(hdr->len));
	return s;
}

void print_buffer(char *buf, int len)
{
#if 0
	int x;
	printf("[ ");
	for (x=0;x<len;x++) 
		printf("%d ", buf[x]);
	printf("]\n");
	printf("[ ");
	for (x=0;x<len;x++)
		printf("%c ", buf[x]);
	printf("]\n");
#endif
}

int sflap_send(struct gaim_connection *gc, char *buf, int olen, int type)
{
	int len;
	int slen=0;
	struct sflap_hdr hdr;
	char obuf[MSG_LEN];

        /* One _last_ 2048 check here!  This shouldn't ever
         * get hit though, hopefully.  If it gets hit on an IM
         * It'll lose the last " and the message won't go through,
         * but this'll stop a segfault. */
        if (strlen(buf) > (MSG_LEN - sizeof(hdr))) {
                buf[MSG_LEN - sizeof(hdr) - 3] = '"';
                buf[MSG_LEN - sizeof(hdr) - 2] = '\0';
        }

        sprintf(debug_buff,"%s [Len %d]\n", buf, strlen(buf));
		debug_print(debug_buff);
        

	
	if (olen < 0)
		len = escape_message(buf);
	else
		len = olen;
	hdr.ast = '*';
	hdr.type = type;
	hdr.seqno = htons(gc->seqno++ & 0xffff);
        hdr.len = htons(len + (type == TYPE_SIGNON ? 0 : 1));

    sprintf(debug_buff,"Escaped message is '%s'\n",buf);
	debug_print(debug_buff);

	memcpy(obuf, &hdr, sizeof(hdr));
	slen += sizeof(hdr);
	memcpy(&obuf[slen], buf, len);
	slen += len;
	if (type != TYPE_SIGNON) {
		obuf[slen]='\0';
		slen += 1;
	}
	print_buffer(obuf, slen);

	return write(gc->toc_fd, obuf, slen);
}


static int wait_reply(struct gaim_connection *gc, char *buffer, size_t buflen)
{
        size_t res=-1;
	int read_rv = -1;
	struct sflap_hdr *hdr=(struct sflap_hdr *)buffer;
        char *c;

	if(buflen < sizeof(struct sflap_hdr)) {
	    do_error_dialog(_("Unable to read from server: Buffer too small"),
			    _("Gaim - Error (internal)"));
	    return -1;
	}

        while((read_rv = read(gc->toc_fd, buffer, 1))) {
		if (read_rv < 0 || read_rv > 1)
			return -1;
		if (buffer[0] == '*')
                        break;

	}

	read_rv = read(gc->toc_fd, buffer+1, sizeof(struct sflap_hdr) - 1);

        if (read_rv < 0)
		return read_rv;

	res = read_rv + 1;
	
        
	sprintf(debug_buff, "Rcv: %s %s\n",print_header(buffer), "");
	debug_print(debug_buff);


	if(buflen < sizeof(struct sflap_hdr) + ntohs(hdr->len) + 1) {
	    do_error_dialog(_("Unable to read from server: Too much information"),
			    _("Gaim - Error (internal)"));
	    return -1;
	}

        while (res < (sizeof(struct sflap_hdr) + ntohs(hdr->len))) {
		read_rv = read(gc->toc_fd, buffer + res, (ntohs(hdr->len) + sizeof(struct sflap_hdr)) - res);
		if(read_rv < 0) return read_rv;
		res += read_rv;
		/* my feeling is this will kill us. if there's data pending then we'll come right back
		 * to where we are now. possible workarounds are to remove the input watcher until
		 * we're done with this part
		while(gtk_events_pending())
			gtk_main_iteration();
		 */
	}
        
        if (res >= sizeof(struct sflap_hdr)) 
		buffer[res]='\0';
	else
		return res - sizeof(struct sflap_hdr);
		
	switch(hdr->type) {
	case TYPE_SIGNON:
		memcpy(&peer_ver, buffer + sizeof(struct sflap_hdr), 4);
		peer_ver = ntohl(peer_ver);
		gc->seqno = ntohs(hdr->seqno);
		gc->state = STATE_SIGNON_REQUEST;
		break;
	case TYPE_DATA:
		if (!strncasecmp(buffer + sizeof(struct sflap_hdr), "SIGN_ON:", strlen("SIGN_ON:")))
			gc->state = STATE_SIGNON_ACK;
		else if (!strncasecmp(buffer + sizeof(struct sflap_hdr), "CONFIG:", strlen("CONFIG:"))) {
			gc->state = STATE_CONFIG;
		} else if (!strncasecmp(buffer + sizeof(struct sflap_hdr), "ERROR:", strlen("ERROR:"))) {
			c = strtok(buffer + sizeof(struct sflap_hdr) + strlen("ERROR:"), ":");
			show_error_dialog(c);
		}

		sprintf(debug_buff, "Data: %s\n",buffer + sizeof(struct sflap_hdr));
		debug_print(debug_buff);

		break;
	default:
			sprintf(debug_buff, "Unknown/unimplemented packet type %d\n",hdr->type);
			debug_print(debug_buff);
	}
        return res;
}



void toc_callback( gpointer          data,
                   gint              source,
                   GdkInputCondition condition )
{
        char *buf;
	char *c;
        char *l;
	struct gaim_connection *gc = (struct gaim_connection *)data;

        buf = g_malloc(2 * BUF_LONG);
        if (wait_reply(gc, buf, 2 * BUF_LONG) < 0) {
                signoff(gc); /* this will free gc for us */
                hide_login_progress("Connection Closed");
                g_free(buf);
		return;
        }
                         
        
	c=strtok(buf+sizeof(struct sflap_hdr),":");	/* Ditch the first part */
	if (!strcasecmp(c,"UPDATE_BUDDY")) {
		char *uc;
		int logged, evil, idle, type = 0;
                time_t signon;
                time_t time_idle;
		
		c = strtok(NULL,":"); /* c is name */

		l = strtok(NULL,":"); /* l is T/F logged status */
       	
		sscanf(strtok(NULL, ":"), "%d", &evil);
		
		sscanf(strtok(NULL, ":"), "%ld", &signon);
		
		sscanf(strtok(NULL, ":"), "%d", &idle);
		
                uc = strtok(NULL, ":");


		if (!strncasecmp(l,"T",1))
			logged = 1;
		else
			logged = 0;


		if (uc[0] == 'A')
			type |= UC_AOL;
		
		switch(uc[1]) {
		case 'A':
			type |= UC_ADMIN;
			break;
		case 'U':
			type |= UC_UNCONFIRMED;
			break;
		case 'O':
			type |= UC_NORMAL;
			break;
		default:
			break;
		}

                switch(uc[2]) {
		case 'U':
			type |= UC_UNAVAILABLE;
			break;
		default:
			break;
		}

                if (idle) {
                        time(&time_idle);
                        time_idle -= idle*60;
                } else
                        time_idle = 0;
		
                serv_got_update(c, logged, evil, signon, time_idle, type, 0);

	} else if (!strcasecmp(c, "CONFIG")) {
		/* do we want to load the buddy list again here? */
		c = strtok(NULL,":");
		parse_toc_buddy_list(gc, c, 0);
	} else if (!strcasecmp(c, "ERROR")) {
		/* This should be handled by wait_reply
		c = strtok(NULL,":");
		show_error_dialog(c);
		*/
	} else if (!strcasecmp(c, "NICK")) {
		c = strtok(NULL,":");
		g_snprintf(gc->username, sizeof(gc->username), "%s", c);
	} else if (!strcasecmp(c, "IM_IN")) {
		char *away, *message;
                int a = 0;
                
		c = strtok(NULL,":");
		away = strtok(NULL,":");

		message = away;

                while(*message && (*message != ':'))
                        message++;

                message++;

		if (!strncasecmp(away, "T", 1))
			a = 1;
                serv_got_im(gc, c, message, a);
		
	} else if (!strcasecmp(c, "GOTO_URL")) {
		char *name;
		char *url;

		char tmp[256];
		
		name = strtok(NULL, ":");
		url = strtok(NULL, ":");


		g_snprintf(tmp, sizeof(tmp), "http://%s:%d/%s", toc_addy, aim_port, url);
/*		fprintf(stdout, "Name: %s\n%s\n", name, url);
		printf("%s", grab_url(tmp));*/
		g_show_info(tmp);
        } else if (!strcasecmp(c, "EVILED")) {
                int lev;
		char *name = NULL;

                sscanf(strtok(NULL, ":"), "%d", &lev);
                name = strtok(NULL, ":");

                sprintf(debug_buff,"%s | %d\n", name, lev);
				debug_print(debug_buff);

		serv_got_eviled(name, lev);
		
        } else if (!strcasecmp(c, "CHAT_JOIN")) {
                char *name;
                int id;
                

		sscanf(strtok(NULL, ":"), "%d", &id);
                name = strtok(NULL, ":");
                serv_got_joined_chat(gc, id, name);

	} else if (!strcasecmp(c, "DIR_STATUS")) {
	} else if (!strcasecmp(c, "ADMIN_PASSWD_STATUS")) {
		do_error_dialog("Password Change Successeful", "Gaim - Password Change");
	} else if (!strcasecmp(c, "CHAT_UPDATE_BUDDY")) {
		int id;
		char *in;
		char *buddy;
                GSList *bcs = gc->buddy_chats;
		struct conversation *b = NULL;
		
		sscanf(strtok(NULL, ":"), "%d", &id);

		in = strtok(NULL, ":");

		while(bcs) {
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;	
			bcs = bcs->next;
                        b = NULL;
		}
		
		if (!b) {
			g_free(buf); 
			return;
		}

		
		if (!strcasecmp(in, "T")) {
			while((buddy = strtok(NULL, ":")) != NULL) {
				add_chat_buddy(b, buddy);
			}
		} else {
			while((buddy = strtok(NULL, ":")) != NULL) {
				remove_chat_buddy(b, buddy);
			}
		}

	} else if (!strcasecmp(c, "CHAT_LEFT")) {
		int id;


                sscanf(strtok(NULL, ":"), "%d", &id);

                serv_got_chat_left(gc, id);


	} else if (!strcasecmp(c, "CHAT_IN")) {

		int id, w;
		char *m;
		char *who, *whisper;

	
		sscanf(strtok(NULL, ":"), "%d", &id);
		who = strtok(NULL, ":");
		whisper = strtok(NULL, ":");
		m = whisper;
                while(*m && (*m != ':')) m++;
                m++;

                if (!strcasecmp(whisper, "T"))
			w = 1;
		else
			w = 0;

		serv_got_chat_in(gc, id, who, w, m);


	} else if (!strcasecmp(c, "CHAT_INVITE")) {
		char *name;
		char *who;
		char *message;
                int id;

               
		name = strtok(NULL, ":");
		sscanf(strtok(NULL, ":"), "%d", &id);
		who = strtok(NULL, ":");
                message = strtok(NULL, ":");

                serv_got_chat_invite(gc, name, id, who, message);


        } else if (!strcasecmp(c, "RVOUS_PROPOSE")) {
                char *user;
                char *uuid;
                char *cookie;
                int seq;
                char *rip, *pip, *vip;
                int port;
                int unk[4];
                char *messages[4];
                int subtype, files, totalsize;
                char *name;
                char *tmp;
                int i;
                struct file_transfer *ft;
                

                user = strtok(NULL, ":");
                uuid = strtok(NULL, ":");
                cookie = strtok(NULL, ":");
                sscanf(strtok(NULL, ":"), "%d", &seq);
                rip = strtok(NULL, ":");
                pip = strtok(NULL, ":");
                vip = strtok(NULL, ":");
                sscanf(strtok(NULL, ":"), "%d", &port);

		if (!strcmp(uuid, FILE_SEND_UID)) {
			/* we're getting a file */
	                for (i=0; i<4; i++) {
	                        sscanf(strtok(NULL, ":"), "%d", &unk[i]);
	                        if (unk[i] == 10001)
	                                break;
	                        messages[i] = frombase64(strtok(NULL, ":"));
	                }
	                tmp = frombase64(strtok(NULL, ":"));
	                subtype = tmp[1];
	                files = tmp[3]; /* These are fine */

			totalsize = 0;
			totalsize |= (tmp[4] << 24) & 0xff000000;
			totalsize |= (tmp[5] << 16) & 0x00ff0000;
			totalsize |= (tmp[6] <<  8) & 0x0000ff00;
			totalsize |= (tmp[7] <<  0) & 0x000000ff;

	                name = tmp + 8;

	                ft = g_new0(struct file_transfer, 1);

	                ft->cookie = g_strdup(cookie);
	                ft->ip = g_strdup(pip);
	                ft->port = port;
	                if (i)
	                        ft->message = g_strdup(messages[0]);
	                else
	                        ft->message = NULL;
	                ft->filename = g_strdup(name);
	                ft->user = g_strdup(user);
	                ft->size = totalsize;
			sprintf(ft->UID, "%s", FILE_SEND_UID);
                
	                g_free(tmp);

	                for (i--; i >= 0; i--)
	                        g_free(messages[i]);
                
			if (totalsize) /* sanity check */
				accept_file_dialog(ft);
		} else if (!strcmp(uuid, FILE_GET_UID)) {
			/* we're sending a file */
	                for (i=0; i<4; i++) {
	                        sscanf(strtok(NULL, ":"), "%d", &unk[i]);
	                        if (unk[i] == 10001)
	                                break;
	                        messages[i] = frombase64(strtok(NULL, ":"));
	                }
	                tmp = frombase64(strtok(NULL, ":"));
			ft = g_new0(struct file_transfer, 1);

			ft->cookie = g_strdup(cookie);
			ft->ip = g_strdup(pip);
			ft->port = port;
			if (i)
				ft->message = g_strdup(messages[0]);
			else
				ft->message = NULL;
			ft->user = g_strdup(user);
			sprintf(ft->UID, "%s", FILE_GET_UID);

			g_free(tmp);

			for (i--; i >= 0; i--)
				g_free(messages[i]);

			accept_file_dialog(ft);
		} else if (!strcmp(uuid, VOICE_UID)) {
			/* oh goody. voice over ip. fun stuff. */

		/*
		} else if (!strcmp(uuid, B_ICON_UID)) {
		} else if (!strcmp(uuid, IMAGE_UID)) {
		*/

		} else {
			sprintf(debug_buff,"don't know what to do with %s\n",
					uuid);
			debug_print(debug_buff);
			tmp = g_malloc(BUF_LEN);
			name = frombase64(cookie);
			snprintf(tmp, BUF_LEN, "toc_rvous_cancel %s %s %s",
					user, name, uuid);
			sflap_send(gc, tmp, strlen(tmp), TYPE_DATA);
			free(name);
			free(tmp);
		}
	} else {
		sprintf(debug_buff,"don't know what to do with %s\n", c);
		debug_print(debug_buff);
	}
        g_free(buf);
}


int toc_signon(struct gaim_connection *gc)
{
	char buf[BUF_LONG];
	int res;
	struct signon so;

        sprintf(debug_buff,"State = %d\n", gc->state);
	debug_print(debug_buff);

	if ((res = write(gc->toc_fd, FLAPON, strlen(FLAPON))) < 0)
		return res;
	/* Wait for signon packet */

	gc->state = STATE_FLAPON;

	if ((res = wait_reply(gc, buf, sizeof(buf)) < 0))
		return res;
	
	if (gc->state != STATE_SIGNON_REQUEST) {
			sprintf(debug_buff, "State should be %d, but is %d instead\n", STATE_SIGNON_REQUEST, gc->state);
			debug_print(debug_buff);
			return -1;
	}
	
	/* Compose a response */
	
	g_snprintf(so.username, sizeof(so.username), "%s", gc->username);
	so.ver = ntohl(1);
	so.tag = ntohs(1);
	so.namelen = htons(strlen(so.username));	
	
	sflap_send(gc, (char *)&so, ntohs(so.namelen) + 8, TYPE_SIGNON);
	
	g_snprintf(buf, sizeof(buf), 
		"toc_signon %s %d %s %s %s \"%s\"",
		login_host, login_port, normalize(gc->username), roast_password(gc->password), LANGUAGE, REVISION);

        sprintf(debug_buff,"Send: %s\n", buf);
		debug_print(debug_buff);

	return sflap_send(gc, buf, -1, TYPE_DATA);
}

int toc_wait_signon(struct gaim_connection *gc)
{
	/* Wait for the SIGNON to be approved */
	char buf[BUF_LONG];
	int res;
	res = wait_reply(gc, buf, sizeof(buf));
	if (res < 0)
		return res;
	if (gc->state != STATE_SIGNON_ACK) {
			sprintf(debug_buff, "State should be %d, but is %d instead\n",STATE_SIGNON_ACK, gc->state);
			debug_print(debug_buff);
		return -1;
	}
	return 0;
}

#ifdef _WIN32
gint win32_read()
{
        int ret;
        struct fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);

        tv.tv_sec = 0;
        tv.tv_usec = 200;

        FD_SET(toc_fd, &fds);

        ret = select(toc_fd + 1, &fds, NULL, NULL, &tv);

        if (ret == 0) {
                return TRUE;
        }

        toc_callback(NULL, 0, (GdkInputCondition)0);
        return TRUE;
}
#endif


char *toc_wait_config(struct gaim_connection *gc)
{
	/* Waits for configuration packet, returning the contents of the packet */
	static char buf[BUF_LONG];
	int res;
	res = wait_reply(gc, buf, sizeof(buf));
	if (res < 0)
		return NULL;
/* Apparently, the toc_config is optional.  *VERY* Optional
*/
	if (gc->state != STATE_CONFIG) {
		res = 0;
	} else {
		res = 1;
	}
	/* At this point, it's time to setup automatic handling of incoming packets */
	gc->state = STATE_ONLINE;
#ifdef _WIN32
	win32_r = gtk_timeout_add(1000, (GtkFunction)win32_read, NULL);
#else
	gc->inpa = gdk_input_add(gc->toc_fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, toc_callback, gc);
#endif
	if (res)
		return buf;
	else
		return NULL;
}

void toc_build_config(char *s, int len, gboolean show)
{
	GSList *grp = groups;
	GList *mem;
	struct group *g;
	struct buddy *b;
	GList *plist = permit;
	GList *dlist = deny;

	int pos=0;

	if (!permdeny)
		permdeny = 1;

	pos += g_snprintf(&s[pos], len - pos, "m %d\n", permdeny);
	while(grp) {
		g = (struct group *)grp->data;
		pos += g_snprintf(&s[pos], len - pos, "g %s\n", g->name);
		mem = g->members;
		while(mem) {
			b = (struct buddy *)mem->data;
			pos += g_snprintf(&s[pos], len - pos, "b %s%s%s\n", b->name,
					show ? ":" : "", show ? b->show : "");
			mem = mem->next;
		}
		grp = g_slist_next(grp);
	}

	while(plist) {
		pos += g_snprintf(&s[pos], len - pos, "p %s\n", (char *)plist->data);
		plist=plist->next;
	}

	while(dlist) {
		pos += g_snprintf(&s[pos], len - pos, "d %s\n", (char *)dlist->data);
		dlist=dlist->next;
	}
}

void parse_toc_buddy_list(struct gaim_connection *gc, char *config, int from_do_import)
{
	char *c;
	char current[256];
	char *name;
	GList *bud;
	int how_many = 0;

	bud = NULL;
        
	if (config != NULL) {

		/* skip "CONFIG:" (if it exists)*/
		c = strncmp(config + sizeof(struct sflap_hdr),"CONFIG:",strlen("CONFIG:"))?
			strtok(config, "\n"):
			strtok(config + sizeof(struct sflap_hdr)+strlen("CONFIG:"), "\n");
		do {
			if (c == NULL) 
				break;
			if (*c == 'g') {
				strncpy(current,c+2, sizeof(current));
				add_group(current);
				how_many++;
			} else if (*c == 'b' && !find_buddy(c+2)) {
				char nm[80], sw[80], *tmp = c+2;
				int i = 0;
				while (*tmp != ':' && *tmp)
					nm[i++] = *tmp++;
				if (*tmp == ':') *tmp++ = '\0';
				nm[i] = '\0';
				i = 0;
				while (*tmp) sw[i++] = *tmp++;
				sw[i] = '\0';
				if (!find_buddy(nm))
					add_buddy(current, nm, sw);
				how_many++;
				
				bud = g_list_append(bud, c+2);
			} else if (*c == 'p') {
				GList *d = permit;
				char *n;
				name = g_malloc(strlen(c+2) + 2);
				g_snprintf(name, strlen(c+2) + 1, "%s", c+2);
				n = g_strdup(normalize(name));
				while (d) {
					if (!strcasecmp(n, normalize(d->data)))
						break;
					d = d->next;
				}
				g_free(n);
				if (!d)
					permit = g_list_append(permit, name);
			} else if (*c == 'd') {
				GList *d = deny;
				char *n;
				name = g_malloc(strlen(c+2) + 2);
				g_snprintf(name, strlen(c+2) + 1, "%s", c+2);
				n = g_strdup(normalize(name));
				while (d) {
					if (!strcasecmp(n, normalize(d->data)))
						break;
					d = d->next;
				}
				g_free(n);
				if (!d)
					deny = g_list_append(deny, name);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &permdeny);
				sprintf(debug_buff, "permdeny: %d\n", permdeny);
				debug_print(debug_buff);
				if (permdeny == 0)
					permdeny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &permdeny);
				sprintf(debug_buff, "permdeny: %d\n", permdeny);
				debug_print(debug_buff);
				if (permdeny == 0)
					permdeny = 1;
			}
		} while((c=strtok(NULL,"\n"))); 
#if 0
		fprintf(stdout, "Sending message '%s'\n",buf);
#endif
				      
		if (bud != NULL) serv_add_buddies(bud);
		serv_set_permit_deny();
		if (blist) {
			build_edit_tree();
			build_permit_tree();
		}
	}

	/* perhaps the server dropped the buddy list, try importing from
           cache */

	if ( how_many == 0 && !from_do_import ) {
		do_import( (GtkWidget *) NULL, gc );
	} else if ( gc && (bud_list_cache_exists(gc) == FALSE) ) {
		do_export( (GtkWidget *) NULL, 0 );	
	}
 }
