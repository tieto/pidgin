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
#include "proxy.h"

#include "pixmaps/admin_icon.xpm"
#include "pixmaps/aol_icon.xpm"
#include "pixmaps/away_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/free_icon.xpm"

#define REVISION "gaim:$Revision: 1156 $"

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
#define USEROPT_SOCKSHOST 2
#define USEROPT_SOCKSPORT 3
#define USEROPT_PROXYTYPE 4

static void toc_callback(gpointer, gint, GdkInputCondition);
static unsigned char *roast_password(char *);

/* ok. this function used to take username/password, and return 0 on success.
 * now, it takes username/password, and returns NULL on error or a new gaim_connection
 * on success. */
static void toc_login(struct aim_user *user) {
	struct gaim_connection *gc;
	struct toc_data *tdt;
	char buf[80];

	gc = new_gaim_conn(user);
	gc->proto_data = tdt = g_new0(struct toc_data, 1);

	g_snprintf(buf, sizeof buf, "Lookin up %s",
			user->proto_opt[USEROPT_AUTH][0] ? user->proto_opt[USEROPT_AUTH] : TOC_HOST);
	/* this is such a hack */
	set_login_progress(gc, 1, buf);
	while (gtk_events_pending())
		gtk_main_iteration();
	if (!g_slist_find(connections, gc))
		return;

	tdt->toc_fd = proxy_connect(
		user->proto_opt[USEROPT_AUTH][0] ? user->proto_opt[USEROPT_AUTH] : TOC_HOST,
		user->proto_opt[USEROPT_AUTHPORT][0] ? atoi(user->proto_opt[USEROPT_AUTHPORT]):TOC_PORT,
		user->proto_opt[USEROPT_SOCKSHOST], atoi(user->proto_opt[USEROPT_SOCKSPORT]),
		atoi(user->proto_opt[USEROPT_PROXYTYPE]));

	debug_printf("* Client connects to TOC\n");
	if (tdt->toc_fd < 0) {
		g_snprintf(buf, sizeof(buf), "Connect to %s failed",
				user->proto_opt[USEROPT_AUTH]);
		hide_login_progress(gc, buf);
		signoff(gc);
		return;
	}

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

static void toc_close(struct gaim_connection *gc) {
	if (gc->inpa > 0)
		gdk_input_remove(gc->inpa);
	gc->inpa = -1;
	close(((struct toc_data *)gc->proto_data)->toc_fd);
}

int sflap_send(struct gaim_connection *gc, char *buf, int olen, int type) {
	int len;
	int slen=0;
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
		obuf[slen]='\0';
		slen += 1;
	}

	return write(tdt->toc_fd, obuf, slen);
}

static int wait_reply(struct gaim_connection *gc, char *buffer, size_t buflen) {
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
		ret = read(tdt->toc_fd, buffer + sizeof(struct sflap_hdr), ntohs(hdr->len));
		buffer[sizeof(struct sflap_hdr) + ret] = '\0';
		return ret;
	} else return 0;
}

static unsigned char *roast_password(char *pass) {
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

static void toc_callback(gpointer data, gint source, GdkInputCondition condition) {
	struct gaim_connection *gc = (struct gaim_connection *)data;
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	struct sflap_hdr *hdr;
	struct signon so;
	char buf[8 * 1024], *c;
	char snd[MSG_LEN];

	if (condition & GDK_INPUT_EXCEPTION) {
		debug_printf("gdk_input exception! check internet connection\n");
		hide_login_progress(gc, _("Connection Closed"));
		signoff(gc);
		return;
	}

	/* there's data waiting to be read, so read it. */
	if (wait_reply(gc, buf, 8 * 1024) < 0) {
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
		g_snprintf(snd, sizeof snd, "toc_signon %s %d %s %s %s \"%s\"",
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
			debug_printf("Didn't get SIGN_ON! buf was: %s\n", buf+sizeof(struct sflap_hdr));
			hide_login_progress(gc, _("Authentication Failed"));
			signoff(gc);
			return;
		}
		/* we're supposed to check that it's really TOC v1 here but we know it is ;) */
		debug_printf("TOC version: %s\n", buf + sizeof(struct sflap_hdr) + 4);

		/* we used to check for the CONFIG here, but we'll wait until we've sent our
		 * version of the config and then the toc_init_done message. we'll come back to
		 * the callback in a better state if we get CONFIG anyway */

		gc->options = gc->user->options;
		tdt->state = STATE_ONLINE;

		account_online(gc);
		serv_finish_login(gc);

		do_import(0, gc);

		/* Client sends TOC toc_init_done message */
		debug_printf("* Client sends TOC toc_init_done message\n");
		g_snprintf(snd, sizeof snd, "toc_init_done");
		sflap_send(gc, snd, -1, TYPE_DATA);

		g_snprintf(snd, sizeof snd, "toc_set_caps %s %s %s %s %s",
			FILE_SEND_UID, FILE_GET_UID, B_ICON_UID, IMAGE_UID, VOICE_UID);
		sflap_send(gc, snd, -1, TYPE_DATA);

		if (gc->keepalive < 0)
			update_keepalive(gc, gc->options & OPT_USR_KEEPALV);

		return;
	}

	debug_printf("From TOC server: %s\n", buf + sizeof(struct sflap_hdr));

	c = strtok(buf + sizeof(struct sflap_hdr), ":"); /* Ditch the first part */

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
		c = strtok(NULL, ":");
		g_snprintf(gc->username, sizeof(gc->username), "%s", c);
	} else if (!strcasecmp(c, "IM_IN")) {
		char *away, *message;
		int a = 0;

		c = strtok(NULL, ":");
		away = strtok(NULL, ":");

		message = away;
		while (*message && (*message != ':')) message++;
		message++;

		a = (away && (*away == 'T')) ? 1 : 0;

		serv_got_im(gc, c, message, a);
	} else if (!strcasecmp(c, "UPDATE_BUDDY")) {
		char *l, *uc;
		int logged, evil, idle, type = 0;
		time_t signon, time_idle;

		c = strtok(NULL, ":"); /* name */
		l = strtok(NULL, ":"); /* online */
		sscanf(strtok(NULL, ":"), "%d", &evil);
		sscanf(strtok(NULL, ":"), "%ld", &signon);
		sscanf(strtok(NULL, ":"), "%d", &idle);
		uc = strtok(NULL, ":");

		logged = (l && (*l == 'T')) ? 1 : 0;

		if (uc[0] == 'A') type |= UC_AOL;
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
		if (uc[2] == 'U') type |= UC_UNAVAILABLE;

		if (idle) { time(&time_idle); time_idle -= idle * 60; }
		else time_idle = 0;

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
		while (*m && (*m != ':')) m++;
		m++;

		w = (whisper && (*whisper == 'T')) ? 1 : 0;

		serv_got_chat_in(gc, id, who, w, m);
	} else if (!strcasecmp(c, "CHAT_UPDATE_BUDDY")) {
		int id;
		char *in, *buddy;
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

		if (!b) return;

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
		int id;

		sscanf(strtok(NULL, ":"), "%d", &id);

		serv_got_chat_left(gc, id);
	} else if (!strcasecmp(c, "GOTO_URL")) {
		char *name, *url, tmp[256];

		name = strtok(NULL, ":");
		url = strtok(NULL, ":");

		g_snprintf(tmp, sizeof(tmp), "http://%s:%d/%s", TOC_HOST, TOC_PORT, url);
		g_show_info(gc->user, tmp);
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
			struct file_transfer *ft;

			for (i = 0; i < 4; i++) {
				sscanf(strtok(NULL, ":"), "%d", &unk[i]);
				if (unk[i] == 10001) break;
				messages[i] = frombase64(strtok(NULL, ":"));
			}
			tmp = frombase64(strtok(NULL, ":"));

			subtype = tmp[1];
			files = tmp[3];
			
			totalsize |= (tmp[4] << 24) & 0xff000000;
			totalsize |= (tmp[5] << 16) & 0x00ff0000;
			totalsize |= (tmp[6] <<  8) & 0x0000ff00;
			totalsize |= (tmp[7] <<  0) & 0x000000ff;

			if (!totalsize) {
				g_free(tmp);
				for (i--; i >= 0; i--) g_free(messages[i]);
				return;
			}

			name = tmp + 8;

			ft = g_new0(struct file_transfer, 1);
			ft->cookie = g_strdup(cookie);
			ft->ip = g_strdup(pip);
			ft->port = port;
			if (i) ft->message = g_strdup(messages[0]);
			else ft->message = NULL;
			ft->filename = g_strdup(name);
			ft->user = g_strdup(user);
			ft->size = totalsize;
			g_snprintf(ft->UID, sizeof(ft->UID), "%s", FILE_SEND_UID);
			ft->gc = gc;

			g_free(tmp);
			for (i--; i >= 0; i--) g_free(messages[i]);

			accept_file_dialog(ft);
		} else if (!strcmp(uuid, FILE_GET_UID)) {
			/* they want us to send a file */
			int unk[4], i;
			char *messages[4], *tmp;
			struct file_transfer *ft;

			for (i = 0; i < 4; i++) {
				sscanf(strtok(NULL, ":"), "%d", unk + i);
				if (unk[i] == 10001) break;
				messages[i] = frombase64(strtok(NULL, ":"));
			}
			tmp = frombase64(strtok(NULL, ":"));

			ft = g_new0(struct file_transfer, 1);
			ft->cookie = g_strdup(cookie);
			ft->ip = g_strdup(pip);
			ft->port = port;
			if (i) ft->message = g_strdup(messages[0]);
			else ft->message = NULL;
			ft->user = g_strdup(user);
			g_snprintf(ft->UID, sizeof(ft->UID), "%s", FILE_GET_UID);
			ft->gc = gc;

			g_free(tmp);
			for (i--; i >= 0; i--) g_free(messages[i]);

			accept_file_dialog(ft);
		} else if (!strcmp(uuid, VOICE_UID)) {
			/* oh goody. voice over ip. fun stuff. */
		} else if (!strcmp(uuid, B_ICON_UID)) {
			/* buddy icon... */
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

void toc_build_config(struct gaim_connection *gc, char *s, int len, gboolean show)
{
	GSList *grp = gc->groups;
	GSList *mem;
	struct group *g;
	struct buddy *b;
	GSList *plist = gc->permit;
	GSList *dlist = gc->deny;

	int pos=0;

	if (!gc->permdeny)
		gc->permdeny = 1;

	pos += g_snprintf(&s[pos], len - pos, "m %d\n", gc->permdeny);
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
				add_group(gc, current);
				how_many++;
			} else if (*c == 'b' && !find_buddy(gc, c+2)) {
				char nm[80], sw[80], *tmp = c+2;
				int i = 0;
				while (*tmp != ':' && *tmp)
					nm[i++] = *tmp++;
				if (*tmp == ':') *tmp++ = '\0';
				nm[i] = '\0';
				i = 0;
				while (*tmp) sw[i++] = *tmp++;
				sw[i] = '\0';
				if (!find_buddy(gc, nm))
					add_buddy(gc, current, nm, sw);
				how_many++;
				
				bud = g_list_append(bud, c+2);
			} else if (*c == 'p') {
				GSList *d = gc->permit;
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
					gc->permit = g_slist_append(gc->permit, name);
			} else if (*c == 'd') {
				GSList *d = gc->deny;
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
					gc->deny = g_slist_append(gc->deny, name);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &gc->permdeny);
				sprintf(debug_buff, "permdeny: %d\n", gc->permdeny);
				debug_print(debug_buff);
				if (gc->permdeny == 0)
					gc->permdeny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &gc->permdeny);
				sprintf(debug_buff, "permdeny: %d\n", gc->permdeny);
				debug_print(debug_buff);
				if (gc->permdeny == 0)
					gc->permdeny = 1;
			}
		} while((c=strtok(NULL,"\n"))); 
#if 0
		fprintf(stdout, "Sending message '%s'\n",buf);
#endif
				      
		if (bud != NULL) serv_add_buddies(gc, bud);
		serv_set_permit_deny(gc);
		if (blist) {
			build_edit_tree();
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

static void toc_set_config(struct gaim_connection *gc) {
	char buf[MSG_LEN], snd[MSG_LEN];
	toc_build_config(gc, buf, MSG_LEN, FALSE);
	g_snprintf(snd, MSG_LEN, "toc_set_config \"%s\"", buf);
	sflap_send(gc, snd, -1, TYPE_DATA);
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
	toc_set_config(g);
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
	toc_set_config(g);
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

static char **toc_list_icon(int uc) {
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

static void toc_info(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_info(gc, who);
}

static void toc_dir_info(GtkObject *obj, char *who) {
	struct gaim_connection *gc = (struct gaim_connection *)gtk_object_get_user_data(obj);
	serv_get_dir(gc, who);
}

static void toc_action_menu(GtkWidget *menu, struct gaim_connection *gc, char *who) {
	GtkWidget *button;

	button = gtk_menu_item_new_with_label(_("Get Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(toc_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Get Dir Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(toc_dir_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static void toc_print_option(GtkEntry *entry, struct aim_user *user) {
	int entrynum;

	entrynum = (int) gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_AUTH) {
		g_snprintf(user->proto_opt[USEROPT_AUTH],
				sizeof(user->proto_opt[USEROPT_AUTH]), 
				"%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_AUTHPORT) {
		g_snprintf(user->proto_opt[USEROPT_AUTHPORT], 
				sizeof(user->proto_opt[USEROPT_AUTHPORT]), 
				"%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_SOCKSHOST) {
		g_snprintf(user->proto_opt[USEROPT_SOCKSHOST], 
				sizeof(user->proto_opt[USEROPT_SOCKSHOST]), 
				"%s", gtk_entry_get_text(entry));
	} else if (entrynum == USEROPT_SOCKSPORT) {
		g_snprintf(user->proto_opt[USEROPT_SOCKSPORT], 
				sizeof(user->proto_opt[USEROPT_SOCKSPORT]), 
				"%s", gtk_entry_get_text(entry));
	} 
}

static void toc_print_optionrad(GtkRadioButton *entry, struct aim_user *user) {
	int entrynum;

	entrynum = (int) gtk_object_get_user_data(GTK_OBJECT(entry));

	g_snprintf(user->proto_opt[USEROPT_PROXYTYPE],
			sizeof(user->proto_opt[USEROPT_PROXYTYPE]),
			"%d", entrynum);
}

static void toc_user_opts(GtkWidget *book, struct aim_user *user) {
	/* so here, we create the new notebook page */
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *first, *opt;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
			gtk_label_new("TOC Options"));
	gtk_widget_show(vbox);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("TOC Host:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_AUTH);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_AUTH][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTH]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTH]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "toc.oscar.aol.com");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);


	label = gtk_label_new("TOC Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)1);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_AUTHPORT][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_AUTHPORT]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_AUTHPORT]);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "9898");

	gtk_widget_show(entry);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("Proxy Host:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_SOCKSHOST);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_SOCKSHOST][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_SOCKSHOST]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_SOCKSHOST]);
	}
	gtk_widget_show(entry);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new("Proxy Port:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_SOCKSPORT);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(toc_print_option), user);
	if (user->proto_opt[USEROPT_SOCKSPORT][0]) {
		debug_printf("setting text %s\n", user->proto_opt[USEROPT_SOCKSPORT]);
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_SOCKSPORT]);
	}
	gtk_widget_show(entry);

	
	first = gtk_radio_button_new_with_label(NULL, "No proxy");
	gtk_box_pack_start(GTK_BOX(vbox), first, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(first), (void *)PROXY_NONE);
	gtk_signal_connect(GTK_OBJECT(first), "clicked", GTK_SIGNAL_FUNC(toc_print_optionrad), user);
	gtk_widget_show(first);
	if (atoi(user->proto_opt[USEROPT_PROXYTYPE]) == PROXY_NONE)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(first), TRUE);

	opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)), "SOCKS 4");
	gtk_box_pack_start(GTK_BOX(vbox), opt, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(opt), (void *)PROXY_SOCKS4);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(toc_print_optionrad), user);
	gtk_widget_show(opt);
	if (atoi(user->proto_opt[USEROPT_PROXYTYPE]) == PROXY_SOCKS4)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)), "SOCKS 5");
	gtk_box_pack_start(GTK_BOX(vbox), opt, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(opt), (void *)PROXY_SOCKS5);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(toc_print_optionrad), user);
	gtk_widget_show(opt);
	if (atoi(user->proto_opt[USEROPT_PROXYTYPE]) == PROXY_SOCKS5)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	opt = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(first)), "HTTP");
	gtk_box_pack_start(GTK_BOX(vbox), opt, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(opt), (void *)PROXY_HTTP);
	gtk_signal_connect(GTK_OBJECT(opt), "clicked", GTK_SIGNAL_FUNC(toc_print_optionrad), user);
	gtk_widget_show(opt);
	if (atoi(user->proto_opt[USEROPT_PROXYTYPE]) == PROXY_HTTP)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);
}

static void toc_add_permit(struct gaim_connection *gc, char *who) {
	char buf2[MSG_LEN];
	if (gc->permdeny != 3) return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_permit %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
}

static void toc_add_deny(struct gaim_connection *gc, char *who) {
	char buf2[MSG_LEN];
	if (gc->permdeny != 4) return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_permit %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
}

static void toc_set_permit_deny(struct gaim_connection *gc) {
	char buf2[MSG_LEN];
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
			if (at > MSG_LEN + 32) /* from out my ass comes greatness */ {
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
			if (at > MSG_LEN + 32) /* from out my ass comes greatness */ {
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
}

static void toc_rem_permit(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 3) return;
	toc_set_permit_deny(gc);
}

static void toc_rem_deny(struct gaim_connection *gc, char *who) {
	if (gc->permdeny != 4) return;
	toc_set_permit_deny(gc);
}

void toc_init(struct prpl *ret) {
        ret->protocol = PROTO_TOC;
        ret->name = toc_name;
	ret->list_icon = toc_list_icon;
	ret->action_menu = toc_action_menu;
	ret->user_opts = toc_user_opts;
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
	ret->rem_deny = toc_add_deny;
	ret->set_permit_deny = toc_set_permit_deny;
        ret->warn = toc_warn;
        ret->accept_chat = toc_accept_chat;
        ret->join_chat = toc_join_chat;
        ret->chat_invite = toc_chat_invite;
        ret->chat_leave = toc_chat_leave;
        ret->chat_whisper = toc_chat_whisper;
        ret->chat_send = toc_chat_send;
	ret->keepalive = toc_keepalive;
}
