/*
 * gaim - IRC Protocol Plugin
 *
 * Copyright (C) 2000, Rob Flynn <rob@tgflinux.com>
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

#include "../config.h"


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
#include <sys/stat.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "gnome_applet_mgr.h"

#include "pixmaps/cancel.xpm"
#include "pixmaps/ok.xpm"

#define IRC_BUF_LEN 4096


static int chat_id = 0;

struct irc_channel { 
	int id;
	gchar *name;
};

struct irc_data {
	int fd;

	int timer;

	int totalblocks;
	int recblocks;

	GSList *templist;
	GList *channels;
};

static char *irc_name() {
	return "IRC";
}

char *name() {
	return "IRC";
}

char *description() {
	return "Allows gaim to use the IRC protocol";
}

void irc_join_chat( struct gaim_connection *gc, int id, char *name) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	gchar *buf = (gchar *)g_malloc(IRC_BUF_LEN+1);

	g_snprintf(buf, IRC_BUF_LEN, "JOIN %s\n", name);
	write(idata->fd, buf, strlen(buf));
	
	g_free(buf);
}

void irc_update_user (struct gaim_connection *gc, char *name, int status) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *u;
	GSList *temp = idata->templist;

	/* Loop through our list */
	
	while (temp) {
		u = (struct irc_channel *)temp->data;
		if (g_strcasecmp(u->name, name) == 0) {
			u->id = status;
			return;
		}
		
		temp = g_slist_next(temp);
	}
	return;
}

void irc_request_buddy_update ( struct gaim_connection *gc ) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GSList *grp = gc->groups;
	GSList *person;
	struct group *g;
	struct buddy *b;
	struct irc_channel *u;
	gchar buf[IRC_BUF_LEN+1];

	if (idata->templist != NULL)
		return;

	idata->recblocks = 0;
	idata->totalblocks = 1;

	/* First, let's check to see if we have anyone on our buddylist */
	if (!grp) {
		return;
	}

	/* Send the first part of our request */	
	write(idata->fd, "ISON", 4);

	/* Step through our list of groups */
	while (grp) {
		
		g = (struct group *)grp->data;
		person = g->members;

		while (person) {
			b = (struct buddy *)person->data;

			/* We will store our buddy info here.  I know, this is cheap
			 * but hey, its the exact same data structure.  Why should we
			 * bother with making another one */
			
			u = g_new0(struct irc_channel, 1);
			u->id = 0; /* Assume by default that they're offline */
			u->name = strdup(b->name);

			write(idata->fd, " ", 1);
			write(idata->fd, u->name, strlen(u->name));
			idata->templist = g_slist_append(idata->templist, u);

			person = person->next;
		}
		
		grp = g_slist_next(grp);
	}
	write(idata->fd, "\n", 1);
}


void irc_send_im( struct gaim_connection *gc, char *who, char *message, int away) {

	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	gchar *buf = (gchar *)g_malloc(IRC_BUF_LEN + 1);

	/* Before we actually send this, we should check to see if they're trying
	 * To issue a /me command and handle it properly. */

	if ( (g_strncasecmp(message, "/me ", 4) == 0) && (strlen(message)>4)) {
		/* We have /me!! We have /me!! :-) */

		gchar *temp = (gchar *)g_malloc(IRC_BUF_LEN+1);
		strcpy(temp, message+4);
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG %s :%cACTION %s%c\n", who, '\001', temp, '\001');
		g_free(temp);
	}
	else
	{
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG %s :%s\n", who, message);
	}

	write(idata->fd, buf, strlen(buf)); 

	g_free(buf);	
}

int find_id_by_name(struct gaim_connection *gc, char *name) {
	gchar *temp = (gchar *)g_malloc(IRC_BUF_LEN + 1);
	GList *templist;
	struct irc_channel *channel;

	templist = ((struct irc_data *)gc->proto_data)->channels;

	while (templist) {
		channel = (struct irc_channel *)templist->data;

		g_snprintf(temp, IRC_BUF_LEN, "#%s", channel->name);

		if (g_strcasecmp(temp, name) == 0) {
			g_free(temp);
			return channel->id;
		}

		templist = templist -> next;
	}

	g_free(temp);

	/* Return -1 if we have no ID */
	return -1;
}

struct irc_channel * find_channel_by_name(struct gaim_connection *gc, char *name) {
	gchar *temp = (gchar *)g_malloc(IRC_BUF_LEN + 1);
	GList *templist;
	struct irc_channel *channel;

	templist = ((struct irc_data *)gc->proto_data)->channels;

	while (templist) {
		channel = (struct irc_channel *)templist->data;

		g_snprintf(temp, IRC_BUF_LEN, "%s", channel->name);

		if (g_strcasecmp(temp, name) == 0) {
			g_free(temp);
			return channel;
		}

		templist = templist -> next;
	}

	g_free(temp);

	/* If we found nothing, return nothing :-) */
	return NULL;
}

struct irc_channel * find_channel_by_id (struct gaim_connection *gc, int id) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *channel;
	
	GList *temp;

	temp = idata->channels;

	while (temp) {
		channel = (struct irc_channel *)temp->data;

		if (channel->id == id) {
			/* We've found our man */
			return channel;
		}
		
		temp = temp->next;
	}


	/* If we didnt find one, return NULL */
	return NULL;
}

void irc_chat_send( struct gaim_connection *gc, int id, char *message) {

	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *channel = NULL;
	gchar *buf = (gchar *)g_malloc(IRC_BUF_LEN + 1);

	/* First lets get our current channel */
	channel = find_channel_by_id(gc, id);
	

	if (!channel) {
		/* If for some reason we've lost our channel, let's bolt */
		g_free(buf);
		return;
	}
	
	
	/* Before we actually send this, we should check to see if they're trying
	 * To issue a /me command and handle it properly. */

	if ( (g_strncasecmp(message, "/me ", 4) == 0) && (strlen(message)>4)) {
		/* We have /me!! We have /me!! :-) */

		gchar *temp = (gchar *)g_malloc(IRC_BUF_LEN+1);
		strcpy(temp, message+4);
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG #%s :%cACTION %s%c\n", channel->name, '\001', temp, '\001');
		g_free(temp);
	}
	else
	{
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG #%s :%s\n", channel->name, message);
	}

	write(idata->fd, buf, strlen(buf)); 

	/* Since AIM expects us to receive the message we send, we gotta fake it */
	serv_got_chat_in(gc, id, gc->username, 0, message);

	g_free(buf);	
}

struct conversation * find_conversation_by_id( struct gaim_connection * gc, int id) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GSList *bc = gc->buddy_chats;
	struct conversation *b = NULL;

	while (bc) {
		b = (struct conversation *)bc->data;
		if (id == b->id) {
			break;
		}
		bc = bc->next;
		b = NULL;
	}

	if (!b) {
		return NULL;
	}

	return b;
}

struct conversation * find_conversation_by_name( struct gaim_connection * gc, char *name) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GSList *bc = gc->buddy_chats;
	struct conversation *b = NULL;

	while (bc) {
		b = (struct conversation *)bc->data;

		if (g_strcasecmp(name, b->name) == 0) {
			break;
		}
		bc = bc->next;
		b = NULL;
	}

	if (!b) {
		return NULL;
	}

	return b;
}



void irc_callback ( struct gaim_connection * gc ) {

	int i = 0;
	char c;
	gchar buf[4096];
	gchar **buf2;
	int status;
	struct irc_data *idata;

	idata = (struct irc_data *)gc->proto_data;
	
	do {
		status = recv(idata->fd, &c, 1, 0);

		if (!status)
		{
			return;
		}
		buf[i] = c;
		i++;
	} while (c != '\n');

	buf[i] = '\0';

	/* And remove that damned trailing \n */
	g_strchomp(buf);

	/* For now, lets display everything to the console too. Im such 
	 * a bitch */
	printf("IRC:'%'s\n", buf);



	/* Check for errors */

	if (((strstr(buf, "ERROR :") && (!strstr(buf, "PRIVMSG ")) &&
		(!strstr(buf, "NOTICE ")) && (strlen(buf) > 7)))) {

		gchar *u_errormsg;

		/* Let's get our error message */
		u_errormsg = strdup(buf + 7);

		/* We got our error message.  Now, let's reaise an
		 * error dialog */

		do_error_dialog(u_errormsg, "Gaim: IRC Error");

		/* And our necessary garbage collection */
		free(u_errormsg);
	}
	
	/* Parse the list of names that we receive when we first sign on to
	 * a channel */

	if (((strstr(buf, " 353 ")) && (!strstr(buf, "PRIVMSG")) &&
	     (!strstr(buf, "NOTICE")))) {
		gchar u_host[255];
		gchar u_command[32];
		gchar u_channel[128];
		gchar u_names[IRC_BUF_LEN + 1];
		struct conversation *convo = NULL;
		int j;

		for (j = 0, i = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0'; i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0'; i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		i++;
		
		for (j = 0; buf[i] != ':'; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j-1] = '\0'; i++;

		while ((buf[i] == ' ') || (buf[i] == ':')) {
			i++;
		}

		strcpy(u_names, buf + i);
	
		buf2 = g_strsplit(u_names, " ", 0);
		
		/* Let's get our conversation window */
		convo = find_conversation_by_name(gc, u_channel);

		if (!convo) {
			return;
		}
			
		/* Now that we've parsed the hell out of this big
		 * mess, let's try to split up the names properly */

		for (i = 0; buf2[i] != NULL; i++) {
			/* We shouldnt play with ourselves */
			if (g_strcasecmp(buf2[i], gc->username) != 0) {
				/* Add the person to the list */
				add_chat_buddy(convo, buf2[i]);
			}
		}

		/* And free our pointers */
		g_strfreev (buf2);
	
		return;
		
	}

	/* Receive a list of users that are currently online */
	
	if (((strstr(buf, " 303 ")) && (!strstr(buf, "PRIVMSG")) &&
	     (!strstr(buf, "NOTICE")))) {
		gchar u_host[255];
		gchar u_command[32];
		gchar u_names[IRC_BUF_LEN + 1];
		int j;

		for (j = 0, i = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0'; i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0'; i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
			/* My Nick */
		}
		i++;
		
		strcpy(u_names, buf + i);
	
		buf2 = g_strsplit(u_names, " ", 0);
		
		/* Now that we've parsed the hell out of this big
		 * mess, let's try to split up the names properly */

		for (i = 0; buf2[i] != NULL; i++) {
			/* If we have a name here then our buddy is online.  We should
			 * update our temporary gslist accordingly.  When we achieve our maximum
			 * list of names then we should force an update */

			irc_update_user(gc, buf2[i], 1);
		}
		
		/* Increase our received blocks counter */
		idata->recblocks++;

		/* If we have our total number of blocks */
		if (idata->recblocks == idata->totalblocks) {
			GSList *temp;
			struct irc_channel *u;
			
			/* Let's grab our list of people and bring them all on or off line */
			temp = idata->templist;
			
			/* Loop */
			while (temp) {
				
				u = temp->data;		
			
				/* Tell Gaim to bring the person on or off line */
				serv_got_update(gc, u->name, u->id, 0, 0, 0, 0, 0);	
			
				/* Grab the next entry */
				temp = g_slist_next(temp);
			}
		
			/* And now, let's delete all of our entries */
			temp = idata->templist;
			while (temp) {
				u = temp->data;
				g_free(u->name);
				temp = g_slist_remove(temp, u);
			}
			
			/* Reset our list */
			idata->totalblocks = 0;
			idata->recblocks = 0;
			
			idata->templist = NULL;
				
			return;
		}

		/* And free our pointers */
		g_strfreev (buf2);
	
		return;
		
	}

	
	if ( (strstr(buf, " JOIN ")) && (buf[0] == ':') && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];
		
		struct irc_channel *channel;
		int id;
		int j;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0'; i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		
		i++;
	
		strcpy(u_channel, buf+i);

		/* Looks like we're going to join the channel for real 
		 * now.  Let's create a valid channel structure and add 
		 * it to our list.  Let's make sure that
		 * we are not already in a channel first */

		channel = find_channel_by_name(gc, u_channel);

		if (!channel) {
			chat_id++;

			channel = g_new0(struct irc_channel, 1);
			
			channel->id = chat_id;
			channel->name = strdup(u_channel);
	
			idata->channels = g_list_append(idata->channels, channel);

			serv_got_joined_chat(gc, chat_id, u_channel);	
		} else {
			struct conversation *convo = NULL;
			
			/* Someone else joined. Find their conversation
			 * window */
			convo = find_conversation_by_id(gc, channel->id);

			/* And add their name to it */
			add_chat_buddy(convo, u_nick);
			
		}

		return;
	}
	
	if ( (strstr(buf, " PART ")) && (buf[0] == ':') && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];

		struct irc_channel *channel;
		int id;
		int j;
		GList *test = NULL;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}
		u_nick[j] = '\0';

		i++;
		
		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		
		i++;
		
		strcpy(u_channel, buf+i);


		/* Now, lets check to see if it was US that was leaving.  
		 * If so, do the correct thing by closing up all of our 
		 * old channel stuff. Otherwise,
		 * we should just print that someone left */

		channel = find_channel_by_name(gc, u_channel);

		if (!channel) {
			return;
		}

		if (g_strcasecmp(u_nick, gc->username) == 0) {
	
			/* Looks like we're going to leave the channel for 
			 * real now.  Let's create a valid channel structure 
			 * and add it to our list */

			serv_got_chat_left(gc, channel->id);

			idata->channels = g_list_remove(idata->channels, channel);
		} else {
			struct conversation *convo = NULL;
			
			/* Find their conversation window */
			convo = find_conversation_by_id(gc, channel->id);

			if (!convo) {
				/* Some how the window doesn't exist. 
				 * Let's get out of here */
				return ;
			}
		
			/* And remove their name */
			remove_chat_buddy(convo, u_nick);

		}

		/* Go Home! */
		return;
	}
	
	if ( (strstr(buf, " PRIVMSG ")) && (buf[0] == ':')) {
		gchar u_nick[128];
		gchar u_host[255];
		gchar u_command[32];
		gchar u_channel[128];
		gchar u_message[IRC_BUF_LEN];
		int j;
		int msgcode = 0;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0'; i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0'; i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0'; i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j-1] = '\0'; i++;


		/* Now that everything is parsed, the rest of this baby must be our message */
		strncpy(u_message, buf + i, IRC_BUF_LEN);
	
		/* Now, lets check the message to see if there's anything special in it */
		if (u_message[0] == '\001') {
			if (g_strncasecmp(u_message, "\001VERSION", 8) == 0) {
				/* Looks like we have a version request.  Let
				 * us handle it thusly */
				
				g_snprintf(buf, IRC_BUF_LEN, "NOTICE %s :%cVERSION GAIM %s:The Pimpin Penguin AIM Clone:www.marko.net/gaim%c\n", u_nick, '\001', VERSION, '\001'); 

				write(idata->fd, buf, strlen(buf));

				/* And get the heck out of dodge */
				return;
			}
			
			if ((g_strncasecmp(u_message, "\001PING ", 6) == 0) && (strlen(u_message) > 6)) {
				/* Someone's triyng to ping us.  Let's respond */
				gchar u_arg[24];

				strcpy(u_arg, u_message + 6);
				u_arg[strlen(u_arg)-1] = '\0';

				g_snprintf(buf, IRC_BUF_LEN, "NOTICE %s :%cPING %s%c\n", u_nick, '\001', u_arg, '\001'); 

				write(idata->fd, buf, strlen(buf));

				/* And get the heck out of dodge */
				return;
			}

			if (g_strncasecmp(u_message, "\001ACTION ", 8) == 0) {
				/* Looks like we have an action. Let's parse it a little */
				strcpy(buf, u_message);

				strcpy(u_message, "/me ");
				for (j = 4, i = 8; buf[i] != '\001'; i++, j++) {
					u_message[j] = buf[i];
				}
				u_message[j] = '\0';
			}
		}


		/* Let's check to see if we have a channel on our hands */
		if (u_channel[0] == '#') {
			/* Yup.  We have a channel */
			int id;

			id = find_id_by_name(gc, u_channel);
			if (id != -1) {
				serv_got_chat_in(gc, id, u_nick, 0, u_message);
			}
		}
		else {
			/* Nope. Let's treat it as a private message */
			serv_got_im(gc, u_nick, u_message, 0);
		}

		return;
	}

	/* Let's parse PING requests so that we wont get booted for inactivity */

	if (strncmp(buf, "PING :", 6) == 0) {
		buf2 = g_strsplit(buf, ":", 1);
		
		/* Let's build a new response */
		g_snprintf(buf, IRC_BUF_LEN, "PONG :%s\n", buf2[1]);
		write(idata->fd, buf, strlen(buf));

		/* And clean up after ourselves */
		g_strfreev(buf2);

		return;
	}

}

void irc_handler(gpointer data, gint source, GdkInputCondition condition) {
	irc_callback(data);
}

void irc_close(struct gaim_connection *gc) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GList *chats = idata->channels;
	struct irc_channel *cc;

	gchar *buf = (gchar *)g_malloc(IRC_BUF_LEN);

	gtk_timeout_remove(idata->timer);
	
	g_snprintf(buf, IRC_BUF_LEN, "QUIT :Download GAIM [www.marko.net/gaim]\n");
	write(idata->fd, buf, strlen(buf));

	g_free(buf);

	while (chats) {
		cc = (struct irc_channel *)chats->data;
		g_free(cc->name);
		chats = g_list_remove(chats, cc);
		g_free(cc);
	}
	
	if (gc->inpa)
		gdk_input_remove(gc->inpa);

	close(idata->fd);
	g_free(gc->proto_data);
}

void irc_chat_leave(struct gaim_connection *gc, int id) {
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *channel;
	gchar *buf = (gchar *)g_malloc(IRC_BUF_LEN+1);
	
	channel = find_channel_by_id(gc, id);
	
	if (!channel) {
		return;
	}

	g_snprintf(buf, IRC_BUF_LEN, "PART #%s\n", channel->name);
	write(idata->fd, buf, strlen(buf));

	g_free(buf);
}

void irc_login(struct aim_user *user) {
	int fd;
	struct hostent *host;
	struct sockaddr_in site;
	char buf[4096];
	
	struct gaim_connection *gc = new_gaim_conn(user);
	struct irc_data *idata = gc->proto_data = g_new0(struct irc_data, 1);
	char c;
	int i;
	int status;

	set_login_progress(gc, 1, buf);

	while (gtk_events_pending())
		gtk_main_iteration();
	if (!g_slist_find(connections, gc))
		return;

	host = gethostbyname(user->proto_opt[0]);
	if (!host) {
		hide_login_progress(gc, "Unable to resolve hostname");
		destroy_gaim_conn(gc);
		return;
	}

	site.sin_family = AF_INET;
	site.sin_addr.s_addr = *(long *)(host->h_addr);
	site.sin_port = htons(atoi(user->proto_opt[1]));

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		hide_login_progress(gc, "Unable to create socket");
		destroy_gaim_conn(gc);
		return;
	}

	if (connect(fd, (struct sockaddr *)&site, sizeof(site)) < 0) {
		hide_login_progress(gc, "Unable to connect.");
		destroy_gaim_conn(gc);
		return;
	}

	idata->fd = fd;
	
	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);

	/* This is where we will attempt to sign on */
	
	/* FIXME: This should be their servername, not their username. im just lazy right now */

	g_snprintf(buf, 4096, "NICK %s\n USER %s localhost %s :GAIM (www.marko.net/gaim)\n", gc->username, getenv("USER"), user->proto_opt[0]);

	printf("Sending: %s\n", buf);
	write(idata->fd, buf, strlen(buf));

	/* Now lets sign ourselves on */
        account_online(gc);
	serv_finish_login(gc);

	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);
	
	
	gc->inpa = gdk_input_add(idata->fd, GDK_INPUT_READ, irc_handler, gc);

	/* We want to update our buddlist every 20 seconds */
	idata->timer = gtk_timeout_add(20000, (GtkFunction)irc_request_buddy_update, gc);

	/* But first, let's go ahead and check our list */
	irc_request_buddy_update(gc);
}

static void irc_print_option(GtkEntry *entry, struct aim_user *user) {
	if (gtk_object_get_user_data(GTK_OBJECT(entry))) {
		g_snprintf(user->proto_opt[1], sizeof(user->proto_opt[1]), "%s",
				gtk_entry_get_text(entry));
	} else {
		g_snprintf(user->proto_opt[0], sizeof(user->proto_opt[0]), "%s",
				gtk_entry_get_text(entry));
	}
}

static void irc_user_opts(GtkWidget *book, struct aim_user *user) {
	/* so here, we create the new notebook page */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
			gtk_label_new("IRC Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("Server:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(irc_print_option), user);
	if (user->proto_opt[0][0]) {
		debug_printf("setting text %s\n", user->proto_opt[0]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[0]);
	}
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	if (user->proto_opt[1][0]) {
		debug_printf("setting text %s\n", user->proto_opt[1]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[1]);
	}
	gtk_object_set_user_data(GTK_OBJECT(entry), user);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(irc_print_option), user);
	gtk_widget_show(entry);
}

static struct prpl *my_protocol = NULL;

void irc_init(struct prpl *ret) {
	ret->protocol = PROTO_IRC;
	ret->name = irc_name;
	ret->user_opts = irc_user_opts;
	ret->login = irc_login;
	ret->close = irc_close;
	ret->send_im = irc_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = NULL;
	ret->get_away_msg = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = NULL;
	ret->change_passwd = NULL;
	ret->add_buddy = NULL;
	ret->add_buddies = NULL;
	ret->remove_buddy = NULL;
	ret->add_permit = NULL;
	ret->add_deny = NULL;
	ret->warn = NULL;
	ret->accept_chat = NULL;
	ret->join_chat = irc_join_chat;
	ret->chat_invite = NULL;
	ret->chat_leave = irc_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = irc_chat_send;
	ret->keepalive = NULL;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule *handle) {
	load_protocol(irc_init);
	return NULL;
}

void gaim_plugin_remove() {
	struct prpl *p = find_prpl(PROTO_IRC);
	if (p == my_protocol)
		unload_protocol(p);
}
