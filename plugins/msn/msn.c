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
#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"

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

#define USEROPT_HOTMAIL		0

struct mod_usr_opt {
	struct aim_user *user;
	int opt;
};

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
	int inpa;
	int status;
	int away;
	time_t last_trid;
};

struct msn_name_dlg {
	GtkWidget *window;
	GtkWidget *menu;
	struct aim_user *user;
	GtkWidget *entry;
	GtkWidget *ok;
	GtkWidget *cancel;
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
static void msn_add_permit(struct gaim_connection *gc, char *who);
static void process_hotmail_msg(struct gaim_connection *gc, gchar *msgdata);
void msn_des_win(GtkWidget *a, GtkWidget *b);
void msn_newmail_dialog(const char *text);
static char *msn_normalize(const char *s);

char tochar(char *h)
{
	char alphabet[] = "0123456789abcdef";
	char tmp;
	char b;
	int v = 0;
	int i;

	for (i = strlen(h); i > 0; i--)
	{
		tmp = tolower(h[strlen(h) - i]);

		if (tmp >= '0' && tmp <= '9')
			b = tmp - '0';
		else if (tmp >= 'a' && tmp <= 'f')
			b = (tmp - 'a') + 10;

		if (i > 1)
			v =+ ((i-1) * 16) * b;
		else
			v += b;
	}

	return v;
}

char *url_decode(char *text)
{
	static char newtext[MSN_BUF_LEN];
	char *buf;
	int c = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < strlen(text); i++)
	{
		if (text[i] == '%')
			c++;
	}

	buf = (char *)malloc(strlen(text) + c + 1);

	for (i = 0, j = 0 ; text[i] != 0; i++)
	{
		if (text[i] != '%')
		{
			buf[j++] = text[i];
		}
		else
		{
			char hex[3];
			hex[0] = text[++i];
			hex[1] = text[++i];
			hex[2] = 0;

			buf[j++] = tochar(hex);
		}
	}

	buf[j] = 0;

	for (i = 0; i < strlen(buf); i++)
		newtext[i] = buf[i];

	free(buf);

	return newtext;
}

void msn_accept_add_permit(gpointer w, struct msn_ask_add_permit *ap)
{
	msn_add_permit(ap->gc, ap->user);
	/* leak if we don't free these? */
	g_free(ap->user);
	g_free(ap->friendly);
	g_free(ap);
}

void msn_cancel_add_permit(gpointer w, struct msn_ask_add_permit *ap)
{
	g_free(ap->user);
	g_free(ap->friendly);
	g_free(ap);
}

void free_msn_conn(struct msn_conn *mc)
{
	if (mc->user)
		free(mc->user);

	if (mc->secret)
		free(mc->secret);

	if (mc->session)
		free(mc->session);

	if (mc->txqueue)
		free(mc->txqueue);

	gdk_input_remove(mc->inpa);
	close(mc->fd);

	msn_connections = g_slist_remove(msn_connections, mc);

	g_free(mc);
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

struct msn_conn *find_msn_conn_by_trid(time_t trid)
{
	struct msn_conn *mc;
	GSList *conns = msn_connections;

	while (conns) {
		mc = (struct msn_conn *)conns->data;

		if (mc != NULL) {

			debug_printf("Comparing: %d <==> %d\n", mc->last_trid, trid);
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
	debug_printf("MSN(%d) <== %s", fd, buf);
}

void msn_add_request(struct gaim_connection *gc, char *buf)
{
	char **res;

	res = g_strsplit(buf, " ", 0);

	if (!strcasecmp(res[2], "RL"))
	{
		struct msn_ask_add_permit *ap = g_new0(struct msn_ask_add_permit, 1);

		snprintf(buf, MSN_BUF_LEN, "The user %s (%s) wants to add you to their buddylist.", res[4], res[5]);

		ap->user = g_strdup(res[4]);

		ap->friendly = g_strdup(url_decode(res[5]));
		ap->gc = gc;

		do_ask_dialog(buf, ap, (GtkFunction) msn_accept_add_permit, (GtkFunction) msn_cancel_add_permit);
	}

	g_strfreev(res);
}

static void msn_answer_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct msn_conn *mc = data;
	char buf[MSN_BUF_LEN];

	fcntl(source, F_SETFL, 0);

	g_snprintf(buf, MSN_BUF_LEN, "ANS 1 %s %s %s\n",mc->gc->username, mc->secret, mc->session);
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
		if (!read(source, buf + i, 1))
		{
			free_msn_conn(mc);
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	debug_printf("MSN(%d) ==> %s\n", source, buf);

	if (!strncmp("USR ", buf, 4))
	{
		char **res;

		res = g_strsplit(buf, " ", 0);
		debug_printf("%s\n",res[2]);
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
		if (!read(source, buf + i, 1))
		{
			if (md->fd == source)
			{
				hide_login_progress(gc, "Read error");
				signoff(gc);
			}

			close(source);
			
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	debug_printf("MSN(%d) ==> %s\n", source, buf);

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
	else if (!strncmp("REA ", buf, 4))
	{
		char **res;

		res = g_strsplit(buf, " ", 0);

		// Kill the old one
		g_free(md->friendly);

		// Set the new one
		md->friendly = g_strdup(res[4]);

		// And free up some memory.  That's all, folks.
		g_strfreev(res);
	}
	
	else if (!strncmp("BYE ", buf, 4))
	{
		char **res;
		struct msn_conn *mc;

		res = g_strsplit(buf, " ", 0);

		mc = find_msn_conn_by_user(res[1]);

		if (mc)
		{
			/* Looks like we need to close up some stuff :-) */
			free_msn_conn(mc);
		}
		
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
		char rahc[MSN_BUF_LEN * 2];
		       
		res = g_strsplit(buf, " ", 0);

		user = g_strdup(res[1]);
		size = atoi(res[3]);

		/* Ok, we know who we're receiving a message from as well as
		 * how big the message is */

		msgdata = (gchar *)g_malloc(sizeof(gchar) *(size + 1));
		num = recv(source, msgdata, size, 0);
		msgdata[size] = 0;

		if (num < size)
			debug_printf("MSN: Uhh .. we gots a problem!. Expected %d but got %d.\n", size, num);

		/* We should ignore messages from the user Hotmail */
		if (!strcasecmp("hotmail", res[1]))
		{
			process_hotmail_msg(gc,msgdata);
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

		g_snprintf(rahc, sizeof(rahc), "%s", strstr(msgdata, "\r\n\r\n") + 4);
		serv_got_im(gc, res[1], rahc, 0, time((time_t)NULL));

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

		debug_printf("Last trid is: %d\n", md->last_trid);
		debug_printf("This TrId is: %d\n", atoi(res[1]));

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

		debug_printf("Connected to: %s:%s\n", res[0], res[1]);

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
	else if (!strncmp("FLN ", buf, 4))
	{
		/* Someone signed off */
		char **res;

		res = g_strsplit(buf, " ", 0);

		serv_got_update(gc, res[1], 0, 0, 0, 0, 0, 0);

		g_strfreev(res);

		return;
	}
	else if (!strncmp("ADD ", buf, 4))
	{
		msn_add_request(gc,buf);
		return;
	}
	if ( (!strncmp("NLN ", buf, 4)) || (!strncmp("ILN ", buf, 4)))
	{
		int status;
		int query;
		char **res;

		res = g_strsplit(buf, " ", 0);

		if (strcasecmp(res[0], "NLN") == 0)
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
			set_login_progress(gc, 2, "Verifying");

			g_snprintf(md->protocol, 6, "MSNP2");

			g_snprintf(buf, MSN_BUF_LEN, "VER %d %s\n", trId(md), md->protocol);
			msn_write(md->fd, buf);
		}

		return;
	}

	bzero(buf, MSN_BUF_LEN);
		
	do 
	{
		if (!read(source, buf + i, 1))
		{
			hide_login_progress(gc, "Read error");
			signoff(gc);
			return;
		}

	} while (buf[i++] != '\n');

	g_strchomp(buf);

	debug_printf("MSN ==> %s\n", buf);

	/* Check to see what was just sent back to us.  We should be seeing a VER tag. */
	if (!strncmp("VER ", buf, 4) && (!strstr("MSNP2", buf)))
	{
		/* Now that we got our ver, we should send a policy request */
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
	else if (!strncmp("ADD ", buf, 4))
	{
		msn_add_request(gc,buf);
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
		
		set_login_progress(gc, 3, "Connecting to Auth");

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
				debug_printf("Before: %s\n", res[4]);
				md->friendly = g_strdup(url_decode(res[4]));
				debug_printf("After: %s\n", md->friendly);

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
		debug_printf("Could not resolve host name: %s\n", server);
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
			debug_printf("Connection would block\n");
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

	sprintf(gc->username, "%s", msn_normalize(gc->username));

	md->inpa = gdk_input_add(md->fd, GDK_INPUT_WRITE, msn_login_callback, gc);

	debug_printf("Connected.\n");
}

void msn_send_im(struct gaim_connection *gc, char *who, char *message, int away)
{
	struct msn_conn *mc;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN];

	if (!g_strcasecmp(who, gc->username))
	{
		do_error_dialog("You can not send a message to yourself!", "Gaim: MSN Error");
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
		g_snprintf(buf, MSN_BUF_LEN, "MSG %d N %d\r\n%s%s", trId(md), 
				strlen(message) + strlen(MIME_HEADER), MIME_HEADER, message);

		msn_write(mc->fd, buf);
	}

}

static void msn_add_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "ADD %d FL %s %s\n", trId(md), who, who);
	msn_write(md->fd, buf);
}

static void msn_remove_buddy(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "REM %d FL %s\n", trId(md), who);
	msn_write(md->fd, buf);
}

static void msn_rem_permit(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "REM %d AL %s\n", trId(md), who);
	msn_write(md->fd, buf);
}

static void msn_add_permit(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "ADD %d AL %s %s\n", trId(md), who, who);
	msn_write(md->fd, buf);
}

static void msn_rem_deny(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "REM %d BL %s\n", trId(md), who);
	msn_write(md->fd, buf);
}

static void msn_add_deny(struct gaim_connection *gc, char *who)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	snprintf(buf, MSN_BUF_LEN, "ADD %d BL %s %s\n", trId(md), who, who);
	msn_write(md->fd, buf);
}

static GList *msn_away_states() 
{
	GList *m = NULL;

	m = g_list_append(m, "Available");	
	m = g_list_append(m, "Away From Computer");	
	m = g_list_append(m, "Be Right Back");	
	m = g_list_append(m, "Busy");	
	m = g_list_append(m, "On The Phone");	
	m = g_list_append(m, "Out To Lunch");	

	return m;
}

static void msn_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];


	gc->away = NULL;
	
	if (msg)
	{
		gc->away = "";
		snprintf(buf, MSN_BUF_LEN, "CHG %d AWY\n", trId(md));
	}

	else if (state)
	{
		char away[4];

		gc->away = "";

		if (!strcmp(state, "Available"))
			sprintf(away, "NLN");
		else if (!strcmp(state, "Away From Computer"))
			sprintf(away, "AWY");
		else if (!strcmp(state, "Be Right Back"))
			sprintf(away, "BRB");
		else if (!strcmp(state, "Busy"))
			sprintf(away, "BSY");
		else if (!strcmp(state, "On The Phone"))
			sprintf(away, "PHN");
		else if (!strcmp(state, "Out To Lunch"))
			sprintf(away, "LUN");
		else
			sprintf(away, "NLN");

		snprintf(buf, MSN_BUF_LEN, "CHG %d %s\n", trId(md), away);
	}
	else if (gc->is_idle)
		snprintf(buf, MSN_BUF_LEN, "CHG %d IDL\n", trId(md));
	else
		snprintf(buf, MSN_BUF_LEN, "CHG %d NLN\n", trId(md));

	msn_write(md->fd, buf);
}


static void msn_set_idle(struct gaim_connection *gc, int idle)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];

	if (idle)
		snprintf(buf, MSN_BUF_LEN, "CHG %d IDL\n", trId(md));
	else
		snprintf(buf, MSN_BUF_LEN, "CHG %d NLN\n", trId(md));

	msn_write(md->fd, buf);
}

static void msn_close(struct gaim_connection *gc)
{
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];
	struct msn_conn *mc = NULL;

	while (msn_connections)
	{
		mc = (struct msn_conn *)msn_connections->data;

		free_msn_conn(mc);
	}	

	if (md->fd)
	{
		snprintf(buf, MSN_BUF_LEN, "OUT\n");
		msn_write(md->fd, buf);
		close(md->fd);
	}

	if (gc->inpa)
		gdk_input_remove(gc->inpa);

	if (md->friendly)
		free(md->friendly);

	g_free(gc->proto_data);
}

static char *msn_get_away_text(int s)
{
	switch (s)
	{
		case MSN_BUSY :
			return "Busy";
		case MSN_BRB :
			return "Be right back";
		case MSN_AWAY :
			return "Away from the computer";
		case MSN_PHONE :
			return "On the phone";
		case MSN_LUNCH :
			return "Out to lunch";
		case MSN_IDLE :
			return "Idle";
		default:
			return NULL;
	}
}
			
static void msn_buddy_menu(GtkWidget *menu, struct gaim_connection *gc, char *who)
{
	struct buddy *b = find_buddy(gc, who);
	char buf[MSN_BUF_LEN];
	GtkWidget *button;

	if (!(b->uc >> 5))
		return;

	g_snprintf(buf, MSN_BUF_LEN, "Status: %s", msn_get_away_text(b->uc >> 5));
	
	button = gtk_menu_item_new_with_label(buf);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);	
}

void msn_newmail_dialog(const char *text)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(window), "prompt", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Gaim-MSN: New Mail"));
	gtk_widget_realize(window);
	aol_icon(window->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("OK"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(msn_des_win), window);

	gtk_widget_show_all(window);
}

void msn_des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

static void mod_opt(GtkWidget *b, struct mod_usr_opt *m)
{
	if (m->user) {
		if (m->user->proto_opt[m->opt][0] == '1')
			m->user->proto_opt[m->opt][0] = '\0';
		else
			strcpy(m->user->proto_opt[m->opt],"1");
	}
}

static void free_muo(GtkWidget *b, struct mod_usr_opt *m)
{
	g_free(m);
}

static GtkWidget *msn_protoopt_button(const char *text, struct aim_user *u, int option, GtkWidget *box)
{
	GtkWidget *button;
	struct mod_usr_opt *muo = g_new0(struct mod_usr_opt, 1);
	button = gtk_check_button_new_with_label(text);
	if (u) {
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (u->proto_opt[option][0] == '1'));
	}
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	muo->user = u;
	muo->opt = option;
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(mod_opt), muo);
	gtk_signal_connect(GTK_OBJECT(button), "destroy", GTK_SIGNAL_FUNC(free_muo), muo);
	gtk_widget_show(button);
	return button;
}

static void msn_user_opts(GtkWidget *book, struct aim_user *user) {
	GtkWidget *vbox;
	GtkWidget *button;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
			gtk_label_new("MSN Options"));
	gtk_widget_show(vbox);
	button = msn_protoopt_button("Notify me of new HotMail",user,USEROPT_HOTMAIL,vbox);
}

/* 
   Process messages from Hotmail service.  right now we just check for new
   mail notifications, if the user has checking enabled.
*/

static void process_hotmail_msg(struct gaim_connection *gc, gchar *msgdata)
{
	gchar *mailnotice;
	char *mailfrom,*mailsubj,*mailct,*mailp;

	if (gc->user->proto_opt[USEROPT_HOTMAIL][0] != '1') return;
	mailfrom=NULL; mailsubj=NULL; mailct=NULL; mailp=NULL;
	mailct = strstr(msgdata,"Content-Type: ");
	mailp = strstr(mailct,";");
	if (mailct != NULL && mailp != NULL && mailp > mailct && !strncmp(mailct,"Content-Type: text/x-msmsgsemailnotification",(mailp-mailct)-1))
	{
		mailfrom=strstr(mailp,"From: ");
		mailsubj=strstr(mailp,"Subject: ");
	}
	
	if (mailfrom != NULL && mailsubj != NULL)
	{
		mailfrom += 6;
		mailp=strstr(mailfrom,"\r\n");
		if (mailp==NULL) return;
		*mailp = 0;
		mailsubj += 9;
		mailp=strstr(mailsubj,"\r\n");
		if (mailp==NULL) return;
		*mailp = 0;
		mailnotice = (gchar *)g_malloc(sizeof(gchar) *(strlen(mailfrom)+strlen(mailsubj)+128));
		sprintf(mailnotice,"Mail from %s, re: %s",mailfrom,mailsubj);
		msn_newmail_dialog(mailnotice);
		g_free(mailnotice);
	}
}

static char *msn_normalize(const char *s)
{
	static char buf[BUF_LEN];
	char *t, *u;
	int x = 0;

	g_return_val_if_fail((s != NULL), NULL);

	u = t = g_strdup(s);

	g_strdown(t);

	while (*t && (x < BUF_LEN - 1)) {
		if (*t != ' ')
			buf[x++] = *t;
		t++;
	}
	buf[x] = '\0';
	g_free(u);

	if (!strchr(buf, '@')) {
		strcat(buf, "@hotmail.com"); /* if they don't specify something, it will be hotmail.com.  msn.com
										is valid too, but hey, if they wanna use it, they gotta enter it
										themselves. */
	} else if ((u = strchr(strchr(buf, '@'), '/')) != NULL) {
		*u = '\0';
	}

	return buf;
}

void do_change_name(GtkWidget *w, struct msn_name_dlg *b)
{
	struct gaim_connection *gc = b->user->gc;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	char buf[MSN_BUF_LEN - 1];
	gchar *newname;

	newname = gtk_entry_get_text(GTK_ENTRY(b->entry));
	
	snprintf(buf, MSN_BUF_LEN, "REA %ld %s %s\n", trId(md), gc->username, newname);

	msn_write(md->fd, buf);

	msn_des_win(NULL, b->window);

	return;
}

void show_change_name(struct gaim_connection *gc)
{
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *buttons;
	GtkWidget *hbox;
	struct aim_user *tmp;
	gchar *buf;
	struct msn_data *md;

	struct msn_name_dlg *b = g_new0(struct msn_name_dlg, 1);
	if (!g_slist_find(connections, gc))
		gc = connections->data;

	tmp = gc->user;
	b->user = tmp;

	md = (struct msn_data *)gc->proto_data;

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "msn_change_name", "Gaim");

	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Change MSN Name"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(msn_des_win), b->window);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	buf = g_malloc(256);
	g_snprintf(buf, 256, "New name for %s (%s):", tmp->username, md->friendly);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	g_free(buf);

	b->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), b->entry, FALSE, FALSE, 5);
	gtk_widget_show(b->entry);

	buttons = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE, FALSE, 0);
	gtk_widget_show(buttons);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(buttons), b->cancel, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(msn_des_win), b->window);

	b->ok = picture_button(b->window, _("Ok"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(buttons), b->ok, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->ok), "clicked", GTK_SIGNAL_FUNC(do_change_name), b);


	gtk_widget_show(b->window);
	

}

static void msn_do_action(struct gaim_connection *gc, char *act)
{
	if (!strcmp(act, "Change Name"))
	{
		show_change_name(gc);
	}
}

static GList *msn_actions()
{
	GList *m = NULL;

	m = g_list_append(m, "Change Name");

	return m;
}

static char **msn_list_icon(int uc)
{
	if (uc == UC_UNAVAILABLE)
		return msn_away_xpm;
	else if (uc == UC_NORMAL)
		return msn_online_xpm;
	
	return msn_away_xpm;
}

static struct prpl *my_protocol = NULL;

void msn_init(struct prpl *ret)
{
	ret->protocol = PROTO_MSN;
	ret->name = msn_name;
	ret->list_icon = msn_list_icon;
	ret->buddy_menu = msn_buddy_menu;
	ret->user_opts = msn_user_opts;
	ret->login = msn_login;
	ret->close = msn_close;
	ret->send_im = msn_send_im;
	ret->set_info = NULL;
	ret->get_info = NULL;
	ret->away_states = msn_away_states;
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
	ret->actions = msn_actions;
	ret->do_action = msn_do_action;
	ret->normalize = msn_normalize;

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
