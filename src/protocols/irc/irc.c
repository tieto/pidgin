/*
 * gaim - IRC Protocol Plugin
 *
 * Copyright (C) 2000-2001, Rob Flynn <rob@tgflinux.com>
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

#include <config.h>


#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ctype.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"

#include "pixmaps/free_icon.xpm"

#define IRC_BUF_LEN 4096


#define USEROPT_SERV      0
#define USEROPT_PORT      1

static int chat_id = 0;

struct irc_channel {
	int id;
	gchar *name;
};

struct irc_data {
	int fd;
	int inpa;		/* used for non-block logins */

	int timer;

	int totalblocks;
	int recblocks;

	GSList *templist;
	GList *channels;
};

static char *irc_name()
{
	return "IRC";
}

static void irc_get_info(struct gaim_connection *gc, char *who);

static void irc_join_chat(struct gaim_connection *gc, int id, char *name)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	gchar *buf = (gchar *) g_malloc(IRC_BUF_LEN + 1);

	g_snprintf(buf, IRC_BUF_LEN, "JOIN %s\n", name);
	write(idata->fd, buf, strlen(buf));
	write(idata->fd, buf, strlen(buf));

	g_free(buf);
}

static void irc_update_user(struct gaim_connection *gc, char *name, int status)
{
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

static gboolean irc_request_buddy_update(gpointer data)
{
	struct gaim_connection *gc = data;
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GSList *grp = gc->groups;
	GSList *person;
	struct group *g;
	struct buddy *b;
	struct irc_channel *u;

	if (idata->templist != NULL)
		return TRUE;

	idata->recblocks = 0;
	idata->totalblocks = 1;

	/* First, let's check to see if we have anyone on our buddylist */
	if (!grp) {
		return TRUE;
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
			u->id = 0;	/* Assume by default that they're offline */
			u->name = strdup(b->name);

			write(idata->fd, " ", 1);
			write(idata->fd, u->name, strlen(u->name));
			idata->templist = g_slist_append(idata->templist, u);

			person = person->next;
		}

		grp = g_slist_next(grp);
	}
	write(idata->fd, "\n", 1);
	return TRUE;
}


static int irc_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{

	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	gchar *buf = (gchar *) g_malloc(IRC_BUF_LEN + 1);

	if (who[0] == '@' || who[0] == '+') {

		/* If the user trys to msg an op or a voice from the channel, the convo will try
		 * to send it to @nick or +nick... needless to say, this is undesirable.
		 */
		who++;
	}

	/* Before we actually send this, we should check to see if they're trying
	 * To issue a command and handle it properly. */

	if (message[0] == '/') {
		/* I'll change the implementation of this a little later :-) */
		if ((g_strncasecmp(message, "/me ", 4) == 0) && (strlen(message) > 4)) {
			/* We have /me!! We have /me!! :-) */

			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 4);
			g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG %s :%cACTION %s%c\n", who, '\001', temp,
				   '\001');
			g_free(temp);
		} else if (!g_strncasecmp(message, "/whois ", 7) && (strlen(message) > 7)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 7);
			irc_get_info(gc, temp);
			g_free(temp);

			return 0;
		}

	} else {
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG %s :%s\n", who, message);
	}

	write(idata->fd, buf, strlen(buf));

	g_free(buf);
	return 0;
}

static int find_id_by_name(struct gaim_connection *gc, char *name)
{
	gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
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

		templist = templist->next;
	}

	g_free(temp);

	/* Return -1 if we have no ID */
	return -1;
}

static struct irc_channel *find_channel_by_name(struct gaim_connection *gc, char *name)
{
	gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
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

		templist = templist->next;
	}

	g_free(temp);

	/* If we found nothing, return nothing :-) */
	return NULL;
}

static struct irc_channel *find_channel_by_id(struct gaim_connection *gc, int id)
{
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

static struct conversation *find_chat(struct gaim_connection *gc, char *name)
{
	GSList *bcs = gc->buddy_chats;
	struct conversation *b = NULL;
	char *chat = g_strdup(normalize(name));

	while (bcs) {
		b = bcs->data;
		if (!strcasecmp(normalize(b->name), chat))
			break;
		b = NULL;
		bcs = bcs->next;
	}

	g_free(chat);
	return b;
}

static void irc_chat_leave(struct gaim_connection *gc, int id);
static int irc_chat_send(struct gaim_connection *gc, int id, char *message)
{

	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *channel = NULL;
	gchar *buf = (gchar *) g_malloc(IRC_BUF_LEN + 1);
	char **kick;
	gboolean is_command = FALSE;
	/* First lets get our current channel */
	channel = find_channel_by_id(gc, id);


	if (!channel) {
		/* If for some reason we've lost our channel, let's bolt */
		g_free(buf);
		return -EINVAL;
	}


	/* Before we actually send this, we should check to see if they're trying
	 * To issue a command and handle it properly. */

	if (message[0] == '/') {

		if ((g_strncasecmp(message, "/me ", 4) == 0) && (strlen(message) > 4)) {
			/* We have /me!! We have /me!! :-) */

			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 4);

			g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG #%s :%cACTION %s%c\n", channel->name,
				   '\001', temp, '\001');
			g_free(temp);
		} else if ((g_strncasecmp(message, "/op ", 4) == 0) && (strlen(message) > 4)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 4);

			g_snprintf(buf, IRC_BUF_LEN, "MODE #%s +o %s\n", channel->name, temp);

			g_free(temp);
			is_command = TRUE;

		} else if ((g_strncasecmp(message, "/deop ", 6) == 0) && (strlen(message) > 6)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 6);
			g_snprintf(buf, IRC_BUF_LEN, "MODE #%s -o %s\n", channel->name, temp);

			g_free(temp);
			is_command = TRUE;
		}

		else if ((g_strncasecmp(message, "/voice ", 7) == 0) && (strlen(message) > 7)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 7);

			g_snprintf(buf, IRC_BUF_LEN, "MODE #%s +v %s\n", channel->name, temp);

			g_free(temp);
			is_command = TRUE;

		} else if ((g_strncasecmp(message, "/devoice ", 9) == 0) && (strlen(message) > 9)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 6);
			g_snprintf(buf, IRC_BUF_LEN, "MODE #%s -v %s\n", channel->name, temp);

			g_free(temp);
			is_command = TRUE;
		} else if ((g_strncasecmp(message, "/mode ", 6) == 0) && (strlen(message) > 6)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 6);
			g_snprintf(buf, IRC_BUF_LEN, "MODE #%s %s\n", channel->name, temp);
			g_free(temp);
			is_command = TRUE;
		}

		else if (!g_strncasecmp(message, "/whois ", 7) && (strlen(message) > 7)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);

			strcpy(temp, message + 7);
			irc_get_info(gc, temp);
			g_free(temp);
			is_command = TRUE;

		}

		else if (!g_strncasecmp(message, "/topic ", 7) && (strlen(message) > 7)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 7);

			/* Send the chat topic change request */
			serv_chat_set_topic(gc, id, temp);

			g_free(temp);
			is_command = TRUE;
		}

		else if (!g_strncasecmp(message, "/part", 5) && (strlen(message) == 5)) {

			/* If I'm not mistaken, the chat_leave command was coded under the
			 * pretense that it would only occur when someone closed the window.
			 * For this reason, the /part command will not close the window.  Nor
			 * will the window close when the user is /kicked.  I'll let you decide
			 * the best way to fix it--I'd imagine it'd just be a little line like
			 * if (convo) close (convo), but I'll let you decide where to put it.
			 */

			irc_chat_leave(gc, id);
			is_command = TRUE;
			return 0;


		}

		else if (!g_strncasecmp(message, "/join ", 6) && (strlen(message) > 6)) {

			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);

			strcpy(temp, message + 6);


			irc_join_chat(gc, 0, temp);
			g_free(temp);
			is_command = TRUE;
			return 0;
		}

		else if (!g_strncasecmp(message, "/raw ", 5) && (strlen(message) > 5)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 5);
			g_snprintf(buf, IRC_BUF_LEN, "%s\r\n", temp);
			g_free(temp);
			is_command = TRUE;
		}

		else if (!g_strncasecmp(message, "/quote ", 7) && (strlen(message) > 7)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 7);
			g_snprintf(buf, IRC_BUF_LEN, "%s\r\n", temp);
			g_free(temp);
			is_command = TRUE;
		}

		else if (!g_strncasecmp(message, "/kick ", 6) && (strlen(message) > 6)) {
			gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
			strcpy(temp, message + 6);
			kick = g_strsplit(temp, " ", 2);
			g_snprintf(buf, IRC_BUF_LEN, "KICK #%s %s :%s\r\n", channel->name, kick[0],
				   kick[1]);
			g_free(temp);
			is_command = TRUE;
		}

/* FIXME: I'll go back in and grab this later.   -- Rob */
/*
I THOUGHT THIS WOULD WORK, BUT I WAS WRONG.  WOULD SOMEONE KINDLY FIX IT?

	    
	    else if (!g_strncasecmp(message, "/help", 5)) {
	      gchar *temp = (gchar *) g_malloc(IRC_BUF_LEN + 1);
	      strcpy(temp, message + 5);
	      	  if (temp == "") {
	     
	      serv_got_chat_in(gc, id, "gAIM", 0, "Available Commands:");
	      		    serv_got_chat_in(gc, id, "gAIM", 0, " ");
				    serv_got_chat_in(gc, id, "gAIM", 0, "<b>op     voice     kick </b>");
				    serv_got_chat_in(gc, id, "gAIM", 0, "<b>deop   devoice   whois</b>");
				    serv_got_chat_in(gc, id, "gAIM", 0, "<b>me     raw       quote</b>");
				    serv_got_chat_in(gc, id, "gAIM", 0, "<b>mode</b>");
		      }
		      else {
		    serv_got_chat_in(gc, id, "gAIM", 0, "Usage: ");
		    if (temp == "op")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/op <nick></b> - Gives operator status to user.");
		    else if (temp == "deop")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/deop <nick></b> - Removes operator status from user.");
		    else if (temp == "me")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/me <action></b> - Sends an action to the channel.");
		    else if (temp == "mode")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/mode {[+|-}|o|p|s|i|t|n|b|v} [<limit][<nick>][<ban mask]</b> - Changes channel and user modes.");
		      else if (temp == "voice")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/voice <nick></b> - Gives voice status to user.");
		    else if (temp == "devoice")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/devoice <nick></b> - Removes voice status from user.");
		    else if (temp == "raw")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/raw <text></b> - Sends raw text to the server.");
		    else if (temp == "kick")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/kick [<comment>]</b> - Kicks a user out of the channel.");
		    else if (temp ==  "whois")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/whois <nick></b> - Gets information about user.");
		    else if (temp == "quote")
		      serv_got_chat_in(gc, id, "gAIM", 0, "<b>/raw <text></b> - Sends raw text to the server.");
		    else
		      serv_got_chat_in(gc, id, "gAIM", 0, "No such command.");
		  }      
		    
	      g_free(temp);
	      is_command = TRUE;
	    }
*/

	}

	else {
		g_snprintf(buf, IRC_BUF_LEN, "PRIVMSG #%s :%s\n", channel->name, message);

	}


	write(idata->fd, buf, strlen(buf));

	/* Since AIM expects us to receive the message we send, we gotta fake it */
	if (is_command == FALSE)
		serv_got_chat_in(gc, id, gc->username, 0, message, time((time_t) NULL));

	g_free(buf);

	return 0;
}

static struct conversation *find_conversation_by_id(struct gaim_connection *gc, int id)
{
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

static struct conversation *find_conversation_by_name(struct gaim_connection *gc, char *name)
{
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



static void irc_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	int i = 0;
	gchar buf[4096];
	gchar **buf2;
	struct irc_data *idata;

	idata = (struct irc_data *)gc->proto_data;


	do {
		if (read(idata->fd, buf + i, 1) < 0) {
			hide_login_progress(gc, "Read error");
			signoff(gc);
			return;
		}
	} while (buf[i++] != '\n');

	buf[--i] = '\0';
	g_strchomp(buf);
	g_print("%s\n", buf);

	/* Check for errors */

	if (((strstr(buf, "ERROR :") && (!strstr(buf, "PRIVMSG ")) &&
	      (!strstr(buf, "NOTICE ")) && (strlen(buf) > 7)))) {

		/*
		 * The ERROR command is for use by servers when reporting a serious or
		 * fatal error to its operators.  It may also be sent from one server to
		 * another but must not be accepted from any normal unknown clients.
		 *
		 * An ERROR message is for use for reporting errors which occur with a
		 * server-to-server link only.  An ERROR message is sent to the server
		 * at the other end (which sends it to all of its connected operators)
		 * and to all operators currently connected.  It is not to be passed
		 * onto any other servers by a server if it is received from a server.
		 *
		 * When a server sends a received ERROR message to its operators, the
		 * message should be encapsulated inside a NOTICE message, indicating
		 * that the client was not responsible for the error.
		 *
		 *
		 * Basically, ignore this.
		 *
		gchar *u_errormsg;

		* Let's get our error message *
		u_errormsg = g_strdup(buf + 7);

		* We got our error message.  Now, let's reaise an
		 * error dialog *

		do_error_dialog(u_errormsg, "Gaim: IRC Error");

		* And our necessary garbage collection *
		g_free(u_errormsg);
		return;

		*/
	}

	/* This should be a whois response. I only care about the first (311) one.  I might do
	 * the other's later. They're boring.  */

	if (((strstr(buf, " 311 ")) && (!strstr(buf, "PRIVMSG")) && (!strstr(buf, "NOTICE")))) {
		char **res;

		res = g_strsplit(buf, " ", 7);

		if (!strcmp(res[1], "311")) {
			char buf[8192];

			g_snprintf(buf, 4096, "<b>Nick:</b> %s<br>"
				   "<b>Host:</b> %s@%s<br>"
				   "<b>Name:</b> %s<br>", res[3], res[4], res[5], res[7] + 1);

			g_show_info_text(buf, NULL);
		}

		g_strfreev(res);
		return;
	}

	/* Autoresponse to an away message */
	if (((strstr(buf, " 301 ")) && (!strstr(buf, "PRIVMSG")) && (!strstr(buf, "NOTICE")))) {
		char **res;

		res = g_strsplit(buf, " ", 5);

		if (!strcmp(res[1], "301"))
			serv_got_im(gc, res[3], res[4] + 1, 1, time((time_t) NULL));

		g_strfreev(res);
		return;
	}

	/* Parse the list of names that we receive when we first sign on to
	 * a channel */

	if (((strstr(buf, " 353 ")) && (!strstr(buf, "PRIVMSG")) && (!strstr(buf, "NOTICE")))) {
		gchar u_host[255];
		gchar u_command[32];
		gchar u_channel[128];
		gchar u_names[IRC_BUF_LEN + 1];
		struct conversation *convo = NULL;
		int j;

		for (j = 0, i = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0';
		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j - 1] = '\0';
		i++;

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

		for (i = 0; buf2[i] != NULL; i++)
			add_chat_buddy(convo, buf2[i]);

		/* And free our pointers */
		g_strfreev(buf2);

		return;

	}

	/* Receive a list of users that are currently online */

	if (((strstr(buf, " 303 ")) && (!strstr(buf, "PRIVMSG")) && (!strstr(buf, "NOTICE")))) {
		gchar u_host[255];
		gchar u_command[32];
		gchar u_names[IRC_BUF_LEN + 1];
		int j;

		for (j = 0, i = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0';
		i++;

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
		g_strfreev(buf2);

		return;

	}


	if ((strstr(buf, " MODE ")) && (strstr(buf, "!"))
	    && (strstr(buf, "+v") || strstr(buf, "-v") || strstr(buf, "-o") || strstr(buf, "+o"))
	    && (buf[0] == ':') && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];

		gchar u_mode[5];
		char **people;
		gchar *temp, *temp_new;


		struct irc_channel *channel;
		int j;
		temp = NULL;
		temp_new = NULL;


		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}
		u_nick[j] = '\0';
		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_mode[j] = buf[i];
		}
		u_mode[j] = '\0';
		i++;




		people = g_strsplit(buf + i, " ", 3);



		channel = find_channel_by_name(gc, u_channel);

		if (!channel) {
			return;
		}

		for (j = 0; j < strlen(u_mode) - 1; j++) {


			struct conversation *convo = NULL;
			convo = find_conversation_by_id(gc, channel->id);



			temp = (gchar *) g_malloc(strlen(people[j]) + 3);
			temp_new = (gchar *) g_malloc(strlen(people[j]) + 3);
			g_snprintf(temp, strlen(people[j]) + 2, "@%s", people[j]);

			if (u_mode[1] == 'v' && u_mode[0] == '+') {
				g_snprintf(temp_new, strlen(people[j]) + 2, "+%s", people[j]);
			} else if (u_mode[1] == 'o' && u_mode[0] == '+') {
				g_snprintf(temp_new, strlen(people[j]) + 2, "@%s", people[j]);
			}

			else if (u_mode[0] == '-') {
				g_snprintf(temp_new, strlen(people[j]) + 1, "%s", people[j]);
			}



			rename_chat_buddy(convo, temp, temp_new);
			g_snprintf(temp, strlen(people[j]) + 2, "+%s", people[j]);
			rename_chat_buddy(convo, temp, temp_new);

			rename_chat_buddy(convo, people[j], temp_new);





		}
		if (temp)
			g_free(temp);
		if (temp_new)
			g_free(temp_new);

		return;
	}


	if ((strstr(buf, " KICK ")) && (strstr(buf, "!")) && (buf[0] == ':')
	    && (!strstr(buf, " NOTICE "))) {
		gchar u_channel[128];
		gchar u_nick[128];
		gchar u_comment[128];
		gchar u_who[128];

		int id;

		gchar *temp;



		struct irc_channel *channel;
		int j;

		temp = NULL;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}
		u_nick[j] = '\0';
		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_who[j] = buf[i];
		}
		u_who[j] = '\0';
		i++;
		i++;
		strcpy(u_comment, buf + i);
		g_strchomp(u_comment);

		channel = find_channel_by_name(gc, u_channel);

		if (!channel) {
			return;
		}


		id = find_id_by_name(gc, u_channel);


		if (g_strcasecmp(u_nick, gc->username) == 0) {

			/* It looks like you've been naughty! */

			serv_got_chat_left(gc, channel->id);

			idata->channels = g_list_remove(idata->channels, channel);
		} else {
			struct conversation *convo = NULL;

			/* Find their conversation window */
			convo = find_conversation_by_id(gc, channel->id);

			if (!convo) {
				/* Some how the window doesn't exist. 
				 * Let's get out of here */
				return;
			}

			/* And remove their name */
			/* If the person is an op or voice, this won't work.
			 * so we'll just do a nice hack and remove nick and
			 * @nick and +nick.  Truly wasteful.
			 */

			temp = (gchar *) g_malloc(strlen(u_who) + 3);
			g_snprintf(temp, strlen(u_who) + 2, "@%s", u_who);
			remove_chat_buddy(convo, temp);
			g_free(temp);
			temp = (gchar *) g_malloc(strlen(u_who) + 3);
			g_snprintf(temp, strlen(u_who) + 2, "+%s", u_who);
			remove_chat_buddy(convo, temp);
			remove_chat_buddy(convo, u_who);

			g_free(temp);

		}

		/* Go Home! */
		return;
	}

	if ((strstr(buf, " TOPIC ")) && (buf[0] == ':') && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];
		gchar u_topic[128];
		int j;
		struct conversation *chatroom = NULL;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}
		u_nick[j] = 0;
		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			if (buf[i] == '\0')
				break;

			u_channel[j] = buf[i];
		}

		for (j = 0; buf[i] != ':'; j++, i++) {
		}
		i++;

		strcpy(u_topic, buf + i);
		g_strchomp(u_topic);

		chatroom = find_chat(gc, u_channel);

		if (!chatroom)
			return;

		chat_set_topic(chatroom, u_nick, u_topic);

		return;
	}


	if ((strstr(buf, " JOIN ")) && (strstr(buf, "!")) && (buf[0] == ':')
	    && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];

		struct irc_channel *channel;
		int j;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0';
		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}

		i++;

		strcpy(u_channel, buf + i);

		g_strchomp(u_channel);

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

	if ((strstr(buf, " NICK ")) && (strstr(buf, "!")) && (buf[0] == ':')
	    && (!strstr(buf, " NOTICE "))) {

		gchar old[128];
		gchar new[128];

		GList *templist;
		gchar *temp, *temp_new;
		struct irc_channel *channel;
		int j;
		temp = temp_new = NULL;
		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			old[j] = buf[i];
		}

		old[j] = '\0';
		i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
		}

		i++;
		strcpy(new, buf + i);

		g_strchomp(new);

		templist = ((struct irc_data *)gc->proto_data)->channels;

		while (templist) {
			struct conversation *convo = NULL;
			channel = templist->data;

			convo = find_conversation_by_id(gc, channel->id);

			/* If the person is an op or voice, this won't work.
			 * so we'll just do a nice hack and rename nick and
			 * @nick and +nick.  Truly wasteful.
			 */

			temp = (gchar *) g_malloc(strlen(old) + 5);
			temp_new = (gchar *) g_malloc(strlen(new) + 5);
			g_snprintf(temp_new, strlen(new) + 2, "@%s", new);
			g_snprintf(temp, strlen(old) + 2, "@%s", old);
			rename_chat_buddy(convo, temp, temp_new);
			g_snprintf(temp, strlen(old) + 2, "+%s", old);
			g_snprintf(temp_new, strlen(new) + 2, "+%s", new);
			rename_chat_buddy(convo, temp, temp_new);
			rename_chat_buddy(convo, old, new);
			if (temp)
				g_free(temp);
			if (temp_new)
				g_free(temp_new);

			templist = templist->next;
		}
		return;
	}


	if ((strstr(buf, "QUIT ")) && (buf[0] == ':') && (strstr(buf, "!"))
	    && (!strstr(buf, " NOTICE "))) {

		gchar u_nick[128];
		gchar *temp;
		GList *templist;

		struct irc_channel *channel;
		int j;


		temp = NULL;
		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0';

		templist = ((struct irc_data *)gc->proto_data)->channels;

		while (templist) {
			struct conversation *convo = NULL;
			channel = templist->data;

			convo = find_conversation_by_id(gc, channel->id);

			/* If the person is an op or voice, this won't work.
			 * so we'll just do a nice hack and remove nick and
			 * @nick and +nick.  Truly wasteful.
			 */

			temp = (gchar *) g_malloc(strlen(u_nick) + 2);
			g_snprintf(temp, strlen(u_nick) + 2, "@%s", u_nick);
			remove_chat_buddy(convo, temp);
			g_free(temp);
			temp = (gchar *) g_malloc(strlen(u_nick) + 2);
			g_snprintf(temp, strlen(u_nick) + 2, "+%s", u_nick);
			remove_chat_buddy(convo, temp);
			remove_chat_buddy(convo, u_nick);



			templist = templist->next;
		}

		g_free(temp);

		return;
	}



	if ((strstr(buf, " PART ")) && (strstr(buf, "!")) && (buf[0] == ':')
	    && (!strstr(buf, " NOTICE "))) {

		gchar u_channel[128];
		gchar u_nick[128];
		gchar *temp;
		struct irc_channel *channel;
		int j;
		temp = NULL;
		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}
		u_nick[j] = '\0';

		i++;

		for (j = 0; buf[i] != '#'; j++, i++) {
		}

		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			if (buf[i] == '\0') {
				break;
			}
			u_channel[j] = buf[i];
		}
		u_channel[j] = '\0';

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
				return;
			}

			/* And remove their name */
			/* If the person is an op or voice, this won't work.
			 * so we'll just do a nice hack and remove nick and
			 * @nick and +nick.  Truly wasteful.
			 */

			temp = (gchar *) g_malloc(strlen(u_nick) + 3);
			g_snprintf(temp, strlen(u_nick) + 2, "@%s", u_nick);
			remove_chat_buddy(convo, temp);
			g_free(temp);
			temp = (gchar *) g_malloc(strlen(u_nick) + 3);
			g_snprintf(temp, strlen(u_nick) + 2, "+%s", u_nick);
			remove_chat_buddy(convo, temp);
			g_free(temp);
			remove_chat_buddy(convo, u_nick);


		}

		/* Go Home! */
		return;
	}

	if ((strstr(buf, " NOTICE ")) && (buf[0] == ':')) {
		gchar u_nick[128];
		gchar u_host[255];
		gchar u_command[32];
		gchar u_channel[128];
		gchar u_message[IRC_BUF_LEN];
		int j;

		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0';
		i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j - 1] = '\0';
		i++;


		/* Now that everything is parsed, the rest of this baby must be our message */
		strncpy(u_message, buf + i, IRC_BUF_LEN);

		/* Now, lets check the message to see if there's anything special in it */
		if (u_message[0] == '\001') {
			if ((g_strncasecmp(u_message, "\001PING ", 6) == 0) && (strlen(u_message) > 6)) {
				/* Someone's triyng to ping us.  Let's respond */
				gchar u_arg[24];
				gchar u_buf[200];
				unsigned long tend = time((time_t *) NULL);
				unsigned long tstart;

				printf("LA: %s\n", buf);

				strcpy(u_arg, u_message + 6);
				u_arg[strlen(u_arg) - 1] = '\0';

				tstart = atol(u_arg);

				g_snprintf(u_buf, sizeof(u_buf), "Ping Reply From %s: [%ld seconds]",
					   u_nick, tend - tstart);

				do_error_dialog(u_buf, "Gaim IRC - Ping Reply");

				return;
			}
		}

	}


	if ((strstr(buf, " PRIVMSG ")) && (buf[0] == ':')) {
		gchar u_nick[128];
		gchar u_host[255];
		gchar u_command[32];
		gchar u_channel[128];
		gchar u_message[IRC_BUF_LEN];
		gboolean is_closing;

		int j;


		for (j = 0, i = 1; buf[i] != '!'; j++, i++) {
			u_nick[j] = buf[i];
		}

		u_nick[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_host[j] = buf[i];
		}

		u_host[j] = '\0';
		i++;

		for (j = 0; buf[i] != ' '; j++, i++) {
			u_command[j] = buf[i];
		}

		u_command[j] = '\0';
		i++;

		for (j = 0; buf[i] != ':'; j++, i++) {
			u_channel[j] = buf[i];
		}

		u_channel[j - 1] = '\0';
		i++;


		/* Now that everything is parsed, the rest of this baby must be our message */
		strncpy(u_message, buf + i, IRC_BUF_LEN);

		/* Now, lets check the message to see if there's anything special in it */
		if (u_message[0] == '\001') {
			if (g_strncasecmp(u_message, "\001VERSION", 8) == 0) {
				/* Looks like we have a version request.  Let
				 * us handle it thusly */

				g_snprintf(buf, IRC_BUF_LEN,
					   "NOTICE %s :%cVERSION GAIM %s:The Pimpin Penguin AIM Clone:%s%c\n",
					   u_nick, '\001', VERSION, WEBSITE, '\001');

				write(idata->fd, buf, strlen(buf));

				/* And get the heck out of dodge */
				return;
			}

			if ((g_strncasecmp(u_message, "\001PING ", 6) == 0) && (strlen(u_message) > 6)) {
				/* Someone's triyng to ping us.  Let's respond */
				gchar u_arg[24];

				strcpy(u_arg, u_message + 6);
				u_arg[strlen(u_arg) - 1] = '\0';

				g_snprintf(buf, IRC_BUF_LEN, "NOTICE %s :%cPING %s%c\n", u_nick, '\001',
					   u_arg, '\001');

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


		/* OK, It is a chat or IM message.  Here, let's translate the IRC formatting into
		 * good ol' fashioned gtkimhtml style hypertext markup. */


		is_closing = FALSE;

		while (strchr(u_message, '\002')) {	/* \002 = ^B */
			gchar *current;
			gchar *temp, *free_here;


			temp = g_strdup(strchr(u_message, '\002'));
			free_here = temp;
			temp++;

			current = strchr(u_message, '\002');
			*current = '<';
			current++;
			if (is_closing) {
				*current = '/';
				current++;
			}
			*current = 'b';
			current++;
			*current = '>';
			current++;


			while (*temp != '\0') {
				*current = *temp;
				current++;
				temp++;
			}
			*current = '\0';
			g_free(free_here);

			is_closing = !is_closing;
		}

		is_closing = FALSE;
		while (strchr(u_message, '\037')) {	/* \037 = ^_ */
			gchar *current;
			gchar *temp, *free_here;


			temp = g_strdup(strchr(u_message, '\037'));
			free_here = temp;
			temp++;

			current = strchr(u_message, '\037');
			*current = '<';
			current++;
			if (is_closing) {
				*current = '/';
				current++;
			}
			*current = 'u';
			current++;
			*current = '>';
			current++;


			while (*temp != '\0') {
				*current = *temp;
				current++;
				temp++;
			}
			*current = '\0';
			g_free(free_here);
			is_closing = !is_closing;

		}

		while (strchr(u_message, '\003')) {	/* \003 = ^C */

			/* This is color formatting.  IRC uses its own weird little system
			 * that we must translate to HTML. */


			/* The format is something like this:
			 *         ^C5 or ^C5,3
			 * The number before the comma is the foreground color, after is the
			 * background color.  Either number can be 1 or two digits.
			 */

			gchar *current;
			gchar *temp, *free_here;
			gchar *font_tag, *body_tag;
			int fg_color, bg_color;

			temp = g_strdup(strchr(u_message, '\003'));
			free_here = temp;
			temp++;

			fg_color = bg_color = -1;
			body_tag = font_tag = "";

			/* Parsing the color information: */
			do {
				if (!isdigit(*temp))
					break;	/* This translates to </font> */
				fg_color = (int)(*temp - 48);
				temp++;
				if (isdigit(*temp)) {
					fg_color = (fg_color * 10) + (int)(*temp - 48);
					temp++;
				}
				if (*temp != ',')
					break;
				temp++;
				if (!isdigit(*temp))
					break;	/* This translates to </font> */
				bg_color = (int)(*temp - 48);
				temp++;
				if (isdigit(*temp)) {
					bg_color = (bg_color * 10) + (int)(*temp - 48);
					temp++;
				}
			} while (FALSE);

			if (fg_color > 15)
				fg_color = fg_color % 16;
			if (bg_color > 15)
				bg_color = bg_color % 16;

			switch (fg_color) {
			case -1:
				font_tag = "</font></body>";
				break;
			case 0:	/* WHITE */
				font_tag = "<font color=\"#ffffff\">";
				/* If no background color is specified, we're going to make it black anyway.
				 * That's probably what the sender anticipated the background color to be. 
				 * White on white would be illegible.
				 */
				if (bg_color == -1) {
					body_tag = "<body bgcolor=\"#000000\">";
				}
				break;
			case 1:	/* BLACK */
				font_tag = "<font color=\"#000000\">";
				break;
			case 2:	/* NAVY BLUE */
				font_tag = "<font color=\"#000066\">";
				break;
			case 3:	/* GREEN */
				font_tag = "<font color=\"#006600\">";
				break;
			case 4:	/* RED */
				font_tag = "<font color=\"#ff0000\">";
				break;
			case 5:	/* MAROON */
				font_tag = "<font color=\"#660000\">";
				break;
			case 6:	/* PURPLE */
				font_tag = "<font color=\"#660066\">";
				break;
			case 7:	/* DISGUSTING PUKE COLOR */
				font_tag = "<font color=\"#666600\">";
				break;
			case 8:	/* YELLOW */
				font_tag = "<font color=\"#cccc00\">";
				break;
			case 9:	/* LIGHT GREEN */
				font_tag = "<font color=\"#33cc33\">";
				break;
			case 10:	/* TEAL */
				font_tag = "<font color=\"#00acac\">";
				break;
			case 11:	/* CYAN */
				font_tag = "<font color=\"#00ccac\">";
				break;
			case 12:	/* BLUE */
				font_tag = "<font color=\"#0000ff\">";
				break;
			case 13:	/* PINK */
				font_tag = "<font color=\"#cc00cc\">";
				break;
			case 14:	/* GREY */
				font_tag = "<font color=\"#666666\">";
				break;
			case 15:	/* SILVER */
				font_tag = "<font color=\"#00ccac\">";
				break;
			}

			switch (bg_color) {
			case 0:	/* WHITE */
				body_tag = "<body bgcolor=\"#ffffff\">";
				break;
			case 1:	/* BLACK */
				body_tag = "<body bgcolor=\"#000000\">";
				break;
			case 2:	/* NAVY BLUE */
				body_tag = "<body bgcolor=\"#000066\">";
				break;
			case 3:	/* GREEN */
				body_tag = "<body bgcolor=\"#006600\">";
				break;
			case 4:	/* RED */
				body_tag = "<body bgcolor=\"#ff0000\">";
				break;
			case 5:	/* MAROON */
				body_tag = "<body bgcolor=\"#660000\">";
				break;
			case 6:	/* PURPLE */
				body_tag = "<body bgcolor=\"#660066\">";
				break;
			case 7:	/* DISGUSTING PUKE COLOR */
				body_tag = "<body bgcolor=\"#666600\">";
				break;
			case 8:	/* YELLOW */
				body_tag = "<body bgcolor=\"#cccc00\">";
				break;
			case 9:	/* LIGHT GREEN */
				body_tag = "<body bgcolor=\"#33cc33\">";
				break;
			case 10:	/* TEAL */
				body_tag = "<body bgcolor=\"#00acac\">";
				break;
			case 11:	/* CYAN */
				body_tag = "<body bgcolor=\"#00ccac\">";
				break;
			case 12:	/* BLUE */
				body_tag = "<body bgcolor=\"#0000ff\">";
				break;
			case 13:	/* PINK */
				body_tag = "<body bgcolor=\"#cc00cc\">";
				break;
			case 14:	/* GREY */
				body_tag = "<body bgcolor=\"#666666\">";
				break;
			case 15:	/* SILVER */
				body_tag = "<body bgcolor=\"#00ccac\">";
				break;
			}

			current = strchr(u_message, '\003');

			while (*body_tag != '\0') {
				*current = *body_tag;
				current++;
				body_tag++;
			}

			while (*font_tag != '\0') {
				*current = *font_tag;
				current++;
				font_tag++;
			}

			while (*temp != '\0') {
				*current = *temp;
				current++;
				temp++;
			}
			*current = '\0';
			g_free(free_here);
			is_closing = !is_closing;

		}

		while (strchr(u_message, '\017')) {	/* \017 = ^O */
			gchar *current;
			gchar *temp, *free_here;


			temp = g_strdup(strchr(u_message, '\017'));
			free_here = temp;
			temp++;

			current = strchr(u_message, '\017');
			*current = '<';
			current++;
			*current = '/';
			current++;
			*current = 'b';
			current++;
			*current = '>';
			current++;
			*current = '<';
			current++;
			*current = '/';
			current++;
			*current = 'u';
			current++;
			*current = '>';
			current++;

			while (*temp != '\0') {
				*current = *temp;
				current++;
				temp++;
			}
			*current = '\0';
			g_free(free_here);
		}

		/* Let's check to see if we have a channel on our hands */
		if (u_channel[0] == '#') {
			/* Yup.  We have a channel */
			int id;

			id = find_id_by_name(gc, u_channel);
			if (id != -1) {
				serv_got_chat_in(gc, id, u_nick, 0, u_message, time((time_t) NULL));

			}

		} else {
			/* Nope. Let's treat it as a private message */

			gchar *temp;
			temp = NULL;

			temp = (gchar *) g_malloc(strlen(u_nick) + 5);
			g_snprintf(temp, strlen(u_nick) + 2, "@%s", u_nick);


			/* If I get a message from SeanEgn, and I already have a window
			 * open for him as @SeanEgn or +SeanEgn, this will keep it in the
			 * same window.  Unfortunately, if SeanEgn loses his op status
			 * (a sad thing indeed), the messages will still appear to come from
			 * @SeanEgn, until that convo is closed.
			 */

			if (find_conversation(temp)) {
				serv_got_im(gc, temp, u_message, 0, time((time_t) NULL));
				g_free(temp);
				return;
			} else {
				g_snprintf(temp, strlen(u_nick) + 2, "+%s", u_nick);
				if (find_conversation(temp)) {
					serv_got_im(gc, temp, u_message, 0, time((time_t) NULL));
					g_free(temp);
					return;
				} else {
					g_free(temp);
					serv_got_im(gc, u_nick, u_message, 0, time((time_t) NULL));
					return;
				}
			}
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

static void irc_close(struct gaim_connection *gc)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	GList *chats = idata->channels;
	struct irc_channel *cc;

	gchar *buf = (gchar *) g_malloc(IRC_BUF_LEN);

	g_snprintf(buf, IRC_BUF_LEN, "QUIT :Download GAIM [%s]\n", WEBSITE);
	write(idata->fd, buf, strlen(buf));

	g_free(buf);

	if (idata->timer)
		g_source_remove(idata->timer);

	while (chats) {
		cc = (struct irc_channel *)chats->data;
		g_free(cc->name);
		chats = g_list_remove(chats, cc);
		g_free(cc);
	}

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	if (idata->inpa)
		gaim_input_remove(idata->inpa);

	close(idata->fd);
	g_free(gc->proto_data);
}

static void irc_chat_leave(struct gaim_connection *gc, int id)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	struct irc_channel *channel;
	gchar *buf = (gchar *) g_malloc(IRC_BUF_LEN + 1);

	channel = find_channel_by_id(gc, id);

	if (!channel) {
		return;
	}

	g_snprintf(buf, IRC_BUF_LEN, "PART #%s\n", channel->name);
	write(idata->fd, buf, strlen(buf));

	g_free(buf);
}

static void irc_login_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct irc_data *idata;
	char buf[4096];

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	idata = gc->proto_data;

	if (source == -1) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}

	if (idata->fd != source)
		idata->fd = source;

	g_snprintf(buf, 4096, "NICK %s\n USER %s localhost %s :GAIM (%s)\n",
		   gc->username, g_get_user_name(), gc->user->proto_opt[USEROPT_SERV], WEBSITE);

	if (write(idata->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}

	idata->inpa = gaim_input_add(idata->fd, GAIM_INPUT_READ, irc_callback, gc);
	idata->inpa = 0;

	/* Now lets sign ourselves on */
	account_online(gc);
	serv_finish_login(gc);

	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);

	/* we don't call this now because otherwise some IRC servers might not like us */
	idata->timer = g_timeout_add(20000, irc_request_buddy_update, gc);
}

static void irc_login(struct aim_user *user)
{
	char buf[4096];

	struct gaim_connection *gc = new_gaim_conn(user);
	struct irc_data *idata = gc->proto_data = g_new0(struct irc_data, 1);

	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);

	idata->fd = proxy_connect(user->proto_opt[USEROPT_SERV],
				  user->proto_opt[USEROPT_PORT][0] ? atoi(user->
									  proto_opt[USEROPT_PORT]) :
				  6667, irc_login_callback, gc);
	if (!user->gc || (idata->fd < 0)) {
		hide_login_progress(gc, "Unable to create socket");
		signoff(gc);
		return;
	}
}

static GList *irc_user_opts()
{
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Server:";
	puo->def = "irc.mozilla.org";
	puo->pos = USEROPT_SERV;
	m = g_list_append(m, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Port:";
	puo->def = "6667";
	puo->pos = USEROPT_PORT;
	m = g_list_append(m, puo);

	return m;
}

static char **irc_list_icon(int uc)
{
	return free_icon_xpm;
}

/* Send out a ping request to the specified user */
static void irc_send_ping(struct gaim_connection *gc, char *who)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	char buf[BUF_LEN];

	g_snprintf(buf, BUF_LEN, "PRIVMSG %s :%cPING %ld%c\n", who, '\001', time((time_t *) NULL),
		   '\001');

	write(idata->fd, buf, strlen(buf));
}

/* Do a whois check on someone :-) */
static void irc_get_info(struct gaim_connection *gc, char *who)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	char buf[BUF_LEN];

	if (((who[0] == '@') || (who[0] == '+')) && (strlen(who) > 1))
		g_snprintf(buf, BUF_LEN, "WHOIS %s\n", who + 1);
	else
		g_snprintf(buf, BUF_LEN, "WHOIS %s\n", who);
	write(idata->fd, buf, strlen(buf));
}

static GList *irc_buddy_menu(struct gaim_connection *gc, char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Ping");
	pbm->callback = irc_send_ping;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Whois");
	pbm->callback = irc_get_info;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}


static void irc_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	char buf[BUF_LEN];

	if (msg)
		g_snprintf(buf, BUF_LEN, "AWAY :%s\n", msg);
	else
		g_snprintf(buf, BUF_LEN, "AWAY\n");

	write(idata->fd, buf, strlen(buf));
}

static void irc_fake_buddy(struct gaim_connection *gc, char *who)
{
	/* Heh, there is no buddy list. We fake it.
	 * I just need this here so the add and remove buttons will
	 * show up */
}

static void irc_chat_set_topic(struct gaim_connection *gc, int id, char *topic)
{
	struct irc_channel *ic = NULL;
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	char buf[BUF_LEN];

	ic = find_channel_by_id(gc, id);

	/* If we ain't in no channel, foo, gets outta da kitchen beeyotch */
	if (!ic)
		return;

	/* Prepare our command */
	g_snprintf(buf, BUF_LEN, "TOPIC #%s :%s\n", ic->name, topic);

	/* And send it */
	write(idata->fd, buf, strlen(buf));
}

static struct prpl *my_protocol = NULL;

void irc_init(struct prpl *ret)
{
	ret->protocol = PROTO_IRC;
	ret->options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD;
	ret->name = irc_name;
	ret->list_icon = irc_list_icon;
	ret->buddy_menu = irc_buddy_menu;
	ret->user_opts = irc_user_opts;
	ret->login = irc_login;
	ret->close = irc_close;
	ret->send_im = irc_send_im;
	ret->join_chat = irc_join_chat;
	ret->chat_leave = irc_chat_leave;
	ret->chat_send = irc_chat_send;
	ret->get_info = irc_get_info;
	ret->set_away = irc_set_away;
	ret->add_buddy = irc_fake_buddy;
	ret->remove_buddy = irc_fake_buddy;
	ret->chat_set_topic = irc_chat_set_topic;
	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(irc_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_IRC);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "IRC";
}

char *description()
{
	return PRPL_DESC("IRC");
}

#endif
