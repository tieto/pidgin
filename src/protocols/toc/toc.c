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
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "multi.h"
#include "notify.h"
#include "proxy.h"
#include "request.h"
#include "util.h"

/* XXX */
#include "gaim.h"
#include "ui.h"
#include "gtkutils.h"

static GaimPlugin *my_protocol = NULL;

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

#define UC_AOL		0x02
#define UC_ADMIN	0x04
#define UC_UNCONFIRMED	0x08
#define UC_NORMAL	0x10
#define UC_WIRELESS     0x20

struct ft_request {
	GaimConnection *gc;
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
	char toc_ip[20];
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

#define TOC_CONNECT_STEPS 3

static void toc_login_callback(gpointer, gint, GaimInputCondition);
static void toc_callback(gpointer, gint, GaimInputCondition);
static void accept_file_dialog(struct ft_request *);

/* The following were added for win32 port - Herman */

int toc_write(int fd, const void *buffer, int len)
{
#ifndef _WIN32
       return write(fd, buffer, len);
#else
       return send(fd, buffer, len, 0);
#endif
}

int toc_read(int fd, void *buffer, int size)
{
#ifndef _WIN32
       return read(fd, buffer, size);
#else
       return recv(fd, buffer, size, 0);
#endif
}

int toc_soc_close( int fd )
{
#ifndef _WIN32
       return close(fd);
#else
       return closesocket(fd);
#endif
}


/* ok. this function used to take username/password, and return 0 on success.
 * now, it takes username/password, and returns NULL on error or a new gaim_connection
 * on success. */
static void toc_login(GaimAccount *account)
{
	GaimConnection *gc;
	struct toc_data *tdt;
	char buf[80];

	gc = gaim_account_get_connection(account);
	gc->proto_data = tdt = g_new0(struct toc_data, 1);
	gc->flags |= OPT_CONN_HTML;
	gc->flags |= OPT_CONN_AUTO_RESP;

	g_snprintf(buf, sizeof buf, _("Looking up %s"),
			gaim_account_get_string(account, "server", TOC_HOST));
	gaim_connection_update_progress(gc, buf, 1, TOC_CONNECT_STEPS);

	debug_printf("* Client connects to TOC\n");
	if (gaim_proxy_connect(account,
				gaim_account_get_string(account, "server", TOC_HOST),
				gaim_account_get_int(account, "port", TOC_PORT),
				toc_login_callback, gc) != 0 || !account->gc) {
		g_snprintf(buf, sizeof(buf), "Connect to %s failed",
				gaim_account_get_string(account, "server", TOC_HOST));
		gaim_connection_error(gc, buf);
		return;
	}
}

static void toc_login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct toc_data *tdt;
	char buf[80];
	struct sockaddr_in name;
	socklen_t namelen;

	if (!g_list_find(gaim_connections_get_all(), data)) {
		toc_soc_close(source);
		return;
	}

	tdt = gc->proto_data;

	if (source == -1) {
		/* we didn't successfully connect. tdt->toc_fd is valid here */
		gaim_connection_error(gc, "Unable to connect.");
		return;
	}
	tdt->toc_fd = source;

	/*
	 * Copy the IP that we're connected to.  We need this because "GOTO_URL"'s 
	 * should open on the exact server we're connected to.  toc.oscar.aol.com 
	 * doesn't work because that hostname resolves to multiple IP addresses.
	 */
	if (getpeername(tdt->toc_fd, (struct sockaddr *)&name, &namelen) == 0)
		strncpy(tdt->toc_ip, inet_ntoa(name.sin_addr), sizeof(tdt->toc_ip));
	else
		strncpy(tdt->toc_ip, gaim_account_get_string(gc->account, "server", TOC_HOST), sizeof(tdt->toc_ip));

	debug_printf("* Client sends \"FLAPON\\r\\n\\r\\n\"\n");
	if (toc_write(tdt->toc_fd, FLAPON, strlen(FLAPON)) < 0) {
		gaim_connection_error(gc, "Disconnected.");
		return;
	}
	tdt->state = STATE_FLAPON;

	/* i know a lot of people like to look at gaim to see how TOC works. so i'll comment
	 * on what this does. it's really simple. when there's data ready to be read from the
	 * toc_fd file descriptor, toc_callback is called, with gc passed as its data arg. */
	gc->inpa = gaim_input_add(tdt->toc_fd, GAIM_INPUT_READ, toc_callback, gc);

	g_snprintf(buf, sizeof(buf), _("Signon: %s"), gaim_account_get_username(gc->account));
	gaim_connection_update_progress(gc, buf, 2, TOC_CONNECT_STEPS);
}

static void toc_close(GaimConnection *gc)
{
	if (gc->inpa > 0)
		gaim_input_remove(gc->inpa);
	gc->inpa = 0;
	toc_soc_close(((struct toc_data *)gc->proto_data)->toc_fd);
	g_free(gc->proto_data);
}

static void toc_build_config(GaimAccount *account, char *s, int len, gboolean show)
{
	GaimBlistNode *gnode,*bnode;
	struct group *g;
	struct buddy *b;
	GSList *plist = account->permit;
	GSList *dlist = account->deny;

	int pos = 0;

	if (!account->perm_deny)
		account->perm_deny = 1;

	pos += g_snprintf(&s[pos], len - pos, "m %d\n", account->perm_deny);
	for(gnode = gaim_get_blist()->root; gnode && len > pos; gnode = gnode->next) {
		g = (struct group *)gnode;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		if(gaim_group_on_account(g, account)) {
			pos += g_snprintf(&s[pos], len - pos, "g %s\n", g->name);
			for(bnode = gnode->child; bnode && len > pos; bnode = bnode->next) {
				b = (struct buddy *)bnode;
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				if(b->account == account) {
					pos += g_snprintf(&s[pos], len - pos, "b %s%s%s\n", b->name,
							(show && b->alias) ? ":" : "",
							(show && b->alias) ? b->alias : "");
				}
			}
		}
	}

	while (len > pos && plist) {
		pos += g_snprintf(&s[pos], len - pos, "p %s\n", (char *)plist->data);
		plist = plist->next;
	}

	while (len > pos && dlist) {
		pos += g_snprintf(&s[pos], len - pos, "d %s\n", (char *)dlist->data);
		dlist = dlist->next;
	}
}

static int escape_message(char *msg)
{
	char *c, *cpy;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
	if (strlen(msg) > BUF_LEN) {
		debug_printf("Warning:  truncating message to 2048 bytes\n");
		msg[2047] = '\0';
	}

	cpy = g_strdup(msg);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '$':
		case '[':
		case ']':
		case '(':
		case ')':
			msg[cnt++] = '\\';
			/* Fall through */
		default:
			msg[cnt++] = *c;
		}
		c++;
	}
	msg[cnt] = '\0';
	g_free(cpy);
	return cnt;
}

static int escape_text(char *msg)
{
	char *c, *cpy;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 4 bytes */
	if (strlen(msg) > BUF_LEN) {
		fprintf(stderr, "Warning:  truncating message to 2048 bytes\n");
		msg[2047] = '\0';
	}

	cpy = g_strdup(msg);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '\n':
			msg[cnt++] = '<';
			msg[cnt++] = 'B';
			msg[cnt++] = 'R';
			msg[cnt++] = '>';
			break;
		case '{':
		case '}':
		case '\\':
		case '"':
			msg[cnt++] = '\\';
			/* Fall through */
		default:
			msg[cnt++] = *c;
		}
		c++;
	}
	msg[cnt] = '\0';
	g_free(cpy);
	return cnt;
}

static int sflap_send(GaimConnection *gc, char *buf, int olen, int type)
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

	if (olen < 0)
		len = escape_message(buf);
	else
		len = olen;

	/* One _last_ 2048 check here!  This shouldn't ever
	   * get hit though, hopefully.  If it gets hit on an IM
	   * It'll lose the last " and the message won't go through,
	   * but this'll stop a segfault. */
	if (len > MSG_LEN) {
		debug_printf("message too long, truncating\n");
		buf[MSG_LEN - 1] = '\0';
		len = MSG_LEN;
	}

	if (olen < 0)
		debug_printf("TOC C: %s\n", buf);

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

	return toc_write(tdt->toc_fd, obuf, slen);
}

static int wait_reply(GaimConnection *gc, char *buffer, size_t buflen)
{
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	struct sflap_hdr *hdr;
	int ret;

	if (toc_read(tdt->toc_fd, buffer, sizeof(struct sflap_hdr)) < 0) {
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
			ret = toc_read(tdt->toc_fd,
				   buffer + sizeof(struct sflap_hdr) + count, ntohs(hdr->len) - count);
		} while (count + ret < ntohs(hdr->len) && ret > 0);
		buffer[sizeof(struct sflap_hdr) + count + ret] = '\0';
		return ret;
	} else
		return 0;
}

static unsigned char *roast_password(const char *pass)
{
	/* Trivial "encryption" */
	static unsigned char rp[256];
	static char *roast = ROAST;
	int pos = 2;
	int x;
	strcpy(rp, "0x");
	for (x = 0; (x < 150) && pass[x]; x++)
		pos += sprintf(&rp[pos], "%02x", pass[x] ^ roast[x % strlen(roast)]);
	rp[pos] = '\0';
	return rp;
}

static void toc_got_info(gpointer data, char *url_text, unsigned long len)
{
	if (!url_text)
		return;

	g_show_info_text(NULL, NULL, 2, url_text, NULL);
}

static char *show_error_message()
{
	int no = atoi(strtok(NULL, ":"));
	char *w = strtok(NULL, ":");
	static char buf[256];

        switch(no) {
        case 69:
                g_snprintf(buf, sizeof(buf), _("Unable to write file %s."), w);
                break;
        case 169:
                g_snprintf(buf, sizeof(buf), _("Unable to read file %s."), w);
                break;
        case 269:
                g_snprintf(buf, sizeof(buf), _("Message too long, last %s bytes truncated."), w);
                break;
        case 901:
                g_snprintf(buf, sizeof(buf), _("%s not currently logged in."), w);
                break;
        case 902:
                g_snprintf(buf, sizeof(buf), _("Warning of %s not allowed."), w);
                break;
        case 903:
                g_snprintf(buf, sizeof(buf), _("A message has been dropped, you are exceeding the server speed limit."));
                break;
        case 950:
                g_snprintf(buf, sizeof(buf), _("Chat in %s is not available."), w);
                break;
        case 960:
                g_snprintf(buf, sizeof(buf), _("You are sending messages too fast to %s."), w);
                break;
        case 961:
                g_snprintf(buf, sizeof(buf), _("You missed an IM from %s because it was too big."), w);
                break;
        case 962:
                g_snprintf(buf, sizeof(buf), _("You missed an IM from %s because it was sent too fast."), w);
                break;
        case 970:
                g_snprintf(buf, sizeof(buf), _("Failure."));
                break;
        case 971:
                g_snprintf(buf, sizeof(buf), _("Too many matches."));
                break;
        case 972:
                g_snprintf(buf, sizeof(buf), _("Need more qualifiers."));
                break;
        case 973:
                g_snprintf(buf, sizeof(buf), _("Dir service temporarily unavailable."));
                break;
        case 974:
                g_snprintf(buf, sizeof(buf), _("Email lookup restricted."));
                break;
        case 975:
                g_snprintf(buf, sizeof(buf), _("Keyword ignored."));
                break;
        case 976:
                g_snprintf(buf, sizeof(buf), _("No keywords."));
                break;
        case 977:
                g_snprintf(buf, sizeof(buf), _("User has no directory information."));
                /* g_snprintf(buf, sizeof(buf), "Language not supported."); */
                break;
        case 978:
                g_snprintf(buf, sizeof(buf), _("Country not supported."));
                break;
        case 979:
                g_snprintf(buf, sizeof(buf), _("Failure unknown: %s."), w);
                break;
        case 980:
                g_snprintf(buf, sizeof(buf), _("Incorrect nickname or password."));
                break;
        case 981:
                g_snprintf(buf, sizeof(buf), _("The service is temporarily unavailable."));
                break;
        case 982:
                g_snprintf(buf, sizeof(buf), _("Your warning level is currently too high to log in."));
                break;
        case 983:
                g_snprintf(buf, sizeof(buf), _("You have been connecting and disconnecting too frequently.  Wait ten minutes and try again.  If you continue to try, you will need to wait even longer."));
                break;
                g_snprintf(buf, sizeof(buf), _("An unknown signon error has occurred: %s."), w);
                break;
        default:
                g_snprintf(buf, sizeof(buf), _("An unknown error, %d, has occurred.  Info: %s"), no, w);
	}

        return buf;
}

static void toc_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = (GaimConnection *)data;
	struct toc_data *tdt = (struct toc_data *)gc->proto_data;
	struct sflap_hdr *hdr;
	struct signon so;
	char buf[8 * 1024], *c;
	char snd[BUF_LEN * 2];

	const char *username = gaim_account_get_username(gc->account);
	char *password;

	/* there's data waiting to be read, so read it. */
	if (wait_reply(gc, buf, 8 * 1024) <= 0) {
		gaim_connection_error(gc, _("Connection Closed"));
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
		g_snprintf(so.username, sizeof(so.username), "%s", username);
		so.ver = htonl(1);
		so.tag = htons(1);
		so.namelen = htons(strlen(so.username));
		if (sflap_send(gc, (char *)&so, ntohs(so.namelen) + 8, TYPE_SIGNON) < 0) {
			gaim_connection_error(gc, _("Disconnected."));
			return;
		}

		debug_printf("* Client sends TOC \"toc_signon\" message\n");
		/* i hate icq. */
		if (username[0] >= '0' && username[0] <= '9')
			password = g_strndup(gaim_account_get_password(gc->account), 8);
		else
			password = g_strdup(gaim_account_get_password(gc->account));
		g_snprintf(snd, sizeof snd, "toc_signon %s %d  %s %s %s \"%s\"",
			   AUTH_HOST, AUTH_PORT, normalize(username),
			   roast_password(password), LANGUAGE, REVISION);
		g_free(password);
		if (sflap_send(gc, snd, -1, TYPE_DATA) < 0) {
			gaim_connection_error(gc, _("Disconnected."));
			return;
		}

		gaim_connection_update_progress(gc, _("Waiting for reply..."), 3, TOC_CONNECT_STEPS);
		return;
	}

	if (tdt->state == STATE_SIGNON_REQUEST) {
		debug_printf("* TOC sends client SIGN_ON reply\n");
		if (g_ascii_strncasecmp(buf + sizeof(struct sflap_hdr), "SIGN_ON", strlen("SIGN_ON"))) {
			debug_printf("Didn't get SIGN_ON! buf was: %s\n",
				     buf + sizeof(struct sflap_hdr));
			if (!g_ascii_strncasecmp(buf + sizeof(struct sflap_hdr), "ERROR", 5)) {
				strtok(buf + sizeof(struct sflap_hdr), ":");
				gaim_connection_error(gc, show_error_message());
			} else
				gaim_connection_error(gc, _("Authentication Failed"));
			return;
		}
		/* we're supposed to check that it's really TOC v1 here but we know it is ;) */
		debug_printf("TOC version: %s\n", buf + sizeof(struct sflap_hdr) + 8);

		/* we used to check for the CONFIG here, but we'll wait until we've sent our
		 * version of the config and then the toc_init_done message. we'll come back to
		 * the callback in a better state if we get CONFIG anyway */

		tdt->state = STATE_ONLINE;

		gaim_connection_set_state(gc, GAIM_CONNECTED);
		serv_finish_login(gc);

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

	debug_printf("TOC S: %s\n", buf + sizeof(struct sflap_hdr));

	c = strtok(buf + sizeof(struct sflap_hdr), ":");	/* Ditch the first part */

	if (!g_ascii_strcasecmp(c, "SIGN_ON")) {
		/* we should only get here after a PAUSE */
		if (tdt->state != STATE_PAUSE)
			debug_printf("got SIGN_ON but not PAUSE!\n");
		else {
			tdt->state = STATE_ONLINE;
			g_snprintf(snd, sizeof snd, "toc_signon %s %d %s %s %s \"%s\"",
				   AUTH_HOST, AUTH_PORT,
				   normalize(gaim_account_get_username(gc->account)),
				   roast_password(gaim_account_get_password(gc->account)),
				   LANGUAGE, REVISION);
			if (sflap_send(gc, snd, -1, TYPE_DATA) < 0) {
				gaim_connection_error(gc, _("Disconnected."));
				return;
			}
			g_snprintf(snd, sizeof snd, "toc_init_done");
			sflap_send(gc, snd, -1, TYPE_DATA);
			gaim_notify_info(gc, NULL,
							 _("TOC has come back from its pause. You may "
							   "now send messages again."), NULL);
		}
	} else if (!g_ascii_strcasecmp(c, "CONFIG")) {
		c = strtok(NULL, ":");
		parse_toc_buddy_list(gc->account, c);
	} else if (!g_ascii_strcasecmp(c, "NICK")) {
		/* ignore NICK so that things get imported/exported properly
		c = strtok(NULL, ":");
		g_snprintf(gc->username, sizeof(gc->username), "%s", c);
		*/
	} else if (!g_ascii_strcasecmp(c, "IM_IN")) {
		char *away, *message;
		int a = 0;

		c = strtok(NULL, ":");
		away = strtok(NULL, ":");

		message = away;
		while (*message && (*message != ':'))
			message++;
		message++;

		a = (away && (*away == 'T')) ? IM_FLAG_AWAY : 0;

		serv_got_im(gc, c, message, a, time(NULL), -1);
	} else if (!g_ascii_strcasecmp(c, "UPDATE_BUDDY")) {
		char *l, *uc, *tmp;
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
		case 'C':
			type |= UC_WIRELESS;
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

		tmp = g_strdup(normalize(gaim_account_get_username(gc->account)));
		if (!strcmp(tmp, normalize(c)))
			gaim_connection_set_display_name(gc, c);
		g_free(tmp);

		serv_got_update(gc, c, logged, evil, signon, time_idle, type);
	} else if (!g_ascii_strcasecmp(c, "ERROR")) {
		gaim_notify_error(gc, NULL, show_error_message(), NULL);
	} else if (!g_ascii_strcasecmp(c, "EVILED")) {
		int lev;
		char *name;

		sscanf(strtok(NULL, ":"), "%d", &lev);
		name = strtok(NULL, ":");

		serv_got_eviled(gc, name, lev);
	} else if (!g_ascii_strcasecmp(c, "CHAT_JOIN")) {
		char *name;
		int id;

		sscanf(strtok(NULL, ":"), "%d", &id);
		name = strtok(NULL, ":");

		serv_got_joined_chat(gc, id, name);
	} else if (!g_ascii_strcasecmp(c, "CHAT_IN")) {
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
	} else if (!g_ascii_strcasecmp(c, "CHAT_UPDATE_BUDDY")) {
		int id;
		char *in, *buddy;
		GSList *bcs = gc->buddy_chats;
		GaimConversation *b = NULL;
		GaimChat *chat;

		sscanf(strtok(NULL, ":"), "%d", &id);
		in = strtok(NULL, ":");

		chat = GAIM_CHAT(b);

		while (bcs) {
			b = (GaimConversation *)bcs->data;
			if (id == gaim_chat_get_id(chat))
				break;
			bcs = bcs->next;
			b = NULL;
		}

		if (!b)
			return;

		if (in && (*in == 'T'))
			while ((buddy = strtok(NULL, ":")) != NULL)
				gaim_chat_add_user(chat, buddy, NULL);
		else
			while ((buddy = strtok(NULL, ":")) != NULL)
				gaim_chat_remove_user(chat, buddy, NULL);
	} else if (!g_ascii_strcasecmp(c, "CHAT_INVITE")) {
		char *name, *who, *message;
		int id;
		GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, g_free);

		name = strtok(NULL, ":");
		sscanf(strtok(NULL, ":"), "%d", &id);
		who = strtok(NULL, ":");
		message = strtok(NULL, ":");

		g_hash_table_replace(components, g_strdup("id"), g_strdup_printf("%d", id));

		serv_got_chat_invite(gc, name, who, message, components);
	} else if (!g_ascii_strcasecmp(c, "CHAT_LEFT")) {
		GSList *bcs = gc->buddy_chats;
		GaimConversation *b = NULL;
		int id;

		sscanf(strtok(NULL, ":"), "%d", &id);

		while (bcs) {
			b = (GaimConversation *)bcs->data;
			if (id == gaim_chat_get_id(GAIM_CHAT(b)))
				break;
			b = NULL;
			bcs = bcs->next;
		}

		if (!b)
			return;

		if (b->window) {
			char error_buf[BUF_LONG];
			gaim_conversation_set_account(b, NULL);
			g_snprintf(error_buf, sizeof error_buf, _("You have been disconnected"
								  " from chat room %s."), b->name);
			gaim_notify_error(gc, NULL, error_buf, NULL);
		} else
			serv_got_chat_left(gc, id);
	} else if (!g_ascii_strcasecmp(c, "GOTO_URL")) {
		char *name, *url, tmp[256];

		name = strtok(NULL, ":");
		url = strtok(NULL, ":");

		g_snprintf(tmp, sizeof(tmp), "http://%s:%d/%s", tdt->toc_ip,
				gaim_account_get_int(gc->account, "port", TOC_PORT),
				url);
		grab_url(tmp, FALSE, toc_got_info, NULL);
	} else if (!g_ascii_strcasecmp(c, "DIR_STATUS")) {
	} else if (!g_ascii_strcasecmp(c, "ADMIN_NICK_STATUS")) {
	} else if (!g_ascii_strcasecmp(c, "ADMIN_PASSWD_STATUS")) {
		gaim_notify_info(gc, NULL, _("Password Change Successful"), NULL);
	} else if (!g_ascii_strcasecmp(c, "PAUSE")) {
		tdt->state = STATE_PAUSE;
		gaim_notify_warning(gc, NULL,
							_("TOC has sent a PAUSE command."),
							_("When this happens, TOC ignores any messages "
							  "sent to it, and may kick you off if you send a"
							  " message. Gaim will prevent anything from "
							  "going through. This is only temporary, please "
							  "be patient."));
	} else if (!g_ascii_strcasecmp(c, "RVOUS_PROPOSE")) {
		char *user, *uuid, *cookie;
		int seq;
		char *rip, *pip, *vip, *trillian = NULL;
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
				trillian = strtok(NULL, ":");
				sscanf(trillian, "%d", &unk[i]);
				if (unk[i] == 10001)
					break;
				/* Trillian likes to send an empty token as a message, rather than
				   no message at all. */
				if (*(trillian + strlen(trillian) +1) != ':')
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
				" total size of %d bytes.\n", user, vip, port, files, totalsize);
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
				/* Trillian likes to send an empty token as a message, rather than
				   no message at all. */
				if (*(trillian + strlen(trillian) +1) != ':')
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

static int toc_send_im(GaimConnection *gc, const char *name, const char *message, int len, int flags)
{
	char buf[BUF_LEN * 2];
	char *tmp = g_malloc(strlen(message) * 4 + 1); /* 4 because \n gets replaced with <BR> */

	strcpy(tmp, message);
	escape_text(tmp);
	if (strlen(tmp) + 52 > MSG_LEN) {
		g_free(tmp);
		return -E2BIG;
	}
	g_snprintf(buf, MSG_LEN - 8, "toc_send_im %s \"%s\"%s", normalize(name),
		   tmp, ((flags & IM_FLAG_AWAY) ? " auto" : ""));
	sflap_send(gc, buf, -1, TYPE_DATA);
	
	g_free(tmp);
	return 1;
}

static void toc_set_config(GaimConnection *gc)
{
	char *buf = g_malloc(MSG_LEN), snd[BUF_LEN * 2];
	toc_build_config(gc->account, buf, MSG_LEN - strlen("toc_set_config \\{\\}"), FALSE);
	g_snprintf(snd, MSG_LEN, "toc_set_config {%s}", buf);
	sflap_send(gc, snd, -1, TYPE_DATA);
	g_free(buf);
}

static void toc_get_info(GaimConnection *g, const char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, MSG_LEN, "toc_get_info %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_get_dir(GaimConnection *g, const char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, MSG_LEN, "toc_get_dir %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_dir(GaimConnection *g, const char *first, const char *middle, const char *last,
			const char *maiden, const char *city, const char *state, const char *country, int web)
{
	char buf2[BUF_LEN * 4], buf[BUF_LEN];
	g_snprintf(buf2, sizeof(buf2), "%s:%s:%s:%s:%s:%s:%s:%s", first,
		   middle, last, maiden, city, state, country, (web == 1) ? "Y" : "");
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_dir %s", buf2);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_dir_search(GaimConnection *g, const char *first, const char *middle, const char *last,
			   const char *maiden, const char *city, const char *state, const char *country, const char *email)
{
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf) / 2, "toc_dir_search %s:%s:%s:%s:%s:%s:%s:%s", first, middle,
		   last, maiden, city, state, country, email);
	debug_printf("Searching for: %s,%s,%s,%s,%s,%s,%s\n", first, middle, last, maiden,
		     city, state, country);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_away(GaimConnection *g, char *state, char *message)
{
	char buf[BUF_LEN * 2];
	if (g->away) {
		g_free (g->away);
		g->away = NULL;
	}
	if (message) {
		char *tmp = g_malloc(strlen(message) * 4 + 1);
		strcpy(tmp, message);
		g->away = g_strdup (message);
		escape_text(tmp);
		g_snprintf(buf, MSG_LEN, "toc_set_away \"%s\"", tmp);
		g_free(tmp);
	} else
		g_snprintf(buf, MSG_LEN, "toc_set_away \"\"");
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_set_info(GaimConnection *g, const char *info)
{
	char buf[BUF_LEN * 2], buf2[BUF_LEN * 2];
	g_snprintf(buf2, sizeof buf2, "%s", info);
	escape_text(buf2);
	g_snprintf(buf, sizeof(buf), "toc_set_info \"%s\n\"", buf2);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_change_passwd(GaimConnection *g, const char *orig, const char *new)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new);
	sflap_send(g, buf, strlen(buf), TYPE_DATA);
}

static void toc_add_buddy(GaimConnection *g, const char *name)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_add_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
	toc_set_config(g);
}

static void toc_add_buddies(GaimConnection *g, GList *buddies)
{
	char buf[BUF_LEN * 2];
	int n;

	n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
	while (buddies) {
		if (strlen(normalize(buddies->data)) + n + 32 > MSG_LEN) {
			sflap_send(g, buf, -1, TYPE_DATA);
			n = g_snprintf(buf, sizeof(buf), "toc_add_buddy");
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
		buddies = buddies->next;
	}
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_remove_buddy(GaimConnection *g, char *name, char *group)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_remove_buddy %s", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
	toc_set_config(g);
}

static void toc_remove_buddies(GaimConnection *g, GList *buddies, const char *group)
{
	char buf[BUF_LEN * 2];
	int n;

	n = g_snprintf(buf, sizeof(buf), "toc_remove_buddy");
	while (buddies) {
		if (strlen(normalize(buddies->data)) + n + 32 > MSG_LEN) {
			sflap_send(g, buf, -1, TYPE_DATA);
			n = g_snprintf(buf, sizeof(buf), "toc_remove_buddy");
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, " %s", normalize(buddies->data));
		buddies = buddies->next;
	}
	sflap_send(g, buf, -1, TYPE_DATA);
	toc_set_config(g);
}

static void toc_set_idle(GaimConnection *g, int time)
{
	char buf[BUF_LEN * 2];
	g_snprintf(buf, sizeof(buf), "toc_set_idle %d", time);
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_warn(GaimConnection *g, char *name, int anon)
{
	char send[BUF_LEN * 2];
	g_snprintf(send, 255, "toc_evil %s %s", name, ((anon) ? "anon" : "norm"));
	sflap_send(g, send, -1, TYPE_DATA);
}

static GList *toc_chat_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Join what group:");
	pce->identifier = "room";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Exchange:");
	pce->identifier = "exchange";
	pce->is_int = TRUE;
	pce->min = 4;
	pce->max = 20;
	m = g_list_append(m, pce);

	return m;
}

static void toc_join_chat(GaimConnection *g, GHashTable *data)
{
	char buf[BUF_LONG];
	char *name, *exchange;
	char *id;

	name = g_hash_table_lookup(data, "room");
	exchange = g_hash_table_lookup(data, "exchange");
	id = g_hash_table_lookup(data, "id");

	if (id) {
		g_snprintf(buf, 255, "toc_chat_accept %d", atoi(id));
	} else {
		g_snprintf(buf, sizeof(buf) / 2, "toc_chat_join %d \"%s\"", atoi(exchange), name);
	}

	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_invite(GaimConnection *g, int id, const char *message, const char *name)
{
	char buf[BUF_LONG];
	g_snprintf(buf, sizeof(buf) / 2, "toc_chat_invite %d \"%s\" %s", id,
			message ? message : "", normalize(name));
	sflap_send(g, buf, -1, TYPE_DATA);
}

static void toc_chat_leave(GaimConnection *g, int id)
{
	GSList *bcs = g->buddy_chats;
	GaimConversation *b = NULL;
	char buf[BUF_LEN * 2];

	while (bcs) {
		b = (GaimConversation *)bcs->data;
		if (id == gaim_chat_get_id(GAIM_CHAT(b)))
			break;
		b = NULL;
		bcs = bcs->next;
	}

	if (!b)
		return;		/* can this happen? */

	if (gaim_conversation_get_account(b) == NULL) {
		/* TOC already kicked us out of this room */
		serv_got_chat_left(g, id);
	}
	else {
		g_snprintf(buf, 255, "toc_chat_leave %d", id);
		sflap_send(g, buf, -1, TYPE_DATA);
	}
}

static void toc_chat_whisper(GaimConnection *g, int id, char *who, char *message)
{
	char buf2[BUF_LEN * 2];
	escape_text(message);
	g_snprintf(buf2, sizeof(buf2), "toc_chat_whisper %d %s \"%s\"", id, normalize(who), message);
	sflap_send(g, buf2, -1, TYPE_DATA);
}

static int toc_chat_send(GaimConnection *g, int id, char *message)
{
	char buf[BUF_LEN * 2];
	escape_text(message);
	if (strlen(message) > 2000)
		return -E2BIG;
	g_snprintf(buf, sizeof(buf), "toc_chat_send %d \"%s\"", id, message);
	sflap_send(g, buf, -1, TYPE_DATA);
	return 0;
}

static void toc_keepalive(GaimConnection *gc)
{
	sflap_send(gc, "", 0, TYPE_KEEPALIVE);
}

static const char *toc_list_icon(GaimAccount *a, struct buddy *b)
{
	return "aim";
}

static void toc_list_emblems(struct buddy *b, char **se, char **sw, char **nw, char **ne)
{
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	int i = 0;

	if (b->present == GAIM_BUDDY_OFFLINE) {
		*se = "offline";
		return;
	} else {
		if (b->uc & UC_UNAVAILABLE)
			emblems[i++] = "away";
		if (b->uc & UC_AOL)
			emblems[i++] = "aol";
		if (b->uc & UC_ADMIN)
			emblems[i++] = "admin";
		if (b->uc & UC_WIRELESS)
			emblems[i++] = "wireless";
	}
	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static GList *toc_buddy_menu(GaimConnection *gc, const char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Dir Info");
	pbm->callback = toc_get_dir;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static void toc_add_permit(GaimConnection *gc, const char *who)
{
	char buf2[BUF_LEN * 2];
	if (gc->account->perm_deny != 3)
		return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_permit %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
	toc_set_config(gc);
}

static void toc_add_deny(GaimConnection *gc, const char *who)
{
	char buf2[BUF_LEN * 2];
	if (gc->account->perm_deny != 4)
		return;
	g_snprintf(buf2, sizeof(buf2), "toc_add_deny %s", normalize(who));
	sflap_send(gc, buf2, -1, TYPE_DATA);
	toc_set_config(gc);
}

static void toc_set_permit_deny(GaimConnection *gc)
{
	char buf2[BUF_LEN * 2];
	GSList *list;
	int at;

	switch (gc->account->perm_deny) {
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
		list = gc->account->permit;
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
		list = gc->account->deny;
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

static void toc_rem_permit(GaimConnection *gc, const char *who)
{
	if (gc->account->perm_deny != 3)
		return;
	toc_set_permit_deny(gc);
}

static void toc_rem_deny(GaimConnection *gc, const char *who)
{
	if (gc->account->perm_deny != 4)
		return;
	toc_set_permit_deny(gc);
}

static GList *toc_away_states(GaimConnection *gc)
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static GList *toc_actions(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set User Info");
	pam->callback = show_set_info;
	pam->gc = gc;
	m = g_list_append(m, pam);

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set Dir Info");
	pam->callback = show_set_dir;
	pam->gc = gc;
	m = g_list_append(m, pam);

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Change Password");
	pam->callback = show_change_passwd;
	pam->gc = gc;
	m = g_list_append(m, pam);

	return m;
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

	GaimConnection *gc;

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
			"\t%d %d %d %d %d %d\n"
			"\t%d %d %d %d %d %d %d %d\n"
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

static void toc_send_file_callback(gpointer data, gint source, GaimInputCondition cond)
{
	char buf[BUF_LONG];
	int rt, i;

	struct file_transfer *ft = data;

	if (ft->hdr.hdrtype != 0x202) {
		char *buf;
		frombase64(ft->cookie, &buf, NULL);

		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		ft->hdr.hdrtype = 0x202;
		memcpy(ft->hdr.bcookie, buf, 8);
		g_free(buf);
		ft->hdr.encrypt = 0; ft->hdr.compress = 0;
		debug_header(ft);
		toc_write(source, ft, 256);

		if (ft->files == 1) {
			ft->file = fopen(ft->filename, "w");
			if (!ft->file) {
				buf = g_strdup_printf(_("Could not open %s for writing!"), ft->filename);
				gaim_notify_error(ft->gc, NULL, buf, strerror(errno));
				g_free(buf);
				gaim_input_remove(ft->inpa);
				toc_soc_close(source);
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
				gaim_notify_error(ft->gc, NULL, buf, strerror(errno));
				g_free(buf);
				gaim_input_remove(ft->inpa);
				toc_soc_close(source);
				g_free(ft->filename);
				g_free(ft->user);
				g_free(ft->ip);
				g_free(ft->cookie);
				g_free(ft);
			}
		}

		return;
	}

	rt = toc_read(source, buf, MIN(ntohl(ft->hdr.size) - ft->recvsize, 1024));
	if (rt < 0) {
		gaim_notify_error(ft->gc, NULL,
						  _("File transfer failed; other side probably "
							"canceled."), NULL);
		gaim_input_remove(ft->inpa);
		toc_soc_close(source);
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
		toc_write(source, ft, 256);
		debug_header(ft);
		ft->recvsize = 0;
		fclose(ft->file);
		if (ft->hdr.filesleft == 0) {
			gaim_input_remove(ft->inpa);
			toc_soc_close(source);
			g_free(ft->filename);
			g_free(ft->user);
			g_free(ft->ip);
			g_free(ft->cookie);
			g_free(ft);
		}
	}
}

static void toc_send_file_connect(gpointer data, gint src, GaimInputCondition cond)
{
	struct file_transfer *ft = data;

	if (src == -1) {
		gaim_notify_error(ft->gc, NULL,
						  _("Could not connect for transfer."), NULL);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}

	ft->inpa = gaim_input_add(src, GAIM_INPUT_READ, toc_send_file_callback, ft);
}

static void toc_send_file(gpointer a, struct file_transfer *old_ft)
{
	struct file_transfer *ft;
	const char *dirname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(old_ft->window));
	GaimAccount *account;
	char buf[BUF_LEN * 2];

	if (gaim_gtk_check_if_dir(dirname, GTK_FILE_SELECTION(old_ft->window)))
		return;
	ft = g_new0(struct file_transfer, 1);
	if (old_ft->files == 1)
		ft->filename = g_strdup(dirname);
	else
		ft->filename = g_path_get_dirname(dirname);
	ft->cookie = g_strdup(old_ft->cookie);
	ft->user = g_strdup(old_ft->user);
	ft->ip = g_strdup(old_ft->ip);
	ft->files = old_ft->files;
	ft->port = old_ft->port;
	ft->gc = old_ft->gc;
	account = ft->gc->account;
	gtk_widget_destroy(old_ft->window);

	g_snprintf(buf, sizeof(buf), "toc_rvous_accept %s %s %s", ft->user, ft->cookie, FILE_SEND_UID);
	sflap_send(ft->gc, buf, -1, TYPE_DATA);

	if (gaim_proxy_connect(account, ft->ip, ft->port, toc_send_file_connect, ft) != 0) {
		gaim_notify_error(ft->gc, NULL,
						  _("Could not connect for transfer."), NULL);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}
}

static void toc_get_file_callback(gpointer data, gint source, GaimInputCondition cond)
{
	char buf[BUF_LONG];

	struct file_transfer *ft = data;

	if (cond & GAIM_INPUT_WRITE) {
		int remain = MIN(ntohl(ft->hdr.totsize) - ft->recvsize, 1024);
		int i;
		for (i = 0; i < remain; i++)
			fscanf(ft->file, "%c", &buf[i]);
		toc_write(source, buf, remain);
		ft->recvsize += remain;
		if (ft->recvsize == ntohl(ft->hdr.totsize)) {
			gaim_input_remove(ft->inpa);
			ft->inpa = gaim_input_add(source, GAIM_INPUT_READ,
						 toc_get_file_callback, ft);
		}
		return;
	}

	if (ft->hdr.hdrtype == htons(0x1108)) {
		struct tm *fortime;
		struct stat st;
		char *basename;

		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		stat(ft->filename, &st);
		fortime = localtime(&st.st_mtime);
		basename = g_path_get_basename(ft->filename);
		g_snprintf(buf, sizeof(buf), "%2d/%2d/%4d %2d:%2d %8ld %s\r\n",
				fortime->tm_mon + 1, fortime->tm_mday, fortime->tm_year + 1900,
				fortime->tm_hour + 1, fortime->tm_min + 1, (long)st.st_size,
				basename);
		toc_write(source, buf, ntohl(ft->hdr.size));
		g_free(basename);
		return;
	}

	if (ft->hdr.hdrtype == htons(0x1209)) {
		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);
		return;
	}

	if (ft->hdr.hdrtype == htons(0x120b)) {
		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		if (ft->hdr.hdrtype != htons(0x120c)) {
			g_snprintf(buf, sizeof(buf), "%s decided to cancel the transfer", ft->user);
			gaim_notify_error(ft->gc, NULL, buf, NULL);
			gaim_input_remove(ft->inpa);
			toc_soc_close(source);
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
		toc_write(source, ft, 256);
		return;
	}

	if (ft->hdr.hdrtype == 0x0101) {
		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		gaim_input_remove(ft->inpa);
		ft->inpa = gaim_input_add(source, GAIM_INPUT_WRITE,
					 toc_get_file_callback, ft);
		return;
	}

	if (ft->hdr.hdrtype == 0x0202) {
		toc_read(source, ft, 8);
		toc_read(source, &ft->hdr.bcookie, MIN(256 - 8, ntohs(ft->hdr.hdrlen) - 8));
		debug_header(ft);

		gaim_input_remove(ft->inpa);
		toc_soc_close(source);
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

static void toc_get_file_connect(gpointer data, gint src, GaimInputCondition cond)
{
	struct file_transfer *ft = data;
	struct file_header *hdr;
	char *buf;
	char *basename;

	if (src == -1) {
		gaim_notify_error(ft->gc, NULL,
						  _("Could not connect for transfer!"), NULL);
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
	basename = g_path_get_basename(ft->filename);
	hdr->size = htonl(28 + strlen(basename)); /* size of listing.txt */
	g_free(basename);
	hdr->modtime = htonl(ft->st.st_mtime);
	hdr->checksum = htonl(0x89f70000); /* uh... */
	g_snprintf(hdr->idstring, 32, "OFT_Windows ICBMFT V1.1 32");
	hdr->flags = 0x02;
	hdr->lnameoffset = 0x1A;
	hdr->lsizeoffset = 0x10;
	g_snprintf(hdr->name, 64, "listing.txt");
	if (toc_write(src, hdr, 256) < 0) {
		gaim_notify_error(ft->gc, NULL,
						  _("Could not write file header.  The file will "
							"not be transferred."), NULL);
		fclose(ft->file);
		g_free(ft->filename);
		g_free(ft->cookie);
		g_free(ft->user);
		g_free(ft->ip);
		g_free(ft);
		return;
	}

	ft->inpa = gaim_input_add(src, GAIM_INPUT_READ, toc_get_file_callback, ft);
}

static void toc_get_file(gpointer a, struct file_transfer *old_ft)
{
	struct file_transfer *ft;
	const char *dirname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(old_ft->window));
	GaimAccount *account;
	char *buf, buf2[BUF_LEN * 2];

	if (gaim_gtk_check_if_dir(dirname, GTK_FILE_SELECTION(old_ft->window)))
		return;
	ft = g_new0(struct file_transfer, 1);
	ft->filename = g_strdup(dirname);
	ft->file = fopen(ft->filename, "r");
	if (!ft->file) {
		buf = g_strdup_printf("Unable to open %s for transfer.", ft->filename);
		gaim_notify_error(ft->gc, NULL, buf, NULL);
		g_free(buf);
		g_free(ft->filename);
		g_free(ft);
		return;
	}
	if (stat(dirname, &ft->st)) {
		buf = g_strdup_printf("Unable to examine %s.", dirname);
		gaim_notify_error(ft->gc, NULL, buf, NULL);
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
	account = ft->gc->account;
	gtk_widget_destroy(old_ft->window);

	g_snprintf(buf2, sizeof(buf2), "toc_rvous_accept %s %s %s", ft->user, ft->cookie, FILE_GET_UID);
	sflap_send(ft->gc, buf2, -1, TYPE_DATA);

	if (gaim_proxy_connect(account, ft->ip, ft->port, toc_get_file_connect, ft) < 0) {
		gaim_notify_error(ft->gc, NULL,
						  _("Could not connect for transfer."), NULL);
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

static void toc_reject_ft(struct ft_request *ft) {
	g_free(ft->user);
	g_free(ft->filename);
	g_free(ft->ip);
	g_free(ft->cookie);
	if (ft->message)
		g_free(ft->message);
	g_free(ft);
}


static void toc_accept_ft(struct ft_request *fr) {
	if(g_list_find(gaim_connections_get_all(), fr->gc)) {
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
		g_snprintf(buf, sizeof(buf), "%s/%s", gaim_home_dir(), fr->filename ? fr->filename : "");
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);
		g_signal_connect(G_OBJECT(window), "destroy",
				G_CALLBACK(cancel_callback), ft);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(ft->window)->cancel_button),
				"clicked", G_CALLBACK(cancel_callback), ft);

		if (!strcmp(fr->UID, FILE_SEND_UID))
			g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
					"clicked", G_CALLBACK(toc_send_file), ft);
		else
			g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
					"clicked", G_CALLBACK(toc_get_file), ft);

		gtk_widget_show(window);
	}

	toc_reject_ft(fr);
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
	        g_snprintf(buf, sizeof(buf), 
				ngettext(
				"%s requests %s to accept %d file: %s (%.2f %s)%s%s",
				"%s requests %s to accept %d files: %s (%.2f %s)%s%s",
				ft->files),
				ft->user, gaim_account_get_username(ft->gc->account), ft->files,
				ft->filename, size, sizes[index], (ft->message) ? "\n" : "",
				(ft->message) ? ft->message : "");
	} else {
		g_snprintf(buf, sizeof(buf), _("%s requests you to send them a file"), ft->user);
	}

	gaim_request_accept_cancel(ft->gc, NULL, buf, NULL, 0, ft,
							   G_CALLBACK(toc_accept_ft),
							   G_CALLBACK(toc_reject_ft));
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_TOC,
	OPT_PROTO_CORRECT_TIME,
	NULL,
	NULL,
	toc_list_icon,
	toc_list_emblems,
	NULL,
	NULL,
	toc_away_states,
	toc_actions,
	toc_buddy_menu,
	toc_chat_info,
	toc_login,
	toc_close,
	toc_send_im,
	toc_set_info,
	NULL,
	toc_get_info,
	toc_set_away,
	NULL,
	toc_set_dir,
	toc_get_dir,
	toc_dir_search,
	toc_set_idle,
	toc_change_passwd,
	toc_add_buddy,
	toc_add_buddies,
	toc_remove_buddy,
	toc_remove_buddies,
	toc_add_permit,
	toc_add_deny,
	toc_rem_permit,
	toc_rem_deny,
	toc_set_permit_deny,
	toc_warn,
	toc_join_chat,
	toc_chat_invite,
	toc_chat_leave,
	toc_chat_whisper,
	toc_chat_send,
	toc_keepalive,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-toc",                                       /**< id             */
	"TOC",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("TOC Protocol Plugin"),
	                                                  /**  description    */
	N_("TOC Protocol Plugin"),
	NULL,                                             /**< author         */
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("TOC host"), "server", TOC_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("TOC port"), "port", TOC_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(toc, init_plugin, info);
