/*
 * gaim - MSN Protocol Plugin
 *
 * Copyright (C) 2000, Rob Flynn <rob@tgflinux.com>
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
#include "md5.h"

#include "pixmaps/msn_online.xpm"
#include "pixmaps/msn_away.xpm"

#define MSN_BUF_LEN 4096

#define MSN_OPT_SERVER   0
#define MSN_OPT_PORT     1

#define MIME_HEADER "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nX-MMS-IM-Format: FN=MS%20Sans%20Serif; EF=; CO=0; CS=0; PF=0\r\n\r\n"

#define MSN_ONLINE  1
#define MSN_BUSY    2
#define MSN_IDLE    3
#define MSN_BRB     4
#define MSN_AWAY    5
#define MSN_PHONE   6
#define MSN_LUNCH   7
#define MSN_OFFLINE 8
#define MSN_HIDDEN  9


struct msn_ask_add_permit {
	struct gaim_connection *gc;
	char *user;
	char *friendly;
};

struct msn_data {
	int fd;

	char protocol[6];
	char *friendly;
	gchar *policy;
};

struct msn_conn {
	gchar *user;
	int inpa;
	int fd;
};

void msn_callback(gpointer data, gint fd, GdkInputCondition condition);

GSList *msn_connections = NULL;

static char *msn_name()
{
	return "MSN";
}

char *name()
{
	return "MSN";
}

char *description()
{
	return "Allows gaim to use the MSN protocol.  For some reason, this frightens me.";
}

struct msn_conn *find_msn_conn_by_user(gchar * user)
{
	struct msn_conn *mc;
	GSList *conns = msn_connections;

	while (conns) {
		mc = (struct msn_conn *)conns->data;

		if (mc != NULL) {
			if (strcasecmp(mc->user, user) == 0) {
				return mc;
			}
		}

		conns = g_slist_next(conns);
	}

	return NULL;
}

void msn_read_line(char *buf, int fd)
{

	int status;
	char c;
	int i = 0;

	printf("%s (%d)\n", strerror(errno), errno);
	
	do {
		status = recv(fd, &c, 1, 0);

		if (!status)
			return;

		buf[i] = c;
		i++;
	} while (c != '\n');

	buf[i] = '\0';
	g_strchomp(buf);

	/* I'm a bastard again :-) */
	printf("MSN: %s\n", buf);
	printf("%s (%d)\n", strerror(errno), errno);
}

int msn_connect(char *server, int port)
{
	int fd;
	struct hostent *host;
	struct sockaddr_in site;

	printf("Connecting to '%s' on '%d'\n", server, port);
	host = gethostbyname(server);
	if (!host) {
		return -1;
	}

	site.sin_family = AF_INET;
	site.sin_addr.s_addr = *(long *)(host->h_addr);
	site.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&site, sizeof(site)) < 0) {
		return -1;
	}

	return fd;
}

static void msn_add_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "ADD %d FL %s %s\n", trId, who, who);
	write(mdata->fd, buf, strlen(buf));
}

static void msn_rem_permit(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "REM %d AL %s %s\n", trId, who, who);
	write(mdata->fd, buf, strlen(buf));
}

static void msn_add_permit(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "ADD %d AL %s %s\n", trId, who, who);
	write(mdata->fd, buf, strlen(buf));
}

static void msn_rem_deny(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "REM %d BL %s %s\n", trId, who, who);
	write(mdata->fd, buf, strlen(buf));
}

static void msn_add_deny(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "ADD %d BL %s %s\n", trId, who, who);
	write(mdata->fd, buf, strlen(buf));
}

static void msn_remove_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	g_snprintf(buf, 4096, "REM %d FL %s\n", trId, who);
	write(mdata->fd, buf, strlen(buf));
}

void msn_accept_add_permit(gpointer w, struct msn_ask_add_permit *ap)
{
	gchar buf[4096];

	msn_add_permit(ap->gc, ap->user);
}

void msn_cancel_add_permit(gpointer w, struct msn_ask_add_permit *ap)
{

	g_free(ap->user);
	g_free(ap->friendly);
	g_free(ap);
}

void msn_callback(gpointer data, gint fd, GdkInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct msn_data *mdata;
	char c;
	int i = 0;
	int status;
	gchar buf[4096];
	gchar **resps;

	mdata = (struct msn_data *)gc->proto_data;

	do {
		/* Read data from whatever connection our inpa
		 * refered us from */
		status = recv(fd, &c, 1, 0);

		if (!status)
			return;

		buf[i] = c;
		i++;
	} while (c != '\n');

	buf[i] = '\0';

	g_strchomp(buf);

	printf("MSN: %s\n", buf);

	if (strlen(buf) == 0) {
		return;
	}

	resps = g_strsplit(buf, " ", 0);

	/* See if someone is bumping us */
	if (strcasecmp(resps[0], "BYE") == 0) {
		struct msn_conn *mc;
		GSList *conns = msn_connections;

		/* Yup.  Let's find their convo and kill it */

		mc = find_msn_conn_by_user(resps[1]);

		/* If we have the convo, remove it */
		if (mc != NULL) {
			/* and remove it */
			conns = g_slist_remove(conns, mc);

			g_free(mc->user);
			gdk_input_remove(mc->inpa);
			close(mc->fd);


			g_free(mc);
		}

		g_strfreev(resps);
		return;
	}

	if (strcasecmp(resps[0], "ADD") == 0) {

		if (strcasecmp(resps[2], "RL") == 0) {
			gchar buf[4096];
			struct msn_ask_add_permit *ap = g_new0(struct msn_ask_add_permit, 1);

			g_snprintf(buf, 4096, "The user %s (%s) wants to add you to their buddylist.",
				   resps[4], resps[5]);

			ap->user = g_strdup(resps[4]);
			ap->friendly = g_strdup(resps[5]);
			ap->gc = gc;

			do_ask_dialog(buf, ap, (GtkFunction) msn_accept_add_permit,
				      (GtkFunction) msn_cancel_add_permit);
		}

		g_strfreev(resps);
		return;
	}

	if (strcasecmp(resps[0], "REM") == 0) {

		if (strcasecmp(resps[2], "RL") == 0) {
			msn_rem_permit(gc, resps[4]);
		}

		g_strfreev(resps);
		return;
	}

	if (strcasecmp(resps[0], "FLN") == 0) {
		serv_got_update(gc, resps[1], 0, 0, 0, 0, 0, 0);
	}

	/* Check buddy update status */
	if ( (strcasecmp(resps[0], "NLN") == 0) || (strcasecmp(resps[0], "ILN") == 0)) {
		int status;
		int query;

		if (strcasecmp(resps[0], "NLN") == 0) 
			query = 1;
		else
			query = 2;
		
		if (!strcasecmp(resps[query], "NLN"))
			status = UC_NORMAL;
		else if (!strcasecmp(resps[query], "BSY"))
			status = UC_NORMAL | (MSN_BUSY << 5);
		else if (!strcasecmp(resps[query], "IDL"))
			status = UC_NORMAL | (MSN_IDLE << 5);
		else if (!strcasecmp(resps[query], "BRB"))
			status = UC_NORMAL | (MSN_BRB << 5);
		else if (!strcasecmp(resps[query], "AWY"))
			status = UC_UNAVAILABLE;
		else if (!strcasecmp(resps[query], "PHN"))
			status = UC_NORMAL | (MSN_PHONE << 5);
		else if (!strcasecmp(resps[query], "LUN"))
			status = UC_NORMAL | (MSN_LUNCH << 5);
		else
			status = UC_NORMAL;

		serv_got_update(gc, resps[query+1], 1, 0, 0, 0, status, 0);

		g_strfreev(resps);
		return;
	}

	/* Check to see if we have an incoming buddylist */
	if (strcasecmp(resps[0], "LST") == 0) {
		/* Check to see if there are any buddies in the list */
		if (atoi(resps[5]) == 0) {
			/* No buddies */
			g_strfreev(resps);
			return;
		}

		/* FIXME: We should support the permit and deny
		 * lists as well */

		if (strcasecmp(resps[2], "FL") == 0) {
			struct buddy *b;

			b = find_buddy(gc, resps[6]);

			if (!b)
				add_buddy(gc, "Buddies", resps[6], resps[6]);
		}

		g_strfreev(resps);
		return;
	}

	/* Check to see if we got a message request */
	if (strcasecmp(resps[0], "MSG") == 0) {
		gchar *message;
		gchar *buf2;
		int size;
		int status;

		/* Determine our message size */
		size = atoi(resps[3]);

		buf2 = (gchar *) g_malloc(sizeof(gchar) * (size + 1));
		status = recv(fd, buf2, size, 0);
		buf2[size] = 0;

		if (strcasecmp("hotmail", resps[1]) == 0) {

			if (strstr(buf2, "Content-Type: text/x-msmsgsemailnotification")) {
				g_snprintf(buf, sizeof(buf), "%s has new hotmail", gc->username);
				do_error_dialog(buf, "Gaim: MSN New Mail");
			}
			
			g_strfreev(resps);
			return;
		}

		/* Looks like we got the message. If it's blank, let's bail */
		if (strcasecmp(strstr(buf2, "\r\n\r\n") + 4, "\r\n") == 0) {
			g_free(buf2);
			g_strfreev(resps);
			return;
		}

		serv_got_im(gc, resps[1], strstr(buf2, "\r\n\r\n") + 4, 0);

		g_free(buf2);
		g_strfreev(resps);
		return;
	}


	/* Check to see if we got a ring request */
	if (strcasecmp(resps[0], "RNG") == 0) {
		gchar **address;
		struct msn_conn *mc = g_new0(struct msn_conn, 1);

		address = g_strsplit(resps[2], ":", 0);

		if (!(mc->fd = msn_connect(address[0], atoi(address[1])))) {
			do_error_dialog(resps[5], "Msg Err from");
			g_strfreev(address);
			g_strfreev(resps);
			g_free(mc);
			return;
		}

		mc->user = g_strdup(resps[5]);

		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_READ, msn_callback, gc);

		g_snprintf(buf, 4096, "ANS 1 %s %s %s\n", gc->username, resps[4], resps[1]);
		write(mc->fd, buf, strlen(buf));

		msn_connections = g_slist_append(msn_connections, mc);

		g_strfreev(address);
		g_strfreev(resps);
		return;
	}

	g_strfreev(resps);

}

void msn_login(struct aim_user *user)
{
	time_t trId = time((time_t *) NULL);
	char buf[4096];
	char buf2[4096];

	struct gaim_connection *gc = new_gaim_conn(user);
	struct msn_data *mdata = gc->proto_data = g_new0(struct msn_data, 1);
	char c;
	int i;
	int status;

	md5_state_t st;
	md5_byte_t di[16];
	int x;

	gchar **results;

	g_snprintf(mdata->protocol, strlen("MSNP2") + 1, "MSNP2");

	set_login_progress(gc, 1, "Connecting");

	while (gtk_events_pending())
		gtk_main_iteration();
	if (!g_slist_find(connections, gc))
		return;

	if (!(mdata->fd = msn_connect("messenger.hotmail.com", 1863))) {
		hide_login_progress(gc, "Error connection to server");
		signoff(gc);
		return;
	}

	printf("AAA: %s (%d)\n", strerror(errno), errno);

	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);

	while (gtk_events_pending())
		gtk_main_iteration();

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	/* This is where we will attempt to sign on */
	g_snprintf(buf, 4096, "VER %d %s\n", trId, mdata->protocol);
	write(mdata->fd, buf, strlen(buf));

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	msn_read_line(&buf2, mdata->fd);

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	buf[strlen(buf) - 1] = '\0';
	if (strcmp(buf, buf2) != 0) {
		hide_login_progress(gc, buf2);
		signoff(gc);
		return;
	}

	/* Looks like our versions matched up.  Let's find out
	 * which policy we should use */

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	g_snprintf(buf, 4096, "INF %d\n", trId);
	write(mdata->fd, buf, strlen(buf));

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	msn_read_line(&buf2, mdata->fd);
	results = g_strsplit(buf2, " ", 0);
	mdata->policy = g_strdup(results[2]);
	g_strfreev(results);

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	/* We've set our policy.  Now, lets attempt a sign on */
	g_snprintf(buf, 4096, "USR %d %s I %s\n", trId, mdata->policy, gc->username);
	write(mdata->fd, buf, strlen(buf));

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	msn_read_line(&buf2, mdata->fd);

	printf("AAA: %s (%d)\n", strerror(errno), errno);
	/* This is where things get kinky */
	results = g_strsplit(buf2, " ", 0);

	/* Are we being transfered to another server ?  */
	if (strcasecmp(results[0], "XFR") == 0) {
		/* Yup.  We should connect to the _new_ server */
		strcpy(buf, results[3]);
		g_strfreev(results);

		results = g_strsplit(buf, ":", 0);

		/* Connect to the new server */
		if (!(mdata->fd = msn_connect(results[0], atoi(results[1])))) {
			hide_login_progress(gc, "Error connecting to server");
			signoff(gc);
			g_strfreev(results);
			return;
		}


		g_strfreev(results);

		/* We're now connected to the new server.  Send signon
		 * information again */
		g_snprintf(buf, 4096, "USR %d %s I %s\n", trId, mdata->policy, gc->username);
		write(mdata->fd, buf, strlen(buf));

		msn_read_line(&buf, mdata->fd);
		results = g_strsplit(buf, " ", 0);

	}

	/* Otherwise, if we have a USR response, let's handle it */
	if (strcasecmp("USR", results[0]) == 0) {
		/* Looks like we got a response.  Let's get our challenge
		 * string */
		strcpy(buf, results[4]);

	} else {
		g_strfreev(results);
		hide_login_progress(gc, "Error signing on");
		signoff(gc);
		return;
	}
	g_strfreev(results);

	/* Build our response string */
	snprintf(buf2, 4096, "%s%s", buf, gc->password);

	/* Use the MD5 Hashing */
	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)buf2, strlen(buf2));
	md5_finish(&st, di);

	/* And now encode it in hex */
	sprintf(buf, "%02x", di[0]);
	for (x = 1; x < 16; x++) {
		sprintf(buf, "%s%02x", buf, di[x]);
	}

	/* And now we should fire back a response */
	g_snprintf(buf2, 4096, "USR %d %s S %s\n", trId, mdata->policy, buf);
	write(mdata->fd, buf2, strlen(buf2));


	msn_read_line(&buf, mdata->fd);

	results = g_strsplit(buf, " ", 0);

	if ((strcasecmp("USR", results[0]) == 0) && (strcasecmp("OK", results[2]) == 0)) {
		mdata->friendly = g_strdup(results[4]);
		g_strfreev(results);
	} else {
		g_strfreev(results);
		hide_login_progress(gc, "Error signing on!");
		signoff(gc);
		return;

	}
	set_login_progress(gc, 3, "Getting Config");
	while (gtk_events_pending())
		gtk_main_iteration();

	g_snprintf(buf, 4096, "SYN %d 0\n", trId);
	write(mdata->fd, buf, strlen(buf));

	/* Go online */
	g_snprintf(buf, 4096, "CHG %d NLN\n", trId);
	write(mdata->fd, buf, strlen(buf));

	account_online(gc);
	serv_finish_login(gc);

	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);

	/* We want to do this so that we can read what's going on */
	gc->inpa = gdk_input_add(mdata->fd, GDK_INPUT_READ, msn_callback, gc);
}

void msn_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{
	struct msn_conn *mc;
	struct msn_data *mdata;
	time_t trId = time((time_t *) NULL);
	char *buf;

	mdata = (struct msn_data *)gc->proto_data;
	mc = find_msn_conn_by_user(who);

	/* Are we trying to send a message to ourselves?  Naughty us! */
	if (!g_strcasecmp(who, gc->username)) {
		do_error_dialog("You cannot send a message to yourself!!", "GAIM: Msn Error");
		return;
	}
	
	if (mc == NULL) {
		gchar buf2[4096];
		gchar *address;
		gchar *auth;
		gchar **resps;

		/* Request a new switchboard connection */
		g_snprintf(buf2, 4096, "XFR %d SB\n", trId);
		write(mdata->fd, buf2, strlen(buf2));

		/* Read the results */
		msn_read_line(&buf2, mdata->fd);

		resps = g_strsplit(buf2, " ", 0);

		address = g_strdup(resps[3]);
		auth = g_strdup(resps[5]);
		g_strfreev(resps);

		resps = g_strsplit(address, ":", 0);

		mc = g_new0(struct msn_conn, 1);

		if (!(mc->fd = msn_connect(resps[0], atoi(resps[1])))) {
			g_strfreev(resps);
			g_free(address);
			g_free(auth);
			g_free(mc);
			return;
		}

		/* Looks like we got connected ok. Now, let's verify */
		g_snprintf(buf2, 4096, "USR %d %s %s\n", trId, gc->username, auth);
		write(mc->fd, buf2, strlen(buf2));

		/* Read the results */
		msn_read_line(&buf2, mc->fd);
		g_strfreev(resps);

		resps = g_strsplit(buf2, " ", 0);

		if (!(strcasecmp("OK", resps[2]) == 0)) {
			g_free(auth);
			g_free(address);
			g_strfreev(resps);
			g_free(mc);
			return;
		}

		mc->user = g_strdup(who);
		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_READ, msn_callback, gc);

		msn_connections = g_slist_append(msn_connections, mc);

		/* Now we must invite our new user to the switchboard session */
		g_snprintf(buf2, 4096, "CAL %d %s\n", trId, who);
		write(mc->fd, buf2, strlen(buf2));

		/* FIXME: This causes a delay.  I will make some sort of queing feature to prevent
		 * this from being needed */

		while ((!strstr(buf2, "JOI")) && (!g_strncasecmp(buf2, "215", 3))) {
			msn_read_line(&buf2, mc->fd);
		}

		g_free(auth);
		g_free(address);
		g_strfreev(resps);

	}

	/* Always practice safe sets :-) */
	buf = (gchar *) g_malloc(sizeof(gchar) * (strlen(message) + strlen(MIME_HEADER) + 64));

	g_snprintf(buf, strlen(message) + strlen(MIME_HEADER) + 64, "MSG %d N %d\r\n%s%s", trId,
		   strlen(message) + strlen(MIME_HEADER), MIME_HEADER, message);

	write(mc->fd, buf, strlen(buf));

	g_free(buf);
}

static void msn_close(struct gaim_connection *gc)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	GSList *conns = msn_connections;
	struct msn_conn *mc = NULL;
	char buf[4096];

	while (conns) {
		mc = (struct msn_conn *)conns->data;

		if (mc->inpa > 0)
			gdk_input_remove(mc->inpa);

		if (mc->fd > 0)
			close(mc->fd);

		if (mc->user != NULL)
			g_free(mc->user);

		conns = g_slist_remove(conns, mc);
		g_free(mc);
	}


	g_snprintf(buf, 4096, "OUT\n");
	write(mdata->fd, buf, strlen(buf));

	if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);

	close(mdata->fd);

	if (mdata->friendly != NULL)
		g_free(mdata->friendly);

	g_free(gc->proto_data);

	debug_printf(_("Signed off.\n"));

}

static void msn_set_away(struct gaim_connection *gc, char *msg)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	if (msg) {
		g_snprintf(buf, 4096, "CHG %d AWY\n", trId);
	} else if (gc->is_idle) {
		g_snprintf(buf, 4096, "CHG %d IDL\n", trId);
	} else {
		g_snprintf(buf, 4096, "CHG %d NLN\n", trId);
	}

	write(mdata->fd, buf, strlen(buf));

}

static void msn_set_idle(struct gaim_connection *gc, int idle)
{
	struct msn_data *mdata = (struct msn_data *)gc->proto_data;
	time_t trId = time((time_t *) NULL);
	gchar buf[4096];

	if (idle) {
		g_snprintf(buf, 4096, "CHG %d IDL\n", trId);
	} else {
		g_snprintf(buf, 4096, "CHG %d NLN\n", trId);
	}

	write(mdata->fd, buf, strlen(buf));

}

static char **msn_list_icon(int uc)
{
	if (uc == UC_UNAVAILABLE)
		return msn_away_xpm;
	else if (uc == UC_NORMAL)
		return msn_online_xpm;

	/* Our default online icon */
	return msn_online_xpm;
}

static struct prpl *my_protocol = NULL;

void msn_init(struct prpl *ret)
{
	ret->protocol = PROTO_MSN;
	ret->name = msn_name;
	ret->list_icon = msn_list_icon;
	ret->action_menu = NULL;
	ret->user_opts = NULL;
	ret->login = msn_login;
	ret->close = msn_close;
	ret->send_im = msn_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->set_away = msn_set_away;
	ret->get_away_msg = NULL;
	ret->set_dir = NULL;
	ret->get_dir = NULL;
	ret->dir_search = NULL;
	ret->set_idle = msn_set_idle;
	ret->change_passwd = NULL;
	ret->add_buddy = msn_add_buddy;
	ret->add_buddies = NULL;
	ret->remove_buddy = msn_remove_buddy;
	ret->add_permit = msn_add_permit;
	ret->rem_permit = msn_rem_permit;
	ret->add_deny = msn_add_deny;
	ret->rem_deny = msn_rem_deny;
	ret->warn = NULL;
	ret->accept_chat = NULL;
	ret->join_chat = NULL;
	ret->chat_invite = NULL;
	ret->chat_leave = NULL;
	ret->chat_whisper = NULL;
	ret->chat_send = NULL;
	ret->keepalive = NULL;

	my_protocol = ret;
}

char *gaim_plugin_init(GModule * handle)
{
	load_protocol(msn_init);
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_MSN);
	if (p == my_protocol)
		unload_protocol(p);
}
