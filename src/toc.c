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
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#include "gnome_applet_mgr.h"

#define REVISION "gaim:$Revision: 1012 $"

struct toc_data {
	int toc_fd;
	int seqno;
	int state;
};


static unsigned int peer_ver=0;
#ifdef _WIN32
static int win32_r;
#endif

static int toc_signon(struct gaim_connection *);



/* ok. this function used to take username/password, and return 0 on success.
 * now, it takes username/password, and returns NULL on error or a new gaim_connection
 * on success. */
void toc_login(struct aim_user *user)
{
	char *config;
        struct in_addr *sin;
	struct gaim_connection *gc;
	struct toc_data *tdt;
	char buf[80];
	char buf2[2048];

	gc = new_gaim_conn(PROTO_TOC, user->username, user->password);
	gc->proto_data = tdt = g_new0(struct toc_data, 1);
	
	g_snprintf(buf, sizeof(buf), "Looking up %s", aim_host);	
	set_login_progress(gc, 1, buf);
	while (gtk_events_pending())
		gtk_main_iteration();

	sin = (struct in_addr *)get_address(aim_host);
	if (!sin) {
		g_snprintf(buf, sizeof(buf), "Unable to lookup %s", aim_host);
		hide_login_progress(gc, buf);
		destroy_gaim_conn(gc);
		return;
	}
	
	g_snprintf(toc_addy, sizeof(toc_addy), "%s", inet_ntoa(*sin));
	g_snprintf(buf, sizeof(buf), "Connecting to %s", inet_ntoa(*sin));
	set_login_progress(gc, 2, buf);
	while (gtk_events_pending())
		gtk_main_iteration();
	


	tdt->toc_fd = connect_address(sin->s_addr, aim_port);

        if (tdt->toc_fd < 0) {
		g_snprintf(buf, sizeof(buf), "Connect to %s failed",
			 inet_ntoa(*sin));
		hide_login_progress(gc, buf);
		destroy_gaim_conn(gc);
		return;
        }

        g_free(sin);
	
	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 3, buf);
	while (gtk_events_pending())
		gtk_main_iteration();
	
	if (toc_signon(gc) < 0) {
		hide_login_progress(gc, "Disconnected.");
		destroy_gaim_conn(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "Waiting for reply...");
	set_login_progress(gc, 4, buf);
	while (gtk_events_pending())
		gtk_main_iteration();
	if (toc_wait_signon(gc) < 0) {
		hide_login_progress(gc, "Authentication Failed");
		destroy_gaim_conn(gc);
		return;
	}

	sprintf(gc->user_info, "%s", user->user_info);
	gc->options = user->options;
	save_prefs();

	g_snprintf(buf, sizeof(buf), "Retrieving config...");
	set_login_progress(gc, 5, buf);
	while (gtk_events_pending())
		gtk_main_iteration();

	serv_finish_login(gc);
	account_online(gc);

	config = toc_wait_config(gc);
	tdt->state = STATE_ONLINE;

	if (config != NULL)
		parse_toc_buddy_list(gc, config, 0);
	else
		do_import(0, gc);
        
	g_snprintf(buf2, sizeof(buf2), "toc_init_done");
	sflap_send(gc, buf2, -1, TYPE_DATA);

	g_snprintf(buf2, sizeof(buf2), "toc_set_caps %s %s %s %s %s",
		   FILE_SEND_UID, FILE_GET_UID, B_ICON_UID, IMAGE_UID,
		   VOICE_UID);
	sflap_send(gc, buf2, -1, TYPE_DATA);

	if (gc->keepalive < 0)
		update_keepalive(gc, gc->options & OPT_USR_KEEPALV);
}

void toc_close(struct gaim_connection *gc)
{
	if (gc->protocol != PROTO_TOC) return; /* how did this happen? */
        if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	gc->inpa = -1;
	close(((struct toc_data *)gc->proto_data)->toc_fd);
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
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;

	/* One _last_ 2048 check here!  This shouldn't ever
	 * get hit though, hopefully.  If it gets hit on an IM
	 * It'll lose the last " and the message won't go through,
	 * but this'll stop a segfault. */
	if (strlen(buf) > (MSG_LEN - sizeof(hdr))) {
		buf[MSG_LEN - sizeof(hdr) - 3] = '"';
		buf[MSG_LEN - sizeof(hdr) - 2] = '\0';
	}

	debug_printf("%s [Len %d]\n", buf, strlen(buf));
	
	if (olen < 0)
		len = escape_message(buf);
	else
		len = olen;
	hdr.ast = '*';
	hdr.type = type;
	hdr.seqno = htons(tdt->seqno++ & 0xffff);
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

	return write(tdt->toc_fd, obuf, slen);
}


static int wait_reply(struct gaim_connection *gc, char *buffer, size_t buflen)
{
        size_t res=-1;
	int read_rv = -1;
	struct sflap_hdr *hdr=(struct sflap_hdr *)buffer;
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
        char *c;

	if(buflen < sizeof(struct sflap_hdr)) {
	    do_error_dialog(_("Unable to read from server: Buffer too small"),
			    _("Gaim - Error (internal)"));
	    return -1;
	}

        while((read_rv = read(tdt->toc_fd, buffer, 1))) {
		if (read_rv < 0 || read_rv > 1)
			return -1;
		if (buffer[0] == '*')
                        break;

	}

	read_rv = read(tdt->toc_fd, buffer+1, sizeof(struct sflap_hdr) - 1);

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
		read_rv = read(tdt->toc_fd, buffer + res, (ntohs(hdr->len) + sizeof(struct sflap_hdr)) - res);
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
		tdt->seqno = ntohs(hdr->seqno);
		tdt->state = STATE_SIGNON_REQUEST;
		break;
	case TYPE_DATA:
		if (!strncasecmp(buffer + sizeof(struct sflap_hdr), "SIGN_ON:", strlen("SIGN_ON:")))
			tdt->state = STATE_SIGNON_ACK;
		else if (!strncasecmp(buffer + sizeof(struct sflap_hdr), "CONFIG:", strlen("CONFIG:"))) {
			tdt->state = STATE_CONFIG;
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
                hide_login_progress(gc, "Connection Closed");
                signoff(gc); /* this will free gc for us */
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
			ft->gc = gc;
                
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
			ft->gc = gc;

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
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;

        sprintf(debug_buff,"State = %d\n", tdt->state);
	debug_print(debug_buff);

	if ((res = write(tdt->toc_fd, FLAPON, strlen(FLAPON))) < 0)
		return res;
	/* Wait for signon packet */

	tdt->state = STATE_FLAPON;

	if ((res = wait_reply(gc, buf, sizeof(buf)) < 0))
		return res;
	
	if (tdt->state != STATE_SIGNON_REQUEST) {
			sprintf(debug_buff, "State should be %d, but is %d instead\n", STATE_SIGNON_REQUEST, tdt->state);
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
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	char buf[BUF_LONG];
	int res;
	res = wait_reply(gc, buf, sizeof(buf));
	if (res < 0)
		return res;
	if (tdt->state != STATE_SIGNON_ACK) {
			sprintf(debug_buff, "State should be %d, but is %d instead\n",STATE_SIGNON_ACK, tdt->state);
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
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	static char buf[BUF_LONG];
	int res;
	res = wait_reply(gc, buf, sizeof(buf));
	if (res < 0)
		return NULL;
/* Apparently, the toc_config is optional.  *VERY* Optional
*/
	if (tdt->state != STATE_CONFIG) {
		res = 0;
	} else {
		res = 1;
	}
	/* At this point, it's time to setup automatic handling of incoming packets */
	tdt->state = STATE_ONLINE;
#ifdef _WIN32
	win32_r = gtk_timeout_add(1000, (GtkFunction)win32_read, NULL);
#else
	gc->inpa = gdk_input_add(tdt->toc_fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, toc_callback, gc);
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

static char *toc_name() {
	return "TOC";
}

static void toc_send_im(struct gaim_connection *gc, char *name, char *message, int away) {
	char buf[MSG_LEN - 7];

	escape_text(message);
	g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
			message, ((away) ? " auto" : ""));
	sflap_send(gc, buf, -1, TYPE_DATA);
}

static void toc_get_info(struct gaim_connection *g, char *name) {
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_get_dir(struct gaim_connection *g, char *name) {
	char buf[MSG_LEN];
	g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_dir(struct gaim_connection *g, char *first, char *middle, char *last,
			char *maiden, char *city, char *state, char *country, int web) {
	char buf2[BUF_LEN*4], buf[BUF_LEN];
	g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
			middle, last, maiden, city, state, country,
			(web == 1) ? "Y" : "");
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_dir_search(struct gaim_connection *g, char *first, char *middle, char *last,
			char *maiden, char *city, char *state, char *country, char *email) {
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf)/2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle,
			last, maiden, city, state, country, email);
	sprintf(debug_buff,"Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden,
			city, state, country);
	debug_print(debug_buff);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_away(struct gaim_connection *g, char *message) {
	char buf[MSG_LEN];
	if (message) {
		escape_text(message);
		g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", message);
	} else
		g_snprintf(buf, MSG_LEN, "toc_set_away \"\"");
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_info(struct gaim_connection *g, char *info) {
	char buf[MSG_LEN];
	escape_text(info);
	g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", info);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_change_passwd(struct gaim_connection *g, char *orig, char *new) {
	char buf[MSG_LEN];
	g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new);
	sflap_send(g, buf, strlen(buf), TYPE_DATA);
}

static void toc_add_buddy(struct gaim_connection *g, char *name) {
	char buf[1024]; 
	g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_add_buddies(struct gaim_connection *g, GList *buddies) {
	char buf[MSG_LEN];
	int n;

	n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	while (buddies) {
		if (strlen(normalize(buddies->data)) > MSG_LEN - n - 16) {
			sflap_send(g, buf, -1, TYPE_DATA);
			n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
		}
		n += g_snprintf(buf + n, sizeof(buf)-n, " %s", normalize(buddies->data));
		buddies = buddies->next;
	}
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_remove_buddy(struct gaim_connection *g, char *name) {
	char buf[1024]; 
	g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_idle(struct gaim_connection *g, int time) {
	char buf[256];
	g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_warn(struct gaim_connection *g, char *name, int anon) {
	char send[256];
	g_snprintf(send, 255, "toc_evil %s %s", name, ((anon) ? "anon" : "norm"));
	sflap_send(g, send, -1, TYPE_DATA);
}

static void toc_accept_chat(struct gaim_connection *g, int i) {
	char buf[256];
	g_snprintf(buf, 255, "toc_chat_accept %d",  i);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_join_chat(struct gaim_connection *g, int exchange, char *name) {
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf)/2, "toc_chat_join %d \"%s\"", exchange, name);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_invite(struct gaim_connection *g, int id, char *message, char *name) {
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf)/2, "toc_chat_invite %d \"%s\" %s", id, message, normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_leave(struct gaim_connection *g, int id) {
	char buf[256];
	g_snprintf(buf, 255, "toc_chat_leave %d",  id);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_whisper(struct gaim_connection *g, int id, char *who, char *message) {
	char buf2[MSG_LEN];
	g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, who, message);
	sflap_send(g, buf2, -1, TYPE_DATA);
}

static void toc_chat_send(struct gaim_connection *g, int id, char *message) {
	char buf[MSG_LEN];
	escape_text(message);
	g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"",id, message);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_keepalive(struct gaim_connection *gc) {
	sflap_send(gc, "", 0, TYPE_KEEPALIVE);
}

void toc_init(struct prpl *ret) {
        ret->protocol = PROTO_TOC;
        ret->name = toc_name;
        ret->login = toc_login;
        ret->close = toc_close;
        ret->send_im = toc_send_im;
        ret->set_info = toc_set_info;
        ret->get_info = toc_get_info;
        ret->set_away = toc_set_away;
        ret->get_away_msg = NULL;
        ret->set_dir = toc_set_dir;
        ret->get_dir = toc_get_dir;
        ret->dir_search = toc_dir_search;
        ret->set_idle = toc_set_idle;
        ret->change_passwd = toc_change_passwd;
        ret->add_buddy = toc_add_buddy;
        ret->add_buddies = toc_add_buddies;
        ret->remove_buddy = toc_remove_buddy;
        ret->add_permit = NULL; /* FIXME */
        ret->add_deny = NULL;
        ret->warn = toc_warn;
        ret->accept_chat = toc_accept_chat;
        ret->join_chat = toc_join_chat;
        ret->chat_invite = toc_chat_invite;
        ret->chat_leave = toc_chat_leave;
        ret->chat_whisper = toc_chat_whisper;
        ret->chat_send = toc_chat_send;
	ret->keepalive = toc_keepalive;
}
