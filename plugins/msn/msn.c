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

#include "config.h"

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
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "md5.h"

#include "pixmaps/msn_online.xpm"
#include "pixmaps/msn_away.xpm"

#define MIME_HEADER "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nX-MMS-IM-Format: FN=MS%20Sans%20Serif; EF=; CO=0; CS=0; PF=0\r\n\r\n"

#define MSN_BUF_LEN 8192

#define MSN_ONLINE  1
#define MSN_BUSY    2
#define MSN_IDLE    3
#define MSN_BRB     4
#define MSN_AWAY    5
#define MSN_PHONE   6
#define MSN_LUNCH   7
#define MSN_OFFLINE 8
#define MSN_HIDDEN  9

#define MSN_SIGNON_GOT_XFR	0x0001
#define MSN_SIGNON_SENT_USR	0x0002

struct msn_data {
	int fd;

	char protocol[6];
	char *friendly;
	gchar *policy;
	int inpa;
	int status;
	time_t last_trid;
};

struct msn_conn {
	gchar *user;
	int inpa;
	int fd;
	struct gaim_connection *gc;
	char *secret;
	char *session;
	time_t last_trid;
	char *txqueue;
};

GSList *msn_connections = NULL;

unsigned long long globalc = 0;
static void msn_callback(gpointer data, gint source, GdkInputCondition condition);

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

struct msn_conn *find_msn_conn_by_trid(time_t trid)
{
	struct msn_conn *mc;
	GSList *conns = msn_connections;

	while (conns) {
		mc = (struct msn_conn *)conns->data;

		if (mc != NULL) {

			printf("Comparing: %d <==> %d\n", mc->last_trid, trid);
			if (mc->last_trid == trid) {
				return mc;
			}
		}

		conns = g_slist_next(conns);
	}

	return NULL;
}

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

time_t trId(struct msn_data *md)
{
	md->last_trid = time((time_t *)NULL) + globalc++;
	return md->last_trid;
}

void msn_write(int fd, char *buf)
{
	write(fd, buf, strlen(buf));
	printf("MSN(%d) <== %s", fd, buf);
}

static void msn_answer_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct msn_conn *mc = data;
	char buf[MSN_BUF_LEN];

	fcntl(source, F_SETFL, 0);

	g_snprintf(buf, MSN_BUF_LEN, "ANS 1 %s %s %s\n", mc->gc->username, mc->secret, mc->session);
	msn_write(mc->fd, buf);

	gdk_input_remove(mc->inpa);
	mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_READ, msn_callback, mc->gc);
	
	/* Append our connection */
	msn_connections = g_slist_append(msn_connections, mc);
}

static void msn_invite_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct msn_conn *mc = data;
	struct msn_data *md = (struct msn_data *)mc->gc->proto_data;
	char buf[MSN_BUF_LEN];
	struct gaim_connection *gc = mc->gc;
	int i = 0;
	
	fcntl(source, F_SETFL, 0);

	printf("GOT HERE|\n");

	if (condition == GDK_INPUT_WRITE)
	{
		/* We just got in here */
		gdk_input_remove(mc->inpa);
		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_READ, msn_invite_callback, mc);

		/* Write our signon request */
		g_snprintf(buf, MSN_BUF_LEN, "USR %d %s %s\n", mc->last_trid, mc->gc->username, mc->secret);
		msn_write(mc->fd, buf);
		return;
	}

	bzero(buf, MSN_BUF_LEN);
	do 
	{
		if (read(source, buf + i, 1) < 0)
		{
			hide_login_progress(gc, "Read error");
			signoff(gc);
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	printf("!!MSN(%d) ==> %s\n", source, buf);
	printf("Got here.\n");

	if (!strncmp("USR ", buf, 4))
	{
		char **res;

		printf("Got here2.\n");

		res = g_strsplit(buf, " ", 0);
		printf("%s\n",res[2]);
		if (strcasecmp("OK", res[2]))
		{
			g_strfreev(res);
			close(mc->fd);
			return;
		}

		/* We've authorized.  Let's send an invite request */
		g_snprintf(buf, MSN_BUF_LEN, "CAL %d %s\n", trId(md), mc->user);
		msn_write(source, buf);
		return;
	}

	else if (!strncmp("JOI ", buf, 4))
	{
		/* Looks like they just joined! Write their queued message */
		g_snprintf(buf, MSN_BUF_LEN, "MSG %d N %d\r\n%s%s", trId(md), strlen(mc->txqueue) + strlen(MIME_HEADER), MIME_HEADER, mc->txqueue);

		msn_write(source, buf);

		gdk_input_remove(mc->inpa);
		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_READ, msn_callback, mc->gc);

		return;

	}
}

static void msn_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN];
	int i = 0;
	int num;

	bzero(buf, MSN_BUF_LEN);
		
	do 
	{
		if (read(source, buf + i, 1) < 0)
		{
			hide_login_progress(gc, "Read error");
			signoff(gc);
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	printf("MSN(%d) ==> %s\n", source, buf);

	if (!strncmp("NLN ", buf, 4) || !strncmp("ILN ", buf, 4))
	{
		int status;
		int query;
		char **res;

		res = g_strsplit(buf, " ", 0);

		if (!strcmp(res[0], "NLN"))
			query = 1;
		else
			query = 2;

		if (!strcasecmp(res[query], "NLN"))
			status = UC_NORMAL;
		else if (!strcasecmp(res[query], "BSY"))
			status = UC_NORMAL | (MSN_BUSY << 5);
		else if (!strcasecmp(res[query], "IDL"))
			status = UC_NORMAL | (MSN_IDLE << 5);
		else if (!strcasecmp(res[query], "BRB"))
			status = UC_NORMAL | (MSN_BRB << 5);
		else if (!strcasecmp(res[query], "AWY"))
			status = UC_UNAVAILABLE;
		else if (!strcasecmp(res[query], "PHN"))
			status = UC_NORMAL | (MSN_PHONE << 5);
		else if (!strcasecmp(res[query], "LUN"))
			status = UC_NORMAL | (MSN_LUNCH << 5);
		else
			status = UC_NORMAL;

		serv_got_update(gc, res[query+1], 1, 0, 0, 0, status, 0);

		g_strfreev(res);

		return;

	}
	else if (!strncmp("MSG ", buf, 4))
	{
		/* We are receiving an incoming message */
		gchar **res;
		gchar *user;
		gchar *msgdata;
		int size;
		       
		res = g_strsplit(buf, " ", 0);

		user = g_strdup(res[1]);
		size = atoi(res[3]);

		/* Ok, we know who we're receiving a message from as well as
		 * how big the message is */

		msgdata = (gchar *)g_malloc(sizeof(gchar) *(size + 1));
		num = recv(source, msgdata, size, 0);
		msgdata[size] = 0;

		if (num < size)
			printf("MSN: Uhh .. we gots a problem!. Expected %d but got %d.\n", size, num);

		/* We should ignore messages from the user Hotmail */
		if (!strcasecmp("hotmail", res[1]))
		{
			g_strfreev(res);
			g_free(msgdata);
			return;
		}

		/* Check to see if any body is in the message */
		if (!strcmp(strstr(msgdata, "\r\n\r\n") + 4, "\r\n"))
		{
			g_strfreev(res);
			g_free(msgdata);
			return;
		}

		/* Otherwise, everything is ok. Let's show the message. Skipping, 
		 * of course, the header. */

		serv_got_im(gc, res[1], strstr(msgdata, "\r\n\r\n") + 4, 0);

		g_strfreev(res);
		g_free(msgdata);

		return;
	}
	else if (!strncmp("RNG ", buf, 4))
	{
		/* Ok, someone wants to talk to us. Ring ring?  Hi!!! */
		gchar **address;
		gchar **res;
		struct msn_conn *mc = g_new0(struct msn_conn, 1);

		res = g_strsplit(buf, " ", 0);
		address = g_strsplit(res[2], ":", 0);

		if (!(mc->fd = msn_connect(address[0], atoi(address[1]))))
		{
			/* Looks like we had an error connecting. */
			g_strfreev(address);
			g_strfreev(res);
			g_free(mc);
			return;
		}

		/* Set up our struct with user and input watcher */
		mc->user = g_strdup(res[5]);
		mc->secret = g_strdup(res[4]);
		mc->session = g_strdup(res[1]);
		mc->gc = gc;

		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_WRITE, msn_answer_callback, mc);

		g_strfreev(address);
		g_strfreev(res);

		return;
	}
	else if (!strncmp("XFR ", buf, 4))
	{
		char **res;
		char *host;
		char *port;
		struct msn_conn *mc;

		res = g_strsplit(buf, " ", 0);

		printf("Last trid is: %d\n", md->last_trid);
		printf("This TrId is: %d\n", atoi(res[1]));

		mc = find_msn_conn_by_trid(atoi(res[1]));

		if (!mc)
		{
			g_strfreev(res);
			return;
		}

		strcpy(buf, res[3]);

		mc->secret = g_strdup(res[5]);
		mc->session = g_strdup(res[1]);

		g_strfreev(res);

		res = g_strsplit(buf, ":", 0);

		/* Now we have the host and port */
		if (!(mc->fd = msn_connect(res[0], atoi(res[1]))))
			return;

		printf("Connected to: %s:%s\n", res[0], res[1]);

		if (mc->inpa)
			gdk_input_remove(mc->inpa);

		mc->inpa = gdk_input_add(mc->fd, GDK_INPUT_WRITE, msn_invite_callback, mc);

		g_strfreev(res);

		return;
	}
	else if (!strncmp("LST ", buf, 4))
	{
		char **res;

		res = g_strsplit(buf, " ", 0);

		/* If we have zero buddies, abort */
		if (atoi(res[5]) == 0)
		{
			g_strfreev(res);
			return;
		}

		/* First, let's check the list type */
		if (!strcmp("FL", res[2]))
		{
			/* We're dealing with a forward list.  Add them
			 * to our buddylist and continue along our
			 * merry little way */

			struct buddy *b;

			b = find_buddy(gc, res[6]);

			if (!b)
				add_buddy(gc, "Buddies", res[6], res[7]);
		}

		g_strfreev(res);

		return;
	}

}

static void msn_login_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN];
	int i = 0;

	if (!gc->inpa)
	{
		fcntl(source, F_SETFL, 0);

		gdk_input_remove(md->inpa);
		md->inpa = 0;

		gc->inpa = gdk_input_add(md->fd, GDK_INPUT_READ, msn_login_callback, gc);
	
		if (md->status & MSN_SIGNON_GOT_XFR)
		{
			/* Looks like we were transfered here.  Just send a sign on */
			set_login_progress(gc, 3, "Signing On");
			g_snprintf(buf, MSN_BUF_LEN, "USR %d %s I %s\n", md->last_trid, md->policy, gc->username);
			msn_write(md->fd, buf);
		
			/* Reset this bit */
			md->status ^= MSN_SIGNON_GOT_XFR;
		}
		else
		{
			/* Otherwise, send an initial request */
			set_login_progress(gc, 2, "Verifiying");

			g_snprintf(md->protocol, 6, "MSNP2");

			g_snprintf(buf, MSN_BUF_LEN, "VER %d %s\n", trId(md), md->protocol);
			msn_write(md->fd, buf);
		}

		return;
	}

	bzero(buf, MSN_BUF_LEN);
		
	do 
	{
		if (read(source, buf + i, 1) < 0)
		{
			hide_login_progress(gc, "Read error");
			signoff(gc);
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	printf("MSN ==> %s\n", buf);

	/* Check to see what was just sent back to us.  We should be seeing a VER tag. */
	if (!strncmp("VER ", buf, 4) && (!strstr("MSNP2", buf)))
	{
		/* Now that we got our ver, we shoudl send a policy request */
		g_snprintf(buf, MSN_BUF_LEN, "INF %d\n", trId(md));
		msn_write(md->fd, buf);

		return;
	}
	else if (!strncmp("INF ", buf, 4))
	{
		char **res;
		
		/* Make a copy of our resulting policy */
		res = g_strsplit(buf, " ", 0);
		md->policy = g_strdup(res[2]);

		/* And send our signon packet */
		set_login_progress(gc, 3, "Signing On");

		g_snprintf(buf, MSN_BUF_LEN, "USR %d %s I %s\n", trId(md), md->policy, gc->username);
		msn_write(md->fd, buf);
		
		g_strfreev(res);

		return;
	}
	else if (!strncmp("XFR ", buf, 4))
	{
		char **res;
		char *host;
		char *port;

		res = g_strsplit(buf, " ", 0);

		strcpy(buf, res[3]);

		g_strfreev(res);

		res = g_strsplit(buf, ":", 0);

		close(md->fd);
		
		set_login_progress(gc, 1, "Connecting");

		/* Now we have the host and port */
		if (!(md->fd = msn_connect(res[0], atoi(res[1]))))
		{
			hide_login_progress(gc, "Error connecting to server");
			signoff(gc);
			return;
		}

		g_strfreev(res);

		md->status |= MSN_SIGNON_GOT_XFR;

		gdk_input_remove(gc->inpa);
		gc->inpa = 0;

		md->inpa = gdk_input_add(md->fd, GDK_INPUT_WRITE, msn_login_callback, gc);

		return;
	}
	else if (!strncmp("USR ", buf, 4))
	{
		if (md->status & MSN_SIGNON_SENT_USR) 
		{
			char **res;

			res = g_strsplit(buf, " ", 0);

			if (strcasecmp("OK", res[2]))
			{
				hide_login_progress(gc, "Error signing on");
				signoff(gc);
			}
			else
			{
				md->friendly = g_strdup(res[4]);

				/* Ok, ok.  Your account is FINALLY online.  Ya think Microsoft
				 * could have had any more steps involved? */
		
				set_login_progress(gc, 4, "Fetching config");

				/* Sync our buddylist */
				g_snprintf(buf, MSN_BUF_LEN, "SYN %d 0\n", trId(md));
				msn_write(md->fd, buf);

				/* And set ourselves online */
				g_snprintf(buf, MSN_BUF_LEN, "CHG %d NLN\n", trId(md));
				msn_write(md->fd, buf);

				account_online(gc);
				serv_finish_login(gc);

				if (bud_list_cache_exists(gc))
					do_import(NULL, gc);

				gdk_input_remove(gc->inpa);
				gc->inpa = gdk_input_add(md->fd, GDK_INPUT_READ, msn_callback, gc);
			}

			g_strfreev(res);
		}
		else
		{
			char **res;
			char buf2[MSN_BUF_LEN];
			int j;
			md5_state_t st;
			md5_byte_t di[16];

			res = g_strsplit(buf, " ", 0);

			/* Make a copy of our MD5 Hash key */
			strcpy(buf, res[4]);

			/* Generate our secret with our key and password */
			snprintf(buf2, MSN_BUF_LEN, "%s%s", buf, gc->password);

			md5_init(&st);
			md5_append(&st, (const md5_byte_t *)buf2, strlen(buf2));
			md5_finish(&st, di);
	
			/* Now that we have the MD5 Hash, lets' hex encode this bad boy. I smoke bad crack. */
			sprintf(buf, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
			di[0],di[1],di[2],di[3],di[4],di[5],di[6],di[7],di[8],di[9],di[10],di[11],di[12],
			di[13],di[14],di[15]);

			/* And now, send our final sign on packet */
			g_snprintf(buf2, MSN_BUF_LEN, "USR %s %s S %s\n", res[1], md->policy, buf);
			msn_write(md->fd, buf2);
	
			md->status |= MSN_SIGNON_SENT_USR;

			g_strfreev(res);
		}

		return;
	}
}

int msn_connect(char *server, int port)
{
	int fd;
	struct hostent *host;
	struct sockaddr_in site;

	if (!(host = gethostbyname(server)))
	{
		printf("Could not resolve host name: %s\n", server);
		return -1;
	}

	bzero(&site, sizeof(struct sockaddr_in));
	site.sin_port = htons(port);
	memcpy(&site.sin_addr, host->h_addr, host->h_length);
	site.sin_family = host->h_addrtype;

	fd = socket(host->h_addrtype, SOCK_STREAM, 0);

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, (struct sockaddr *)&site, sizeof(struct sockaddr_in)) < 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR))
		{
			printf("Connection would block\n");
			return fd;
		}

		close(fd);
		fd = -1;
	}
	
	return fd;
}

void msn_login(struct aim_user *user)
{
	struct gaim_connection *gc = new_gaim_conn(user);
	struct msn_data *md = gc->proto_data = g_new0(struct msn_data, 1);

	gc->inpa = 0;

	set_login_progress(gc, 1, "Connecting");

	while (gtk_events_pending())
		gtk_main_iteration();

	if (!g_slist_find(connections, gc))
		return;

	md->status = 0;

	if (!(md->fd = msn_connect("messenger.hotmail.com", 1863)))
	{
		hide_login_progress(gc, "Error connecting to server");
		signoff(gc);
		return;
	}

	md->inpa = gdk_input_add(md->fd, GDK_INPUT_WRITE, msn_login_callback, gc);

	printf("Connected.\n");
}

void msn_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{
	struct msn_conn *mc;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!g_strcasecmp(who, gc->username))
	{
		do_error_dialog("You can not send a message to  yourself!", "Gaim: MSN Error");
		return;
	}

	mc = find_msn_conn_by_user(who);

	/* If we're not already in a conversation with
	 * this person then we have to do some tricky things. */

	if (!mc)
	{
		gchar buf2[MSN_BUF_LEN];
		gchar *address;
		gchar *auth;
		gchar **res;

		/* Request a new switchboard connection */
		g_snprintf(buf, MSN_BUF_LEN, "XFR %d SB\n", trId(md));
		msn_write(md->fd, buf);

		mc = g_new0(struct msn_conn, 1);

		mc->user = g_strdup(who);
		mc->gc = gc;
		mc->last_trid = md->last_trid;
		mc->txqueue = g_strdup(message);

		/* Append our connection */
		msn_connections = g_slist_append(msn_connections, mc);
	}
	else
	{
		g_snprintf(buf, MSN_BUF_LEN, "MSG %d N %d\r\n%s%s", trId(md), strlen(message) + strlen(MIME_HEADER), MIME_HEADER, message);

		msn_write(mc->fd, buf);
	}

}

static char **msn_list_icon(int uc)
{
	if (uc == UC_UNAVAILABLE)
		return msn_away_xpm;
	else if (uc == UC_NORMAL)
		return msn_online_xpm;
	
	return msn_online_xpm;
}

static struct prpl *my_protocol = NULL;

void msn_init(struct prpl *ret)
{
	ret->protocol = PROTO_MSN;
	ret->name = msn_name;
	ret->list_icon = msn_list_icon;
	ret->buddy_menu = NULL;
	ret->user_opts = NULL;
	ret->login = msn_login;
	ret->close = NULL;
	ret->send_im = msn_send_im;
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
	ret->rem_permit = NULL;
	ret->add_deny = NULL;
	ret->rem_deny = NULL;
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
	load_protocol(msn_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_MSN);
	if (p == my_protocol)
		unload_protocol(p);
}
