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
#include <sys/types.h>
#include <sys/stat.h>
#include "prpl.h"
#include "multi.h"
#include "gaim.h"
#include "proxy.h"

#include "pixmaps/admin_icon.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/away_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/free_icon.xpm"

#define REVISION "penguin"

#define TYPE_SIGNON    1
#define TYPE_DATA      2
#define TYPE_ERROR     3
#define TYPE_SIGNOFF   4
#define TYPE_KEEPALIVE 5

#define FLAPON "FLAPON\r\n\r\n"
#define ROAST "Tic/Toc"

#define TOC_HOST "toc.oscar.aol.com"
#define TOC_PORT 9898
#define AUTH_HOST "login.oscar.aol.com"
#define AUTH_PORT 5190
#define LANGUAGE "english"

#define STATE_OFFLINE 0
#define STATE_FLAPON 1
#define STATE_SIGNON_REQUEST 2
#define STATE_ONLINE 3
#define STATE_PAUSE 4

#define VOICE_UID     "09461341-4C7F-11D1-8222-444553540000"
#define FILE_SEND_UID "09461343-4C7F-11D1-8222-444553540000"
#define IMAGE_UID     "09461345-4C7F-11D1-8222-444553540000"
#define B_ICON_UID    "09461346-4C7F-11D1-8222-444553540000"
#define STOCKS_UID    "09461347-4C7F-11D1-8222-444553540000"
#define FILE_GET_UID  "09461348-4C7F-11D1-8222-444553540000"
#define GAMES_UID     "0946134a-4C7F-11D1-8222-444553540000"

struct ft_request {
	struct gaim_connection *gc;
        char *user;
	char UID[2048];
        char *cookie;
        char *ip;
        int port;
        char *message;
        char *filename;
	int files;
        int size;
};

struct buddy_icon {
	guint32 hash;
	guint32 len;
	time_t time;
	void *data;
};

struct toc_data {
	int toc_fd;
	int seqno;
	int state;
};

struct sflap_hdr {
	unsigned char ast;
	unsigned char type;
	unsigned short seqno;
	unsigned short len;
};

struct signon {
	unsigned int ver;
	unsigned short tag;
	unsigned short namelen;
	char username[80];
};

/* constants to identify proto_opts */
#define USEROPT_AUTH      0
#define USEROPT_AUTHPORT  1

static GtkWidget *join_chat_spin = NULL;
static GtkWidget *join_chat_entry = NULL;

static void toc_login_callback(gpointer, gint, GdkInputCondition);
static void toc_callback(gpointer, gint, GdkInputCondition);
static unsigned char *roast_password(char *);
static void accept_file_dialog(struct ft_request *);

/* ok. this function used to take username/password, and return 0 on success.
 * now, it takes username/password, and returns NULL on error or a new gaim_connection
 * on success. */
static void toc_login(struct aim_user *user)
{
	struct gaim_connection *gc;
	struct toc_data *tdt;
	char buf[80];

	gc = new_gaim_conn(user);
	gc->proto_data = tdt = g_new0(struct toc_data, 1);

	g_snprintf(buf, sizeof buf, "Looking up %s",
		   user->proto_opt[USEROPT_AUTH][0] ? user->proto_opt[USEROPT_AUTH] : TOC_HOST);
	set_login_progress(gc, 1, buf);

	debug_printf("* Client connects to TOC\n");
	tdt->toc_fd =
	    proxy_connect(user->proto_opt[USEROPT_AUTH][0] ? user->proto_opt[USEROPT_AUTH] : TOC_HOST,
			  user->proto_opt[USEROPT_AUTHPORT][0] ?
				  atoi(user->proto_opt[USEROPT_AUTHPORT]) : TOC_PORT,
			  toc_login_callback, gc);

	if (!user->gc || (tdt->toc_fd < 0)) {
		g_snprintf(buf, sizeof(buf), "Connect to %s failed", user->proto_opt[USEROPT_AUTH]);
		hide_login_progress(gc, buf);
		signoff(gc);
		return;
	}
}

static void toc_login_callback(gpointer data, gint source, GdkInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct toc_data *tdt = gc->proto_data;
	char buf[80];

	if (source == -1) {
		/* we didn't successfully connect. tdt->toc_fd is valid here */
		hide_login_progress(gc, "Unable to connect.");
		signoff(gc);
		return;
	}

	if (tdt->toc_fd == 0)
		tdt->toc_fd = source;

	debug_printf("* Client sends \"FLAPON\\r\\n\\r\\n\"\n");
	if (write(tdt->toc_fd, FLAPON, strlen(FLAPON)) < 0) {
		hide_login_progress(gc, "Disconnected.");
		signoff(gc);
		return;
	}
	tdt->state = STATE_FLAPON;

	/* i know a lot of people like to look at gaim to see how TOC works. so i'll comment
	 * on what this does. it's really simple. when there's data ready to be read from the
	 * toc_fd file descriptor, toc_callback is called, with gc passed as its data arg. */
	gc->inpa = gdk_input_add(tdt->toc_fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, toc_callback, gc);

	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);
}

static void toc_close(struct gaim_connection *gc)
{
	if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	gc->inpa = 0;
	close(((struct toc_data *)gc->proto_data)->toc_fd);
	g_free(gc->proto_data);
}

static int sflap_send(struct gaim_connection *gc, char *buf, int olen, int type)
{
	int len;
	int slen = 0;
	struct sflap_hdr hdr;
	char obuf[MSG_LEN];
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;

	if (tdt->state == STATE_PAUSE)
		/* TOC has given us the PAUSE message; sending could cause a disconnect
		 * so we just return here like everything went through fine */
		return 0;

	/* One _last_ 2048 check here!  This shouldn't ever
	   * get hit though, hopefully.  If it gets hit on an IM
	   * It'll lose the last " and the message won't go through,
	   * but this'll stop a segfault. */
	if (strlen(buf) > (MSG_LEN - sizeof(hdr))) {
		debug_printf("message too long, truncating\n");
		buf[MSG_LEN - sizeof(hdr) - 3] = '"';
		buf[MSG_LEN - sizeof(hdr) - 2] = '\0';
	}

	if (olen < 0)
		len = escape_message(buf);
	else
		len = olen;
	hdr.ast = '*';
	hdr.type = type;
	hdr.seqno = htons(tdt->seqno++ & 0xffff);
	hdr.len = htons(len + (type == TYPE_SIGNON ? 0 : 1));

	memcpy(obuf, &hdr, sizeof(hdr));
	slen += sizeof(hdr);
	memcpy(&obuf[slen], buf, len);
	slen += len;
	if (type != TYPE_SIGNON) {
		obuf[slen] = '\0';
		slen += 1;
	}

	return write(tdt->toc_fd, obuf, slen);
}

static int wait_reply(struct gaim_connection *gc, char *buffer, size_t buflen)
{
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	struct sflap_hdr *hdr;
	int ret;

	if (read(tdt->toc_fd, buffer, sizeof(struct sflap_hdr)) < 0) {
		debug_printf("error, couldn't read flap header\n");
		return -1;
	}

	hdr = (struct sflap_hdr *)buffer;

	if (buflen < ntohs(hdr->len)) {
		/* fake like there's a read error */
		debug_printf("buffer too small (have %d, need %d)\n", buflen, ntohs(hdr->len));
		return -1;
	}

	if (ntohs(hdr->len) > 0) {
		int count = 0;
		ret = 0;
		do {
			count += ret;
			ret = read(tdt->toc_fd,
				   buffer + sizeof(struct sflap_hdr) + count, ntohs(hdr->len) - count);
		} while (count + ret < ntohs(hdr->len) && ret > 0);
		buffer[sizeof(struct sflap_hdr) + count + ret] = '\0';
		return ret;
	} else
		return 0;
}

static unsigned char *roast_password(char *pass)
{
	/* Trivial "encryption" */
	static char rp[256];
	static char *roast = ROAST;
	int pos = 2;
	int x;
	strcpy(rp, "0x");
	for (x = 0; (x < 150) && pass[x]; x++)
		pos += sprintf(&rp[pos], "%02x", pass[x] ^ roast[x % strlen(roast)]);
	rp[pos] = '\0';
	return rp;
}

static void toc_got_info(gpointer data, char *url_text)
{
	if (!url_text)
		return;

	g_show_info_text(url_text);
}

static void toc_callback(gpointer data, gint source, GdkInputCondition condition)
{
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	struct sflap_hdr *hdr;
	struct signon so;
	char buf[8 * 1024], *c;
	char snd[BUF_LEN * 2];

	if (condition & GDK_INPUT_EXCEPTION) {
		debug_printf("gdk_input exception! check internet connection\n");
		hide_login_progress(gc, _("Connection Closed"));
		signoff(gc);
		return;
	}

	/* there's data waiting to be read, so read it. */
	if (wait_reply(gc, buf, 8 * 1024) <= 0) {
		hide_login_progress(gc, _("Connection Closed"));
		signoff(gc);
		return;
	}

	if (tdt->state == STATE_FLAPON) {
		hdr = (struct sflap_hdr *)buf;
		if (hdr->type != TYPE_SIGNON)
			debug_printf("problem, hdr->type != TYPE_SIGNON\n");
		else
			debug_printf("* TOC sends Client FLAP SIGNON\n");
		tdt->seqno = ntohs(hdr->seqno);
		tdt->state = STATE_SIGNON_REQUEST;

		debug_printf("* Client sends TOC FLAP SIGNON\n");
		g_snprintf(so.username, sizeof(so.username), "%s", gc->username);
		so.ver = htonl(1);
		so.tag = htons(1);
		so.namelen = htons(strlen(so.username));
		if (sflap_send(gc, (char *)&so, ntohs(so.namelen) + 8, TYPE_SIGNON) < 0) {
			hide_login_progress(gc, _("Disconnected."));
			signoff(gc);
			return;
		}

		debug_printf("* Client sends TOC \"toc_signon\" message\n");
		g_snprintf(snd, sizeof snd, "toc_signon %s %d  %s %s %s \"%s\"",
			   AUTH_HOST, AUTH_PORT, normalize(gc->username),
			   roast_password(gc->password), LANGUAGE, REVISION);
		if (sflap_send(gc, snd, -1, TYPE_DATA) < 0) {
			hide_login_progress(gc, _("Disconnected."));
			signoff(gc);
			return;
		}

		set_login_progress(gc, 3, _("Waiting for reply..."));
		return;
	}

	if (tdt->state == STATE_SIGNON_REQUEST) {
		debug_printf("* TOC sends client SIGN_ON reply\n");
		if (strncasecmp(buf + sizeof(struct sflap_hdr), "SIGN_ON", strlen("SIGN_ON"))) {
			debug_printf("Didn't get SIGN_ON! buf was: %s\n",
				     buf + sizeof(struct sflap_hdr));
			hide_login_progress(gc, _("Authentication Failed"));
			signoff(gc);
			return;
		}
		/* we're supposed to check that it's really TOC v1 here but we know it is ;) */
		debug_printf("TOC version: %s\n", buf + sizeof(struct sflap_hdr) + 4);

		/* we used to check for the CONFIG here, but we'll wait until we've sent our
		 * version of the config and then the toc_init_done message. we'll come back to
		 * the callback in a better state if we get CONFIG anyway */

		tdt->state = STATE_ONLINE;

		account_online(gc);
		serv_finish_login(gc);

		do_import(0, gc);

		/* Client sends TOC toc_init_done message */
		debug_printf("* Client sends TOC toc_init_done message\n");
		g_snprintf(snd, sizeof snd, "toc_init_done");
		sflap_send(gc, snd, -1, TYPE_DATA);

		/*
		g_snprintf(snd, sizeof snd, "toc_set_caps %s %s %s",
				FILE_SEND_UID, FILE_GET_UID, B_ICON_UID);
		*/
		g_snprintf(snd, sizeof snd, "toc_set_caps %s %s", FILE_SEND_UID, FILE_GET_UID);
		sflap_send(gc, snd, -1, TYPE_DATA);

		return;
	}

	debug_printf("From TOC server: %s\n", buf + sizeof(struct sflap_hdr));

	c = strtok(buf + sizeof(struct sflap_hdr), ":");	/* Ditch the first part */

	if (!strcasecmp(c, "SIGN_ON")) {
		/* we should only get here after a PAUSE */
		if (tdt->state != STATE_PAUSE)
			debug_printf("got SIGN_ON but not PAUSE!\n");
		else {
			tdt->state = STATE_ONLINE;
			g_snprintf(snd, sizeof snd, "toc_signon %s %d %s %s %s \"%s\"",
				   AUTH_HOST, AUTH_PORT, normalize(gc->username),
				   roast_password(gc->password), LANGUAGE, REVISION);
			if (sflap_send(gc, snd, -1, TYPE_DATA) < 0) {
				hide_login_progress(gc, _("Disconnected."));
				signoff(gc);
				return;
			}
			do_import(0, gc);
			g_snprintf(snd, sizeof snd, "toc_init_done");
			sflap_send(gc, snd, -1, TYPE_DATA);
			do_error_dialog(_("TOC has come back from its pause. You may now send"
					  " messages again."), _("TOC Resume"));
		}
	} else if (!strcasecmp(c, "CONFIG")) {
		c = strtok(NULL, ":");
		parse_toc_buddy_list(gc, c, 0);
	} else if (!strcasecmp(c, "NICK")) {
		/* ignore NICK so that things get imported/exported properly
		c = strtok(NULL, ":");
		g_snprintf(gc->username, sizeof(gc->username), "%s", c);
		*/
	} else if (!strcasecmp(c, "IM_IN")) {
		char *away, *message;
		int a = 0;

		c = strtok(NULL, ":");
		away = strtok(NULL, ":");

		message = away;
		while (*message && (*message != ':'))
			message++;
		message++;

		a = (away && (*away == 'T')) ? 1 : 0;

		serv_got_im(gc, c, message, a, time((time_t)NULL));
	} else if (!strcasecmp(c, "UPDATE_BUDDY")) {
		char *l, *uc;
		int logged, evil, idle, type = 0;
		time_t signon, time_idle;

		c = strtok(NULL, ":");	/* name */
		l = strtok(NULL, ":");	/* online */
		sscanf(strtok(NULL, ":"), "%d", &evil);
		sscanf(strtok(NULL, ":"), "%ld", &signon);
		sscanf(strtok(NULL, ":"), "%d", &idle);
		uc = strtok(NULL, ":");

		logged = (l && (*l == 'T')) ? 1 : 0;

		if (uc[0] == 'A')
			type |= UC_AOL;
		switch (uc[1]) {
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
		if (uc[2] == 'U')
			type |= UC_UNAVAILABLE;

		if (idle) {
			time(&time_idle);
			time_idle -= idle * 60;
		} else
			time_idle = 0;

		serv_got_update(gc, c, logged, evil, signon, time_idle, type, 0);
	} else if (!strcasecmp(c, "ERROR")) {
		c = strtok(NULL, ":");
		show_error_dialog(c);
	} else if (!strcasecmp(c, "EVILED")) {
		int lev;
		char *name;

		sscanf(strtok(NULL, ":"), "%d", &lev);
		name = strtok(NULL, ":");

		serv_got_eviled(gc, name, lev);
	} else if (!strcasecmp(c, "CHAT_JOIN")) {
		char *name;
		int id;

		sscanf(strtok(NULL, ":"), "%d", &id);
		name = strtok(NULL, ":");

		serv_got_joined_chat(gc, id, name);
	} else if (!strcasecmp(c, "CHAT_IN")) {
		int id, w;
		char *m, *who, *whisper;

		sscanf(strtok(NULL, ":"), "%d", &id);
		who = strtok(NULL, ":");
		whisper = strtok(NULL, ":");
		m = whisper;
		while (*m && (*m != ':'))
			m++;
		m++;

		w = (whisper && (*whisper == 'T')) ? 1 : 0;

		serv_got_chat_in(gc, id, who, w, m, time((time_t)NULL));
	} else if (!strcasecmp(c, "CHAT_UPDATE_BUDDY")) {
		int id;
		char *in, *buddy;
		GSList *bcs = gc->buddy_chats;
		struct conversation *b = NULL;

		sscanf(strtok(NULL, ":"), "%d", &id);
		in = strtok(NULL, ":");

		while (bcs) {
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;
			bcs = bcs->next;
			b = NULL;
		}

		if (!b)
			return;

		if (in && (*in == 'T'))
			while ((buddy = strtok(NULL, ":")) != NULL)
				add_chat_buddy(b, buddy);
		else
			while ((buddy = strtok(NULL, ":")) != NULL)
				remove_chat_buddy(b, buddy);
	} else if (!strcasecmp(c, "CHAT_INVITE")) {
		char *name, *who, *message;
		int id;

		name = strtok(NULL, ":");
		sscanf(strtok(NULL, ":"), "%d", &id);
		who = strtok(NULL, ":");
		message = strtok(NULL, ":");

		serv_got_chat_invite(gc, name, id, who, message);
	} else if (!strcasecmp(c, "CHAT_LEFT")) {
		GSList *bcs = gc->buddy_chats;
		struct conversation *b = NULL;
		int id;

		sscanf(strtok(NULL, ":"), "%d", &id);

		while (bcs) {
			b = (struct conversation *)bcs->data;
			if (id == b->id)
				break;
			b = NULL;
			bcs = bcs->next;
		}

		if (!b)
			return;

		if (b->window) {
			char error_buf[BUF_LONG];
			b->gc = NULL;
			g_snprintf(error_buf, sizeof error_buf, _("You have been disconnected"
								  " from chat room %s."), b->name);
			do_error_dialog(error_buf, _("Chat Error"));
		} else
			serv_got_chat_left(gc, id);
	} else if (!strcasecmp(c, "GOTO_URL")) {
		char *name, *url, tmp[256];

		name = strtok(NULL, ":");
		url = strtok(NULL, ":");

		g_snprintf(tmp, sizeof(tmp), "http://%s:%d/%s",
				gc->user->proto_opt[USEROPT_AUTH][0] ?
					gc->user->proto_opt[USEROPT_AUTH] : TOC_HOST,
				gc->user->proto_opt[USEROPT_AUTHPORT][0] ?
					atoi(gc->user->proto_opt[USEROPT_AUTHPORT]) : TOC_PORT,
				url);
		grab_url(tmp, toc_got_info, NULL);
	} else if (!strcasecmp(c, "DIR_STATUS")) {
	} else if (!strcasecmp(c, "ADMIN_NICK_STATUS")) {
	} else if (!strcasecmp(c, "ADMIN_PASSWD_STATUS")) {
		do_error_dialog(_("Password Change Successeful"), _("Gaim - Password Change"));
	} else if (!strcasecmp(c, "PAUSE")) {
		tdt->state = STATE_PAUSE;
		do_error_dialog(_("TOC has sent a PAUSE command. When this happens, TOC ignores"
				  " any messages sent to it, and may kick you off if you send a"
				  " message. Gaim will prevent anything from going through. This"
				  " is only temporary, please be patient."), _("TOC Pause"));
	} else if (!strcasecmp(c, "RVOUS_PROPOSE")) {
		char *user, *uuid, *cookie;
		int seq;
		char *rip, *pip, *vip;
		int port;

		user = strtok(NULL, ":");
		uuid = strtok(NULL, ":");
		cookie = strtok(NULL, ":");
		sscanf(strtok(NULL, ":"), "%d", &seq);
		rip = strtok(NULL, ":");
		pip = strtok(NULL, ":");
		vip = strtok(NULL, ":");
		sscanf(strtok(NULL, ":"), "%d", &port);

		if (!strcmp(uuid, FILE_SEND_UID)) {
			/* they want us to get a file */
			int unk[4], i;
			char *messages[4], *tmp, *name;
			int subtype, files, totalsize = 0;
			struct ft_request *ft;

			for (i = 0; i < 4; i++) {
				sscanf(strtok(NULL, ":"), "%d", &unk[i]);
				if (unk[i] == 10001)
					break;
				frombase64(strtok(NULL, ":"), &messages[i], NULL);
			}
			frombase64(strtok(NULL, ":"), &tmp, NULL);

			subtype = tmp[1];
			files = tmp[3];

			totalsize |= (tmp[4] << 24) & 0xff000000;
			totalsize |= (tmp[5] << 16) & 0x00ff0000;
			totalsize |= (tmp[6] << 8) & 0x0000ff00;
			totalsize |= (tmp[7] << 0) & 0x000000ff;

			if (!totalsize) {
				g_free(tmp);
				for (i--; i >= 0; i--)
					g_free(messages[i]);
				return;
			}

			name = tmp + 8;

			ft = g_new0(struct ft_request, 1);
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
			ft->files = files;
			g_snprintf(ft->UID, sizeof(ft->UID), "%s", FILE_SEND_UID);
			ft->gc = gc;

			g_free(tmp);
			for (i--; i >= 0; i--)
				g_free(messages[i]);

			debug_printf("English translation of RVOUS_PROPOSE: %s requests Send File (i.e."
				" send a file to you); %s:%d (verified_ip:port), %d files at"
				" total size of %ld bytes.\n", user, vip, port, files, totalsize);
			accept_file_dialog(ft);
		} else if (!strcmp(uuid, FILE_GET_UID)) {
			/* they want us to send a file */
			int unk[4], i;
			char *messages[4], *tmp;
			struct ft_request *ft;

			for (i = 0; i < 4; i++) {
				sscanf(strtok(NULL, ":"), "%d", unk + i);
				if (unk[i] == 10001)
					break;
				frombase64(strtok(NULL, ":"), &messages[i], NULL);
			}
			frombase64(strtok(NULL, ":"), &tmp, NULL);

			ft = g_new0(struct ft_request, 1);
			ft->cookie = g_strdup(cookie);
			ft->ip = g_strdup(pip);
			ft->port = port;
			if (i)
				ft->message = g_strdup(messages[0]);
			else
				ft->message = NULL;
			ft->user = g_strdup(user);
			g_snprintf(ft->UID, sizeof(ft->UID), "%s", FILE_GET_UID);
			ft->gc = gc;

			g_free(tmp);
			for (i--; i >= 0; i--)
				g_free(messages[i]);

			accept_file_dialog(ft);
		} else if (!strcmp(uuid, VOICE_UID)) {
			/* oh goody. voice over ip. fun stuff. */
		} else if (!strcmp(uuid, B_ICON_UID)) {
			/*
			int unk[4], i;
			char *messages[4];
			struct buddy_icon *icon;

			for (i = 0; i < 4; i++) {
				sscanf(strtok(NULL, ":"), "%d", unk + i);
				if (unk[i] == 10001)
					break;
				frombase64(strtok(NULL, ":"), &messages[i], NULL);
			}
			frombase64(strtok(NULL, ":"), (char **)&icon, NULL);

			debug_printf("received icon of length %d\n", icon->len);
			g_free(icon);
			for (i--; i >= 0; i--)
				g_free(messages[i]);
			*/
		} else if (!strcmp(uuid, IMAGE_UID)) {
			/* aka Direct IM */
		} else {
			debug_printf("Don't know what to do with RVOUS UUID %s\n", uuid);
			/* do we have to do anything here? i think it just times out */
		}
	} else {
		debug_printf("don't know what to do with %s\n", c);
	}
}

static char *toc_name()
{
	return "TOC";
}

static void toc_send_im(struct gaim_connection *gc, char *name, char *message, int away)
{
	char buf[BUF_LEN * 2];

	escape_text(message);
	g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
		   message, ((away) ? " auto" : ""));
	sflap_send(gc, buf, -1, TYPE_DATA);
}

static void toc_set_config(struct gaim_connection *gc)
{
	char buf[MSG_LEN], snd[BUF_LEN * 2];
	toc_build_config(gc, buf, MSG_LEN, FALSE);
	g_snprintf(snd, MSG_LEN, "toc_set_config {%s}", buf);
	sflap_send(gc, snd, -1, TYPE_DATA);
}

static void toc_get_info(struct gaim_connection *g, char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_get_dir(struct gaim_connection *g, char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_dir(struct gaim_connection *g, char *first, char *middle, char *last,
			char *maiden, char *city, char *state, char *country, int web)
{
	char buf2[BUF_LEN * 4], buf[BUF_LEN];
	g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
		   middle, last, maiden, city, state, country, (web == 1) ? "Y" : "");
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_dir_search(struct gaim_connection *g, char *first, char *middle, char *last,
			   char *maiden, char *city, char *state, char *country, char *email)
{
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf) / 2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle,
		   last, maiden, city, state, country, email);
	debug_printf("Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden,
		     city, state, country);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_away(struct gaim_connection *g, char *state, char *message)
{
	char buf[BUF_LEN * 2];
	if (g->away)
		g_free (g->away);
	g->away = NULL;
	if (message) {
		g->away = g_strdup (message);
		escape_text(message);
		g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", message);
	} else
		g_snprintf(buf, MSG_LEN, "toc_set_away \"\"");
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_info(struct gaim_connection *g, char *info)
{
	char buf[BUF_LEN * 2], buf2[BUF_LEN * 2];
	g_snprintf(buf2, sizeof buf2, "%s", info);
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", buf2);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_change_passwd(struct gaim_connection *g, char *orig, char *new)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new);
	sflap_send(g, buf, strlen(buf), TYPE_DATA);
}

static void toc_add_buddy(struct gaim_connection *g, char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
	toc_set_config(g);
}

static void toc_add_buddies(struct gaim_connection *g, GList * buddies)
{
	char buf[BUF_LEN * 2];
	int n;

	n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	while (buddies) {
		if (strlen(normalize(buddies->data)) > MSG_LEN - n - 16) {
			sflap_send(g, buf, -1, TYPE_DATA);
			n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
		buddies = buddies->next;
	}
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_remove_buddy(struct gaim_connection *g, char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
	toc_set_config(g);
}

static void toc_set_idle(struct gaim_connection *g, int time)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_warn(struct gaim_connection *g, char *name, int anon)
{
	char send[BUF_LEN * 2];
	g_snprintf(send, 255, "toc_evil %s %s", name, ((anon) ? "anon" : "norm"));
	sflap_send(g, send, -1, TYPE_DATA);
}

static void toc_accept_chat(struct gaim_connection *g, int i)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, 255, "toc_chat_accept %d", i);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_join_chat(struct gaim_connection *g, int exchange, char *name)
{
	char buf[BUF_LONG];
	if (!name) {
		const char *nm;
		if (!join_chat_entry || !join_chat_spin)
			return;
		nm = gtk_entry_get_text(GTK_ENTRY(join_chat_entry));
		exchange = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(join_chat_spin));
		if (!name || !strlen(name))
			return;
		g_snprintf(buf, sizeof(buf) / 2, "toc_chat_join %d \"%s\"", exchange, nm);
	} else
		g_snprintf(buf, sizeof(buf) / 2, "toc_chat_join %d \"%s\"", exchange, name);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_invite(struct gaim_connection *g, int id, char *message, char *name)
{
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf) / 2, "toc_chat_invite %d \"%s\" %s", id, message, normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_leave(struct gaim_connection *g, int id)
{
	GSList *bcs = g->buddy_chats;
	struct conversation *b = NULL;
	char buf[BUF_LEN * 2];

	while (bcs) {
		b = (struct conversation *)bcs->data;
		if (id == b->id)
			break;
		b = NULL;
		bcs = bcs->next;
	}

	if (!b)
		return;		/* can this happen? */

	if (!b->gc)		/* TOC already kicked us out of this room */
		serv_got_chat_left(g, id);
	else {
		g_snprintf(buf, 255, "toc_chat_leave %d", id);
		sflap_send(g, buf, -1, TYPE_DATA);
	}
}

static void toc_chat_whisper(struct gaim_connection *g, int id, char *who, char *message)
{
	char buf2[BUF_LEN * 2];
	g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, who, message);
	sflap_send(g, buf2, -1, TYPE_DATA);
}

static void toc_chat_send(struct gaim_connection *g, int id, char *message)
{
	char buf[BUF_LEN * 2];
	escape_text(message);
	g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"", id, message);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_keepalive(struct gaim_connection *gc)
{
	sflap_send(gc, "", 0, TYPE_KEEPALIVE);
}

static char **toc_list_icon(int uc)
{
	if (uc & UC_UNAVAILABLE)
		return (char **)away_icon_xpm;
	if (uc & UC_AOL)
		return (char **)aol_icon_xpm;
	if (uc & UC_NORMAL)
		return (char **)free_icon_xpm;
	if (uc & UC_ADMIN)
		return (char **)admin_icon_xpm;
	if (uc & UC_UNCONFIRMED)
		return (char **)dt_icon_xpm;
	return NULL;
}

static void toc_info(GtkObject * obj, char *who)
{
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_info(gc, who);
}

static void toc_dir_info(GtkObject * obj, char *who)
{
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_dir(gc, who);
}

static void des_jc()
{
	join_chat_entry = NULL;
	join_chat_spin = NULL;
}

static void toc_draw_join_chat(struct gaim_connection *gc, GtkWidget *fbox) {
	GtkWidget *label;
	GtkWidget *rowbox;
	GtkObject *adjust;

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);
	gtk_widget_show(rowbox);

	label = gtk_label_new(_("Join what group:"));
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(label), "destroy", GTK_SIGNAL_FUNC(des_jc), NULL);
	gtk_widget_show(label);

	join_chat_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(rowbox), join_chat_entry, TRUE, TRUE, 0);
	gtk_widget_grab_focus(join_chat_entry);
	gtk_signal_connect(GTK_OBJECT(join_chat_entry), "activate", GTK_SIGNAL_FUNC(do_join_chat), NULL);
	gtk_widget_show(join_chat_entry);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), rowbox, TRUE, TRUE, 0);
	gtk_widget_show(rowbox);

	label = gtk_label_new(_("Community:"));
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	
	adjust = gtk_adjustment_new(4, 4, 20, 1, 10, 10);
	join_chat_spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_widget_set_usize(join_chat_spin, 50, -1);
	gtk_box_pack_start(GTK_BOX(rowbox), join_chat_spin, FALSE, FALSE, 0);
	gtk_widget_show(join_chat_spin);
}

static void toc_buddy_menu(GtkWidget *menu, struct gaim_connection *gc, char *who)
{
	GtkWidget *button;

	button = gtk_menu_item_new_with_label(_("Get Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(toc_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Get Dir Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate", GTK_SIGNAL_FUNC(toc_dir_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static void toc_print_option(GtkEntry *entry, struct aim_user *user)
{
	int entrynum;

	entrynum = (int)gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_AUTH) {
		g_snprintf(user->proto_opt[USEROPT_AUTH],
			   sizeof(user->proto_opt[USEROPT_AUTH]), "%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_AUTHPORT) {
		g_snprintf(user->proto_opt[USEROPT_AUTHPORT],
			   sizeof(user->proto_opt[USEROPT_AUTHPORT]), "%s", gtk_entry_get_text(entry));
	}
}

static void toc_user_opts(GtkWidget *book, struct aim_user *user)
{
	/* so here, we create the new notebook page */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, gtk_label_new("TOC Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("TOC Host:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_AUTH);
	gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_AUTH][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTH]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTH]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "toc.oscar.aol.com");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("TOC Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_AUTHPORT);
	gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_AUTHPORT][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTHPORT]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTHPORT]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "9898");

	gtk_widget_show(entry);
}

static void toc_add_permit(struct gaim_connection *gc, char *who)
{
	char buf2[BUF_LEN * 2];
	if (gc->permdeny != 3)
		return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_permit %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
	toc_set_config(gc);
}

static void toc_add_deny(struct gaim_connection *gc, char *who)
{
	char buf2[BUF_LEN * 2];
	if (gc->permdeny != 4)
		return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_deny %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
	toc_set_config(gc);
}

static void toc_set_permit_deny(struct gaim_connection *gc)
{
	char buf2[BUF_LEN * 2];
	GSList *list;
	int at;

	switch (gc->permdeny) {
	case 1:
		/* permit all, deny none. to get here reliably we need to have been in permit
		 * mode, and send an empty toc_add_deny message, which will switch us to deny none */
		g_snprintf(buf2, sizeof(buf2), "toc_add_permit ");
		sflap_send(gc, buf2, -1, TYPE_DATA);
		g_snprintf(buf2, sizeof(buf2), "toc_add_deny ");
		sflap_send(gc, buf2, -1, TYPE_DATA);
		break;
	case 2:
		/* deny all, permit none. to get here reliably we need to have been in deny
		 * mode, and send an empty toc_add_permit message, which will switch us to permit none */
		g_snprintf(buf2, sizeof(buf2), "toc_add_deny ");
		sflap_send(gc, buf2, -1, TYPE_DATA);
		g_snprintf(buf2, sizeof(buf2), "toc_add_permit ");
		sflap_send(gc, buf2, -1, TYPE_DATA);
		break;
	case 3:
		/* permit some. we want to switch to deny mode first, then send the toc_add_permit
		 * message, which will clear and set our permit list. toc sucks. */
		g_snprintf(buf2, sizeof(buf2), "toc_add_deny ");
		sflap_send(gc, buf2, -1, TYPE_DATA);

		at = g_snprintf(buf2, sizeof(buf2), "toc_add_permit ");
		list = gc->permit;
		while (list) {
			at += g_snprintf(buf2 + at, sizeof(buf2) - at, "%s ", normalize(list->data));
			if (at > MSG_LEN + 32) {	/* from out my ass comes greatness */
				sflap_send(gc, buf2, -1, TYPE_DATA);
				at = g_snprintf(buf2, sizeof(buf2), "toc_add_permit ");
			}
			list = list->next;
		}
		sflap_send(gc, buf2, -1, TYPE_DATA);
		break;
	case 4:
		/* deny some. we want to switch to permit mode first, then send the toc_add_deny
		 * message, which will clear and set our deny list. toc sucks. */
		g_snprintf(buf2, sizeof(buf2), "toc_add_permit ");
		sflap_send(gc, buf2, -1, TYPE_DATA);

		at = g_snprintf(buf2, sizeof(buf2), "toc_add_deny ");
		list = gc->deny;
		while (list) {
			at += g_snprintf(buf2 + at, sizeof(buf2) - at, "%s ", normalize(list->data));
			if (at > MSG_LEN + 32) {	/* from out my ass comes greatness */
				sflap_send(gc, buf2, -1, TYPE_DATA);
				at = g_snprintf(buf2, sizeof(buf2), "toc_add_deny ");
			}
			list = list->next;
		}
		sflap_send(gc, buf2, -1, TYPE_DATA);
		break;
	default:
		break;
	}
	toc_set_config(gc);
}

static void toc_rem_permit(struct gaim_connection *gc, char *who)
{
	if (gc->permdeny != 3)
		return;
	toc_set_permit_deny(gc);
}

static void toc_rem_deny(struct gaim_connection *gc, char *who)
{
	if (gc->permdeny != 4)
		return;
	toc_set_permit_deny(gc);
}

static void toc_draw_new_user(GtkWidget *box)
{
	GtkWidget *label;

	label = gtk_label_new(_("Unfortunately, currently TOC only allows new user registration by "
				"going to http://aim.aol.com/aimnew/Aim/register.adp?promo=106723&pageset=Aim&client=no"
				". Clicking the Register button will open the URL for you."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
}

static void toc_do_new_user()
{
	open_url(NULL, "http://aim.aol.com/aimnew/Aim/register.adp?promo=106723&pageset=Aim&client=no");
}

static GList *toc_away_states()
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void toc_do_action(struct gaim_connection *gc, char *act)
{
	if (!strcmp(act, "Set User Info")) {
		show_set_info(gc);
	} else if (!strcmp(act, "Set Dir Info")) {
		show_set_dir(gc);
	} else if (!strcmp(act, "Change Password")) {
		show_change_passwd(gc);
	}
}

static GList *toc_actions()
{
	GList *m = NULL;

	m = g_list_append(m, "Set User Info");
	m = g_list_append(m, "Set Dir Info");
	m = g_list_append(m, "Change Password");

	return m;
}

void toc_init(struct prpl *ret)
{
	ret->protocol = PROTO_TOC;
	ret->options = OPT_PROTO_HTML | OPT_PROTO_CORRECT_TIME;
	ret->name = toc_name;
	ret->list_icon = toc_list_icon;
	ret->away_states = toc_away_states;
	ret->actions = toc_actions;
	ret->do_action = toc_do_action;
	ret->buddy_menu = toc_buddy_menu;
	ret->user_opts = toc_user_opts;
	ret->draw_new_user = toc_draw_new_user;
	ret->do_new_user = toc_do_new_user;
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
	ret->add_permit = toc_add_permit;
	ret->add_deny = toc_add_deny;
	ret->rem_permit = toc_rem_permit;
	ret->rem_deny = toc_rem_deny;
	ret->set_permit_deny = toc_set_permit_deny;
	ret->warn = toc_warn;
	ret->draw_join_chat = toc_draw_join_chat;
	ret->accept_chat = toc_accept_chat;
	ret->join_chat = toc_join_chat;
	ret->chat_invite = toc_chat_invite;
	ret->chat_leave = toc_chat_leave;
	ret->chat_whisper = toc_chat_whisper;
	ret->chat_send = toc_chat_send;
	ret->keepalive = toc_keepalive;
}

/*********
 * RVOUS ACTIONS
 ********/

struct file_header {
	char  magic[4];		/* 0 */
	short hdrlen;		/* 4 */
	short hdrtype;		/* 6 */
	char  bcookie[8];	/* 8 */
	short encrypt;		/* 16 */
	short compress;		/* 18 */
	short totfiles;		/* 20 */
	short filesleft;	/* 22 */
	short totparts;		/* 24 */
	short partsleft;	/* 26 */
	long  totsize;		/* 28 */
	long  size;		/* 32 */
	long  modtime;		/* 36 */
	long  checksum;		/* 40 */
	long  rfrcsum;		/* 44 */
	long  rfsize;		/* 48 */
	long  cretime;		/* 52 */
	long  rfcsum;		/* 56 */
	long  nrecvd;		/* 60 */
	long  recvcsum;		/* 64 */
	char  idstring[32];	/* 68 */
	char  flags;		/* 100 */
	char  lnameoffset;	/* 101 */
	char  lsizeoffset;	/* 102 */
	char  dummy[69];	/* 103 */
	char  macfileinfo[16];	/* 172 */
	short nencode;		/* 188 */
	short nlanguage;	/* 190 */
	char  name[64];		/* 192 */
				/* 256 */
};

struct file_transfer {
	struct file_header hdr;

	struct gaim_connection *gc;

	char *user;
	char *cookie;
	char *ip;
	int port;
	long size;
	struct stat st;

	GtkWidget *window;
	int files;
	char *filename;
	FILE *file;
	int recvsize;

	gint inpa;
};

static void debug_header(struct file_transfer *ft) {
	struct file_header *f = (struct file_header *)ft;
	debug_printf("TOC FT HEADER:\n"
			"\t%s %d 0x%04x\n"
			"\t%s %d %d\n"
			"\t%d %d %d %d %ld %ld\n"
			"\t%ld %ld %ld %ld %ld %ld %ld %ld\n"
			"\t%s\n"
			"\t0x%02x, 0x%02x, 0x%02x\n"
			"\t%s %s\n"
			"\t%d %d\n"
			"\t%s\n",
			f->magic, ntohs(f->hdrlen), f->hdrtype,
			f->bcookie, ntohs(f->encrypt), ntohs(f->compress),
			ntohs(f->totfiles), ntohs(f->filesleft), ntohs(f->totparts),
				ntohs(f->partsleft), ntohl(f->totsize), ntohl(f->size),
			ntohl(f->modtime), ntohl(f->checksum), ntohl(f->rfrcsum), ntohl(f->rfsize),
				ntohl(f->cretime), ntohl(f->rfcsum), ntohl(f->nrecvd),
				ntohl(f->recvcsum),
			f->idstring,
			f->flags, f->lnameoffset, f->lsizeoffset,
			f->dummy, f->macfileinfo,
			ntohs(f->nencode), ntohs(f->nlanguage),
			f->name);
}

static void toc_send_file_callback(gpointer data, gint source, GdkInputCondition cond)
{
	char buf[BUF_LONG];
	int rt, i;

	struct file_transfer *ft = data;

	if (cond & GDK_INPUT_EXCEPTION) {
		gdk_input_remove(ft->inpa);
		close(source);
		g_free(ft->filename);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft->cookie);
		if (ft->file)
			fclose(ft->file);
		g_free(ft);
		return;
	}

	if (ft->hdr.hdrtype != 0x202) {
		char *buf;
		frombase64(ft->cookie, &buf, NULL);

		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		ft->hdr.hdrtype = 0x202;
		memcpy(ft->hdr.bcookie, buf, 8);
		g_free(buf);
		ft->hdr.encrypt = 0; ft->hdr.compress = 0;
		debug_header(ft);
		write(source, ft, 256);

		if (ft->files == 1) {
			ft->file = fopen(ft->filename, "w");
			if (!ft->file) {
				buf = g_strdup_printf("Could not open %s for writing!", ft->filename);
				do_error_dialog(buf, _("Error"));
				g_free(buf);
				gdk_input_remove(ft->inpa);
				close(source);
				g_free(ft->filename);
				g_free(ft->user);
				g_free(ft->ip);
				g_free(ft->cookie);
				g_free(ft);
			}
		} else {
			buf = g_strdup_printf("%s/%s", ft->filename, ft->hdr.name);
			ft->file = fopen(buf, "w");
			g_free(buf);
			if (!ft->file) {
				buf = g_strdup_printf("Could not open %s/%s for writing!", ft->filename,
						ft->hdr.name);
				do_error_dialog(buf, _("Error"));
				g_free(buf);
				gdk_input_remove(ft->inpa);
				close(source);
				g_free(ft->filename);
				g_free(ft->user);
				g_free(ft->ip);
				g_free(ft->cookie);
				g_free(ft);
			}
		}

		return;
	}

	rt = read(source, buf, MIN(ntohl(ft->hdr.size) - ft->recvsize, 1024));
	if (rt < 0) {
		do_error_dialog("File transfer failed; other side probably canceled.", "Error");
		gdk_input_remove(ft->inpa);
		close(source);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft->cookie);
		if (ft->file)
			fclose(ft->file);
		g_free(ft);
		return;
	}
	ft->recvsize += rt;
	for (i = 0; i < rt; i++)
		fprintf(ft->file, "%c", buf[i]);

	if (ft->recvsize == ntohl(ft->hdr.size)) {
		ft->hdr.hdrtype = htons(0x0204);
		ft->hdr.filesleft = htons(ntohs(ft->hdr.filesleft) - 1);
		ft->hdr.partsleft = htons(ntohs(ft->hdr.partsleft) - 1);
		ft->hdr.recvcsum = ft->hdr.checksum; /* uh... */
		ft->hdr.nrecvd = htons(ntohs(ft->hdr.nrecvd) + 1);
		ft->hdr.flags = 0;
		write(source, ft, 256);
		debug_header(ft);
		ft->recvsize = 0;
		fclose(ft->file);
		if (ft->hdr.filesleft == 0) {
			gdk_input_remove(ft->inpa);
			close(source);
			g_free(ft->filename);
			g_free(ft->user);
			g_free(ft->ip);
			g_free(ft->cookie);
			g_free(ft);
		}
	}
}

static void toc_send_file_connect(gpointer data, gint src, GdkInputCondition cond)
{
	struct file_transfer *ft = data;

	if (src == -1) {
		do_error_dialog(_("Could not connect for transfer!"), _("Error"));
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}

	ft->inpa = gdk_input_add(src, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, toc_send_file_callback, ft);
}

static void toc_send_file(gpointer a, struct file_transfer *old_ft)
{
	struct file_transfer *ft;
	const char *dirname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(old_ft->window));
	int fd;
	struct aim_user *user;
	char buf[BUF_LEN * 2];

	if (file_is_dir(dirname, old_ft->window))
		return;
	ft = g_new0(struct file_transfer, 1);
	if (old_ft->files == 1)
		ft->filename = g_strdup(dirname);
	else
		ft->filename = g_dirname(dirname);
	ft->cookie = g_strdup(old_ft->cookie);
	ft->user = g_strdup(old_ft->user);
	ft->ip = g_strdup(old_ft->ip);
	ft->files = old_ft->files;
	ft->port = old_ft->port;
	ft->gc = old_ft->gc;
	user = ft->gc->user;
	gtk_widget_destroy(old_ft->window);

	g_snprintf(buf, sizeof(buf), "toc_rvous_accept %s %s %s", ft->user, ft->cookie, FILE_SEND_UID);
	sflap_send(ft->gc, buf, -1, TYPE_DATA);

	fd =
	    proxy_connect(ft->ip, ft->port, toc_send_file_connect, ft);
	if (fd < 0) {
		do_error_dialog(_("Could not connect for transfer!"), _("Error"));
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}
}

static void toc_get_file_callback(gpointer data, gint source, GdkInputCondition cond)
{
	char buf[BUF_LONG];

	struct file_transfer *ft = data;

	if (cond & GDK_INPUT_EXCEPTION) {
		do_error_dialog("The file tranfer has been aborted; the other side most likely"
				" cancelled.", "Error");
		gdk_input_remove(ft->inpa);
		close(source);
		g_free(ft->filename);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft->cookie);
		if (ft->file)
			fclose(ft->file);
		g_free(ft);
		return;
	}

	if (cond & GDK_INPUT_WRITE) {
		int remain = MIN(ntohl(ft->hdr.totsize) - ft->recvsize, 1024);
		int i;
		for (i = 0; i < remain; i++)
			fscanf(ft->file, "%c", &buf[i]);
		write(source, buf, remain);
		ft->recvsize += remain;
		if (ft->recvsize == ntohl(ft->hdr.totsize)) {
			gdk_input_remove(ft->inpa);
			ft->inpa = gdk_input_add(source, GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
						 toc_get_file_callback, ft);
		}
		return;
	}

	if (ft->hdr.hdrtype == htons(0x1108)) {
		struct tm *fortime;
		struct stat st;

		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		stat(ft->filename, &st);
		fortime = localtime(&st.st_mtime);
		g_snprintf(buf, sizeof(buf), "%2d/%2d/%4d %2d:%2d %8ld %s\r\n",
				fortime->tm_mon + 1, fortime->tm_mday, fortime->tm_year + 1900,
				fortime->tm_hour + 1, fortime->tm_min + 1, (long)st.st_size,
				g_basename(ft->filename));
		write(source, buf, ntohl(ft->hdr.size));
		return;
	}

	if (ft->hdr.hdrtype == htons(0x1209)) {
		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);
		return;
	}

	if (ft->hdr.hdrtype == htons(0x120b)) {
		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		if (ft->hdr.hdrtype != htons(0x120c)) {
			g_snprintf(buf, sizeof(buf), "%s decided to cancel the transfer", ft->user);
			do_error_dialog(buf, "Error");
			gdk_input_remove(ft->inpa);
			close(source);
			g_free(ft->filename);
			g_free(ft->user);
			g_free(ft->ip);
			g_free(ft->cookie);
			if (ft->file)
				fclose(ft->file);
			g_free(ft);
			return;
		}

		ft->hdr.hdrtype = 0x0101;
		ft->hdr.totfiles = htons(1); ft->hdr.filesleft = htons(1);
		ft->hdr.flags = 0x20;
		write(source, ft, 256);
		return;
	}

	if (ft->hdr.hdrtype == 0x0101) {
		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		gdk_input_remove(ft->inpa);
		ft->inpa = gdk_input_add(source, GDK_INPUT_WRITE | GDK_INPUT_EXCEPTION,
					 toc_get_file_callback, ft);
		return;
	}

	if (ft->hdr.hdrtype == 0x0202) {
		read(source, ft, 8);
		read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		gdk_input_remove(ft->inpa);
		close(source);
		g_free(ft->filename);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft->cookie);
		if (ft->file)
			fclose(ft->file);
		g_free(ft);
		return;
	}
}

static void toc_get_file_connect(gpointer data, gint src, GdkInputCondition cond)
{
	struct file_transfer *ft = data;
	struct file_header *hdr;
	char *buf;

	if (src == -1) {
		do_error_dialog(_("Could not connect for transfer!"), _("Error"));
		fclose(ft->file);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}

	hdr = (struct file_header *)ft;
	hdr->magic[0] = 'O'; hdr->magic[1] = 'F'; hdr->magic[2] = 'T'; hdr->magic[3] = '2';
	hdr->hdrlen = htons(256);
	hdr->hdrtype = htons(0x1108);
	frombase64(ft->cookie, &buf, NULL);
	g_snprintf(hdr->bcookie, 8, "%s", buf);
	g_free(buf);
	hdr->totfiles = htons(1); hdr->filesleft = htons(1);
	hdr->totparts = htons(1); hdr->partsleft = htons(1);
	hdr->totsize = htonl((long)ft->st.st_size); /* combined size of all files */
	/* size = strlen("mm/dd/yyyy hh:mm sizesize 'name'\r\n") */
	hdr->size = htonl(28 + strlen(g_basename(ft->filename))); /* size of listing.txt */
	hdr->modtime = htonl(ft->st.st_mtime);
	hdr->checksum = htonl(0x89f70000); /* uh... */
	g_snprintf(hdr->idstring, 32, "OFT_Windows ICBMFT V1.1 32");
	hdr->flags = 0x02;
	hdr->lnameoffset = 0x1A;
	hdr->lsizeoffset = 0x10;
	g_snprintf(hdr->name, 64, "listing.txt");
	if (write(src, hdr, 256) < 0) {
		do_error_dialog(_("Could not write file header!"), _("Error"));
		fclose(ft->file);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}

	ft->inpa = gdk_input_add(src, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, toc_get_file_callback, ft);
}

static void toc_get_file(gpointer a, struct file_transfer *old_ft)
{
	struct file_transfer *ft;
	const char *dirname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(old_ft->window));
	int fd;
	struct aim_user *user;
	char *buf, buf2[BUF_LEN * 2];

	if (file_is_dir(dirname, old_ft->window))
		return;
	ft = g_new0(struct file_transfer, 1);
	ft->filename = g_strdup(dirname);
	ft->file = fopen(ft->filename, "r");
	if (!ft->file) {
		buf = g_strdup_printf("Unable to open %s for transfer!", ft->filename);
		do_error_dialog(buf, "Error");
		g_free(buf);
		g_free(ft->filename);
		g_free(ft);
		return;
	}
	if (stat(dirname, &ft->st)) {
		buf = g_strdup_printf("Unable to examine %s!", dirname);
		do_error_dialog(buf, "Error");
		g_free(buf);
		g_free(ft->filename);
		g_free(ft);
		return;
	}
	ft->cookie = g_strdup(old_ft->cookie);
	ft->user = g_strdup(old_ft->user);
	ft->ip = g_strdup(old_ft->ip);
	ft->port = old_ft->port;
	ft->gc = old_ft->gc;
	user = ft->gc->user;
	gtk_widget_destroy(old_ft->window);

	g_snprintf(buf2, sizeof(buf2), "toc_rvous_accept %s %s %s", ft->user, ft->cookie, FILE_GET_UID);
	sflap_send(ft->gc, buf2, -1, TYPE_DATA);

	fd =
	    proxy_connect(ft->ip, ft->port, toc_get_file_connect, ft);
	if (fd < 0) {
		do_error_dialog(_("Could not connect for transfer!"), _("Error"));
		fclose(ft->file);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}
}

static void cancel_callback(gpointer a, struct file_transfer *ft) {
	gtk_widget_destroy(ft->window);
	if (a == ft->window) {
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
	}
}

static void toc_accept_ft(gpointer a, struct ft_request *fr) {
	GtkWidget *window;
	char buf[BUF_LEN];

	struct file_transfer *ft = g_new0(struct file_transfer, 1);
	ft->gc = fr->gc;
	ft->user = g_strdup(fr->user);
	ft->cookie = g_strdup(fr->cookie);
	ft->ip = g_strdup(fr->ip);
	ft->port = fr->port;
	ft->files = fr->files;

	ft->window = window = gtk_file_selection_new(_("Gaim - Save As..."));
	g_snprintf(buf, sizeof(buf), "%s/%s", g_get_home_dir(), fr->filename ? fr->filename : "");
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(ft->window)->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);

	if (!strcmp(fr->UID, FILE_SEND_UID))
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(toc_send_file), ft);
	else
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(toc_get_file), ft);

	gtk_widget_show(window);
}

static void toc_reject_ft(gpointer a, struct ft_request *ft) {
	g_free(ft->user);
	g_free(ft->filename);
	g_free(ft->ip);
	g_free(ft->cookie);
	if (ft->message)
		g_free(ft->message);
	g_free(ft);
}

static void accept_file_dialog(struct ft_request *ft) {
	char buf[BUF_LONG];
	if (!strcmp(ft->UID, FILE_SEND_UID)) {
		/* holy crap. who the fuck would transfer gigabytes through AIM?! */
		static char *sizes[4] = { "bytes", "KB", "MB", "GB" };
		float size = ft->size;
		int index = 0;
		while ((index < 4) && (size > 1024)) {
			size /= 1024;
			index++;
		}
	        g_snprintf(buf, sizeof(buf), _("%s requests %s to accept %d file%s: %s (%.2f %s)%s%s"),
				ft->user, ft->gc->username, ft->files, (ft->files == 1) ? "" : "s",
				ft->filename, size, sizes[index], (ft->message) ? "\n" : "",
				(ft->message) ? ft->message : "");
	} else {
		g_snprintf(buf, sizeof(buf), _("%s requests you to send them a file"), ft->user);
	}
	do_ask_dialog(buf, ft, toc_accept_ft, toc_reject_ft);
}
